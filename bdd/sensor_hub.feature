Feature: ESP32 Sensor Hub Dashboard
  The ESP32-S3 board should expose a persisted multi-sensor dashboard over HTTP and on the onboard LCD.

  Scenario: Firmware builds and uploads with Arduino CLI
    Given the Arduino CLI can compile the sketch for esp32:esp32:esp32s3 with OPI PSRAM enabled
    When the sketch is uploaded through the detected ESP32 serial port
    Then the serial console should report a reachable dashboard URL

  Scenario: Live dashboard exposes sensor telemetry
    Given the board is running on the local Wi-Fi
    When the client requests /api/status, /api/health, /api/live and /api/history
    Then DHT11 raw frame bytes, effective DHT resolution, AP3216C, QMA6100P, microphone, ADC input voltage, chip temperature, CPU, health, live cadence, alerts, config and board speaker verification fields should be present
    And /api/health should report compact OK health for storage, sensors, ADC input voltage, speaker, alerts, uptime, heap and persisted samples
    And /api/live should report 500 ms live polling, cached live payloads, ADC input voltage, and 10000 ms sample and flush cadence
    And /api/status and /api/live should expose the CPU-linked status LED blink half-period and frequency
    And /api/status should report boot timing telemetry with bounded setup time and tail-loaded history rows
    And the MC5640 camera should report online status and serve a JPEG frame to the HTML dashboard
    And /api/status should report PSRAM available for camera frame buffers
    And at least one persisted history row with ADC input voltage fields should exist in LittleFS
    And the LCD state should report an active offline page rotation
    And the board speaker playback verification should be marked as passed
    And posting /api/speak_temperature should increase or otherwise change board playback evidence in status fields such as speak_count

  Scenario: Boot-to-dashboard readiness remains fast
    Given the board has persisted LittleFS CSV history
    When the firmware boots
    Then the serial console should report the dashboard URL immediately after setup
    And setup_complete_ms should stay below the accepted boot readiness budget
    And only the latest dashboard history rows should be parsed during boot

  Scenario: HTML dashboard, health, fast live polling, board speaker playback controls, alert thresholds, self-test and logs are available
    Given the board HTTP service is online
    When the client requests /, /api/log.csv, /api/diagnostics and /api/diagnostics/log
    Then the HTML should contain the dashboard shell, a health link, completion-based 0.5 second live polling, a board speaker playback button, a board speaker self-test button, alert threshold controls, a CSV log link and diagnostic log controls
    And the CSV log should contain persisted sensor samples including ADC input voltage columns
    And the diagnostic log should be JSON Lines that can be downloaded and cleared through HTTP

  Scenario: Hardware register and camera controls are exposed
    Given the board HTTP service is online
    When the verification flow reads an AP3216C register and applies camera quality and brightness settings
    Then the register API should return a masked value
    And the camera control API should report effective values that round-trip through /api/camera
    And the camera control API should return success=true only when the hardware readback verifies the setting
    And camera controls should still complete and capture should recover while an MJPEG stream is connected
    And the camera status should expose capture recovery counters so stalled frame capture is visible
    And register controls should use Chinese decimal guidance and safe write ranges
    And OV5640 gain ceiling should use decimal raw 10-bit register values and reject the old generic 0 to 6 enum values
    And unsafe register writes, non-decimal register numbers, invalid devices, wrapped 8-bit register addresses, out-of-range camera values, and invalid camera frame sizes should be rejected
    And the camera dashboard should use a dedicated MJPEG stream with a 24 FPS scheduler target to keep measured throughput near or above 20 FPS

  Scenario: Dashboard controls remain editable while live polling runs
    Given the board HTTP service is online and the HTML dashboard polls /api/live every 500 ms
    When the user changes a camera selector
    Then live polling should not overwrite the edited control value before the user applies it
    And the dashboard edit-lock JavaScript behavior should pass a host-side executable test
    And one-click camera presets should round-trip through /api/camera/preset
    And normal camera and peripheral settings should be exposed as dropdown selectors instead of manual numeric inputs
    And AP3216C mode, interrupt clear, ALS/PS threshold and calibration presets should round-trip through /api/peripheral/control
    And QMA6100P range, bandwidth, power, interrupt latch, step, tap and motion presets should round-trip through /api/peripheral/control
    And ES8388 volume, mic gain, input channel, ALC, 3D, sample-rate and EQ presets should round-trip through /api/peripheral/control
    And out-of-range preset values should be rejected with explicit errors

  Scenario: Diagnostic logging captures feature failures
    Given the board HTTP service is online
    When verification triggers rejected camera, peripheral, register, config and system operations
    Then /api/diagnostics should report persisted diagnostic events without write failures
    And /api/diagnostics/log should contain JSONL entries for camera, peripheral, register, config and system components

  Scenario: 2Hz live polling remains stable
    Given the board HTTP service is online
    When the verification flow polls /api/live at 2Hz
    Then every live response should preserve the advertised cadence
    And live JSON build count should grow slower than the response count
    And heap should remain within the accepted stability band

  Scenario: LittleFS config and sensor samples survive reboot
    Given the board HTTP service is online
    When the verification flow writes config, flushes logs, and reboots the board
    Then /api/status should report the persisted config and ADC input voltage after reboot
    And /api/history should still include recovered LittleFS samples with ADC input voltage

  Scenario: Short unattended soak remains healthy
    Given the board HTTP service is online
    When the verification flow polls the board through a short soak window
    Then persisted samples should increase
    And storage failures and dropped samples should remain zero
    And heap usage should remain within the accepted stability band
