#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nova_core.h"

static const char *TAG = "NOVA_MAIN";

/* Put secrets in provisioning/NVS in production. These are intentionally blank. */
#define NOVA_WIFI_SSID ""
#define NOVA_WIFI_PASSWORD ""
#define NOVA_AI_BASE_URL ""
#define NOVA_AI_TOKEN ""

void app_main(void) {
    ESP_LOGI(TAG, "NOVA Watch firmware boot");
    ESP_ERROR_CHECK(nova_storage_init());
    ESP_ERROR_CHECK(nova_power_init());
    ESP_ERROR_CHECK(nova_wifi_init());
    ESP_ERROR_CHECK(nova_ble_init());
    ESP_ERROR_CHECK(nova_audio_init());
    nova_set_ai_endpoint(NOVA_AI_BASE_URL, NOVA_AI_TOKEN);

    if (NOVA_WIFI_SSID[0]) {
        esp_err_t e = nova_wifi_connect(NOVA_WIFI_SSID, NOVA_WIFI_PASSWORD);
        ESP_LOGI(TAG, "Wi-Fi: %s", esp_err_to_name(e));
    } else {
        ESP_LOGW(TAG, "Wi-Fi credentials not provisioned yet");
    }

    while (1) {
        nova_status_t s;
        nova_get_status(&s);
        ESP_LOGI(TAG, "wifi=%d ble=%d audio=%d ai=%d", s.wifi_connected, s.ble_ready, s.audio_ready, s.ai_state);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
