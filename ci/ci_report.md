# ESP32 Sensor Hub CI Report

Generated: 2026-04-23 21:32:58

Overall: **PASS**

## BDD Scenarios
- Firmware builds and uploads with Arduino CLI
- Live dashboard exposes sensor telemetry, CPU status, LCD state, board speaker verification state, and board speaker playback evidence
- HTML dashboard includes board speaker playback/self-test controls and CSV log availability
- Host USB camera capture writes an image artifact

## Step Results
[PASS] Build: Sketch uses 1103469 bytes (84%) of program storage space. Maximum is 1310720 bytes.
Global variables use 178408 bytes (54%) of dynamic memory, leaving 149272 bytes for local variables. Maximum is 327680 bytes.
[PASS] Upload: Writing at 0x001122f5... (97 %)
Writing at 0x001179da... (100 %)
Wrote 1103840 bytes (720553 compressed) at 0x00010000 in 12.6 seconds (effective 702.4 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

[PASS] Serial URL: Detected dashboard IP 192.168.124.60 from serial URLs [192.168.124.60, 192.168.4.1]
[PASS] API Status: {"network":{"mode":"STA","ip":"192.168.124.60","ap_ssid":"ESP32S3-SensorHub","rssi_dbm":-45},"storage":{"used_bytes":962560,"total_bytes":1441792,"persisted_samples":6765,"last_sample_epoch":1776951056,"pending_bytes":140,"write_failures":0,"dropped_samples":0,"mount_ok":true},"dht11":{"online":true,"temp_c":27.00,"humidity":6.00,"success_count":3,"failure_count":0},"chip_temp":{"online":true,"temp_c":50.43},"ap3216c":{"online":true,"als":0,"ir":4,"ps":24},"qma6100p":{"online":true,"ax":0.029,"ay":-0.211,"az":10.276,"ag":10.278,"pitch":-0.16,"roll":-1.17},"mic":{"online":true,"dbfs":-67.92,"rms":0.00040,"peak":53},"system":{"runtime_ready":true,"cpu_usage_pct":0.82,"cpu_freq_mhz":240,"free_heap_bytes":122976,"min_free_heap_bytes":114992,"max_alloc_heap_bytes":110580,"uptime_sec":23,"cores":2,"chip_revision":0,"top_task":null,"top_task_cpu_pct":null,"reset_reason":"POWERON"},"display":{"online":true,"page":0,"page_name":"CLIMATE"},"speaker":{"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-64.20,"enabled_dbfs":-30.37,"delta_dbfs":33.83,"muted_peak":57,"enabled_peak":4620,"baseline_dbfs":-64.20,"observed_dbfs":-30.37,"observed_peak":4620,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":15662}}

[PASS] Speaker Verification: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-64.2,"enabled_dbfs":-30.37,"delta_dbfs":33.83,"muted_peak":57,"enabled_peak":4620,"baseline_dbfs":-64.2,"observed_dbfs":-30.37,"observed_peak":4620,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":15662}
[PASS] Speaker Playback Fields: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-64.2,"enabled_dbfs":-30.37,"delta_dbfs":33.83,"muted_peak":57,"enabled_peak":4620,"baseline_dbfs":-64.2,"observed_dbfs":-30.37,"observed_peak":4620,"pass_count":1,"fail_count":0,"speak_count":0,"last_spoken_temp_c":0,"has_spoken_temp":false,"last_test_ms":15662}
[PASS] Speak Temperature Trigger: {"accepted":true}
[PASS] Speak Temperature Playback: Temperature playback evidence changed: {"online":true,"verification_state":"PASS","loopback_passed":true,"test_running":false,"speak_running":false,"muted_dbfs":-64.2,"enabled_dbfs":-30.37,"delta_dbfs":33.83,"muted_peak":57,"enabled_peak":4620,"baseline_dbfs":-64.2,"observed_dbfs":-30.37,"observed_peak":4620,"pass_count":1,"fail_count":0,"speak_count":1,"last_spoken_temp_c":27,"has_spoken_temp":true,"last_test_ms":15662}
[PASS] API History: rows=60
[PASS] HTML Dashboard: HTML bytes=21953
[PASS] CSV Log: lines=3759
[PASS] USB Camera: {"ok": true, "camera_index": 0, "backend": 700, "backend_name": "DSHOW", "captured_at": "2026-04-23T21:32:58+08:00", "width": 1280, "height": 720, "mean_brightness": 127.96, "snapshot_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json", "warmup": {"frames": 24, "seconds": 2.953, "stabilized": true, "brightness_stddev": 4.52, "white_balance_stddev": 0.91}, "selection": {"candidate_count": 5, "selected_frame_index": 4, "selection_seconds": 0.406, "score_key": "bright_scene_score", "best_metrics": {"mean_brightness": 127.96, "contrast_stddev": 52.78, "sharpness_laplacian_var": 183.08, "low_clip_pct": 7.15, "high_clip_pct": 0.22, "saturated_color_pct": 0.03, "motion_mean_abs_diff": 1.53, "channel_mean_b": 127.46, "channel_mean_g": 126.34, "channel_mean_r": 131.06, "white_balance_delta_pct": 3.67, "temperature_bias_pct": 2.81, "white_balance_cast": "neutral", "exposure_score": 0.701, "white_balance_score": 0.903, "sharpness_score": 0.567, "stability_score": 0.93, "contrast_score": 1.0, "selection_score": 0.743, "highlight_preservation_score": 0.987, "shadow_preservation_score": 0.745, "bright_scene_score": 0.863}}, "image_metrics": {"mean_brightness": 127.96, "contrast_stddev": 52.78, "sharpness_laplacian_var": 183.08, "low_clip_pct": 7.15, "high_clip_pct": 0.22, "saturated_color_pct": 0.03, "motion_mean_abs_diff": 0.0, "channel_mean_b": 127.46, "channel_mean_g": 126.34, "channel_mean_r": 131.06, "white_balance_delta_pct": 3.67, "temperature_bias_pct": 2.81, "white_balance_cast": "neutral", "exposure_score": 0.701, "white_balance_score": 0.903, "sharpness_score": 0.567, "stability_score": 1.0, "contrast_score": 1.0, "selection_score": 0.75, "highlight_preservation_score": 0.987, "shadow_preservation_score": 0.745, "bright_scene_score": 0.863}, "adaptive_fallback": {"triggered": true, "used": true, "strategy": "manual_exposure_bracketing", "reason": "manual_exposure_selected", "best_metrics": {"mean_brightness": 127.96, "contrast_stddev": 52.78, "sharpness_laplacian_var": 183.08, "low_clip_pct": 7.15, "high_clip_pct": 0.22, "saturated_color_pct": 0.03, "motion_mean_abs_diff": 1.53, "channel_mean_b": 127.46, "channel_mean_g": 126.34, "channel_mean_r": 131.06, "white_balance_delta_pct": 3.67, "temperature_bias_pct": 2.81, "white_balance_cast": "neutral", "exposure_score": 0.701, "white_balance_score": 0.903, "sharpness_score": 0.567, "stability_score": 0.93, "contrast_score": 1.0, "selection_score": 0.743, "highlight_preservation_score": 0.987, "shadow_preservation_score": 0.745, "bright_scene_score": 0.863}, "best_attempt": {"manual_auto_exposure": 0.25, "requested_exposure": -6.0, "reported_exposure": -6.0, "high_clip_reduction_pct": 18.28}}, "notes": ["Bright-scene fallback selected a lower-exposure frame to preserve highlights."], "gpt_handoff": {"artifact_type": "image_with_metadata", "image_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json", "prompt": "Analyze the attached USB camera snapshot of the ESP32 sensor hub test setup. Use the metadata JSON for capture quality signals, especially image_metrics and adaptive_fallback. Check framing, glare, blur, display readability, device presence, cable visibility, and any obvious test setup issues.", "checklist": ["Is the device or dashboard visible and reasonably framed?", "Is the image too dark, too bright, blurry, or color shifted?", "Did the bright-scene fallback reduce highlight loss, or does the metadata still report clipping risk?", "Are there obvious occlusions, reflections, or missing hardware cues?"]}, "artifact_bundle": {"image_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.jpg", "metadata_path": "C:\\Users\\lyl\\Desktop\\ESP32\\esp32_sensor_hub\\ci\\artifacts\\usb_camera_snapshot.metadata.json"}}

## Serial Excerpt
```text
[NET] mode=AP ip=192.168.4.1 ap=ESP32S3-SensorHub
[URL] 192.168.4.1/api/status
[LIVE] dht=off 0.0C 0.0% | als=0 ir=0 ps=0 | mic=off -120.00dBFS peak=0 | spk=PENDING off=-120.0 on=-120.0 d=0.0 say=0 | qma=off g=0.00 p=0.0 r=0.0 | chip=off 0.00C | cpu=0.0% heap=121KB | fs=962560/1441792 persisted=6764 pending=0 fail=0 drop=0 | lcd=on p1
[NET] mode=STA ip=192.168.124.60 ap=ESP32S3-SensorHub
[URL] 192.168.124.60/api/status
```
