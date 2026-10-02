#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>

bool wifiConnect();                         // 尝试连接已保存的 WiFi，成功返回 true
void wifiRunPortal();                       // 配网 AP 模式（阻塞，完成配网后自动重启）
String wifiPortalPage();                    // 配网页面 HTML（正常模式下 /wificonfig 复用）
void wifiHandleSave(ESP8266WebServer &server);  // 处理配网表单提交
