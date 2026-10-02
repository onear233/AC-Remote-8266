#pragma once
#include <Arduino.h>

void homekitAcSetup();        // 挂接 setter、同步初始值、启动 HomeKit 服务
void homekitAcLoop();         // 需在 loop() 中持续调用
void homekitSyncFromState();  // 红外反馈/识别/Web 修改后，把状态推送到 HomeKit
