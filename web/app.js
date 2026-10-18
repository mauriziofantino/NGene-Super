import {createReceiverWorkspace} from './workspace.js';
import {createReceiverConfiguration} from './receiver-config.js';
import {moduleParameters} from './module-parameters.js';

const selectedSession = 'receiver';
const sessionPrefix = () => '';
let deviceCaps = null;
let receiverWorkspace = null, activeWorkspaceView = 'rf';
/* NGene browser UI: presentation and commands only. No receiver/DSP implementation. */
const $ = id => document.getElementById(id);
let receiverConfiguration = null, moduleForm = null;
const fields = [
  ['Frontend', 'frontend.type', 'Source', 'select', ['file', 'hackrf', 'rtlsdr', 'bladerf']],
  ['Frontend', 'frontend.serial', 'Device serial', 'text', null, '', 'Empty selects the only connected device of the selected type'],
  ['Frontend', 'frontend.sample_rate_hz', 'Sample rate', 'number', null, 'pairs/s'],
  ['Frontend', 'frontend.center_frequency_hz', 'SDR hardware tuning frequency', 'number', null, 'Hz'],
  ['Frontend', 'frontend.signal_frequency_hz', 'Signal centre frequency', 'number', null, 'Hz', '0 = follow hardware; digital IF = signal − hardware'],
  ['Frontend', 'hackrf.bandwidth_hz', 'Analogue filter', 'number', null, 'Hz', '0 = automatic supported filter'],
  ['HackRF', 'hackrf.lna_gain_db', 'LNA gain', 'select', [0,8,16,24,32,40], 'dB · step 8'],
  ['HackRF', 'hackrf.vga_gain_db', 'VGA gain', 'select', Array.from({length:32},(_,k)=>k*2), 'dB · step 2'],
  ['HackRF', 'hackrf.rf_amp', 'RF amplifier', 'select', ['false', 'true']],
  ['HackRF', 'hackrf.antenna_power', 'Antenna power', 'select', ['false', 'true']],
  ['RTL-SDR', 'rtlsdr.auto_gain', 'Automatic tuner gain', 'select', ['true','false']],
  ['RTL-SDR', 'rtlsdr.gain_tenths_db', 'Tuner gain', 'select', [], '0.1 dB', 'Query device limits to load this tuner’s exact supported gains'],
  ['RTL-SDR', 'rtlsdr.ppm', 'Frequency correction', 'number', null, 'ppm'],
  ['RTL-SDR', 'rtlsdr.bias_tee', 'Bias tee', 'select', ['false','true']],
  ['bladeRF', 'bladerf.gain_db', 'RX gain', 'number', null, 'dB', 'Query device limits for frequency-dependent min, max and step'],
  ['bladeRF', 'bladerf.bias_tee', 'Bias tee', 'select', ['false','true']],
  ['bladeRF', 'bladerf.format', 'Sample precision', 'select', ['SC16_Q11','CI8'], '', 'SC16_Q11: 12-bit ADC in 16-bit I/Q; CI8: native 8-bit I/Q (requires device/FPGA support)'],
  ['File replay', 'replay.format', 'Native IQ format', 'select', ['CI8','CU8','SC16_Q11']],
  ['File replay', 'replay.path', 'Input file on receiver computer', 'text', null, '', 'Native I/Q (.cs8, .cu8, .sc16q11); sidecar loads format, rate and tuning'],
  ['File replay', 'replay.start_sample', 'Start sample pair', 'number', null, 'pairs'],
  ['File replay', 'replay.sample_limit', 'Range length', 'number', null, 'pairs', '0 = remainder of file'],
  ['File replay', 'replay.realtime', 'Real-time pacing', 'select', ['true', 'false']],
  ['File replay', 'replay.loop', 'Repeat range', 'select', ['true', 'false'], '', 'Each rewind reports a discontinuity'],
  ['Receiver', 'receiver.mode', 'Mode', 'select', ['capture', 'record']],
  ['Receiver', 'receiver.run_seconds', 'Run duration', 'number', null, 's', '0 = until Stop'],
  ['Receiver', 'receiver.idle_timeout_ms', 'Input timeout', 'number', null, 'ms'],
  ['Receiver', 'conditioning.module', 'Conditioning', 'select', ['bypass','dc_iir']],
  ['Receiver', 'conditioning.dc_shift', 'DC estimator time constant exponent', 'number', null, '4–24', 'Continuous α = 2^−shift; keeps state across blocks. Default 16.'],
  ['Buffers', 'buffer.window_samples', 'Common window', 'number', null, 'pairs'],
  ['Buffers', 'buffer.ingress_blocks', 'Frontend slots', 'number'],
  ['Buffers', 'buffer.ring_blocks', 'Central ring slots', 'number'],
  ['Buffers', 'buffer.overflow_policy', 'Input overflow', 'select', ['stop', 'drop_newest']],
  ['Recording', 'recording.point', 'Capture point', 'select', ['post', 'pre'], '', 'post = shared conditioned stream; pre = original frontend samples'],
  ['Recording', 'recording.enabled', 'Record on receiver start', 'select', ['false', 'true']],
  ['Recording', 'recording.prefix', 'Base filename', 'text', null, '', 'Letters, digits, _ and - · unique suffix added automatically'],
  ['Recording', 'recording.directory', 'Folder on receiver computer', 'text', null, '', 'Receiver-side path, e.g. /data/gnss or D:/GNSS. Parent folder must exist.'],
  ['Recording', 'recording.queue_blocks', 'Writer queue slots', 'number'],
  ['Recording', 'recording.segment_samples', 'Pairs per segment', 'number', null, '', '0 = single raw file']
];
let values = {}, savedValues = {}, revision = '1', token = '', status = null, capabilities = null;
let dirty = false, httpBusy = false, pendingCommand = null, socket = null, reconnectTimer = null;
let waiting = false, statusRequestPending = false, lastRequest = 0, lastResponse = -Infinity, lastSample = null, lastMessage = '', lastCommand = '', lastRun = '';
let paused = false, fileBusy = false, lastFileRefresh = 0;
let selectedSettings = 'source', deletingFile = false;
let quitting = false, pendingTelemetry=null, renderPending=false;
const profileErrors = new Map(), profileTouched = new Set(), analysisErrors = new Map(), analysisTouched = new Set();
let profileValidationRequested = false, analysisValidationRequested = false, profileRequestNotice = null;
let lastNotice = {error:false, owner:'receiver'};
const analysisHelp = 'Runtime changes stay in this session. Save the full profile while stopped to persist them.';

function log(message, error = false) {
  const item = document.createElement('li'), time = document.createElement('time'), text = document.createElement('span');
  time.textContent = new Date().toLocaleTimeString('en-GB'); text.textContent = message;
  if (error) item.classList.add('error'); item.append(time, text); $('events').prepend(item);
  while ($('events').children.length > 50) $('events').lastChild.remove();
}
function notice(message, error = false, owner = 'action') {
  if (owner === 'receiver' && !error && lastNotice.error && lastNotice.owner !== 'receiver') return;
  lastNotice = {error, owner};
  if ($('notice').textContent !== message) $('notice').textContent = message;
  $('notice').classList.toggle('error', error);
}
/** Keep validation feedback in the form instead of a transient browser tooltip. */
function fieldFeedback(input, message) {
  const feedback = $(`${input.id}_error`);
  if (feedback.textContent !== message) feedback.textContent = message;
  feedback.hidden = !message;
  const invalid = String(Boolean(message));
  if (input.getAttribute('aria-invalid') !== invalid) input.setAttribute('aria-invalid', invalid);
}
function attachFieldFeedback(input, wrapper) {
  const feedback = document.createElement('small');
  feedback.id = `${input.id}_error`; feedback.className = 'field-error'; feedback.hidden = true;
  wrapper.append(feedback);
  const hint = wrapper.querySelector('small:not(.field-error)');
  if (hint) hint.id = `${input.id}_hint`;
  input.setAttribute('aria-describedby', [hint?.id, feedback.id].filter(Boolean).join(' '));
}
function profileSummary() {
  const first = profileErrors.values().next().value;
  const message = first || profileRequestNotice?.message || (dirty ? 'Unsaved changes. Apply & save before starting.' : `Profile r${revision} · parameters lock during reception.`);
  const node = $('configMessage');
  if (node.textContent !== message) node.textContent = message;
  node.classList.toggle('error', Boolean(first || profileRequestNotice?.error));
  node.setAttribute('role', 'status'); node.setAttribute('aria-live', 'polite');
}
/** Reading validity never opens the browser bubble; raw integer text follows the INI parser. */
function numericFieldError(input, title, required, integer) {
  if (input.validity.badInput) return `${title}: enter a number.`;
  if (!input.value) return required ? `${title}: enter a value.` : '';
  const number = Number(input.value);
  if (!Number.isFinite(number)) return `${title}: enter a finite number.`;
  if (integer && !Number.isInteger(number)) return `${title}: enter a whole number.`;
  if (integer === 'decimal' && !/^\d+$/.test(input.value)) return `${title}: use decimal digits without a sign or exponent.`;
  if (input.min !== '' && number < Number(input.min)) return `${title}: minimum ${input.min}.`;
  if (input.max !== '' && number > Number(input.max)) return `${title}: maximum ${input.max}.`;
  if (input.validity.stepMismatch) return `${title}: use increments of ${input.step}.`;
  return '';
}
function profileFieldError(input) {
  if (!moduleForm?.active(input.name, values)) return '';
  if (input.type !== 'number') return moduleForm?.error(input.name, values) || '';
  const title = fields.find(field => field[1] === input.name)?.[2] || input.name;
  const descriptor = moduleForm?.definitions.get(input.name);
  const integer = descriptor ? (descriptor.integer ? 'decimal' : false) : input.name === 'receiver.run_seconds' ? false : ['rtlsdr.ppm','bladerf.gain_db'].includes(input.name) ? true : 'decimal';
  const message = numericFieldError(input, title, Object.hasOwn(values, input.name), integer);
  if (message) return message;
  const rate = Number(input.value);
  if (input.name === 'frontend.sample_rate_hz' && values['frontend.type'] === 'rtlsdr' && rate > 300000 && rate <= 900000) return 'RTL-SDR sample rate must be (225000,300000] or (900000,3200000] pairs/s.';
  return moduleForm?.error(input.name, values) || '';
}
function refreshProfileErrors(force = false) {
  if (force) profileValidationRequested = true;
  for (const input of $('configForm').elements) {
    if (!input.name) continue;
    if (profileValidationRequested || profileTouched.has(input.name)) {
      const message = profileFieldError(input);
      if (message) profileErrors.set(input.name, message); else profileErrors.delete(input.name);
    }
    fieldFeedback(input, profileErrors.get(input.name) || '');
  }
  profileSummary();
  return profileErrors.size === 0;
}
function clearProfileErrors() {
  profileErrors.clear(); profileTouched.clear(); profileValidationRequested = false; profileRequestNotice = null;
}
function connected() { return socket?.readyState === WebSocket.OPEN; }
function editable() { return connected() && status && ['stopped', 'error'].includes(status.state) && !httpBusy && !pendingCommand; }
function pairs(value) { try { return BigInt(value || '0').toLocaleString('en-US'); } catch { return '—'; } }
function bytes(value) { const n = Number(value); if (n < 1000) return `${n} B`; if (n < 1e6) return `${(n / 1e3).toFixed(1)} kB`; if (n < 1e9) return `${(n / 1e6).toFixed(2)} MB`; return `${(n / 1e9).toFixed(2)} GB`; }
function duration(value) { const n = Math.max(0, Math.floor(value || 0)); return `${Math.floor(n / 3600) ? `${Math.floor(n / 3600)}:` : ''}${String(Math.floor(n / 60) % 60).padStart(2, '0')}:${String(n % 60).padStart(2, '0')}`; }
function parseIni(text) {
  const parsed = {}; let group = '';
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim(); if (!line || line.startsWith('#') || line.startsWith(';')) continue;
    if (line.startsWith('[') && line.endsWith(']')) { group = line.slice(1, -1); continue; }
    const position = line.indexOf('=');
    if (!group || position < 1) throw new Error('INI requires sections and key=value lines.');
    const key = `${group}.${line.slice(0, position).trim()}`;
    if (Object.hasOwn(parsed, key)) throw new Error(`Duplicate INI key: ${key}`);
    parsed[key] = line.slice(position + 1).trim();
  }
  return parsed;
}
function toIni(object) {
  const groups = new Map();
  for (const [key, value] of Object.entries(object)) {
    if (['frontend.serial', 'recording.directory', 'replay.path'].includes(key) && !value) continue;
    const split = key.indexOf('.'), group = key.slice(0, split), name = key.slice(split + 1);
    if (!groups.has(group)) groups.set(group, []);
    groups.get(group).push(`${name}=${value}`);
  }
  return [...groups].map(([group, lines]) => `[${group}]\n${lines.join('\n')}`).join('\n\n') + '\n';
}
function dirtyState(changed) {
  dirty = changed; $('configBadge').textContent = dirty ? 'Unsaved' : `r${revision}`;
  $('configBadge').classList.toggle('dirty', dirty);
  if (changed) profileRequestNotice = null;
  profileSummary();
  $('iniEditor').value = toIni(values); controls();
}
const settingsTopics = {
  source: ['Front end', 'Choose the live SDR or recorded input, sample format, tuning and gains.', 'Before start'],
  receiver: ['Receiver & signals', 'Select GPS searches and PRNs, then configure execution, conditioning and buffers. Other constellations remain unavailable.', 'Before start'],
  acquisition: ['Acquisition', 'Configure GPS L1 C/A coarse acquisition, Doppler/code refinement and optional confirmation. Coherent processing follows the NGene 1 ms baseline.', 'GPS available'],
  tracking: ['Tracking', 'Configure coherent integration, code and carrier loops, lock criteria and correlators.', 'Module unavailable'],
  navigation: ['Navigation & PVT', 'Configure solution rate, satellite selection, smoothing and navigation corrections.', 'Module unavailable'],
  recording: ['Capture to disk', 'Choose a name, receiver-side folder and pre/post capture point.', 'Before start'],
  analysis: ['Diagnostic processing', 'NGene computes PSD and histograms. Apply these settings during reception.', 'Runtime'],
  display: ['Plot appearance', 'Adjust axes, colours and history in this browser.', 'Display only'],
  profile: ['Profile tools', 'Import, export, validate or inspect the complete receiver profile.', 'INI profile']
};
/** Show a bounded set of settings without changing the INI profile or diagnostics subscription. */
function selectSettings(topic) {
  if (!Object.hasOwn(settingsTopics, topic)) return;
  selectedSettings = topic;
  document.querySelectorAll('[data-settings]').forEach(button => { const active = button.dataset.settings === topic; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  const [title, hint, mode] = settingsTopics[topic]; $('settingsTitle').textContent = title; $('settingsHint').textContent = hint; $('settingsMode').textContent = mode;
  $('configForm').classList.toggle('hidden', !['source', 'receiver', 'acquisition', 'recording'].includes(topic) && ![...(moduleForm?.groups.values() || [])].some(g => g.topic === topic));
  const groups = {source: ['Frontend', 'HackRF', 'RTL-SDR', 'bladeRF', 'File replay'], receiver: ['Receiver', 'Buffers', 'GPS receiver'], acquisition: ['GPS acquisition', 'Acquisition refinement'], recording: ['Recording']};
  document.querySelectorAll('#fields [data-group]').forEach(group => group.classList.toggle('topic-hidden', !(groups[topic] || []).includes(group.dataset.group) && moduleForm?.groups.get(group.dataset.group)?.topic !== topic));
  $('sourcePresets').classList.toggle('hidden', topic !== 'source');
  receiverConfiguration?.select(topic);
  $('apply').disabled = !editable() || referenceSettingsOnly();
  for (const [id, key] of [['analysisSettings', 'analysis'], ['displaySettings', 'display'], ['profileSettings', 'profile']]) $(id).classList.toggle('hidden', topic !== key);
  updateSourceFields();
}
function referenceSettingsOnly() { return ['tracking', 'navigation'].includes(selectedSettings) && ![...(moduleForm?.groups.values() || [])].some(g => g.topic === selectedSettings); }
function validateProfileFields() {
  refreshProfileErrors(true);
  const invalid = [...$('configForm').elements].find(input => profileErrors.has(input.name));
  if (!invalid) return true;
  const group = invalid.closest('[data-group]')?.dataset.group;
  selectSettings(moduleForm?.groups.get(group)?.topic || (group === 'Recording' ? 'recording' : ['GPS acquisition','Acquisition refinement'].includes(group) ? 'acquisition' : ['Receiver', 'Buffers','GPS receiver'].includes(group) ? 'receiver' : 'source'));
  invalid.focus(); return false;
}
function buildForm() {
  moduleForm?.defaults(values);
  $('configForm').noValidate = true;
  const holder = $('fields'); holder.replaceChildren(); let group = '', grid;
  for (const [section, key, label, type, options, unit, hint] of fields) {
    if (section !== group) {
      const fieldset = document.createElement('fieldset'), legend = document.createElement('legend');
      fieldset.className = 'config-group'; fieldset.dataset.group = section; legend.textContent = section;
      grid = document.createElement('div'); grid.className = 'field-grid'; fieldset.append(legend);
      const description = moduleForm?.groups.get(section)?.note;
      if (description) { const p = document.createElement('p'); p.className = 'muted'; p.textContent = description; fieldset.append(p); }
      fieldset.append(grid); holder.append(fieldset); group = section;
    }
    const wrapper = document.createElement('label'), title = document.createElement('span'), units = document.createElement('span');
    wrapper.className = `field${['frontend.serial', 'frontend.center_frequency_hz', 'recording.prefix', 'recording.directory', 'recording.point', 'recording.enabled', 'buffer.overflow_policy', 'replay.path'].includes(key) ? ' full' : ''}`;
    title.textContent = label; units.className = 'unit'; units.textContent = unit || ''; title.append(units); wrapper.append(title);
    const nativeBandwidth = key === 'hackrf.bandwidth_hz' && values['frontend.type'] !== 'hackrf';
    const input = document.createElement(type === 'select' && !nativeBandwidth ? 'select' : 'input'); input.name = key;
    if (type === 'select' && !nativeBandwidth) for (const option of (key === 'rtlsdr.gain_tenths_db' ? (deviceCaps?.tuner_gains_tenths_db || [values[key] || '0']) : options)) { const element = document.createElement('option'); element.value = typeof option === 'object' ? option.value : option; element.textContent = typeof option === 'object' ? option.label : option === 'true' ? 'On' : option === 'false' ? 'Off' : option === 'record' ? 'Record only' : option === 'capture' ? 'Capture' : option; input.append(element); }
    else { input.type = nativeBandwidth ? 'number' : type; if(input.type==='number'){input.step='any';input.min='0'; if(key==='conditioning.dc_shift'){input.min='4';input.max='24';input.step='1';} if(key==='frontend.sample_rate_hz'){input.min='1';input.max='20000000';input.step='1';} if(key.endsWith('frequency_hz')){input.max='6000000000';input.step='1';} } input.inputMode = type === 'number' ? 'decimal' : 'text'; input.autocomplete = 'off'; }
    const limits = {'conditioning.dc_shift':[4,24], 'buffer.window_samples':[1,1048576], 'buffer.ingress_blocks':[2,4096], 'buffer.ring_blocks':[2,4096], 'recording.queue_blocks':[2,4096], 'receiver.idle_timeout_ms':[100,3600000], 'receiver.run_seconds':[0,31536000], 'rtlsdr.ppm':[-1000,1000], 'bladerf.gain_db':[-100,100]};
    if (input.type === 'number') { input.step = key === 'receiver.run_seconds' ? 'any' : '1'; if (limits[key]) [input.min,input.max] = limits[key]; }
    const descriptor = moduleForm?.definitions.get(key);
    if (descriptor && input.type === 'number') {
      input.step = descriptor.integer ? '1' : 'any';
      input.min = descriptor.min ?? ''; input.max = descriptor.max ?? '';
    }
    input.id = `config_${key.replaceAll('.', '_')}`;
    input.value = values[key] ?? ''; input.addEventListener('input', () => {
      values[key] = input.value.trim();
      if (key === 'frontend.type') profileTouched.add('frontend.sample_rate_hz');
      dirtyState(true); updateSourceFields(); refreshProfileErrors();
    });
    input.addEventListener('blur', () => { profileTouched.add(key); refreshProfileErrors(); });
    wrapper.append(input); if (hint) { const small = document.createElement('small'); small.textContent = hint; wrapper.append(small); } grid.append(wrapper);
    attachFieldFeedback(input, wrapper);
  }
  selectSettings(selectedSettings); controls(); refreshProfileErrors();
}
function updateSourceFields() {
  const source = values['frontend.type'];
  const caps = deviceCaps && deviceCaps.source === source && deviceCaps.serial === (values['frontend.serial'] || '') && deviceCaps.tuning === values['frontend.center_frequency_hz'] && (source !== 'bladerf' || deviceCaps.precision === values['bladerf.format']) ? deviceCaps : null;
  if (!caps) deviceCaps = null;
  const rate=document.querySelector('[name="frontend.sample_rate_hz"]');
  if(rate) { rate.min=caps?.sample_rate_min_hz || (source==='hackrf'?2000000:source==='rtlsdr'?225001:1); rate.max=caps?.sample_rate_max_hz || (source==='hackrf'?20000000:source==='rtlsdr'?3200000:61440000); }
  const gain=document.querySelector('[name="bladerf.gain_db"]');
  if(gain){gain.min=caps?.gain_min_db??-100;gain.max=caps?.gain_max_db??100;gain.step=caps?.gain_step_db||1;gain.disabled=!editable()||source==='bladerf'&&!caps;}
  const rtlGain=document.querySelector('[name="rtlsdr.gain_tenths_db"]');
  if(rtlGain)rtlGain.disabled=!editable()||values['rtlsdr.auto_gain']!=='false'||!caps;
  const ppm=document.querySelector('[name="rtlsdr.ppm"]');if(ppm){ppm.min=-1000;ppm.max=1000;ppm.step=1;}
  const tuning=document.querySelector('[name="frontend.center_frequency_hz"]');
  if(tuning){tuning.min=caps?.frequency_min_hz||0;tuning.max=caps?.frequency_max_hz||(source==='rtlsdr'?4294967295:6000000000);}
  for (const group of document.querySelectorAll('[data-group]')) {
    const kind = {'HackRF':'hackrf','RTL-SDR':'rtlsdr','bladeRF':'bladerf','File replay':'file'}[group.dataset.group];
    group.classList.toggle('hidden', Boolean(kind && source !== kind));
  }
}

function controls() {
  const ready = editable(), running = connected() && status?.state === 'running';
  const commandPending = httpBusy || pendingCommand !== null;
  for(const id of ['showSpectrum','showWaterfall'])$(id).disabled=commandPending||['starting','stopping'].includes(status?.state);
  for (const field of $('configForm').elements) field.disabled = !ready;
  for (const id of ['browseReplay', 'filePreset', 'hackrfPreset', 'rtlPreset', 'bladePreset', 'deviceLimits', 'importIni', 'useIni', 'iniEditor', 'apply']) $(id).disabled = !ready;
  $('apply').disabled = !ready || referenceSettingsOnly();
  if (capabilities && !capabilities.hackrf) $('hackrfPreset').disabled = true;
  $('rtlPreset').disabled = !ready || !capabilities?.rtlsdr;
  $('bladePreset').disabled = !ready || !capabilities?.bladerf;
  updateSourceFields();
  const sourceSelect=document.querySelector('[name="frontend.type"]'); if(sourceSelect){for(const option of sourceSelect.options)option.disabled=option.value!=='file' && !capabilities?.[option.value];}
  moduleForm?.update($('configForm'), values, ready);
  $('validate').disabled = !connected() || httpBusy;
  $('start').disabled = !ready || dirty || (values['frontend.type']==='file' && !values['replay.path']);
  $('stop').disabled = !connected() || !['running', 'starting'].includes(status?.state) || httpBusy;
  $('recordStart').disabled = !running || commandPending || status.recording_state !== 'off';
  $('recordStop').disabled = !running || commandPending || status.recording_state !== 'recording';
  $('exit').disabled = !connected() || httpBusy;
  for (const input of $('analysisForm').elements) input.disabled = !connected() || httpBusy || pendingCommand !== null || ['starting', 'stopping'].includes(status?.state);
  receiverWorkspace?.update({status, capabilities, profile:savedValues, draft:values, revision, dirty, connected:connected(), editable:ready});
}
async function api(path, body) {
  const options = body === undefined ? {} : {method: 'POST', headers: {'Content-Type': 'application/json', 'X-NGene-Token': token}, body: JSON.stringify(body)};
  const response = await fetch(`/api/v1/${path==='bootstrap'||path==='shutdown'?'':sessionPrefix()}${path}`, options);
  const data = await response.json(); if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`); return data;
}
async function loadConfig(force = false) {
  const data = await api('config');
  if (!dirty || force) { revision = data.revision; analysisConfigRevision=revision; analysisConfig=null; savedValues = parseIni(data.ini); values = {...savedValues}; clearProfileErrors(); buildForm(); if (!analysisDirty) buildAnalysisForm(); dirtyState(false); selectSettings(selectedSettings); syncPlotVisibility(); }
  else if (data.revision !== revision) { notice('The server configuration changed. Export your draft, then reload this page before applying.', true); }
}
async function applyConfig(validateOnly = false) {
  if(!validateProfileFields())return;
  httpBusy = true; controls();
  try {
    const result = await api(validateOnly ? 'config/validate' : 'config', {revision, ini: toIni(values)});
    if (!validateOnly) { revision = result.revision; analysisConfigRevision=revision; analysisConfig=null; savedValues = parseIni(result.ini); values = {...savedValues}; clearProfileErrors(); buildForm(); if (!analysisDirty) buildAnalysisForm(); dirtyState(false); selectSettings(selectedSettings); syncPlotVisibility(); }
    const message = validateOnly ? 'Configuration is valid. Apply & save to use it.' : `Profile r${revision} saved on the receiver. Recording folder applied.`;
    profileRequestNotice = {message,error:false}; profileSummary(); notice(message); log(validateOnly ? 'Configuration validated.' : `Configuration r${revision} saved.`);
  } catch (error) { profileRequestNotice = {message:error.message,error:true}; profileSummary(); notice(error.message, true); log(error.message, true); }
  finally { httpBusy = false; controls(); }
}
async function command(path) {
  httpBusy = true; controls();
  try { const result = await api(path, {revision}); pendingCommand = result.command_id; notice('Command accepted. Waiting for NGene…'); requestStatus(); }
  catch (error) { notice(error.message, true); log(error.message, true); }
  finally { httpBusy = false; controls(); }
}
function requestStatus() {
  // Space requests after receiving the previous reply, even if that request was delayed.
  // Retain explicit requests across this pacing guard so disabling a plot does not wait
  // for the selected display refresh interval before notifying the receiver.
  statusRequestPending=true;
  const now = performance.now();
  if (!connected() || waiting || now-lastRequest < 34 || now-lastResponse < 34) return;
  statusRequestPending=false;waiting = true; lastRequest = performance.now(); socket.send(`status:${diagnosticInterest()}`);
}
function renderStatus(next) {
  status = next;
  if (pendingCommand && BigInt(next.completed_command_id) >= BigInt(pendingCommand)) pendingCommand = null;
  if (lastRun !== next.run_id) { lastSample = null; lastRun = next.run_id; resetAnalysis(); }
  $('runState').textContent = next.state; $('runState').className = `state ${next.state}`;
  $('revision').textContent = next.active_revision === '0' ? 'Not started yet' : `Active profile r${next.active_revision}`;
  const source = savedValues['frontend.type'] || '—', fs = Number(savedValues['frontend.sample_rate_hz'] || 0), rf = Number(savedValues['frontend.center_frequency_hz'] || 0);
  const hardware=next.hardware_frequency_hz||rf, signal=next.signal_frequency_hz||Number(savedValues['frontend.signal_frequency_hz'])||hardware, actual=next.actual_sample_rate_hz||fs;
  const sourceName={hackrf:'HackRF',rtlsdr:'RTL-SDR',bladerf:'bladeRF',file:'Recorded I/Q'}[source]||source;
  $('streamDescription').textContent = `${sourceName} · Signal ${(signal/1e6).toFixed(6)} MHz · SDR ${(hardware/1e6).toFixed(6)} MHz · Digital IF ${((signal-hardware)/1e6).toFixed(6)} MHz · Fs ${(actual/1e6).toFixed(6)} Msps (configured nominal) · ${next.sample_format||'Unknown format'}`;
  $('processed').textContent = pairs(next.processed_samples); $('windows').textContent = `${pairs(next.windows)} common windows`;
  let rate = null;
  if (lastSample && next.wall_seconds > lastSample.time) rate = Number(BigInt(next.received_samples) - lastSample.samples) / (next.wall_seconds - lastSample.time);
  if (!lastSample || next.wall_seconds !== lastSample.time) lastSample = {samples:BigInt(next.received_samples), time:next.wall_seconds};
  $('rate').textContent = next.state !== 'running' ? '—' : rate === null ? $('rate').textContent : `${(Math.max(0, rate) / 1e6).toFixed(3)} Msps`;
  $('elapsed').textContent = `Elapsed ${duration(next.wall_seconds)}`;
  $('written').textContent = bytes(BigInt(next.written_samples) * (next.sample_format==='SC16_Q11'?4n:2n));
  $('recordState').textContent = next.recording_state === 'recording' ? 'Recording active' : `Recording ${next.recording_state}`;
  $('dropped').textContent = pairs(next.known_dropped_samples); $('gaps').textContent = `${pairs(next.gap_events)} gap events`;
  $('inputQueue').textContent = `${pairs(next.input_queued_samples)} pairs`; $('recordQueue').textContent = `${pairs(next.recording_queued_samples)} pairs`;
  $('inputProgress').value = next.ingress_available ? Number(next.ingress_occupancy_percent) : 0;
  $('recordProgress').value = 100 * Number(next.recording_queued_samples) / (Number(savedValues['recording.queue_blocks']) * Number(savedValues['buffer.window_samples']) || 1);
  $('bandwidth').textContent = next.actual_bandwidth_hz ? `${(next.actual_bandwidth_hz / 1e6).toFixed(2)} MHz` : 'Not applicable / not started';
  $('ringHigh').textContent = `${next.ring_high_water} blocks`; $('discarded').textContent = pairs(next.input_discarded_on_stop);
  $('ingressStatus').textContent=next.ingress_available ?
    `${next.ingress_used_blocks}/${next.ingress_capacity_blocks} blocks · ${Number(next.ingress_occupancy_percent).toFixed(1)}% · high-water ${next.ingress_high_water_blocks} · free buffering ${Number(next.ingress_available_ms).toFixed(2)} ms · overflow events ${pairs(next.ingress_overflow_events)} · transport errors ${pairs(next.transport_errors)}` : 'No ingress queue for file replay / receiver stopped.';
  $('deviceIntegrity').textContent=`Device continuity: ${next.device_continuity_state}${next.device_continuity_state==='valid'?` · gaps ${pairs(next.device_gap_events)} · timestamp-derived missing pairs ${pairs(next.device_missing_samples)}`:''}. Hardware-only loss: ${next.hardware_loss_state}. Native format: ${next.sample_format}.`;
  $('health').textContent = next.state === 'running' ? (next.known_dropped_samples !== '0' ? 'Losses detected' : 'Streaming') : next.state;
  $('recordBadge').textContent = next.recording_state; $('sessionName').textContent = next.recording_name || 'No recording yet';
  $('sessionSamples').textContent = pairs(next.session_written_samples); $('segments').textContent = next.recording_segments;
  $('outputDirectory').textContent = next.output_directory;
  if (next.message !== lastMessage) { lastMessage = next.message; notice(next.message, !next.command_ok || next.state === 'error', 'receiver'); log(next.message, !next.command_ok || next.state === 'error'); }
  if (next.completed_command_id !== lastCommand) { lastCommand = next.completed_command_id; refreshFiles(); }

  renderAnalysis(next.analysis,next.revision);

  if (next.revision !== revision && !dirty) loadConfig().catch(error => log(error.message, true));
  if (performance.now() - lastFileRefresh > 4000) refreshFiles();
  controls();
}
async function refreshFiles() {
  if (fileBusy || !token) return; const fileSession = selectedSession; fileBusy = true; lastFileRefresh = performance.now();
  try {
    const result = await api('files'); if (fileSession !== selectedSession) return; $('files').replaceChildren();
    result.files.sort((a, b) => b.name.localeCompare(a.name));
    for (const file of result.files) {
      const row = document.createElement('tr');
      for (const content of [file.name, bytes(file.bytes), file.closed ? 'Closed' : 'Writing']) { const cell = document.createElement('td'); cell.textContent = content; row.append(cell); }
      const cell = document.createElement('td');
      if (file.closed) { const link = document.createElement('a'); link.textContent = 'Transfer ↓'; link.title = 'Download to the computer running this browser'; link.href = `/api/v1/${sessionPrefix()}files/${encodeURIComponent(file.name)}`; link.download = file.name; cell.append(link); }
      else cell.textContent = '—'; row.append(cell);
      const replayCell = document.createElement('td');
      if (['.cs8','.cu8','.sc16q11'].some(ext=>file.name.endsWith(ext))) {
        const play = document.createElement('button'); play.textContent = 'Replay';
        play.disabled = !file.closed || BigInt(file.bytes) === 0n;
        play.title = 'Select this file as the receiver input; stop reception first';
        play.addEventListener('click', async () => {
          try {
            if (!editable()) throw new Error('Stop the receiver before choosing a recorded file.');
            await openReplayPicker(result.directory, `${result.directory}/${file.name}`);
          } catch (error) { notice(error.message, true); }
        }); replayCell.append(play);
      } else replayCell.textContent = '—';
      row.append(replayCell);
      const manage = document.createElement('td'), remove = document.createElement('button');
      remove.className = 'danger delete-file'; remove.textContent = 'Delete'; remove.dataset.filename = file.name;
      remove.disabled = !file.closed || deletingFile; remove.title = file.closed ? 'Delete this file on the receiver' : 'Stop recording before deleting this file';
      remove.addEventListener('click', () => deleteFile(file.name, result.directory, result.revision)); manage.append(remove); row.append(manage); $('files').append(row);
    }
    if (!result.files.length) { const row = document.createElement('tr'), cell = document.createElement('td'); cell.colSpan = 6; cell.className = 'empty-table'; cell.textContent = 'No recordings yet. Start the receiver, then start recording.'; row.append(cell); $('files').append(row); }
    $('fileNote').textContent = result.truncated ? 'Showing at most 200 files. All recordings remain on the receiver disk.' : 'Closed means no longer being written. Check capture metadata for completion and gap information.';
  } catch (error) { log(`File list: ${error.message}`, true); }
  finally { fileBusy = false; }
}
/** Delete the explicitly confirmed closed file; server validates directory revision and active recording. */
async function deleteFile(name, directory, fileRevision) {
  if (deletingFile) return;
  const dialog = $('deleteFileDialog'); $('deleteFileName').textContent = name; $('deleteFileDirectory').textContent = directory;
  deletingFile = true;
  // Resolve the user's action synchronously; a queued close event from an earlier
  // dialog must not cancel a newly opened confirmation.
  const choice = await new Promise(resolve => {
    const finish = value => {
      dialog.removeEventListener('submit', submit);
      dialog.removeEventListener('cancel', cancel);
      dialog.close(value); resolve(value);
    };
    const submit = event => { event.preventDefault(); finish(event.submitter?.value || 'cancel'); };
    const cancel = event => { event.preventDefault(); finish('cancel'); };
    dialog.addEventListener('submit', submit); dialog.addEventListener('cancel', cancel);
    dialog.showModal();
  });
  if (choice !== 'delete') { deletingFile = false; return; }
  document.querySelectorAll('.delete-file').forEach(button => button.disabled = true);
  try { await api('files/delete', {name, revision: fileRevision}); notice(`Deleted ${name} from the receiver.`); log(`File deleted: ${name}`); }
  catch (error) { notice(error.message, true); log(`Delete: ${error.message}`, true); }
  finally { deletingFile = false; await refreshFiles(); }
}
async function connect() {
  clearTimeout(reconnectTimer); if (quitting) return;
  try {
    capabilities = await api('bootstrap'); token = capabilities.token;
    $('platform').textContent = `${capabilities.os} / ${capabilities.architecture}`; $('version').textContent = `v${capabilities.version}`;
    if (!moduleForm) {
      moduleForm = moduleParameters(await api('module-schema'));
      fields.push(...moduleForm.fields);
      receiverConfiguration = createReceiverConfiguration($('gnssSettings'), moduleForm.references);
    }
    await loadConfig();
    socket = new WebSocket(`${location.protocol === 'https:' ? 'wss:' : 'ws:'}//${location.host}/api/v1/events?token=${encodeURIComponent(token)}`);
    socket.onopen = () => { lastResponse=-Infinity; $('connection').textContent = 'Connected'; $('connection').classList.add('online'); waiting = false; log('Connected to NGene.'); requestStatus(); refreshFiles(); };
    socket.onmessage = event => { waiting=false; lastResponse=performance.now(); try { pendingTelemetry=JSON.parse(event.data); if(!renderPending){renderPending=true;requestAnimationFrame(()=>{renderPending=false;const latest=pendingTelemetry;pendingTelemetry=null;if(latest)renderStatus(latest);});} } catch(error){log(`Invalid telemetry: ${error.message}`,true);} };
    socket.onclose = () => { waiting = false; pendingCommand = null; $('connection').textContent = quitting ? 'Service closed' : 'Disconnected'; $('connection').classList.remove('online'); controls(); if (!quitting) { notice('Connection lost. Receiver state is unknown; reconnecting…', true); reconnectTimer = setTimeout(connect, 1500); } };
    socket.onerror = () => socket.close();
  } catch (error) { $('connection').textContent = 'Disconnected'; notice(error.message, true); controls(); reconnectTimer = setTimeout(connect, 2000); }
}
const analysisFields = [
  ['spectrum_enabled','Spectrum enabled',['true','false']],
  ['histogram_enabled','Histogram enabled',['true','false']],
  ['spectrum_source','Spectrum source',['pre','post']],
  ['histogram_source','Histogram source',['pre','post']],
  ['fft_size','FFT size',[256,512,1024,2048,4096,8192,16384]],
  ['window','Window',['rectangular','hann','hamming','blackman','blackman_harris']],
  ['overlap_percent','FFT overlap (%)',null,0,99,1],
  ['averages','Welch segments',null,1,64,1],
  ['update_hz','Spectrum rate limit (Hz)',null,0.1,30,0.1],
  ['units','Spectrum units',['dbfs_bin','dbfs_hz']],
  ['temporal','Temporal power averaging',['none','mean','ema']],
  ['temporal_frames','Temporal mean frames',null,1,16,1],
  ['smoothing_alpha','EMA newest-frame coefficient',null,0.001,1,0.001],
  ['histogram_update_hz','Histogram rate limit (Hz)',null,0.1,30,0.1],
  ['histogram_pairs','Pairs per histogram',null,1024,262144,1],
  ['histogram_bins','Histogram bins',[16,32,64,128,256]]
];
let analysisDirty=false, analysisSnapshot=null, analysisSequence='0', analysisRevision='', waterfallHistory=[];
let waterfallTime=-Infinity, skippedDisplay=0, rejectedFrames=0, histogramSequence='', analysisConfig=null;
let analysisConfigRevision='0', plotConfigBusy=false, lastSpectrumComputation=null;
let previousPlotSelection={spectrum:true,waterfall:true};
function diagnosticVisibility(){return document.hidden||activeWorkspaceView!=='rf'?0:(($('showSpectrum').checked||$('showWaterfall').checked)?1:0)|($('showHistogram').checked?2:0);}
function diagnosticEnabled(name){const value=analysisConfig?.[name]??savedValues[`analysis.${name}`];return value!==false&&value!=='false';}
function plotEnabled(id){return diagnosticEnabled('spectrum_enabled')&&$(id==='spectrum'?'showSpectrum':'showWaterfall').checked;}
function syncPlotVisibility(){
  const computation=diagnosticEnabled('spectrum_enabled');
  if(!plotConfigBusy&&computation!==lastSpectrumComputation){
    if(!computation){
      if($('showSpectrum').checked||$('showWaterfall').checked)previousPlotSelection={spectrum:$('showSpectrum').checked,waterfall:$('showWaterfall').checked};
      $('showSpectrum').checked=false;$('showWaterfall').checked=false;
    }else if(lastSpectrumComputation===false&&!$('showSpectrum').checked&&!$('showWaterfall').checked){
      $('showSpectrum').checked=previousPlotSelection.spectrum;$('showWaterfall').checked=previousPlotSelection.waterfall;
    }
    lastSpectrumComputation=computation;
  }
  for(const id of ['spectrum','waterfall']){
    const enabled=plotEnabled(id),message=$(`${id}Disabled`);
    $(`${id}Body`).classList.toggle('hidden',!enabled);
    message.classList.toggle('hidden',enabled);
    const text=computation?`${id==='spectrum'?'Spectrum':'Waterfall'} disabled.`:'FFT computation is off. Use Enable to restart it.';
    if(message.textContent!==text)message.textContent=text;
  }
}
function diagnosticMask(){
  const visible=diagnosticVisibility();
  return (visible&1&&diagnosticEnabled('spectrum_enabled')?1:0)|(visible&2&&diagnosticEnabled('histogram_enabled')?2:0);
}
function diagnosticInterest(){return paused?0:diagnosticMask();}
/** Apply current server settings without replacing unrelated profile or diagnostic drafts. */
function acceptAnalysisConfig(result){
  const iniDraft=$('iniEditor').value,customIni=iniDraft!==toIni(values);
  revision=result.revision;analysisConfigRevision=revision;
  for(const [name,value] of Object.entries(result.analysis)){
    const key=`analysis.${name}`,previous=savedValues[key],next=String(value),input=$(`analysis_${name}`);
    if(name==='spectrum_enabled'||values[key]===previous)values[key]=next;
    if(input&&(name==='spectrum_enabled'||input.value===previous))input.value=next;
    savedValues[key]=next;
  }
  analysisDirty=analysisFields.some(([name])=>$(`analysis_${name}`)?.value!==savedValues[`analysis.${name}`]);
  analysisErrors.delete('spectrum_enabled');fieldFeedback($('analysis_spectrum_enabled'),'');
  dirtyState(true);if(customIni)$('iniEditor').value=iniDraft;resetAnalysis(result.analysis);
}
async function changePlotEnabled(event){
  const id=event.target.id,selection={spectrum:$('showSpectrum').checked,waterfall:$('showWaterfall').checked};
  const before={...selection,[id==='showSpectrum'?'spectrum':'waterfall']:!event.target.checked};
  const previousSelection={...previousPlotSelection},enabled=selection.spectrum||selection.waterfall;
  if(enabled)previousPlotSelection={...selection};
  $('plotNotice').classList.add('hidden');
  syncPlotVisibility();requestStatus();
  if(enabled===diagnosticEnabled('spectrum_enabled'))return;
  plotConfigBusy=true;httpBusy=true;controls();
  try{
    // The endpoint accepts a complete analysis configuration. Use applied server
    // settings rather than sending any unsaved form fields with this one toggle.
    const current=await api('analysis/config');
    const result=current.analysis.spectrum_enabled===enabled?current:await api('analysis/config',{revision:current.revision,...current.analysis,spectrum_enabled:enabled});
    acceptAnalysisConfig(result);
    log(`Spectrum FFT computation ${enabled?'enabled':'disabled'} for this session.`);
  }catch(error){
    $('showSpectrum').checked=before.spectrum;$('showWaterfall').checked=before.waterfall;previousPlotSelection=previousSelection;
    $('plotNotice').textContent=`Cannot change FFT computation: ${error.message}`;$('plotNotice').classList.remove('hidden');
    log(error.message,true);
  }finally{plotConfigBusy=false;httpBusy=false;syncPlotVisibility();controls();requestStatus();}
}
function buildAnalysisForm() {
  $('analysisForm').noValidate = true;
  analysisErrors.clear(); analysisTouched.clear(); analysisValidationRequested = false;
  setAnalysisNotice(analysisHelp);
  $('analysisFields').replaceChildren();
  for (const [name,title,options,min,max,step] of analysisFields) {
    const label=document.createElement('label');label.className='field';
    const span=document.createElement('span');span.textContent=title;label.append(span);
    const input=document.createElement(options?'select':'input');input.id=`analysis_${name}`;input.name=name;
    if(options)for(const value of options){const option=document.createElement('option');option.value=String(value);option.textContent=String(value);input.append(option);}
    else {input.type='number';input.min=min;input.max=max;input.step=step;}
    input.value=savedValues[`analysis.${name}`] ?? '';input.addEventListener('input',()=>{analysisDirty=true;setAnalysisNotice(analysisHelp);refreshAnalysisErrors();});
    input.addEventListener('blur',()=>{analysisTouched.add(name);refreshAnalysisErrors();});label.append(input);attachFieldFeedback(input,label);$('analysisFields').append(label);
  }
  syncPlotVisibility();controls();
}
function setAnalysisNotice(message, error = false) {
  const node = $('analysisNotice');
  if (node.textContent !== message) node.textContent = message;
  node.classList.toggle('error', error); node.setAttribute('role', 'status'); node.setAttribute('aria-live', 'polite');
}
function refreshAnalysisErrors(force = false) {
  if (force) analysisValidationRequested = true;
  for (const [name,title,options] of analysisFields) {
    const input = $(`analysis_${name}`);
    if (analysisValidationRequested || analysisTouched.has(name)) {
      const message = options ? (input.value ? '' : `${title}: choose a value.`) : numericFieldError(input,title,true,input.step==='1');
      if (message) analysisErrors.set(name,message); else analysisErrors.delete(name);
    }
    fieldFeedback(input,analysisErrors.get(name)||'');
  }
  const first = analysisErrors.values().next().value;
  if (first) setAnalysisNotice(first,true);
  return analysisErrors.size===0;
}
function showView(configuration) {
  receiverWorkspace?.setView(typeof configuration === 'string' ? configuration : configuration ? 'setup' : 'rf');
}
$('analysisForm').addEventListener('submit',async event=>{
  event.preventDefault();
  if(!refreshAnalysisErrors(true)){ $(`analysis_${analysisErrors.keys().next().value}`).focus();return; }
  const body={revision};
  for(const [name] of analysisFields){const value=$(`analysis_${name}`).value;body[name]=name.endsWith('_enabled')?value==='true':['window','temporal','units','spectrum_source','histogram_source'].includes(name)?value:Number(value);}
  httpBusy=true;controls();
  try {
    const result=await api('analysis/config',body);revision=result.revision;analysisConfigRevision=revision;
    for(const [name,value] of Object.entries(result.analysis)){savedValues[`analysis.${name}`]=String(value);values[`analysis.${name}`]=String(value);}
    analysisDirty=false;dirtyState(true);resetAnalysis(result.analysis);
    setAnalysisNotice('Applied to this session. Use Apply & save in Receiver configuration while stopped to persist the full profile.');
    log('Analysis parameters applied without restarting the receiver.');requestStatus();
  } catch(error){setAnalysisNotice(error.message,true);log(error.message,true);}
  finally{httpBusy=false;controls();}
});
function resetAnalysis(config=null){analysisSnapshot=null;analysisConfig=config;analysisSequence='0';histogramSequence='';analysisRevision='';waterfallHistory=[];waterfallTime=-Infinity;skippedDisplay=0;rejectedFrames=0;syncPlotVisibility();drawAnalysis();}
function psdIntegrity(a){
  if(!Array.isArray(a.dbfs)||a.dbfs.length>1024||a.dbfs.length!==Math.min(a.fft_size,1024))return false;
  let hash=2166136261,sum=0,min=Infinity,max=-Infinity;
  for(const x of a.dbfs){
    if(!Number.isFinite(x)||x < -1000||x > 1000)return false;
    sum+=x;min=Math.min(min,x);max=Math.max(max,x);
    let word=Math.floor(x*1000+0.5)>>>0;
    for(let j=0;j<4;j++){hash=Math.imul(hash^(word&255),16777619)>>>0;word>>>=8;}
  }
  return a.integrity_ok&&a.nan_count===0&&a.inf_count===0&&hash===a.checksum&&hash===a.telemetry_checksum&&
    Math.abs(min-a.min)<0.00001&&Math.abs(max-a.max)<0.00001&&Math.abs(sum/a.dbfs.length-a.mean)<0.00001;
}
function renderAnalysis(a,configRevision=null){
  if(!a||plotConfigBusy||configRevision!==null&&BigInt(configRevision)<BigInt(analysisConfigRevision))return;
  if(configRevision!==null)analysisConfigRevision=configRevision;
  analysisConfig=a.config;
  syncPlotVisibility();
  if(paused)return;
  const mask=diagnosticMask();
  if(!mask)return;
  if(a.sequence==='0'){if(analysisSequence!=='0')resetAnalysis(a.config);return;}
  if(a.revision!==analysisRevision){waterfallHistory=[];waterfallTime=-Infinity;analysisSequence='0';histogramSequence='';analysisRevision=a.revision;}
  const incoming=a.sequence_id||'0';
  const spectrumChanged=Boolean(mask&1)&&incoming!==analysisSequence&&a.dbfs.length>0;
  const histogramIncoming=a.histogram_sequence??`${a.histogram_first_sample}:${a.histogram_count}`;
  const histogramChanged=Boolean(mask&2)&&histogramIncoming!==histogramSequence;
  let waterfallChanged=false;
  if(spectrumChanged){
    if(BigInt(incoming)<BigInt(analysisSequence)||!psdIntegrity(a)){
      rejectedFrames++;$('frameIntegrity').textContent=`Rejected invalid/stale PSD: ${rejectedFrames}`;return;
    }
  }
  if(!spectrumChanged&&!histogramChanged)return;
  if(spectrumChanged){
    if(analysisSequence!=='0')skippedDisplay+=Number(BigInt(incoming)-BigInt(analysisSequence)-1n);
    analysisSequence=incoming;
    if(plotEnabled('waterfall')&&a.produced_seconds-waterfallTime>=1/Math.max(0.1,Math.min(30,Number($('waterfallRate').value)))){
      waterfallHistory.unshift({values:Object.freeze([...a.dbfs]),first:a.first_sample,epoch:a.epoch,sequence:incoming,produced:a.produced_seconds});
      waterfallHistory.length=Math.min(waterfallHistory.length,historyRows());waterfallTime=a.produced_seconds;
      waterfallChanged=true;
    }
  }
  analysisSnapshot={...a,dbfs:spectrumChanged?[...a.dbfs]:(analysisSnapshot?.dbfs||[]),
    histogram_i:histogramChanged?a.histogram_i:(analysisSnapshot?.histogram_i||[]),histogram_q:histogramChanged?a.histogram_q:(analysisSnapshot?.histogram_q||[])};
  const unit=a.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin';
  if(spectrumChanged){
    $('fftBackend').textContent=a.backend||'Integer FFT';
    if(plotEnabled('spectrum')){
    $('spectrumInfo').textContent=`${a.fft_size}-point FFT · Δf ${a.bin_hz.toFixed(2)} Hz · ENBW ${a.enbw_hz.toFixed(2)} Hz · ${a.config.window} · ${a.config.averages} Welch segments · ${a.config.spectrum_source}`;
    $('spectrumPeak').textContent=`Peak ${displayFrequency(a.peak_hz).toFixed(4)} ${frequencyUnit()} · ${a.peak_dbfs.toFixed(1)} ${unit}`;
    }
    $('frameIntegrity').textContent=`PSD #${incoming} · produced ${a.produced_seconds.toFixed(3)} s · min/max/mean ${a.min.toFixed(2)} / ${a.max.toFixed(2)} / ${a.mean.toFixed(2)} · NaN/Inf ${a.nan_count}/${a.inf_count} · hash ${a.checksum.toString(16)} · rejected ${rejectedFrames}`;
    drawSpectrum();if(waterfallChanged)drawWaterfall();
  }
  if(histogramChanged){
    histogramSequence=histogramIncoming;
    $('histogramInfo').textContent=a.histogram_count?`${pairs(a.histogram_count)} pairs · ${a.histogram_i.length} bins · ${a.config.histogram_source}`:'Histogram disabled / waiting';
    for(const [id,t] of [['statsI',a.stats_i],['statsQ',a.stats_q]]){
      const dl=document.createElement('dl');
      for(const [label,value] of [['Mean',t.mean],['Std',t.std],['RMS',t.rms],['Min',t.min],['Max',t.max],['Clipping / rail count',t.clipping_count],['Zero count',t.zero_count]]){
        const div=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=label;dd.textContent=Number.isInteger(value)?pairs(value):value.toFixed(4);div.append(dt,dd);dl.append(div);
      }$(id).replaceChildren(dl);
    }
    $('dcMagnitude').textContent=`DC magnitude √(mean I² + mean Q²): ${a.dc_magnitude.toFixed(4)} counts · histogram sample ${a.histogram_first_sample} · age ${a.histogram_age_seconds.toFixed(2)} s`;
    $('histogramRails').textContent=`Clipping indicator (${a.histogram_min ?? -128} / ${a.histogram_max ?? 127}): I ${pairs(a.clipped_i)} · Q ${pairs(a.clipped_q)}`;
    drawHistogram('histogramI',analysisSnapshot.histogram_i,'#39d5b0');drawHistogram('histogramQ',analysisSnapshot.histogram_q,'#62a6ff');
  }
  $('analysisCost').textContent=`Worker ${a.compute_ms.toFixed(2)} ms · ${a.sample_backend} · skipped bursts ${a.skipped_bursts} · contended blocks ${a.contention_blocks}`;
}
function historyRows(){return Math.max(32,Math.min(2048,Math.floor(Number($('waterfallRows').value)||128)));}
function frequencyUnit(){return $('frequencyReference').value==='rf'?'MHz':'kHz';}
function displayFrequency(bin){const ref=$('frequencyReference').value,hardware=status?.hardware_frequency_hz||Number(savedValues['frontend.center_frequency_hz'])||0,di=status?.digital_if_hz||0;return ref==='rf'?(hardware+bin)/1e6:ref==='signal'?(bin-di)/1000:bin/1000;}
function frequencyRange(){
  const fs=analysisSnapshot?.sample_rate_hz||Number(savedValues['frontend.sample_rate_hz'])||100000;
  let lo=displayFrequency(-fs/2),hi=displayFrequency(fs/2),l=$('frequencyMin').value,h=$('frequencyMax').value;
  if(l!==''&&h!==''&&Number.isFinite(Number(l))&&Number.isFinite(Number(h))&&Number(l)<Number(h)){lo=Math.max(lo,Number(l));hi=Math.min(hi,Number(h));}
  if(lo>=hi)return [displayFrequency(-fs/2),displayFrequency(fs/2)];return [lo,hi];
}
function surface(id){
  const canvas=$(id),rect=canvas.getBoundingClientRect(),w=rect.width,h=rect.height;
  if(w<5||h<5)return null;
  const d=Math.min(devicePixelRatio||1,2);canvas.width=Math.round(w*d);canvas.height=Math.round(h*d);
  const ctx=canvas.getContext('2d');ctx.scale(d,d);ctx.fillStyle='#0c1520';ctx.fillRect(0,0,w,h);ctx.font='11px system-ui';return {ctx,w,h,left:52,right:w-18,top:16,bottom:h-32};
}
function range(colour=false){
  const lo=Math.max(-250,Number($(colour?'colourMin':'dbMin').value)),hi=Math.min(100,Number($(colour?'colourMax':'dbMax').value));
  if(!Number.isFinite(lo)||!Number.isFinite(hi)||lo>=hi)return [-100,0];
  return [lo,hi];
}
function frequencyAxes(s){const {ctx,w,h,left,right}=s,[lo,hi]=frequencyRange();ctx.fillStyle='#8fa6ba';for(let j=0;j<=4;j++)ctx.fillText((lo+j*(hi-lo)/4).toFixed($('frequencyReference').value==='rf'?3:1),Math.min(w-58,Math.max(4,left+j*(right-left)/4-20)),h-10);ctx.fillText(frequencyUnit(),w-30,h-10);}
function drawSpectrum(){
  if(!(diagnosticMask()&1)||!plotEnabled('spectrum'))return;
  const s=surface('spectrum');if(!s)return;const [lo,hi]=range(),a=analysisSnapshot;
  const {ctx,left,right,top,bottom}=s,[flo,fhi]=frequencyRange();
  ctx.fillStyle='#8fa6ba';ctx.fillText(a?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin',6,11);
  for(let j=0;j<=4;j++){const y=top+j*(bottom-top)/4;ctx.strokeStyle='#233446';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((hi-j*(hi-lo)/4).toFixed(0),6,y+4);}
  frequencyAxes(s);if(!a?.dbfs.length){ctx.fillText('Waiting for a complete spectrum burst',left+20,top+35);return;}
  ctx.save();ctx.beginPath();ctx.rect(left,top,right-left,bottom-top);ctx.clip();ctx.strokeStyle='#39d5b0';ctx.lineWidth=1.5;ctx.beginPath();
  a.dbfs.forEach((v,k)=>{const f=displayFrequency((k/a.dbfs.length-0.5)*a.sample_rate_hz),x=left+(f-flo)/(fhi-flo)*(right-left),y=bottom-Math.max(0,Math.min(1,(v-lo)/(hi-lo)))*(bottom-top);if(k)ctx.lineTo(x,y);else ctx.moveTo(x,y);});ctx.stroke();ctx.restore();
}
const waterfallBitmap=document.createElement('canvas');
const waterfallPalette=Array.from({length:256},(_,n)=>{
  const stops=[[8,17,35],[31,52,103],[41,112,165],[32,187,158],[224,211,98],[247,97,61]];
  const x=n/255*(stops.length-1),i=Math.min(stops.length-2,Math.floor(x)),f=x-i;
  return stops[i].map((c,k)=>Math.round(c+(stops[i+1][k]-c)*f));
});
const waterfallCssPalette=waterfallPalette.map(colour=>`rgb(${colour.join(',')})`);
let waterfallRender={mode:'2d',rows:0,columns:0};
const waterfallDefaultCamera={yaw:-24,pitch:34};
let waterfallCamera={...waterfallDefaultCamera};
function drawWaterfall(){
  if(!(diagnosticMask()&1)||!plotEnabled('waterfall'))return;
  const mode=$('waterfallMode').value,threeD=mode==='3d';
  $('waterfall').setAttribute('aria-label',threeD?'3D spectrum waterfall: frequency, observation age and power; default view has newest observation at front. Drag or use arrow keys to rotate.':'Spectrum waterfall, newest observation at top');
  $('waterfall').classList.toggle('waterfall-rotatable',threeD);
  $('waterfall').tabIndex=threeD?0:-1;
  if(threeD)$('waterfall').setAttribute('aria-describedby','waterfallHelp');else $('waterfall').removeAttribute('aria-describedby');
  $('waterfallOrbit').classList.toggle('hidden',!threeD);
  $('waterfallInfo').textContent=`${threeD?`3D · yaw ${Math.round(waterfallCamera.yaw)}° / tilt ${Math.round(waterfallCamera.pitch)}°`:'Newest at top'} · ${waterfallHistory.length} snapshots · display skips ${skippedDisplay}${analysisSnapshot?` · sample ${analysisSnapshot.first_sample}`:''}`;
  const [lo,hi]=range(true);
  $('colourLegend').textContent=`Global colours ${lo} … ${hi} ${analysisSnapshot?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin'}`;
  const s=surface('waterfall');if(!s)return;
  waterfallRender={mode,rows:0,columns:0};
  if(threeD){drawWaterfall3D(s);return;}
  const {ctx,left,right,top,bottom}=s;
  const rows=historyRows(),columns=analysisSnapshot?.dbfs.length||1024;
  waterfallBitmap.width=columns;waterfallBitmap.height=rows;
  const bitmap=waterfallBitmap.getContext('2d'),pixels=bitmap.createImageData(columns,rows);
  waterfallHistory.slice(0,rows).forEach((row,j)=>{
    for(let k=0;k<columns;k++){
      const level=Math.max(0,Math.min(255,Math.round(255*(row.values[k]-lo)/(hi-lo))));
      const colour=waterfallPalette[level],offset=4*(j*columns+k);
      pixels.data[offset]=colour[0];pixels.data[offset+1]=colour[1];pixels.data[offset+2]=colour[2];pixels.data[offset+3]=255;
    }
  });
  bitmap.putImageData(pixels,0,0);ctx.imageSmoothingEnabled=false;const fs=analysisSnapshot?.sample_rate_hz||100000,fullLo=displayFrequency(-fs/2),fullHi=displayFrequency(fs/2),[flo,fhi]=frequencyRange();
  ctx.drawImage(waterfallBitmap,(flo-fullLo)/(fullHi-fullLo)*columns,0,(fhi-flo)/(fullHi-fullLo)*columns,rows,left,top,right-left,bottom-top);
  ctx.fillStyle='#8fa6ba';ctx.fillText('New',6,top+9);ctx.fillText('Older',6,bottom);frequencyAxes(s);
  waterfallRender={mode,rows:Math.min(rows,waterfallHistory.length),columns};
}
/** Project the existing PSD history; bounded peak buckets keep browser work independent of history capacity. */
function drawWaterfall3D(s){
  const {ctx,w,h,left,right,top,bottom}=s,[lo,hi]=range(),[colourLo,colourHi]=range(true),[flo,fhi]=frequencyRange();
  const fs=analysisSnapshot?.sample_rate_hz||100000,fullLo=displayFrequency(-fs/2),fullHi=displayFrequency(fs/2);
  const yaw=waterfallCamera.yaw*Math.PI/180,pitch=waterfallCamera.pitch*Math.PI/180;
  const cy=Math.cos(yaw),sy=Math.sin(yaw),cp=Math.cos(pitch),sp=Math.sin(pitch);
  const rotate=(frequency,depth,power)=>{
    const x=(frequency-0.5)*2.2,z=(depth-0.5)*1.5,y=(power-0.5)*1.2,away=x*sy+z*cy;
    return [x*cy-z*sy,y*cp+away*sp,away*cp-y*sp];
  };
  const corners=[];for(const x of [0,1])for(const z of [0,1])for(const y of [0,1])corners.push(rotate(x,z,y));
  const minX=Math.min(...corners.map(p=>p[0])),maxX=Math.max(...corners.map(p=>p[0]));
  const minY=Math.min(...corners.map(p=>p[1])),maxY=Math.max(...corners.map(p=>p[1]));
  const scale=Math.min((right-left)/(maxX-minX),(bottom-top)/(maxY-minY));
  const midX=(left+right)/2,midY=(top+bottom)/2;
  const project=(frequency,depth,power)=>{const [x,y,z]=rotate(frequency,depth,power);return [midX+(x-(minX+maxX)/2)*scale,midY-(y-(minY+maxY)/2)*scale,z];};
  const line=(a,b)=>{ctx.beginPath();ctx.moveTo(a[0],a[1]);ctx.lineTo(b[0],b[1]);ctx.stroke();};
  const unit=analysisSnapshot?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin';
  const label=(text,point,dx=0,dy=0)=>ctx.fillText(text,Math.max(4,Math.min(w-ctx.measureText(text).width-4,point[0]+dx)),Math.max(12,Math.min(h-4,point[1]+dy)));
  ctx.fillStyle='#8fa6ba';ctx.strokeStyle='#293e52';
  for(let j=0;j<=4;j++){
    const x=j/4;line(project(x,0,0),project(x,1,0));
    line(project(0,x,0),project(1,x,0));
  }
  const wall=cy>=0?1:0,side=sy>=0?1:0;
  for(let j=0;j<=2;j++){
    const level=j/2;line(project(side,0,level),project(side,1,level));
    line(project(0,wall,level),project(1,wall,level));
  }
  const axes=()=>{
    ctx.strokeStyle='#7190aa';ctx.fillStyle='#adc5d8';
    line(project(0,0,0),project(1,0,0));line(project(1,0,0),project(1,1,0));line(project(0,0,0),project(0,0,1));
    for(let j=0;j<=4;j++){const x=j/4;label((flo+x*(fhi-flo)).toFixed($('frequencyReference').value==='rf'?3:1),project(x,0,0),-12,15);}
    for(let j=0;j<=2;j++)label((lo+j*(hi-lo)/2).toFixed(0),project(0,0,j/2),-30,0);
    label(frequencyUnit(),project(0.5,0,0),0,29);label(unit,project(0,0,1),-20,-9);
    label('New',project(1,0,0),8,0);
    const span=waterfallHistory.length?Math.max(0,waterfallHistory[0].produced-waterfallHistory[waterfallHistory.length-1].produced):0;
    label(`Older · ${span.toFixed(1)} s`,project(1,1,0),8,0);
  };
  if(!waterfallHistory.length){axes();ctx.fillText('Waiting for waterfall history',left+20,top+35);return;}
  const oldest=waterfallHistory[waterfallHistory.length-1],newest=waterfallHistory[0];
  const span=Math.max(0,newest.produced-oldest.produced);
  // Each bucket retains its peak, including narrow carriers, across the selected frequency interval.
  const bins=newest.values.length,start=Math.max(0,Math.min(bins-1,Math.floor((flo-fullLo)/(fullHi-fullLo)*bins)));
  const end=Math.max(start+1,Math.min(bins,Math.ceil((fhi-fullLo)/(fullHi-fullLo)*bins)));
  const columns=Math.min(end-start,192,Math.max(16,Math.floor((right-left)/5)));
  const rows=Math.min(48,waterfallHistory.length),mesh=[];
  for(let j=0;j<rows;j++){
    const index=rows===1?0:Math.round(j*(waterfallHistory.length-1)/(rows-1)),row=waterfallHistory[index];
    const depth=span>0?Math.max(0,Math.min(1,(newest.produced-row.produced)/span)):0,points=[];
    for(let k=0;k<columns;k++){
      const first=start+Math.floor(k*(end-start)/columns),last=start+Math.floor((k+1)*(end-start)/columns);
      let peak=-Infinity;for(let bin=first;bin<last;bin++)peak=Math.max(peak,row.values[bin]);
      const colour=waterfallCssPalette[Math.max(0,Math.min(255,Math.round(255*(peak-colourLo)/(colourHi-colourLo))))];
      points.push({xy:project(columns===1?0.5:k/(columns-1),depth,Math.max(0,Math.min(1,(peak-lo)/(hi-lo)))),colour});
    }
    mesh.push(points);
  }
  // Sort facets in camera space so rear observations also occlude correctly after rotation.
  const facets=[];
  for(let j=0;j<rows;j++)for(let k=0;k<columns-1;k++){
    const a=mesh[j][k],b=mesh[j][k+1],points=j>0?[a,b,mesh[j-1][k+1],mesh[j-1][k]]:[a,b];
    facets.push({points,colour:a.colour,depth:points.reduce((sum,p)=>sum+p.xy[2],0)/points.length});
  }
  facets.sort((a,b)=>b.depth-a.depth);ctx.lineWidth=0.7;
  for(const facet of facets){
    ctx.beginPath();ctx.moveTo(facet.points[0].xy[0],facet.points[0].xy[1]);for(let n=1;n<facet.points.length;n++)ctx.lineTo(facet.points[n].xy[0],facet.points[n].xy[1]);
    if(facet.points.length>2){ctx.closePath();ctx.fillStyle=facet.colour;ctx.fill();}
    ctx.strokeStyle=facet.colour;ctx.stroke();
  }
  if(columns===1)for(const row of mesh){ctx.fillStyle=row[0].colour;ctx.fillRect(row[0].xy[0]-2,row[0].xy[1]-2,4,4);}
  axes();waterfallRender={mode:'3d',rows,columns};
}
function drawHistogram(id,data,color){
  if(!(diagnosticMask()&2))return;
  const s=surface(id);if(!s)return;const {ctx,w,h,left,right,top,bottom}=s;const percent=$('histScale').value==='percent',count=analysisSnapshot?.histogram_count||1;
  const values=(data||[]).map(v=>percent?100*v/count:v),max=Number($('histMax').value)>0?Number($('histMax').value):Math.max(1,...values);
  ctx.fillStyle='#8fa6ba';ctx.fillText(percent?'%':'count',4,11);
  for(let j=0;j<=2;j++){const y=top+j*(bottom-top)/2;ctx.strokeStyle='#233446';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((max*(1-j/2)).toFixed(percent?1:0),4,y+4);}
  const dw=(right-left)/(values.length||1);ctx.fillStyle=color;
  values.forEach((v,k)=>{const bh=(bottom-top)*Math.min(1,v/max);ctx.fillRect(left+k*dw,bottom-bh,Math.max(1,dw-1),bh);});
  ctx.fillStyle='#8fa6ba';for(let j=0;j<=4;j++)ctx.fillText(String(j===4?(analysisSnapshot?.histogram_max??127):Math.round((analysisSnapshot?.histogram_min??-128)+j*((analysisSnapshot?.histogram_max??127)-(analysisSnapshot?.histogram_min??-128)+1)/4)),Math.min(w-28,left+j*(right-left)/4-10),h-10);
}
function drawAnalysis(){drawSpectrum();drawWaterfall();drawHistogram('histogramI',analysisSnapshot?.histogram_i,'#39d5b0');drawHistogram('histogramQ',analysisSnapshot?.histogram_q,'#62a6ff');}
let plotDrawPending=false,waterfallDrawPending=false;
function schedulePlotDraw(){if(plotDrawPending||!diagnosticMask())return;plotDrawPending=true;requestAnimationFrame(()=>{plotDrawPending=false;drawAnalysis();});}
function scheduleWaterfallDraw(){if(waterfallDrawPending||!(diagnosticMask()&1)||!plotEnabled('waterfall'))return;waterfallDrawPending=true;requestAnimationFrame(()=>{waterfallDrawPending=false;drawWaterfall();});}
function setPlotHeight(id,height){
  height=Math.max(180,Math.min(720,Math.round(height/10)*10));
  $(`${id}Wrap`).style.height=`${height}px`;$(`${id}Height`).value=height;$(`${id}HeightValue`).value=`${height} px`;
  $(`${id}Resize`).setAttribute('aria-valuenow',height);$(`${id}Resize`).setAttribute('aria-valuetext',`${height} pixels`);schedulePlotDraw();
}
for(const id of ['spectrum','waterfall']){
  setPlotHeight(id,$(`${id}Wrap`).getBoundingClientRect().height);
  $(`${id}Height`).addEventListener('input',event=>setPlotHeight(id,Number(event.target.value)));
  const handle=$(`${id}Resize`);let drag=null;
  handle.addEventListener('pointerdown',event=>{if(!event.isPrimary||event.button!==0)return;event.preventDefault();handle.focus();drag={pointer:event.pointerId,y:event.clientY,height:$(`${id}Wrap`).getBoundingClientRect().height};handle.setPointerCapture(event.pointerId);});
  handle.addEventListener('pointermove',event=>{if(drag&&event.pointerId===drag.pointer)setPlotHeight(id,drag.height+event.clientY-drag.y);});
  const finish=event=>{if(drag&&event.pointerId===drag.pointer){drag=null;if(handle.hasPointerCapture(event.pointerId))handle.releasePointerCapture(event.pointerId);}};
  for(const name of ['pointerup','pointercancel','lostpointercapture'])handle.addEventListener(name,finish);
  handle.addEventListener('keydown',event=>{
    const change={ArrowUp:-10,ArrowDown:10,PageUp:-100,PageDown:100}[event.key];
    if(change===undefined&&!['Home','End'].includes(event.key))return;
    event.preventDefault();setPlotHeight(id,event.key==='Home'?180:event.key==='End'?720:Number($(`${id}Height`).value)+change);
  });
}
let plotsExpanded=false,plotFocusReturn=null,plotScrollReturn=0;
function setPlotsExpanded(expanded){
  if(expanded===plotsExpanded)return;
  if(expanded){plotFocusReturn=document.activeElement;plotScrollReturn=scrollY;}
  plotsExpanded=expanded;const panel=$('spectrumPanel');
  panel.classList.toggle('plot-focused',expanded);document.body.classList.toggle('plots-focused',expanded);
  $('expandPlots').textContent=expanded?'Restore plots':'Expand plots';$('expandPlots').setAttribute('aria-expanded',String(expanded));
  if(expanded){panel.setAttribute('role','dialog');panel.setAttribute('aria-modal','true');$('expandPlots').focus({preventScroll:true});}
  else {panel.removeAttribute('role');panel.removeAttribute('aria-modal');scrollTo(0,plotScrollReturn);if(plotFocusReturn?.isConnected)plotFocusReturn.focus({preventScroll:true});}
  schedulePlotDraw();
}
$('expandPlots').addEventListener('click',()=>setPlotsExpanded(!plotsExpanded));
document.addEventListener('keydown',event=>{
  if(!plotsExpanded)return;
  if(event.key==='Escape'){event.preventDefault();setPlotsExpanded(false);}
  else if(event.key==='Tab'){
    const controls=[...$('spectrumPanel').querySelectorAll('button,input,select,[tabindex="0"]')].filter(el=>!el.disabled&&el.getClientRects().length);
    const first=controls[0],last=controls[controls.length-1];
    if(event.shiftKey&&document.activeElement===first){event.preventDefault();last.focus();}
    else if(!event.shiftKey&&document.activeElement===last){event.preventDefault();first.focus();}
  }
});
function rotateWaterfall(yaw,pitch){
  waterfallCamera={yaw:((yaw+180)%360+360)%360-180,pitch:Math.max(12,Math.min(78,pitch))};scheduleWaterfallDraw();
}
function resetWaterfallCamera(){waterfallCamera={...waterfallDefaultCamera};scheduleWaterfallDraw();}
for(const [id,yaw,pitch] of [['waterfallLeft',-10,0],['waterfallRight',10,0],['waterfallUp',0,5],['waterfallDown',0,-5]])$(id).addEventListener('click',()=>rotateWaterfall(waterfallCamera.yaw+yaw,waterfallCamera.pitch+pitch));
$('waterfallReset').addEventListener('click',resetWaterfallCamera);
let waterfallDrag=null;
$('waterfall').addEventListener('pointerdown',event=>{
  if($('waterfallMode').value!=='3d'||!event.isPrimary||event.button!==0)return;
  event.preventDefault();$('waterfall').focus({preventScroll:true});
  waterfallDrag={pointer:event.pointerId,x:event.clientX,y:event.clientY,...waterfallCamera};$('waterfall').setPointerCapture(event.pointerId);$('waterfall').classList.add('dragging');
});
$('waterfall').addEventListener('pointermove',event=>{
  if(!waterfallDrag||event.pointerId!==waterfallDrag.pointer)return;
  rotateWaterfall(waterfallDrag.yaw+(event.clientX-waterfallDrag.x)*0.4,waterfallDrag.pitch+(event.clientY-waterfallDrag.y)*0.25);
});
function finishWaterfallDrag(event){
  if(!waterfallDrag||event.pointerId!==waterfallDrag.pointer)return;
  waterfallDrag=null;$('waterfall').classList.remove('dragging');if($('waterfall').hasPointerCapture(event.pointerId))$('waterfall').releasePointerCapture(event.pointerId);
}
for(const name of ['pointerup','pointercancel','lostpointercapture'])$('waterfall').addEventListener(name,finishWaterfallDrag);
$('waterfall').addEventListener('keydown',event=>{
  if($('waterfallMode').value!=='3d')return;
  const direction={ArrowLeft:[-10,0],ArrowRight:[10,0],ArrowUp:[0,5],ArrowDown:[0,-5]}[event.key];
  if(direction){event.preventDefault();rotateWaterfall(waterfallCamera.yaw+direction[0],waterfallCamera.pitch+direction[1]);}
  else if(event.key==='Home'){event.preventDefault();resetWaterfallCamera();}
});
$('waterfall').addEventListener('dblclick',()=>{if($('waterfallMode').value==='3d')resetWaterfallCamera();});
$('clearWaterfall').addEventListener('click',()=>{waterfallHistory=[];drawWaterfall();});
$('waterfallMode').addEventListener('change',drawWaterfall);
$('waterfallRows').addEventListener('change',()=>{waterfallHistory.length=Math.min(waterfallHistory.length,historyRows());drawAnalysis();});
for(const id of ['dbMin','dbMax','colourMin','colourMax','waterfallRate','frequencyMin','frequencyMax','histScale','histMax'])$(id).addEventListener('change',drawAnalysis);
$('configForm').addEventListener('submit', event => event.preventDefault());
$('apply').addEventListener('click', () => applyConfig()); $('validate').addEventListener('click', () => applyConfig(true));
$('start').addEventListener('click', () => command('receiver/start')); $('stop').addEventListener('click', () => command('receiver/stop'));
$('recordStart').addEventListener('click', () => command('recording/start')); $('recordStop').addEventListener('click', () => command('recording/stop'));
$('reloadFiles').addEventListener('click', refreshFiles); $('clearLog').addEventListener('click', () => $('events').replaceChildren());
$('pausePlot').addEventListener('click', () => { paused = !paused; $('pausePlot').textContent = paused ? 'Resume plots' : 'Pause plots'; if (!paused && status)renderAnalysis(status.analysis,status.revision);requestStatus(); });
$('useIni').addEventListener('click', () => { try { values = parseIni($('iniEditor').value); buildForm(); dirtyState(true); } catch (error) { notice(error.message, true); } });
$('importIni').addEventListener('change', async event => { try { const file = event.target.files[0]; if (!file) return; if (file.size > 12000) throw new Error('INI file exceeds 12 kB.'); values = parseIni(await file.text()); buildForm(); dirtyState(true); notice('INI imported into draft. Validate and apply before starting.'); } catch (error) { notice(error.message, true); } finally { event.target.value = ''; } });
$('exportIni').addEventListener('click', () => { const url = URL.createObjectURL(new Blob([toIni(values)], {type:'text/plain'})), link = document.createElement('a'); link.href = url; link.download = 'receiver.ini'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); });
$('rtlPreset').addEventListener('click', () => {
  deviceCaps = null;
  Object.assign(values, {'frontend.type':'rtlsdr','frontend.sample_rate_hz':'2400000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'2400','hackrf.bandwidth_hz':'0','rtlsdr.auto_gain':'true','rtlsdr.gain_tenths_db':'0','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'});
  buildForm(); dirtyState(true);
});
$('bladePreset').addEventListener('click', () => {
  deviceCaps = null;
  Object.assign(values, {'frontend.type':'bladerf','frontend.sample_rate_hz':'16000000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'16000','hackrf.bandwidth_hz':'0','bladerf.gain_db':'30','bladerf.format':'SC16_Q11','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'});
  buildForm(); dirtyState(true);
});
let capabilityRequestId = 0;
$('deviceLimits').addEventListener('click', async () => {
  if (!editable() || !validateProfileFields()) return;
  const requestId = ++capabilityRequestId;
  const keys = ['frontend.type','frontend.serial','frontend.center_frequency_hz','bladerf.format'];
  const requested = keys.map(key => values[key] || '');
  const current = () => requestId === capabilityRequestId && keys.every((key,index) => (values[key] || '') === requested[index]);
  try {
    const result = await api('frontend/capabilities', {ini: toIni(values)});
    if (!current() || !editable()) return;
    deviceCaps = { ...result, source: requested[0], serial: requested[1], tuning: requested[2], precision: requested[3] };
    buildForm(); notice(`Native device limits loaded · ${result.format}`);
  } catch(error) { if (current()) { profileRequestNotice = {message:error.message,error:true}; profileSummary(); notice(error.message, true); } }
});
$('filePreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'file','replay.realtime':'true','replay.loop':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('frequencyReference').addEventListener('change',()=>{$('frequencyMin').value='';$('frequencyMax').value='';drawAnalysis();});
$('resetFrequency').addEventListener('click',()=>{$('frequencyMin').value='';$('frequencyMax').value='';drawAnalysis();});
$('autoScale').addEventListener('click',()=>{if(!($('showSpectrum').checked||$('showWaterfall').checked)||!diagnosticEnabled('spectrum_enabled')||!analysisSnapshot?.dbfs.length)return;const lo=Math.floor(Math.min(...analysisSnapshot.dbfs)/10)*10,hi=Math.ceil(Math.max(...analysisSnapshot.dbfs)/10)*10+10;for(const id of ['dbMin','colourMin'])$(id).value=lo;for(const id of ['dbMax','colourMax'])$(id).value=hi;drawAnalysis();});
for(const id of ['showSpectrum','showWaterfall'])$(id).addEventListener('change',changePlotEnabled);
$('showHistogram').addEventListener('change',()=>{$('histogramPanel').classList.toggle('hidden',!$('showHistogram').checked);if($('showHistogram').checked&&!paused&&status)renderAnalysis(status.analysis,status.revision);requestStatus();});
document.addEventListener('visibilitychange',requestStatus);
$('hackrfPreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'hackrf','frontend.sample_rate_hz':'16000000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'16000','hackrf.bandwidth_hz':'0','hackrf.lna_gain_db':'16','hackrf.vga_gain_db':'20','hackrf.rf_amp':'false','hackrf.antenna_power':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('exit').addEventListener('click', async () => { if (!confirm('Stop the receiver, finish recording and close the local NGene service?')) return; try { await api('shutdown', {}); quitting = true; notice('NGene is closing. Restart ngene --web to reconnect.'); } catch (error) { notice(error.message, true); } });
setInterval(() => { if (waiting && performance.now() - lastRequest > 4000) socket?.close(); else if (statusRequestPending||performance.now() - lastRequest >= Number($('refresh').value)) requestStatus(); }, 10);

const plotResizeObserver=new ResizeObserver(schedulePlotDraw);
for(const id of ['spectrum','waterfall','histogramI','histogramQ'])plotResizeObserver.observe($(id));
receiverWorkspace = createReceiverWorkspace({
  onViewChange(view) {
    activeWorkspaceView = view;
    if (view !== 'rf') setPlotsExpanded(false);
    requestStatus();
    if (view === 'rf') requestAnimationFrame(drawAnalysis);
  },
  onOpenSettings(topic) { showView('setup'); selectSettings(topic); },
  onBrowseReplay() {
    if (!editable()) { notice('Stop the receiver before choosing another input file.', true); return; }
    showView('setup'); selectSettings('source'); $('browseReplay').click();
  }
});
connect();

/* Read-only inspection for frame troubleshooting and browser regression tests. */
export function diagnosticSnapshot(){return {sequence:analysisSequence,skipped:skippedDisplay,rejected:rejectedFrames,waterfall:{...waterfallRender,camera:{...waterfallCamera}},history:waterfallHistory.map(row=>({...row,values:[...row.values]}))};}
export {psdIntegrity, renderAnalysis, displayFrequency};

document.querySelectorAll('[data-settings]').forEach(button => button.addEventListener('click', () => selectSettings(button.dataset.settings)));

let pickerListing = null, replaySelection = null, pickerBusy = false, pickerSession = null, pickerRequestId = 0, pickerFilterTimer = null;
/** Join paths for the receiver OS without using the browser's local filesystem. */
function pickerPath(directory, name) { return directory ? `${directory.replace(/[\\/]$/, '')}/${name}` : name; }
/** Inspect one closed CI8 selection. This request never changes or starts a receiver. */
async function inspectReplaySelection(path) {
  const requestId = ++pickerRequestId;
  replaySelection = null; $('pickerUse').disabled = true; $('pickerMetadata').replaceChildren();
  $('pickerSelection').textContent = path; $('pickerNotice').textContent = 'Reading capture metadata…';
  const result = await api('replay-files/inspect', {path});
  if (requestId !== pickerRequestId || pickerSession !== selectedSession || !$('replayFileDialog').open) return;
  replaySelection = result;
  const settings = parseIni(result.ini);
  const lines = [['Samples', pairs(result.samples)], ['Format', 'Signed int8 · I,Q interleaved']];
  if (result.metadata_available) {
    lines.push(['Metadata', result.metadata_path], ['Sample rate', `${settings['frontend.sample_rate_hz']} complex pairs/s (nominal)`],
      ['SDR tuning', `${settings['frontend.center_frequency_hz']} Hz`], ['Signal centre', `${settings['frontend.signal_frequency_hz']} Hz`],
      ['Digital IF', `${Number(settings['frontend.signal_frequency_hz']) - Number(settings['frontend.center_frequency_hz'])} Hz`],
      ['Saved samples', result.recording_point], ['Recorded conditioner', result.recorded_conditioning],
      ['Completion', result.complete ? 'Complete' : 'Incomplete / interrupted'], ['Stream gaps', result.has_gaps ? 'Present — see sample index' : 'See session sample index'],
      ['Replay conditioner', 'Bypass — saved sample values are preserved']);
  } else lines.push(['Metadata', 'Not found — set sampling rate and RF parameters manually']);
  for (const [label, value] of lines) {
    const dt = document.createElement('dt'), dd = document.createElement('dd'); dt.textContent = label; dd.textContent = value; $('pickerMetadata').append(dt, dd);
  }
  $('pickerNotice').textContent = result.metadata_available ? (result.complete ? 'Metadata loaded. Review the settings before starting.' : 'Capture is marked incomplete. Review the capture/index before playback.') : 'This file has no sidecar; its stream parameters cannot be inferred from raw bytes.';
  $('pickerUse').disabled = false;
}
/** Render a bounded, filterable receiver-side directory listing using text-only names. */
function renderPickerEntries() {
  $('pickerEntries').replaceChildren();
  if (!pickerListing) return;
  const filter = $('pickerFilter').value.toLowerCase();
  const entries = pickerListing.entries.filter(entry => entry.directory || entry.name.toLowerCase().includes(filter));
  entries.sort((a,b) => Number(b.directory)-Number(a.directory) || a.name.localeCompare(b.name));
  for (const entry of entries) {
    const row = document.createElement('tr'), name = document.createElement('td'), size = document.createElement('td'), action = document.createElement('td'), button = document.createElement('button');
    name.textContent = entry.name; size.textContent = entry.directory ? 'Folder' : bytes(entry.bytes);
    button.textContent = entry.directory ? 'Open folder' : 'Select'; button.disabled = pickerBusy || !entry.selectable;
    button.title = entry.selectable ? '' : 'File is empty, incomplete as I/Q pairs, too long or currently being written';
    button.addEventListener('click', async () => {
      if (pickerBusy) return; pickerBusy = true; renderPickerEntries();
      try { const path = pickerPath(pickerListing.directory, entry.name); if (entry.directory) await browseReplayDirectory(path); else await inspectReplaySelection(path); }
      catch (error) { $('pickerNotice').textContent = error.message; }
      finally { pickerBusy = false; renderPickerEntries(); }
    }); action.append(button); row.append(name, size, action); $('pickerEntries').append(row);
  }
  if (!entries.length) { const row = document.createElement('tr'), cell = document.createElement('td'); cell.colSpan = 3; cell.textContent = 'No matching CI8 files or folders.'; row.append(cell); $('pickerEntries').append(row); }
}
/** Browse one existing receiver folder; navigation performs no filesystem writes. */
async function browseReplayDirectory(path) {
  const requestId = ++pickerRequestId;
  replaySelection = null; $('pickerUse').disabled = true;
  $('pickerSelection').textContent = 'No file selected.'; $('pickerMetadata').replaceChildren();
  $('pickerNotice').textContent = 'Reading receiver folder…';
  const body = {filter:$('pickerFilter').value}; if (path !== undefined) body.path = path;
  const result = await api('replay-files/browse', body);
  if (requestId !== pickerRequestId || pickerSession !== selectedSession || !$('replayFileDialog').open) return;
  pickerListing = result; replaySelection = null; $('pickerUse').disabled = true;
  $('pickerSelection').textContent = 'No file selected.'; $('pickerMetadata').replaceChildren();
  $('pickerDirectory').value = result.directory;
  $('pickerNotice').textContent = result.truncated ? 'Showing at most 500 entries; refine the filename filter or open a subfolder.' : 'Choose a closed .cs8, .cu8 or .sc16q11 file. Its adjacent metadata is loaded automatically.';
  renderPickerEntries();
}
/** Open selection for the currently stopped session; a table Replay action chooses replay first. */
async function openReplayPicker(directory, selectedPath) {
  if (!editable()) throw new Error('Stop the receiver before choosing another input file.');
  pickerSession = selectedSession; replaySelection = null; $('pickerFilter').value = '';
  $('replayFileDialog').showModal();
  try { await browseReplayDirectory(directory); if (selectedPath) await inspectReplaySelection(selectedPath); }
  catch (error) { $('pickerNotice').textContent = error.message; }
}
$('browseReplay').addEventListener('click', () => openReplayPicker().catch(error => notice(error.message, true)));
$('pickerClose').addEventListener('click', () => $('replayFileDialog').close());
$('replayFileDialog').addEventListener('close', () => { ++pickerRequestId; clearTimeout(pickerFilterTimer); replaySelection = null; });
$('pickerRoots').addEventListener('click', () => browseReplayDirectory('').catch(error => $('pickerNotice').textContent = error.message));
$('pickerParent').addEventListener('click', () => browseReplayDirectory(pickerListing?.parent || '').catch(error => $('pickerNotice').textContent = error.message));
$('pickerNavigation').addEventListener('submit', event => { event.preventDefault(); browseReplayDirectory($('pickerDirectory').value.trim()).catch(error => $('pickerNotice').textContent = error.message); });
$('pickerFilter').addEventListener('input', () => {
  renderPickerEntries(); clearTimeout(pickerFilterTimer);
  pickerFilterTimer = setTimeout(() => browseReplayDirectory(pickerListing?.directory).catch(error => $('pickerNotice').textContent = error.message), 180);
});
$('pickerUse').addEventListener('click', () => {
  if (!replaySelection || pickerSession !== selectedSession || !editable()) return;
  if (replaySelection.revision !== revision) { $('pickerNotice').textContent = 'The profile changed. Select the file again.'; return; }
  const draft = parseIni(replaySelection.ini);
  const keys = ['frontend.type','replay.format','replay.path','replay.start_sample','replay.sample_limit','receiver.run_seconds','receiver.mode','recording.enabled','conditioning.module'];
  if (replaySelection.metadata_available) keys.push('frontend.sample_rate_hz','frontend.center_frequency_hz','frontend.signal_frequency_hz','hackrf.bandwidth_hz','conditioning.dc_shift','buffer.window_samples');
  for (const key of keys) values[key] = draft[key];
  buildForm(); dirtyState(true); selectSettings('source'); $('showConfiguration').click();
  notice(replaySelection.metadata_available ? 'Replay metadata loaded into the draft. Review, Apply & save, then Start receiver.' : 'File selected. Set sampling rate and RF parameters, then Apply & save.');
  $('replayFileDialog').close();
});
