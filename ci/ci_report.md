# ESP32 Sensor Hub CI Report

Generated: 2026-04-25 18:15:36

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, MC5640 camera status, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- Boot-to-dashboard readiness exposes setup timing telemetry and limits boot history parsing to the latest dashboard rows
- LCD recovery can be triggered without a board power-cycle and proves XL9555 power/reset pins are outputs driven high after reinit and software reboot
- Camera JPEG capture, effective camera controls and safe decimal hardware register access are verified through HTTP APIs
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
[PASS] Build: Sketch uses 1216957 bytes (92%) of program storage space. Maximum is 1310720 bytes.
Global variables use 183224 bytes (55%) of dynamic memory, leaving 144456 bytes for local variables. Maximum is 327680 bytes.
[PASS] Dashboard Edit Lock Behavior: DASHBOARD_EDIT_LOCK_BEHAVIOR_PASS
[PASS] Serial Port: using detected port COM7; requested=(auto); available=COM1, COM7, COM9
[PASS] Upload: Writing at 0x001312aa... (97 %)
Writing at 0x00136d28... (100 %)
Wrote 1217328 bytes (776525 compressed) at 0x00010000 in 19.6 seconds (effective 496.2 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-48},"storage":{"used_bytes":872448,"total_bytes":1441792,"persisted_samples":6057,"last_sample_epoch":1777111910,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":29.00,"humidity":8.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":53.06},"ap3216c":{"online":true,"als":0,"ir":4,"ps":81,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":0.029,"ay":-0.402,"az":10.132,"ag":10.140,"pitch":-0.16,"roll":-2.27},"mic":{"online":true,"dbfs":-59.31,"rms":0.00108,"peak":72},"system":{"runtime_ready":true,"cpu_usage_pct":7.42,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":79872,"min_free_heap_bytes":76840,"max_alloc_heap_bytes":69620,"uptime_sec":9,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":1,"page_name":"LIGHT","init_count":1,"reinit_count":0,"prepare_restart_count":0,"last_init_ms":630,"last_reinit_ms":0,"last_power_cycle_ms":630,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"boot"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.37,"enabled_dbfs":-32.16,"delta_dbfs":31.21,"muted_peak":66,"enabled_peak":2960,"baseline_dbfs":-63.37,"observed_dbfs":-32.16,"observed_peak":2960,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9264},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":false,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":8,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":373,"littlefs_mount_ms":7,"history_load_ms":973,"history_rows_counted":6056,"history_rows_loaded":120,"config_load_ms":66,"network_start_ms":74,"display_init_ms":630,"sensors_init_ms":1018,"server_start_ms":2,"setup_complete_ms":3786,"first_url_report_ms":3786}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.37,"enabled_dbfs":-32.16,"delta_dbfs":31.21,"muted_peak":66,"enabled_peak":2960,"baseline_dbfs":-63.37,"observed_dbfs":-32.16,"observed_peak":2960,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9264}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.37,"enabled_dbfs":-32.16,"delta_dbfs":31.21,"muted_peak":66,"enabled_peak":2960,"baseline_dbfs":-63.37,"observed_dbfs":-32.16,"observed_peak":2960,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"speaker_volume":26,"headphone_volume":18,"last_test_ms":9264}
[PASS] Boot Telemetry: setup_ms=3786 history_ms=973 rows_counted=6056 rows_loaded=120 first_url_ms=3786 server_ms=2 display_ms=630 sensors_ms=1018
[PASS] Display Reinit Recovery: before_init=1 init=2 before_reinit=0 reinit=1 last_reinit_ms=629 out_p1=255 cfg_p1=243 power_high=True reset_high=True pins_output=True applied=True
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":11,"free_heap_bytes":78564,"min_free_heap_bytes":61348,"persisted_samples":6057,"last_sample_epoch":1777111910,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"camera_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0,"setup_complete_ms":3786,"history_load_ms":973}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-48},"storage":{"persisted_samples":6057,"last_sample_epoch":1777111910,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true,"live_build_count":1,"live_cache_age_ms":0},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"live_cache_ttl_ms":1000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":29.00,"humidity":8.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":53.06},"ap3216c":{"online":true,"als":2,"ir":5,"ps":70,"mode":3},"qma6100p":{"online":true,"range_g":8,"ax":0.182,"ay":-0.421,"az":10.094,"ag":10.104,"pitch":-1.03,"roll":-2.39},"mic":{"online":true,"dbfs":-68.32,"rms":0.00038,"peak":32},"system":{"runtime_ready":true,"cpu_usage_pct":17.20,"cpu_freq_mhz":240,"wifi_tx_power":80,"free_heap_bytes":78564,"min_free_heap_bytes":61348,"max_alloc_heap_bytes":65524,"uptime_sec":11,"reset_reason":"POWERON"},"display":{"online":true,"page":3,"page_name":"STORAGE","init_count":2,"reinit_count":1,"prepare_restart_count":0,"last_init_ms":629,"last_reinit_ms":629,"last_power_cycle_ms":629,"xl9555_output_port1":255,"xl9555_config_port1":243,"power_high":true,"reset_high":true,"pins_output":true,"last_action":"manual"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.37,"enabled_dbfs":-32.16,"delta_dbfs":31.21,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0,"speaker_volume":26,"headphone_volume":18},"camera":{"online":true,"name":"OV5640","pid":22080,"init_error":0,"psram":false,"external_clock":true,"frame_size":1,"frame_size_name":"QQVGA","quality":30,"brightness":0,"contrast":0,"saturation":0,"sharpness":0,"special_effect":0,"wb_mode":0,"awb":1,"awb_gain":0,"aec":1,"aec2":0,"ae_level":0,"aec_value":885,"agc":1,"agc_gain":8,"gainceiling":248,"hmirror":0,"vflip":0,"colorbar":0,"capture_count":0,"capture_failures":0,"last_capture_ms":0,"last_capture_duration_ms":0,"last_frame_bytes":0,"last_width":0,"last_height":0,"stream_port":81,"stream_target_fps":20,"stream_clients":0,"stream_frame_count":0,"stream_fps":0.00,"stream_last_client_ms":0},"boot":{"serial_ready_ms":373,"littlefs_mount_ms":7,"history_load_ms":973,"history_rows_counted":6056,"history_rows_loaded":120,"config_load_ms":66,"network_start_ms":74,"display_init_ms":629,"sensors_init_ms":1018,"server_start_ms":2,"setup_complete_ms":3786,"first_url_report_ms":3786}}
[PASS] Camera Capture And Controls: online=True name=OV5640 pid=22080 jpg_bytes=1548 avg_jpg_ms=68 max_jpg_ms=96 stream_bytes=99457 stream_frames=61 stream_fps=20.1 capture_before=0 capture_after=68 failures_before=0 failures_after=0 preset=7/7 reg_value=3 quality=18 quality_verified=True brightness_verified=True invalid_quality=out_of_range hex_reg=invalid_request es8388_volume=18 unsafe_write=write_blocked bad_frame=unsupported_framesize bad_value=invalid_value bad_device=invalid_request wrapped_reg=reg_out_of_range
[PASS] Peripheral Preset Controls: ap_mode=1->3 qma_range=4->8 speaker=16->playback_reg16->26 headphone=17->18 bad_ap=out_of_range bad_qma=out_of_range bad_volume=out_of_range live_ap=3 live_qma=8 live_spk=26 live_hp=18
[PASS] API Live 2Hz Soak: duration_s=20 ok_responses=39 failed_responses=0 live_builds=16 heap_before=73068 heap_after=73052 min_heap=64636 heap_drop=16 max_cpu_pct=29.45
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.06,"enabled_dbfs":-49.44,"delta_dbfs":13.62,"muted_peak":53,"enabled_peak":356,"baseline_dbfs":-63.06,"observed_dbfs":-49.44,"observed_peak":356,"pass_count":2,"fail_count":0,"speak_count":1,"last_spoken_temp_c":29,"has_spoken_temp":true,"speaker_volume":26,"headphone_volume":18,"last_test_ms":19855}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=49434
[PASS] Dashboard Edit Lock: missing=(none); direct_overwrites=(none)
[PASS] Reboot Persistence: before_persisted=6065 after_persisted=6065 before_uptime=92 after_uptime=5 before_temp_high=61.5 after_temp_high=61.5 history_rows=60 display_init=1 out_p1=255 cfg_p1=243 display_ok=True
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=6065 persisted_after=6072 heap_before=80016 heap_after=75912 heap_min_seen=75912 heap_drop=4104
[PASS] CSV Log: lines=6072
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
[LIVE] dht=off 0.0C 0.0% | als=1 ir=0 ps=81 | mic=on -35.13dBFS peak=624 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=on g=10.14 p=-1.2 r=-2.2 | chip=on 51.31C | cpu=0.0% heap=78KB | fs=872448/1441792 persisted=6056 pending=0 fail=0 drop=0 | lcd=on p4
[BOOT] serial=373 littlefs=7 history=973 rows=6056 loaded=120 config=66 network=74 display=630 sensors=1018 server=2 setup=3786 first_url=3786
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
```
