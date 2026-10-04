# NOVA architecture

## Runtime pipeline
Button/wake -> microphone -> VAD/audio buffer -> STT -> transcript -> agent -> structured response/tool call -> UI/TTS.

## FreeRTOS task plan
- ui_task: LVGL and touch
- audio_task: microphone ring buffer / playback
- network_task: Wi-Fi state and HTTPS
- ble_task: provisioning / companion sync
- ai_task: STT + agent state machine
- power_task: battery, thermal policy, sleep
- sensor_task: optional IMU/environment/custom modules

## Proposed BLE services
- Device info
- Secure provisioning
- Notification bridge
- Companion command channel
- Sensor/event stream

## AI execution tiers
Tier 0 local: timers, deterministic commands, sensor thresholds.
Tier 1 local ML: wake word / VAD / small classifier if model fits.
Tier 2 remote: speech recognition, general reasoning, multimodal tasks.

## Custom-PCB future
Keep board drivers behind HALs so Waveshare prototype drivers can later be replaced by NOVA PCB drivers without rewriting the application layer.
