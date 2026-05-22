#include "app_html.h"

String app_html_get_login_page() {
  return R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-S3 Login</title>
  <style>
    :root {
      --bg1: #eef7f4;
      --bg2: #f7efe8;
      --card: #ffffff;
      --ink: #1f2a1f;
      --muted: #5f6d62;
      --accent: #0f766e;
      --line: #d6e0d2;
      --err: #b3261e;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      padding: 16px;
      background:
        radial-gradient(circle at 0% 0%, #d9efe4 0, transparent 32%),
        radial-gradient(circle at 100% 100%, #efe3cc 0, transparent 38%),
        linear-gradient(135deg, var(--bg1), var(--bg2));
      font-family: "Segoe UI", "Trebuchet MS", sans-serif;
      color: var(--ink);
    }
    .card {
      width: 100%;
      max-width: 420px;
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 16px;
      padding: 18px;
      box-shadow: 0 12px 32px rgba(0, 0, 0, 0.06);
    }
    h1 { margin: 0 0 8px; font-size: 1.4rem; }
    .sub { color: var(--muted); margin-bottom: 12px; line-height: 1.45; }
    input, button {
      width: 100%;
      border-radius: 10px;
      border: 1px solid #cad8c8;
      padding: 10px;
      font-size: 0.95rem;
    }
    button {
      border: none;
      background: var(--accent);
      color: white;
      cursor: pointer;
      font-weight: 700;
      margin-top: 10px;
    }
    .msg { margin-top: 10px; color: var(--muted); }
    .err { color: var(--err); font-weight: 700; }
    .ok { color: #2d8a52; font-weight: 700; }
    .note {
      margin: 10px 0;
      padding: 10px;
      border-radius: 10px;
      background: #fff4e5;
      border: 1px solid #f2c078;
      color: #7a4b0f;
      font-size: 0.9rem;
      line-height: 1.45;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>Config Mode Login</h1>
    <div class="note" id="authNote">Lưu ý: Nếu bạn chưa được cấp quyền (Unauthorized), hệ thống sẽ giữ bạn ở màn hình login. Vui lòng đăng nhập lại bằng login key hoặc đổi client hiện tại hết phiên.</div>
    <input id="key" type="password" placeholder="Nhập login key" />
    <button onclick="login()">Đăng nhập</button>
    <div id="msg" class="msg"></div>
  </div>

<script>
async function api(path, method = "GET", bodyObj = null) {
  const opts = { method };
  if (bodyObj) {
    const params = new URLSearchParams();
    Object.keys(bodyObj).forEach(k => params.append(k, bodyObj[k]));
    opts.body = params;
    opts.headers = { "Content-Type": "application/x-www-form-urlencoded" };
  }
  const res = await fetch(path, opts);
  const txt = await res.text();
  let data = {};
  try { data = JSON.parse(txt || "{}"); } catch (_) {}
  if (!res.ok) {
    if (res.status === 401) {
      window.location.href = '/?note=unauthorized';
      throw new Error('Unauthorized. Please login again');
    }
    throw new Error(data.message || txt || "Request failed");
  }
  return data;
}

function setMsg(text, ok) {
  const m = document.getElementById('msg');
  m.className = 'msg ' + (ok ? 'ok' : 'err');
  m.textContent = text;
}

function showNoteFromQuery() {
  const params = new URLSearchParams(window.location.search);
  const note = params.get('note');
  if (note === 'unauthorized') {
    setMsg('Bạn chưa được cấp quyền. Vui lòng đăng nhập để tiếp tục.', false);
  }
}

async function login() {
  const key = document.getElementById('key').value;
  try {
    const r = await api('/api/login', 'POST', { key });
    setMsg(r.message || 'Đăng nhập thành công, đang vào config...', true);
    setTimeout(() => { window.location.href = '/'; }, 250);
  } catch (e) {
    setMsg(e.message, false);
  }
}

showNoteFromQuery();
</script>
</body>
</html>
)HTML";
}

String app_html_get_normal_page() {
  return R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-S3 Normal Mode</title>
  <style>
    :root {
      --bg: #f4f8fb;
      --card: #ffffff;
      --ink: #1e293b;
      --muted: #64748b;
      --line: #dbe4ee;
      --accent: #0f766e;
      --ok: #2d8a52;
      --warn: #b3541e;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      padding: 20px;
      font-family: "Segoe UI", "Trebuchet MS", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 0% 0%, #d7ecf6 0, transparent 34%),
        radial-gradient(circle at 100% 100%, #e6efe4 0, transparent 36%),
        var(--bg);
    }
    .wrap { max-width: 860px; margin: 0 auto; }
    .card {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 16px;
      padding: 16px;
      box-shadow: 0 10px 28px rgba(0, 0, 0, 0.04);
      margin-bottom: 14px;
    }
    h1 { margin: 0 0 8px; font-size: 1.7rem; }
    h2 { margin: 0 0 10px; font-size: 1.1rem; }
    .sub { margin: 0 0 8px; color: var(--muted); line-height: 1.45; }
    .pill {
      display: inline-block;
      padding: 3px 10px;
      border-radius: 999px;
      border: 1px solid var(--line);
      background: #eff5fb;
      margin-right: 6px;
      margin-bottom: 6px;
      font-size: 0.82rem;
    }
    .status { line-height: 1.5; font-size: 0.95rem; }
    .ok { color: var(--ok); font-weight: 700; }
    .warn { color: var(--warn); font-weight: 700; }
    .toggle {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      margin-bottom: 10px;
      font-weight: 600;
      color: var(--ink);
    }
    .row {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      align-items: center;
      margin-bottom: 10px;
    }
    .input {
      border: 1px solid var(--line);
      border-radius: 10px;
      padding: 8px 10px;
      font-size: 0.95rem;
      width: 130px;
      background: #fff;
      color: var(--ink);
    }
    .range {
      width: 220px;
      max-width: 100%;
    }
    button {
      border: none;
      border-radius: 10px;
      padding: 10px 12px;
      background: var(--accent);
      color: white;
      font-weight: 700;
      cursor: pointer;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <div class="card">
      <h1>ESP32-S3 Normal Mode</h1>
      <button onclick="switchToConfigMode()">Chuyển sang Config Mode</button>
      <div class="status" id="modeSwitchStatus">--</div>
      <div id="summary">Loading...</div>
    </div>

    <div class="card">
      <h2>Thông tin kết nối</h2>
      <div class="status" id="networkStatus">--</div>
    </div>

    <div class="card">
      <h2>AP in Normal Mode</h2>
      <label class="toggle">
        <input id="apToggle" type="checkbox" onchange="onToggleAp(this.checked)" />
        <span>Bật AP ở normal mode</span>
      </label>
      <div class="status" id="apToggleStatus">--</div>
    </div>

    <div class="card">
      <h2>Output Control (Normal Mode)</h2>
      <label class="toggle">
        <input id="led1Toggle" type="checkbox" onchange="onToggleLed1(this.checked)" />
        <span>LED1</span>
      </label>
      <label class="toggle">
        <input id="pwm1Toggle" type="checkbox" onchange="onPwmChanged()" />
        <span>PWM1</span>
      </label>
      <div class="row">
        <label for="pwm1Duty">Duty (%)</label>
        <input id="pwm1Duty" class="range" type="range" min="0" max="100" step="1" value="50" oninput="onDutyInput(this.value)" onchange="onPwmChanged()" />
        <span id="pwm1DutyValue" class="pill">50%</span>
      </div>
      <div class="status" id="outputStatus">--</div>
    </div>

    <div class="card">
      <h2>Sensor</h2>
      <div class="status" id="sensorStatus">--</div>
    </div>
  </div>

<script>
async function api(path, method = "GET", bodyObj = null) {
  const opts = { method };
  if (bodyObj) {
    const params = new URLSearchParams();
    Object.keys(bodyObj).forEach(k => params.append(k, bodyObj[k]));
    opts.body = params;
    opts.headers = { "Content-Type": "application/x-www-form-urlencoded" };
  }

  const res = await fetch(path, opts);
  const txt = await res.text();
  let data = {};
  try { data = JSON.parse(txt || "{}"); } catch (_) {}
  if (!res.ok) {
    if (res.status === 401) {
      window.location.href = '/?note=unauthorized';
      throw new Error('Unauthorized. Please login again');
    }
    throw new Error(data.message || txt || "Request failed");
  }
  return data;
}

function esc(s) { return String(s ?? ""); }

function setApToggleMsg(text, ok = true) {
  const el = document.getElementById('apToggleStatus');
  el.innerHTML = '<span class="' + (ok ? 'ok' : 'warn') + '">' + esc(text) + '</span>';
}

function setModeSwitchMsg(text, ok = true) {
  const el = document.getElementById('modeSwitchStatus');
  el.innerHTML = '<span class="' + (ok ? 'ok' : 'warn') + '">' + esc(text) + '</span>';
}

function setOutputMsg(text, ok = true) {
  const el = document.getElementById('outputStatus');
  el.innerHTML = '<span class="' + (ok ? 'ok' : 'warn') + '">' + esc(text) + '</span>';
}

function onDutyInput(v) {
  document.getElementById('pwm1DutyValue').textContent = String(v) + '%';
}

function hardResetToRoot() {
  const target = '/?r=' + Date.now();
  window.location.replace(target);
  setTimeout(() => { window.location.reload(); }, 80);
}

async function switchToConfigMode() {
  try {
    const r = await api('/api/mode', 'POST', { mode: 'config' });
    setModeSwitchMsg(r.message || 'Đã chuyển sang config mode', true);
    setTimeout(() => { hardResetToRoot(); }, 200);
  } catch (e) {
    setModeSwitchMsg(e.message, false);
  }
}

async function refreshStatus() {
  try {
    const s = await api('/api/status');
    if (s.mode !== 'normal') {
      hardResetToRoot();
      return;
    }

    const internetText = s.internet_checked
      ? (s.internet_connected ? 'Online' : 'Offline')
      : 'Checking';
    document.getElementById('summary').innerHTML =
      '<span class="pill">Mode: ' + s.mode + '</span>' +
      '<span class="pill">STA: ' + (s.sta_connected ? 'Connected' : 'Disconnected') + '</span>' +
      '<span class="pill">Internet: ' + internetText + '</span>' +
      '<span class="pill">AP: ' + (s.ap_active ? 'On' : 'Off') + '</span>';

    document.getElementById('networkStatus').innerHTML =
      'STA SSID: <span class="ok">' + esc(s.sta_connected_ssid || '--') + '</span><br>' +
      'Internet: <span class="' + (s.internet_connected ? 'ok' : 'warn') + '">' + internetText + '</span><br>' +
      'Last check: ' + (s.internet_checked ? (s.internet_last_check_sec_ago + 's ago') : '--') + '<br>' +
      'STA URL: ' + esc(s.sta_url || '-') + '<br>' +
      'AP URL: ' + esc(s.ap_url || '-') + '<br>';

    const apToggle = document.getElementById('apToggle');
    apToggle.checked = !!s.normal_ap_enabled;
    setApToggleMsg(s.normal_ap_enabled ? 'AP đang bật ở normal mode' : 'AP đang tắt ở normal mode', true);

    const led1Toggle = document.getElementById('led1Toggle');
    const pwm1Toggle = document.getElementById('pwm1Toggle');
    const pwm1Duty = document.getElementById('pwm1Duty');

    led1Toggle.checked = !!s.led1_enabled;
    pwm1Toggle.checked = !!s.pwm1_enabled;
    pwm1Duty.value = Number(s.pwm1_duty_percent || 0);
    onDutyInput(pwm1Duty.value);

    setOutputMsg(
      'LED1: ' + (s.led1_enabled ? 'ON' : 'OFF') +
      ' | PWM1: ' + (s.pwm1_enabled ? 'ON' : 'OFF') +
      ' | 5000Hz, ' + Number(s.pwm1_duty_percent || 0) + '%',
      true
    );
  } catch (e) {
    document.getElementById('networkStatus').textContent = e.message;
    setApToggleMsg(e.message, false);
    setOutputMsg(e.message, false);
  }
}

async function onToggleAp(enabled) {
  try {
    const r = await api('/api/ap-toggle', 'POST', { enabled: enabled ? '1' : '0' });
    setApToggleMsg(r.message || 'Đã cập nhật AP', true);
    refreshStatus();
  } catch (e) {
    setApToggleMsg(e.message, false);
  }
}

async function onToggleLed1(enabled) {
  try {
    const r = await api('/api/output-led1', 'POST', { enabled: enabled ? '1' : '0' });
    setOutputMsg(r.message || 'LED1 updated', true);
    refreshStatus();
  } catch (e) {
    setOutputMsg(e.message, false);
  }
}

async function onPwmChanged() {
  const enabled = document.getElementById('pwm1Toggle').checked;
  const duty = Number(document.getElementById('pwm1Duty').value || 0);

  if (!Number.isFinite(duty) || duty < 0 || duty > 100) {
    setOutputMsg('Duty must be 0..100%', false);
    return;
  }

  try {
    const r = await api('/api/output-pwm1', 'POST', {
      enabled: enabled ? '1' : '0',
      duty_percent: String(Math.round(duty))
    });
    setOutputMsg(r.message || 'PWM1 updated', true);
    refreshStatus();
  } catch (e) {
    setOutputMsg(e.message, false);
  }
}

async function refreshSensor() {
  try {
    const s = await api('/api/sensor');
    let rtcDate = 'RTC unavailable';
    let rtcTime = '';
    if (s.rtc) {
      const parts = String(s.rtc).split('T');
      if (parts.length === 2) {
        const d = parts[0].split('-');
        const t = parts[1].split(':');
        if (d.length === 3 && t.length >= 2) {
          rtcDate = d[2] + '/' + d[1] + '/' + d[0];
          rtcTime = t[0] + ':' + t[1];
        } else {
          rtcDate = String(s.rtc);
        }
      } else {
        rtcDate = String(s.rtc);
      }
    }

    document.getElementById('sensorStatus').innerHTML =
      'Temp: ' + s.temp_c.toFixed(1) + ' C<br>' +
      'Humidity: ' + s.humidity.toFixed(1) + ' %<br>' +
      'Counter: ' + s.counter + '<br>' +
      'RTC: ' + rtcDate + (rtcTime ? '<br>' + rtcTime : '') + '<br>' +
      'NTP: <span class="' + (s.ntp_synced ? 'ok' : 'warn') + '">' + (s.ntp_synced ? 'Synced' : 'Pending') + '</span>';
  } catch (e) {
    document.getElementById('sensorStatus').textContent = e.message;
  }
}

function refreshAll() {
  refreshStatus();
  refreshSensor();
}

setInterval(refreshAll, 3000);
refreshAll();
</script>
</body>
</html>
)HTML";
}

String app_html_get_main_page() {
  return R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32-S3 Control</title>
  <style>
    :root {
      --bg: #f3f6ef;
      --card: #ffffff;
      --ink: #1f2a1f;
      --muted: #617063;
      --line: #d8e2d5;
      --ok: #2d8a52;
      --warn: #b3541e;
      --danger: #b3261e;
      --accent: #0f766e;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: "Segoe UI", "Trebuchet MS", sans-serif;
      color: var(--ink);
      background:
        radial-gradient(circle at 0% 0%, #d9efe4 0, transparent 32%),
        radial-gradient(circle at 100% 100%, #efe3cc 0, transparent 38%),
        var(--bg);
      min-height: 100vh;
      padding: 20px;
    }
    .wrap { max-width: 980px; margin: 0 auto; }
    h1 { margin: 0 0 8px; font-size: 1.8rem; }
    .sub { color: var(--muted); margin-bottom: 16px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 14px;
    }
    .card {
      background: var(--card);
      border: 1px solid var(--line);
      border-radius: 16px;
      padding: 14px;
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.04);
    }
    h2 { margin: 0 0 10px; font-size: 1.1rem; }
    label { font-size: 0.9rem; color: var(--muted); display: block; margin-top: 8px; }
    input, select, button {
      width: 100%;
      border-radius: 10px;
      border: 1px solid #cfd9cc;
      padding: 10px;
      margin-top: 4px;
      font-size: 0.95rem;
    }
    button {
      border: none;
      color: white;
      background: var(--accent);
      cursor: pointer;
      font-weight: 600;
      margin-top: 10px;
    }
    button.secondary { background: #334155; }
    button.warn { background: var(--warn); }
    button.danger { background: var(--danger); }
    .status { margin-top: 10px; line-height: 1.45; font-size: 0.93rem; }
    .pill {
      display: inline-block;
      padding: 2px 9px;
      border-radius: 999px;
      background: #ecf2ec;
      border: 1px solid var(--line);
      margin-right: 6px;
      margin-bottom: 4px;
      font-size: 0.8rem;
    }
    .ok { color: var(--ok); font-weight: 700; }
    .err { color: var(--danger); font-weight: 700; }
    .row { display: flex; gap: 8px; }
    .row > * { flex: 1; }
    @media (max-width: 600px) {
      body { padding: 12px; }
      .row { flex-direction: column; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32-S3 Web Server</h1>
    <div class="sub" id="summary">Loading...</div>

    <div class="grid">
      <section class="card">
        <h2>Config Mode</h2>
        <div class="row">
          <button class="secondary" onclick="switchMode('normal')">Chuyển sang Normal mode</button>
        </div>
        <label>Timeout (seconds)</label>
        <input id="timeoutSec" type="number" min="10" step="10" />
        <button onclick="saveTimeout()">Lưu timeout</button>
        <div class="status" id="modeStatus"></div>
      </section>

      <section class="card">
        <h2>Station WiFi</h2>
        <div class="status" style="background: #f0f9f7; border: 1px solid #0f766e; border-radius: 8px; padding: 10px; margin-bottom: 12px;">
          <strong>Mạng đang kết nối:</strong> <span id="connectedSsid" class="ok">--</span>
        </div>
        <button class="secondary" onclick="scanWifi()">Scan WiFi</button>
        <label>Mạng gần đây</label>
        <select id="scanList"></select>
        <label>STA SSID</label>
        <input id="staSsid" type="text" maxlength="32" />
        <label>STA Password</label>
        <input id="staPass" type="password" maxlength="64" />
        <button onclick="saveSta()">Kết nối STA</button>
        <div class="status" id="staStatus"></div>
      </section>

      <section class="card">
        <h2>Access Point</h2>
        <label>AP SSID</label>
        <input id="apSsid" type="text" maxlength="32" />
        <label>AP Password</label>
        <input id="apPass" type="password" maxlength="64" />
        <button onclick="saveAp()">Lưu AP</button>
        <div class="status" id="apStatus"></div>
      </section>

      <section class="card">
        <h2>Security</h2>
        <label>Change login key</label>
        <input id="newLoginKey" type="password" maxlength="64" placeholder="Khoa moi" />
        <button onclick="changeLoginKey()">Đổi khóa login</button>
        <div class="status" id="loginStatus"></div>
      </section>

      <section class="card">
        <h2>CoreIoT MQTT</h2>
        <label>MQTT Server</label>
        <input id="mqttServer" type="text" maxlength="64" />
        <label>MQTT Port</label>
        <input id="mqttPort" type="number" min="1" max="65535" />
        <label>MQTT User</label>
        <input id="mqttUser" type="text" maxlength="64" />
        <label>MQTT Password</label>
        <input id="mqttPassword" type="text" maxlength="64" />
        <label>Publish Interval (seconds)</label>
        <input id="mqttIntervalSec" type="number" min="10" max="3600" step="10" />
        <button onclick="saveMqttConfig()">Lưu MQTT</button>
        <div class="status" id="mqttCurrentValues" style="margin-top:12px; padding-top:12px; border-top:1px solid var(--line);"></div>
        <div class="status" id="mqttStatus"></div>
      </section>

      <section class="card">
        <h2>Sensor</h2>
        <div class="status" id="sensorStatus">--</div>
      </section>
    </div>
  </div>

<script>
let g_isInitialized = false;

function esc(s) { return String(s ?? ""); }

async function api(path, method = "GET", bodyObj = null) {
  const opts = { method };
  if (bodyObj) {
    const params = new URLSearchParams();
    Object.keys(bodyObj).forEach(k => params.append(k, bodyObj[k]));
    opts.body = params;
    opts.headers = { "Content-Type": "application/x-www-form-urlencoded" };
  }
  const res = await fetch(path, opts);
  const txt = await res.text();
  let data = {};
  try { data = JSON.parse(txt || "{}"); } catch (_) {}
  if (!res.ok) {
    throw new Error(data.message || txt || "Request failed");
  }
  return data;
}

function setMsg(id, msg, ok = true) {
  const el = document.getElementById(id);
  el.innerHTML = '<span class="' + (ok ? 'ok' : 'err') + '">' + esc(msg) + '</span>';
}

function hardResetToRoot() {
  const target = '/?r=' + Date.now();
  window.location.replace(target);
  setTimeout(() => { window.location.reload(); }, 80);
}

async function refreshStatus() {
  try {
    const s = await api('/api/status');
    if (s.mode !== 'config') {
      hardResetToRoot();
      return;
    }

    const internetText = s.internet_checked
      ? (s.internet_connected ? 'Online' : 'Offline')
      : 'Checking';
    document.getElementById('summary').innerHTML =
      '<span class="pill">Mode: ' + s.mode + '</span>' +
      '<span class="pill">STA: ' + (s.sta_connected ? 'Connected' : 'Disconnected') + '</span>' +
      '<span class="pill">Internet: ' + internetText + '</span>' +
      '<span class="pill">AP: ' + (s.ap_active ? 'On' : 'Off') + '</span>';

    if (!g_isInitialized) {
      document.getElementById('timeoutSec').value = s.config_timeout_sec;
      document.getElementById('staSsid').value = s.sta_ssid;
      document.getElementById('apSsid').value = s.ap_ssid;
      
      document.getElementById('mqttServer').value = s.mqtt_server || '';
      document.getElementById('mqttPort').value = s.mqtt_port || 1883;
      document.getElementById('mqttUser').value = s.mqtt_user || '';
      document.getElementById('mqttPassword').value = s.mqtt_password || '';
      document.getElementById('mqttIntervalSec').value = s.mqtt_publish_interval_sec || 60;

      g_isInitialized = true;
    }

    document.getElementById('mqttCurrentValues').innerHTML = 
      '<strong>Giá trị hiện tại trên ESP32:</strong><br>' +
      'Server: ' + esc(s.mqtt_server || '(trống)') + ':' + (s.mqtt_port || 1883) + '<br>' +
      'User: ' + esc(s.mqtt_user || '(trống)') + '<br>' +
      'Chu kỳ gửi: ' + (s.mqtt_publish_interval_sec || 60) + 's';

    document.getElementById('modeStatus').innerHTML =
      'Config timeout: ' + s.config_timeout_sec + 's<br>' +
      'Internet: <span class="' + (s.internet_connected ? 'ok' : 'err') + '">' + internetText + '</span><br>' +
      'Last check: ' + (s.internet_checked ? (s.internet_last_check_sec_ago + 's ago') : '--') + '<br>' +
      'AP URL: ' + esc(s.ap_url || '-') + '<br>' +
      'STA URL: ' + esc(s.sta_url || '-');

    const connectedSsid = esc(s.sta_connected_ssid || '--');
    document.getElementById('connectedSsid').textContent = connectedSsid;

    document.getElementById('staStatus').innerHTML =
      'Connected: ' + s.sta_connected + '<br>' +
      'Internet: ' + internetText + '<br>' +
      'Connected SSID: ' + connectedSsid + '<br>' +
      'Web by STA: ' + esc(s.sta_url || '-');

    document.getElementById('apStatus').innerHTML =
      'AP SSID: ' + esc(s.ap_ssid) + '<br>' +
      'Web by AP: ' + esc(s.ap_url || '-') + '<br>' +
      'AP in normal: ' + (s.normal_ap_enabled ? 'On' : 'Off');
  } catch (e) {
    setMsg('modeStatus', e.message, false);
  }
}

async function refreshSensor() {
  try {
    const s = await api('/api/sensor');
    let rtcDate = 'RTC unavailable';
    let rtcTime = '';
    if (s.rtc) {
      const parts = String(s.rtc).split('T');
      if (parts.length === 2) {
        const d = parts[0].split('-');
        const t = parts[1].split(':');
        if (d.length === 3 && t.length >= 2) {
          rtcDate = d[2] + '/' + d[1] + '/' + d[0];
          rtcTime = t[0] + ':' + t[1];
        } else {
          rtcDate = String(s.rtc);
        }
      } else {
        rtcDate = String(s.rtc);
      }
    }

    document.getElementById('sensorStatus').innerHTML =
      'Temp: ' + s.temp_c.toFixed(1) + ' C<br>' +
      'Humidity: ' + s.humidity.toFixed(1) + ' %<br>' +
      'Counter: ' + s.counter + '<br>' +
      'RTC: ' + rtcDate + (rtcTime ? '<br>' + rtcTime : '') + '<br>' +
      'NTP: <span class="' + (s.ntp_synced ? 'ok' : 'err') + '">' + (s.ntp_synced ? 'Synced' : 'Pending') + '</span>';
  } catch (e) {
    setMsg('sensorStatus', e.message, false);
  }
}

async function scanWifi() {
  try {
    const sel = document.getElementById('scanList');
    sel.innerHTML = '<option>Đang scan...</option>';
    setMsg('staStatus', 'Đang scan WiFi xung quanh...', true);

    await api('/api/scan');

    let tries = 0;
    const maxTries = 20;
    while (tries < maxTries) {
      const data = await api('/api/scan-result');
      const networks = data.networks || [];
      const message = String(data.message || '');

      if (message === 'Scan in progress' || message === 'Scan queued') {
        tries += 1;
        await new Promise(resolve => setTimeout(resolve, 500));
        continue;
      }

      sel.innerHTML = '<option value="" disabled selected>-- Chọn mạng WiFi --</option>';
      networks.forEach(n => {
        const opt = document.createElement('option');
        opt.value = n.ssid;
        opt.textContent = n.ssid + ' (' + n.rssi + ' dBm)';
        sel.appendChild(opt);
      });

      if (networks.length === 0) {
        const opt = document.createElement('option');
        opt.value = '';
        opt.textContent = 'Khong tim thay mang WiFi';
        sel.appendChild(opt);
        setMsg('staStatus', message || 'Khong tim thay mang WiFi gan day', false);
        return;
      }

      setMsg('staStatus', 'Tim thay ' + networks.length + ' mang WiFi', true);
      sel.onchange = () => { document.getElementById('staSsid').value = sel.value; };
      
      // Auto-fill the strongest network if they don't want to select manually
      if (networks.length > 0) {
        document.getElementById('staSsid').value = networks[0].ssid;
      }
      return;
    }

    setMsg('staStatus', 'Scan het thoi gian cho phep', false);
  } catch (e) {
    setMsg('staStatus', e.message, false);
  }
}

async function saveSta() {
  try {
    const ssid = document.getElementById('staSsid').value;
    const password = document.getElementById('staPass').value;
    const r = await api('/api/sta', 'POST', { ssid, password });
    setMsg('staStatus', r.message || 'Đã lưu STA');
    refreshStatus();
  } catch (e) {
    setMsg('staStatus', e.message, false);
  }
}

async function saveAp() {
  try {
    const ssid = document.getElementById('apSsid').value;
    const password = document.getElementById('apPass').value;
    const r = await api('/api/ap', 'POST', { ssid, password });
    setMsg('apStatus', r.message || 'Đã lưu AP');
    refreshStatus();
  } catch (e) {
    setMsg('apStatus', e.message, false);
  }
}

async function saveTimeout() {
  try {
    const timeout_sec = document.getElementById('timeoutSec').value;
    const r = await api('/api/config-timeout', 'POST', { timeout_sec });
    setMsg('modeStatus', r.message || 'Đã lưu timeout');
    refreshStatus();
  } catch (e) {
    setMsg('modeStatus', e.message, false);
  }
}

async function saveMqttConfig() {
  try {
    const server = document.getElementById('mqttServer').value;
    const port = document.getElementById('mqttPort').value;
    const user = document.getElementById('mqttUser').value;
    const password = document.getElementById('mqttPassword').value;
    const publish_interval_sec = document.getElementById('mqttIntervalSec').value;
    const r = await api('/api/mqtt-config', 'POST', {
      server,
      port,
      user,
      password,
      publish_interval_sec
    });
    setMsg('mqttStatus', r.message || 'Đã lưu cấu hình MQTT');
    refreshStatus();
  } catch (e) {
    setMsg('mqttStatus', e.message, false);
  }
}

async function switchMode(mode) {
  try {
    const r = await api('/api/mode', 'POST', { mode });
    setMsg('modeStatus', r.message || 'Đã chuyển chế độ');
    setTimeout(() => { hardResetToRoot(); }, 200);
  } catch (e) {
    setMsg('modeStatus', e.message, false);
    if (mode === 'normal') {
      setTimeout(() => { hardResetToRoot(); }, 300);
    }
  }
}

async function changeLoginKey() {
  try {
    const new_key = document.getElementById('newLoginKey').value;
    const r = await api('/api/login-key', 'POST', { new_key });
    setMsg('loginStatus', r.message || 'Đã đổi khóa login');
  } catch (e) {
    setMsg('loginStatus', e.message, false);
  }
}

setInterval(refreshStatus, 4000);
setInterval(refreshSensor, 3000);
refreshStatus();
refreshSensor();
</script>
</body>
</html>
)HTML";
}
