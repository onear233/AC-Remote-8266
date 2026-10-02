#include "wifi_portal.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <DNSServer.h>

static void setWifiLed(bool connected) {
  digitalWrite(PIN_LED_WIFI, connected ? HIGH : LOW);  // 低电平点亮：联网后熄灭，未联网时常亮/闪烁
}

bool wifiConnect() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);   // 凭据自动写入 flash，重启后自动连接
  WiFi.setAutoReconnect(true);
  WiFi.begin();            // 使用 flash 中已保存的凭据

  Serial.print(F("[WiFi] connecting"));
  for (uint16_t i = 0; i < WIFI_CONNECT_TIMEOUT_S * 2; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      setWifiLed(true);
      Serial.printf("\n[WiFi] connected, IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    digitalWrite(PIN_LED_WIFI, i % 2 ? LOW : HIGH);  // 等待时闪烁
    Serial.print('.');
    delay(500);
  }
  Serial.println(F("\n[WiFi] connect timeout"));
  return false;
}

String wifiPortalPage() {
  String page = FPSTR(
    "<!DOCTYPE html><html lang='zh'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFi 配网</title><style>"
    "body{font-family:-apple-system,'PingFang SC','Microsoft YaHei',sans-serif;background:#f2f2f7;margin:0;padding:16px}"
    ".card{background:#fff;border-radius:12px;padding:16px;max-width:480px;margin:0 auto 16px;box-shadow:0 1px 4px rgba(0,0,0,.08)}"
    "h1{font-size:18px;margin:0 0 12px}label{display:block;font-size:13px;color:#666;margin:10px 0 4px}"
    "input,select{width:100%;box-sizing:border-box;padding:10px;border:1px solid #ddd;border-radius:8px;font-size:15px}"
    "button{width:100%;margin-top:16px;padding:12px;border:0;border-radius:8px;background:#007aff;color:#fff;font-size:16px}"
    ".net{padding:10px;border-bottom:1px solid #eee;cursor:pointer;font-size:15px}.net:hover{background:#f5f5f5}"
    "</style></head><body><div class='card'><h1>WiFi 配网</h1>"
    "<form method='POST' action='/wificonfig/save'>"
    "<label>WiFi 名称（SSID）</label><input id='ssid' name='ssid' required>"
    "<label>WiFi 密码</label><input name='pass' type='password'>"
    "<button type='submit'>保存并重启</button></form></div>"
    "<div class='card'><h1>附近网络</h1><div id='list'>扫描中…</div></div>"
    "<script>fetch('/wificonfig/scan').then(r=>r.json()).then(a=>{"
    "document.getElementById('list').innerHTML=a.length?a.map(s=>\"<div class='net' onclick=\\\"document.getElementById('ssid').value='\"+s+\"'\\\">\"+s+\"</div>\").join(''):'未发现网络';});"
    "</script></body></html>");
  return page;
}

void wifiHandleSave(ESP8266WebServer &server) {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  ssid.trim();
  if (ssid.isEmpty()) {
    server.send(400, "text/plain; charset=utf-8", "SSID 不能为空");
    return;
  }
  server.send(200, "text/html; charset=utf-8",
              "<meta charset='utf-8'><body style='font-family:sans-serif;text-align:center;padding-top:40px'>"
              "已保存，设备正在重启并连接新网络…</body>");
  Serial.printf("[WiFi] new credentials saved: %s\n", ssid.c_str());
  delay(500);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());  // persistent=true，自动写入 flash
  delay(500);
  ESP.restart();
}

static void handleScan(ESP8266WebServer &server) {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) { server.send(200, "application/json", "[]"); return; }
  if (n < 0) { WiFi.scanNetworks(true); server.send(200, "application/json", "[]"); return; }
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i) json += ',';
    json += "\"" + WiFi.SSID(i) + "\"";
  }
  json += "]";
  WiFi.scanDelete();
  WiFi.scanNetworks(true);  // 为下一次扫描做准备
  server.send(200, "application/json", json);
}

void wifiRunPortal() {
  String apSsid = String(AP_SSID_PREFIX) + WiFi.macAddress();
  apSsid.replace(":", "");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str());
  Serial.printf("[WiFi] config AP started: %s  http://192.168.4.1\n", apSsid.c_str());

  DNSServer dns;
  dns.start(53, "*", WiFi.softAPIP());  // 捕获式 DNS，任意域名都指向本机

  ESP8266WebServer portal(80);
  portal.on("/wificonfig", HTTP_GET, [&]() { portal.send(200, "text/html", wifiPortalPage()); });
  portal.on("/wificonfig/save", HTTP_POST, [&]() { wifiHandleSave(portal); });
  portal.on("/wificonfig/scan", HTTP_GET, [&]() { handleScan(portal); });
  portal.onNotFound([&]() {  // 未匹配的路径一律跳转到配网页（ captive portal ）
    portal.sendHeader("Location", "http://192.168.4.1/wificonfig", true);
    portal.send(302, "text/plain", "");
  });
  portal.begin();
  WiFi.scanNetworks(true);  // 后台扫描供页面列表使用

  Serial.println(F("[WiFi] waiting for configuration..."));
  while (true) {
    dns.processNextRequest();
    portal.handleClient();
    digitalWrite(PIN_LED_WIFI, (millis() / 300) % 2 ? LOW : HIGH);  // 快闪表示配网模式
    yield();
  }
}
