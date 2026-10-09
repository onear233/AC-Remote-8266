#include "ir_ctrl.h"
#include "config.h"
#include "settings.h"
#include "homekit_ac.h"

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRutils.h>
#include <ir_Kelon.h>

static IRac irac(PIN_IR_TX);
#if ENABLE_IR_RX
static IRrecv irrecv(PIN_IR_RX, 1024, kTimeoutMs, true);  // 1024 字节缓冲以容纳长空调报文
static decode_results irResults;
#endif

// KELON168（海信/Kelon 168bit）未被 IRac 集成（sendAc/decodeToState 均无分支），
// 直接驱动 IRKelon168Ac。
static IRKelon168Ac ac168(PIN_IR_TX);
static bool ac168Initialized = false;  // 首次发送前需 stateReset 以保证合法前导/校验

// 仅保留可信字段：电源位（_.Power 状态位）无法从接收帧回读真实开关状态
static void adoptKelon168State(stdAc::state_t &dst, const stdAc::state_t &src) {
  dst.mode = src.mode;
  dst.degrees = src.degrees;
  dst.celsius = true;
  dst.fanspeed = src.fanspeed;
  dst.swingv = src.swingv;
  dst.turbo = src.turbo;
  dst.light = src.light;
  dst.sleep = src.sleep;
}

// 采纳解码结果：KELON168 电源位不可信只取安全字段，其余协议整帧采纳
static void adoptDecodedState(stdAc::state_t &dst, const stdAc::state_t &src, bool isKelon168) {
  if (isKelon168) adoptKelon168State(dst, src);
  else dst = src;
}

#if ENABLE_IR_RX
static bool kelon168Decode(stdAc::state_t *out, String *text) {
  IRKelon168Ac decoder(kGpioUnused);
  decoder.setRaw(irResults.state, kKelon168StateLength);
  *out = decoder.toCommon(&ac.cur);
  if (text) *text = decoder.toString();
  return true;
}
#endif

static String frameToHex(const uint8_t *b, uint16_t len) {
  String s;
  s.reserve(len * 3);
  for (uint16_t i = 0; i < len; i++) {
    if (i) s += ' ';
    if (b[i] < 0x10) s += '0';
    s += String(b[i], HEX);
  }
  return s;
}

void irSetup() {
  pinMode(PIN_LED_IR, OUTPUT);
  digitalWrite(PIN_LED_IR, HIGH);  // 低电平点亮，默认熄灭
#if ENABLE_IR_RX
  irrecv.setTolerance(kTolerance);
  irrecv.enableIRIn();
#endif
}

void irMarkDirty() {
  if (!ac.dirty) ac.dirtySince = millis();
  ac.dirty = true;
}

// KELON168 为"全量帧"协议：空调每帧都应用帧内全部字段，Cmd 字节只是"按了哪个键"
// 的标签（依据：库内唯一真实捕获帧 Cmd=0x00/Light 却携带有效 Fan/Temp/Mode/On）。
// 因此一次变更只需发送单帧，而非按 Cmd 拆成多帧——多帧会被空调当成连按而报警。
// setter 存在级联副作用（setSuper 会改写 temp/mode/fan，setSleep 会改写 fan），
// 故按"带副作用者优先、无副作用者殿后"的顺序调用，最后由 setPower 盖上 Cmd。
static bool sendKelon168State() {
  ac168.begin();
  if (!ac168Initialized) {  // 首次（开机后）保证帧有合法前导/校验
    ac168.stateReset();
    ac168Initialized = true;
  }

  ac168.setModel(ac.model >= 1 ? (kelon168_ac_remote_model_t)ac.model
                               : kelon168_ac_remote_model_t::DG11R201);

  // 1) sleep 先设：setSleep(true) 会改写 fan（下方 setFan 恢复）
  ac168.setSleep(ac.cur.sleep >= 0);

  // 2) 无副作用字段殿后，恢复被 setSleep/setSuper 改写的 temp/mode/fan
  ac168.setSwing(ac.cur.swingv != stdAc::swingv_t::kOff);
  ac168.setLight(ac.cur.light);
  ac168.setTemp((uint8_t)ac.cur.degrees);
  ac168.setMode(IRKelon168Ac::convertMode(ac.cur.mode));
  ac168.setFan(IRKelon168Ac::convertFan(ac.cur.fanspeed));

  // 3) 最后 setPower：唯一副作用是清 super 位（不改其他字段），并把 Cmd 置为 Power
  ac168.setPower(ac.cur.power);

  // 4) turbo=true 时复原被 setPower 清掉的 super 位。这里只用 setSuper 置位再恢复
  //    字段（setSuper(true) 会级联改写 temp/mode/fan），最后把 Cmd 盖回 Power 标签。
  if (ac.cur.turbo) {
    ac168.setSuper(true);
    ac168.setTemp((uint8_t)ac.cur.degrees);
    ac168.setMode(IRKelon168Ac::convertMode(ac.cur.mode));
    ac168.setFan(IRKelon168Ac::convertFan(ac.cur.fanspeed));
    ac168.setCommand(kKelon168CommandPower);
  }

  ac168.send();
  // 记录发出的帧（串口 + Web 调试面板），便于与原装遥控器捕获帧逐字节比对
  uint8_t *sent = ac168.getRaw(false);
  ac.lastTxFrame = frameToHex(sent, kKelon168StateLength);
  ac.lastTxTime = millis();
  Serial.print(F("[TX] KELON168: "));
  Serial.println(ac.lastTxFrame);
  return true;
}

void irSendNow() {
  ac.dirty = false;
#if ENABLE_IR_RX
  irrecv.disableIRIn();
#endif
  digitalWrite(PIN_LED_IR, LOW);

  bool ok;
  if (ac.protocol == decode_type_t::KELON168) {
    ok = sendKelon168State();
  } else {
    // 组装完整目标状态，交给 IRac 统一发送
    irac.next = ac.cur;
    irac.next.protocol = ac.protocol;
    irac.next.model = ac.model;
    ok = irac.sendAc();
  }

  digitalWrite(PIN_LED_IR, HIGH);
#if ENABLE_IR_RX
  irrecv.enableIRIn();
#endif
  Serial.printf("[IR] sendAc(%s model=%d) -> %s\n",
                typeToString(ac.protocol).c_str(), ac.model, ok ? "OK" : "FAIL");
  settingsSave();
}

void irStartIdentify() {
#if ENABLE_IR_RX
  ac.identifying = true;
  ac.identifyStart = millis();
  ac.lastRx = F("识别中：请将空调原配遥控器对准接收头，按任意键 ...");
  Serial.println(F("[IR] identify mode started"));
#endif
}

#if ENABLE_IR_RX
static void handleDecode() {
  String name = typeToString(irResults.decode_type);
  if (!hasACState(irResults.decode_type)) {
    Serial.printf("[IR] rx %s (not an A/C protocol, ignored)\n", name.c_str());
    if (ac.identifying) ac.lastRx = "收到 " + name + "（非空调协议，忽略）";
    return;
  }

  // KELON168 无 IRAcUtils 支持，手动解码；其余协议走统一接口
  const bool isKelon168 = (irResults.decode_type == decode_type_t::KELON168);
  if (isKelon168) {  // 记录接收帧（串口 + Web 调试面板）
    ac.lastRxFrame = frameToHex(irResults.state, kKelon168StateLength);
    ac.lastRxTime = millis();
    Serial.print(F("[RX] KELON168: "));
    Serial.println(ac.lastRxFrame);
  }
  stdAc::state_t decoded;
  String detail;
  bool ok = isKelon168 ? kelon168Decode(&decoded, &detail)
                       : IRAcUtils::decodeToState(&irResults, &decoded, &ac.cur);
  if (!ok) return;

  if (ac.identifying) {
    // 识别模式：采纳该协议与解析出的状态（电源位不可信，保留原值）
    ac.identifying = false;
    ac.protocol = irResults.decode_type;
    if (decoded.model != -1) ac.model = decoded.model;
    adoptDecodedState(ac.cur, decoded, isKelon168);
    ac.cur.celsius = true;
    ac.lastRx = "已识别：" + name + (ac.model >= 0 ? "（型号 " + String(ac.model) + "）" : "");
    if (isKelon168) ac.lastRx += "  " + detail;
    Serial.printf("[IR] identified %s model=%d\n", name.c_str(), ac.model);
    settingsSave();
    homekitSyncFromState();
    return;
  }

  ac.lastRx = name + "  " + (isKelon168 ? detail : IRAcUtils::resultAcToString(&irResults));

  // 反馈模式：协议与当前设定一致时，用实体遥控器的状态同步本机状态
  if (ac.rxFeedbackEnabled && irResults.decode_type == ac.protocol) {
    adoptDecodedState(ac.cur, decoded, isKelon168);
    ac.cur.celsius = true;
    Serial.printf("[IR] feedback sync from %s\n", name.c_str());
    settingsSave();
    homekitSyncFromState();
  }
}
#endif

void irLoop() {
#if ENABLE_IR_RX
  if (ac.identifying && millis() - ac.identifyStart > IR_IDENTIFY_TIMEOUT_S * 1000UL) {
    ac.identifying = false;
    ac.lastRx = F("识别超时：60 秒内未收到有效的空调红外信号");
    Serial.println(F("[IR] identify timeout"));
  }
  if (irrecv.decode(&irResults)) {
    handleDecode();
    irrecv.resume();
  }
#endif
  // 防抖：HomeKit/Web 连续修改时合并为一次发送
  if (ac.dirty && millis() - ac.dirtySince >= IR_DEBOUNCE_MS) irSendNow();
}
