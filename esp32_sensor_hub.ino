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
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "ap3216c.h"
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
constexpr unsigned long kSampleIntervalMs = 5000;
constexpr unsigned long kFlushIntervalMs = 15000;
constexpr unsigned long kSerialReportIntervalMs = 10000;
constexpr size_t kHistoryCapacity = 120;
constexpr char kLogPath[] = "/sensor_log.csv";
constexpr char kRotatedLogPath[] = "/sensor_log.old.csv";
constexpr size_t kMaxLogFileBytes = 512 * 1024;
constexpr size_t kMaxPendingLogBytes = 24 * 1024;
constexpr size_t kRuntimeTrackCapacity = 32;

constexpr char kStaSsid[] = WIFI_STA_SSID;
constexpr char kStaPass[] = WIFI_STA_PASS;

WebServer server(80);

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
bool gLittleFsReady = false;
bool gLittleFsMountError = false;
bool gChipTempReady = false;
bool gDisplayReady = false;
bool gSpeakerTestRequested = false;
bool gSpeakTemperatureRequested = false;

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
      <a class="btn" href="/api/log.csv" target="_blank" rel="noreferrer">查看 CSV 日志</a>
    </div>

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
      rows: document.getElementById('rows'),
      speakTempBtn: document.getElementById('speakTempBtn'),
      speakerSelfTestBtn: document.getElementById('speakerSelfTestBtn'),
    };
    const charts = {
      climate: document.getElementById('climateChart').getContext('2d'),
      env: document.getElementById('envChart').getContext('2d'),
      motion: document.getElementById('motionChart').getContext('2d'),
    };
    let latestStatus = null;

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
        .then(() => setTimeout(loop, 1200))
        .catch(() => {
          statusEls.speakerState.textContent = 'board speaker request failed';
        });
    }

    statusEls.speakTempBtn.addEventListener('click', speakTemperature);
    statusEls.speakerSelfTestBtn.addEventListener('click', async () => {
      try {
        statusEls.speakerState.textContent = 'self test running...';
        await fetch('/api/speaker_test', { method: 'POST', cache: 'no-store' });
        setTimeout(loop, 1200);
      } catch (error) {
        statusEls.speakerState.textContent = 'self test trigger failed';
      }
    });

    async function refresh() {
      const [statusRes, historyRes] = await Promise.all([
        fetch('/api/status', { cache: 'no-store' }),
        fetch('/api/history', { cache: 'no-store' }),
      ]);
      const status = await statusRes.json();
      const history = await historyRes.json();
      latestStatus = status;

      statusEls.netState.textContent = `${status.network.mode} ${status.network.ip}`;
      statusEls.storageState.textContent = `LittleFS ${status.storage.used_bytes}/${status.storage.total_bytes} bytes`;
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
      statusEls.speakerState.textContent = `amp-off ${fmtNum(status.speaker.muted_dbfs)} / amp-on ${fmtNum(status.speaker.enabled_dbfs)} / delta ${fmtNum(status.speaker.delta_dbfs)} / say ${status.speaker.speak_count}${status.speaker.has_spoken_temp ? ` / last ${status.speaker.last_spoken_temp_c}C` : ''}`;
      statusEls.speakerState.className = `meta ${status.speaker.verification_state === 'PASS' ? 'ok' : status.speaker.verification_state === 'FAIL' ? 'bad' : ''}`;

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

    async function loop() {
      try {
        await refresh();
      } catch (error) {
        statusEls.netState.textContent = 'dashboard fetch failed';
      }
    }

    loop();
    setInterval(loop, 3000);
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

void initDisplay() {
  xl9555_init();
  lcd_init();
  lcd_display_dir(1);
  g_back_color = BLACK;
  lcd_clear(BLACK);
  gSystem.displayPage = 3;
  gDisplayReady = true;
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
  lastDraw = millis();

  gSystem.displayPage = (gSystem.displayPage + 1U) % 4U;
  snprintf(gSystem.displayPageName, sizeof(gSystem.displayPageName), "%s", displayPageName(gSystem.displayPage));

  lcd_clear(BLACK);
  lcdPrintLine(6, LCD_FONT_16, CYAN, "ESP32 SENSOR HUB");
  lcdPrintLine(26, LCD_FONT_12, WHITE, "IP %s", currentDashboardIp().c_str());
  lcdPrintLine(42, LCD_FONT_12, WHITE, "PAGE %u %s", gSystem.displayPage + 1, gSystem.displayPageName);

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

void flushPendingLog();

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
           "%lu,%lu,%d,%.2f,%.2f,%d,%.2f,%d,%u,%u,%u,%d,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%d,%.2f,%.5f,%d,%.2f,%lu,%lu,%lu,%u,%u\n",
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
           static_cast<unsigned int>(row.displayPage));
  if (trimPendingLog(strlen(line))) {
    gPendingLog += line;
  }
}

void flushPendingLog() {
  if (!gLittleFsReady || gPendingLog.isEmpty()) {
    return;
  }

  rotateLogIfNeeded();
  File logFile = LittleFS.open(kLogPath, FILE_APPEND);
  if (!logFile) {
    gStorageWriteFailures++;
    return;
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
}

bool parseLogLine(const String &line, SampleRow *row) {
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

  int parsed = sscanf(line.c_str(),
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
  if (parsed != 28) {
    parsed = sscanf(line.c_str(),
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
  if (parsed != 22 && parsed != 28) {
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
  return true;
}

void loadLogFile(const char *path) {
  if (!LittleFS.exists(path)) {
    return;
  }

  File logFile = LittleFS.open(path, "r");
  if (!logFile) {
    return;
  }

  while (logFile.available()) {
    const String line = logFile.readStringUntil('\n');
    if (line.length() < 10) {
      continue;
    }
    SampleRow row;
    if (parseLogLine(line, &row)) {
      pushHistory(row);
      gPersistedSamples++;
      gPersistedLastSampleEpoch = row.epoch;
      if (row.seq > gSampleSeq) {
        gSampleSeq = row.seq;
      }
    }
  }
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
  loadLogFile(kRotatedLogPath);
  loadLogFile(kLogPath);
}

void initLittleFs() {
  gLittleFsReady = LittleFS.begin(false);
  gLittleFsMountError = !gLittleFsReady;
  if (gLittleFsReady) {
    loadHistoryFromLittleFs();
  }
}

void initChipTemperature() {
  temp_sensor_config_t config = TSENS_CONFIG_DEFAULT();
  if (temp_sensor_set_config(config) == ESP_OK && temp_sensor_start() == ESP_OK) {
    gChipTempReady = true;
  }
}

void initNetwork() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.softAP(kApSsid);
  if (strlen(kStaSsid) > 0) {
    WiFi.begin(kStaSsid, kStaPass);
  }
  configTime(8 * 3600, 0, "ntp.ntsc.ac.cn", "ntp1.aliyun.com", "time.windows.com");
}

void readDhtIfDue() {
  static unsigned long lastRead = 0;
  if (millis() < kDhtStartupDelayMs || millis() - lastRead < kDhtIntervalMs) {
    return;
  }
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
  if (!gChipTempReady || millis() - lastRead < kChipTempIntervalMs) {
    return;
  }
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
  if (!gAp3216Ready || millis() - lastRead < kLightIntervalMs) {
    return;
  }
  lastRead = millis();
  ap3216c::read(&gLight);
}

void readQmaIfDue() {
  static unsigned long lastRead = 0;
  if (!gQmaReady || millis() - lastRead < kAccelIntervalMs) {
    return;
  }
  lastRead = millis();
  qma6100p::read(&gAccel);
}

void readMicIfDue() {
  static unsigned long lastRead = 0;
  if (!gMicReady || millis() - lastRead < kMicIntervalMs) {
    return;
  }
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
    if (!es8388codec::playMonoPcm(sequence[i]->samples, sequence[i]->sampleCount, 27)) {
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
    const bool ok = es8388codec::playToneAndMeasure(880, 650, &reading, 28);
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

String statusJson() {
  String json;
  json.reserve(2200);
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
  json += "},";
  json += "\"qma6100p\":{\"online\":";
  json += gAccel.online ? "true" : "false";
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
  json += "\"display\":{\"online\":";
  json += gDisplayReady ? "true" : "false";
  json += ",\"page\":";
  json += String(gSystem.displayPage);
  json += ",\"page_name\":\"";
  json += gSystem.displayPageName;
  json += "\"},";
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
  json += ",\"last_test_ms\":";
  json += String(gSpeaker.lastTestMs);
  json += "}";
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
    json += "}";
  }
  json += "]}";
  return json;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", FPSTR(kDashboardHtml));
}

void handleStatus() {
  server.send(200, "application/json; charset=utf-8", statusJson());
}

void handleHistory() {
  server.send(200, "application/json; charset=utf-8", historyJson());
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
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/history", HTTP_GET, handleHistory);
  server.on("/api/speaker_test", HTTP_POST, handleSpeakerTest);
  server.on("/api/speak_temperature", HTTP_POST, handleSpeakTemperature);
  server.on("/api/log.csv", HTTP_GET, handleLogDownload);
  server.begin();
}

void reportStatusIfDue() {
  static unsigned long lastReport = 0;
  if (millis() - lastReport < kSerialReportIntervalMs) {
    return;
  }
  lastReport = millis();
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
}

}  // namespace

void setup() {
  pinMode(kStatusLedPin, OUTPUT);
  digitalWrite(kStatusLedPin, LOW);

  Serial.begin(115200);
  delay(1200);
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

  gAp3216Ready = ap3216c::begin();
  gQmaReady = qma6100p::begin();
  gMicReady = es8388codec::beginMic();
  gSpeaker.online = gMicReady && es8388codec::isReady();

  setupServer();
  updateSystemStatsIfDue();
  updateDisplayIfDue();
  reportStatusIfDue();
}

void loop() {
  server.handleClient();
  updateSystemStatsIfDue();
  readDhtIfDue();
  readChipTempIfDue();
  readAp3216IfDue();
  readQmaIfDue();
  readMicIfDue();
  runSpeakerActionsIfDue();
  publishSampleIfDue();
  flushIfDue();
  updateDisplayIfDue();
  reportStatusIfDue();
  digitalWrite(kStatusLedPin, (millis() / 500) % 2);
}
