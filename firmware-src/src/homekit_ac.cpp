#include "homekit_ac.h"
#include "settings.h"
#include "ir_ctrl.h"

#include <arduino_homekit_server.h>

extern "C" homekit_server_config_t config;
extern "C" homekit_characteristic_t cha_active;
extern "C" homekit_characteristic_t cha_current_hc_state;
extern "C" homekit_characteristic_t cha_target_hc_state;
extern "C" homekit_characteristic_t cha_current_temp;
extern "C" homekit_characteristic_t cha_cooling_threshold;
extern "C" homekit_characteristic_t cha_heating_threshold;
extern "C" homekit_characteristic_t cha_rotation_speed;
extern "C" homekit_characteristic_t cha_swing_mode;

// ---------- 枚举映射 ----------

// HomeKit TargetHeaterCoolerState: 0=Auto 1=Heat 2=Cool
static uint8_t modeToTargetState(stdAc::opmode_t m) {
  switch (m) {
    case stdAc::opmode_t::kHeat: return 1;
    case stdAc::opmode_t::kCool: return 2;
    default:                   return 0;  // dry/fan 在 HomeKit 中无对应项，显示为自动
  }
}

static stdAc::opmode_t targetStateToMode(uint8_t v) {
  switch (v) {
    case 1:  return stdAc::opmode_t::kHeat;
    case 2:  return stdAc::opmode_t::kCool;
    default: return stdAc::opmode_t::kAuto;
  }
}

// HomeKit CurrentHeaterCoolerState: 0=Inactive 1=Idle 2=Heating 3=Cooling
static uint8_t currentHcState() {
  if (!ac.cur.power) return 0;
  switch (ac.cur.mode) {
    case stdAc::opmode_t::kHeat: return 2;
    case stdAc::opmode_t::kCool: return 3;
    default:                     return 1;
  }
}

// RotationSpeed 0~100 与风速档位的互相映射
static float fanToRotation(stdAc::fanspeed_t f) {
  switch (f) {
    case stdAc::fanspeed_t::kMin:    return 20;
    case stdAc::fanspeed_t::kLow:    return 40;
    case stdAc::fanspeed_t::kMedium: return 60;
    case stdAc::fanspeed_t::kHigh:   return 80;
    case stdAc::fanspeed_t::kMax:    return 100;
    default:                         return 0;  // kAuto
  }
}

static stdAc::fanspeed_t rotationToFan(float v) {
  if (v < 10) return stdAc::fanspeed_t::kAuto;
  if (v < 30) return stdAc::fanspeed_t::kMin;
  if (v < 50) return stdAc::fanspeed_t::kLow;
  if (v < 70) return stdAc::fanspeed_t::kMedium;
  if (v < 90) return stdAc::fanspeed_t::kHigh;
  return stdAc::fanspeed_t::kMax;
}

// ---------- HomeKit -> 本机（setter） ----------

static void onActiveWrite(const homekit_value_t v) {
  cha_active.value.uint8_value = v.uint8_value;
  ac.cur.power = v.uint8_value != 0;
  irMarkDirty();
}

static void onTargetStateWrite(const homekit_value_t v) {
  cha_target_hc_state.value.uint8_value = v.uint8_value;
  ac.cur.mode = targetStateToMode(v.uint8_value);
  irMarkDirty();
}

static void onCoolingThresholdWrite(const homekit_value_t v) {
  float t = constrain(v.float_value, 16.0f, 30.0f);
  cha_cooling_threshold.value.float_value = t;
  cha_heating_threshold.value.float_value = t;  // 单一设定温度，两个阈值联动
  ac.cur.degrees = t;
  irMarkDirty();
}

static void onHeatingThresholdWrite(const homekit_value_t v) {
  float t = constrain(v.float_value, 16.0f, 30.0f);
  cha_heating_threshold.value.float_value = t;
  cha_cooling_threshold.value.float_value = t;
  ac.cur.degrees = t;
  irMarkDirty();
}

static void onRotationSpeedWrite(const homekit_value_t v) {
  cha_rotation_speed.value.float_value = v.float_value;
  ac.cur.fanspeed = rotationToFan(v.float_value);
  irMarkDirty();
}

static void onSwingModeWrite(const homekit_value_t v) {
  cha_swing_mode.value.uint8_value = v.uint8_value;
  ac.cur.swingv = v.uint8_value ? stdAc::swingv_t::kAuto : stdAc::swingv_t::kOff;
  irMarkDirty();
}

// ---------- 本机 -> HomeKit（通知） ----------

static void syncValues(bool notify) {
  cha_active.value.uint8_value = ac.cur.power ? 1 : 0;
  cha_target_hc_state.value.uint8_value = modeToTargetState(ac.cur.mode);
  cha_current_hc_state.value.uint8_value = currentHcState();
  cha_current_temp.value.float_value = ac.cur.degrees;  // 无温度传感器，回显设定温度
  cha_cooling_threshold.value.float_value = ac.cur.degrees;
  cha_heating_threshold.value.float_value = ac.cur.degrees;
  cha_rotation_speed.value.float_value = fanToRotation(ac.cur.fanspeed);
  cha_swing_mode.value.uint8_value = (ac.cur.swingv != stdAc::swingv_t::kOff) ? 1 : 0;

  if (!notify) return;  // 服务启动前仅赋值，不推送
  homekit_characteristic_notify(&cha_active, cha_active.value);
  homekit_characteristic_notify(&cha_target_hc_state, cha_target_hc_state.value);
  homekit_characteristic_notify(&cha_current_hc_state, cha_current_hc_state.value);
  homekit_characteristic_notify(&cha_current_temp, cha_current_temp.value);
  homekit_characteristic_notify(&cha_cooling_threshold, cha_cooling_threshold.value);
  homekit_characteristic_notify(&cha_heating_threshold, cha_heating_threshold.value);
  homekit_characteristic_notify(&cha_rotation_speed, cha_rotation_speed.value);
  homekit_characteristic_notify(&cha_swing_mode, cha_swing_mode.value);
}

void homekitSyncFromState() {
  syncValues(true);
}

void homekitAcSetup() {
  cha_active.setter = onActiveWrite;
  cha_target_hc_state.setter = onTargetStateWrite;
  cha_cooling_threshold.setter = onCoolingThresholdWrite;
  cha_heating_threshold.setter = onHeatingThresholdWrite;
  cha_rotation_speed.setter = onRotationSpeedWrite;
  cha_swing_mode.setter = onSwingModeWrite;

  syncValues(false);  // 服务未启动，只同步数值
  arduino_homekit_setup(&config);
  Serial.println(F("[HomeKit] server started, pair code: 111-11-111"));
}

void homekitAcLoop() {
  arduino_homekit_loop();
}
