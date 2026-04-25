# ESP32 Sensor Hub CI Report

Generated: 2026-04-25 22:31:07

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
[PASS] Build: Sketch uses 1223733 bytes (93%) of program storage space. Maximum is 1310720 bytes.
Global variables use 183440 bytes (55%) of dynamic memory, leaving 144240 bytes for local variables. Maximum is 327680 bytes.
[PASS] Dashboard Edit Lock Behavior: DASHBOARD_EDIT_LOCK_BEHAVIOR_PASS
[PASS] Serial Port: using detected port COM12; requested=(auto); available=COM1, COM12, COM9
[PASS] Upload: Writing at 0x001319e7... (97 %)
Writing at 0x0013748e... (100 %)
Wrote 1224096 bytes (780310 compressed) at 0x00010000 in 19.8 seconds (effective 494.0 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1] after API readiness wait
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-45},"storage":{"used_bytes":552960,"total_bytes":1441792,"persisted_samples":3842,"last_sample_epoch":1777127252,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":28.00,"humidity":7.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":51.31},"ap3216c":{"online":true,"als":0,"ir":0,"ps":76,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":-0.067,"ay":-0.316,"az":10.132,"ag":10.137,"pitch":0.38,"roll":-1.79},"mic":{"online":true,"dbfs":-58.94,"rms":0.00113,"peak":103},"system":{"runtime_ready":true,"cpu_usage_pct":6.06,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":98556,"min_free_heap_bytes":97448,"max_alloc_heap_bytes":90100,"uptime_sec":9,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":3,"page_name":"STORAGE","init_count":1,"reinit_count":0,"visible_test_count":1,"prepare_restart_count":0,"last_init_ms":652,"last_reinit_ms":0,"last_visible_test_ms":1975,"last_power_cycle_ms":652,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"boot"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.48,"enabled_dbfs":-33.99,"delta_dbfs":22.49,"muted_peak":135,"enabled_peak":2975,"baseline_dbfs":-56.48,"observed_dbfs":-33.99,"observed_peak":2975,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9267},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":true,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":16,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"consecutive_capture_failures":0,"recovery_count":0,"last_recovery_ms":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":375,"littlefs_mount_ms":5,"history_load_ms":608,"history_rows_counted":3841,"history_rows_loaded":120,"config_load_ms":42,"network_start_ms":93,"display_init_ms":652,"sensors_init_ms":1019,"server_start_ms":2,"setup_complete_ms":3198,"first_url_report_ms":3198}}
[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.48,"enabled_dbfs":-33.99,"delta_dbfs":22.49,"muted_peak":135,"enabled_peak":2975,"baseline_dbfs":-56.48,"observed_dbfs":-33.99,"observed_peak":2975,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9267}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.48,"enabled_dbfs":-33.99,"delta_dbfs":22.49,"muted_peak":135,"enabled_peak":2975,"baseline_dbfs":-56.48,"observed_dbfs":-33.99,"observed_peak":2975,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9267}
[PASS] Boot Telemetry: setup_ms=3198 history_ms=608 rows_counted=3841 rows_loaded=120 first_url_ms=3198 server_ms=2 display_ms=652 sensors_ms=1019
[PASS] Display Reinit Recovery: before_init=1 init=2 before_reinit=0 reinit=1 before_visible=1 visible=3 last_reinit_ms=653 last_visible_ms=12384 out_p1=255 cfg_p1=243 power_high=True reset_high=True pins_output=True reinit_applied=True test_applied=True
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":12,"free_heap_bytes":93860,"min_free_heap_bytes":82548,"persisted_samples":3842,"last_sample_epoch":1777127252,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"camera_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0,"setup_complete_ms":3198,"history_load_ms":608}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-44},"storage":{"persisted_samples":3842,"last_sample_epoch":1777127252,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true,"live_build_count":1,"live_cache_age_ms":0},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"live_cache_ttl_ms":1000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":28.00,"humidity":7.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":51.31},"ap3216c":{"online":true,"als":0,"ir":5,"ps":76,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":0.057,"ay":-0.259,"az":10.171,"ag":10.174,"pitch":-0.32,"roll":-1.46},"mic":{"online":true,"dbfs":-58.90,"rms":0.00114,"peak":108},"system":{"runtime_ready":true,"cpu_usage_pct":5.31,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":93860,"min_free_heap_bytes":82548,"max_alloc_heap_bytes":81908,"uptime_sec":12,"reset_reason":"POWERON"},"display":{"online":true,"page":2,"page_name":"SYSTEM","init_count":2,"reinit_count":1,"visible_test_count":3,"prepare_restart_count":0,"last_init_ms":653,"last_reinit_ms":653,"last_visible_test_ms":12384,"last_power_cycle_ms":653,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"manual"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-56.48,"enabled_dbfs":-33.99,"delta_dbfs":22.49,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0,"speaker_volume":26,"headphone_volume":18},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":true,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":16,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"consecutive_capture_failures":0,"recovery_count":0,"last_recovery_ms":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":375,"littlefs_mount_ms":5,"history_load_ms":608,"history_rows_counted":3841,"history_rows_loaded":120,"config_load_ms":42,"network_start_ms":93,"display_init_ms":653,"sensors_init_ms":1019,"server_start_ms":2,"setup_complete_ms":3198,"first_url_report_ms":3198}}
[PASS] Camera Capture And Controls: online=True psram=True name=OV5640 pid=22080 jpg_bytes=1473 avg_jpg_ms=63 max_jpg_ms=85 stream_bytes=94742 stream_frames=61 stream_fps=20.1 capture_before=0 capture_after=68 failures_before=0 failures_after=0 consecutive_failures=0 recovery_count=0 preset=7/7 preset_success=True reg_value=3 quality=18 quality_success=True quality_verified=True brightness_success=True brightness_verified=True invalid_quality=out_of_range hex_reg=invalid_request es8388_volume=18 unsafe_write=write_blocked bad_frame=unsupported_framesize bad_value=invalid_value bad_device=invalid_request wrapped_reg=reg_out_of_range
[PASS] Peripheral Preset Controls: ap_mode=1->3 qma_range=4->8 speaker=16->playback_reg16->26 headphone=17->18 bad_ap=out_of_range bad_qma=out_of_range bad_volume=out_of_range live_ap=3 live_qma=8 live_spk=26 live_hp=18
[PASS] API Live 2Hz Soak: duration_s=20 ok_responses=39 failed_responses=0 live_builds=18 heap_before=91644 heap_after=85156 min_heap=85156 heap_drop=6488 max_cpu_pct=22.65
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"FAIL","loopback_passed":false,"test_running":false,"speak_running":false,"muted_dbfs":-56.32,"enabled_dbfs":-48.29,"delta_dbfs":8.04,"muted_peak":120,"enabled_peak":267,"baseline_dbfs":-56.32,"observed_dbfs":-48.29,"observed_peak":267,"pass_count":1,"fail_count":1,"speak_count":1,"last_spoken_temp_c":28,"has_spoken_temp":true,"speaker_volume":26,"headphone_volume":18,"last_test_ms":20594}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=50354
[PASS] Dashboard Edit Lock: missing=(none); direct_overwrites=(none)
[PASS] Reboot Persistence: before_persisted=3850 after_persisted=3850 samples_retained=True before_uptime=93 after_uptime=5 before_temp_high=61.5 after_temp_high=61.5 history_rows=60 history_retained=True display_init=1 out_p1=255 cfg_p1=243 display_ok=True
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=3850 persisted_after=3857 heap_before=98356 heap_after=94764 heap_min_seen=94756 heap_drop=3592
[PASS] CSV Log: lines=3857
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=1 ps=85 | mic=on -59.49dBFS peak=88 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=on g=10.14 p=-0.3 r=-1.9 | chip=on 49.99C | cpu=0.0% heap=96KB | fs=552960/1441792 persisted=3841 pending=0 fail=0 drop=0 | lcd=on p3
[BOOT] serial=375 littlefs=5 history=608 rows=3841 loaded=120 config=42 network=93 display=652 sensors=1019 server=2 setup=3198 first_url=3198
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
