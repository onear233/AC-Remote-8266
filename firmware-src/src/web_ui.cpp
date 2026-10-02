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
    "a{color:#007aff}"
    "</style></head><body>");
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
    if (IRac::isProtocolSupported(dt)) {
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
  html += F(" 型号="); html += String(ac.model); html += F("</div>");
  if (ac.protocol == decode_type_t::UNKNOWN) {
    html += F("<div class='warn'>尚未选择空调协议，请在下方选择或点击“识别协议”自动识别。</div>");
  }
  html += F("<form action='/settings' method='POST'>");
  html += F("<label>协议</label><select name='protocol'>"); html += protocolOptions(); html += F("</select>");
  html += F("<label>子型号（-1=默认）</label><input type='number' name='model' value='"); html += String(ac.model); html += F("'>");
  html += F("<button type='submit' class='btn2'>保存协议</button>");
  html += F("</form>");
  html += F("<form action='/identify' method='POST' style='margin-top:8px'>");
  html += F("<button type='submit' class='btn2'>"); html += (ac.identifying?F("识别中…"):F("识别空调协议")); html += F("</button>");
  html += F("</form>");
  if (!ac.identifying && !ac.lastRx.isEmpty()) {
    html += F("<div class='status'>"); html += ac.lastRx; html += F("</div>");
  }
  html += F("</div>");

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
  if (server.hasArg("protocol")) ac.protocol = (decode_type_t)server.arg("protocol").toInt();
  if (server.hasArg("model")) ac.model = server.arg("model").toInt();
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
