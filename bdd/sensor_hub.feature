Feature: ESP32 Sensor Hub Dashboard
  The ESP32-S3 board should expose a persisted multi-sensor dashboard over HTTP and on the onboard LCD.

  Scenario: Firmware builds and uploads with Arduino CLI
    Given the Arduino CLI can compile the sketch for esp32:esp32:esp32s3
    When the sketch is uploaded to COM7
    Then the serial console should report a reachable dashboard URL

  Scenario: Live dashboard exposes sensor telemetry
    Given the board is running on the local Wi-Fi
    When the client requests /api/status and /api/history
    Then DHT11, AP3216C, QMA6100P, microphone, chip temperature, CPU and board speaker verification fields should be present
    And at least one persisted history row should exist in LittleFS
    And the LCD state should report an active offline page rotation
    And the board speaker playback verification should be marked as passed

  Scenario: HTML dashboard, board speaker self-test and CSV log are available
    Given the board HTTP service is online
    When the client requests / and /api/log.csv
    Then the HTML should contain the dashboard shell, a board speaker self-test button and a CSV log link
    And the CSV log should contain persisted sensor samples

  Scenario: Host USB camera can capture an observation frame
    Given the host machine has a USB camera
    When the verification flow captures a snapshot
    Then a camera image artifact should be written successfully
