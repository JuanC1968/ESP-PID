const PWM_MAX = 1023;
const configForm = document.getElementById('configForm');
const resetForm = document.getElementById('resetForm');
const refreshState = document.getElementById('refreshState');
let editing = false;

function txt(id, value) {
  const element = document.getElementById(id);
  if (element) {
    element.textContent = value;
  }
}

function value(id, nextValue) {
  const element = document.getElementById(id);
  if (element) {
    element.value = nextValue;
  }
}

function setEditing(nextEditing) {
  editing = nextEditing;
  refreshState.textContent = nextEditing
    ? 'Actualizacion pausada mientras editas'
    : 'Actualizacion automatica activa';
}

function fillConfig(config) {
  value('setpoint', Number(config.setpoint).toFixed(1));
  value('kp', Number(config.kp).toFixed(2));
  value('ki', Number(config.ki).toFixed(2));
  value('kd', Number(config.kd).toFixed(2));
  value('outMin', Number(config.outMin).toFixed(0));
  value('outMax', Number(config.outMax).toFixed(0));
  document.getElementById('invert').checked = Boolean(config.invert);
}

async function loadConfig() {
  const response = await fetch('/config.json', { cache: 'no-store' });
  fillConfig(await response.json());
}

async function updateStatus() {
  if (editing) {
    return;
  }

  try {
    const response = await fetch('/status', { cache: 'no-store' });
    const data = await response.json();

    txt('input', Number(data.input).toFixed(1));
    txt('setpointValue', Number(data.setpoint).toFixed(1));
    txt('error', Number(data.error).toFixed(1));
    txt('output', (Number(data.output) * 100 / PWM_MAX).toFixed(1));
    txt('raw', data.raw);
    txt('rawPercent', Number(data.rawPercent).toFixed(1));

    const state = document.getElementById('state');
    state.textContent = data.inSet ? 'SET' : 'NO SET';
    state.classList.toggle('ok', Boolean(data.inSet));
  } catch (error) {
  }
}

async function submitForm(event) {
  event.preventDefault();
  const body = new URLSearchParams(new FormData(configForm));
  await fetch('/config', { method: 'POST', body });
  setEditing(false);
  await loadConfig();
  await updateStatus();
}

async function resetPid(event) {
  event.preventDefault();
  await fetch('/reset', { method: 'POST' });
  await updateStatus();
}

configForm.addEventListener('focusin', () => setEditing(true));
configForm.addEventListener('focusout', () => {
  setTimeout(() => setEditing(configForm.contains(document.activeElement)), 80);
});
configForm.addEventListener('submit', submitForm);
resetForm.addEventListener('submit', resetPid);

loadConfig().catch(() => {});
updateStatus();
setInterval(updateStatus, 1000);
