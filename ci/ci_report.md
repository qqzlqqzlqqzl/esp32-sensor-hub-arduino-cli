# ESP32 Sensor Hub CI Report

Generated: 2026-04-25 21:16:04

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI using OPI PSRAM enabled
- Live dashboard exposes sensor telemetry, MC5640 camera status, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- Boot-to-dashboard readiness exposes setup timing telemetry and limits boot history parsing to the latest dashboard rows
- LCD recovery can be triggered without a board power-cycle, holds a high-contrast visible pixel test pattern, and proves XL9555 power/reset pins are outputs driven high after reinit and software reboot
- Camera JPEG capture, OPI PSRAM availability, effective camera controls and safe decimal hardware register access are verified through HTTP APIs
- Peripheral register controls expose Chinese decimal guidance, safe writable ranges, and blocked unsafe writes
- Dashboard controls keep user edits during 0.5s live refresh, with executable JavaScript behavior coverage, and expose verified camera/AP3216C/QMA6100P/ES8388 presets
- Camera dashboard uses a dedicated MJPEG stream on port 81 with a measured 20 FPS target
- HTML dashboard uses completion-based /api/live polling for 0.5s visible telemetry refresh and keeps full status/history snapshots at 10s
- Hardware CI runs a 2Hz /api/live soak to check live polling stability
- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability
- LittleFS data and config survive a board reboot
- Short soak verifies continued sampling, storage writes and heap stability
- Host USB camera capture is optional and skipped unless requested

## Step Results
[PASS] Build: Sketch uses 1222429 bytes (93%) of program storage space. Maximum is 1310720 bytes.
Global variables use 183416 bytes (55%) of dynamic memory, leaving 144264 bytes for local variables. Maximum is 327680 bytes.
[PASS] Dashboard Edit Lock Behavior: DASHBOARD_EDIT_LOCK_BEHAVIOR_PASS
[PASS] Serial Port: using detected port COM11; requested=(auto); available=COM1, COM11, COM9
[PASS] Upload: Writing at 0x001316e1... (97 %)
Writing at 0x00137230... (100 %)
Wrote 1222800 bytes (779854 compressed) at 0x00010000 in 19.8 seconds (effective 495.0 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-52},"storage":{"used_bytes":1024000,"total_bytes":1441792,"persisted_samples":7119,"last_sample_epoch":1777122745,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":28.00,"humidity":7.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":52.62},"ap3216c":{"online":true,"als":0,"ir":1,"ps":84,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":0.163,"ay":-0.345,"az":10.132,"ag":10.139,"pitch":-0.92,"roll":-1.95},"mic":{"online":true,"dbfs":-35.09,"rms":0.01760,"peak":868},"system":{"runtime_ready":true,"cpu_usage_pct":8.26,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":98592,"min_free_heap_bytes":97492,"max_alloc_heap_bytes":90100,"uptime_sec":9,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":3,"page_name":"STORAGE","init_count":1,"reinit_count":0,"visible_test_count":1,"prepare_restart_count":0,"last_init_ms":652,"last_reinit_ms":0,"last_visible_test_ms":2482,"last_power_cycle_ms":652,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"boot"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.44,"enabled_dbfs":-34.95,"delta_dbfs":21.49,"muted_peak":140,"enabled_peak":1801,"baseline_dbfs":-56.44,"observed_dbfs":-34.95,"observed_peak":1801,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9262},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":true,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":11,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":375,"littlefs_mount_ms":6,"history_load_ms":1117,"history_rows_counted":7118,"history_rows_loaded":120,"config_load_ms":59,"network_start_ms":73,"display_init_ms":652,"sensors_init_ms":1019,"server_start_ms":1,"setup_complete_ms":3705,"first_url_report_ms":3705}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.44,"enabled_dbfs":-34.95,"delta_dbfs":21.49,"muted_peak":140,"enabled_peak":1801,"baseline_dbfs":-56.44,"observed_dbfs":-34.95,"observed_peak":1801,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9262}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.44,"enabled_dbfs":-34.95,"delta_dbfs":21.49,"muted_peak":140,"enabled_peak":1801,"baseline_dbfs":-56.44,"observed_dbfs":-34.95,"observed_peak":1801,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9262}
[PASS] Boot Telemetry: setup_ms=3705 history_ms=1117 rows_counted=7118 rows_loaded=120 first_url_ms=3705 server_ms=1 display_ms=652 sensors_ms=1019
[PASS] Display Reinit Recovery: before_init=1 init=2 before_reinit=0 reinit=1 before_visible=1 visible=3 last_reinit_ms=653 last_visible_ms=12438 out_p1=255 cfg_p1=243 power_high=True reset_high=True pins_output=True reinit_applied=True test_applied=True
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":12,"free_heap_bytes":93324,"min_free_heap_bytes":82760,"persisted_samples":7119,"last_sample_epoch":1777122745,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"camera_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0,"setup_complete_ms":3705,"history_load_ms":1117}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-51},"storage":{"persisted_samples":7119,"last_sample_epoch":1777122745,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true,"live_build_count":1,"live_cache_age_ms":0},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"live_cache_ttl_ms":1000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":28.00,"humidity":7.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":52.62},"ap3216c":{"online":true,"als":0,"ir":4,"ps":80,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":-0.038,"ay":-0.316,"az":10.056,"ag":10.061,"pitch":0.22,"roll":-1.80},"mic":{"online":true,"dbfs":-62.70,"rms":0.00073,"peak":74},"system":{"runtime_ready":true,"cpu_usage_pct":16.07,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":93324,"min_free_heap_bytes":82760,"max_alloc_heap_bytes":81908,"uptime_sec":12,"reset_reason":"POWERON"},"display":{"online":true,"page":2,"page_name":"SYSTEM","init_count":2,"reinit_count":1,"visible_test_count":3,"prepare_restart_count":0,"last_init_ms":653,"last_reinit_ms":653,"last_visible_test_ms":12438,"last_power_cycle_ms":653,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"manual"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.44,"enabled_dbfs":-34.95,"delta_dbfs":21.49,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0,"speaker_volume":26,"headphone_volume":18},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":true,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":11,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":375,"littlefs_mount_ms":6,"history_load_ms":1117,"history_rows_counted":7118,"history_rows_loaded":120,"config_load_ms":59,"network_start_ms":73,"display_init_ms":653,"sensors_init_ms":1019,"server_start_ms":1,"setup_complete_ms":3705,"first_url_report_ms":3705}}
[PASS] Camera Capture And Controls: online=True psram=True name=OV5640 pid=22080 jpg_bytes=1556 avg_jpg_ms=66 max_jpg_ms=92 stream_bytes=98492 stream_frames=61 stream_fps=20.2 capture_before=0 capture_after=67 failures_before=0 failures_after=0 preset=7/7 reg_value=3 quality=18 quality_verified=True brightness_verified=True invalid_quality=out_of_range hex_reg=invalid_request es8388_volume=18 unsafe_write=write_blocked bad_frame=unsupported_framesize bad_value=invalid_value bad_device=invalid_request wrapped_reg=reg_out_of_range
[PASS] Peripheral Preset Controls: ap_mode=1->3 qma_range=4->8 speaker=16->playback_reg16->26 headphone=17->18 bad_ap=out_of_range bad_qma=out_of_range bad_volume=out_of_range live_ap=3 live_qma=8 live_spk=26 live_hp=18
[PASS] API Live 2Hz Soak: duration_s=20 ok_responses=39 failed_responses=0 live_builds=19 heap_before=91772 heap_after=83364 min_heap=83692 heap_drop=8408 max_cpu_pct=15.66
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"FAIL","loopback_passed":false,"test_running":false,"speak_running":false,"muted_dbfs":-52.71,"enabled_dbfs":-45.71,"delta_dbfs":7.0,"muted_peak":212,"enabled_peak":384,"baseline_dbfs":-52.71,"observed_dbfs":-45.71,"observed_peak":384,"pass_count":1,"fail_count":1,"speak_count":1,"last_spoken_temp_c":28,"has_spoken_temp":true,"speaker_volume":26,"headphone_volume":18,"last_test_ms":20381}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=50012
[PASS] Dashboard Edit Lock: missing=(none); direct_overwrites=(none)
[PASS] Reboot Persistence: before_persisted=7127 after_persisted=7127 before_uptime=93 after_uptime=5 before_temp_high=61.5 after_temp_high=61.5 history_rows=60 display_init=1 out_p1=255 cfg_p1=243 display_ok=True
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=7127 persisted_after=7134 heap_before=97944 heap_after=94784 heap_min_seen=94784 heap_drop=3160
[PASS] CSV Log: lines=7134
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=0 ps=80 | mic=on -49.06dBFS peak=578 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=on g=10.08 p=0.2 r=-0.4 | chip=on 51.75C | cpu=0.0% heap=96KB | fs=1024000/1441792 persisted=7118 pending=0 fail=0 drop=0 | lcd=on p3
[BOOT] serial=375 littlefs=6 history=1117 rows=7118 loaded=120 config=59 network=73 display=652 sensors=1019 server=1 setup=3705 first_url=3705
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
