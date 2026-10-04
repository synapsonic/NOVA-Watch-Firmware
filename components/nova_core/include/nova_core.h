#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    NOVA_AI_IDLE = 0,
    NOVA_AI_LISTENING,
    NOVA_AI_TRANSCRIBING,
    NOVA_AI_THINKING,
    NOVA_AI_SPEAKING,
    NOVA_AI_ERROR
} nova_ai_state_t;

typedef struct {
    bool wifi_connected;
    bool ble_ready;
    bool audio_ready;
    nova_ai_state_t ai_state;
    int battery_percent;
    float chip_temperature_c;
} nova_status_t;

esp_err_t nova_storage_init(void);
esp_err_t nova_wifi_init(void);
esp_err_t nova_wifi_connect(const char *ssid, const char *password);
esp_err_t nova_ble_init(void);
esp_err_t nova_audio_init(void);
esp_err_t nova_audio_capture_pcm16(int16_t *samples, size_t max_samples, size_t *written);
esp_err_t nova_ai_transcribe(const int16_t *samples, size_t sample_count, char *out, size_t out_len);
esp_err_t nova_ai_ask(const char *prompt, char *out, size_t out_len);
esp_err_t nova_tts_speak(const char *text);
esp_err_t nova_power_init(void);
void nova_get_status(nova_status_t *status);
void nova_set_ai_endpoint(const char *base_url, const char *bearer_token);
