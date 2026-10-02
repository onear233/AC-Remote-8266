/*
 * ESP8266 IR HomeKit 空调遥控器（重制版）
 *
 * 基于 IRremoteESP8266 v2.9.0 + Arduino-HomeKit-ESP8266
 * 原项目 Mixiaoxiao/ESP8266-IR-HOMEKIT 仅发布二进制固件（内嵌 IRremoteESP8266 v2.7.7），
 * 本项目按其公开文档与功能清单重写，并升级到最新版红外库。
 */
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <arduino_homekit_server.h>  // homekit_server_reset()

#include "config.h"
#include "settings.h"
#include "ir_ctrl.h"
#include "homekit_ac.h"
#include "web_ui.h"
#include "wifi_portal.h"

static uint32_t btnPressStart = 0;
static bool btnWasDown = false;

static void buttonLoop() {
  bool down = (digitalRead(PIN_BUTTON) == LOW);
  if (down && !btnWasDown) {
    btnPressStart = millis();
  } else if (!down && btnWasDown) {
    btnPressStart = 0;
  } else if (down && btnPressStart && millis() - btnPressStart > 5000) {
    // 长按 5 秒：重置 HomeKit 配对并重启
    Serial.println(F("[Button] long press -> reset HomeKit & reboot"));
    homekit_server_reset();
    delay(200);
    ESP.restart();
  }
  btnWasDown = down;
}

static void wifiLedLoop() {
  // 已联网：熄灭；断线：慢闪（连接成功后由 wifiConnect 置为熄灭）
  static uint32_t last = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - last > 1000) {
      last = millis();
      digitalWrite(PIN_LED_WIFI, !digitalRead(PIN_LED_WIFI));
    }
  } else {
    digitalWrite(PIN_LED_WIFI, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.printf_P(PSTR("\n\nAC-IR HomeKit v%s (IRremoteESP8266 v%s)\n"),
                  FW_VERSION, _IRREMOTEESP8266_VERSION_STR);

  pinMode(PIN_LED_WIFI, OUTPUT);
  digitalWrite(PIN_LED_WIFI, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  settingsLoad();
  irSetup();

  if (!wifiConnect()) wifiRunPortal();  // 阻塞式配网，完成后自动重启

  homekitAcSetup();
  webSetup();
}

void loop() {
  homekitAcLoop();
  webLoop();
  irLoop();
  buttonLoop();
  wifiLedLoop();
  delay(10);
}
