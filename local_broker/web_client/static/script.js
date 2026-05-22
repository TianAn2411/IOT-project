const socket = io();

// DOM Elements
const valTemp = document.getElementById('val-temp');
const valHum = document.getElementById('val-hum');
const valPred = document.getElementById('val-pred');

const attrDevice = document.getElementById('attr-device');
const attrSsid = document.getElementById('attr-ssid');
const attrInterval = document.getElementById('attr-interval');
const statusBadge = document.getElementById('status-badge');

const pwmToggle = document.getElementById('pwm-toggle');
const pwmDuty = document.getElementById('pwm-duty');
const valDuty = document.getElementById('val-duty');
const btnApply = document.getElementById('btn-apply-pwm');

let isOnline = false;
let onlineTimeout;

function setOnline() {
    isOnline = true;
    statusBadge.textContent = 'Online';
    statusBadge.classList.remove('status-offline');
    statusBadge.classList.add('status-online');
    
    // Auto offline after 15s if no new messages
    clearTimeout(onlineTimeout);
    onlineTimeout = setTimeout(() => {
        isOnline = false;
        statusBadge.textContent = 'Offline';
        statusBadge.classList.remove('status-online');
        statusBadge.classList.add('status-offline');
    }, 15000);
}

socket.on('connect', () => {
    console.log('Connected to server');
});

socket.on('telemetry', (data) => {
    setOnline();
    if(data.temperature !== undefined) valTemp.textContent = `${data.temperature.toFixed(1)} °C`;
    if(data.humidity !== undefined) valHum.textContent = `${data.humidity.toFixed(1)} %`;
    if(data.prediction !== undefined) valPred.textContent = data.prediction;
});

socket.on('attributes', (data) => {
    setOnline();
    if(data.device) attrDevice.textContent = data.device;
    if(data.wifi_ssid) attrSsid.textContent = data.wifi_ssid;
    if(data.mqtt_interval_sec) attrInterval.textContent = data.mqtt_interval_sec;
});

socket.on('rpc_response', (res) => {
    console.log("RPC Response:", res);
    
    // Attempt to parse response if it contains current PWM settings
    if(res.data) {
        if(typeof res.data === 'string') {
            // Could be a boolean "true"/"false" or a number "50"
            if(res.data === 'true' || res.data === 'false') {
                pwmToggle.checked = (res.data === 'true');
            } else if (!isNaN(res.data)) {
                const duty = parseInt(res.data);
                pwmDuty.value = duty;
                valDuty.textContent = `${duty}%`;
            }
        } else if(typeof res.data === 'object') {
            // JSON Object response
            if(res.data.pwm2_enabled !== undefined) {
                pwmToggle.checked = res.data.pwm2_enabled;
            }
            if(res.data.pwm2_duty_percent !== undefined) {
                pwmDuty.value = res.data.pwm2_duty_percent;
                valDuty.textContent = `${res.data.pwm2_duty_percent}%`;
            }
        }
    }
});

// Update slider value display
pwmDuty.addEventListener('input', (e) => {
    valDuty.textContent = `${e.target.value}%`;
});

// Send RPC on Apply button click
btnApply.addEventListener('click', () => {
    const enabled = pwmToggle.checked;
    const duty = parseInt(pwmDuty.value);
    
    btnApply.textContent = 'Applying...';
    btnApply.disabled = true;

    socket.emit('send_rpc', {
        method: 'setPwm2',
        params: {
            enabled: enabled,
            duty: duty
        }
    });

    setTimeout(() => {
        btnApply.textContent = 'Apply Settings';
        btnApply.disabled = false;
    }, 1000);
});

// Initial RPC call to get current PWM status when connecting
setTimeout(() => {
    socket.emit('send_rpc', {
        method: 'getPwm2',
        params: {}
    });
}, 2000);
