#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

#include <driver/temp_sensor.h>
#include <esp_freertos_hooks.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "ap3216c.h"
#include "board_camera.h"
#include "dht11.h"
#include "es8388_codec.h"
#include "qma6100p.h"
#include "speech_assets.h"
#include "spilcd.h"
#include "xl9555.h"

#ifndef WIFI_STA_SSID
#define WIFI_STA_SSID ""
#endif

#ifndef WIFI_STA_PASS
#define WIFI_STA_PASS ""
#endif

extern uint32_t g_back_color;

namespace {

constexpr char kApSsid[] = "ESP32S3-SensorHub";
constexpr uint8_t kStatusLedPin = 1;
constexpr gpio_num_t kI2cSdaPin = GPIO_NUM_41;
constexpr gpio_num_t kI2cSclPin = GPIO_NUM_42;
constexpr unsigned long kDhtStartupDelayMs = 3000;
constexpr unsigned long kDhtIntervalMs = 4000;
constexpr unsigned long kLightIntervalMs = 1000;
constexpr unsigned long kAccelIntervalMs = 800;
constexpr unsigned long kMicIntervalMs = 1500;
constexpr unsigned long kChipTempIntervalMs = 5000;
constexpr unsigned long kSystemIntervalMs = 2000;
constexpr unsigned long kDisplayIntervalMs = 2500;
constexpr unsigned long kSpeakerSelfTestDelayMs = 8000;
constexpr unsigned long kSampleIntervalMs = 10000;
constexpr unsigned long kFlushIntervalMs = 10000;
constexpr unsigned long kDashboardLiveIntervalMs = 500;
constexpr unsigned long kLivePayloadCacheTtlMs = 1000;
constexpr unsigned long kDashboardSnapshotIntervalMs = 10000;
constexpr unsigned long kSerialReportIntervalMs = 10000;
constexpr uint16_t kCameraStreamPort = 81;
constexpr uint16_t kCameraStreamTargetFps = 20;
constexpr uint32_t kCameraStreamFrameIntervalMs = 1000 / kCameraStreamTargetFps;
constexpr size_t kHistoryCapacity = 120;
constexpr char kLogPath[] = "/sensor_log.csv";
constexpr char kRotatedLogPath[] = "/sensor_log.old.csv";
constexpr char kConfigPath[] = "/hub_config.txt";
constexpr char kConfigTmpPath[] = "/hub_config.tmp";
constexpr char kConfigBackupPath[] = "/hub_config.bak";
constexpr size_t kMaxLogFileBytes = 512 * 1024;
constexpr size_t kMaxPendingLogBytes = 24 * 1024;
constexpr size_t kMaxLogLineLength = 420;
constexpr size_t kRuntimeTrackCapacity = 32;

constexpr char kStaSsid[] = WIFI_STA_SSID;
constexpr char kStaPass[] = WIFI_STA_PASS;

WebServer server(80);
WiFiServer cameraStreamServer(kCameraStreamPort);
TaskHandle_t gCameraStreamTask = nullptr;
volatile uint32_t gCameraStreamClients = 0;
volatile uint32_t gCameraStreamFrameCount = 0;
volatile uint32_t gCameraStreamLastFpsX100 = 0;
volatile uint32_t gCameraStreamLastClientMs = 0;

struct DhtState {
  bool online = false;
  float tempC = 0.0f;
  float humidity = 0.0f;
  uint32_t success = 0;
  uint32_t failure = 0;
  unsigned long lastUpdateMs = 0;
} gDht;

struct ChipTempState {
  bool online = false;
  float tempC = 0.0f;
  unsigned long lastUpdateMs = 0;
} gChipTemp;

Ap3216cReading gLight;
Qma6100pReading gAccel;
MicLevelReading gMic;
bool gAp3216Ready = false;
bool gQmaReady = false;
bool gMicReady = false;
uint8_t gAp3216Mode = 3;
uint8_t gSpeakerVolume = 26;
uint8_t gHeadphoneVolume = 18;
bool gLittleFsReady = false;
bool gLittleFsMountError = false;
bool gChipTempReady = false;
bool gDisplayReady = false;
bool gSpeakerTestRequested = false;
bool gSpeakTemperatureRequested = false;
bool gRebootRequested = false;
bool gConfigPersisted = false;
String gLiveJsonCache;
unsigned long gLiveJsonCacheMs = 0;
uint32_t gLiveJsonBuildCount = 0;

struct DisplayRecoveryState {
  uint32_t initCount = 0;
  uint32_t reinitCount = 0;
  uint32_t visibleTestCount = 0;
  uint32_t prepareRestartCount = 0;
  uint32_t lastInitMs = 0;
  uint32_t lastReinitMs = 0;
  uint32_t lastVisibleTestMs = 0;
  uint32_t lastPowerCycleMs = 0;
  uint8_t outputPort1 = 0xFF;
  uint8_t configPort1 = 0xFF;
  bool powerHigh = false;
  bool resetHigh = false;
  bool pinsOutput = false;
  char lastAction[20] = "BOOT";
} gDisplayRecovery;
unsigned long gDisplayVisibleTestUntilMs = 0;

struct BootState {
  uint32_t serialReadyMs = 0;
  uint32_t littleFsMountMs = 0;
  uint32_t historyLoadMs = 0;
  uint32_t configLoadMs = 0;
  uint32_t networkStartMs = 0;
  uint32_t displayInitMs = 0;
  uint32_t sensorsInitMs = 0;
  uint32_t serverStartMs = 0;
  uint32_t setupCompleteMs = 0;
  uint32_t firstUrlReportMs = 0;
  uint32_t historyRowsCounted = 0;
  uint32_t historyRowsLoaded = 0;
} gBoot;

struct HubConfig {
  float tempHighC = 32.0f;
  float humidityHighPct = 80.0f;
  float soundHighDbfs = -35.0f;
  uint16_t lightHighAls = 2000;
  float cpuHighPct = 85.0f;
  uint32_t heapLowBytes = 60000;
  bool speakerAlerts = false;
} gConfig;

struct AlertState {
  bool active = false;
  uint8_t count = 0;
  bool tempHigh = false;
  bool humidityHigh = false;
  bool soundHigh = false;
  bool lightHigh = false;
  bool cpuHigh = false;
  bool heapLow = false;
  char summary[120] = "OK";
} gAlerts;

struct SystemState {
  bool runtimeReady = false;
  float cpuUsagePct = 0.0f;
  uint32_t cpuFreqMhz = 0;
  uint32_t freeHeapBytes = 0;
  uint32_t minFreeHeapBytes = 0;
  uint32_t maxAllocHeapBytes = 0;
  uint32_t uptimeSec = 0;
  uint8_t cores = 0;
  uint16_t chipRevision = 0;
  int32_t wifiRssiDbm = 0;
  int32_t wifiTxPower = 0;
  bool topTaskAvailable = false;
  float topTaskCpuPct = 0.0f;
  char topTaskName[20] = "";
  char resetReason[24] = "UNKNOWN";
  uint8_t displayPage = 0;
  char displayPageName[20] = "BOOT";
} gSystem;

struct SpeakerState {
  bool online = false;
  bool loopbackPassed = false;
  bool speaking = false;
  bool testRunning = false;
  float mutedDbfs = -120.0f;
  float enabledDbfs = -120.0f;
  float deltaDbfs = 0.0f;
  int16_t mutedPeak = 0;
  int16_t enabledPeak = 0;
  unsigned long lastTestMs = 0;
  uint32_t passCount = 0;
  uint32_t failCount = 0;
  uint32_t speakCount = 0;
  int16_t lastSpokenTempC = 0;
  bool hasSpokenTemp = false;
} gSpeaker;

struct SampleRow {
  uint32_t seq = 0;
  uint32_t epoch = 0;
  bool dhtOnline = false;
  float dhtTempC = 0.0f;
  float dhtHumidity = 0.0f;
  bool chipOnline = false;
  float chipTempC = 0.0f;
  bool lightOnline = false;
  uint16_t als = 0;
  uint16_t ir = 0;
  uint16_t ps = 0;
  bool accelOnline = false;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float ag = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  bool micOnline = false;
  float micDbfs = -120.0f;
  float micRms = 0.0f;
  int16_t micPeak = 0;
  float cpuUsagePct = 0.0f;
  uint32_t freeHeapBytes = 0;
  uint32_t minFreeHeapBytes = 0;
  uint32_t maxAllocHeapBytes = 0;
  uint16_t cpuFreqMhz = 0;
  uint8_t displayPage = 0;
  uint8_t alertCount = 0;
};

SampleRow gHistory[kHistoryCapacity];
size_t gHistoryHead = 0;
size_t gHistoryCount = 0;
String gPendingLog;
uint32_t gPersistedSamples = 0;
uint32_t gPersistedLastSampleEpoch = 0;
uint32_t gSampleSeq = 0;
uint32_t gStorageWriteFailures = 0;
uint32_t gDroppedSamples = 0;
volatile uint32_t gIdleHookCounts[2] = {};
uint32_t gIdleMaxCounts[2] = {1, 1};
bool gIdleHooksReady = false;

const char kDashboardHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32 Sensor Hub</title>
  <style>
    :root {
      color-scheme: dark;
      --bg: #081018;
      --panel: #121b25;
      --line: #233648;
      --text: #ecf3fb;
      --muted: #8aa0b6;
      --ok: #42c68a;
      --bad: #ff6b6b;
      --accent: #61b4ff;
      --temp: #ff974f;
      --humi: #4bc8f3;
      --light: #ffd166;
      --sound: #b289ff;
      --motion: #62d699;
      --cpu: #ff7a91;
      --heap: #85e3b2;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", Arial, sans-serif;
      background: linear-gradient(180deg, #081018 0%, #0d1824 100%);
      color: var(--text);
    }
    .wrap {
      max-width: 1260px;
      margin: 0 auto;
      padding: 24px 16px 40px;
    }
    .head {
      display: flex;
      justify-content: space-between;
      align-items: flex-end;
      gap: 16px;
      flex-wrap: wrap;
      margin-bottom: 16px;
    }
    h1 {
      margin: 0;
      font-size: 30px;
    }
    .sub {
      margin-top: 6px;
      color: var(--muted);
      font-size: 14px;
    }
    .status-box {
      min-width: 260px;
      border: 1px solid var(--line);
      background: rgba(18, 27, 37, 0.98);
      border-radius: 8px;
      padding: 12px;
    }
    .toolbar {
      display: flex;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 14px;
    }
    .btn {
      appearance: none;
      border: 1px solid rgba(97, 180, 255, 0.35);
      background: rgba(16, 27, 38, 0.98);
      color: var(--text);
      padding: 10px 14px;
      border-radius: 8px;
      cursor: pointer;
      font-size: 14px;
      text-decoration: none;
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .btn:hover { border-color: rgba(97, 180, 255, 0.7); }
    input {
      width: 100%;
      min-width: 0;
      border: 1px solid rgba(50, 79, 106, 0.75);
      border-radius: 6px;
      background: rgba(8, 14, 22, 0.96);
      color: var(--text);
      padding: 8px 9px;
      font-size: 13px;
    }
    .alert-line {
      border: 1px solid rgba(255, 107, 107, 0.55);
      background: rgba(255, 107, 107, 0.10);
      color: #ffd7d7;
      border-radius: 8px;
      padding: 10px 12px;
      margin-bottom: 14px;
      display: none;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(12, minmax(0, 1fr));
      gap: 12px;
    }
    .card {
      background: rgba(18, 27, 37, 0.97);
      border: 1px solid rgba(50, 79, 106, 0.75);
      border-radius: 8px;
      padding: 14px;
      min-width: 0;
    }
    .span-3 { grid-column: span 3; }
    .span-4 { grid-column: span 4; }
    .span-6 { grid-column: span 6; }
    .span-8 { grid-column: span 8; }
    .span-12 { grid-column: span 12; }
    .label {
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 10px;
    }
    .value {
      font-size: 30px;
      line-height: 1.1;
      font-weight: 700;
      word-break: break-word;
    }
    .value.sm { font-size: 22px; }
    .meta {
      color: var(--muted);
      font-size: 13px;
      margin-top: 8px;
      line-height: 1.5;
    }
    .ok { color: var(--ok); }
    .bad { color: var(--bad); }
    .mono { font-family: Consolas, "Courier New", monospace; }
    .kv {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 8px 14px;
      font-size: 13px;
    }
    .kv div span {
      display: block;
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 4px;
    }
    canvas {
      width: 100%;
      height: 200px;
      border-radius: 8px;
      border: 1px solid rgba(50, 79, 106, 0.55);
      background: rgba(8, 14, 22, 0.96);
      display: block;
      margin-top: 10px;
    }
    .camera-img {
      width: 100%;
      aspect-ratio: 4 / 3;
      object-fit: contain;
      background: #07101a;
      border-radius: 8px;
      border: 1px solid rgba(50, 79, 106, 0.55);
      display: block;
      margin-top: 10px;
    }
    .control-row {
      display: grid;
      grid-template-columns: 130px minmax(0, 1fr) 56px;
      align-items: center;
      gap: 8px;
      margin-top: 8px;
      font-size: 12px;
      color: var(--muted);
    }
    .control-row input, .control-row select {
      width: 100%;
    }
    table {
      width: 100%;
      border-collapse: collapse;
      font-size: 13px;
    }
    th, td {
      text-align: left;
      padding: 10px 8px;
      border-bottom: 1px solid rgba(50, 79, 106, 0.45);
      vertical-align: top;
    }
    th { color: var(--muted); font-weight: 600; }
    @media (max-width: 980px) {
      .span-3, .span-4, .span-6, .span-8 { grid-column: span 12; }
      .value { font-size: 26px; }
      h1 { font-size: 26px; }
      .kv { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="head">
      <div>
        <h1>ESP32 多传感器看板</h1>
        <div class="sub">DHT11 / AP3216C / QMA6100P / ES8388 / Chip Temp / CPU / LCD / LittleFS</div>
      </div>
      <div class="status-box">
        <div id="netState" class="mono">loading...</div>
        <div id="storageState" class="meta">LittleFS loading...</div>
        <div id="displayState" class="meta">LCD loading...</div>
      </div>
    </div>

    <div class="toolbar">
      <button id="speakTempBtn" class="btn" type="button">播报当前温度</button>
      <button id="speakerSelfTestBtn" class="btn" type="button">板载喇叭自检</button>
      <button id="saveConfigBtn" class="btn" type="button">保存阈值</button>
      <a class="btn" href="/api/health" target="_blank" rel="noreferrer">健康检查</a>
      <a class="btn" href="/api/log.csv" target="_blank" rel="noreferrer">查看 CSV 日志</a>
    </div>
    <div id="alertLine" class="alert-line"></div>

    <div class="grid">
      <section class="card span-3">
        <div class="label">DHT11 温度</div>
        <div class="value" id="tempC">--</div>
        <div class="meta" id="dhtState">离线</div>
      </section>
      <section class="card span-3">
        <div class="label">DHT11 湿度</div>
        <div class="value" id="humidity">--</div>
        <div class="meta" id="dhtCount">success 0 / fail 0</div>
      </section>
      <section class="card span-3">
        <div class="label">环境音量</div>
        <div class="value" id="soundDb">--</div>
        <div class="meta" id="soundState">离线</div>
      </section>
      <section class="card span-3">
        <div class="label">CPU 使用率</div>
        <div class="value" id="cpuUsage">--</div>
        <div class="meta" id="cpuState">runtime stats pending</div>
      </section>

      <section class="card span-3">
        <div class="label">环境光 AP3216C</div>
        <div class="value sm" id="alsValue">ALS --</div>
        <div class="meta" id="lightState">IR -- / PS --</div>
      </section>
      <section class="card span-3">
        <div class="label">姿态 QMA6100P</div>
        <div class="value sm" id="motionValue">g --</div>
        <div class="meta" id="motionState">X -- / Y -- / Z --</div>
      </section>
      <section class="card span-3">
        <div class="label">芯片温度</div>
        <div class="value sm" id="chipTemp">--</div>
        <div class="meta" id="chipState">离线</div>
      </section>
      <section class="card span-3">
        <div class="label">可用堆内存</div>
        <div class="value sm" id="heapFree">--</div>
        <div class="meta" id="heapState">min -- / max alloc --</div>
      </section>

      <section class="card span-4">
        <div class="label">持久化</div>
        <div class="value sm" id="persistedCount">0</div>
        <div class="meta" id="lastSample">最近样本 --</div>
        <div class="meta" id="storageHealth">pending 0 / write fail 0 / drop 0</div>
      </section>
      <section class="card span-4">
        <div class="label">LCD 离线页</div>
        <div class="value sm" id="lcdPage">--</div>
        <div class="meta" id="lcdState">LCD offline</div>
      </section>
      <section class="card span-4">
        <div class="label">系统摘要</div>
        <div class="kv">
          <div><span>Uptime</span><strong id="uptime">--</strong></div>
          <div><span>CPU Freq</span><strong id="cpuFreq">--</strong></div>
          <div><span>Top Task</span><strong id="topTask">--</strong></div>
          <div><span>Reset</span><strong id="resetReason">--</strong></div>
        </div>
      </section>
      <section class="card span-4">
        <div class="label">板载喇叭闭环</div>
        <div class="value sm" id="speakerLoopback">--</div>
        <div class="meta" id="speakerState">waiting</div>
      </section>
      <section class="card span-8">
        <div class="label">告警阈值</div>
        <div class="kv">
          <div><span>温度上限 C</span><input id="cfgTempHigh" type="number" step="0.1"></div>
          <div><span>湿度上限 %</span><input id="cfgHumidityHigh" type="number" step="0.1"></div>
          <div><span>声音上限 dBFS</span><input id="cfgSoundHigh" type="number" step="0.1"></div>
          <div><span>光照上限 ALS</span><input id="cfgLightHigh" type="number" step="1"></div>
          <div><span>CPU 上限 %</span><input id="cfgCpuHigh" type="number" step="0.1"></div>
          <div><span>Heap 下限 bytes</span><input id="cfgHeapLow" type="number" step="1024"></div>
        </div>
        <div class="meta" id="configState">config loading...</div>
      </section>
      <section class="card span-4">
        <div class="label">告警状态</div>
        <div class="value sm" id="alertCount">0</div>
        <div class="meta" id="alertSummary">OK</div>
      </section>

      <section class="card span-6">
        <div class="label">MC5640 摄像头</div>
        <div class="value sm" id="cameraState">--</div>
        <div class="meta" id="cameraMeta">waiting</div>
        <img id="cameraFrame" class="camera-img" alt="camera">
        <div class="toolbar">
          <button class="btn" data-camera-preset="smooth" type="button">流畅 20 帧</button>
          <button class="btn" data-camera-preset="clear" type="button">清晰画质</button>
          <button class="btn" data-camera-preset="bright" type="button">弱光增强</button>
        </div>
        <div class="control-row"><span>分辨率</span><select id="camFrameSize"><option selected>QQVGA</option><option>QVGA</option><option>VGA</option><option>SVGA</option></select><button class="btn" data-cam-select="framesize_name" data-cam-input="camFrameSize" type="button">设</button></div>
        <div class="control-row"><span>JPEG 质量</span><input id="camQuality" type="range" min="4" max="35" value="30"><button class="btn" data-cam="quality" data-cam-input="camQuality" type="button">设</button></div>
        <div class="control-row"><span>亮度</span><input id="camBrightness" type="range" min="-2" max="2" value="0"><button class="btn" data-cam="brightness" data-cam-input="camBrightness" type="button">设</button></div>
        <div class="control-row"><span>对比度</span><input id="camContrast" type="range" min="-2" max="2" value="0"><button class="btn" data-cam="contrast" data-cam-input="camContrast" type="button">设</button></div>
        <div class="control-row"><span>饱和度</span><input id="camSaturation" type="range" min="-2" max="2" value="0"><button class="btn" data-cam="saturation" data-cam-input="camSaturation" type="button">设</button></div>
        <div class="control-row"><span>自动曝光</span><select id="camAec"><option value="1">开</option><option value="0">关</option></select><button class="btn" data-cam-select="aec" data-cam-input="camAec" type="button">设</button></div>
        <div class="control-row"><span>手动曝光值</span><input id="camAecValue" type="range" min="0" max="1200" value="300"><button class="btn" data-cam="aec_value" data-cam-input="camAecValue" type="button">设</button></div>
        <div class="control-row"><span>自动增益</span><select id="camAgc"><option value="1">开</option><option value="0">关</option></select><button class="btn" data-cam-select="agc" data-cam-input="camAgc" type="button">设</button></div>
        <div class="control-row"><span>手动增益值</span><input id="camAgcGain" type="range" min="0" max="30" value="0"><button class="btn" data-cam="agc_gain" data-cam-input="camAgcGain" type="button">设</button></div>
        <div class="control-row"><span>镜像</span><select id="camMirror"><option value="0">off</option><option value="1">on</option></select><button class="btn" data-cam-select="hmirror" data-cam-input="camMirror" type="button">设</button></div>
        <div class="control-row"><span>翻转</span><select id="camVflip"><option value="0">off</option><option value="1">on</option></select><button class="btn" data-cam-select="vflip" data-cam-input="camVflip" type="button">设</button></div>
        <div class="meta">摄像头设置每次写入后都会回读 /api/camera；只有显示“已生效”才算设置成功。拖动滑条或修改下拉框时，0.5 秒后台刷新不会覆盖正在编辑的值，点“设”或一键预设后才按硬件回读值同步。自动曝光或自动增益打开时，手动曝光值和手动增益值可能会被传感器算法覆盖；需要稳定手动值时先把自动项设为 0。</div>
        <table>
          <tbody>
            <tr><th>设置项</th><th>十进制范围</th><th>说明</th></tr>
            <tr><td>JPEG 质量</td><td>4 到 63</td><td>数字越小画质越高，码流越大；20 帧优先建议 18 到 35。</td></tr>
            <tr><td>亮度/对比度/饱和度</td><td>-2 到 2</td><td>写入后回读同值才算生效。</td></tr>
            <tr><td>手动曝光值</td><td>0 到 1200</td><td>自动曝光为 0 时更稳定。</td></tr>
            <tr><td>手动增益值</td><td>0 到 30</td><td>自动增益为 0 时更稳定。</td></tr>
          </tbody>
        </table>
      </section>
      <section class="card span-6">
        <div class="label">硬件控制台</div>
        <div class="kv">
          <div><span>设备</span><select id="regDevice"><option value="ap3216c">AP3216C</option><option value="es8388">ES8388</option><option value="qma6100p">QMA6100P</option><option value="xl9555">XL9555</option><option value="ov5640">OV5640</option></select></div>
          <div><span>寄存器地址 十进制</span><input id="regAddr" type="number" min="0" max="65535" step="1" value="0"></div>
          <div><span>掩码 十进制 0-255</span><input id="regMask" type="number" min="0" max="255" step="1" value="255"></div>
          <div><span>写入值 十进制 0-255</span><input id="regValue" type="number" min="0" max="255" step="1" value="0"></div>
        </div>
        <div class="toolbar">
          <button id="regReadBtn" class="btn" type="button">读寄存器</button>
          <button id="regWriteBtn" class="btn" type="button">写寄存器</button>
        </div>
        <div class="meta mono" id="regState">寄存器控制台就绪：只接受十进制整数</div>
        <div class="meta">外设寄存器只接受十进制数字，不接受 0x 前缀。默认读操作安全；写操作只开放 ES8388 音量寄存器 46 到 49，其他写入会被拦截。</div>
        <div class="kv">
          <div><span>AP3216C 工作模式</span><select id="apModeControl"><option value="0">0 低功耗/暂停</option><option value="1">1 只开环境光</option><option value="2">2 只开接近/红外</option><option value="3" selected>3 全部开启</option></select></div>
          <div><span>QMA6100P 量程 g</span><select id="qmaRangeControl"><option>2</option><option>4</option><option selected>8</option><option>16</option></select></div>
          <div><span>ES8388 喇叭音量 0-33</span><input id="speakerVolumeControl" type="number" min="0" max="33" step="1" value="26"></div>
          <div><span>ES8388 耳机音量 0-33</span><input id="headphoneVolumeControl" type="number" min="0" max="33" step="1" value="18"></div>
        </div>
        <div class="toolbar">
          <button id="apModeBtn" class="btn" type="button">设置 AP3216C</button>
          <button id="qmaRangeBtn" class="btn" type="button">设置 QMA 量程</button>
          <button id="speakerVolumeBtn" class="btn" type="button">设置喇叭音量</button>
          <button id="headphoneVolumeBtn" class="btn" type="button">设置耳机音量</button>
        </div>
        <div class="meta mono" id="peripheralState">外设预设就绪：写入后会读取寄存器确认</div>
        <table>
          <tbody>
            <tr><th>设备</th><th>可读地址 十进制</th><th>可写范围 十进制</th><th>怎么设</th></tr>
            <tr><td>AP3216C 光照</td><td>0 到 255，常用 0 和 10 到 15</td><td>只读</td><td>地址 0 读系统模式；地址 10 到 15 读红外、环境光、接近数据。写入被禁止。</td></tr>
            <tr><td>ES8388 音频</td><td>0 到 255</td><td>寄存器 46 到 49，写入值 0 到 33</td><td>46 左耳机音量，47 右耳机音量，48 左喇叭/输出音量，49 右喇叭/输出音量；掩码通常填 255。</td></tr>
            <tr><td>QMA6100P 姿态</td><td>0 到 255，常用 0 到 6</td><td>只读</td><td>地址 0 读芯片标识；地址 1 到 6 读加速度原始数据。写入被禁止。</td></tr>
            <tr><td>XL9555 IO 扩展</td><td>0 到 7</td><td>只读</td><td>这里连接屏幕、摄像头复位和电源控制，避免误操作导致外设掉线，所以写入被禁止。</td></tr>
            <tr><td>OV5640 摄像头</td><td>0 到 65535</td><td>只读诊断</td><td>寄存器只用于诊断读取；亮度、曝光、增益、镜像请使用上面的摄像头设置。</td></tr>
          </tbody>
        </table>
        <div class="control-row"><span>CPU MHz</span><select id="cpuMhzControl"><option>80</option><option>160</option><option selected>240</option></select><button id="cpuMhzBtn" class="btn" type="button">设</button></div>
        <div class="control-row"><span>WiFi Tx</span><select id="wifiPowerControl"><option value="-4">-1dBm</option><option value="8">2dBm</option><option value="20">5dBm</option><option value="28">7dBm</option><option value="34">8.5dBm</option><option value="44">11dBm</option><option value="52">13dBm</option><option value="60">15dBm</option><option value="68">17dBm</option><option value="78">19.5dBm</option></select><button id="wifiPowerBtn" class="btn" type="button">设</button></div>
      </section>

      <section class="card span-6">
        <div class="label">温湿度与芯片温度</div>
        <canvas id="climateChart" width="560" height="200"></canvas>
      </section>
      <section class="card span-6">
        <div class="label">环境光与声音</div>
        <canvas id="envChart" width="560" height="200"></canvas>
      </section>
      <section class="card span-6">
        <div class="label">姿态与 CPU</div>
        <canvas id="motionChart" width="560" height="200"></canvas>
      </section>
      <section class="card span-6">
        <div class="label">最近持久化样本</div>
        <table>
          <thead>
            <tr><th>时间</th><th>DHT</th><th>光照</th><th>声音</th><th>系统</th></tr>
          </thead>
          <tbody id="rows"></tbody>
        </table>
      </section>
    </div>
  </div>

  <script>
    const statusEls = {
      netState: document.getElementById('netState'),
      storageState: document.getElementById('storageState'),
      displayState: document.getElementById('displayState'),
      tempC: document.getElementById('tempC'),
      humidity: document.getElementById('humidity'),
      dhtState: document.getElementById('dhtState'),
      dhtCount: document.getElementById('dhtCount'),
      soundDb: document.getElementById('soundDb'),
      soundState: document.getElementById('soundState'),
      cpuUsage: document.getElementById('cpuUsage'),
      cpuState: document.getElementById('cpuState'),
      alsValue: document.getElementById('alsValue'),
      lightState: document.getElementById('lightState'),
      motionValue: document.getElementById('motionValue'),
      motionState: document.getElementById('motionState'),
      chipTemp: document.getElementById('chipTemp'),
      chipState: document.getElementById('chipState'),
      heapFree: document.getElementById('heapFree'),
      heapState: document.getElementById('heapState'),
      persistedCount: document.getElementById('persistedCount'),
      lastSample: document.getElementById('lastSample'),
      storageHealth: document.getElementById('storageHealth'),
      lcdPage: document.getElementById('lcdPage'),
      lcdState: document.getElementById('lcdState'),
      uptime: document.getElementById('uptime'),
      cpuFreq: document.getElementById('cpuFreq'),
      topTask: document.getElementById('topTask'),
      resetReason: document.getElementById('resetReason'),
      speakerLoopback: document.getElementById('speakerLoopback'),
      speakerState: document.getElementById('speakerState'),
      alertLine: document.getElementById('alertLine'),
      alertCount: document.getElementById('alertCount'),
      alertSummary: document.getElementById('alertSummary'),
      cameraState: document.getElementById('cameraState'),
      cameraMeta: document.getElementById('cameraMeta'),
      cameraFrame: document.getElementById('cameraFrame'),
      camFrameSize: document.getElementById('camFrameSize'),
      camQuality: document.getElementById('camQuality'),
      camBrightness: document.getElementById('camBrightness'),
      camContrast: document.getElementById('camContrast'),
      camSaturation: document.getElementById('camSaturation'),
      camAec: document.getElementById('camAec'),
      camAecValue: document.getElementById('camAecValue'),
      camAgc: document.getElementById('camAgc'),
      camAgcGain: document.getElementById('camAgcGain'),
      camMirror: document.getElementById('camMirror'),
      camVflip: document.getElementById('camVflip'),
      regDevice: document.getElementById('regDevice'),
      regAddr: document.getElementById('regAddr'),
      regMask: document.getElementById('regMask'),
      regValue: document.getElementById('regValue'),
      regState: document.getElementById('regState'),
      regReadBtn: document.getElementById('regReadBtn'),
      regWriteBtn: document.getElementById('regWriteBtn'),
      apModeControl: document.getElementById('apModeControl'),
      apModeBtn: document.getElementById('apModeBtn'),
      qmaRangeControl: document.getElementById('qmaRangeControl'),
      qmaRangeBtn: document.getElementById('qmaRangeBtn'),
      speakerVolumeControl: document.getElementById('speakerVolumeControl'),
      speakerVolumeBtn: document.getElementById('speakerVolumeBtn'),
      headphoneVolumeControl: document.getElementById('headphoneVolumeControl'),
      headphoneVolumeBtn: document.getElementById('headphoneVolumeBtn'),
      peripheralState: document.getElementById('peripheralState'),
      cpuMhzControl: document.getElementById('cpuMhzControl'),
      cpuMhzBtn: document.getElementById('cpuMhzBtn'),
      wifiPowerControl: document.getElementById('wifiPowerControl'),
      wifiPowerBtn: document.getElementById('wifiPowerBtn'),
      configState: document.getElementById('configState'),
      cfgTempHigh: document.getElementById('cfgTempHigh'),
      cfgHumidityHigh: document.getElementById('cfgHumidityHigh'),
      cfgSoundHigh: document.getElementById('cfgSoundHigh'),
      cfgLightHigh: document.getElementById('cfgLightHigh'),
      cfgCpuHigh: document.getElementById('cfgCpuHigh'),
      cfgHeapLow: document.getElementById('cfgHeapLow'),
      rows: document.getElementById('rows'),
      speakTempBtn: document.getElementById('speakTempBtn'),
      speakerSelfTestBtn: document.getElementById('speakerSelfTestBtn'),
      saveConfigBtn: document.getElementById('saveConfigBtn'),
    };
    const charts = {
      climate: document.getElementById('climateChart').getContext('2d'),
      env: document.getElementById('envChart').getContext('2d'),
      motion: document.getElementById('motionChart').getContext('2d'),
    };
    let latestStatus = null;
    let configInputsDirty = false;
    let cameraStreamTimer = 0;
    let cameraStreamStarted = false;
    const LIVE_POLL_MS = 500;
    const SNAPSHOT_POLL_MS = 10000;
    const CAMERA_STREAM_PORT = 81;
    const CAMERA_STREAM_TARGET_FPS = 20;
    const CAMERA_STREAM_RETRY_MS = 1200;

    function fmtBool(online) { return online ? '在线' : '离线'; }
    function fmtNum(v, digits = 1) { return (v === null || v === undefined) ? '--' : Number(v).toFixed(digits); }
    function fmtTime(epoch) {
      if (!epoch) return '--';
      return new Date(epoch * 1000).toLocaleTimeString('zh-CN', { hour12: false });
    }
    function fmtBytes(bytes) {
      if (bytes === null || bytes === undefined) return '--';
      return `${(Number(bytes) / 1024).toFixed(1)} KB`;
    }
    function fmtDuration(seconds) {
      const sec = Number(seconds || 0);
      const h = Math.floor(sec / 3600);
      const m = Math.floor((sec % 3600) / 60);
      const s = sec % 60;
      return `${h}h ${m}m ${s}s`;
    }

    function drawMultiLine(ctx, labels, series, padding = 0) {
      const w = ctx.canvas.width;
      const h = ctx.canvas.height;
      ctx.clearRect(0, 0, w, h);
      ctx.strokeStyle = '#223446';
      ctx.lineWidth = 1;
      for (let i = 0; i < 5; i++) {
        const y = 18 + i * ((h - 36) / 4);
        ctx.beginPath();
        ctx.moveTo(16, y);
        ctx.lineTo(w - 16, y);
        ctx.stroke();
      }
      if (!labels.length) {
        ctx.fillStyle = '#8aa0b6';
        ctx.font = '14px Segoe UI';
        ctx.fillText('等待样本...', 24, 28);
        return;
      }

      let min = Number.POSITIVE_INFINITY;
      let max = Number.NEGATIVE_INFINITY;
      for (const line of series) {
        for (const value of line.data) {
          if (typeof value === 'number' && isFinite(value)) {
            min = Math.min(min, value);
            max = Math.max(max, value);
          }
        }
      }
      if (!isFinite(min) || !isFinite(max)) {
        min = 0;
        max = 1;
      }
      if (min === max) {
        min -= 1;
        max += 1;
      }
      min -= padding;
      max += padding;

      const xStep = labels.length > 1 ? (w - 32) / (labels.length - 1) : (w - 32);
      const yFor = (value) => h - 18 - ((value - min) / (max - min)) * (h - 36);

      for (const line of series) {
        ctx.strokeStyle = line.color;
        ctx.lineWidth = 2.5;
        let active = false;
        line.data.forEach((value, index) => {
          if (typeof value !== 'number' || !isFinite(value)) {
            active = false;
            return;
          }
          const x = 16 + index * xStep;
          const y = yFor(value);
          if (!active) {
            ctx.beginPath();
            ctx.moveTo(x, y);
            active = true;
          } else {
            ctx.lineTo(x, y);
          }
          const next = line.data[index + 1];
          if (typeof next !== 'number' || !isFinite(next)) {
            ctx.stroke();
            active = false;
          }
        });
        if (active) ctx.stroke();
      }

      ctx.font = '12px Segoe UI';
      ctx.fillStyle = '#8aa0b6';
      ctx.fillText(`范围 ${min.toFixed(1)} - ${max.toFixed(1)}`, 20, 16);
      let legendX = w - 16;
      [...series].reverse().forEach(line => {
        const width = ctx.measureText(line.name).width + 18;
        legendX -= width;
        ctx.fillStyle = line.color;
        ctx.fillRect(legendX, 9, 10, 10);
        ctx.fillStyle = '#cfe0f1';
        ctx.fillText(line.name, legendX + 14, 18);
      });
    }

    function renderRows(rows) {
      statusEls.rows.innerHTML = rows.map(row => `
        <tr>
          <td class="mono">${fmtTime(row.epoch)}</td>
          <td>${row.dht_online ? `${fmtNum(row.dht_temp_c)}C / ${fmtNum(row.dht_humidity)}%` : 'offline'}</td>
          <td>${row.light_online ? `ALS ${row.als} / IR ${row.ir}` : 'offline'}</td>
          <td>${row.mic_online ? `${fmtNum(row.mic_dbfs)} dBFS` : 'offline'}</td>
          <td>CPU ${fmtNum(row.cpu_usage_pct)}% / Heap ${fmtBytes(row.free_heap_bytes)}</td>
        </tr>
      `).join('');
    }

    function speakTemperature() {
      if (!latestStatus || !latestStatus.dht11 || !latestStatus.dht11.online) {
        return;
      }
      statusEls.speakerState.textContent = 'board speaker request queued';
      fetch('/api/speak_temperature', { method: 'POST', cache: 'no-store' })
        .then(() => setTimeout(refreshLive, 1200))
        .catch(() => {
          statusEls.speakerState.textContent = 'board speaker request failed';
        });
    }

    statusEls.speakTempBtn.addEventListener('click', speakTemperature);
    statusEls.speakerSelfTestBtn.addEventListener('click', async () => {
      try {
        statusEls.speakerState.textContent = 'self test running...';
        await fetch('/api/speaker_test', { method: 'POST', cache: 'no-store' });
        setTimeout(refreshLive, 1200);
      } catch (error) {
        statusEls.speakerState.textContent = 'self test trigger failed';
      }
    });
    statusEls.saveConfigBtn.addEventListener('click', async () => {
      const params = new URLSearchParams({
        temp_high_c: statusEls.cfgTempHigh.value,
        humidity_high_pct: statusEls.cfgHumidityHigh.value,
        sound_high_dbfs: statusEls.cfgSoundHigh.value,
        light_high_als: statusEls.cfgLightHigh.value,
        cpu_high_pct: statusEls.cfgCpuHigh.value,
        heap_low_bytes: statusEls.cfgHeapLow.value,
      });
      try {
        statusEls.configState.textContent = 'saving...';
        const res = await fetch(`/api/config?${params.toString()}`, { method: 'POST', cache: 'no-store' });
        statusEls.configState.textContent = res.ok ? 'saved to LittleFS' : 'save failed';
        if (res.ok) configInputsDirty = false;
        setTimeout(refreshSnapshot, 600);
      } catch (error) {
        statusEls.configState.textContent = 'save failed';
      }
    });
    [
      statusEls.cfgTempHigh,
      statusEls.cfgHumidityHigh,
      statusEls.cfgSoundHigh,
      statusEls.cfgLightHigh,
      statusEls.cfgCpuHigh,
      statusEls.cfgHeapLow,
    ].forEach(input => input.addEventListener('input', () => {
      configInputsDirty = true;
    }));

    const cameraControlInputKeys = [
      'camFrameSize',
      'camQuality',
      'camBrightness',
      'camContrast',
      'camSaturation',
      'camAec',
      'camAecValue',
      'camAgc',
      'camAgcGain',
      'camMirror',
      'camVflip',
    ];

    function markCameraControlEditing(input) {
      if (input) input.dataset.userEditing = '1';
    }

    function clearCameraControlEditing(input) {
      if (input) delete input.dataset.userEditing;
    }

    function bindCameraControlEditing() {
      cameraControlInputKeys.forEach(key => {
        const input = statusEls[key];
        if (!input) return;
        ['focus', 'pointerdown', 'input', 'change'].forEach(eventName => {
          input.addEventListener(eventName, () => markCameraControlEditing(input));
        });
      });
    }

    function updateCameraControlFromStatus(input, value) {
      if (!input) return;
      if (input.dataset.userEditing === '1' || document.activeElement === input) return;
      input.value = value;
    }

    function clearAllCameraControlEditing() {
      cameraControlInputKeys.forEach(key => clearCameraControlEditing(statusEls[key]));
    }
    bindCameraControlEditing();

    const cameraControlLabels = {
      framesize_name: '分辨率',
      framesize: '分辨率',
      quality: 'JPEG 质量',
      brightness: '亮度',
      contrast: '对比度',
      saturation: '饱和度',
      aec: '自动曝光',
      aec_value: '手动曝光值',
      agc: '自动增益',
      agc_gain: '手动增益值',
      hmirror: '镜像',
      vflip: '翻转',
    };

    function cameraControlLabel(name) {
      return cameraControlLabels[name] || name;
    }

    function getCameraEffectiveValue(camera, name) {
      if (!camera) return undefined;
      if (name === 'framesize_name' || name === 'framesize') return camera.frame_size_name;
      return camera[name];
    }

    async function applyCameraControl(name, value, input) {
      const params = new URLSearchParams({ name, value });
      const res = await fetch(`/api/camera/control?${params.toString()}`, { method: 'POST', cache: 'no-store' });
      const json = await res.json().catch(() => ({}));
      if (!res.ok || json.applied !== true) {
        statusEls.cameraMeta.textContent = `未生效：${cameraControlLabel(name)} ${json.error || res.status}`;
        setTimeout(refreshLive, 300);
        return;
      }

      const status = await fetch('/api/camera', { cache: 'no-store' }).then(r => r.json());
      const canonicalName = json.name || name;
      const effective = getCameraEffectiveValue(status.camera, canonicalName);
      const expected = (canonicalName === 'framesize' || canonicalName === 'framesize_name') ? String(value) : Number(value);
      const actual = (canonicalName === 'framesize' || canonicalName === 'framesize_name') ? String(effective) : Number(effective);
      const verified = json.verified === true && actual === expected;
      statusEls.cameraMeta.textContent = verified
        ? `已生效：${cameraControlLabel(name)}=${effective}`
        : `未生效：${cameraControlLabel(name)} 目标=${value} 回读=${effective}`;
      if (verified) {
        clearCameraControlEditing(input);
        if (input) input.value = String(effective);
      }
      setTimeout(refreshLive, 300);
    }

    async function applyCameraPreset(preset) {
      const params = new URLSearchParams({ preset });
      const res = await fetch(`/api/camera/preset?${params.toString()}`, { method: 'POST', cache: 'no-store' });
      const json = await res.json().catch(() => ({}));
      clearAllCameraControlEditing();
      statusEls.cameraMeta.textContent = json.applied
        ? `一键预设已生效：${json.label || preset} / ${json.verified_count}/${json.control_count}`
        : `一键预设失败：${json.error || res.status}`;
      setTimeout(refreshLive, 300);
    }

    document.querySelectorAll('[data-cam]').forEach(button => {
      button.addEventListener('click', () => {
        const input = document.getElementById(button.dataset.camInput);
        applyCameraControl(button.dataset.cam, input.value, input).catch(() => {
          statusEls.cameraMeta.textContent = 'camera control failed';
        });
      });
    });
    document.querySelectorAll('[data-cam-select]').forEach(button => {
      button.addEventListener('click', () => {
        const input = document.getElementById(button.dataset.camInput);
        applyCameraControl(button.dataset.camSelect, input.value, input).catch(() => {
          statusEls.cameraMeta.textContent = 'camera control failed';
        });
      });
    });
    document.querySelectorAll('[data-camera-preset]').forEach(button => {
      button.addEventListener('click', () => {
        applyCameraPreset(button.dataset.cameraPreset).catch(error => {
          statusEls.cameraMeta.textContent = `一键预设失败：${error.message}`;
        });
      });
    });

    function readDecimalInput(input, min, max, label) {
      const raw = String(input.value || '').trim();
      if (!/^[0-9]+$/.test(raw)) {
        throw new Error(`${label} 只接受十进制整数`);
      }
      const value = Number(raw);
      if (!Number.isInteger(value) || value < min || value > max) {
        throw new Error(`${label} 范围是 ${min} 到 ${max}`);
      }
      return String(value);
    }

    async function accessRegister(write) {
      const params = new URLSearchParams({
        device: statusEls.regDevice.value,
        reg: readDecimalInput(statusEls.regAddr, 0, 65535, '寄存器地址'),
        mask: readDecimalInput(statusEls.regMask, 0, 255, '掩码'),
      });
      if (write) params.set('value', readDecimalInput(statusEls.regValue, 0, 255, '写入值'));
      const res = await fetch(`/api/register?${params.toString()}`, { method: write ? 'POST' : 'GET', cache: 'no-store' });
      const json = await res.json();
      if (json.ok) statusEls.regValue.value = String(Number(json.value));
      statusEls.regState.textContent = `${json.ok ? '成功' : '失败'} 十进制 ${JSON.stringify(json)}`;
    }
    statusEls.regReadBtn.addEventListener('click', () => accessRegister(false).catch(error => {
      statusEls.regState.textContent = error.message;
    }));
    statusEls.regWriteBtn.addEventListener('click', () => accessRegister(true).catch(error => {
      statusEls.regState.textContent = error.message;
    }));
    async function applyPeripheralControl(device, name, value) {
      const params = new URLSearchParams({ device, name, value });
      const res = await fetch(`/api/peripheral/control?${params.toString()}`, { method: 'POST', cache: 'no-store' });
      const json = await res.json().catch(() => ({}));
      statusEls.peripheralState.textContent = json.applied
        ? `已生效：${json.device}.${json.name}=${json.effective}`
        : `未生效：${device}.${name} ${json.error || res.status}`;
      setTimeout(refreshLive, 300);
    }
    statusEls.apModeBtn.addEventListener('click', () => applyPeripheralControl('ap3216c', 'mode', statusEls.apModeControl.value).catch(error => {
      statusEls.peripheralState.textContent = error.message;
    }));
    statusEls.qmaRangeBtn.addEventListener('click', () => applyPeripheralControl('qma6100p', 'range_g', statusEls.qmaRangeControl.value).catch(error => {
      statusEls.peripheralState.textContent = error.message;
    }));
    statusEls.speakerVolumeBtn.addEventListener('click', () => applyPeripheralControl('es8388', 'speaker_volume', statusEls.speakerVolumeControl.value).catch(error => {
      statusEls.peripheralState.textContent = error.message;
    }));
    statusEls.headphoneVolumeBtn.addEventListener('click', () => applyPeripheralControl('es8388', 'headphone_volume', statusEls.headphoneVolumeControl.value).catch(error => {
      statusEls.peripheralState.textContent = error.message;
    }));
    statusEls.cpuMhzBtn.addEventListener('click', async () => {
      const params = new URLSearchParams({ cpu_mhz: statusEls.cpuMhzControl.value });
      await fetch(`/api/system/control?${params.toString()}`, { method: 'POST', cache: 'no-store' });
      setTimeout(refreshLive, 300);
    });
    statusEls.wifiPowerBtn.addEventListener('click', async () => {
      const params = new URLSearchParams({ wifi_tx_power: statusEls.wifiPowerControl.value });
      await fetch(`/api/system/control?${params.toString()}`, { method: 'POST', cache: 'no-store' });
      setTimeout(refreshLive, 300);
    });

    function renderLive(status) {
      latestStatus = status;

      statusEls.netState.textContent = `${status.network.mode} ${status.network.ip}`;
      statusEls.storageState.textContent = status.storage.used_bytes === undefined
        ? `LittleFS persisted ${status.storage.persisted_samples}`
        : `LittleFS ${status.storage.used_bytes}/${status.storage.total_bytes} bytes`;
      statusEls.displayState.textContent = `LCD ${status.display.online ? 'online' : 'offline'} / page ${status.display.page_name}`;
      statusEls.tempC.textContent = status.dht11.online ? `${fmtNum(status.dht11.temp_c)} °C` : '--';
      statusEls.humidity.textContent = status.dht11.online ? `${fmtNum(status.dht11.humidity)} %` : '--';
      statusEls.dhtState.textContent = `DHT11 ${fmtBool(status.dht11.online)}`;
      statusEls.dhtState.className = `meta ${status.dht11.online ? 'ok' : 'bad'}`;
      statusEls.dhtCount.textContent = `success ${status.dht11.success_count} / fail ${status.dht11.failure_count}`;
      statusEls.soundDb.textContent = status.mic.online ? `${fmtNum(status.mic.dbfs)} dBFS` : '--';
      statusEls.soundState.textContent = status.mic.online ? `rms ${fmtNum(status.mic.rms, 3)} peak ${status.mic.peak}` : 'Mic offline';
      statusEls.soundState.className = `meta ${status.mic.online ? 'ok' : 'bad'}`;
      statusEls.cpuUsage.textContent = status.system.runtime_ready ? `${fmtNum(status.system.cpu_usage_pct)} %` : '--';
      const topTaskAvailable = typeof status.system.top_task === 'string'
        && status.system.top_task.length > 0
        && typeof status.system.top_task_cpu_pct === 'number'
        && Number.isFinite(status.system.top_task_cpu_pct);
      statusEls.cpuState.textContent = status.system.runtime_ready
        ? `RSSI ${status.network.rssi_dbm} dBm / top task ${topTaskAvailable ? 'available' : 'unavailable'}`
        : 'runtime stats pending';
      statusEls.cpuState.className = `meta ${status.system.runtime_ready ? 'ok' : 'bad'}`;
      statusEls.alsValue.textContent = status.ap3216c.online ? `ALS ${status.ap3216c.als}` : 'ALS --';
      statusEls.lightState.textContent = status.ap3216c.online ? `IR ${status.ap3216c.ir} / PS ${status.ap3216c.ps}` : 'AP3216C offline';
      statusEls.lightState.className = `meta ${status.ap3216c.online ? 'ok' : 'bad'}`;
      statusEls.motionValue.textContent = status.qma6100p.online ? `g ${fmtNum(status.qma6100p.ag, 2)}` : 'g --';
      statusEls.motionState.textContent = status.qma6100p.online
        ? `X ${fmtNum(status.qma6100p.ax, 2)} / Y ${fmtNum(status.qma6100p.ay, 2)} / Z ${fmtNum(status.qma6100p.az, 2)}`
        : 'QMA6100P offline';
      statusEls.motionState.className = `meta ${status.qma6100p.online ? 'ok' : 'bad'}`;
      statusEls.chipTemp.textContent = status.chip_temp.online ? `${fmtNum(status.chip_temp.temp_c)} °C` : '--';
      statusEls.chipState.textContent = `Chip Temp ${fmtBool(status.chip_temp.online)}`;
      statusEls.chipState.className = `meta ${status.chip_temp.online ? 'ok' : 'bad'}`;
      statusEls.heapFree.textContent = fmtBytes(status.system.free_heap_bytes);
      statusEls.heapState.textContent = `min ${fmtBytes(status.system.min_free_heap_bytes)} / max alloc ${fmtBytes(status.system.max_alloc_heap_bytes)}`;
      statusEls.persistedCount.textContent = status.storage.persisted_samples;
      statusEls.lastSample.textContent = `最近落盘 ${fmtTime(status.storage.last_sample_epoch)}`;
      statusEls.storageHealth.textContent = `pending ${status.storage.pending_bytes} / write fail ${status.storage.write_failures} / drop ${status.storage.dropped_samples}`;
      statusEls.lcdPage.textContent = `${status.display.page + 1}. ${status.display.page_name}`;
      statusEls.lcdState.textContent = status.display.online ? '离线屏幕轮播正常' : 'LCD offline';
      statusEls.lcdState.className = `meta ${status.display.online ? 'ok' : 'bad'}`;
      statusEls.uptime.textContent = fmtDuration(status.system.uptime_sec);
      statusEls.cpuFreq.textContent = `${status.system.cpu_freq_mhz} MHz`;
      statusEls.topTask.textContent = topTaskAvailable
        ? `${status.system.top_task} ${fmtNum(status.system.top_task_cpu_pct)}%`
        : 'N/A';
      statusEls.resetReason.textContent = status.system.reset_reason;
      statusEls.speakerLoopback.textContent = status.speaker.verification_state;
      statusEls.speakerState.textContent = `amp-off ${fmtNum(status.speaker.muted_dbfs)} / amp-on ${fmtNum(status.speaker.enabled_dbfs)} / delta ${fmtNum(status.speaker.delta_dbfs)} / spk ${status.speaker.speaker_volume} / hp ${status.speaker.headphone_volume} / say ${status.speaker.speak_count}${status.speaker.has_spoken_temp ? ` / last ${status.speaker.last_spoken_temp_c}C` : ''}`;
      statusEls.speakerState.className = `meta ${status.speaker.verification_state === 'PASS' ? 'ok' : status.speaker.verification_state === 'FAIL' ? 'bad' : ''}`;
      statusEls.alertCount.textContent = status.alerts.count;
      statusEls.alertSummary.textContent = status.alerts.summary;
      statusEls.alertSummary.className = `meta ${status.alerts.active ? 'bad' : 'ok'}`;
      statusEls.alertLine.style.display = status.alerts.active ? 'block' : 'none';
      statusEls.alertLine.textContent = status.alerts.summary;
      if (status.camera) {
        statusEls.cameraState.textContent = status.camera.online ? `${status.camera.name} ${status.camera.frame_size_name}` : 'offline';
        statusEls.cameraState.className = `value sm ${status.camera.online ? 'ok' : 'bad'}`;
        statusEls.cameraMeta.textContent = `pid 0x${Number(status.camera.pid).toString(16)} / ${status.camera.last_width}x${status.camera.last_height} / ${status.camera.last_frame_bytes} bytes / cap ${status.camera.capture_count} / ${fmtNum(status.camera.stream_fps, 1)} fps / stream ${status.camera.stream_clients}`;
        updateCameraControlFromStatus(statusEls.camFrameSize, status.camera.frame_size_name);
        updateCameraControlFromStatus(statusEls.camQuality, status.camera.quality);
        updateCameraControlFromStatus(statusEls.camBrightness, status.camera.brightness);
        updateCameraControlFromStatus(statusEls.camContrast, status.camera.contrast);
        updateCameraControlFromStatus(statusEls.camSaturation, status.camera.saturation);
        updateCameraControlFromStatus(statusEls.camAec, status.camera.aec ? '1' : '0');
        updateCameraControlFromStatus(statusEls.camAecValue, status.camera.aec_value);
        updateCameraControlFromStatus(statusEls.camAgc, status.camera.agc ? '1' : '0');
        updateCameraControlFromStatus(statusEls.camAgcGain, status.camera.agc_gain);
        updateCameraControlFromStatus(statusEls.camMirror, status.camera.hmirror ? '1' : '0');
        updateCameraControlFromStatus(statusEls.camVflip, status.camera.vflip ? '1' : '0');
      }
      if (status.ap3216c && document.activeElement !== statusEls.apModeControl) {
        statusEls.apModeControl.value = String(status.ap3216c.mode);
      }
      if (status.qma6100p && document.activeElement !== statusEls.qmaRangeControl) {
        statusEls.qmaRangeControl.value = String(status.qma6100p.range_g);
      }
      if (status.speaker) {
        if (document.activeElement !== statusEls.speakerVolumeControl) statusEls.speakerVolumeControl.value = status.speaker.speaker_volume;
        if (document.activeElement !== statusEls.headphoneVolumeControl) statusEls.headphoneVolumeControl.value = status.speaker.headphone_volume;
      }
      if (status.config) {
        if (!configInputsDirty) {
          statusEls.cfgTempHigh.value = status.config.temp_high_c;
          statusEls.cfgHumidityHigh.value = status.config.humidity_high_pct;
          statusEls.cfgSoundHigh.value = status.config.sound_high_dbfs;
          statusEls.cfgLightHigh.value = status.config.light_high_als;
          statusEls.cfgCpuHigh.value = status.config.cpu_high_pct;
          statusEls.cfgHeapLow.value = status.config.heap_low_bytes;
        }
        statusEls.configState.textContent = `config persisted ${status.config.persisted ? 'yes' : 'defaults'}`;
      }
    }

    async function refreshLive() {
      const liveRes = await fetch('/api/live', { cache: 'no-store' });
      const live = await liveRes.json();
      renderLive(live);
    }

    async function refreshSnapshot() {
      const [statusRes, historyRes] = await Promise.all([
        fetch('/api/status', { cache: 'no-store' }),
        fetch('/api/history', { cache: 'no-store' }),
      ]);
      const status = await statusRes.json();
      const history = await historyRes.json();
      renderLive(status);

      const rows = history.rows || [];
      renderRows(rows.slice().reverse().slice(0, 8));
      const labels = rows.map(row => fmtTime(row.epoch));
      drawMultiLine(charts.climate, labels, [
        { name: 'Temp', color: '#ff974f', data: rows.map(row => row.dht_online ? row.dht_temp_c : null) },
        { name: 'Humi', color: '#4bc8f3', data: rows.map(row => row.dht_online ? row.dht_humidity : null) },
        { name: 'Chip', color: '#61b4ff', data: rows.map(row => row.chip_online ? row.chip_temp_c : null) },
      ]);
      drawMultiLine(charts.env, labels, [
        { name: 'ALS', color: '#ffd166', data: rows.map(row => row.light_online ? row.als : null) },
        { name: 'dBFS', color: '#b289ff', data: rows.map(row => row.mic_online ? row.mic_dbfs : null) },
      ]);
      drawMultiLine(charts.motion, labels, [
        { name: 'g', color: '#62d699', data: rows.map(row => row.accel_online ? row.ag : null) },
        { name: 'Pitch', color: '#61b4ff', data: rows.map(row => row.accel_online ? row.pitch : null) },
        { name: 'CPU', color: '#ff7a91', data: rows.map(row => row.cpu_usage_pct) },
      ]);
    }

    async function liveLoop() {
      try {
        await refreshLive();
      } catch (error) {
        statusEls.netState.textContent = 'dashboard fetch failed';
      } finally {
        setTimeout(liveLoop, LIVE_POLL_MS);
      }
    }

    async function snapshotLoop() {
      try {
        await refreshSnapshot();
      } catch (error) {
        statusEls.storageHealth.textContent = 'snapshot fetch failed';
      } finally {
        setTimeout(snapshotLoop, SNAPSHOT_POLL_MS);
      }
    }

    function cameraStreamUrl() {
      return `${location.protocol}//${location.hostname}:${CAMERA_STREAM_PORT}/stream.mjpg?t=${Date.now()}`;
    }

    function scheduleCameraStream(delayMs) {
      if (cameraStreamTimer) clearTimeout(cameraStreamTimer);
      cameraStreamTimer = setTimeout(startCameraStream, delayMs);
    }

    function startCameraStream() {
      if (cameraStreamStarted) {
        return;
      }
      if (latestStatus && latestStatus.camera && latestStatus.camera.online) {
        cameraStreamStarted = true;
        statusEls.cameraFrame.src = cameraStreamUrl();
      } else {
        scheduleCameraStream(CAMERA_STREAM_RETRY_MS);
      }
    }

    statusEls.cameraFrame.addEventListener('error', () => {
      cameraStreamStarted = false;
      scheduleCameraStream(CAMERA_STREAM_RETRY_MS);
    });

    snapshotLoop();
    liveLoop();
    scheduleCameraStream(100);
  </script>
</body>
</html>
)HTML";

const SampleRow &historyAt(size_t index) {
  return gHistory[(gHistoryHead + index) % kHistoryCapacity];
}

void pushHistory(const SampleRow &row) {
  size_t slot = (gHistoryHead + gHistoryCount) % kHistoryCapacity;
  gHistory[slot] = row;
  if (gHistoryCount < kHistoryCapacity) {
    gHistoryCount++;
  } else {
    gHistoryHead = (gHistoryHead + 1) % kHistoryCapacity;
  }
}

uint32_t currentEpoch() {
  const time_t now = time(nullptr);
  if (now >= 1700000000) {
    return static_cast<uint32_t>(now);
  }
  return gPersistedLastSampleEpoch;
}

String currentDashboardIp() {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return WiFi.softAPIP().toString();
}

String currentNetworkMode() {
  return WiFi.status() == WL_CONNECTED ? "STA" : "AP";
}

int32_t currentRssiDbm() {
  return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

bool parseUnsignedArg(const char *name, uint32_t maxValue, uint32_t *value) {
  if (!value || !server.hasArg(name)) {
    return false;
  }
  const String raw = server.arg(name);
  if (raw.length() == 0) {
    return false;
  }
  char *end = nullptr;
  const unsigned long parsed = strtoul(raw.c_str(), &end, 0);
  if (!end || *end != '\0' || parsed > maxValue) {
    return false;
  }
  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseDecimalUnsignedArg(const char *name, uint32_t maxValue, uint32_t *value) {
  if (!value || !server.hasArg(name)) {
    return false;
  }
  const String raw = server.arg(name);
  if (raw.length() == 0) {
    return false;
  }
  for (size_t i = 0; i < raw.length(); i++) {
    if (!isdigit(static_cast<unsigned char>(raw[i]))) {
      return false;
    }
  }
  char *end = nullptr;
  const unsigned long parsed = strtoul(raw.c_str(), &end, 10);
  if (!end || *end != '\0' || parsed > maxValue) {
    return false;
  }
  *value = static_cast<uint32_t>(parsed);
  return true;
}

bool parseSignedText(const String &raw, int32_t minValue, int32_t maxValue, int32_t *value) {
  if (!value || raw.length() == 0) {
    return false;
  }
  char *end = nullptr;
  const long parsed = strtol(raw.c_str(), &end, 0);
  if (!end || *end != '\0' || parsed < minValue || parsed > maxValue) {
    return false;
  }
  *value = static_cast<int32_t>(parsed);
  return true;
}

uint8_t parseUint8Arg(const char *name, uint8_t fallback) {
  if (!server.hasArg(name)) {
    return fallback;
  }
  uint32_t value = 0;
  return parseUnsignedArg(name, 255, &value) ? static_cast<uint8_t>(value) : fallback;
}

uint16_t parseUint16Arg(const char *name, uint16_t fallback) {
  if (!server.hasArg(name)) {
    return fallback;
  }
  uint32_t value = 0;
  return parseUnsignedArg(name, 65535, &value) ? static_cast<uint16_t>(value) : fallback;
}

float clampFloat(float value, float minValue, float maxValue) {
  if (!isfinite(value)) {
    return minValue;
  }
  return constrain(value, minValue, maxValue);
}

uint32_t clampUint32(uint32_t value, uint32_t minValue, uint32_t maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

bool parseFloatStrict(String value, float minValue, float maxValue, float *out) {
  value.trim();
  if (!out || value.length() == 0) {
    return false;
  }

  char *end = nullptr;
  const float parsed = strtof(value.c_str(), &end);
  if (end == value.c_str() || !isfinite(parsed)) {
    return false;
  }
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
    end++;
  }
  if (*end != '\0') {
    return false;
  }
  *out = constrain(parsed, minValue, maxValue);
  return true;
}

bool parseLongStrict(String value, long minValue, long maxValue, long *out) {
  value.trim();
  if (!out || value.length() == 0) {
    return false;
  }

  char *end = nullptr;
  const long parsed = strtol(value.c_str(), &end, 10);
  if (end == value.c_str()) {
    return false;
  }
  while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') {
    end++;
  }
  if (*end != '\0') {
    return false;
  }
  if (parsed < minValue || parsed > maxValue) {
    return false;
  }
  *out = parsed;
  return true;
}

bool parseConfigLine(const String &line) {
  const int equalsIndex = line.indexOf('=');
  if (equalsIndex <= 0) {
    return false;
  }

  const String key = line.substring(0, equalsIndex);
  const String value = line.substring(equalsIndex + 1);
  float parsedFloat = 0.0f;
  long parsedLong = 0;
  if (key == "temp_high_c") {
    if (!parseFloatStrict(value, -20.0f, 80.0f, &parsedFloat)) {
      return false;
    }
    gConfig.tempHighC = parsedFloat;
  } else if (key == "humidity_high_pct") {
    if (!parseFloatStrict(value, 0.0f, 100.0f, &parsedFloat)) {
      return false;
    }
    gConfig.humidityHighPct = parsedFloat;
  } else if (key == "sound_high_dbfs") {
    if (!parseFloatStrict(value, -120.0f, 0.0f, &parsedFloat)) {
      return false;
    }
    gConfig.soundHighDbfs = parsedFloat;
  } else if (key == "light_high_als") {
    if (!parseLongStrict(value, 1, 65535, &parsedLong)) {
      return false;
    }
    gConfig.lightHighAls = static_cast<uint16_t>(parsedLong);
  } else if (key == "cpu_high_pct") {
    if (!parseFloatStrict(value, 0.0f, 100.0f, &parsedFloat)) {
      return false;
    }
    gConfig.cpuHighPct = parsedFloat;
  } else if (key == "heap_low_bytes") {
    if (!parseLongStrict(value, 8192, 320000, &parsedLong)) {
      return false;
    }
    gConfig.heapLowBytes = static_cast<uint32_t>(parsedLong);
  } else if (key == "speaker_alerts") {
    if (!parseLongStrict(value, 0, 1, &parsedLong)) {
      return false;
    }
    gConfig.speakerAlerts = parsedLong != 0;
  } else {
    return false;
  }
  return true;
}

bool loadConfigFile(const char *path) {
  if (!path || !LittleFS.exists(path)) {
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    return false;
  }

  bool loadedAny = false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      continue;
    }
    loadedAny = parseConfigLine(line) || loadedAny;
  }
  file.close();
  return loadedAny;
}

void loadConfigFromLittleFs() {
  if (!gLittleFsReady) {
    gConfigPersisted = false;
    return;
  }

  gConfigPersisted = loadConfigFile(kConfigPath) || loadConfigFile(kConfigBackupPath);
}

bool saveConfigToLittleFs() {
  if (!gLittleFsReady) {
    return false;
  }

  LittleFS.remove(kConfigTmpPath);
  File file = LittleFS.open(kConfigTmpPath, "w");
  if (!file) {
    gStorageWriteFailures++;
    return false;
  }

  file.printf("temp_high_c=%.2f\n", gConfig.tempHighC);
  file.printf("humidity_high_pct=%.2f\n", gConfig.humidityHighPct);
  file.printf("sound_high_dbfs=%.2f\n", gConfig.soundHighDbfs);
  file.printf("light_high_als=%u\n", static_cast<unsigned int>(gConfig.lightHighAls));
  file.printf("cpu_high_pct=%.2f\n", gConfig.cpuHighPct);
  file.printf("heap_low_bytes=%lu\n", static_cast<unsigned long>(gConfig.heapLowBytes));
  file.printf("speaker_alerts=%u\n", gConfig.speakerAlerts ? 1 : 0);
  file.flush();
  const bool writeOk = file.getWriteError() == 0 && file.size() > 20;
  file.close();

  if (!writeOk) {
    LittleFS.remove(kConfigTmpPath);
    gStorageWriteFailures++;
    gConfigPersisted = LittleFS.exists(kConfigPath);
    return false;
  }

  LittleFS.remove(kConfigBackupPath);
  if (LittleFS.exists(kConfigPath) && !LittleFS.rename(kConfigPath, kConfigBackupPath)) {
    LittleFS.remove(kConfigTmpPath);
    gStorageWriteFailures++;
    gConfigPersisted = true;
    return false;
  }

  const bool ok = LittleFS.rename(kConfigTmpPath, kConfigPath);
  if (!ok) {
    if (LittleFS.exists(kConfigBackupPath)) {
      LittleFS.rename(kConfigBackupPath, kConfigPath);
    }
    gStorageWriteFailures++;
    gConfigPersisted = LittleFS.exists(kConfigPath);
    return false;
  }

  LittleFS.remove(kConfigBackupPath);
  gConfigPersisted = true;
  return ok;
}

void appendAlertName(char *buffer, size_t bufferSize, const char *name, bool *first) {
  if (!buffer || !name || !first) {
    return;
  }
  const size_t used = strlen(buffer);
  if (used >= bufferSize - 1) {
    return;
  }
  snprintf(buffer + used, bufferSize - used, "%s%s", *first ? "" : ", ", name);
  *first = false;
}

void updateAlerts() {
  gAlerts.tempHigh = gDht.online && gDht.tempC >= gConfig.tempHighC;
  gAlerts.humidityHigh = gDht.online && gDht.humidity >= gConfig.humidityHighPct;
  gAlerts.soundHigh = gMic.online && gMic.dbfs >= gConfig.soundHighDbfs;
  gAlerts.lightHigh = gLight.online && gLight.als >= gConfig.lightHighAls;
  gAlerts.cpuHigh = gSystem.runtimeReady && gSystem.cpuUsagePct >= gConfig.cpuHighPct;
  gAlerts.heapLow = gSystem.freeHeapBytes > 0 && gSystem.freeHeapBytes <= gConfig.heapLowBytes;

  gAlerts.count = 0;
  gAlerts.count += gAlerts.tempHigh ? 1 : 0;
  gAlerts.count += gAlerts.humidityHigh ? 1 : 0;
  gAlerts.count += gAlerts.soundHigh ? 1 : 0;
  gAlerts.count += gAlerts.lightHigh ? 1 : 0;
  gAlerts.count += gAlerts.cpuHigh ? 1 : 0;
  gAlerts.count += gAlerts.heapLow ? 1 : 0;
  gAlerts.active = gAlerts.count > 0;

  if (!gAlerts.active) {
    snprintf(gAlerts.summary, sizeof(gAlerts.summary), "OK");
    return;
  }

  gAlerts.summary[0] = '\0';
  bool first = true;
  if (gAlerts.tempHigh) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "TEMP", &first);
  }
  if (gAlerts.humidityHigh) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "HUMI", &first);
  }
  if (gAlerts.soundHigh) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "SOUND", &first);
  }
  if (gAlerts.lightHigh) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "LIGHT", &first);
  }
  if (gAlerts.cpuHigh) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "CPU", &first);
  }
  if (gAlerts.heapLow) {
    appendAlertName(gAlerts.summary, sizeof(gAlerts.summary), "HEAP", &first);
  }
}

bool isIdleTaskName(const char *name) {
  return name != nullptr && strncmp(name, "IDLE", 4) == 0;
}

uint32_t counterDelta(uint32_t current, uint32_t previous) {
  return current >= previous ? (current - previous) : (UINT32_MAX - previous + current + 1UL);
}

bool idleHookCore0() {
  gIdleHookCounts[0]++;
  return false;
}

bool idleHookCore1() {
  gIdleHookCounts[1]++;
  return false;
}

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_SW:
      return "SW";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
      return "INT_WDT";
    case ESP_RST_TASK_WDT:
      return "TASK_WDT";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_DEEPSLEEP:
      return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT";
    case ESP_RST_SDIO:
      return "SDIO";
    default:
      return "UNKNOWN";
  }
}

const char *displayPageName(uint8_t page) {
  switch (page % 4) {
    case 0:
      return "CLIMATE";
    case 1:
      return "LIGHT";
    case 2:
      return "SYSTEM";
    default:
      return "STORAGE";
  }
}

const speechassets::Clip *digitClip(uint8_t digit) {
  switch (digit) {
    case 0:
      return &speechassets::kZero;
    case 1:
      return &speechassets::kOne;
    case 2:
      return &speechassets::kTwo;
    case 3:
      return &speechassets::kThree;
    case 4:
      return &speechassets::kFour;
    case 5:
      return &speechassets::kFive;
    case 6:
      return &speechassets::kSix;
    case 7:
      return &speechassets::kSeven;
    case 8:
      return &speechassets::kEight;
    default:
      return &speechassets::kNine;
  }
}

bool appendClip(const speechassets::Clip *clip, const speechassets::Clip **sequence, size_t maxCount, size_t *count) {
  if (!clip || !sequence || !count || *count >= maxCount) {
    return false;
  }
  sequence[*count] = clip;
  (*count)++;
  return true;
}

bool appendChineseIntegerClips(int value, const speechassets::Clip **sequence, size_t maxCount, size_t *count) {
  if (value < 0) {
    value = 0;
  }
  if (value > 99) {
    value = 99;
  }

  if (value < 10) {
    return appendClip(digitClip(static_cast<uint8_t>(value)), sequence, maxCount, count);
  }

  if (value < 20) {
    if (!appendClip(&speechassets::kTen, sequence, maxCount, count)) {
      return false;
    }
    if ((value % 10) == 0) {
      return true;
    }
    return appendClip(digitClip(static_cast<uint8_t>(value % 10)), sequence, maxCount, count);
  }

  if (!appendClip(digitClip(static_cast<uint8_t>(value / 10)), sequence, maxCount, count) ||
      !appendClip(&speechassets::kTen, sequence, maxCount, count)) {
    return false;
  }
  if ((value % 10) == 0) {
    return true;
  }
  return appendClip(digitClip(static_cast<uint8_t>(value % 10)), sequence, maxCount, count);
}

const char *speakerVerificationState() {
  if (!gSpeaker.online) {
    return "OFFLINE";
  }
  if (gSpeaker.testRunning) {
    return "RUNNING";
  }
  if (gSpeaker.lastTestMs == 0) {
    return "PENDING";
  }
  return gSpeaker.loopbackPassed ? "PASS" : "FAIL";
}

void refreshDisplayRecoveryIoState() {
  const uint8_t powerMask = static_cast<uint8_t>(SLCD_PWR >> 8);
  const uint8_t resetMask = static_cast<uint8_t>(SLCD_RST >> 8);
  gDisplayRecovery.outputPort1 = xl9555_read_reg(XL9555_OUTPUT_PORT1_REG);
  gDisplayRecovery.configPort1 = xl9555_read_reg(XL9555_CONFIG_PORT1_REG);
  gDisplayRecovery.powerHigh = (gDisplayRecovery.outputPort1 & powerMask) != 0;
  gDisplayRecovery.resetHigh = (gDisplayRecovery.outputPort1 & resetMask) != 0;
  gDisplayRecovery.pinsOutput = ((gDisplayRecovery.configPort1 & (powerMask | resetMask)) == 0);
}

void prepareDisplayForRestart() {
  xl9555_init();
  xl9555_io_config(SLCD_PWR, IO_SET_OUTPUT);
  xl9555_io_config(SLCD_RST, IO_SET_OUTPUT);
  xl9555_pin_set(SLCD_RST, IO_SET_LOW);
  xl9555_pin_set(SLCD_PWR, IO_SET_LOW);
  delay(120);
  gDisplayReady = false;
  gDisplayRecovery.prepareRestartCount++;
  snprintf(gDisplayRecovery.lastAction, sizeof(gDisplayRecovery.lastAction), "restart");
  refreshDisplayRecoveryIoState();
}

void lcdPrintLine(uint16_t y, lcd_font_t font, uint16_t color, const char *fmt, ...) {
  if (!gDisplayReady) {
    return;
  }

  char line[80];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof(line), fmt, args);
  va_end(args);

  lcd_show_string(6, y, spilcd_width - 12, 24, font, line, color);
}

void drawDisplayVisibleTest(const char *label, unsigned long holdMs = 30000UL) {
  if (!gDisplayReady) {
    return;
  }
  const uint16_t bandHeight = spilcd_height / 6;
  const uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, CYAN, WHITE};
  for (uint8_t i = 0; i < 6; i++) {
    const uint16_t y0 = i * bandHeight;
    const uint16_t y1 = (i == 5) ? (spilcd_height - 1) : ((i + 1) * bandHeight - 1);
    lcd_fill(0, y0, spilcd_width - 1, y1, colors[i]);
  }
  g_back_color = BLACK;
  lcdPrintLine(8, LCD_FONT_16, WHITE, "LCD PIXEL TEST");
  lcdPrintLine(32, LCD_FONT_12, WHITE, "%s #%lu",
               label,
               static_cast<unsigned long>(gDisplayRecovery.initCount));
  lcdPrintLine(52, LCD_FONT_12, WHITE, "If black, SPI/panel init failed");
  gDisplayRecovery.visibleTestCount++;
  gDisplayRecovery.lastVisibleTestMs = millis();
  gDisplayVisibleTestUntilMs = millis() + holdMs;
}

void initDisplay(bool manualReinit = false) {
  const unsigned long startedAt = millis();
  gDisplayReady = false;
  xl9555_init();
  lcd_init();
  lcd_display_dir(1);
  lcd_display_on();
  g_back_color = BLACK;
  lcd_clear(BLACK);
  gDisplayReady = true;
  gDisplayRecovery.initCount++;
  if (manualReinit) {
    gDisplayRecovery.reinitCount++;
  }
  gDisplayRecovery.lastInitMs = millis() - startedAt;
  gDisplayRecovery.lastPowerCycleMs = gDisplayRecovery.lastInitMs;
  gDisplayRecovery.lastReinitMs = manualReinit ? gDisplayRecovery.lastInitMs : gDisplayRecovery.lastReinitMs;
  snprintf(gDisplayRecovery.lastAction, sizeof(gDisplayRecovery.lastAction), "%s", manualReinit ? "manual" : "boot");
  refreshDisplayRecoveryIoState();
  gSystem.displayPage = 2;
  snprintf(gSystem.displayPageName, sizeof(gSystem.displayPageName), "BOOT");
  drawDisplayVisibleTest(manualReinit ? "MANUAL REINIT" : "BOOT", 8000UL);
  lcdPrintLine(6, LCD_FONT_16, CYAN, "ESP32 SENSOR HUB");
  lcdPrintLine(30, LCD_FONT_16, GREEN, "LCD READY");
  lcdPrintLine(54, LCD_FONT_12, WHITE, "IP %s", currentDashboardIp().c_str());
  lcdPrintLine(70, LCD_FONT_12, WHITE, "INIT %lu ms #%lu",
               static_cast<unsigned long>(gDisplayRecovery.lastInitMs),
               static_cast<unsigned long>(gDisplayRecovery.initCount));
  gBoot.displayInitMs = gDisplayRecovery.lastInitMs;
}

void initCpuTelemetry() {
  const bool core0Ok = esp_register_freertos_idle_hook_for_cpu(idleHookCore0, 0) == ESP_OK;
  const bool core1Ok = esp_register_freertos_idle_hook_for_cpu(idleHookCore1, 1) == ESP_OK;
  gIdleHooksReady = core0Ok || core1Ok;
  gSystem.topTaskAvailable = false;
  gSystem.topTaskCpuPct = 0.0f;
  gSystem.topTaskName[0] = '\0';
}

void updateSystemStatsIfDue() {
  static unsigned long lastRead = 0;
  static uint32_t previousIdleCounts[2] = {};
  if (millis() - lastRead < kSystemIntervalMs) {
    return;
  }
  lastRead = millis();

  gSystem.cpuFreqMhz = getCpuFrequencyMhz();
  gSystem.freeHeapBytes = ESP.getFreeHeap();
  gSystem.minFreeHeapBytes = ESP.getMinFreeHeap();
  gSystem.maxAllocHeapBytes = ESP.getMaxAllocHeap();
  gSystem.uptimeSec = millis() / 1000UL;
  gSystem.wifiRssiDbm = currentRssiDbm();
  gSystem.wifiTxPower = static_cast<int32_t>(WiFi.getTxPower());
  snprintf(gSystem.resetReason, sizeof(gSystem.resetReason), "%s", resetReasonName(esp_reset_reason()));
  snprintf(gSystem.displayPageName, sizeof(gSystem.displayPageName), "%s", displayPageName(gSystem.displayPage));

  esp_chip_info_t chipInfo = {};
  esp_chip_info(&chipInfo);
  gSystem.cores = static_cast<uint8_t>(chipInfo.cores);
  gSystem.chipRevision = static_cast<uint16_t>(chipInfo.revision);

  if (!gIdleHooksReady || gSystem.cores == 0) {
    gSystem.runtimeReady = false;
    return;
  }

  float idleRatioSum = 0.0f;
  uint8_t measuredCores = 0;
  for (uint8_t core = 0; core < gSystem.cores && core < 2; core++) {
    const uint32_t delta = counterDelta(gIdleHookCounts[core], previousIdleCounts[core]);
    previousIdleCounts[core] = gIdleHookCounts[core];
    if (delta == 0) {
      continue;
    }

    if (delta > gIdleMaxCounts[core]) {
      gIdleMaxCounts[core] = delta;
    }
    idleRatioSum += static_cast<float>(delta) / static_cast<float>(gIdleMaxCounts[core]);
    measuredCores++;
  }

  if (measuredCores == 0) {
    gSystem.runtimeReady = false;
    return;
  }

  const float idleRatio = constrain(idleRatioSum / static_cast<float>(measuredCores), 0.0f, 1.0f);
  gSystem.cpuUsagePct = (1.0f - idleRatio) * 100.0f;
  gSystem.topTaskAvailable = false;
  gSystem.topTaskCpuPct = 0.0f;
  gSystem.topTaskName[0] = '\0';
  gSystem.runtimeReady = true;
}

void updateDisplayIfDue() {
  static unsigned long lastDraw = 0;
  if (!gDisplayReady || millis() - lastDraw < kDisplayIntervalMs) {
    return;
  }
  if (gDisplayVisibleTestUntilMs != 0 && static_cast<long>(gDisplayVisibleTestUntilMs - millis()) > 0) {
    return;
  }
  gDisplayVisibleTestUntilMs = 0;
  lastDraw = millis();

  gSystem.displayPage = (gSystem.displayPage + 1U) % 4U;
  snprintf(gSystem.displayPageName, sizeof(gSystem.displayPageName), "%s", displayPageName(gSystem.displayPage));

  g_back_color = DARKBLUE;
  lcd_clear(DARKBLUE);
  lcd_fill(0, 0, spilcd_width - 1, 20, BLUE);
  lcdPrintLine(6, LCD_FONT_16, CYAN, "ESP32 SENSOR HUB");
  lcdPrintLine(26, LCD_FONT_12, WHITE, "IP %s", currentDashboardIp().c_str());
  lcdPrintLine(42, LCD_FONT_12, WHITE, "PAGE %u %s", gSystem.displayPage + 1, gSystem.displayPageName);
  if (gAlerts.active) {
    lcdPrintLine(56, LCD_FONT_12, RED, "ALERT %u %s", static_cast<unsigned int>(gAlerts.count), gAlerts.summary);
  }

  switch (gSystem.displayPage) {
    case 0:
      lcdPrintLine(70, LCD_FONT_16, YELLOW, "TEMP %.1f C", gDht.tempC);
      lcdPrintLine(92, LCD_FONT_16, YELLOW, "HUMI %.1f %%", gDht.humidity);
      lcdPrintLine(114, LCD_FONT_16, LIGHTBLUE, "CHIP %.2f C", gChipTemp.tempC);
      lcdPrintLine(136, LCD_FONT_16, MAGENTA, "MIC %.1f dBFS", gMic.dbfs);
      lcdPrintLine(158, LCD_FONT_12, WHITE, "DHT %s / CHIP %s", gDht.online ? "ON" : "OFF", gChipTemp.online ? "ON" : "OFF");
      break;
    case 1:
      lcdPrintLine(70, LCD_FONT_16, YELLOW, "ALS %u", gLight.als);
      lcdPrintLine(92, LCD_FONT_16, YELLOW, "IR %u  PS %u", gLight.ir, gLight.ps);
      lcdPrintLine(114, LCD_FONT_16, GREEN, "G %.2f", gAccel.ag);
      lcdPrintLine(136, LCD_FONT_16, GREEN, "P %.1f  R %.1f", gAccel.pitch, gAccel.roll);
      lcdPrintLine(158, LCD_FONT_12, WHITE, "LIGHT %s / QMA %s", gLight.online ? "ON" : "OFF", gAccel.online ? "ON" : "OFF");
      break;
    case 2:
      lcdPrintLine(70, LCD_FONT_16, RED, "CPU %.1f %%", gSystem.cpuUsagePct);
      lcdPrintLine(92, LCD_FONT_16, LIGHTGREEN, "HEAP %.1f KB", gSystem.freeHeapBytes / 1024.0f);
      lcdPrintLine(114, LCD_FONT_16, LIGHTGREEN, "MIN %.1f KB", gSystem.minFreeHeapBytes / 1024.0f);
      lcdPrintLine(136, LCD_FONT_12, WHITE, "SPK %s D%.1f", speakerVerificationState(), gSpeaker.deltaDbfs);
      lcdPrintLine(152, LCD_FONT_12, WHITE, "CPU %luMHz RSSI %ld", static_cast<unsigned long>(gSystem.cpuFreqMhz), static_cast<long>(gSystem.wifiRssiDbm));
      lcdPrintLine(168, LCD_FONT_12, WHITE, "UP %lus SAY %lu", static_cast<unsigned long>(gSystem.uptimeSec), static_cast<unsigned long>(gSpeaker.speakCount));
      break;
    default:
      lcdPrintLine(70, LCD_FONT_16, CYAN, "SAVE %lu", static_cast<unsigned long>(gPersistedSamples));
      lcdPrintLine(92, LCD_FONT_16, CYAN, "PEND %u", static_cast<unsigned int>(gPendingLog.length()));
      lcdPrintLine(114, LCD_FONT_16, RED, "WFAIL %lu", static_cast<unsigned long>(gStorageWriteFailures));
      lcdPrintLine(136, LCD_FONT_16, RED, "DROP %lu", static_cast<unsigned long>(gDroppedSamples));
      lcdPrintLine(158, LCD_FONT_12, WHITE, "FS %s", gLittleFsReady ? "READY" : "OFF");
      break;
  }
}

bool flushPendingLog();

bool trimPendingLog(size_t incomingBytes) {
  if (incomingBytes > kMaxPendingLogBytes) {
    gDroppedSamples++;
    return false;
  }

  if (gPendingLog.length() + incomingBytes <= kMaxPendingLogBytes) {
    return true;
  }

  flushPendingLog();
  while (gPendingLog.length() + incomingBytes > kMaxPendingLogBytes) {
    const int newlineIndex = gPendingLog.indexOf('\n');
    if (newlineIndex < 0) {
      gPendingLog = "";
      gDroppedSamples++;
      break;
    }
    gPendingLog.remove(0, newlineIndex + 1);
    gDroppedSamples++;
  }
  return gPendingLog.length() + incomingBytes <= kMaxPendingLogBytes;
}

void rotateLogIfNeeded() {
  if (!gLittleFsReady || !LittleFS.exists(kLogPath)) {
    return;
  }

  File logFile = LittleFS.open(kLogPath, "r");
  if (!logFile) {
    return;
  }
  const size_t size = logFile.size();
  logFile.close();

  if (size < kMaxLogFileBytes) {
    return;
  }

  LittleFS.remove(kRotatedLogPath);
  LittleFS.rename(kLogPath, kRotatedLogPath);
}

void appendLog(const SampleRow &row) {
  char line[420];
  snprintf(line,
           sizeof(line),
           "%lu,%lu,%d,%.2f,%.2f,%d,%.2f,%d,%u,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%d,%.2f,%.5f,%d,%.2f,%lu,%lu,%lu,%u,%u,%u\n",
           static_cast<unsigned long>(row.seq),
           static_cast<unsigned long>(row.epoch),
           row.dhtOnline ? 1 : 0,
           row.dhtTempC,
           row.dhtHumidity,
           row.chipOnline ? 1 : 0,
           row.chipTempC,
           row.lightOnline ? 1 : 0,
           row.als,
           row.ir,
           row.ps,
           row.accelOnline ? 1 : 0,
           row.ax,
           row.ay,
           row.az,
           row.ag,
           row.pitch,
           row.roll,
           row.micOnline ? 1 : 0,
           row.micDbfs,
           row.micRms,
           row.micPeak,
           row.cpuUsagePct,
           static_cast<unsigned long>(row.freeHeapBytes),
           static_cast<unsigned long>(row.minFreeHeapBytes),
           static_cast<unsigned long>(row.maxAllocHeapBytes),
           static_cast<unsigned int>(row.cpuFreqMhz),
           static_cast<unsigned int>(row.displayPage),
           static_cast<unsigned int>(row.alertCount));
  if (trimPendingLog(strlen(line))) {
    gPendingLog += line;
  }
}

bool flushPendingLog() {
  const uint32_t failuresBefore = gStorageWriteFailures;
  if (!gLittleFsReady || gPendingLog.isEmpty()) {
    return gLittleFsReady && gPendingLog.isEmpty();
  }

  rotateLogIfNeeded();
  File logFile = LittleFS.open(kLogPath, FILE_APPEND);
  if (!logFile) {
    gStorageWriteFailures++;
    return false;
  }

  size_t consumed = 0;
  uint32_t appended = 0;
  uint32_t lastPersistedEpoch = gPersistedLastSampleEpoch;
  while (consumed < gPendingLog.length()) {
    const int newlineIndex = gPendingLog.indexOf('\n', consumed);
    if (newlineIndex < 0) {
      break;
    }

    const String line = gPendingLog.substring(consumed, newlineIndex + 1);
    const size_t written = logFile.write(reinterpret_cast<const uint8_t *>(line.c_str()), line.length());
    if (written != line.length()) {
      gStorageWriteFailures++;
      break;
    }
    const int firstComma = line.indexOf(',');
    const int secondComma = line.indexOf(',', firstComma + 1);
    if (firstComma >= 0 && secondComma > firstComma) {
      lastPersistedEpoch = static_cast<uint32_t>(line.substring(firstComma + 1, secondComma).toInt());
    }
    consumed = newlineIndex + 1;
    appended++;
  }
  logFile.flush();
  logFile.close();

  if (consumed > 0) {
    gPersistedSamples += appended;
    gPersistedLastSampleEpoch = lastPersistedEpoch;
    gPendingLog.remove(0, consumed);
  }
  return gPendingLog.isEmpty() && gStorageWriteFailures == failuresBefore;
}

bool parseLogLine(const char *line, SampleRow *row) {
  unsigned long seq = 0;
  unsigned long epoch = 0;
  int dhtOnline = 0;
  int chipOnline = 0;
  int lightOnline = 0;
  int accelOnline = 0;
  int micOnline = 0;
  int micPeak = 0;
  unsigned int als = 0;
  unsigned int ir = 0;
  unsigned int ps = 0;
  float dhtTemp = 0.0f;
  float dhtHumidity = 0.0f;
  float chipTemp = 0.0f;
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float ag = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float micDbfs = 0.0f;
  float micRms = 0.0f;
  float cpuUsagePct = 0.0f;
  unsigned long freeHeapBytes = 0;
  unsigned long minFreeHeapBytes = 0;
  unsigned long maxAllocHeapBytes = 0;
  unsigned int cpuFreqMhz = 0;
  unsigned int displayPage = 0;
  unsigned int alertCount = 0;

  int parsed = sscanf(line,
                      "%lu,%lu,%d,%f,%f,%d,%f,%d,%u,%u,%u,%d,%f,%f,%f,%f,%f,%f,%d,%f,%f,%d,%f,%lu,%lu,%lu,%u,%u,%u",
                      &seq,
                      &epoch,
                      &dhtOnline,
                      &dhtTemp,
                      &dhtHumidity,
                      &chipOnline,
                      &chipTemp,
                      &lightOnline,
                      &als,
                      &ir,
                      &ps,
                      &accelOnline,
                      &ax,
                      &ay,
                      &az,
                      &ag,
                      &pitch,
                      &roll,
                      &micOnline,
                      &micDbfs,
                      &micRms,
                      &micPeak,
                      &cpuUsagePct,
                      &freeHeapBytes,
                      &minFreeHeapBytes,
                      &maxAllocHeapBytes,
                      &cpuFreqMhz,
                      &displayPage,
                      &alertCount);
  if (parsed != 29) {
    parsed = sscanf(line,
                    "%lu,%lu,%d,%f,%f,%d,%f,%d,%u,%u,%u,%d,%f,%f,%f,%f,%f,%f,%d,%f,%f,%d,%f,%lu,%lu,%lu,%u,%u",
                    &seq,
                    &epoch,
                    &dhtOnline,
                    &dhtTemp,
                    &dhtHumidity,
                    &chipOnline,
                    &chipTemp,
                    &lightOnline,
                    &als,
                    &ir,
                    &ps,
                    &accelOnline,
                    &ax,
                    &ay,
                    &az,
                    &ag,
                    &pitch,
                    &roll,
                    &micOnline,
                    &micDbfs,
                    &micRms,
                    &micPeak,
                    &cpuUsagePct,
                    &freeHeapBytes,
                    &minFreeHeapBytes,
                    &maxAllocHeapBytes,
                    &cpuFreqMhz,
                    &displayPage);
  }
  if (parsed != 28 && parsed != 29) {
    parsed = sscanf(line,
                    "%lu,%lu,%d,%f,%f,%d,%f,%d,%u,%u,%u,%d,%f,%f,%f,%f,%f,%f,%d,%f,%f,%d",
                    &seq,
                    &epoch,
                    &dhtOnline,
                    &dhtTemp,
                    &dhtHumidity,
                    &chipOnline,
                    &chipTemp,
                    &lightOnline,
                    &als,
                    &ir,
                    &ps,
                    &accelOnline,
                    &ax,
                    &ay,
                    &az,
                    &ag,
                    &pitch,
                    &roll,
                    &micOnline,
                    &micDbfs,
                    &micRms,
                    &micPeak);
  }
  if (parsed != 22 && parsed != 28 && parsed != 29) {
    return false;
  }

  row->seq = static_cast<uint32_t>(seq);
  row->epoch = static_cast<uint32_t>(epoch);
  row->dhtOnline = dhtOnline == 1;
  row->dhtTempC = dhtTemp;
  row->dhtHumidity = dhtHumidity;
  row->chipOnline = chipOnline == 1;
  row->chipTempC = chipTemp;
  row->lightOnline = lightOnline == 1;
  row->als = static_cast<uint16_t>(als);
  row->ir = static_cast<uint16_t>(ir);
  row->ps = static_cast<uint16_t>(ps);
  row->accelOnline = accelOnline == 1;
  row->ax = ax;
  row->ay = ay;
  row->az = az;
  row->ag = ag;
  row->pitch = pitch;
  row->roll = roll;
  row->micOnline = micOnline == 1;
  row->micDbfs = micDbfs;
  row->micRms = micRms;
  row->micPeak = static_cast<int16_t>(micPeak);
  row->cpuUsagePct = cpuUsagePct;
  row->freeHeapBytes = static_cast<uint32_t>(freeHeapBytes);
  row->minFreeHeapBytes = static_cast<uint32_t>(minFreeHeapBytes);
  row->maxAllocHeapBytes = static_cast<uint32_t>(maxAllocHeapBytes);
  row->cpuFreqMhz = static_cast<uint16_t>(cpuFreqMhz);
  row->displayPage = static_cast<uint8_t>(displayPage);
  row->alertCount = static_cast<uint8_t>(alertCount);
  return true;
}

bool isCandidateLogLine(const char *line, size_t length) {
  return length >= 10 && isdigit(static_cast<unsigned char>(line[0]));
}

uint32_t countLogRows(const char *path) {
  if (!LittleFS.exists(path)) {
    return 0;
  }

  File logFile = LittleFS.open(path, "r");
  if (!logFile) {
    return 0;
  }

  uint8_t buffer[256];
  char firstChar = '\0';
  size_t lineLength = 0;
  uint32_t rows = 0;

  while (logFile.available()) {
    const size_t bytesRead = logFile.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      break;
    }

    for (size_t i = 0; i < bytesRead; i++) {
      const char ch = static_cast<char>(buffer[i]);
      if (ch == '\n') {
        if (lineLength >= 10 && isdigit(static_cast<unsigned char>(firstChar))) {
          rows++;
        }
        firstChar = '\0';
        lineLength = 0;
        continue;
      }
      if (ch == '\r') {
        continue;
      }
      if (lineLength == 0) {
        firstChar = ch;
      }
      lineLength++;
    }
  }

  if (lineLength >= 10 && isdigit(static_cast<unsigned char>(firstChar))) {
    rows++;
  }
  logFile.close();
  return rows;
}

void loadLogFileTail(const char *path, uint32_t skipRows, uint32_t *seenRows) {
  if (!LittleFS.exists(path)) {
    return;
  }

  File logFile = LittleFS.open(path, "r");
  if (!logFile) {
    return;
  }

  uint8_t buffer[256];
  char line[kMaxLogLineLength];
  size_t lineLength = 0;
  bool oversized = false;

  auto consumeLine = [&]() {
    if (lineLength == 0) {
      oversized = false;
      return;
    }

    const bool candidate = !oversized && isCandidateLogLine(line, lineLength);
    if (candidate) {
      const uint32_t index = *seenRows;
      (*seenRows)++;
      if (index >= skipRows) {
        line[lineLength] = '\0';
        SampleRow row;
        if (parseLogLine(line, &row)) {
          pushHistory(row);
          gBoot.historyRowsLoaded++;
          gPersistedLastSampleEpoch = row.epoch;
          if (row.seq > gSampleSeq) {
            gSampleSeq = row.seq;
          }
        }
      }
    }

    lineLength = 0;
    oversized = false;
  };

  while (logFile.available()) {
    const size_t bytesRead = logFile.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      break;
    }

    for (size_t i = 0; i < bytesRead; i++) {
      const char ch = static_cast<char>(buffer[i]);
      if (ch == '\n') {
        consumeLine();
        continue;
      }
      if (ch == '\r') {
        continue;
      }
      if (lineLength + 1 < sizeof(line)) {
        line[lineLength++] = ch;
      } else {
        oversized = true;
      }
    }
  }

  consumeLine();
  logFile.close();
}

void loadHistoryFromLittleFs() {
  if (!gLittleFsReady) {
    return;
  }

  gHistoryHead = 0;
  gHistoryCount = 0;
  gPersistedSamples = 0;
  gPersistedLastSampleEpoch = 0;
  gSampleSeq = 0;

  const uint32_t rotatedRows = countLogRows(kRotatedLogPath);
  const uint32_t currentRows = countLogRows(kLogPath);
  const uint32_t totalRows = rotatedRows + currentRows;
  const uint32_t skipRows = totalRows > kHistoryCapacity ? totalRows - kHistoryCapacity : 0;
  uint32_t seenRows = 0;

  gPersistedSamples = totalRows;
  gBoot.historyRowsCounted = totalRows;
  gBoot.historyRowsLoaded = 0;
  loadLogFileTail(kRotatedLogPath, skipRows, &seenRows);
  loadLogFileTail(kLogPath, skipRows, &seenRows);
  if (gSampleSeq == 0 && totalRows > 0) {
    gSampleSeq = totalRows;
  }
}

void initLittleFs() {
  const unsigned long startedAt = millis();
  gLittleFsReady = LittleFS.begin(false);
  gLittleFsMountError = !gLittleFsReady;
  gBoot.littleFsMountMs = millis() - startedAt;
  if (gLittleFsReady) {
    const unsigned long historyStartedAt = millis();
    loadHistoryFromLittleFs();
    gBoot.historyLoadMs = millis() - historyStartedAt;
    const unsigned long configStartedAt = millis();
    loadConfigFromLittleFs();
    gBoot.configLoadMs = millis() - configStartedAt;
  }
}

void initChipTemperature() {
  temp_sensor_config_t config = TSENS_CONFIG_DEFAULT();
  if (temp_sensor_set_config(config) == ESP_OK && temp_sensor_start() == ESP_OK) {
    gChipTempReady = true;
  }
}

void initNetwork() {
  const unsigned long startedAt = millis();
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAP(kApSsid);
  if (strlen(kStaSsid) > 0) {
    WiFi.begin(kStaSsid, kStaPass);
  }
  configTime(8 * 3600, 0, "ntp.ntsc.ac.cn", "ntp1.aliyun.com", "time.windows.com");
  gBoot.networkStartMs = millis() - startedAt;
}

void readDhtIfDue() {
  static unsigned long lastRead = 0;
  static bool firstRead = true;
  if (millis() < kDhtStartupDelayMs || (!firstRead && millis() - lastRead < kDhtIntervalMs)) {
    return;
  }
  firstRead = false;
  lastRead = millis();

  uint8_t temp = 0;
  uint8_t hum = 0;
  if (dht11_read_data(&temp, &hum) == 0) {
    gDht.online = true;
    gDht.tempC = temp;
    gDht.humidity = hum;
    gDht.success++;
    gDht.lastUpdateMs = millis();
  } else {
    gDht.online = false;
    gDht.failure++;
    gDht.lastUpdateMs = millis();
  }
}

void readChipTempIfDue() {
  static unsigned long lastRead = 0;
  static bool firstRead = true;
  if (!gChipTempReady || (!firstRead && millis() - lastRead < kChipTempIntervalMs)) {
    return;
  }
  firstRead = false;
  lastRead = millis();

  float temp = 0.0f;
  if (temp_sensor_read_celsius(&temp) == ESP_OK) {
    gChipTemp.online = true;
    gChipTemp.tempC = temp;
    gChipTemp.lastUpdateMs = millis();
  } else {
    gChipTemp.online = false;
  }
}

void readAp3216IfDue() {
  static unsigned long lastRead = 0;
  static bool firstRead = true;
  if (!gAp3216Ready || (!firstRead && millis() - lastRead < kLightIntervalMs)) {
    return;
  }
  firstRead = false;
  lastRead = millis();
  ap3216c::read(&gLight);
}

void readQmaIfDue() {
  static unsigned long lastRead = 0;
  static bool firstRead = true;
  if (!gQmaReady || (!firstRead && millis() - lastRead < kAccelIntervalMs)) {
    return;
  }
  firstRead = false;
  lastRead = millis();
  qma6100p::read(&gAccel);
}

void readMicIfDue() {
  static unsigned long lastRead = 0;
  static bool firstRead = true;
  if (!gMicReady || (!firstRead && millis() - lastRead < kMicIntervalMs)) {
    return;
  }
  firstRead = false;
  lastRead = millis();
  if (!es8388codec::readMicLevel(&gMic)) {
    gMic.online = false;
  }
}

bool speakTemperatureOnBoard() {
  if (!gMicReady || !gDht.online) {
    return false;
  }

  const speechassets::Clip *sequence[6] = {};
  size_t count = 0;
  if (!appendClip(&speechassets::kPrefix, sequence, 6, &count) ||
      !appendChineseIntegerClips(static_cast<int>(lroundf(gDht.tempC)), sequence, 6, &count) ||
      !appendClip(&speechassets::kDegree, sequence, 6, &count)) {
    return false;
  }

  gSpeaker.speaking = true;
  bool ok = true;
  for (size_t i = 0; i < count; i++) {
    if (!es8388codec::playMonoPcm(sequence[i]->samples, sequence[i]->sampleCount, gSpeakerVolume)) {
      ok = false;
      break;
    }
    delay(30);
  }
  gSpeaker.speaking = false;

  if (ok) {
    gSpeaker.speakCount++;
    gSpeaker.lastSpokenTempC = static_cast<int16_t>(lroundf(gDht.tempC));
    gSpeaker.hasSpokenTemp = true;
  }
  return ok;
}

void runSpeakerActionsIfDue() {
  gSpeaker.online = gMicReady && es8388codec::isReady();
  if (!gSpeaker.online || gSpeaker.testRunning || gSpeaker.speaking) {
    return;
  }

  if (gSpeakerTestRequested || (gSpeaker.lastTestMs == 0 && millis() >= kSpeakerSelfTestDelayMs)) {
    gSpeaker.testRunning = true;
    SpeakerLoopbackReading reading;
    const bool ok = es8388codec::playToneAndMeasure(880, 650, &reading, gSpeakerVolume);
    gSpeaker.mutedDbfs = reading.mutedDbfs;
    gSpeaker.enabledDbfs = reading.enabledDbfs;
    gSpeaker.deltaDbfs = reading.deltaDbfs;
    gSpeaker.mutedPeak = reading.mutedPeak;
    gSpeaker.enabledPeak = reading.enabledPeak;
    gSpeaker.loopbackPassed = ok && reading.detected;
    if (gSpeaker.loopbackPassed) {
      gSpeaker.passCount++;
    } else {
      gSpeaker.failCount++;
    }
    gSpeaker.lastTestMs = millis();
    gSpeaker.testRunning = false;
    gSpeakerTestRequested = false;
    return;
  }

  if (gSpeakTemperatureRequested) {
    speakTemperatureOnBoard();
    gSpeakTemperatureRequested = false;
  }
}

void publishSampleIfDue() {
  static unsigned long lastSample = 0;
  if (millis() - lastSample < kSampleIntervalMs) {
    return;
  }
  lastSample = millis();

  SampleRow row;
  row.seq = ++gSampleSeq;
  row.epoch = currentEpoch();
  row.dhtOnline = gDht.online;
  row.dhtTempC = gDht.tempC;
  row.dhtHumidity = gDht.humidity;
  row.chipOnline = gChipTemp.online;
  row.chipTempC = gChipTemp.tempC;
  row.lightOnline = gLight.online;
  row.als = gLight.als;
  row.ir = gLight.ir;
  row.ps = gLight.ps;
  row.accelOnline = gAccel.online;
  row.ax = gAccel.ax;
  row.ay = gAccel.ay;
  row.az = gAccel.az;
  row.ag = gAccel.ag;
  row.pitch = gAccel.pitch;
  row.roll = gAccel.roll;
  row.micOnline = gMic.online;
  row.micDbfs = gMic.dbfs;
  row.micRms = gMic.rms;
  row.micPeak = gMic.peak;
  row.cpuUsagePct = gSystem.cpuUsagePct;
  row.freeHeapBytes = gSystem.freeHeapBytes;
  row.minFreeHeapBytes = gSystem.minFreeHeapBytes;
  row.maxAllocHeapBytes = gSystem.maxAllocHeapBytes;
  row.cpuFreqMhz = static_cast<uint16_t>(gSystem.cpuFreqMhz);
  row.displayPage = gSystem.displayPage;
  row.alertCount = gAlerts.count;

  pushHistory(row);
  appendLog(row);
}

void flushIfDue() {
  static unsigned long lastFlush = 0;
  if (millis() - lastFlush < kFlushIntervalMs) {
    return;
  }
  lastFlush = millis();
  flushPendingLog();
}

void appendBootJson(String &json) {
  json += "\"boot\":{\"serial_ready_ms\":";
  json += String(gBoot.serialReadyMs);
  json += ",\"littlefs_mount_ms\":";
  json += String(gBoot.littleFsMountMs);
  json += ",\"history_load_ms\":";
  json += String(gBoot.historyLoadMs);
  json += ",\"history_rows_counted\":";
  json += String(gBoot.historyRowsCounted);
  json += ",\"history_rows_loaded\":";
  json += String(gBoot.historyRowsLoaded);
  json += ",\"config_load_ms\":";
  json += String(gBoot.configLoadMs);
  json += ",\"network_start_ms\":";
  json += String(gBoot.networkStartMs);
  json += ",\"display_init_ms\":";
  json += String(gBoot.displayInitMs);
  json += ",\"sensors_init_ms\":";
  json += String(gBoot.sensorsInitMs);
  json += ",\"server_start_ms\":";
  json += String(gBoot.serverStartMs);
  json += ",\"setup_complete_ms\":";
  json += String(gBoot.setupCompleteMs);
  json += ",\"first_url_report_ms\":";
  json += String(gBoot.firstUrlReportMs);
  json += "}";
}

void appendDisplayJson(String &json) {
  refreshDisplayRecoveryIoState();
  json += "\"display\":{\"online\":";
  json += gDisplayReady ? "true" : "false";
  json += ",\"page\":";
  json += String(gSystem.displayPage);
  json += ",\"page_name\":\"";
  json += gSystem.displayPageName;
  json += "\",\"init_count\":";
  json += String(gDisplayRecovery.initCount);
  json += ",\"reinit_count\":";
  json += String(gDisplayRecovery.reinitCount);
  json += ",\"visible_test_count\":";
  json += String(gDisplayRecovery.visibleTestCount);
  json += ",\"prepare_restart_count\":";
  json += String(gDisplayRecovery.prepareRestartCount);
  json += ",\"last_init_ms\":";
  json += String(gDisplayRecovery.lastInitMs);
  json += ",\"last_reinit_ms\":";
  json += String(gDisplayRecovery.lastReinitMs);
  json += ",\"last_visible_test_ms\":";
  json += String(gDisplayRecovery.lastVisibleTestMs);
  json += ",\"last_power_cycle_ms\":";
  json += String(gDisplayRecovery.lastPowerCycleMs);
  json += ",\"xl9555_output_port1\":";
  json += String(gDisplayRecovery.outputPort1);
  json += ",\"xl9555_config_port1\":";
  json += String(gDisplayRecovery.configPort1);
  json += ",\"power_high\":";
  json += gDisplayRecovery.powerHigh ? "true" : "false";
  json += ",\"reset_high\":";
  json += gDisplayRecovery.resetHigh ? "true" : "false";
  json += ",\"pins_output\":";
  json += gDisplayRecovery.pinsOutput ? "true" : "false";
  json += ",\"last_action\":\"";
  json += gDisplayRecovery.lastAction;
  json += "\"}";
}

void appendCameraJson(String &json) {
  const BoardCameraStatus &cam = boardcamera::status();
  json += "\"camera\":{\"online\":";
  json += cam.online ? "true" : "false";
  json += ",\"name\":\"";
  json += cam.name;
  json += "\",\"pid\":";
  json += String(cam.pid);
  json += ",\"init_error\":";
  json += String(cam.initError);
  json += ",\"psram\":";
  json += cam.psram ? "true" : "false";
  json += ",\"external_clock\":";
  json += cam.externalClock ? "true" : "false";
  json += ",\"frame_size\":";
  json += String(cam.frameSize);
  json += ",\"frame_size_name\":\"";
  json += boardcamera::frameSizeName(cam.frameSize);
  json += "\",\"quality\":";
  json += String(cam.quality);
  json += ",\"brightness\":";
  json += String(cam.brightness);
  json += ",\"contrast\":";
  json += String(cam.contrast);
  json += ",\"saturation\":";
  json += String(cam.saturation);
  json += ",\"sharpness\":";
  json += String(cam.sharpness);
  json += ",\"special_effect\":";
  json += String(cam.specialEffect);
  json += ",\"wb_mode\":";
  json += String(cam.wbMode);
  json += ",\"awb\":";
  json += String(cam.awb);
  json += ",\"awb_gain\":";
  json += String(cam.awbGain);
  json += ",\"aec\":";
  json += String(cam.aec);
  json += ",\"aec2\":";
  json += String(cam.aec2);
  json += ",\"ae_level\":";
  json += String(cam.aeLevel);
  json += ",\"aec_value\":";
  json += String(cam.aecValue);
  json += ",\"agc\":";
  json += String(cam.agc);
  json += ",\"agc_gain\":";
  json += String(cam.agcGain);
  json += ",\"gainceiling\":";
  json += String(cam.gainCeiling);
  json += ",\"hmirror\":";
  json += String(cam.hmirror);
  json += ",\"vflip\":";
  json += String(cam.vflip);
  json += ",\"colorbar\":";
  json += String(cam.colorbar);
  json += ",\"capture_count\":";
  json += String(cam.captureCount);
  json += ",\"capture_failures\":";
  json += String(cam.captureFailures);
  json += ",\"last_capture_ms\":";
  json += String(cam.lastCaptureMs);
  json += ",\"last_capture_duration_ms\":";
  json += String(cam.lastCaptureDurationMs);
  json += ",\"last_frame_bytes\":";
  json += String(cam.lastFrameBytes);
  json += ",\"last_width\":";
  json += String(cam.lastWidth);
  json += ",\"last_height\":";
  json += String(cam.lastHeight);
  json += ",\"stream_port\":";
  json += String(kCameraStreamPort);
  json += ",\"stream_target_fps\":";
  json += String(kCameraStreamTargetFps);
  json += ",\"stream_clients\":";
  json += String(static_cast<uint32_t>(gCameraStreamClients));
  json += ",\"stream_frame_count\":";
  json += String(static_cast<uint32_t>(gCameraStreamFrameCount));
  json += ",\"stream_fps\":";
  json += String(static_cast<float>(gCameraStreamLastFpsX100) / 100.0f, 2);
  json += ",\"stream_last_client_ms\":";
  json += String(static_cast<uint32_t>(gCameraStreamLastClientMs));
  json += "}";
}

String statusJson() {
  String json;
  json.reserve(2600);
  json += "{";
  json += "\"network\":{\"mode\":\"";
  json += currentNetworkMode();
  json += "\",\"ip\":\"";
  json += currentDashboardIp();
  json += "\",\"ap_ssid\":\"";
  json += kApSsid;
  json += "\",\"rssi_dbm\":";
  json += String(currentRssiDbm());
  json += "},";
  json += "\"storage\":{\"used_bytes\":";
  json += gLittleFsReady ? String(LittleFS.usedBytes()) : "0";
  json += ",\"total_bytes\":";
  json += gLittleFsReady ? String(LittleFS.totalBytes()) : "0";
  json += ",\"persisted_samples\":";
  json += String(gPersistedSamples);
  json += ",\"last_sample_epoch\":";
  json += String(gPersistedLastSampleEpoch);
  json += ",\"pending_bytes\":";
  json += String(gPendingLog.length());
  json += ",\"write_failures\":";
  json += String(gStorageWriteFailures);
  json += ",\"dropped_samples\":";
  json += String(gDroppedSamples);
  json += ",\"mount_ok\":";
  json += gLittleFsReady ? "true" : "false";
  json += "},";
  json += "\"config\":{\"persisted\":";
  json += gConfigPersisted ? "true" : "false";
  json += ",\"temp_high_c\":";
  json += String(gConfig.tempHighC, 2);
  json += ",\"humidity_high_pct\":";
  json += String(gConfig.humidityHighPct, 2);
  json += ",\"sound_high_dbfs\":";
  json += String(gConfig.soundHighDbfs, 2);
  json += ",\"light_high_als\":";
  json += String(gConfig.lightHighAls);
  json += ",\"cpu_high_pct\":";
  json += String(gConfig.cpuHighPct, 2);
  json += ",\"heap_low_bytes\":";
  json += String(gConfig.heapLowBytes);
  json += ",\"speaker_alerts\":";
  json += gConfig.speakerAlerts ? "true" : "false";
  json += "},";
  json += "\"alerts\":{\"active\":";
  json += gAlerts.active ? "true" : "false";
  json += ",\"count\":";
  json += String(gAlerts.count);
  json += ",\"summary\":\"";
  json += gAlerts.summary;
  json += "\",\"temp_high\":";
  json += gAlerts.tempHigh ? "true" : "false";
  json += ",\"humidity_high\":";
  json += gAlerts.humidityHigh ? "true" : "false";
  json += ",\"sound_high\":";
  json += gAlerts.soundHigh ? "true" : "false";
  json += ",\"light_high\":";
  json += gAlerts.lightHigh ? "true" : "false";
  json += ",\"cpu_high\":";
  json += gAlerts.cpuHigh ? "true" : "false";
  json += ",\"heap_low\":";
  json += gAlerts.heapLow ? "true" : "false";
  json += "},";
  json += "\"dht11\":{\"online\":";
  json += gDht.online ? "true" : "false";
  json += ",\"temp_c\":";
  json += String(gDht.tempC, 2);
  json += ",\"humidity\":";
  json += String(gDht.humidity, 2);
  json += ",\"success_count\":";
  json += String(gDht.success);
  json += ",\"failure_count\":";
  json += String(gDht.failure);
  json += "},";
  json += "\"chip_temp\":{\"online\":";
  json += gChipTemp.online ? "true" : "false";
  json += ",\"temp_c\":";
  json += String(gChipTemp.tempC, 2);
  json += "},";
  json += "\"ap3216c\":{\"online\":";
  json += gLight.online ? "true" : "false";
  json += ",\"als\":";
  json += String(gLight.als);
  json += ",\"ir\":";
  json += String(gLight.ir);
  json += ",\"ps\":";
  json += String(gLight.ps);
  json += ",\"mode\":";
  json += String(gAp3216Mode);
  json += "},";
  json += "\"qma6100p\":{\"online\":";
  json += gAccel.online ? "true" : "false";
  json += ",\"range_g\":";
  json += String(qma6100p::currentRangeG());
  json += ",\"ax\":";
  json += String(gAccel.ax, 3);
  json += ",\"ay\":";
  json += String(gAccel.ay, 3);
  json += ",\"az\":";
  json += String(gAccel.az, 3);
  json += ",\"ag\":";
  json += String(gAccel.ag, 3);
  json += ",\"pitch\":";
  json += String(gAccel.pitch, 2);
  json += ",\"roll\":";
  json += String(gAccel.roll, 2);
  json += "},";
  json += "\"mic\":{\"online\":";
  json += gMic.online ? "true" : "false";
  json += ",\"dbfs\":";
  json += String(gMic.dbfs, 2);
  json += ",\"rms\":";
  json += String(gMic.rms, 5);
  json += ",\"peak\":";
  json += String(gMic.peak);
  json += "},";
  json += "\"system\":{\"runtime_ready\":";
  json += gSystem.runtimeReady ? "true" : "false";
  json += ",\"cpu_usage_pct\":";
  json += String(gSystem.cpuUsagePct, 2);
  json += ",\"cpu_freq_mhz\":";
  json += String(gSystem.cpuFreqMhz);
  json += ",\"wifi_tx_power\":";
  json += String(gSystem.wifiTxPower);
  json += ",\"free_heap_bytes\":";
  json += String(gSystem.freeHeapBytes);
  json += ",\"min_free_heap_bytes\":";
  json += String(gSystem.minFreeHeapBytes);
  json += ",\"max_alloc_heap_bytes\":";
  json += String(gSystem.maxAllocHeapBytes);
  json += ",\"uptime_sec\":";
  json += String(gSystem.uptimeSec);
  json += ",\"cores\":";
  json += String(gSystem.cores);
  json += ",\"chip_revision\":";
  json += String(gSystem.chipRevision);
  json += ",\"top_task\":";
  if (gSystem.topTaskAvailable) {
    json += "\"";
    json += gSystem.topTaskName;
    json += "\"";
  } else {
    json += "null";
  }
  json += ",\"top_task_cpu_pct\":";
  if (gSystem.topTaskAvailable) {
    json += String(gSystem.topTaskCpuPct, 2);
  } else {
    json += "null";
  }
  json += ",\"reset_reason\":\"";
  json += gSystem.resetReason;
  json += "\"},";
  appendDisplayJson(json);
  json += ",";
  json += "\"speaker\":{\"online\":";
  json += gSpeaker.online ? "true" : "false";
  json += ",\"verification_state\":\"";
  json += speakerVerificationState();
  json += "\"";
  json += ",\"loopback_passed\":";
  json += gSpeaker.loopbackPassed ? "true" : "false";
  json += ",\"test_running\":";
  json += gSpeaker.testRunning ? "true" : "false";
  json += ",\"speak_running\":";
  json += gSpeaker.speaking ? "true" : "false";
  json += ",\"muted_dbfs\":";
  json += String(gSpeaker.mutedDbfs, 2);
  json += ",\"enabled_dbfs\":";
  json += String(gSpeaker.enabledDbfs, 2);
  json += ",\"delta_dbfs\":";
  json += String(gSpeaker.deltaDbfs, 2);
  json += ",\"muted_peak\":";
  json += String(gSpeaker.mutedPeak);
  json += ",\"enabled_peak\":";
  json += String(gSpeaker.enabledPeak);
  json += ",\"baseline_dbfs\":";
  json += String(gSpeaker.mutedDbfs, 2);
  json += ",\"observed_dbfs\":";
  json += String(gSpeaker.enabledDbfs, 2);
  json += ",\"observed_peak\":";
  json += String(gSpeaker.enabledPeak);
  json += ",\"pass_count\":";
  json += String(gSpeaker.passCount);
  json += ",\"fail_count\":";
  json += String(gSpeaker.failCount);
  json += ",\"speak_count\":";
  json += String(gSpeaker.speakCount);
  json += ",\"last_spoken_temp_c\":";
  json += String(gSpeaker.lastSpokenTempC);
  json += ",\"has_spoken_temp\":";
  json += gSpeaker.hasSpokenTemp ? "true" : "false";
  json += ",\"speaker_volume\":";
  json += String(gSpeakerVolume);
  json += ",\"headphone_volume\":";
  json += String(gHeadphoneVolume);
  json += ",\"last_test_ms\":";
  json += String(gSpeaker.lastTestMs);
  json += "}";
  json += ",";
  appendCameraJson(json);
  json += ",";
  appendBootJson(json);
  json += "}";
  return json;
}

String historyJson() {
  String json;
  json.reserve(12288);
  json += "{\"rows\":[";
  const size_t maxRows = gHistoryCount > 60 ? 60 : gHistoryCount;
  const size_t start = gHistoryCount > 60 ? gHistoryCount - 60 : 0;
  for (size_t i = 0; i < maxRows; i++) {
    const SampleRow &row = historyAt(start + i);
    if (i != 0) {
      json += ",";
    }
    json += "{";
    json += "\"seq\":";
    json += String(row.seq);
    json += ",\"epoch\":";
    json += String(row.epoch);
    json += ",\"dht_online\":";
    json += row.dhtOnline ? "true" : "false";
    json += ",\"dht_temp_c\":";
    json += String(row.dhtTempC, 2);
    json += ",\"dht_humidity\":";
    json += String(row.dhtHumidity, 2);
    json += ",\"chip_online\":";
    json += row.chipOnline ? "true" : "false";
    json += ",\"chip_temp_c\":";
    json += String(row.chipTempC, 2);
    json += ",\"light_online\":";
    json += row.lightOnline ? "true" : "false";
    json += ",\"als\":";
    json += String(row.als);
    json += ",\"ir\":";
    json += String(row.ir);
    json += ",\"ps\":";
    json += String(row.ps);
    json += ",\"accel_online\":";
    json += row.accelOnline ? "true" : "false";
    json += ",\"ax\":";
    json += String(row.ax, 3);
    json += ",\"ay\":";
    json += String(row.ay, 3);
    json += ",\"az\":";
    json += String(row.az, 3);
    json += ",\"ag\":";
    json += String(row.ag, 3);
    json += ",\"pitch\":";
    json += String(row.pitch, 2);
    json += ",\"roll\":";
    json += String(row.roll, 2);
    json += ",\"mic_online\":";
    json += row.micOnline ? "true" : "false";
    json += ",\"mic_dbfs\":";
    json += String(row.micDbfs, 2);
    json += ",\"mic_rms\":";
    json += String(row.micRms, 5);
    json += ",\"cpu_usage_pct\":";
    json += String(row.cpuUsagePct, 2);
    json += ",\"free_heap_bytes\":";
    json += String(row.freeHeapBytes);
    json += ",\"min_free_heap_bytes\":";
    json += String(row.minFreeHeapBytes);
    json += ",\"max_alloc_heap_bytes\":";
    json += String(row.maxAllocHeapBytes);
    json += ",\"cpu_freq_mhz\":";
    json += String(row.cpuFreqMhz);
    json += ",\"display_page\":";
    json += String(row.displayPage);
    json += ",\"alert_count\":";
    json += String(row.alertCount);
    json += "}";
  }
  json += "]}";
  return json;
}

String liveJson() {
  const unsigned long nowMs = millis();
  if (!gLiveJsonCache.isEmpty() && nowMs - gLiveJsonCacheMs < kLivePayloadCacheTtlMs) {
    return gLiveJsonCache;
  }

  String json;
  json.reserve(1900);
  gLiveJsonBuildCount++;
  json += "{\"network\":{\"mode\":\"";
  json += currentNetworkMode();
  json += "\",\"ip\":\"";
  json += currentDashboardIp();
  json += "\",\"rssi_dbm\":";
  json += String(currentRssiDbm());
  json += "},\"storage\":{\"persisted_samples\":";
  json += String(gPersistedSamples);
  json += ",\"last_sample_epoch\":";
  json += String(gPersistedLastSampleEpoch);
  json += ",\"pending_bytes\":";
  json += String(gPendingLog.length());
  json += ",\"write_failures\":";
  json += String(gStorageWriteFailures);
  json += ",\"dropped_samples\":";
  json += String(gDroppedSamples);
  json += ",\"mount_ok\":";
  json += gLittleFsReady ? "true" : "false";
  json += ",\"live_build_count\":";
  json += String(gLiveJsonBuildCount);
  json += ",\"live_cache_age_ms\":0";
  json += "},\"cadence\":{\"live_poll_ms\":";
  json += String(kDashboardLiveIntervalMs);
  json += ",\"snapshot_poll_ms\":";
  json += String(kDashboardSnapshotIntervalMs);
  json += ",\"live_cache_ttl_ms\":";
  json += String(kLivePayloadCacheTtlMs);
  json += ",\"sample_interval_ms\":";
  json += String(kSampleIntervalMs);
  json += ",\"flush_interval_ms\":";
  json += String(kFlushIntervalMs);
  json += "},\"alerts\":{\"active\":";
  json += gAlerts.active ? "true" : "false";
  json += ",\"count\":";
  json += String(gAlerts.count);
  json += ",\"summary\":\"";
  json += gAlerts.summary;
  json += "\"},\"dht11\":{\"online\":";
  json += gDht.online ? "true" : "false";
  json += ",\"temp_c\":";
  json += String(gDht.tempC, 2);
  json += ",\"humidity\":";
  json += String(gDht.humidity, 2);
  json += ",\"success_count\":";
  json += String(gDht.success);
  json += ",\"failure_count\":";
  json += String(gDht.failure);
  json += "},\"chip_temp\":{\"online\":";
  json += gChipTemp.online ? "true" : "false";
  json += ",\"temp_c\":";
  json += String(gChipTemp.tempC, 2);
  json += "},\"ap3216c\":{\"online\":";
  json += gLight.online ? "true" : "false";
  json += ",\"als\":";
  json += String(gLight.als);
  json += ",\"ir\":";
  json += String(gLight.ir);
  json += ",\"ps\":";
  json += String(gLight.ps);
  json += ",\"mode\":";
  json += String(gAp3216Mode);
  json += "},\"qma6100p\":{\"online\":";
  json += gAccel.online ? "true" : "false";
  json += ",\"range_g\":";
  json += String(qma6100p::currentRangeG());
  json += ",\"ax\":";
  json += String(gAccel.ax, 3);
  json += ",\"ay\":";
  json += String(gAccel.ay, 3);
  json += ",\"az\":";
  json += String(gAccel.az, 3);
  json += ",\"ag\":";
  json += String(gAccel.ag, 3);
  json += ",\"pitch\":";
  json += String(gAccel.pitch, 2);
  json += ",\"roll\":";
  json += String(gAccel.roll, 2);
  json += "},\"mic\":{\"online\":";
  json += gMic.online ? "true" : "false";
  json += ",\"dbfs\":";
  json += String(gMic.dbfs, 2);
  json += ",\"rms\":";
  json += String(gMic.rms, 5);
  json += ",\"peak\":";
  json += String(gMic.peak);
  json += "},\"system\":{\"runtime_ready\":";
  json += gSystem.runtimeReady ? "true" : "false";
  json += ",\"cpu_usage_pct\":";
  json += String(gSystem.cpuUsagePct, 2);
  json += ",\"cpu_freq_mhz\":";
  json += String(gSystem.cpuFreqMhz);
  json += ",\"wifi_tx_power\":";
  json += String(gSystem.wifiTxPower);
  json += ",\"free_heap_bytes\":";
  json += String(gSystem.freeHeapBytes);
  json += ",\"min_free_heap_bytes\":";
  json += String(gSystem.minFreeHeapBytes);
  json += ",\"max_alloc_heap_bytes\":";
  json += String(gSystem.maxAllocHeapBytes);
  json += ",\"uptime_sec\":";
  json += String(gSystem.uptimeSec);
  json += ",\"reset_reason\":\"";
  json += gSystem.resetReason;
  json += "\"},";
  appendDisplayJson(json);
  json += ",\"speaker\":{\"online\":";
  json += gSpeaker.online ? "true" : "false";
  json += ",\"verification_state\":\"";
  json += speakerVerificationState();
  json += "\",\"loopback_passed\":";
  json += gSpeaker.loopbackPassed ? "true" : "false";
  json += ",\"test_running\":";
  json += gSpeaker.testRunning ? "true" : "false";
  json += ",\"speak_running\":";
  json += gSpeaker.speaking ? "true" : "false";
  json += ",\"muted_dbfs\":";
  json += String(gSpeaker.mutedDbfs, 2);
  json += ",\"enabled_dbfs\":";
  json += String(gSpeaker.enabledDbfs, 2);
  json += ",\"delta_dbfs\":";
  json += String(gSpeaker.deltaDbfs, 2);
  json += ",\"speak_count\":";
  json += String(gSpeaker.speakCount);
  json += ",\"has_spoken_temp\":";
  json += gSpeaker.hasSpokenTemp ? "true" : "false";
  json += ",\"last_spoken_temp_c\":";
  json += String(gSpeaker.lastSpokenTempC);
  json += ",\"speaker_volume\":";
  json += String(gSpeakerVolume);
  json += ",\"headphone_volume\":";
  json += String(gHeadphoneVolume);
  json += "},";
  appendCameraJson(json);
  json += ",";
  appendBootJson(json);
  json += "}";
  gLiveJsonCache = json;
  gLiveJsonCacheMs = nowMs;
  return json;
}

String healthJson() {
  const bool storageOk = gLittleFsReady && gStorageWriteFailures == 0 && gDroppedSamples == 0;
  const bool sensorsOk = gDht.online && gLight.online && gAccel.online && gMic.online && gChipTemp.online && gDisplayReady;
  const bool speakerOk = gSpeaker.online && gSpeaker.loopbackPassed;
  const bool cameraOk = boardcamera::isReady();
  const bool ok = storageOk && sensorsOk && speakerOk && cameraOk && !gAlerts.active;
  const char *state = ok ? "OK" : (gAlerts.active ? "ALERT" : "DEGRADED");

  String json;
  json.reserve(760);
  json += "{\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"status\":\"";
  json += state;
  json += "\",\"ip\":\"";
  json += currentDashboardIp();
  json += "\",\"uptime_sec\":";
  json += String(gSystem.uptimeSec);
  json += ",\"free_heap_bytes\":";
  json += String(gSystem.freeHeapBytes);
  json += ",\"min_free_heap_bytes\":";
  json += String(gSystem.minFreeHeapBytes);
  json += ",\"persisted_samples\":";
  json += String(gPersistedSamples);
  json += ",\"last_sample_epoch\":";
  json += String(gPersistedLastSampleEpoch);
  json += ",\"storage_ok\":";
  json += storageOk ? "true" : "false";
  json += ",\"sensors_ok\":";
  json += sensorsOk ? "true" : "false";
  json += ",\"speaker_ok\":";
  json += speakerOk ? "true" : "false";
  json += ",\"camera_ok\":";
  json += cameraOk ? "true" : "false";
  json += ",\"alerts_active\":";
  json += gAlerts.active ? "true" : "false";
  json += ",\"alert_count\":";
  json += String(gAlerts.count);
  json += ",\"alert_summary\":\"";
  json += gAlerts.summary;
  json += "\",\"write_failures\":";
  json += String(gStorageWriteFailures);
  json += ",\"dropped_samples\":";
  json += String(gDroppedSamples);
  json += ",\"setup_complete_ms\":";
  json += String(gBoot.setupCompleteMs);
  json += ",\"history_load_ms\":";
  json += String(gBoot.historyLoadMs);
  json += "}";
  return json;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", kDashboardHtml);
}

void handleStatus() {
  server.send(200, "application/json; charset=utf-8", statusJson());
}

void handleHistory() {
  server.send(200, "application/json; charset=utf-8", historyJson());
}

void handleHealth() {
  server.send(200, "application/json; charset=utf-8", healthJson());
}

void handleLive() {
  server.send(200, "application/json; charset=utf-8", liveJson());
}

void handleSpeakerTest() {
  if (!gMicReady || gSpeaker.testRunning || gSpeaker.speaking) {
    server.send(409, "application/json; charset=utf-8", "{\"accepted\":false}");
    return;
  }
  gSpeakerTestRequested = true;
  server.send(202, "application/json; charset=utf-8", "{\"accepted\":true}");
}

void handleSpeakTemperature() {
  if (!gMicReady || !gDht.online || gSpeaker.testRunning || gSpeaker.speaking) {
    server.send(409, "application/json; charset=utf-8", "{\"accepted\":false}");
    return;
  }
  gSpeakTemperatureRequested = true;
  server.send(202, "application/json; charset=utf-8", "{\"accepted\":true}");
}

void handleConfig() {
  if (server.method() == HTTP_POST) {
    HubConfig nextConfig = gConfig;
    float parsedFloat = 0.0f;
    long parsedLong = 0;
    if (server.hasArg("temp_high_c")) {
      if (!parseFloatStrict(server.arg("temp_high_c"), -20.0f, 80.0f, &parsedFloat)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid temp_high_c\"}");
        return;
      }
      nextConfig.tempHighC = parsedFloat;
    }
    if (server.hasArg("humidity_high_pct")) {
      if (!parseFloatStrict(server.arg("humidity_high_pct"), 0.0f, 100.0f, &parsedFloat)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid humidity_high_pct\"}");
        return;
      }
      nextConfig.humidityHighPct = parsedFloat;
    }
    if (server.hasArg("sound_high_dbfs")) {
      if (!parseFloatStrict(server.arg("sound_high_dbfs"), -120.0f, 0.0f, &parsedFloat)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid sound_high_dbfs\"}");
        return;
      }
      nextConfig.soundHighDbfs = parsedFloat;
    }
    if (server.hasArg("light_high_als")) {
      if (!parseLongStrict(server.arg("light_high_als"), 1, 65535, &parsedLong)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid light_high_als\"}");
        return;
      }
      nextConfig.lightHighAls = static_cast<uint16_t>(parsedLong);
    }
    if (server.hasArg("cpu_high_pct")) {
      if (!parseFloatStrict(server.arg("cpu_high_pct"), 0.0f, 100.0f, &parsedFloat)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid cpu_high_pct\"}");
        return;
      }
      nextConfig.cpuHighPct = parsedFloat;
    }
    if (server.hasArg("heap_low_bytes")) {
      if (!parseLongStrict(server.arg("heap_low_bytes"), 8192, 320000, &parsedLong)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid heap_low_bytes\"}");
        return;
      }
      nextConfig.heapLowBytes = static_cast<uint32_t>(parsedLong);
    }
    if (server.hasArg("speaker_alerts")) {
      if (!parseLongStrict(server.arg("speaker_alerts"), 0, 1, &parsedLong)) {
        server.send(400, "application/json; charset=utf-8", "{\"saved\":false,\"error\":\"invalid speaker_alerts\"}");
        return;
      }
      nextConfig.speakerAlerts = parsedLong != 0;
    }
    gConfig = nextConfig;
    updateAlerts();
    const bool saved = saveConfigToLittleFs();
    server.send(saved ? 200 : 500, "application/json; charset=utf-8", saved ? "{\"saved\":true}" : "{\"saved\":false}");
    return;
  }

  String json;
  json.reserve(260);
  json += "{\"persisted\":";
  json += gConfigPersisted ? "true" : "false";
  json += ",\"temp_high_c\":";
  json += String(gConfig.tempHighC, 2);
  json += ",\"humidity_high_pct\":";
  json += String(gConfig.humidityHighPct, 2);
  json += ",\"sound_high_dbfs\":";
  json += String(gConfig.soundHighDbfs, 2);
  json += ",\"light_high_als\":";
  json += String(gConfig.lightHighAls);
  json += ",\"cpu_high_pct\":";
  json += String(gConfig.cpuHighPct, 2);
  json += ",\"heap_low_bytes\":";
  json += String(gConfig.heapLowBytes);
  json += ",\"speaker_alerts\":";
  json += gConfig.speakerAlerts ? "true" : "false";
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

void handleCameraStatus() {
  String json;
  json.reserve(900);
  json += "{";
  appendCameraJson(json);
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

void handleCameraJpeg() {
  camera_fb_t *frame = boardcamera::capture();
  if (!frame) {
    server.send(503, "application/json; charset=utf-8", "{\"captured\":false}");
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Content-Length", String(frame->len));
  server.setContentLength(frame->len);
  server.send(200, "image/jpeg", "");
  WiFiClient client = server.client();
  client.write(frame->buf, frame->len);
  boardcamera::release(frame);
}

void streamCameraClient(WiFiClient &client) {
  client.setNoDelay(true);
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n");
  client.print("Cache-Control: no-store, no-cache, must-revalidate\r\n");
  client.print("Pragma: no-cache\r\n");
  client.print("Connection: close\r\n");
  client.print("Access-Control-Allow-Origin: *\r\n\r\n");

  gCameraStreamClients++;
  const uint32_t clientStartedAt = millis();
  uint32_t framesThisClient = 0;
  uint32_t fpsWindowStartedAt = millis();
  uint32_t fpsWindowFrames = 0;

  while (client.connected()) {
    const uint32_t frameStartedAt = millis();
    camera_fb_t *frame = boardcamera::capture();
    if (!frame) {
      vTaskDelay(pdMS_TO_TICKS(10));
      continue;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\nX-Frame-Number: %lu\r\n\r\n",
                  static_cast<unsigned int>(frame->len),
                  static_cast<unsigned long>(gCameraStreamFrameCount + 1));
    const size_t written = client.write(frame->buf, frame->len);
    client.print("\r\n");
    boardcamera::release(frame);

    if (written != frame->len) {
      break;
    }

    gCameraStreamFrameCount++;
    framesThisClient++;
    fpsWindowFrames++;

    const uint32_t nowMs = millis();
    const uint32_t fpsWindowMs = nowMs - fpsWindowStartedAt;
    if (fpsWindowMs >= 1000) {
      gCameraStreamLastFpsX100 = (fpsWindowFrames * 100000UL) / fpsWindowMs;
      fpsWindowFrames = 0;
      fpsWindowStartedAt = nowMs;
    }

    const uint32_t frameElapsedMs = millis() - frameStartedAt;
    if (frameElapsedMs < kCameraStreamFrameIntervalMs) {
      vTaskDelay(pdMS_TO_TICKS(kCameraStreamFrameIntervalMs - frameElapsedMs));
    } else {
      taskYIELD();
    }
  }

  const uint32_t clientElapsedMs = millis() - clientStartedAt;
  if (clientElapsedMs > 0 && framesThisClient > 0) {
    gCameraStreamLastFpsX100 = (framesThisClient * 100000UL) / clientElapsedMs;
  }
  gCameraStreamLastClientMs = clientElapsedMs;
  if (gCameraStreamClients > 0) {
    gCameraStreamClients--;
  }
}

void cameraStreamTask(void *parameter) {
  (void)parameter;
  for (;;) {
    WiFiClient client = cameraStreamServer.available();
    if (client) {
      streamCameraClient(client);
      client.stop();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool cameraControlValueAllowed(const String &name, int value, const char **error) {
  if (name == "framesize") {
    if (!boardcamera::isSupportedFrameSize(value)) {
      *error = "unsupported_framesize";
      return false;
    }
    return true;
  }

  struct Range {
    const char *name;
    int minValue;
    int maxValue;
  };
  static const Range ranges[] = {
      {"quality", 4, 63},
      {"brightness", -2, 2},
      {"contrast", -2, 2},
      {"saturation", -2, 2},
      {"sharpness", -2, 2},
      {"special_effect", 0, 6},
      {"wb_mode", 0, 4},
      {"awb", 0, 1},
      {"awb_gain", 0, 1},
      {"aec", 0, 1},
      {"aec2", 0, 1},
      {"ae_level", -2, 2},
      {"aec_value", 0, 1200},
      {"agc", 0, 1},
      {"agc_gain", 0, 30},
      {"gainceiling", 0, 6},
      {"hmirror", 0, 1},
      {"vflip", 0, 1},
      {"colorbar", 0, 1},
      {"dcw", 0, 1},
      {"bpc", 0, 1},
      {"wpc", 0, 1},
      {"raw_gma", 0, 1},
      {"lenc", 0, 1},
  };

  for (const Range &range : ranges) {
    if (name == range.name) {
      if (value < range.minValue || value > range.maxValue) {
        *error = "out_of_range";
        return false;
      }
      return true;
    }
  }

  *error = "unsupported_control";
  return false;
}

bool cameraEffectiveValue(const BoardCameraStatus &cam, const String &name, int *value) {
  if (!value) {
    return false;
  }
  if (name == "framesize") *value = cam.frameSize;
  else if (name == "quality") *value = cam.quality;
  else if (name == "brightness") *value = cam.brightness;
  else if (name == "contrast") *value = cam.contrast;
  else if (name == "saturation") *value = cam.saturation;
  else if (name == "sharpness") *value = cam.sharpness;
  else if (name == "special_effect") *value = cam.specialEffect;
  else if (name == "wb_mode") *value = cam.wbMode;
  else if (name == "awb") *value = cam.awb;
  else if (name == "awb_gain") *value = cam.awbGain;
  else if (name == "aec") *value = cam.aec;
  else if (name == "aec2") *value = cam.aec2;
  else if (name == "ae_level") *value = cam.aeLevel;
  else if (name == "aec_value") *value = cam.aecValue;
  else if (name == "agc") *value = cam.agc;
  else if (name == "agc_gain") *value = cam.agcGain;
  else if (name == "gainceiling") *value = cam.gainCeiling;
  else if (name == "hmirror") *value = cam.hmirror;
  else if (name == "vflip") *value = cam.vflip;
  else if (name == "colorbar") *value = cam.colorbar;
  else return false;
  return true;
}

void handleCameraPreset() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"POST required\"}");
    return;
  }
  if (!server.hasArg("preset")) {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"missing_arg\"}");
    return;
  }

  struct PresetControl {
    const char *name;
    int value;
  };

  static const PresetControl smooth[] = {
      {"framesize", FRAMESIZE_QQVGA},
      {"quality", 30},
      {"brightness", 0},
      {"contrast", 0},
      {"saturation", 0},
      {"aec", 1},
      {"agc", 1},
  };
  static const PresetControl clear[] = {
      {"framesize", FRAMESIZE_QVGA},
      {"quality", 18},
      {"brightness", 0},
      {"contrast", 1},
      {"saturation", 0},
      {"aec", 1},
      {"agc", 1},
  };
  static const PresetControl bright[] = {
      {"framesize", FRAMESIZE_QQVGA},
      {"quality", 25},
      {"brightness", 2},
      {"contrast", 0},
      {"saturation", 1},
      {"aec", 1},
      {"agc", 1},
  };

  const String preset = server.arg("preset");
  const PresetControl *controls = nullptr;
  size_t controlCount = 0;
  const char *label = "";
  if (preset == "smooth") {
    controls = smooth;
    controlCount = sizeof(smooth) / sizeof(smooth[0]);
    label = "流畅 20 帧";
  } else if (preset == "clear") {
    controls = clear;
    controlCount = sizeof(clear) / sizeof(clear[0]);
    label = "清晰画质";
  } else if (preset == "bright") {
    controls = bright;
    controlCount = sizeof(bright) / sizeof(bright[0]);
    label = "弱光增强";
  } else {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"unsupported_preset\"}");
    return;
  }

  size_t appliedCount = 0;
  for (size_t i = 0; i < controlCount; i++) {
    const String controlName = controls[i].name;
    const char *rangeError = "";
    if (!cameraControlValueAllowed(controlName, controls[i].value, &rangeError) ||
        !boardcamera::setControl(controlName, controls[i].value)) {
      String json;
      json.reserve(160);
      json += "{\"applied\":false,\"preset\":\"";
      json += preset;
      json += "\",\"error\":\"control_failed\",\"control\":\"";
      json += controlName;
      json += "\"}";
      server.send(400, "application/json; charset=utf-8", json);
      return;
    }
    appliedCount++;
  }

  const BoardCameraStatus &cam = boardcamera::status();
  size_t verifiedCount = 0;
  for (size_t i = 0; i < controlCount; i++) {
    int effective = 0;
    if (cameraEffectiveValue(cam, controls[i].name, &effective) && effective == controls[i].value) {
      verifiedCount++;
    }
  }

  String json;
  json.reserve(220);
  json += "{\"applied\":";
  json += appliedCount == controlCount ? "true" : "false";
  json += ",\"preset\":\"";
  json += preset;
  json += "\",\"label\":\"";
  json += label;
  json += "\",\"control_count\":";
  json += String(controlCount);
  json += ",\"verified_count\":";
  json += String(verifiedCount);
  json += ",\"verified\":";
  json += verifiedCount == controlCount ? "true" : "false";
  json += "}";
  server.send(verifiedCount == controlCount ? 200 : 207, "application/json; charset=utf-8", json);
}

void handleCameraControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"POST required\"}");
    return;
  }
  if (!server.hasArg("name") || !server.hasArg("value")) {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"missing_arg\"}");
    return;
  }

  const String requestedName = server.arg("name");
  String name = requestedName;
  int value = 0;
  if (name == "framesize_name") {
    value = boardcamera::frameSizeFromName(server.arg("value"));
    if (!boardcamera::isSupportedFrameSize(value)) {
      server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"unsupported_framesize\"}");
      return;
    }
    name = "framesize";
  } else {
    int32_t parsedValue = 0;
    if (!parseSignedText(server.arg("value"), -10000, 10000, &parsedValue)) {
      server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"invalid_value\"}");
      return;
    }
    value = static_cast<int>(parsedValue);
    if (name == "framesize" && !boardcamera::isSupportedFrameSize(value)) {
      server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"unsupported_framesize\"}");
      return;
    }
  }

  const char *rangeError = "";
  if (!cameraControlValueAllowed(name, value, &rangeError)) {
    String json;
    json.reserve(120);
    json += "{\"applied\":false,\"error\":\"";
    json += rangeError;
    json += "\"}";
    server.send(400, "application/json; charset=utf-8", json);
    return;
  }

  const bool ok = boardcamera::setControl(name, value);
  if (!ok) {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"unsupported_control\"}");
    return;
  }

  const BoardCameraStatus &cam = boardcamera::status();
  int effective = 0;
  const bool hasEffective = cameraEffectiveValue(cam, name, &effective);
  const bool verified = hasEffective && effective == value;
  String json;
  json.reserve(260);
  json += "{\"applied\":true,\"name\":\"";
  json += name;
  json += "\",\"requested_name\":\"";
  json += requestedName;
  json += "\",\"value\":";
  json += String(value);
  json += ",\"effective\":";
  json += String(hasEffective ? effective : -1);
  json += ",\"verified\":";
  json += verified ? "true" : "false";
  if (name == "framesize") {
    json += ",\"effective_name\":\"";
    json += boardcamera::frameSizeName(effective);
    json += "\"";
  }
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

enum class RegisterDevice : uint8_t {
  kInvalid,
  kAp3216c,
  kEs8388,
  kQma6100p,
  kXl9555,
  kOv5640,
};

RegisterDevice parseRegisterDevice(const String &device) {
  if (device == "ap3216c") return RegisterDevice::kAp3216c;
  if (device == "es8388") return RegisterDevice::kEs8388;
  if (device == "qma6100p") return RegisterDevice::kQma6100p;
  if (device == "xl9555") return RegisterDevice::kXl9555;
  if (device == "camera" || device == "ov5640") return RegisterDevice::kOv5640;
  return RegisterDevice::kInvalid;
}

const char *registerDeviceName(RegisterDevice device) {
  switch (device) {
    case RegisterDevice::kAp3216c:
      return "ap3216c";
    case RegisterDevice::kEs8388:
      return "es8388";
    case RegisterDevice::kQma6100p:
      return "qma6100p";
    case RegisterDevice::kXl9555:
      return "xl9555";
    case RegisterDevice::kOv5640:
      return "ov5640";
    default:
      return "invalid";
  }
}

bool registerAddressAllowed(RegisterDevice device, uint16_t reg) {
  if (device == RegisterDevice::kInvalid) {
    return false;
  }
  if (device == RegisterDevice::kXl9555) {
    return reg <= 7;
  }
  if (device == RegisterDevice::kOv5640) {
    return true;
  }
  return reg <= 0xFF;
}

bool registerWriteAllowed(RegisterDevice device, uint16_t reg) {
  if (!registerAddressAllowed(device, reg)) {
    return false;
  }
  if (device == RegisterDevice::kEs8388) {
    return reg >= 46 && reg <= 49;
  }
  return false;
}

bool registerRead(RegisterDevice device, uint16_t reg, uint8_t mask, int *value) {
  if (!registerAddressAllowed(device, reg)) {
    return false;
  }
  uint8_t byteValue = 0;
  switch (device) {
    case RegisterDevice::kAp3216c:
      if (!ap3216c::readRegister(static_cast<uint8_t>(reg), &byteValue)) return false;
      *value = byteValue & mask;
      return true;
    case RegisterDevice::kEs8388:
      if (!es8388codec::readRegister(static_cast<uint8_t>(reg), &byteValue)) return false;
      *value = byteValue & mask;
      return true;
    case RegisterDevice::kQma6100p:
      if (!qma6100p::readRegister(static_cast<uint8_t>(reg), &byteValue)) return false;
      *value = byteValue & mask;
      return true;
    case RegisterDevice::kXl9555:
      *value = xl9555_read_reg(static_cast<uint8_t>(reg)) & mask;
      return true;
    case RegisterDevice::kOv5640:
      return boardcamera::readRegister(reg, mask, value);
    default:
      return false;
  }
}

bool registerWrite(RegisterDevice device, uint16_t reg, uint8_t mask, uint8_t value) {
  if (!registerWriteAllowed(device, reg)) {
    return false;
  }
  uint8_t current = 0;
  if (device == RegisterDevice::kEs8388) {
    if (!es8388codec::readRegister(static_cast<uint8_t>(reg), &current)) return false;
    const bool ok = es8388codec::writeRegister(static_cast<uint8_t>(reg), static_cast<uint8_t>((current & ~mask) | (value & mask)));
    if (ok && mask == 255) {
      if (reg == 46 || reg == 47) gHeadphoneVolume = value;
      if (reg == 48 || reg == 49) gSpeakerVolume = value;
    }
    return ok;
  }
  return false;
}

void sendPeripheralControlResult(bool ok,
                                 const char *device,
                                 const char *name,
                                 uint32_t requested,
                                 int effective,
                                 const char *error = "") {
  String json;
  json.reserve(220);
  json += "{\"applied\":";
  json += ok ? "true" : "false";
  json += ",\"device\":\"";
  json += device;
  json += "\",\"name\":\"";
  json += name;
  json += "\",\"value\":";
  json += String(requested);
  json += ",\"effective\":";
  json += String(effective);
  json += ",\"verified\":";
  json += ok && static_cast<int>(requested) == effective ? "true" : "false";
  if (!ok && error && error[0]) {
    json += ",\"error\":\"";
    json += error;
    json += "\"";
  }
  json += "}";
  server.send(ok ? 200 : 400, "application/json; charset=utf-8", json);
}

void handlePeripheralControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"POST required\"}");
    return;
  }
  if (!server.hasArg("device") || !server.hasArg("name") || !server.hasArg("value")) {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"missing_arg\"}");
    return;
  }

  const String device = server.arg("device");
  const String name = server.arg("name");
  uint32_t requested = 0;
  if (!parseDecimalUnsignedArg("value", 255, &requested)) {
    server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"invalid_value\"}");
    return;
  }

  if (device == "ap3216c" && name == "mode") {
    if (requested > 3) {
      sendPeripheralControlResult(false, "ap3216c", "mode", requested, -1, "out_of_range");
      return;
    }
    const bool writeOk = ap3216c::writeRegister(0, static_cast<uint8_t>(requested));
    delay(20);
    uint8_t effective = 255;
    const bool readOk = ap3216c::readRegister(0, &effective);
    if (writeOk && readOk) {
      gAp3216Mode = effective;
    }
    sendPeripheralControlResult(writeOk && readOk && effective == requested, "ap3216c", "mode", requested, effective, "write_failed");
    return;
  }

  if (device == "qma6100p" && name == "range_g") {
    if (!(requested == 2 || requested == 4 || requested == 8 || requested == 16)) {
      sendPeripheralControlResult(false, "qma6100p", "range_g", requested, -1, "out_of_range");
      return;
    }
    const bool ok = qma6100p::setRangeG(static_cast<uint8_t>(requested));
    uint8_t reg = 0;
    qma6100p::readRegister(15, &reg);
    sendPeripheralControlResult(ok && qma6100p::currentRangeG() == requested, "qma6100p", "range_g", requested, qma6100p::currentRangeG(), "write_failed");
    return;
  }

  if (device == "es8388" && (name == "speaker_volume" || name == "headphone_volume" || name == "all_volume")) {
    if (requested > 33) {
      sendPeripheralControlResult(false, "es8388", name.c_str(), requested, -1, "out_of_range");
      return;
    }
    bool ok = true;
    if (name == "speaker_volume" || name == "all_volume") {
      ok = ok && es8388codec::writeRegister(48, static_cast<uint8_t>(requested));
      ok = ok && es8388codec::writeRegister(49, static_cast<uint8_t>(requested));
      if (ok) gSpeakerVolume = static_cast<uint8_t>(requested);
    }
    if (name == "headphone_volume" || name == "all_volume") {
      ok = ok && es8388codec::writeRegister(46, static_cast<uint8_t>(requested));
      ok = ok && es8388codec::writeRegister(47, static_cast<uint8_t>(requested));
      if (ok) gHeadphoneVolume = static_cast<uint8_t>(requested);
    }
    uint8_t left = 0;
    uint8_t right = 0;
    const uint8_t readRegLeft = (name == "headphone_volume") ? 46 : 48;
    const uint8_t readRegRight = (name == "headphone_volume") ? 47 : 49;
    ok = ok && es8388codec::readRegister(readRegLeft, &left) && es8388codec::readRegister(readRegRight, &right);
    const int effective = (left == right) ? left : -1;
    sendPeripheralControlResult(ok && effective == static_cast<int>(requested), "es8388", name.c_str(), requested, effective, "write_failed");
    return;
  }

  server.send(400, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"unsupported_control\"}");
}

void handleRegisterAccess() {
  if (server.method() != HTTP_GET && server.method() != HTTP_POST) {
    server.send(405, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"method_not_allowed\"}");
    return;
  }

  const RegisterDevice device = parseRegisterDevice(server.arg("device"));
  uint32_t regValue = 0;
  uint32_t maskValue = 255;
  if (device == RegisterDevice::kInvalid || !parseDecimalUnsignedArg("reg", 65535, &regValue)) {
    server.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"invalid_request\"}");
    return;
  }
  if (server.hasArg("mask") && !parseDecimalUnsignedArg("mask", 255, &maskValue)) {
    server.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"invalid_mask\"}");
    return;
  }

  const uint16_t reg = static_cast<uint16_t>(regValue);
  const uint8_t mask = static_cast<uint8_t>(maskValue);
  if (!registerAddressAllowed(device, reg)) {
    server.send(400, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"reg_out_of_range\"}");
    return;
  }

  int value = 0;
  bool ok = false;
  int statusCode = 400;
  const char *error = "";

  if (server.method() == HTTP_POST) {
    uint32_t parsedValue = 0;
    if (!parseDecimalUnsignedArg("value", 255, &parsedValue)) {
      error = "invalid_value";
    } else if (!registerWriteAllowed(device, reg)) {
      statusCode = 403;
      error = "write_blocked";
    } else if (device == RegisterDevice::kEs8388 && parsedValue > 33) {
      error = "value_out_of_range";
    } else {
      const uint8_t writeValue = static_cast<uint8_t>(parsedValue);
      ok = registerWrite(device, reg, mask, writeValue) && registerRead(device, reg, mask, &value);
      error = ok ? "" : "write_failed";
    }
  } else {
    ok = registerRead(device, reg, mask, &value);
    error = ok ? "" : "read_failed";
  }

  String json;
  json.reserve(220);
  json += "{\"ok\":";
  json += ok ? "true" : "false";
  json += ",\"device\":\"";
  json += registerDeviceName(device);
  json += "\",\"reg\":";
  json += String(reg);
  json += ",\"mask\":";
  json += String(mask);
  json += ",\"value\":";
  json += String(value);
  if (!ok) {
    json += ",\"error\":\"";
    json += error;
    json += "\"";
  }
  json += "}";
  server.send(ok ? 200 : statusCode, "application/json; charset=utf-8", json);
}

void handleSystemControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "application/json; charset=utf-8", "{\"applied\":false,\"error\":\"POST required\"}");
    return;
  }

  bool ok = false;
  if (server.hasArg("cpu_mhz")) {
    int32_t mhz = 0;
    if (!parseSignedText(server.arg("cpu_mhz"), 80, 240, &mhz)) {
      server.send(400, "application/json; charset=utf-8", "{\"applied\":false}");
      return;
    }
    if (mhz == 80 || mhz == 160 || mhz == 240) {
      ok = setCpuFrequencyMhz(static_cast<uint32_t>(mhz));
    }
  } else if (server.hasArg("wifi_tx_power")) {
    int32_t power = 0;
    if (!parseSignedText(server.arg("wifi_tx_power"), WIFI_POWER_MINUS_1dBm, WIFI_POWER_19_5dBm, &power)) {
      server.send(400, "application/json; charset=utf-8", "{\"applied\":false}");
      return;
    }
    if (power >= WIFI_POWER_MINUS_1dBm && power <= WIFI_POWER_19_5dBm) {
      ok = WiFi.setTxPower(static_cast<wifi_power_t>(power));
    }
  }
  server.send(ok ? 200 : 400, "application/json; charset=utf-8", ok ? "{\"applied\":true}" : "{\"applied\":false}");
}

void handleFlush() {
  const bool ok = flushPendingLog();
  String json;
  json.reserve(120);
  json += "{\"flushed\":";
  json += ok ? "true" : "false";
  json += ",\"pending_bytes\":";
  json += String(gPendingLog.length());
  json += ",\"write_failures\":";
  json += String(gStorageWriteFailures);
  json += "}";
  server.send(ok ? 200 : 500, "application/json; charset=utf-8", json);
}

void handleDisplayReinit() {
  initDisplay(true);
  gLiveJsonCache = "";
  gLiveJsonCacheMs = 0;
  const bool ok = gDisplayReady && gDisplayRecovery.powerHigh && gDisplayRecovery.resetHigh && gDisplayRecovery.pinsOutput;
  String json;
  json.reserve(420);
  json += "{\"applied\":";
  json += ok ? "true" : "false";
  json += ",";
  appendDisplayJson(json);
  json += "}";
  server.send(ok ? 200 : 500, "application/json; charset=utf-8", json);
}

void handleDisplayTest() {
  drawDisplayVisibleTest("HTTP TEST", 30000UL);
  gLiveJsonCache = "";
  gLiveJsonCacheMs = 0;
  String json;
  json.reserve(420);
  json += "{\"applied\":";
  json += gDisplayReady ? "true" : "false";
  json += ",";
  appendDisplayJson(json);
  json += "}";
  server.send(gDisplayReady ? 200 : 500, "application/json; charset=utf-8", json);
}

void handleReboot() {
  const bool ok = flushPendingLog();
  if (!ok) {
    String json;
    json.reserve(120);
    json += "{\"rebooting\":false,\"pending_bytes\":";
    json += String(gPendingLog.length());
    json += ",\"write_failures\":";
    json += String(gStorageWriteFailures);
    json += "}";
    server.send(500, "application/json; charset=utf-8", json);
    return;
  }
  server.send(202, "application/json; charset=utf-8", "{\"rebooting\":true,\"flushed\":true}");
  gRebootRequested = true;
}

void handleLogDownload() {
  if (!gLittleFsReady || (!LittleFS.exists(kRotatedLogPath) && !LittleFS.exists(kLogPath))) {
    server.send(404, "text/plain", "log not found");
    return;
  }
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv; charset=utf-8", "");
  const char *paths[] = {kRotatedLogPath, kLogPath};
  uint8_t buffer[256];

  for (size_t i = 0; i < 2; i++) {
    if (!LittleFS.exists(paths[i])) {
      continue;
    }
    File logFile = LittleFS.open(paths[i], "r");
    if (!logFile) {
      continue;
    }
    while (logFile.available()) {
      const size_t bytesRead = logFile.read(buffer, sizeof(buffer));
      if (bytesRead == 0) {
        break;
      }
      server.sendContent(reinterpret_cast<const char *>(buffer), bytesRead);
    }
    logFile.close();
  }
  server.sendContent("");
}

void setupServer() {
  const unsigned long startedAt = millis();
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/live", HTTP_GET, handleLive);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/camera", HTTP_GET, handleCameraStatus);
  server.on("/api/camera.jpg", HTTP_GET, handleCameraJpeg);
  server.on("/api/camera/control", HTTP_POST, handleCameraControl);
  server.on("/api/camera/preset", HTTP_POST, handleCameraPreset);
  server.on("/api/register", HTTP_ANY, handleRegisterAccess);
  server.on("/api/peripheral/control", HTTP_POST, handlePeripheralControl);
  server.on("/api/system/control", HTTP_POST, handleSystemControl);
  server.on("/api/speaker_test", HTTP_POST, handleSpeakerTest);
  server.on("/api/speak_temperature", HTTP_POST, handleSpeakTemperature);
  server.on("/api/config", HTTP_ANY, handleConfig);
  server.on("/api/flush", HTTP_POST, handleFlush);
  server.on("/api/display/reinit", HTTP_POST, handleDisplayReinit);
  server.on("/api/display/test", HTTP_POST, handleDisplayTest);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/log.csv", HTTP_GET, handleLogDownload);
  server.begin();
  cameraStreamServer.begin();
  if (!gCameraStreamTask) {
    xTaskCreatePinnedToCore(cameraStreamTask, "camera_stream", 6144, nullptr, 1, &gCameraStreamTask, 0);
  }
  gBoot.serverStartMs = millis() - startedAt;
}

void printStatusReport() {
  if (gBoot.firstUrlReportMs == 0) {
    gBoot.firstUrlReportMs = millis();
  }
  Serial.printf("[NET] mode=%s ip=%s ap=%s\n",
                currentNetworkMode().c_str(),
                currentDashboardIp().c_str(),
                kApSsid);
  Serial.printf("[URL] %s/api/status\n", currentDashboardIp().c_str());
  Serial.printf("[LIVE] dht=%s %.1fC %.1f%% | als=%u ir=%u ps=%u | mic=%s %.2fdBFS peak=%d | spk=%s off=%.1f on=%.1f d=%.1f say=%lu | qma=%s g=%.2f p=%.1f r=%.1f | chip=%s %.2fC | cpu=%.1f%% heap=%luKB | fs=%u/%u persisted=%lu pending=%u fail=%lu drop=%lu | lcd=%s p%u\n",
                gDht.online ? "on" : "off",
                gDht.tempC,
                gDht.humidity,
                gLight.als,
                gLight.ir,
                gLight.ps,
                gMic.online ? "on" : "off",
                gMic.dbfs,
                gMic.peak,
                speakerVerificationState(),
                gSpeaker.mutedDbfs,
                gSpeaker.enabledDbfs,
                gSpeaker.deltaDbfs,
                static_cast<unsigned long>(gSpeaker.speakCount),
                gAccel.online ? "on" : "off",
                gAccel.ag,
                gAccel.pitch,
                gAccel.roll,
                gChipTemp.online ? "on" : "off",
                gChipTemp.tempC,
                gSystem.cpuUsagePct,
                static_cast<unsigned long>(gSystem.freeHeapBytes / 1024UL),
                gLittleFsReady ? static_cast<unsigned int>(LittleFS.usedBytes()) : 0,
                gLittleFsReady ? static_cast<unsigned int>(LittleFS.totalBytes()) : 0,
                static_cast<unsigned long>(gPersistedSamples),
                static_cast<unsigned int>(gPendingLog.length()),
                static_cast<unsigned long>(gStorageWriteFailures),
                static_cast<unsigned long>(gDroppedSamples),
                gDisplayReady ? "on" : "off",
                static_cast<unsigned int>(gSystem.displayPage + 1));
  Serial.printf("[BOOT] serial=%lu littlefs=%lu history=%lu rows=%lu loaded=%lu config=%lu network=%lu display=%lu sensors=%lu server=%lu setup=%lu first_url=%lu\n",
                static_cast<unsigned long>(gBoot.serialReadyMs),
                static_cast<unsigned long>(gBoot.littleFsMountMs),
                static_cast<unsigned long>(gBoot.historyLoadMs),
                static_cast<unsigned long>(gBoot.historyRowsCounted),
                static_cast<unsigned long>(gBoot.historyRowsLoaded),
                static_cast<unsigned long>(gBoot.configLoadMs),
                static_cast<unsigned long>(gBoot.networkStartMs),
                static_cast<unsigned long>(gBoot.displayInitMs),
                static_cast<unsigned long>(gBoot.sensorsInitMs),
                static_cast<unsigned long>(gBoot.serverStartMs),
                static_cast<unsigned long>(gBoot.setupCompleteMs),
                static_cast<unsigned long>(gBoot.firstUrlReportMs));
}

void reportStatusIfDue() {
  static unsigned long lastReport = 0;
  if (millis() - lastReport < kSerialReportIntervalMs) {
    return;
  }
  lastReport = millis();
  printStatusReport();
}

}  // namespace

void setup() {
  pinMode(kStatusLedPin, OUTPUT);
  digitalWrite(kStatusLedPin, LOW);

  Serial.begin(115200);
  delay(100);
  gBoot.serialReadyMs = millis();
  Serial.println();
  Serial.println("ESP32-S3 Sensor Hub boot");
  Serial.println("Reference sources: DHT11 + AP3216C + QMA6100P + ES8388 from ALIENTEK examples");

  Wire.begin(kI2cSdaPin, kI2cSclPin, 400000);
  DHT11_MODE_IN;
  initChipTemperature();
  initLittleFs();
  initNetwork();
  initCpuTelemetry();
  initDisplay();

  const unsigned long sensorsStartedAt = millis();
  gAp3216Ready = ap3216c::begin();
  gQmaReady = qma6100p::begin();
  gMicReady = es8388codec::beginMic();
  gSpeaker.online = gMicReady && es8388codec::isReady();
  boardcamera::begin();
  gBoot.sensorsInitMs = millis() - sensorsStartedAt;

  setupServer();
  updateSystemStatsIfDue();
  readChipTempIfDue();
  readAp3216IfDue();
  readQmaIfDue();
  readMicIfDue();
  updateAlerts();
  updateDisplayIfDue();
  gBoot.setupCompleteMs = millis();
  printStatusReport();
}

void loop() {
  server.handleClient();
  if (gRebootRequested) {
    flushPendingLog();
    delay(150);
    prepareDisplayForRestart();
    delay(150);
    ESP.restart();
  }
  updateSystemStatsIfDue();
  readDhtIfDue();
  readChipTempIfDue();
  readAp3216IfDue();
  readQmaIfDue();
  readMicIfDue();
  updateAlerts();
  runSpeakerActionsIfDue();
  publishSampleIfDue();
  flushIfDue();
  updateDisplayIfDue();
  reportStatusIfDue();
  digitalWrite(kStatusLedPin, (millis() / 500) % 2);
}
