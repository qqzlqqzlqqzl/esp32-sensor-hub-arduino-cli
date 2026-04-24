# ESP32 Sensor Hub CI Report

Generated: 2026-04-24 11:51:05

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, CPU status, LCD state, health, alerts, persistent config, board speaker verification state, and board speaker playback evidence
- HTML dashboard includes board speaker playback/self-test controls, alert thresholds and CSV log availability
- LittleFS data and config survive a board reboot
- Short soak verifies continued sampling, storage writes and heap stability
- Host USB camera capture is optional and skipped unless requested

## Step Results
[PASS] Build: Sketch uses 1117105 bytes (85%) of program storage space. Maximum is 1310720 bytes.
Global variables use 178552 bytes (54%) of dynamic memory, leaving 149128 bytes for local variables. Maximum is 327680 bytes.
[PASS] Upload: Writing at 0x00119000... (97 %)
Writing at 0x0011eb91... (100 %)
Wrote 1117472 bytes (726607 compressed) at 0x00010000 in 12.9 seconds (effective 693.0 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-50},"storage":{"used_bytes":999424,"total_bytes":1441792,"persisted_samples":6983,"last_sample_epoch":1777002499,"pending_bytes":142,"write_failures":0,"dropped_samples":0,"mount_ok":true},"config":{"persisted":true,"temp_high_c":61.50,"humidity_high_pct":88.00,"sound_high_dbfs":-22.50,"light_high_als":3456,"cpu_high_pct":92.50,"heap_low_bytes":54321,"speaker_alerts":false},"alerts":{"active":false,"count":0,"summary":"OK","temp_high":false,"humidity_high":false,"sound_high":false,"light_high":false,"cpu_high":false,"heap_low":false},"dht11":{"online":true,"temp_c":25.00,"humidity":5.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":49.55},"ap3216c":{"online":true,"als":0,"ir":5,"ps":24},"qma6100p":{"online":true,"ax":0.019,"ay":-0.316,"az":10.084,"ag":10.089,"pitch":-0.11,"roll":-1.80},"mic":{"online":true,"dbfs":-66.55,"rms":0.00047,"peak":40},"system":{"runtime_ready":true,"cpu_usage_pct":0.86,"cpu_freq_mhz":240,"free_heap_bytes":122712,"min_free_heap_bytes":114272,"max_alloc_heap_bytes":110580,"uptime_sec":24,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":0,"page_name":"CLIMATE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.11,"enabled_dbfs":-29.98,"delta_dbfs":33.13,"muted_peak":65,"enabled_peak":3567,"baseline_dbfs":-63.11,"observed_dbfs":-29.98,"observed_peak":3567,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":16595}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.11,"enabled_dbfs":-29.98,"delta_dbfs":33.13,"muted_peak":65,"enabled_peak":3567,"baseline_dbfs":-63.11,"observed_dbfs":-29.98,"observed_peak":3567,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":16595}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.11,"enabled_dbfs":-29.98,"delta_dbfs":33.13,"muted_peak":65,"enabled_peak":3567,"baseline_dbfs":-63.11,"observed_dbfs":-29.98,"observed_peak":3567,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":16595}
[PASS] API Health: {"ok":true,"status":"OK","ip":"192.168.124.60","uptime_sec":24,"free_heap_bytes":122712,"min_free_heap_bytes":114272,"persisted_samples":6983,"last_sample_epoch":1777002499,"storage_ok":true,"sensors_ok":true,"speaker_ok":true,"alerts_active":false,"alert_count":0,"alert_summary":"OK","write_failures":0,"dropped_samples":0}
[PASS] Persistent Config: save={"saved":true} invalid_rejected=True range_invalid_rejected=True config={"persisted":true,"temp_high_c":61.5,"humidity_high_pct":88.0,"sound_high_dbfs":-22.5,"light_high_als":3456,"cpu_high_pct":92.5,"heap_low_bytes":54321,"speaker_alerts":false}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-63.11,"enabled_dbfs":-29.98,"delta_dbfs":33.13,"muted_peak":65,"enabled_peak":3567,"baseline_dbfs":-63.11,"observed_dbfs":-29.98,"observed_peak":3567,"pass_count":1,"fail_count":0,"speak_count":1,"last_spoken_temp_c":25,"has_spoken_temp":true,"last_test_ms":16595}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=26537
[PASS] Reboot Persistence: before_persisted=6985 after_persisted=6986 before_uptime=30 after_uptime=16 before_temp_high=61.5 after_temp_high=61.5 history_rows=60
[PASS] Short Soak: duration_s=70 polls=14 persisted_before=6986 persisted_after=7000 heap_before=120172 heap_after=118404 heap_min_seen=118404 heap_drop=1768
[PASS] CSV Log: lines=7000
[PASS] USB Camera: Skipped by default; pass -IncludeCamera to verify host USB camera artifacts.

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=0 ps=0 | mic=off -120.00dBFS peak=0 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=off g=0.00 p=0.0 r=0.0 | chip=off 0.00C | cpu=0.0% heap=121KB | fs=999424/1441792 persisted=6982 pending=0 fail=0 drop=0 | lcd=on p1
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
