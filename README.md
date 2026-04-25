# ESP32 Sensor Hub

Arduino CLI based ESP32-S3 sensor hub for the ALIENTEK DNESP32S3 board.

## Features

- DHT11 temperature and humidity
- AP3216C ambient light / IR / proximity
- QMA6100P motion and posture
- ES8388 microphone level capture
- ES8388 board speaker playback and hardware verification
- MC5640 / OV5640 board camera JPEG snapshots in the HTML dashboard
- Hardware control panel for camera settings, selected ESP32 controls and safe device register access
- LittleFS CSV persistence
- Fast boot history recovery with boot timing telemetry
- LittleFS-backed alert threshold configuration
- Compact `/api/health` endpoint for unattended polling
- Cached lightweight `/api/live` endpoint for 0.5s visible dashboard refresh
- API / dashboard alert state
- LCD offline dashboard pages
- HTTP dashboard with charts and CSV export
- Reboot recovery and short soak CI checks
- Optional USB camera capture artifacts for lab verification
- BDD and CI verification scripts

## Build

Requirements:

- Arduino CLI
- ESP32 core for Arduino
- Board connected on the expected serial port

Example:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile -u -p COM7 --fqbn esp32:esp32:esp32s3 --build-property 'build.extra_flags=-DWIFI_STA_SSID="Ziroom402" -DWIFI_STA_PASS="4001001111"' 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub'
```

## Verify

```powershell
& 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\verify_sensor_hub.ps1' -WifiSsid 'Ziroom402' -WifiPassword '4001001111'
```

The default verification path skips host USB camera checks and focuses on the board sensor hub:

- Arduino CLI build/upload
- live API, HTML, CSV, LCD and speaker playback checks
- MC5640 camera status, JPEG capture, camera control and safe register console checks
- MC5640 dashboard refresh cadence and JPEG latency checks
- compact health endpoint checks through `/api/health`
- cached lightweight live endpoint checks through `/api/live`
- boot readiness telemetry checks through `/api/status`
- LittleFS config persistence through `/api/config`
- forced log flush through `/api/flush`
- reboot recovery through `/api/reboot`
- short soak sampling and heap stability

Pass `-IncludeCamera` only when host USB camera artifacts are part of the current acceptance target.

## Issue Workflow

- Bugs and feature work should start from a GitHub issue.
- The assignee should not close an issue until a subagent has reviewed the implementation or verification result.
- CI / BDD evidence should be attached before closing hardware-impacting issues.

## License

MIT
