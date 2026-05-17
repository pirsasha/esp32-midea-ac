#include "esp_zigbee_core.h"
#include "esp_log.h"

static const char *TAG = "zb_app";

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    ESP_LOGI(TAG, "Received Zigbee signal with status: %s", esp_err_to_name(signal_struct->esp_err_status));
}
