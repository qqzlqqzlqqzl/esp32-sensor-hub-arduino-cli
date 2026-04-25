# ESP32 Sensor Hub

Arduino CLI based ESP32-S3 sensor hub for the ALIENTEK DNESP32S3 board.

## Features

- DHT11 temperature and humidity
- AP3216C ambient light / IR / proximity
- QMA6100P motion and posture
- ES8388 microphone level capture
- ES8388 board speaker playback and hardware verification
- MC5640 / OV5640 board camera JPEG snapshots in the HTML dashboard
- OPI PSRAM enabled for camera frame buffers
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
- Board connected on an Arduino CLI detectable serial port

Example:

```powershell
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32s3:PSRAM=opi' --build-property 'build.extra_flags=-DWIFI_STA_SSID="Ziroom402" -DWIFI_STA_PASS="4001001111"' 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub'
```

## Verify

```powershell
& 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\verify_sensor_hub.ps1' -WifiSsid 'Ziroom402' -WifiPassword '4001001111'
```

The verification script uploads through the detected ESP32 serial port by default. When the board is behind SmartUSBHub, discover the hub control port by `VID_1A86 PID_FE0C` and leave the tested channel power and dataline enabled; do not hard-code the hub control COM port or confuse it with the ESP32 CH340 upload port.

The default verification path skips host USB camera checks and focuses on the board sensor hub:

- Arduino CLI build/upload
- OPI PSRAM detection through `/api/status`
- live API, HTML, CSV, LCD and speaker playback checks
- MC5640 camera status, JPEG capture, camera control and safe register console checks
- MC5640 MJPEG stream cadence and JPEG latency checks
- camera one-click presets, AP3216C mode, QMA6100P range and ES8388 volume preset readback checks
- dashboard edit-lock checks so 0.5s live polling does not overwrite camera controls while the user is editing
- executable host-side dashboard JavaScript edit-lock behavior test through `ci/test_dashboard_edit_lock.js`
- compact health endpoint checks through `/api/health`
- cached lightweight live endpoint checks through `/api/live`
- boot readiness telemetry checks through `/api/status`
- LittleFS config persistence through `/api/config`
- forced log flush through `/api/flush`
- reboot recovery through `/api/reboot`
- short soak sampling and heap stability

Pass `-IncludeCamera` only when host USB camera artifacts are part of the current acceptance target.

## 外设寄存器设置

Dashboard 的硬件控制台只接受十进制整数。寄存器地址范围和写入策略如下：

- AP3216C：地址 0 到 255 只读；常用地址 0 读系统模式，10 到 15 读红外、环境光和接近数据。
- ES8388：地址 0 到 255 可读；只允许写音量寄存器 46 到 49，写入值 0 到 33，掩码通常填 255。
- QMA6100P：地址 0 到 255 只读；常用地址 0 读芯片标识，1 到 6 读加速度原始数据。
- XL9555：地址 0 到 7 只读；写入会影响屏幕、摄像头复位或电源控制，因此被固件拦截。
- OV5640：地址 0 到 65535 只读诊断；亮度、曝光、增益和镜像使用摄像头设置区。

摄像头设置写入后会回读 `/api/camera`，只有 `verified=true` 或页面显示“已生效”才算设置真正生效。
一键摄像头预设通过 `/api/camera/preset` 执行并回读确认；外设预设通过 `/api/peripheral/control` 执行并回读确认。
页面里的摄像头滑条和下拉框有编辑锁，用户拖动或改值时，0.5 秒 live 刷新不会把控件跳回旧值；点击“设”或一键预设后才同步硬件回读值。

## Issue Workflow

- Bugs and feature work should start from a GitHub issue.
- The assignee should not close an issue until a subagent has reviewed the implementation or verification result.
- CI / BDD evidence should be attached before closing hardware-impacting issues.

## License

MIT
