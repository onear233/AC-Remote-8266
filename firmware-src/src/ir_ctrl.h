#pragma once
#include <Arduino.h>

void irSetup();
void irLoop();            // 处理接收解码、识别模式、防抖发送
void irMarkDirty();       // 标记状态变更（防抖后发送）
void irSendNow();         // 立即发送当前状态
void irStartIdentify();   // 进入“识别空调协议”模式
