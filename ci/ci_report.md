# ESP32 Sensor Hub CI Report

Generated: 2026-04-24 21:03:46

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- HTML dashboard uses /api/live for 0.5s visible telemetry refresh and keeps full status/history snapshots at 10s
- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability
- LittleFS data and config survive a board reboot
- Short soak verifies continued sampling, storage writes and heap stability
- Host USB camera capture is optional and skipped unless requested

## Step Results
[PASS] Build: Sketch uses 1121529 bytes (85%) of program storage space. Maximum is 1310720 bytes.
Global variables use 178552 bytes (54%) of dynamic memory, leaving 149128 bytes for local variables. Maximum is 327680 bytes.
[PASS] Upload: Writing at 0x001198c0... (97 %)
Writing at 0x0011f450... (100 %)
Wrote 1121888 bytes (728355 compressed) at 0x00010000 in 12.7 seconds (effective 705.6 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-47},"storage":{"used_bytes":876544,"total_bytes":1441792,"persisted_samples":6069,"last_sample_epoch":1777035604,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":24.00,"humidity":5.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":46.48},"ap3216c":{"online":true,"als":128,"ir":0,"ps":23},"qma6100p":{"online":true,"ax":-0.239,"ay":-0.354,"az":10.036,"ag":10.046,"pitch":1.37,"roll":-2.02},"mic":{"online":true,"dbfs":-67.65,"rms":0.00041,"peak":44},"system":{"runtime_ready":true,"cpu_usage_pct":0.86,"cpu_freq_mhz":240,"free_heap_bytes":122272,"min_free_heap_bytes":112656,"max_alloc_heap_bytes":110580,"uptime_sec":22,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":0,"page_name":"CLIMATE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-59.56,"enabled_dbfs":-29.28,"delta_dbfs":30.28,"muted_peak":106,"enabled_peak":4492,"baseline_dbfs":-59.56,"observed_dbfs":-29.28,"observed_peak":4492,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":14473}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-59.56,"enabled_dbfs":-29.28,"delta_dbfs":30.28,"muted_peak":106,"enabled_peak":4492,"baseline_dbfs":-59.56,"observed_dbfs":-29.28,"observed_peak":4492,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":14473}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-59.56,"enabled_dbfs":-29.28,"delta_dbfs":30.28,"muted_peak":106,"enabled_peak":4492,"baseline_dbfs":-59.56,"observed_dbfs":-29.28,"observed_peak":4492,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":14473}
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":22,"free_heap_bytes":122272,"min_free_heap_bytes":112656,"persisted_samples":6069,"last_sample_epoch":1777035604,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0}
[PASS] API Live Cadence: {"network":{"mode":"STA","ip":"192.168.124.60","rssi_dbm":-47},"storage":{"persisted_samples":6069,"last_sample_epoch":1777035604,"pending_bytes":0,"write_failures":0,"dropped_samples":0,"mount_ok":true},"cadence":{"live_poll_ms":500,"snapshot_poll_ms":10000,"sample_interval_ms":10000,"flush_interval_ms":10000},"alerts":{"active":false,"count":0,"summary":"OK"},"dht11":{"online":true,"temp_c":24.00,"humidity":5.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":46.48},"ap3216c":{"online":true,"als":129,"ir":4,"ps":27},"qma6100p":{"online":true,"ax":-0.239,"ay":-0.354,"az":10.036,"ag":10.046,"pitch":1.37,"roll":-2.02},"mic":{"online":true,"dbfs":-67.65,"rms":0.00041,"peak":44},"system":{"runtime_ready":true,"cpu_usage_pct":0.86,"cpu_freq_mhz":240,"free_heap_bytes":122272,"min_free_heap_bytes":112656,"max_alloc_heap_bytes":110580,"uptime_sec":22,"reset_reason":"POWERON"},"display":{"online":true,"page":0,"page_name":"CLIMATE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-59.56,"enabled_dbfs":-29.28,"delta_dbfs":30.28,"speak_count":0,"has_spoken_temp":false,"last_spoken_temp_c":0}}
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-59.56,"enabled_dbfs":-29.28,"delta_dbfs":30.28,"muted_peak":106,"enabled_peak":4492,"baseline_dbfs":-59.56,"observed_dbfs":-29.28,"observed_peak":4492,"pass_count":1,"fail_count":0,"speak_count":1,"last_spoken_temp_c":24,"has_spoken_temp":true,"last_test_ms":14473}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=27306
[PASS] Reboot Persistence: before_persisted=6076 after_persisted=6077 before_uptime=90 after_uptime=16 before_temp_high=61.5 after_temp_high=61.5 history_rows=60
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=6077 persisted_after=6084 heap_before=122448 heap_after=118468 heap_min_seen=114568 heap_drop=3980
[PASS] CSV Log: lines=6084
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=0 ps=0 | mic=off -120.00dBFS peak=0 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=off g=0.00 p=0.0 r=0.0 | chip=off 0.00C | cpu=0.0% heap=121KB | fs=876544/1441792 persisted=6068 pending=0 fail=0 drop=0 | lcd=on p1
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
