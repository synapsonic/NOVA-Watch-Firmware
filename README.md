# NOVA Watch Firmware — v0.1 architecture

NOVA is an AI-first smartwatch firmware scaffold for the Waveshare ESP32-S3-Touch-AMOLED-1.43C (ESP32-S3-PICO-1-N8R8, 8 MB Flash / 8 MB PSRAM).

## Included
- ESP-IDF project skeleton
- NVS initialization
- Wi-Fi station mode + reconnect
- Bluetooth LE / NimBLE initialization
- Audio abstraction for the onboard ES7210 dual-mic path + ES8311 codec
- Remote transcription endpoint abstraction (`POST /v1/transcribe`)
- AI agent endpoint abstraction (`POST /v1/agent`)
- HTTPS certificate bundle support
- Power/status abstraction
- No credentials committed

## Important hardware note
The Waveshare 1.43C has board-specific display/touch/audio/power initialization. NOVA deliberately does **not** invent GPIO mappings. Bind the exact Waveshare ESP-IDF BSP/example for your board revision into the HAL before expecting microphone, speaker, AMOLED or touch to work.

## AI architecture
The ESP32-S3 handles the UI, sensors, audio capture, connectivity, wake/button logic and small local models/rules. Full speech-to-text and general LLM reasoning are exposed as configurable HTTPS services. This is more practical than pretending a general-purpose LLM can run entirely inside 8 MB PSRAM.

Expected backend contract:
- `POST /v1/transcribe` — PCM16 mono 16 kHz body -> UTF-8 transcript (or adapt parser to JSON)
- `POST /v1/agent` — JSON `{device,prompt}` -> UTF-8 response
- Future: `/v1/tts`, tool calls, streaming, device auth

## Build
Install ESP-IDF, then:

    idf.py set-target esp32s3
    idf.py build
    idf.py flash monitor

## Next implementation milestones
1. Import Waveshare 1.43C ESP-IDF board drivers (CO5300 AMOLED, CST820 touch, ES7210, ES8311, power IC).
2. Add LVGL circular UI.
3. Add BLE GATT provisioning and phone companion protocol.
4. Capture 16 kHz mono PCM from microphones.
5. Add push-to-talk -> transcription -> agent -> TTS pipeline.
6. Add encrypted NVS credentials and per-device API keys.
7. Add OTA with signed firmware and rollback.
8. Add battery/thermal safeguards and sleep modes.
9. Add local wake word / VAD where feasible.
10. Add sensors and hardware expansion API for future custom PCB.

## Security model
- Never hard-code production Wi-Fi or AI credentials.
- Use TLS and per-device credentials.
- Require explicit confirmation for consequential actions.
- Use signed OTA images and rollback before shipping.
- Keep tool permissions allow-listed; never execute arbitrary AI-generated firmware/system commands.
