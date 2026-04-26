# ESP32 Sensor Hub

Arduino CLI based ESP32-S3 sensor hub for the ALIENTEK DNESP32S3 board.

## Features

- DHT11 temperature and humidity with raw frame bytes and explicit effective resolution
- AP3216C ambient light / IR / proximity
- QMA6100P motion and posture
- ES8388 microphone level capture
- ES8388 board speaker playback and hardware verification
- ADC input voltage telemetry with persisted raw, pin-mV and scaled voltage fields
- MC5640 / OV5640 board camera JPEG snapshots in the HTML dashboard
- OPI PSRAM enabled for camera frame buffers
- Hardware control panel for camera settings, selected ESP32 controls and safe device register access
- Dropdown peripheral control presets for AP3216C thresholds/calibration/interrupt clear, QMA6100P bandwidth/power/interrupt/motion features, and ES8388 input/gain/ALC/3D/sample-rate/EQ settings
- CPU-linked status LED blink rate telemetry
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
& 'C:\Program Files\Arduino CLI\arduino-cli.exe' compile --fqbn 'esp32:esp32:esp32s3:PSRAM=opi' --build-property 'build.extra_flags=-DWIFI_STA_SSID="Ziroom402" -DWIFI_STA_PASS="4001001111" -DADC_VOLTAGE_SCALE=1.0' 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub'
```

## 输入电压 ADC

固件默认按正点原子 ADC 示例使用 `GPIO8`（ESP32-S3 `ADC1_CHANNEL_7`）作为输入电压采样脚。`/api/status`、`/api/live`、`/api/history`、CSV、HTML 看板和 LCD 系统页都会显示：

- `raw`：12 位 ADC 原始值。
- `millivolts`：ADC 引脚电压，单位 mV。
- `voltage_v`：按 `ADC_VOLTAGE_SCALE` 换算后的输入电压，单位 V。

注意：ESP32 ADC 引脚不能直接接 5V、USB VBUS、12V 或其他高于 3.3V 的输入。测输入电压必须先用电阻分压把 ADC 引脚电压压到安全范围，再把 `ADC_VOLTAGE_SCALE` 设成分压倍率。例如 100k/100k 二分压测 5V 时，ADC 脚约 2.5V，`ADC_VOLTAGE_SCALE=2.0`。如果硬件接到其他 ADC 脚，可以编译时加 `-DADC_VOLTAGE_PIN=<gpio>` 覆盖。

## Verify

```powershell
& 'C:\Users\lyl\Desktop\ESP32\esp32_sensor_hub\ci\verify_sensor_hub.ps1' -WifiSsid 'Ziroom402' -WifiPassword '4001001111'
```

The verification script uploads through the detected ESP32 serial port by default. When the board is behind SmartUSBHub, discover the hub control port by `VID_1A86 PID_FE0C` and leave the tested channel power and dataline enabled; do not hard-code the hub control COM port or confuse it with the ESP32 CH340 upload port.

The default verification path skips host USB camera checks and focuses on the board sensor hub:

- Arduino CLI build/upload
- OPI PSRAM detection through `/api/status`
- live API, HTML, CSV, LCD and speaker playback checks
- ADC input voltage fields in status, live, history, CSV and dashboard HTML
- MC5640 camera status, JPEG capture, camera control and safe register console checks
- MC5640 MJPEG stream cadence and JPEG latency checks
- camera one-click presets, AP3216C mode/threshold/calibration/interrupt presets, QMA6100P range/bandwidth/power/interrupt/motion presets and ES8388 volume/input/gain/ALC/3D/sample-rate/EQ preset readback checks
- camera controls while MJPEG streaming is active, including stream pause and post-control capture recovery
- dashboard edit-lock checks so 0.5s live polling does not overwrite camera controls while the user is editing
- dropdown control checks for camera advanced settings, ES8388 volume presets and common safe register presets
- CPU-linked status LED half-period and blink frequency checks in `/api/status` and `/api/live`
- executable host-side dashboard JavaScript edit-lock behavior test through `ci/test_dashboard_edit_lock.js`
- compact health endpoint checks through `/api/health`
- cached lightweight live endpoint checks through `/api/live`
- boot readiness telemetry checks through `/api/status`
- LittleFS config persistence through `/api/config`
- forced log flush through `/api/flush`
- reboot recovery through `/api/reboot`
- short soak sampling and heap stability

Pass `-IncludeCamera` only when host USB camera artifacts are part of the current acceptance target.

## DHT 温湿度精度

DHT11 本身通常只有 `1 C` 和 `1 %RH` 有效分辨率；固件不会把真实小数截掉，但会在 `/api/status` 和 `/api/live` 中暴露原始 5 字节帧里的整数、小数和校验字节、`resolution_c` 和 `resolution_humidity`，方便确认传感器实际输出。如果换成 DHT22/AM2302 兼容传感器，可以编译时设置 `-DDHT_SENSOR_TYPE=22`，固件会按 0.1 单位解析。

## 外设寄存器设置

Dashboard 的硬件控制台只接受十进制整数。寄存器地址范围和写入策略如下：

- AP3216C：地址 0 到 255 可读；常用地址 0 读系统模式，10 到 15 读红外、环境光和接近数据。普通寄存器写入仍被拦截；工作模式、ALS/PS 阈值、ALS/PS 校准和中断清除策略通过下拉预设写入并回读。ALS 校准默认无补偿值为 64，即系数 1.0。
- ES8388：地址 0 到 255 可读；只允许写音量寄存器 46 到 49，最终写入值必须保持 0 到 33。麦克风增益、输入通道、ALC、3D、采样率和 EQ 通过下拉预设写入并回读。
- QMA6100P：地址 0 到 255 可读；常用地址 0 读芯片标识，1 到 6 读加速度原始数据。普通寄存器写入仍被拦截；量程、带宽、电源模式、中断锁存、步数中断、敲击检测和运动检测通过下拉预设写入并回读。
- XL9555：地址 0 到 7 只读；写入会影响屏幕、摄像头复位或电源控制，因此被固件拦截。
- OV5640：地址 0 到 65535 只读诊断；亮度、曝光、增益和镜像使用摄像头设置区。

摄像头设置写入后会回读 `/api/camera`，只有 `verified=true` 或页面显示“已生效”才算设置真正生效。
一键摄像头预设通过 `/api/camera/preset` 执行并回读确认；外设预设通过 `/api/peripheral/control` 执行并回读确认。
页面里的正常摄像头和外设设置都用下拉框选择；常用寄存器也提供下拉预设，手动十进制寄存器输入只保留作诊断。摄像头和外设控件有编辑锁，用户改值时，0.5 秒 live 刷新不会把控件跳回旧值；点击“设”或一键预设后才同步硬件回读值。

## Issue Workflow

- Bugs and feature work should start from a GitHub issue.
- The assignee should not close an issue until a subagent has reviewed the implementation or verification result.
- CI / BDD evidence should be attached before closing hardware-impacting issues.

## License

MIT
