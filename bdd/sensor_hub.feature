Feature: ESP32 Sensor Hub Dashboard
  The ESP32-S3 board should expose a persisted multi-sensor dashboard over HTTP and on the onboard LCD.

  Scenario: Firmware builds and uploads with Arduino CLI
    Given the Arduino CLI can compile the sketch for esp32:esp32:esp32s3
    When the sketch is uploaded to COM7
    Then the serial console should report a reachable dashboard URL

  Scenario: Live dashboard exposes sensor telemetry
    Given the board is running on the local Wi-Fi
    When the client requests /api/status, /api/health, /api/live and /api/history
    Then DHT11, AP3216C, QMA6100P, microphone, chip temperature, CPU, health, live cadence, alerts, config and board speaker verification fields should be present
    And /api/health should report compact OK health for storage, sensors, speaker, alerts, uptime, heap and persisted samples
    And /api/live should report 500 ms live polling, cached live payloads, and 10000 ms sample and flush cadence
    And /api/status should report boot timing telemetry with bounded setup time and tail-loaded history rows
    And the MC5640 camera should report online status and serve a JPEG frame to the HTML dashboard
    And at least one persisted history row should exist in LittleFS
    And the LCD state should report an active offline page rotation
    And the board speaker playback verification should be marked as passed
    And posting /api/speak_temperature should increase or otherwise change board playback evidence in status fields such as speak_count

  Scenario: Boot-to-dashboard readiness remains fast
    Given the board has persisted LittleFS CSV history
    When the firmware boots
    Then the serial console should report the dashboard URL immediately after setup
    And setup_complete_ms should stay below the accepted boot readiness budget
    And only the latest dashboard history rows should be parsed during boot

  Scenario: HTML dashboard, health, fast live polling, board speaker playback controls, alert thresholds, self-test and CSV log are available
    Given the board HTTP service is online
    When the client requests / and /api/log.csv
    Then the HTML should contain the dashboard shell, a health link, completion-based 0.5 second live polling, a board speaker playback button, a board speaker self-test button, alert threshold controls and a CSV log link
    And the CSV log should contain persisted sensor samples

  Scenario: Hardware register and camera controls are exposed
    Given the board HTTP service is online
    When the verification flow reads an AP3216C register and applies camera quality and brightness settings
    Then the register API should return a masked value
    And the camera control API should report effective values that round-trip through /api/camera
    And register controls should use Chinese decimal guidance and safe write ranges
    And unsafe register writes, non-decimal register numbers, invalid devices, wrapped 8-bit register addresses, out-of-range camera values, and invalid camera frame sizes should be rejected
    And the camera dashboard should use a dedicated MJPEG stream with a measured 20 FPS target

  Scenario: Dashboard controls remain editable while live polling runs
    Given the board HTTP service is online and the HTML dashboard polls /api/live every 500 ms
    When the user drags a camera slider or changes a camera selector
    Then live polling should not overwrite the edited control value before the user applies it
    And the dashboard edit-lock JavaScript behavior should pass a host-side executable test
    And one-click camera presets should round-trip through /api/camera/preset
    And AP3216C mode, QMA6100P range, and ES8388 volume presets should round-trip through /api/peripheral/control
    And out-of-range preset values should be rejected with explicit errors

  Scenario: 2Hz live polling remains stable
    Given the board HTTP service is online
    When the verification flow polls /api/live at 2Hz
    Then every live response should preserve the advertised cadence
    And live JSON build count should grow slower than the response count
    And heap should remain within the accepted stability band

  Scenario: LittleFS config and sensor samples survive reboot
    Given the board HTTP service is online
    When the verification flow writes config, flushes logs, and reboots the board
    Then /api/status should report the persisted config after reboot
    And /api/history should still include recovered LittleFS samples

  Scenario: Short unattended soak remains healthy
    Given the board HTTP service is online
    When the verification flow polls the board through a short soak window
    Then persisted samples should increase
    And storage failures and dropped samples should remain zero
    And heap usage should remain within the accepted stability band
