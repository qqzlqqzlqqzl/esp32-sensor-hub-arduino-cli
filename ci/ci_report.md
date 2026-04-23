# ESP32 Sensor Hub CI Report

Generated: 2026-04-23 21:14:23

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, CPU status, LCD state and board speaker verification state
- HTML dashboard includes board speaker self-test control and CSV log availability
- Host USB camera capture writes an image artifact

## Step Results
[PASS] Build: Sketch uses 1103105 bytes (84%) of program storage space. Maximum is 1310720 bytes.
Global variables use 178392 bytes (54%) of dynamic memory, leaving 149288 bytes for local variables. Maximum is 327680 bytes.
[PASS] Upload: Writing at 0x001122ec... (97 %)
Writing at 0x001179c0... (100 %)
Wrote 1103472 bytes (720323 compressed) at 0x00010000 in 12.4 seconds (effective 713.5 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-45},"storage":{"used_bytes":933888,"total_bytes":1441792,"persisted_samples":6570,"last_sample_epoch":1776950001,"pending_bytes":140,"write_failures":0,"dropped_samples":0,"mount_ok":true},"dht11":{"online":true,"temp_c":27.00,"humidity":6.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":50.43},"ap3216c":{"online":true,"als":4,"ir":1,"ps":32},"qma6100p":{"online":true,"ax":0.057,"ay":-0.354,"az":10.171,"ag":10.177,"pitch":-0.32,"roll":-2.00},"mic":{"online":true,"dbfs":-66.13,"rms":0.00049,"peak":54},"system":{"runtime_ready":true,"cpu_usage_pct":0.86,"cpu_freq_mhz":240,"free_heap_bytes":123004,"min_free_heap_bytes":113396,"max_alloc_heap_bytes":110580,"uptime_sec":23,"cores":2,"chip_revision":0,"top_task":"idle-est","top_task_cpu_pct":0.00,"reset_reason":"POWERON"},"display":{"online":true,"page":0,"page_name":"CLIMATE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-61.92,"enabled_dbfs":-31.74,"delta_dbfs":30.18,"muted_peak":64,"enabled_peak":3001,"baseline_dbfs":-61.92,"observed_dbfs":-31.74,"observed_peak":3001,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":15283}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-61.92,"enabled_dbfs":-31.74,"delta_dbfs":30.18,"muted_peak":64,"enabled_peak":3001,"baseline_dbfs":-61.92,"observed_dbfs":-31.74,"observed_peak":3001,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":15283}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=21677
[PASS] CSV Log: lines=3464
[PASS] USB Camera: {"ok": true, "camera_index": 0, "backend": 700, "backend_name": "DSHOW", "captured_at": "2026-04-23T21:14:23+08:00", "width": 1280, "height": 720, "mean_brightness": 200.47, "snapshot_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json", "warmup": {"frames": 26, "seconds": 3.171, "stabilized": true, "brightness_stddev": 3.83, "white_balance_stddev": 0.63}, "selection": {"candidate_count": 8, "selected_frame_index": 6, "selection_seconds": 0.704, "best_metrics": {"mean_brightness": 200.47, "contrast_stddev": 57.44, "sharpness_laplacian_var": 255.37, "low_clip_pct": 0.0, "high_clip_pct": 19.95, "saturated_color_pct": 0.0, "motion_mean_abs_diff": 1.3, "channel_mean_b": 197.66, "channel_mean_g": 198.98, "channel_mean_r": 204.27, "white_balance_delta_pct": 3.3, "temperature_bias_pct": 3.3, "white_balance_cast": "neutral", "exposure_score": 0.084, "white_balance_score": 0.913, "sharpness_score": 0.646, "stability_score": 0.941, "contrast_score": 1.0, "selection_score": 0.533}}, "image_metrics": {"mean_brightness": 200.47, "contrast_stddev": 57.44, "sharpness_laplacian_var": 255.37, "low_clip_pct": 0.0, "high_clip_pct": 19.95, "saturated_color_pct": 0.0, "motion_mean_abs_diff": 0.0, "channel_mean_b": 197.66, "channel_mean_g": 198.98, "channel_mean_r": 204.27, "white_balance_delta_pct": 3.3, "temperature_bias_pct": 3.3, "white_balance_cast": "neutral", "exposure_score": 0.084, "white_balance_score": 0.913, "sharpness_score": 0.646, "stability_score": 1.0, "contrast_score": 1.0, "selection_score": 0.539}, "notes": ["Image is bright; GPT should watch for highlight washout.", "Clipping is present in shadows or highlights."], "gpt_handoff": {"artifact_type": "image_with_metadata", "image_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json", "prompt": "Analyze the attached USB camera snapshot of the ESP32 sensor hub test setup. Use the metadata JSON for capture quality signals. Check framing, glare, blur, display readability, device presence, cable visibility, and any obvious test setup issues.", "checklist": ["Is the device or dashboard visible and reasonably framed?", "Is the image too dark, too bright, blurry, or color shifted?", "Are there obvious occlusions, reflections, or missing hardware cues?"]}, "artifact_bundle": {"image_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json"}}

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=0 ps=0 | mic=off -120.00dBFS peak=0 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=off g=0.00 p=0.0 r=0.0 | chip=off 0.00C | cpu=0.0% heap=121KB | fs=933888/1441792 persisted=6569 pending=0 fail=0 drop=0 | lcd=on p1
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
