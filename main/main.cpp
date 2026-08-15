#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "esp_zigbee_attribute.h"
#include "esp_zigbee_cluster.h"
#include "esp_zigbee_core.h"
#include "zcl/esp_zigbee_zcl_analog_input.h"
#include "zcl/esp_zigbee_zcl_basic.h"
#include "zcl/esp_zigbee_zcl_common.h"
#include "zcl/esp_zigbee_zcl_ota.h"
#include "zcl/esp_zigbee_zcl_thermostat.h"
#include "zcl/esp_zigbee_zcl_temperature_meas.h"

#include "midea.h"

static const char *TAG = "ZB_AC";

static constexpr gpio_num_t UART_TX_GPIO = GPIO_NUM_5;
static constexpr gpio_num_t UART_RX_GPIO = GPIO_NUM_8;
static constexpr gpio_num_t FACTORY_RESET_GPIO = GPIO_NUM_9;
static constexpr uint8_t ZIGBEE_ENDPOINT = 1;
static constexpr int64_t FACTORY_RESET_HOLD_MS = 5000;

// Zigbee OTA (обновление по воздуху через Z2M). manufacturer/image_type должны
// совпадать с .ota-файлом в OTA-индексе Z2M; обновление предлагается, если версия
// файла > текущей firmware_version. image_type уникален на чип (H2 != C6).
static constexpr uint16_t OTA_MANUFACTURER       = 0x1289;
static constexpr uint16_t OTA_IMAGE_TYPE         = 0xB201;  // ESP32-H2 (в C6-прошивке 0xC601)
static constexpr uint16_t OTA_HW_VERSION         = 0x0101;
static constexpr uint8_t  OTA_MAX_DATA_SIZE      = 223;
static constexpr int      OTA_ELEMENT_HEADER_LEN = 6;
static constexpr int      OTA_QUERY_INTERVAL_MIN = 60;      // как часто спрашивать сервер (мин)

static constexpr uint16_t ATTR_AC_POWER_ID          = 0xF000;
static constexpr uint16_t ATTR_AC_MODE_ID           = 0xF001;
static constexpr uint16_t ATTR_AC_FAN_MODE_ID       = 0xF002;
static constexpr uint16_t ATTR_AC_SWING_MODE_ID     = 0xF003;
static constexpr uint16_t ATTR_AC_PRESET_ID         = 0xF004;
static constexpr uint16_t ATTR_AC_DISPLAY_ID        = 0xF005;
static constexpr uint16_t ATTR_AC_INDOOR_TEMP_ID    = 0xF006;
static constexpr uint16_t ATTR_AC_OUTDOOR_TEMP_ID   = 0xF007;
static constexpr uint16_t ATTR_AC_TARGET_TEMP_ID    = 0xF008;
static constexpr uint16_t ATTR_AC_FW_VERSION_ID     = 0xF009;

static constexpr uint8_t AC_MODE_OFF      = 0;
static constexpr uint8_t AC_MODE_AUTO     = 1;
static constexpr uint8_t AC_MODE_COOL     = 2;
static constexpr uint8_t AC_MODE_HEAT     = 3;
static constexpr uint8_t AC_MODE_DRY      = 4;
static constexpr uint8_t AC_MODE_FAN_ONLY = 5;

static constexpr uint8_t FAN_AUTO   = 0;
static constexpr uint8_t FAN_LOW    = 1;
static constexpr uint8_t FAN_MEDIUM = 2;
static constexpr uint8_t FAN_HIGH   = 3;
static constexpr uint8_t FAN_QUIET  = 4;

static constexpr uint8_t SWING_OFF        = 0;
static constexpr uint8_t SWING_HORIZONTAL = 1;
static constexpr uint8_t SWING_VERTICAL   = 2;
static constexpr uint8_t SWING_BOTH       = 3;

static constexpr uint8_t PRESET_NONE  = 0;
static constexpr uint8_t PRESET_SLEEP = 1;
static constexpr uint8_t PRESET_TURBO = 2;

static bool ac_power = false;
static uint8_t ac_mode = AC_MODE_OFF;
static uint8_t ac_fan_mode = FAN_AUTO;
static uint8_t ac_swing_mode = SWING_OFF;
static uint8_t ac_preset = PRESET_NONE;
static bool ac_display = true;
static float ac_indoor_temp = 22.0f;
static float ac_outdoor_temp = 0.0f;
static float ac_target_temp = 22.0f;
static int16_t thermostat_local_temp = 2200;
static int16_t thermostat_outdoor_temp = 0;
static int16_t thermostat_cooling_setpoint = 2200;
static int16_t thermostat_heating_setpoint = 2200;
static int16_t thermostat_min_heat_setpoint = 1600;
static int16_t thermostat_max_heat_setpoint = 3000;
static int16_t thermostat_min_cool_setpoint = 1600;
static int16_t thermostat_max_cool_setpoint = 3000;
static uint8_t thermostat_control_sequence = ESP_ZB_ZCL_THERMOSTAT_CONTROL_SEQ_OF_OPERATION_COOLING_AND_HEATING_4_PIPES;
static uint8_t thermostat_system_mode = ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF;
static uint8_t thermostat_running_mode = 0x00;
static uint8_t thermostat_ac_type = 0x04;
static uint8_t thermostat_louver_position = ESP_ZB_ZCL_THERMOSTAT_LOUVER_FULLY_CLOSED;
static uint32_t firmware_version = 0x00010B00;  // 0.1.11 — PirogovX ID + Router + factory reset + Zigbee OTA

static int16_t celsius_to_zcl(float celsius)
{
    return (int16_t)(celsius * 100.0f);
}

static float normalize_target_temp(float celsius)
{
    if (celsius < 16.0f) {
        celsius = 16.0f;
    } else if (celsius > 30.0f) {
        celsius = 30.0f;
    }
    return (float)((int)(celsius + 0.5f));
}

static uint8_t midea_mode_to_ac_mode(midea::Mode mode, bool power_on)
{
    if (!power_on) {
        return AC_MODE_OFF;
    }
    switch (mode) {
    case midea::Mode::Auto: return AC_MODE_AUTO;
    case midea::Mode::Cool: return AC_MODE_COOL;
    case midea::Mode::Heat: return AC_MODE_HEAT;
    case midea::Mode::Dry:  return AC_MODE_DRY;
    case midea::Mode::Fan:  return AC_MODE_FAN_ONLY;
    }
    return AC_MODE_AUTO;
}

static midea::Mode ac_mode_to_midea(uint8_t mode)
{
    switch (mode) {
    case AC_MODE_COOL:     return midea::Mode::Cool;
    case AC_MODE_HEAT:     return midea::Mode::Heat;
    case AC_MODE_DRY:      return midea::Mode::Dry;
    case AC_MODE_FAN_ONLY: return midea::Mode::Fan;
    case AC_MODE_AUTO:
    default:               return midea::Mode::Auto;
    }
}

static uint8_t ac_mode_to_system_mode(uint8_t mode)
{
    switch (mode) {
    case AC_MODE_OFF:      return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF;
    case AC_MODE_COOL:     return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_COOL;
    case AC_MODE_HEAT:     return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT;
    case AC_MODE_DRY:      return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_DRY;
    case AC_MODE_FAN_ONLY: return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_FAN_ONLY;
    case AC_MODE_AUTO:
    default:               return ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_AUTO;
    }
}

static uint8_t system_mode_to_ac_mode(uint8_t mode)
{
    switch (mode) {
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_OFF:      return AC_MODE_OFF;
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_COOL:     return AC_MODE_COOL;
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_HEAT:     return AC_MODE_HEAT;
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_DRY:      return AC_MODE_DRY;
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_FAN_ONLY: return AC_MODE_FAN_ONLY;
    case ESP_ZB_ZCL_THERMOSTAT_SYSTEM_MODE_AUTO:
    default:                                         return AC_MODE_AUTO;
    }
}

static uint8_t midea_fan_to_ac_fan(midea::FanSpeed fan)
{
    switch (fan) {
    case midea::FanSpeed::Auto:   return FAN_AUTO;
    case midea::FanSpeed::Low:    return FAN_LOW;
    case midea::FanSpeed::Medium: return FAN_MEDIUM;
    case midea::FanSpeed::High:   return FAN_HIGH;
    case midea::FanSpeed::Silent: return FAN_QUIET;
    }
    return FAN_AUTO;
}

static midea::FanSpeed ac_fan_to_midea(uint8_t fan)
{
    switch (fan) {
    case FAN_LOW:    return midea::FanSpeed::Low;
    case FAN_MEDIUM: return midea::FanSpeed::Medium;
    case FAN_HIGH:   return midea::FanSpeed::High;
    case FAN_QUIET:  return midea::FanSpeed::Silent;
    case FAN_AUTO:
    default:         return midea::FanSpeed::Auto;
    }
}

static uint8_t midea_swing_to_ac_swing(bool horizontal, bool vertical)
{
    if (horizontal && vertical) return SWING_BOTH;
    if (horizontal) return SWING_HORIZONTAL;
    if (vertical) return SWING_VERTICAL;
    return SWING_OFF;
}

static uint8_t swing_to_louver(uint8_t swing)
{
    return swing == SWING_OFF ? ESP_ZB_ZCL_THERMOSTAT_LOUVER_FULLY_CLOSED
                              : ESP_ZB_ZCL_THERMOSTAT_LOUVER_FULLY_OPEN;
}

static void update_attr(uint16_t cluster, uint16_t attr, const void *value)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(ZIGBEE_ENDPOINT, cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attr, (void *)value, false);
    esp_zb_lock_release();
}

static void publish_all_attrs()
{
    thermostat_local_temp = celsius_to_zcl(ac_indoor_temp);
    thermostat_outdoor_temp = celsius_to_zcl(ac_outdoor_temp);
    thermostat_cooling_setpoint = celsius_to_zcl(ac_target_temp);
    thermostat_heating_setpoint = celsius_to_zcl(ac_target_temp);
    thermostat_system_mode = ac_mode_to_system_mode(ac_mode);
    thermostat_running_mode = ac_power ? thermostat_system_mode : 0x00;
    thermostat_louver_position = swing_to_louver(ac_swing_mode);

    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_LOCAL_TEMPERATURE_ID, &thermostat_local_temp);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OUTDOOR_TEMPERATURE_ID, &thermostat_outdoor_temp);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID, &thermostat_cooling_setpoint);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID, &thermostat_heating_setpoint);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID, &thermostat_system_mode);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_RUNNING_MODE_ID, &thermostat_running_mode);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT, ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_LOUVER_POSITION_ID, &thermostat_louver_position);

    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_POWER_ID, &ac_power);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_MODE_ID, &ac_mode);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_FAN_MODE_ID, &ac_fan_mode);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_SWING_MODE_ID, &ac_swing_mode);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_PRESET_ID, &ac_preset);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_DISPLAY_ID, &ac_display);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_INDOOR_TEMP_ID, &ac_indoor_temp);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_OUTDOOR_TEMP_ID, &ac_outdoor_temp);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_TARGET_TEMP_ID, &ac_target_temp);
    update_attr(ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_FW_VERSION_ID, &firmware_version);
}

static void on_midea_state(const midea::State &state)
{
    if (!state.valid) {
        return;
    }

    ac_power = state.power_on;
    ac_mode = midea_mode_to_ac_mode(state.mode, state.power_on);
    ac_fan_mode = midea_fan_to_ac_fan(state.fan);
    ac_swing_mode = midea_swing_to_ac_swing(state.swing_h, state.swing_v);
    ac_preset = state.turbo ? PRESET_TURBO : (state.sleep ? PRESET_SLEEP : PRESET_NONE);
    ac_indoor_temp = state.indoor_temp;
    ac_outdoor_temp = state.outdoor_temp;
    ac_target_temp = state.set_temp;

    ESP_LOGI(TAG, "AC state: power=%d mode=%u fan=%u swing=%u preset=%u target=%.1f in=%.1f out=%.1f",
             ac_power, ac_mode, ac_fan_mode, ac_swing_mode, ac_preset,
             ac_target_temp, ac_indoor_temp, ac_outdoor_temp);
    publish_all_attrs();
}

static bool attr_bool(const esp_zb_zcl_set_attr_value_message_t *message)
{
    return message->attribute.data.value && *((bool *)message->attribute.data.value);
}

static uint8_t attr_u8(const esp_zb_zcl_set_attr_value_message_t *message)
{
    return message->attribute.data.value ? *((uint8_t *)message->attribute.data.value) : 0;
}

static int16_t attr_i16(const esp_zb_zcl_set_attr_value_message_t *message)
{
    return message->attribute.data.value ? *((int16_t *)message->attribute.data.value) : 0;
}

static float attr_single(const esp_zb_zcl_set_attr_value_message_t *message)
{
    return message->attribute.data.value ? *((float *)message->attribute.data.value) : 0.0f;
}

static void apply_power(bool on)
{
    ac_power = on;
    if (!on) {
        ac_mode = AC_MODE_OFF;
    } else if (ac_mode == AC_MODE_OFF) {
        ac_mode = AC_MODE_COOL;
    }
    midea::set_power(on);
}

static void apply_mode(uint8_t mode)
{
    ac_mode = mode;
    ac_power = mode != AC_MODE_OFF;
    if (ac_power) {
        midea::set_mode(ac_mode_to_midea(mode));
    } else {
        midea::set_power(false);
    }
}

static void apply_fan(uint8_t fan)
{
    ac_fan_mode = fan <= FAN_QUIET ? fan : FAN_AUTO;
    midea::set_fan(ac_fan_to_midea(ac_fan_mode));
}

static void apply_swing(uint8_t swing)
{
    ac_swing_mode = swing <= SWING_BOTH ? swing : SWING_OFF;
    midea::set_swing(ac_swing_mode == SWING_HORIZONTAL || ac_swing_mode == SWING_BOTH,
                     ac_swing_mode == SWING_VERTICAL || ac_swing_mode == SWING_BOTH);
}

static void apply_preset(uint8_t preset)
{
    ac_preset = preset <= PRESET_TURBO ? preset : PRESET_NONE;
    midea::set_preset(ac_preset == PRESET_SLEEP, ac_preset == PRESET_TURBO);
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty attribute message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG,
                        TAG, "Attribute write failed with status %d", message->info.status);
    ESP_RETURN_ON_FALSE(message->info.dst_endpoint == ZIGBEE_ENDPOINT, ESP_OK, TAG, "Other endpoint");
    ESP_RETURN_ON_FALSE(message->attribute.data.value, ESP_OK, TAG, "Empty value");

    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_THERMOSTAT) {
        switch (message->attribute.id) {
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_SYSTEM_MODE_ID:
            apply_mode(system_mode_to_ac_mode(attr_u8(message)));
            break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_COOLING_SETPOINT_ID:
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_OCCUPIED_HEATING_SETPOINT_ID:
            ac_target_temp = normalize_target_temp((float)attr_i16(message) / 100.0f);
            ac_power = true;
            if (ac_mode == AC_MODE_OFF) {
                ac_mode = AC_MODE_COOL;
            }
            midea::set_target_temp(ac_target_temp);
            break;
        case ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_LOUVER_POSITION_ID:
            apply_swing(attr_u8(message) == ESP_ZB_ZCL_THERMOSTAT_LOUVER_FULLY_CLOSED ? SWING_OFF : SWING_VERTICAL);
            break;
        default:
            break;
        }
        publish_all_attrs();
        return ESP_OK;
    }

    if (message->info.cluster != ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT) {
        return ESP_OK;
    }

    switch (message->attribute.id) {
    case ATTR_AC_POWER_ID:
        apply_power(attr_bool(message));
        break;
    case ATTR_AC_MODE_ID:
        apply_mode(attr_u8(message));
        break;
    case ATTR_AC_FAN_MODE_ID:
        apply_fan(attr_u8(message));
        break;
    case ATTR_AC_SWING_MODE_ID:
        apply_swing(attr_u8(message));
        break;
    case ATTR_AC_PRESET_ID:
        apply_preset(attr_u8(message));
        break;
    case ATTR_AC_DISPLAY_ID:
        ac_display = attr_bool(message);
        midea::toggle_display();
        break;
    case ATTR_AC_TARGET_TEMP_ID:
        ac_target_temp = normalize_target_temp(attr_single(message));
        ac_power = true;
        if (ac_mode == AC_MODE_OFF) {
            ac_mode = AC_MODE_COOL;
        }
        midea::set_target_temp(ac_target_temp);
        break;
    default:
        break;
    }

    publish_all_attrs();
    return ESP_OK;
}

// ---- Zigbee OTA client ----
static const esp_partition_t *s_ota_partition = nullptr;
static esp_ota_handle_t s_ota_handle = 0;
static bool s_ota_tagid_received = false;

// Снимает служебный заголовок OTA-суб-элемента с первого блока, дальше отдаёт сырые
// данные образа приложения для записи в OTA-раздел.
static esp_err_t ota_extract_element(uint32_t total_size, const void *payload, uint16_t payload_size,
                                     void **outbuf, uint16_t *outlen)
{
    static uint16_t tagid = 0;
    if (!s_ota_tagid_received) {
        if (!payload || payload_size <= OTA_ELEMENT_HEADER_LEN) {
            ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Invalid OTA element format");
        }
        tagid = *(const uint16_t *)payload;
        uint32_t length = *(const uint32_t *)((const uint8_t *)payload + sizeof(tagid));
        if ((length + OTA_ELEMENT_HEADER_LEN) != total_size) {
            ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Invalid OTA element length [%lu/%lu]",
                                (unsigned long)length, (unsigned long)total_size);
        }
        s_ota_tagid_received = true;
        *outbuf = (void *)((const uint8_t *)payload + OTA_ELEMENT_HEADER_LEN);
        *outlen = payload_size - OTA_ELEMENT_HEADER_LEN;
    } else {
        *outbuf = (void *)payload;
        *outlen = payload_size;
    }
    if (tagid != 0 /* UPGRADE_IMAGE */) {
        ESP_RETURN_ON_ERROR(ESP_ERR_INVALID_ARG, TAG, "Unsupported OTA element tag %u", tagid);
    }
    return ESP_OK;
}

static esp_err_t zb_ota_upgrade_status_handler(esp_zb_zcl_ota_upgrade_value_message_t message)
{
    static uint32_t total_size = 0;
    static uint32_t offset = 0;
    esp_err_t ret = ESP_OK;
    if (message.info.status != ESP_ZB_ZCL_STATUS_SUCCESS) {
        return ESP_OK;
    }
    switch (message.upgrade_status) {
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_START:
        ESP_LOGI(TAG, "OTA start");
        s_ota_tagid_received = false;
        offset = 0;
        s_ota_partition = esp_ota_get_next_update_partition(nullptr);
        ESP_RETURN_ON_FALSE(s_ota_partition, ESP_ERR_NOT_FOUND, TAG, "No OTA partition");
        ret = esp_ota_begin(s_ota_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
        ESP_RETURN_ON_ERROR(ret, TAG, "esp_ota_begin failed: %s", esp_err_to_name(ret));
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_RECEIVE:
        total_size = message.ota_header.image_size;
        offset += message.payload_size;
        if (message.payload_size && message.payload) {
            uint16_t out_len = 0;
            void *out_buf = nullptr;
            ret = ota_extract_element(total_size, message.payload, message.payload_size, &out_buf, &out_len);
            ESP_RETURN_ON_ERROR(ret, TAG, "OTA element parse failed");
            if (out_len) {
                ret = esp_ota_write(s_ota_handle, out_buf, out_len);
                ESP_RETURN_ON_ERROR(ret, TAG, "esp_ota_write failed: %s", esp_err_to_name(ret));
            }
        }
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_APPLY:
        ESP_LOGI(TAG, "OTA apply");
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_CHECK:
        ret = (offset == total_size) ? ESP_OK : ESP_FAIL;
        ESP_LOGI(TAG, "OTA check [%lu/%lu]: %s", (unsigned long)offset, (unsigned long)total_size, esp_err_to_name(ret));
        offset = 0;
        total_size = 0;
        s_ota_tagid_received = false;
        break;
    case ESP_ZB_ZCL_OTA_UPGRADE_STATUS_FINISH:
        ESP_LOGW(TAG, "OTA finished, applying and rebooting");
        ret = esp_ota_end(s_ota_handle);
        ESP_RETURN_ON_ERROR(ret, TAG, "esp_ota_end failed: %s", esp_err_to_name(ret));
        ret = esp_ota_set_boot_partition(s_ota_partition);
        ESP_RETURN_ON_ERROR(ret, TAG, "set_boot_partition failed: %s", esp_err_to_name(ret));
        esp_restart();
        break;
    default:
        break;
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        return zb_attribute_handler((const esp_zb_zcl_set_attr_value_message_t *)message);
    case ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID:
        return zb_ota_upgrade_status_handler(*(const esp_zb_zcl_ota_upgrade_value_message_t *)message);
    default:
        break;
    }
    return ESP_OK;
}

static void factory_reset_button_task(void *)
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << FACTORY_RESET_GPIO;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    int64_t pressed_since = 0;
    bool reset_done = false;

    while (true) {
        bool pressed = gpio_get_level(FACTORY_RESET_GPIO) == 0;
        int64_t now_ms = esp_log_timestamp();

        if (pressed) {
            if (pressed_since == 0) {
                pressed_since = now_ms;
            } else if (!reset_done && now_ms - pressed_since >= FACTORY_RESET_HOLD_MS) {
                reset_done = true;
                ESP_LOGW(TAG, "Factory reset requested by BOOT button");
                // Zigbee-данные (сеть, биндинги) лежат в разделе zb_storage, а не
                // в основном nvs — стираем именно Zigbee-NVRAM: устройство покидает
                // сеть и перезагружается готовым к новому спариванию.
                esp_zb_factory_reset();
            }
        } else {
            pressed_since = 0;
            reset_done = false;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    xTaskCreate(factory_reset_button_task, "factory_reset", 2048, NULL, 5, NULL);

    ESP_ERROR_CHECK(midea::init(UART_TX_GPIO, UART_RX_GPIO, on_midea_state));

    esp_zb_platform_config_t platform_config = {};
    platform_config.radio_config.radio_mode = ZB_RADIO_MODE_NATIVE;
    platform_config.host_config.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE;
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform_config));

    // Router (ZR): модуль питается от кондиционера (mains) — работает роутером,
    // всегда онлайн, ретранслирует меш и надёжно принимает команды. Не спит.
    esp_zb_cfg_t zb_cfg = {};
    zb_cfg.esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER;
    zb_cfg.install_code_policy = false;
    zb_cfg.nwk_cfg.zczr_cfg.max_children = 10;
    esp_zb_init(&zb_cfg);

    // Единый fingerprint с C6 — один конвертер/определение Z2M на оба чипа.
    // Первый байт — длина Zigbee-строки. "ZB-MIDEA-AC"=11, "PirogovX"=8.
    static char model_id[] = {11, 'Z', 'B', '-', 'M', 'I', 'D', 'E', 'A', '-', 'A', 'C'};
    static char manuf_name[] = {8, 'P', 'i', 'r', 'o', 'g', 'o', 'v', 'X'};

    esp_zb_basic_cluster_cfg_t basic_cfg = {};
    basic_cfg.zcl_version = 3;
    basic_cfg.power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DC_SOURCE;
    esp_zb_attribute_list_t *basic_attr = esp_zb_basic_cluster_create(&basic_cfg);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model_id));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_attr, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manuf_name));

    esp_zb_thermostat_cluster_cfg_t therm_cfg = {};
    therm_cfg.local_temperature = thermostat_local_temp;
    therm_cfg.occupied_cooling_setpoint = thermostat_cooling_setpoint;
    therm_cfg.occupied_heating_setpoint = thermostat_heating_setpoint;
    therm_cfg.control_sequence_of_operation = thermostat_control_sequence;
    therm_cfg.system_mode = thermostat_system_mode;
    esp_zb_attribute_list_t *therm_attr = esp_zb_thermostat_cluster_create(&therm_cfg);
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_HEAT_SETPOINT_LIMIT_ID,
                                                       &thermostat_min_heat_setpoint));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_HEAT_SETPOINT_LIMIT_ID,
                                                       &thermostat_max_heat_setpoint));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_MIN_COOL_SETPOINT_LIMIT_ID,
                                                       &thermostat_min_cool_setpoint));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_MAX_COOL_SETPOINT_LIMIT_ID,
                                                       &thermostat_max_cool_setpoint));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_OUTDOOR_TEMPERATURE_ID,
                                                       &thermostat_outdoor_temp));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_RUNNING_MODE_ID,
                                                       &thermostat_running_mode));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_TYPE_ID,
                                                       &thermostat_ac_type));
    ESP_ERROR_CHECK(esp_zb_thermostat_cluster_add_attr(therm_attr, ESP_ZB_ZCL_ATTR_THERMOSTAT_AC_LOUVER_POSITION_ID,
                                                       &thermostat_louver_position));

    esp_zb_analog_input_cluster_cfg_t analog_cfg = {};
    analog_cfg.out_of_service = false;
    analog_cfg.present_value = 0;
    esp_zb_attribute_list_t *analog_attr = esp_zb_analog_input_cluster_create(&analog_cfg);
    uint8_t rw = ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
    uint8_t ro = ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_POWER_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_BOOL, rw, &ac_power));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_MODE_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_U8, rw, &ac_mode));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_FAN_MODE_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_U8, rw, &ac_fan_mode));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_SWING_MODE_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_U8, rw, &ac_swing_mode));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_PRESET_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_U8, rw, &ac_preset));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_DISPLAY_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_BOOL, rw, &ac_display));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_INDOOR_TEMP_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_SINGLE, ro, &ac_indoor_temp));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_OUTDOOR_TEMP_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_SINGLE, ro, &ac_outdoor_temp));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_TARGET_TEMP_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_SINGLE, rw, &ac_target_temp));
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_attr, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ATTR_AC_FW_VERSION_ID,
                                            ESP_ZB_ZCL_ATTR_TYPE_U32, ro, &firmware_version));

    // OTA client cluster — обновление прошивки по воздуху через Z2M.
    esp_zb_ota_cluster_cfg_t ota_cfg = {};
    ota_cfg.ota_upgrade_file_version = firmware_version;
    ota_cfg.ota_upgrade_downloaded_file_ver = firmware_version;
    ota_cfg.ota_upgrade_manufacturer = OTA_MANUFACTURER;
    ota_cfg.ota_upgrade_image_type = OTA_IMAGE_TYPE;
    esp_zb_attribute_list_t *ota_attr = esp_zb_ota_cluster_create(&ota_cfg);
    esp_zb_zcl_ota_upgrade_client_variable_t ota_var = {};
    ota_var.timer_query = ESP_ZB_ZCL_OTA_UPGRADE_QUERY_TIMER_COUNT_DEF;
    ota_var.hw_version = OTA_HW_VERSION;
    ota_var.max_data_size = OTA_MAX_DATA_SIZE;
    uint16_t ota_server_addr = 0xffff;
    uint8_t ota_server_ep = 0xff;
    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(ota_attr, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_CLIENT_DATA_ID, &ota_var));
    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(ota_attr, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ADDR_ID, &ota_server_addr));
    ESP_ERROR_CHECK(esp_zb_ota_cluster_add_attr(ota_attr, ESP_ZB_ZCL_ATTR_OTA_UPGRADE_SERVER_ENDPOINT_ID, &ota_server_ep));

    esp_zb_cluster_list_t *clusters = esp_zb_zcl_cluster_list_create();
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(clusters, basic_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_thermostat_cluster(clusters, therm_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(clusters, analog_attr, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_ota_cluster(clusters, ota_attr, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    esp_zb_endpoint_config_t ep_config = {};
    ep_config.endpoint = ZIGBEE_ENDPOINT;
    ep_config.app_profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    ep_config.app_device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;
    ep_config.app_device_version = 1;
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    ESP_ERROR_CHECK(esp_zb_ep_list_add_ep(ep_list, clusters, ep_config));
    ESP_ERROR_CHECK(esp_zb_device_register(ep_list));
    esp_zb_core_action_handler_register(zb_action_handler);

    publish_all_attrs();
    ESP_ERROR_CHECK(esp_zb_start(true));
    while (true) {
        esp_zb_main_loop_iteration();
    }
}

// Нашли OTA-сервер (координатор) — включаем периодический опрос и запрашиваем образ.
static void ota_match_server_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *)
{
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "OTA server found (addr 0x%04x ep %d)", addr, endpoint);
        esp_zb_ota_upgrade_client_query_interval_set(ZIGBEE_ENDPOINT, OTA_QUERY_INTERVAL_MIN);
        esp_zb_ota_upgrade_client_query_image_req(addr, endpoint);
    } else {
        ESP_LOGW(TAG, "OTA server not found");
    }
}

// Ищем на координаторе кластер OTA Upgrade, чтобы устройство знало, куда слать запросы.
static void ota_find_server(void)
{
    static uint16_t ota_cluster = ESP_ZB_ZCL_CLUSTER_ID_OTA_UPGRADE;
    esp_zb_zdo_match_desc_req_param_t req = {};
    req.addr_of_interest = 0x0000;
    req.dst_nwk_addr = 0x0000;
    req.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
    req.num_in_clusters = 0;
    req.num_out_clusters = 1;
    req.cluster_list = &ota_cluster;
    esp_zb_zdo_match_cluster(&req, ota_match_server_cb, nullptr);
}

extern "C" void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*(signal_struct->p_app_signal);
    esp_err_t err_status = signal_struct->esp_err_status;
    ESP_LOGI(TAG, "Zigbee signal: %d, status: %s", sig_type, esp_err_to_name(err_status));

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Joined Zigbee network");
            midea::request_status();
            ota_find_server();
        }
        break;
    default:
        break;
    }
}
