#pragma once
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>   // stdAc::state_t

// 全局空调状态（单一事实来源，HomeKit / Web / 红外反馈三方共享）
struct AcRuntime {
  decode_type_t protocol = decode_type_t::KELVINATOR;  // 当前空调协议
  int16_t model = -1;          // 子型号，-1 = 默认
  stdAc::state_t cur;          // 目标状态（电源/模式/温度/风速/扫风/开关项）
  bool dirty = false;          // 有待发送的变更
  uint32_t dirtySince = 0;     // 首次变脏的时间（用于防抖合并）
  bool identifying = false;    // 协议识别模式
  uint32_t identifyStart = 0;
  String lastRx;               // 最近一次红外接收的摘要（Web 显示用）
  bool rxFeedbackEnabled = true;  // 红外状态反馈开关
};

extern AcRuntime ac;

void settingsLoad();   // 开机从 EEPROM 恢复
void settingsSave();   // 状态变化后写入 EEPROM（带 CRC 校验）
