const fs = require('fs');
const vm = require('vm');
const path = require('path');

const sketchPath = path.resolve(__dirname, '..', 'esp32_sensor_hub.ino');
const sketch = fs.readFileSync(sketchPath, 'utf8');
const htmlMatch = sketch.match(/const char kDashboardHtml\[\] PROGMEM = R"HTML\(([\s\S]*?)\)HTML";/);
if (!htmlMatch) {
  throw new Error('dashboard HTML block not found');
}

const scriptMatch = htmlMatch[1].match(/<script>([\s\S]*?)<\/script>/);
if (!scriptMatch) {
  throw new Error('dashboard script block not found');
}

class FakeElement {
  constructor(id) {
    this.id = id;
    this.value = '';
    this.textContent = '';
    this.innerHTML = '';
    this.className = '';
    this.src = '';
    this.dataset = {};
    this.style = {};
    this.listeners = {};
  }

  addEventListener(name, handler) {
    if (!this.listeners[name]) this.listeners[name] = [];
    this.listeners[name].push(handler);
  }

  dispatch(name) {
    for (const handler of this.listeners[name] || []) {
      handler({ target: this, type: name });
    }
  }

  getContext() {
    return {
      canvas: { width: 560, height: 200 },
      clearRect() {},
      beginPath() {},
      moveTo() {},
      lineTo() {},
      stroke() {},
      fillText() {},
      fillRect() {},
      measureText(text) {
        return { width: String(text).length * 7 };
      },
    };
  }
}

const ids = [
  'netState', 'storageState', 'displayState', 'tempC', 'humidity', 'dhtState', 'dhtCount',
  'soundDb', 'soundState', 'cpuUsage', 'cpuState', 'alsValue', 'lightState', 'motionValue',
  'motionState', 'chipTemp', 'chipState', 'heapFree', 'heapState', 'persistedCount',
  'lastSample', 'storageHealth', 'lcdPage', 'lcdState', 'uptime', 'cpuFreq', 'topTask',
  'resetReason', 'speakerLoopback', 'speakerState', 'alertLine', 'alertCount',
  'alertSummary', 'cameraState', 'cameraMeta', 'cameraFrame', 'camFrameSize', 'camQuality',
  'camBrightness', 'camContrast', 'camSaturation', 'camAec', 'camAecValue', 'camAgc',
  'camAgcGain', 'camMirror', 'camVflip', 'regDevice', 'regAddr', 'regMask', 'regValue',
  'regState', 'regReadBtn', 'regWriteBtn', 'apModeControl', 'apModeBtn', 'qmaRangeControl',
  'qmaRangeBtn', 'speakerVolumeControl', 'speakerVolumeBtn', 'headphoneVolumeControl',
  'headphoneVolumeBtn', 'peripheralState', 'cpuMhzControl', 'cpuMhzBtn', 'wifiPowerControl',
  'wifiPowerBtn', 'configState', 'cfgTempHigh', 'cfgHumidityHigh', 'cfgSoundHigh',
  'cfgLightHigh', 'cfgCpuHigh', 'cfgHeapLow', 'rows', 'speakTempBtn', 'speakerSelfTestBtn',
  'saveConfigBtn', 'climateChart', 'envChart', 'motionChart',
];
const elements = Object.fromEntries(ids.map(id => [id, new FakeElement(id)]));

const document = {
  activeElement: null,
  getElementById(id) {
    if (!elements[id]) elements[id] = new FakeElement(id);
    return elements[id];
  },
  querySelectorAll(selector) {
    if (selector === '[data-cam]' || selector === '[data-cam-select]' || selector === '[data-camera-preset]') {
      return [];
    }
    return [];
  },
};

function statusWithCamera(camera) {
  return {
    network: { mode: 'STA', ip: '192.168.1.2', rssi_dbm: -45 },
    storage: { persisted_samples: 1, last_sample_epoch: 1777080000, pending_bytes: 0, write_failures: 0, dropped_samples: 0 },
    display: { online: true, page: 0, page_name: 'SYSTEM' },
    dht11: { online: true, temp_c: 28, humidity: 7, success_count: 1, failure_count: 0 },
    mic: { online: true, dbfs: -50, rms: 0.01, peak: 10 },
    system: { runtime_ready: true, cpu_usage_pct: 12, free_heap_bytes: 100000, min_free_heap_bytes: 90000, max_alloc_heap_bytes: 80000, uptime_sec: 12, cpu_freq_mhz: 240, reset_reason: 'POWERON', top_task: null, top_task_cpu_pct: null },
    ap3216c: { online: true, als: 20, ir: 10, ps: 5, mode: 3 },
    qma6100p: { online: true, range_g: 8, ax: 0, ay: 0, az: 9.8, ag: 9.8, pitch: 0, roll: 0 },
    chip_temp: { online: true, temp_c: 52 },
    speaker: { verification_state: 'PASS', muted_dbfs: -60, enabled_dbfs: -35, delta_dbfs: 25, speak_count: 0, has_spoken_temp: false, speaker_volume: 26, headphone_volume: 18 },
    alerts: { active: false, count: 0, summary: 'OK' },
    config: { persisted: true, temp_high_c: 60, humidity_high_pct: 80, sound_high_dbfs: -20, light_high_als: 3000, cpu_high_pct: 90, heap_low_bytes: 50000 },
    camera: {
      online: true,
      name: 'OV5640',
      pid: 22080,
      frame_size_name: 'QQVGA',
      quality: 18,
      brightness: 1,
      contrast: 0,
      saturation: 0,
      aec: 1,
      aec_value: 885,
      agc: 1,
      agc_gain: 5,
      hmirror: 0,
      vflip: 0,
      last_width: 160,
      last_height: 120,
      last_frame_bytes: 1500,
      capture_count: 4,
      stream_fps: 20.1,
      stream_clients: 1,
      ...camera,
    },
  };
}

const context = {
  document,
  location: { protocol: 'http:', hostname: '127.0.0.1' },
  console,
  URLSearchParams,
  Number,
  Date,
  Math,
  isFinite,
  statusWithCamera,
  setTimeout() { return 0; },
  clearTimeout() {},
  fetch() { return new Promise(() => {}); },
};

const testCode = `
${scriptMatch[1]}

function assert(condition, message) {
  if (!condition) throw new Error(message);
}

renderLive(statusWithCamera({ quality: 18, brightness: 1, frame_size_name: 'QQVGA' }));
assert(statusEls.camQuality.value === '18' || statusEls.camQuality.value === 18, 'idle quality should sync from camera status');
assert(statusEls.camBrightness.value === '1' || statusEls.camBrightness.value === 1, 'idle brightness should sync from camera status');

statusEls.camQuality.value = '31';
statusEls.camQuality.dispatch('input');
renderLive(statusWithCamera({ quality: 18 }));
assert(statusEls.camQuality.value === '31', 'dirty quality slider was overwritten by live status');
assert(statusEls.camQuality.dataset.userEditing === '1', 'dirty quality slider did not keep edit lock');

delete statusEls.camQuality.dataset.userEditing;
document.activeElement = statusEls.camQuality;
statusEls.camQuality.value = '32';
renderLive(statusWithCamera({ quality: 18 }));
assert(statusEls.camQuality.value === '32', 'focused quality slider was overwritten by live status');

document.activeElement = null;
clearCameraControlEditing(statusEls.camQuality);
renderLive(statusWithCamera({ quality: 18 }));
assert(statusEls.camQuality.value === '18' || statusEls.camQuality.value === 18, 'quality did not resync after edit lock cleared');

statusEls.camFrameSize.value = 'SVGA';
statusEls.camFrameSize.dispatch('change');
renderLive(statusWithCamera({ frame_size_name: 'QQVGA' }));
assert(statusEls.camFrameSize.value === 'SVGA', 'dirty frame-size selector was overwritten by live status');

clearAllCameraControlEditing();
renderLive(statusWithCamera({ frame_size_name: 'QVGA' }));
assert(statusEls.camFrameSize.value === 'QVGA', 'frame-size selector did not resync after preset lock clear');
`;

vm.createContext(context);
vm.runInContext(testCode, context, { filename: 'dashboard_edit_lock.vm.js', timeout: 1000 });
console.log('DASHBOARD_EDIT_LOCK_BEHAVIOR_PASS');
