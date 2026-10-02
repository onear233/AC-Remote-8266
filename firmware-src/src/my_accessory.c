#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void my_accessory_identify(homekit_value_t _value) {
    printf("AC IR identify\n");
}

homekit_characteristic_t cha_active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0);
homekit_characteristic_t cha_current_hc_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 0);
homekit_characteristic_t cha_target_hc_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 0);
homekit_characteristic_t cha_current_temp = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 25.0);
homekit_characteristic_t cha_cooling_threshold = HOMEKIT_CHARACTERISTIC_(COOLING_THRESHOLD_TEMPERATURE, 26.0);
homekit_characteristic_t cha_heating_threshold = HOMEKIT_CHARACTERISTIC_(HEATING_THRESHOLD_TEMPERATURE, 24.0);
homekit_characteristic_t cha_rotation_speed = HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 0.0);
homekit_characteristic_t cha_swing_mode = HOMEKIT_CHARACTERISTIC_(SWING_MODE, 0);
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, "AC IR");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_air_conditioner, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "AC IR"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "DIY"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "ACIR001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266-IR-v2"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "2.0.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
        HOMEKIT_SERVICE(HEATER_COOLER, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            &cha_active,
            &cha_current_hc_state,
            &cha_target_hc_state,
            &cha_current_temp,
            &cha_cooling_threshold,
            &cha_heating_threshold,
            &cha_rotation_speed,
            &cha_swing_mode,
            &cha_name,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111",
    .setupId = "ACIR",
    .config_number = 1,
};
