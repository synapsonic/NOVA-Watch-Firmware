#include "nova_core.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "driver/i2s_std.h"

static const char *TAG = "NOVA";
static nova_status_t s_status = {.battery_percent = -1};
static char s_ai_url[192] = "";
static char s_token[192] = "";
static EventGroupHandle_t s_wifi_events;
#define WIFI_OK BIT0

esp_err_t nova_storage_init(void) {
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    return e;
}

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_status.wifi_connected = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_status.wifi_connected = true;
        xEventGroupSetBits(s_wifi_events, WIFI_OK);
    }
}

esp_err_t nova_wifi_init(void) {
    s_wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    return esp_wifi_start();
}

esp_err_t nova_wifi_connect(const char *ssid, const char *password) {
    wifi_config_t cfg = {0};
    snprintf((char*)cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid ? ssid : "");
    snprintf((char*)cfg.sta.password, sizeof(cfg.sta.password), "%s", password ? password : "");
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_connect());
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, WIFI_OK, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));
    return (bits & WIFI_OK) ? ESP_OK : ESP_ERR_TIMEOUT;
}

static void ble_host_task(void *param) { nimble_port_run(); nimble_port_freertos_deinit(); }
static void ble_on_sync(void) { s_status.ble_ready = true; }

esp_err_t nova_ble_init(void) {
    esp_err_t e = nimble_port_init();
    if (e != ESP_OK) return e;
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
    return ESP_OK;
}

esp_err_t nova_audio_init(void) {
    /* Hardware abstraction point for Waveshare 1.43C ES7210 + ES8311.
       Pin/clock programming should be copied from the exact Waveshare ESP-IDF
       board example matching the PCB revision. Keeping it isolated here avoids
       baking guessed pin assignments into NOVA. */
    s_status.audio_ready = true;
    ESP_LOGW(TAG, "Audio HAL placeholder ready: bind ES7210/ES8311 from Waveshare BSP before capture/playback");
    return ESP_OK;
}

esp_err_t nova_audio_capture_pcm16(int16_t *samples, size_t max_samples, size_t *written) {
    if (!samples || !written || max_samples == 0) return ESP_ERR_INVALID_ARG;
    *written = 0;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t http_post_binary(const char *path, const char *content_type, const void *body, int len, char *out, size_t out_len) {
    if (!s_ai_url[0]) return ESP_ERR_INVALID_STATE;
    char url[256];
    snprintf(url, sizeof(url), "%s%s", s_ai_url, path);
    esp_http_client_config_t cfg = {.url = url, .timeout_ms = 30000, .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_FAIL;
    esp_http_client_set_method(c, HTTP_METHOD_POST);
    esp_http_client_set_header(c, "Content-Type", content_type);
    if (s_token[0]) { char auth[224]; snprintf(auth, sizeof(auth), "Bearer %s", s_token); esp_http_client_set_header(c, "Authorization", auth); }
    esp_err_t e = esp_http_client_open(c, len);
    if (e == ESP_OK) {
        esp_http_client_write(c, body, len);
        esp_http_client_fetch_headers(c);
        int n = esp_http_client_read_response(c, out, out_len - 1);
        if (n >= 0) out[n] = 0; else e = ESP_FAIL;
    }
    esp_http_client_cleanup(c);
    return e;
}

esp_err_t nova_ai_transcribe(const int16_t *samples, size_t sample_count, char *out, size_t out_len) {
    s_status.ai_state = NOVA_AI_TRANSCRIBING;
    esp_err_t e = http_post_binary("/v1/transcribe", "audio/pcm;rate=16000;encoding=signed-integer;bits=16;channels=1", samples, (int)(sample_count * sizeof(int16_t)), out, out_len);
    s_status.ai_state = e == ESP_OK ? NOVA_AI_IDLE : NOVA_AI_ERROR;
    return e;
}

esp_err_t nova_ai_ask(const char *prompt, char *out, size_t out_len) {
    if (!prompt) return ESP_ERR_INVALID_ARG;
    s_status.ai_state = NOVA_AI_THINKING;
    char body[1024];
    snprintf(body, sizeof(body), "{\"device\":\"nova-watch\",\"prompt\":\"%s\"}", prompt);
    esp_err_t e = http_post_binary("/v1/agent", "application/json", body, strlen(body), out, out_len);
    s_status.ai_state = e == ESP_OK ? NOVA_AI_IDLE : NOVA_AI_ERROR;
    return e;
}

esp_err_t nova_tts_speak(const char *text) {
    /* Endpoint can return PCM in a production implementation; ES8311 playback
       belongs in the board audio HAL. */
    (void)text;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t nova_power_init(void) { return ESP_OK; }
void nova_get_status(nova_status_t *status) { if (status) *status = s_status; }
void nova_set_ai_endpoint(const char *base_url, const char *bearer_token) {
    snprintf(s_ai_url, sizeof(s_ai_url), "%s", base_url ? base_url : "");
    snprintf(s_token, sizeof(s_token), "%s", bearer_token ? bearer_token : "");
}
