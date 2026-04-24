Feature: ESP32 Sensor Hub Dashboard
  The ESP32-S3 board should expose a persisted multi-sensor dashboard over HTTP and on the onboard LCD.

  Scenario: Firmware builds and uploads with Arduino CLI
    Given the Arduino CLI can compile the sketch for esp32:esp32:esp32s3
    When the sketch is uploaded to COM7
    Then the serial console should report a reachable dashboard URL

  Scenario: Live dashboard exposes sensor telemetry
    Given the board is running on the local Wi-Fi
    When the client requests /api/status and /api/history
    Then DHT11, AP3216C, QMA6100P, microphone, chip temperature, CPU, alerts, config and board speaker verification fields should be present
    And at least one persisted history row should exist in LittleFS
    And the LCD state should report an active offline page rotation
    And the board speaker playback verification should be marked as passed
    And posting /api/speak_temperature should increase or otherwise change board playback evidence in status fields such as speak_count

  Scenario: HTML dashboard, board speaker playback controls, alert thresholds, self-test and CSV log are available
    Given the board HTTP service is online
    When the client requests / and /api/log.csv
    Then the HTML should contain the dashboard shell, a board speaker playback button, a board speaker self-test button, alert threshold controls and a CSV log link
    And the CSV log should contain persisted sensor samples

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
