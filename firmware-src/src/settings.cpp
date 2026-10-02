#include "settings.h"
#include <EEPROM.h>

#define SETTINGS_MAGIC 0xAC1A2601

struct __attribute__((packed)) PersistedSettings {
  uint32_t magic;
  int16_t protocol;
  int16_t model;
  uint8_t power;
  uint8_t mode;
  float degrees;
  uint8_t fanspeed;
  uint8_t swingv;
  uint8_t swingh;
  uint8_t flags;   // bit0 quiet, 1 turbo, 2 econo, 3 light, 4 filter, 5 clean, 6 beep, 7 rxFeedback
  uint16_t crc;
};

AcRuntime ac;

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
  }
  return crc;
}

static uint8_t packFlags() {
  return (ac.cur.quiet ? 1 : 0) | (ac.cur.turbo ? 2 : 0) | (ac.cur.econo ? 4 : 0) |
         (ac.cur.light ? 8 : 0) | (ac.cur.filter ? 16 : 0) | (ac.cur.clean ? 32 : 0) |
         (ac.cur.beep ? 64 : 0) | (ac.rxFeedbackEnabled ? 128 : 0);
}

void settingsLoad() {
  EEPROM.begin(512);
  PersistedSettings s;
  EEPROM.get(0, s);
  if (s.magic != SETTINGS_MAGIC ||
      s.crc != crc16((const uint8_t *)&s, offsetof(PersistedSettings, crc))) {
    // 首次启动或数据损坏：使用默认值
    ac.protocol = decode_type_t::KELVINATOR;
    ac.model = -1;
    ac.cur.power = false;
    ac.cur.mode = stdAc::opmode_t::kCool;
    ac.cur.degrees = 26;
    ac.cur.celsius = true;
    ac.cur.fanspeed = stdAc::fanspeed_t::kAuto;
    ac.cur.swingv = stdAc::swingv_t::kOff;
    ac.cur.swingh = stdAc::swingh_t::kOff;
    return;
  }
  ac.protocol = (decode_type_t)s.protocol;
  ac.model = s.model;
  ac.cur.power = s.power;
  ac.cur.mode = (stdAc::opmode_t)s.mode;
  ac.cur.degrees = s.degrees;
  ac.cur.celsius = true;
  ac.cur.fanspeed = (stdAc::fanspeed_t)s.fanspeed;
  ac.cur.swingv = (stdAc::swingv_t)s.swingv;
  ac.cur.swingh = (stdAc::swingh_t)s.swingh;
  ac.cur.quiet = s.flags & 1;
  ac.cur.turbo = s.flags & 2;
  ac.cur.econo = s.flags & 4;
  ac.cur.light = s.flags & 8;
  ac.cur.filter = s.flags & 16;
  ac.cur.clean = s.flags & 32;
  ac.cur.beep = s.flags & 64;
  ac.rxFeedbackEnabled = s.flags & 128;
  ac.cur.sleep = -1;
  ac.cur.clock = -1;
}

void settingsSave() {
  PersistedSettings s;
  s.magic = SETTINGS_MAGIC;
  s.protocol = (int16_t)ac.protocol;
  s.model = ac.model;
  s.power = ac.cur.power;
  s.mode = (uint8_t)ac.cur.mode;
  s.degrees = ac.cur.degrees;
  s.fanspeed = (uint8_t)ac.cur.fanspeed;
  s.swingv = (uint8_t)ac.cur.swingv;
  s.swingh = (uint8_t)ac.cur.swingh;
  s.flags = packFlags();
  s.crc = crc16((const uint8_t *)&s, offsetof(PersistedSettings, crc));
  EEPROM.put(0, s);
  EEPROM.commit();
}
