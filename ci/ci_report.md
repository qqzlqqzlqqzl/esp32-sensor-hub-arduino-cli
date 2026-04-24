# ESP32 Sensor Hub CI Report

Generated: 2026-04-24 21:59:52

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- Boot-to-dashboard readiness exposes setup timing telemetry and limits boot history parsing to the latest dashboard rows
- HTML dashboard uses completion-based /api/live polling for 0.5s visible telemetry refresh and keeps full status/history snapshots at 10s
- Hardware CI runs a 2Hz /api/live soak to check live polling stability
- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability
- LittleFS data and config survive a board reboot
- Short soak verifies continued sampling, storage writes and heap stability
- Host USB camera capture is optional and skipped unless requested

## Step Results
[PASS] Build: Sketch uses 1124381 bytes (85%) of program storage space. Maximum is 1310720 bytes.
Global variables use 178640 bytes (54%) of dynamic memory, leaving 149040 bytes for local variables. Maximum is 327680 bytes.
[PASS] Upload: Writing at 0x00119bd1... (97 %)
Writing at 0x0011f4db... (100 %)
Wrote 1124752 bytes (730014 compressed) at 0x00010000 in 12.5 seconds (effective 717.7 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-48},"storage":{"used_bytes":917504,"total_bytes":1441792,"persisted_samples":6334,"last_sample_epoch":1777038983,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":24.00,"humidity":5.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":47.36},"ap3216c":{"online":true,"als":484,"ir":0,"ps":18},"qma6100p":{"online":true,"ax":-0.211,"ay":-0.354,"az":10.046,"ag":10.055,"pitch":1.20,"roll":-2.02},"mic":{"online":true,"dbfs":-31.20,"rms":0.02753,"peak":1295},"system":{"runtime_ready":true,"cpu_usage_pct":0.00,"cpu_freq_mhz":240,"free_heap_bytes":121780,"min_free_heap_bytes":75712,"max_alloc_heap_bytes":110580,"uptime_sec":9,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":3,"page_name":"STORAGE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-65.38,"enabled_dbfs":-31.02,"delta_dbfs":34.36,"muted_peak":47,"enabled_peak":2353,"baseline_dbfs":-65.38,"observed_dbfs":-31.02,"observed_peak":2353,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9269},"boot":{"serial_ready_ms":342,"littlefs_mount_ms":3,"history_load_ms":912,"history_rows_counted":6333,"history_rows_loaded":120,"config_load_ms":26,"network_start_ms":72,"display_init_ms":309,"sensors_init_ms":234,"server_start_ms":283,"setup_complete_ms":2188,"first_url_report_ms":2188}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-65.38,"enabled_dbfs":-31.02,"delta_dbfs":34.36,"muted_peak":47,"enabled_peak":2353,"baseline_dbfs":-65.38,"observed_dbfs":-31.02,"observed_peak":2353,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9269}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-65.38,"enabled_dbfs":-31.02,"delta_dbfs":34.36,"muted_peak":47,"enabled_peak":2353,"baseline_dbfs":-65.38,"observed_dbfs":-31.02,"observed_peak":2353,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":9269}
[PASS] Boot Telemetry: setup_ms=2188 history_ms=912 rows_counted=6333 rows_loaded=120 first_url_ms=2188 server_ms=283 display_ms=309 sensors_ms=234
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":9,"free_heap_bytes":121780,"min_free_heap_bytes":75712,"persisted_samples":6334,"last_sample_epoch":1777038983,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0,"setup_complete_ms":2188,"history_load_ms":912}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-48},"storage":{"persisted_samples":6334,"last_sample_epoch":1777038983,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true,"live_build_count":1,"live_cache_age_ms":0},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"live_cache_ttl_ms":1000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":24.00,"humidity":5.00,"success_count":2,"failure_count":0},"chip_temp":{"online":true,"temp_c":47.36},"ap3216c":{"online":true,"als":484,"ir":3,"ps":24},"qma6100p":{"online":true,"ax":-0.211,"ay":-0.354,"az":10.046,"ag":10.055,"pitch":1.20,"roll":-2.02},"mic":{"online":true,"dbfs":-67.55,"rms":0.00042,"peak":36},"system":{"runtime_ready":true,"cpu_usage_pct":0.00,"cpu_freq_mhz":240,"free_heap_bytes":121780,"min_free_heap_bytes":75712,"max_alloc_heap_bytes":110580,"uptime_sec":9,"reset_reason":"POWERON"},"display":{"online":true,"page":3,"page_name":"STORAGE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-65.38,"enabled_dbfs":-31.02,"delta_dbfs":34.36,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0},"boot":{"serial_ready_ms":342,"littlefs_mount_ms":3,"history_load_ms":912,"history_rows_counted":6333,"history_rows_loaded":120,"config_load_ms":26,"network_start_ms":72,"display_init_ms":309,"sensors_init_ms":234,"server_start_ms":283,"setup_complete_ms":2188,"first_url_report_ms":2188}}
[PASS] API Live 2Hz Soak: duration_s=20 ok_responses=39 failed_responses=0 live_builds=16 heap_before=121780 heap_after=116916 min_heap=106884 heap_drop=4864 max_cpu_pct=39.46
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-65.38,"enabled_dbfs":-31.02,"delta_dbfs":34.36,"muted_peak":47,"enabled_peak":2353,"baseline_dbfs":-65.38,"observed_dbfs":-31.02,"observed_peak":2353,"pass_count":1,"fail_count":0,"speak_count":1,"last_spoken_temp_c":24,"has_spoken_temp":true,"last_test_ms":9269}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=27350
[PASS] Reboot Persistence: before_persisted=6342 after_persisted=6342 before_uptime=91 after_uptime=4 before_temp_high=61.5 after_temp_high=61.5 history_rows=60
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=6342 persisted_after=6349 heap_before=113212 heap_after=117068 heap_min_seen=104032 heap_drop=-3856
[PASS] CSV Log: lines=6349
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=485 ir=3 ps=18 | mic=on -69.32dBFS peak=32 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=on g=10.06 p=1.3 r=-1.7 | chip=on 45.17C | cpu=0.0% heap=120KB | fs=917504/1441792 persisted=6333 pending=0 fail=0 drop=0 | lcd=on p4
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
