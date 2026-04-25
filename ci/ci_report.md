# ESP32 Sensor Hub CI Report

Generated: 2026-04-25 10:52:57

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, MC5640 camera status, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- Boot-to-dashboard readiness exposes setup timing telemetry and limits boot history parsing to the latest dashboard rows
- Camera JPEG capture, effective camera controls and safe decimal hardware register access are verified through HTTP APIs
- Peripheral register controls expose Chinese decimal guidance, safe writable ranges, and blocked unsafe writes
- Camera dashboard uses a dedicated MJPEG stream on port 81 with a measured 20 FPS target
- HTML dashboard uses completion-based /api/live polling for 0.5s visible telemetry refresh and keeps full status/history snapshots at 10s
- Hardware CI runs a 2Hz /api/live soak to check live polling stability
- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability
- LittleFS data and config survive a board reboot
- Short soak verifies continued sampling, storage writes and heap stability
- Host USB camera capture is optional and skipped unless requested

## Step Results
[PASS] Build: Sketch uses 1203385 bytes (91%) of program storage space. Maximum is 1310720 bytes.
Global variables use 183160 bytes (55%) of dynamic memory, leaving 144520 bytes for local variables. Maximum is 327680 bytes.
[PASS] Serial Port: using detected port COM7; requested=(auto); available=COM1, COM7
[PASS] Upload: Writing at 0x0012f656... (97 %)
Writing at 0x00135460... (100 %)
Wrote 1203744 bytes (771685 compressed) at 0x00010000 in 19.4 seconds (effective 495.4 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-48},"storage":{"used_bytes":1028096,"total_bytes":1441792,"persisted_samples":7125,"last_sample_epoch":1777085348,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":27.00,"humidity":6.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":52.19},"ap3216c":{"online":true,"als":16,"ir":11,"ps":79},"qma6100p":{"online":true,"ax":0.057,"ay":-0.345,"az":10.199,"ag":10.205,"pitch":-0.32,"roll":-1.94},"mic":{"online":true,"dbfs":-70.28,"rms":0.00031,"peak":33},"system":{"runtime_ready":true,"cpu_usage_pct":0.00,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":80448,"min_free_heap_bytes":70848,"max_alloc_heap_bytes":69620,"uptime_sec":9,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":2,"page_name":"SYSTEM"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-55.96,"enabled_dbfs":-30.09,"delta_dbfs":25.87,"muted_peak":121,"enabled_peak":4817,"baseline_dbfs":-55.96,"observed_dbfs":-30.09,"observed_peak":4817,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9274},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":false,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":6,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":370,"littlefs_mount_ms":7,"history_load_ms":1164,"history_rows_counted":7124,"history_rows_loaded":120,"config_load_ms":68,"network_start_ms":73,"display_init_ms":308,"sensors_init_ms":1078,"server_start_ms":2,"setup_complete_ms":3489,"first_url_report_ms":3489}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-55.96,"enabled_dbfs":-30.09,"delta_dbfs":25.87,"muted_peak":121,"enabled_peak":4817,"baseline_dbfs":-55.96,"observed_dbfs":-30.09,"observed_peak":4817,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9274}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-55.96,"enabled_dbfs":-30.09,"delta_dbfs":25.87,"muted_peak":121,"enabled_peak":4817,"baseline_dbfs":-55.96,"observed_dbfs":-30.09,"observed_peak":4817,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9274}
[PASS] Boot Telemetry: setup_ms=3489 history_ms=1164 rows_counted=7124 rows_loaded=120 first_url_ms=3489 server_ms=2 display_ms=308 sensors_ms=1078
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":9,"free_heap_bytes":80448,"min_free_heap_bytes":70848,"persisted_samples":7125,"last_sample_epoch":1777085348,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"camera_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0,"setup_complete_ms":3489,"history_load_ms":1164}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-48},"storage":{"persisted_samples":7125,"last_sample_epoch":1777085348,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true,"live_build_count":1,"live_cache_age_ms":0},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"live_cache_ttl_ms":1000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":27.00,"humidity":6.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":52.19},"ap3216c":{"online":true,"als":16,"ir":9,"ps":76},"qma6100p":{"online":true,"ax":0.134,"ay":-0.287,"az":10.123,"ag":10.128,"pitch":-0.76,"roll":-1.63},"mic":{"online":true,"dbfs":-70.28,"rms":0.00031,"peak":33},"system":{"runtime_ready":true,"cpu_usage_pct":0.00,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":80448,"min_free_heap_bytes":70848,"max_alloc_heap_bytes":69620,"uptime_sec":9,"reset_reason":"POWERON"},"display":{"online":true,"page":2,"page_name":"SYSTEM"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-55.96,"enabled_dbfs":-30.09,"delta_dbfs":25.87,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":false,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":6,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":370,"littlefs_mount_ms":7,"history_load_ms":1164,"history_rows_counted":7124,"history_rows_loaded":120,"config_load_ms":68,"network_start_ms":73,"display_init_ms":308,"sensors_init_ms":1078,"server_start_ms":2,"setup_complete_ms":3489,"first_url_report_ms":3489}}
[PASS] Camera Capture And Controls: online=True name=OV5640 pid=22080 jpg_bytes=1558 avg_jpg_ms=63 max_jpg_ms=84 stream_bytes=99552 stream_frames=61 stream_fps=20.1 capture_before=0 capture_after=67 failures_before=0 failures_after=0 reg_value=3 quality=18 quality_verified=True brightness_verified=True invalid_quality=out_of_range hex_reg=invalid_request es8388_volume=18 unsafe_write=write_blocked bad_frame=unsupported_framesize bad_value=invalid_value bad_device=invalid_request wrapped_reg=reg_out_of_range
[PASS] API Live 2Hz Soak: duration_s=20 ok_responses=40 failed_responses=0 live_builds=17 heap_before=73796 heap_after=73820 min_heap=65696 heap_drop=-24 max_cpu_pct=28.49
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-55.96,"enabled_dbfs":-30.09,"delta_dbfs":25.87,"muted_peak":121,"enabled_peak":4817,"baseline_dbfs":-55.96,"observed_dbfs":-30.09,"observed_peak":4817,"pass_count":1,"fail_count":0,"speak_count":1,"last_spoken_temp_c":27,"has_spoken_temp":true,"last_test_ms":9274}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=42557
[PASS] Reboot Persistence: before_persisted=7133 after_persisted=7133 before_uptime=93 after_uptime=5 before_temp_high=61.5 after_temp_high=61.5 history_rows=60
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=7133 persisted_after=7140 heap_before=80584 heap_after=74356 heap_min_seen=74344 heap_drop=6228
[PASS] CSV Log: lines=7140
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=14 ir=8 ps=73 | mic=on -70.03dBFS peak=31 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=on g=10.19 p=0.6 r=-1.5 | chip=on 50.87C | cpu=0.0% heap=78KB | fs=1028096/1441792 persisted=7124 pending=0 fail=0 drop=0 | lcd=on p1
[BOOT] serial=370 littlefs=7 history=1164 rows=7124 loaded=120 config=68 network=73 display=308 sensors=1078 server=2 setup=3489 first_url=3489
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
