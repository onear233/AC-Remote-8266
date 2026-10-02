#pragma once

// ---------- 硬件引脚定义（适配 AC-IR PCB，亦兼容 NodeMCU/D1 mini） ----------
// NodeMCU 丝印: D5=IO14, D6=IO12, D2=IO4, D7=IO13, D3=IO0
#define PIN_IR_TX       14   // 红外发射（必需）
#define PIN_IR_RX       12   // 红外接收（协议识别/状态反馈；不接时可把 ENABLE_IR_RX 设为 0）
#define PIN_LED_WIFI    2    // WiFi 状态灯（ESP-12 板上蓝色 LED，低电平点亮）
#define PIN_LED_IR      13   // 红外发射指示灯（低电平点亮）
#define PIN_BUTTON      0    // 按键（Flash 键），长按 5s 重置 HomeKit 配对

#define ENABLE_IR_RX    1    // 置 0 可完全关闭接收功能

// ---------- WiFi ----------
#define WIFI_CONNECT_TIMEOUT_S  20      // 开机等待已保存 WiFi 连接的超时（秒）
#define AP_SSID_PREFIX          "ESP_CONFIG_"  // 配网热点名前缀（后接 MAC 后 6 位）

// ---------- HomeKit ----------
#define HK_PASSWORD   "111-11-111"
#define HK_SETUP_ID   "ACIR"
#define FW_VERSION    "2.0.0"

// ---------- 红外 ----------
#define IR_IDENTIFY_TIMEOUT_S  60       // “识别空调协议”模式的等待时长（秒）
#define IR_DEBOUNCE_MS         800      // HomeKit 连续写入的合并发送间隔（毫秒）
