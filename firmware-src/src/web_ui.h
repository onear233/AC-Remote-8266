#pragma once
#include <Arduino.h>

void webSetup();  // 注册路由、启动 80 端口 Web 服务
void webLoop();   // handleClient() + captive redirect
