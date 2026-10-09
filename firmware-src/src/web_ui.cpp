#include "web_ui.h"
#include "config.h"
#include "settings.h"
#include "ir_ctrl.h"
#include "homekit_ac.h"
#include "wifi_portal.h"

#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <IRutils.h>
#include <IRac.h>
#include <arduino_homekit_server.h>

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer httpUpdater;

static String pageHead(const char* title) {
  return String(F("<!DOCTYPE html><html lang='zh'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>")) + title + F("</title><style>"
    "body{font-family:-apple-system,'PingFang SC','Microsoft YaHei',sans-serif;background:#f2f2f7;margin:0;padding:12px}"
    ".card{background:#fff;border-radius:12px;padding:16px;margin:0 auto 12px;max-width:480px;box-shadow:0 1px 4px rgba(0,0,0,.08)}"
    "h1{font-size:18px;margin:0 0 12px}h2{font-size:15px;color:#666;margin:16px 0 8px}"
    "label{display:block;font-size:13px;color:#666;margin:10px 0 4px}"
    "input,select{width:100%;box-sizing:border-box;padding:10px;border:1px solid #ddd;border-radius:8px;font-size:15px}"
    "button{width:100%;margin-top:8px;padding:12px;border:0;border-radius:8px;background:#007aff;color:#fff;font-size:16px;cursor:pointer}"
    ".btn2{background:#34c759}.btn3{background:#ff3b30}"
    ".row{display:flex;gap:8px}.row>button{flex:1;margin-top:0}"
    ".status{padding:8px 12px;border-radius:8px;background:#e8f5e9;font-size:13px;margin-bottom:8px}"
    ".status.off{background:#ffebee}"
    ".warn{background:#fff3e0;padding:8px 12px;border-radius:8px;font-size:13px;margin-bottom:8px}"
    ".hint{font-size:12px;color:#999;margin:4px 0 0;line-height:1.5}"
    "a{color:#007aff}"
    "</style></head><body>");
}

// 帧时间相对现在的描述（调试面板显示"几秒前"）
static String frameAge(uint32_t t) {
  if (!t) return String("");
  uint32_t s = (millis() - t) / 1000;
  if (s < 60) return F("(") + String(s) + F("s前)");
  return F("(") + String(s / 60) + F("m前)");
}

static String pageFoot() { return F("</body></html>"); }

static const char* modeName(stdAc::opmode_t m) {
  switch (m) {
    case stdAc::opmode_t::kAuto: return "自动";
    case stdAc::opmode_t::kCool: return "制冷";
    case stdAc::opmode_t::kHeat: return "制热";
    case stdAc::opmode_t::kDry:  return "除湿";
    case stdAc::opmode_t::kFan:  return "送风";
    default:                     return "关";
  }
}

static const char* fanName(stdAc::fanspeed_t f) {
  switch (f) {
    case stdAc::fanspeed_t::kMin:    return "Min";
    case stdAc::fanspeed_t::kLow:    return "Low";
    case stdAc::fanspeed_t::kMedium: return "Medium";
    case stdAc::fanspeed_t::kHigh:   return "High";
    case stdAc::fanspeed_t::kMax:    return "Max";
    default:                         return "Auto";
  }
}

static String protocolOptions() {
  String s;
  for (int i = 1; i <= (int)kLastDecodeType; i++) {
    decode_type_t dt = (decode_type_t)i;
    // KELON168 未被 IRac::isProtocolSupported 收录，但本固件已直接支持收发
    if (dt == decode_type_t::KELON168 || IRac::isProtocolSupported(dt)) {
      s += F("<option value='");
      s += String(i);
      s += F("'");
      if (i == (int)ac.protocol) s += F(" selected");
      s += F(">");
      s += String(i) + ". " + typeToString(dt);
      s += F("</option>");
    }
  }
  return s;
}

// ---------- 子型号（遥控器型号枚举）----------
// 仅列出 IRac 发送路径实际消费 model 的协议；无表的协议视为无子型号。
// 枚举取值来自 IRremoteESP8266 v2.9.0 IRsend.h / ir_*.h。
struct ModelOption { int v; const char* n; };

static const ModelOption kModel168[] = {{-1,"默认"},{1,"DG11R2-01 / RCH-R0Y3"}};
static const ModelOption kModelLg[] = {{-1,"默认"},{1,"GE6711AR2853M"},{2,"AKB75215403"},{3,"AKB74955603"}};
static const ModelOption kModelWhirlpool[] = {{-1,"默认"},{1,"DG11J13A"},{2,"DG11J191"}};
static const ModelOption kModelFujitsu[] = {{-1,"默认"},{1,"AR-RAH2E"},{2,"AR-DB1"},{3,"AR-REB1E"}};
static const ModelOption kModelGree[] = {{-1,"默认"},{1,"YAW1F"},{2,"YBOFB"},{3,"YX1FSF"}};
static const ModelOption kModelHaier176[] = {{-1,"默认"},{1,"V9014557-A"},{2,"V9014557-B"}};
static const ModelOption kModelHitachi1[] = {{-1,"默认"},{1,"R-LT0541-A"},{2,"R-LT0541-B"}};
static const ModelOption kModelMirage[] = {{-1,"默认"},{1,"KKG9A-C1"},{2,"KKG29A-C1"}};
static const ModelOption kModelPanasonic[] = {{-1,"默认"},{1,"LKE"},{2,"NKE"},{3,"DKE"},{4,"JKE"},{5,"CKP"},{6,"RKR"}};
static const ModelOption kModelSharp[] = {{-1,"默认"},{1,"A907"},{2,"A705"},{3,"A903"}};
static const ModelOption kModelTcl[] = {{-1,"默认"},{1,"TAC09CHSD"},{2,"GZ055BE1"}};
static const ModelOption kModelVoltas[] = {{-1,"默认"},{1,"122LZF"}};
static const ModelOption kModelArgo[] = {{-1,"默认"},{1,"WREM2"},{2,"WREM3"}};
static const ModelOption kModelToshiba[] = {{-1,"默认"},{0,"Generic-A"},{1,"Generic-B"}};

static const ModelOption* modelOptions(decode_type_t p, size_t &count) {
  count = 0;
  const ModelOption* t = nullptr;
  switch (p) {
    case decode_type_t::KELON168:    t = kModel168;      count = sizeof(kModel168)/sizeof(*kModel168); break;
    case decode_type_t::LG:
    case decode_type_t::LG2:         t = kModelLg;       count = sizeof(kModelLg)/sizeof(*kModelLg); break;
    case decode_type_t::WHIRLPOOL_AC: t = kModelWhirlpool; count = sizeof(kModelWhirlpool)/sizeof(*kModelWhirlpool); break;
    case decode_type_t::FUJITSU_AC:  t = kModelFujitsu;  count = sizeof(kModelFujitsu)/sizeof(*kModelFujitsu); break;
    case decode_type_t::GREE:        t = kModelGree;     count = sizeof(kModelGree)/sizeof(*kModelGree); break;
    case decode_type_t::HAIER_AC176: t = kModelHaier176; count = sizeof(kModelHaier176)/sizeof(*kModelHaier176); break;
    case decode_type_t::HITACHI_AC1: t = kModelHitachi1; count = sizeof(kModelHitachi1)/sizeof(*kModelHitachi1); break;
    case decode_type_t::MIRAGE:      t = kModelMirage;   count = sizeof(kModelMirage)/sizeof(*kModelMirage); break;
    case decode_type_t::PANASONIC_AC: t = kModelPanasonic; count = sizeof(kModelPanasonic)/sizeof(*kModelPanasonic); break;
    case decode_type_t::SHARP_AC:    t = kModelSharp;    count = sizeof(kModelSharp)/sizeof(*kModelSharp); break;
    case decode_type_t::TCL112AC:
    case decode_type_t::TEKNOPOINT:  t = kModelTcl;      count = sizeof(kModelTcl)/sizeof(*kModelTcl); break;
    case decode_type_t::VOLTAS:      t = kModelVoltas;   count = sizeof(kModelVoltas)/sizeof(*kModelVoltas); break;
    case decode_type_t::ARGO:        t = kModelArgo;     count = sizeof(kModelArgo)/sizeof(*kModelArgo); break;
    case decode_type_t::TOSHIBA_AC:  t = kModelToshiba;  count = sizeof(kModelToshiba)/sizeof(*kModelToshiba); break;
    default: break;
  }
  return t;
}

static bool modelAllowed(decode_type_t p, int m) {
  size_t n;
  const ModelOption* t = modelOptions(p, n);
  if (!t) return m == -1;
  for (size_t i = 0; i < n; i++) if (t[i].v == m) return true;
  return false;
}

static String modelName(decode_type_t p, int m) {
  size_t n;
  const ModelOption* t = modelOptions(p, n);
  if (t)
    for (size_t i = 0; i < n; i++)
      if (t[i].v == m) return t[i].n;
  return String(m);
}

// 渲染当前协议的子型号下拉框（服务端按 ac.protocol 生成，协议切换时由前端 JSON 联动）
static String modelSelect(decode_type_t p, int cur) {
  size_t n;
  const ModelOption* t = modelOptions(p, n);
  String s;
  if (!t) {
    s += F("<select name='model' disabled><option value='-1'>该协议无子型号</option></select>");
    s += F("<input type='hidden' name='model' value='-1'>");
    return s;
  }
  s += F("<select name='model'>");
  for (size_t i = 0; i < n; i++) {
    s += F("<option value='");
    s += String(t[i].v);
    s += F("'");
    if (t[i].v == cur) s += F(" selected");
    s += F(">");
    s += t[i].n;
    s += F("</option>");
  }
  s += F("</select>");
  return s;
}

static void handleRoot() {
  String html = pageHead("AC IR Controller");
  html += F("<div class='card'><h1>"); html += WiFi.localIP().toString(); html += F("</h1>");

  // 状态条
  html += F("<div class='status");
  if (!ac.cur.power) html += F(" off");
  html += F("'>");
  html += ac.cur.power ? F("电源：开") : F("电源：关");
  html += F(" | 模式："); html += modeName(ac.cur.mode);
  html += F(" | 温度："); html += String((int)ac.cur.degrees); html += F("°C");
  html += F(" | 风速："); html += fanName(ac.cur.fanspeed);
  html += F(" | 扫风："); html += (ac.cur.swingv != stdAc::swingv_t::kOff ? F("开") : F("关"));
  html += F("</div>");

  // 控制表单
  html += F("<form action='/control' method='POST'>");
  html += F("<label>电源</label><select name='power'>");
  html += F("<option value='0'"); if (!ac.cur.power) html += F(" selected"); html += F(">关</option>");
  html += F("<option value='1'"); if (ac.cur.power)  html += F(" selected"); html += F(">开</option></select>");

  html += F("<label>模式</label><select name='mode'>");
  // 表单值直接使用 stdAc::opmode_t 枚举值（0=自动 1=制冷 2=制热 3=除湿 4=送风）
  static const struct { int v; const char* n; } modes[] = {
    {0,"自动"},{1,"制冷"},{2,"制热"},{3,"除湿"},{4,"送风"}
  };
  for (auto &m : modes) {
    html += F("<option value='"); html += String(m.v); html += F("'");
    if (m.v == (int)ac.cur.mode) html += F(" selected");
    html += F(">"); html += m.n; html += F("</option>");
  }
  html += F("</select>");

  html += F("<label>温度 (16~30°C)</label><input type='number' name='temp' min='16' max='30' value='");
  html += String((int)ac.cur.degrees); html += F("'>");

  html += F("<label>风速</label><select name='fan'>");
  static const struct { int v; const char* n; } fans[] = {
    {0,"Auto"},{1,"Min"},{2,"Low"},{3,"Medium"},{4,"High"},{5,"Max"}
  };
  for (auto &f : fans) {
    html += F("<option value='"); html += String(f.v); html += F("'");
    if (f.v == 0 && ac.cur.fanspeed == stdAc::fanspeed_t::kAuto) html += F(" selected");
    else if (f.v == 1 && ac.cur.fanspeed == stdAc::fanspeed_t::kMin) html += F(" selected");
    else if (f.v == 2 && ac.cur.fanspeed == stdAc::fanspeed_t::kLow) html += F(" selected");
    else if (f.v == 3 && ac.cur.fanspeed == stdAc::fanspeed_t::kMedium) html += F(" selected");
    else if (f.v == 4 && ac.cur.fanspeed == stdAc::fanspeed_t::kHigh) html += F(" selected");
    else if (f.v == 5 && ac.cur.fanspeed == stdAc::fanspeed_t::kMax) html += F(" selected");
    html += F(">"); html += f.n; html += F("</option>");
  }
  html += F("</select>");

  html += F("<label>扫风</label><select name='swing'>");
  html += F("<option value='0'"); if (ac.cur.swingv==stdAc::swingv_t::kOff) html += F(" selected"); html += F(">关</option>");
  html += F("<option value='1'"); if (ac.cur.swingv!=stdAc::swingv_t::kOff) html += F(" selected"); html += F(">开</option></select>");

  // 高级选项
  html += F("<h2>高级选项</h2><div class='row'>");
  html += F("<button type='button' class='btn2' onclick='t(\"quiet\",this)'>静音"); html += (ac.cur.quiet?F("●"):F("○")); html += F("</button>");
  html += F("<button type='button' class='btn2' onclick='t(\"turbo\",this)'>强劲"); html += (ac.cur.turbo?F("●"):F("○")); html += F("</button>");
  html += F("<button type='button' class='btn2' onclick='t(\"econo\",this)'>节能"); html += (ac.cur.econo?F("●"):F("○")); html += F("</button>");
  html += F("</div><div class='row' style='margin-top:8px'>");
  html += F("<button type='button' class='btn2' onclick='t(\"light\",this)'>灯光"); html += (ac.cur.light?F("●"):F("○")); html += F("</button>");
  html += F("<button type='button' class='btn2' onclick='t(\"filter\",this)'>滤网"); html += (ac.cur.filter?F("●"):F("○")); html += F("</button>");
  html += F("<button type='button' class='btn2' onclick='t(\"clean\",this)'>清洁"); html += (ac.cur.clean?F("●"):F("○")); html += F("</button>");
  html += F("</div>");
  html += F("<input type='hidden' name='quiet' id='quiet' value='"); html += (ac.cur.quiet?"1":"0"); html += F("'>");
  html += F("<input type='hidden' name='turbo' id='turbo' value='"); html += (ac.cur.turbo?"1":"0"); html += F("'>");
  html += F("<input type='hidden' name='econo' id='econo' value='"); html += (ac.cur.econo?"1":"0"); html += F("'>");
  html += F("<input type='hidden' name='light' id='light' value='"); html += (ac.cur.light?"1":"0"); html += F("'>");
  html += F("<input type='hidden' name='filter' id='filter' value='"); html += (ac.cur.filter?"1":"0"); html += F("'>");
  html += F("<input type='hidden' name='clean' id='clean' value='"); html += (ac.cur.clean?"1":"0"); html += F("'>");

  html += F("<button type='submit' style='margin-top:16px'>应用设置</button>");
  html += F("</form>");
  html += F("<script>function t(k,b){var i=document.getElementById(k);i.value=i.value==='1'?'0':'1';b.innerHTML=b.innerHTML.replace(/[●○]/,i.value==='1'?'●':'○');}</script>");
  html += F("</div>");

  // 协议设置
  html += F("<div class='card'><h1>红外协议</h1>");
  html += F("<div class='status'>当前："); html += typeToString(ac.protocol);
  html += F(" 子型号："); html += modelName(ac.protocol, ac.model); html += F("</div>");
  if (ac.protocol == decode_type_t::UNKNOWN) {
    html += F("<div class='warn'>尚未选择空调协议，请在下方选择或点击“识别协议”自动识别。</div>");
  }
  html += F("<form action='/settings' method='POST'>");
  html += F("<label>协议</label><select name='protocol' id='protoSel' onchange='mChanged()'>"); html += protocolOptions(); html += F("</select>");
  html += F("<label>子型号（原装遥控器型号）</label><span id='modelBox'>"); html += modelSelect(ac.protocol, ac.model); html += F("</span>");
  html += F("<div class='hint'>子型号是原装遥控器的型号枚举，仅少数协议需要区分；不确定时保持「默认」。点「识别空调协议」成功后会自动填写。海信 Hisense / Kelon 空请选择 KELON168。</div>");
  html += F("<button type='submit' class='btn2'>保存协议</button>");
  html += F("</form>");
  html += F("<form action='/identify' method='POST' style='margin-top:8px'>");
  html += F("<button type='submit' class='btn2'>"); html += (ac.identifying?F("识别中…"):F("识别空调协议")); html += F("</button>");
  html += F("</form>");
  if (!ac.identifying && !ac.lastRx.isEmpty()) {
    html += F("<div class='status'>"); html += ac.lastRx; html += F("</div>");
  }

  // 红外收发帧调试面板（KELON168 排障用，与串口 [TX]/[RX] 同源）
  if (!ac.lastTxFrame.isEmpty() || !ac.lastRxFrame.isEmpty()) {
    html += F("<h2>收发帧（调试）</h2>");
    html += F("<div class='status' style='font-family:monospace;word-break:break-all'>");
    if (!ac.lastTxFrame.isEmpty()) {
      html += F("<b>TX</b> ");
      html += frameAge(ac.lastTxTime);
      html += F(": ");
      html += ac.lastTxFrame;
    }
    if (!ac.lastRxFrame.isEmpty()) {
      html += F("<br><b>RX</b> ");
      html += frameAge(ac.lastRxTime);
      html += F(": ");
      html += ac.lastRxFrame;
    }
    html += F("</div>");
    html += F("<div class='hint'>TX = 本机发出的帧，RX = 原装遥控器被接收的帧。两行贴给 Claude 逐字节比对即可定位差异。</div>");
  }
  html += F("</div>");

  // 协议切换时联动重建子型号下拉框（数据由服务端按协议表生成）
  html += F("<script>var MD=");
  {
    String md = F("{");
    for (int i = 1; i <= (int)kLastDecodeType; i++) {
      decode_type_t dt = (decode_type_t)i;
      size_t n;
      const ModelOption* t = modelOptions(dt, n);
      if (!t) continue;
      if (md.length() > 1) md += F(",");
      md += String(i) + F(":[");
      for (size_t k = 0; k < n; k++) {
        if (k) md += F(",");
        md += String(t[k].v) + F(",") + String('"') + t[k].n + String('"');
      }
      md += F("]");
    }
    md += F("}");
    html += md;
  }
  html += F(";"
    "function mChanged(){"
    "var p=document.getElementById('protoSel').value,o=MD[p],h;"
    "if(!o){h='<select name=\"model\" disabled><option value=\"-1\">该协议无子型号</option></select><input type=\"hidden\" name=\"model\" value=\"-1\">';}"
    "else{h='<select name=\"model\">';for(var i=0;i<o.length;i+=2){h+='<option value=\"'+o[i]+'\">'+o[i+1]+'</option>';}h+='</select>';}"
    "document.getElementById('modelBox').innerHTML=h;}"
    "</script>");

  // 系统
  html += F("<div class='card'><h1>系统</h1>");
  html += F("<a href='/wificonfig'><button class='btn2'>WiFi 配网</button></a>");
  html += F("<a href='/update'><button class='btn2' style='margin-top:8px'>固件更新</button></a>");
  html += F("<form action='/reboot' method='POST' style='margin-top:8px'><button>重启设备</button></form>");
  html += F("<form action='/reset' method='POST' style='margin-top:8px' onsubmit='return confirm(\"这将移除所有 HomeKit 配对信息并重启，确定吗？\")'>");
  html += F("<button type='submit' class='btn3'>重置 HomeKit</button></form>");
  html += F("</div>");

  html += pageFoot();
  server.send(200, "text/html; charset=utf-8", html);
}

static void handleControl() {
  if (server.hasArg("power")) {
    int v = server.arg("power").toInt();
    ac.cur.power = (v != 0);
  }
  if (server.hasArg("mode")) {
    int v = server.arg("mode").toInt();
    ac.cur.mode = (stdAc::opmode_t)v;
  }
  if (server.hasArg("temp")) {
    float t = server.arg("temp").toFloat();
    ac.cur.degrees = constrain(t, 16.0f, 30.0f);
  }
  if (server.hasArg("fan")) {
    int v = server.arg("fan").toInt();
    ac.cur.fanspeed = (stdAc::fanspeed_t)v;
  }
  if (server.hasArg("swing")) {
    ac.cur.swingv = server.arg("swing").toInt() ? stdAc::swingv_t::kAuto : stdAc::swingv_t::kOff;
  }
  ac.cur.quiet  = server.hasArg("quiet") && server.arg("quiet") == "1";
  ac.cur.turbo  = server.hasArg("turbo") && server.arg("turbo") == "1";
  ac.cur.econo  = server.hasArg("econo") && server.arg("econo") == "1";
  ac.cur.light  = server.hasArg("light") && server.arg("light") == "1";
  ac.cur.filter = server.hasArg("filter") && server.arg("filter") == "1";
  ac.cur.clean  = server.hasArg("clean") && server.arg("clean") == "1";
  irMarkDirty();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

static void handleSettings() {
  if (server.hasArg("protocol")) {
    int p = server.arg("protocol").toInt();
    if (p >= 1 && p <= (int)kLastDecodeType) ac.protocol = (decode_type_t)p;
  }
  if (server.hasArg("model")) {
    int m = server.arg("model").toInt();
    ac.model = modelAllowed(ac.protocol, m) ? m : -1;
  }
  settingsSave();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

static void handleIdentify() {
  irStartIdentify();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

static void handleReset() {
  homekit_server_reset();
  server.send(200, "text/html; charset=utf-8",
    "<meta charset='utf-8'><body style='font-family:sans-serif;text-align:center;padding-top:40px'>"
    "<h1>HomeKit 已重置</h1><p>设备将在 3 秒后重启…</p></body>");
  delay(300);
  ESP.restart();
}

static void handleReboot() {
  server.send(200, "text/html; charset=utf-8",
    "<meta charset='utf-8'><body style='font-family:sans-serif;text-align:center;padding-top:40px'>"
    "<h1>正在重启…</h1></body>");
  delay(300);
  ESP.restart();
}

void webSetup() {
  httpUpdater.setup(&server, "/update");  // 网页端固件更新（选择 .bin 上传）
  server.on("/", HTTP_GET, handleRoot);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/identify", HTTP_POST, handleIdentify);
  server.on("/wificonfig", HTTP_GET, []() { server.send(200, "text/html; charset=utf-8", wifiPortalPage()); });
  server.on("/wificonfig/save", HTTP_POST, []() { wifiHandleSave(server); });
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.onNotFound([]() {
    server.sendHeader("Location", "http://" + WiFi.localIP().toString() + "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
  Serial.println(F("[Web] server started on port 80"));
}

void webLoop() {
  server.handleClient();
}
