#include "ir_ctrl.h"
#include "config.h"
#include "settings.h"
#include "homekit_ac.h"

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRac.h>
#include <IRutils.h>

static IRac irac(PIN_IR_TX);
#if ENABLE_IR_RX
static IRrecv irrecv(PIN_IR_RX, 1024, kTimeoutMs, true);  // 1024 字节缓冲以容纳长空调报文
static decode_results irResults;
#endif

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

void irSendNow() {
  ac.dirty = false;
#if ENABLE_IR_RX
  irrecv.disableIRIn();
#endif
  digitalWrite(PIN_LED_IR, LOW);

  // 组装完整目标状态，交给 IRac 统一发送
  irac.next = ac.cur;
  irac.next.protocol = ac.protocol;
  irac.next.model = ac.model;
  bool ok = irac.sendAc();

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

  stdAc::state_t decoded;
  if (!IRAcUtils::decodeToState(&irResults, &decoded, &ac.cur)) return;

  if (ac.identifying) {
    // 识别模式：采纳该协议与解析出的完整状态
    ac.identifying = false;
    ac.protocol = irResults.decode_type;
    if (decoded.model != -1) ac.model = decoded.model;
    ac.cur = decoded;
    ac.cur.celsius = true;
    ac.lastRx = "已识别：" + name + (ac.model >= 0 ? "（型号 " + String(ac.model) + "）" : "");
    Serial.printf("[IR] identified %s model=%d\n", name.c_str(), ac.model);
    settingsSave();
    homekitSyncFromState();
    return;
  }

  ac.lastRx = name + "  " + IRAcUtils::resultAcToString(&irResults);

  // 反馈模式：协议与当前设定一致时，用实体遥控器的状态同步本机状态
  if (ac.rxFeedbackEnabled && irResults.decode_type == ac.protocol) {
    ac.cur = decoded;
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
