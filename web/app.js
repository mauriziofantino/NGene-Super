import {createReceiverWorkspace} from './workspace.js';
import {createReceiverConfiguration} from './receiver-config.js';
import {moduleParameters} from './module-parameters.js';
import {frequencyUnits, frequencyToHz, frequencyFromHz} from './frequency-units.js';
import {createWaterfallSurface, createWaterfallScroll, reliefPalette} from './waterfall-relief.js';

const selectedSession = 'receiver';
const sessionPrefix = () => '';
let deviceCaps = null;
let receiverWorkspace = null, activeWorkspaceView = 'rf';
/* NGene browser UI: presentation and commands only. No receiver/DSP implementation. */
const $ = id => document.getElementById(id);
let receiverConfiguration = null, moduleForm = null;
const fields = [
  ['Frontend', 'frontend.type', 'Source', 'select', ['file', 'hackrf', 'rtlsdr', 'bladerf', 'sige']],
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
  ['RTL-SDR', 'rtlsdr.if_gain_db', 'E4000 IF gain', 'select', [-1,...Array.from({length:54},(_,k)=>k+3)], 'dB', '-1 = driver default; manual E4000 only. Earlier IF stages receive gain first.'],
  ['RTL-SDR', 'rtlsdr.ppm', 'Frequency correction', 'number', null, 'ppm'],
  ['RTL-SDR', 'rtlsdr.bias_tee', 'Bias tee', 'select', ['false','true']],
  ['bladeRF', 'bladerf.gain_db', 'RX gain', 'number', null, 'dB', 'Query device limits for frequency-dependent min, max and step'],
  ['bladeRF', 'bladerf.bias_tee', 'Bias tee', 'select', ['false','true']],
  ['bladeRF', 'bladerf.format', 'Sample precision', 'select', ['SC16_Q11','CI8'], '', 'SC16_Q11: 12-bit ADC in 16-bit I/Q; CI8: native 8-bit I/Q (requires device/FPGA support)'],
  ['File replay', 'replay.format', 'Native IQ format', 'select', ['CI8','CU8','SC16_Q11','CI16']],
  ['File replay', 'replay.spectrum_inverted', 'I/Q spectrum inverted', 'select', ['false','true'], '', 'On for native SiGe ADC I/Q; capture metadata preserves this convention.'],
  ['File replay', 'replay.path', 'Input file on receiver computer', 'text', null, '', 'Native I/Q (.cs8, .cu8, .sc16q11); sidecar loads format, rate and tuning'],
  ['File replay', 'replay.start_sample', 'Start sample pair', 'number', null, 'pairs'],
  ['File replay', 'replay.sample_limit', 'Range length', 'number', null, 'pairs', '0 = remainder of file'],
  ['File replay', 'replay.realtime', 'Real-time pacing', 'select', ['true', 'false']],
  ['File replay', 'replay.loop', 'Repeat range', 'select', ['true', 'false'], '', 'Each rewind reports a discontinuity'],
  ['Receiver', 'receiver.mode', 'Mode', 'select', ['capture', 'record']],
  ['Receiver', 'receiver.run_seconds', 'Run duration', 'number', null, 's', '0 = until Stop'],
  ['Receiver', 'receiver.idle_timeout_ms', 'Input timeout', 'number', null, 'ms'],
  ['Buffers', 'buffer.window_samples', 'Common window', 'number', null, 'pairs'],
  ['Buffers', 'buffer.ingress_blocks', 'Frontend slots', 'number'],
  ['Buffers', 'buffer.ring_blocks', 'Central ring slots', 'number'],
  ['Buffers', 'buffer.overflow_policy', 'Input overflow', 'select', ['stop', 'drop_newest']],
  ['Recording', 'recording.enabled', 'Record on receiver start', 'select', ['false', 'true']],
  ['Recording', 'recording.prefix', 'Base filename', 'text', null, '', 'Letters, digits, _ and - · unique suffix added automatically'],
  ['Recording', 'recording.directory', 'Folder on receiver computer', 'text', null, '', 'Receiver-side path, e.g. /data/gnss or D:/GNSS. Parent folder must exist.'],
  ['Recording', 'recording.queue_blocks', 'Writer queue slots', 'number'],
  ['Recording', 'recording.segment_samples', 'Pairs per segment', 'number', null, '', '0 = single raw file']
];
let values = {}, savedValues = {}, revision = '1', token = '', status = null, capabilities = null;
let dirty = false, httpBusy = false, pendingCommand = null, socket = null;
let terminalShutdown = false;
let calibrationBusy = false, calibrationJobId = null, calibrationRequest = null;
let calibrationStopRequested = false;
let calibrationPolling = false, calibrationTerminal = '';
let calibrationSeenJob = 0n;
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
  const source = values['frontend.type'];
  const sourceFields = {'hackrf.':'hackrf', 'rtlsdr.':'rtlsdr', 'bladerf.':'bladerf', 'replay.':'file'};
  for (const [prefix, kind] of Object.entries(sourceFields))
    if (input.name.startsWith(prefix) && source !== kind) return '';
  if (input.name === 'rtlsdr.gain_tenths_db' && values['rtlsdr.auto_gain'] !== 'false') return '';
  if (!moduleForm?.active(input.name, values)) return '';
  if (input.type !== 'number') return moduleForm?.error(input.name, values) || '';
  const title = fields.find(field => field[1] === input.name)?.[2] || input.name;
  const descriptor = moduleForm?.definitions.get(input.name);
  const scaledFrequency = frequencyUnits(input.name);
  const integer = scaledFrequency ? false : descriptor ? (descriptor.integer ? 'decimal' : false) : input.name === 'receiver.run_seconds' ? false : ['rtlsdr.ppm','bladerf.gain_db'].includes(input.name) ? true : 'decimal';
  const message = numericFieldError(input, title, Object.hasOwn(values, input.name), integer);
  if (message) return message;
  if (scaledFrequency && frequencyToHz(input.name, input.value) === null)
    return `${title}: use decimal notation with a resolution of 1 Hz (${10 ** -scaledFrequency.decimals} ${scaledFrequency.unit}).`;
  const rate = Number(values[input.name]);
  if (input.name === 'frontend.sample_rate_hz' && values['frontend.type'] === 'rtlsdr' && rate > 300000 && rate <= 900000) return 'RTL-SDR sample rate must be (0.225,0.3] or (0.9,3.2] Msps.';
  return moduleForm?.error(input.name, values) || '';
}
function refreshProfileErrors(force = false) {
  if (force) profileValidationRequested = true;
  for (const input of $('configForm').elements) {
    if (!input.name) continue;
    if (profileValidationRequested || profileTouched.has(input.name) || profileErrors.has(input.name)) {
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
function editable() { return connected() && status && ['stopped', 'error'].includes(status.state) && !httpBusy && !pendingCommand && !calibrationBusy; }
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
  const legacyArithmetic = {
    'tracking.fast_carrier':'dsp.tracking_fast_carrier',
    'tracking.square_carrier':'dsp.tracking_square_carrier',
    'tracking.xor_correlation':'dsp.tracking_xor_correlation',
    'tracking.mixer_bits':'dsp.tracking_mixer_bits',
    'acquisition.square_carrier':'dsp.gps_square_carrier',
    'acquisition.xor_correlation':'dsp.gps_xor_correlation',
    'galileo_acquisition.square_carrier':'dsp.galileo_square_carrier',
    'galileo_acquisition.xor_correlation':'dsp.galileo_xor_correlation'
  };
  for (const [oldKey, newKey] of Object.entries(legacyArithmetic)) {
    if (Object.hasOwn(parsed, oldKey)) {
      if (!Object.hasOwn(parsed, newKey)) parsed[newKey] = parsed[oldKey];
      delete parsed[oldKey];
    }
  }
  delete parsed['beidou_acquisition.enabled'];
  if (Object.hasOwn(parsed, 'tracking.egnos')) {
    parsed['receiver.egnos'] ??= parsed['tracking.egnos'];
    delete parsed['tracking.egnos'];
  }
  if (Object.hasOwn(parsed, 'tracking.broadcast_ionosphere')) {
    parsed['navigation.broadcast_ionosphere'] ??= parsed['tracking.broadcast_ionosphere'];
    delete parsed['tracking.broadcast_ionosphere'];
  }
  return parsed;
}
function toIni(object) {
  const groups = new Map();
  for (const [key, value] of Object.entries(object)) {
    if(key.startsWith("conditioning.")) continue;
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
  receiver: ['Receiver & signals', 'Configure shared channels and reservations for GPS, Galileo and BeiDou.', 'Before start'],
  acquisition: ['Acquisition', 'Configure the GPS, Galileo and BeiDou searches.', 'Before start'],
  tracking: ['Tracking', 'Configure worker allocation and multi-correlator geometry.', 'Before start'],
  navigation: ['Navigation & PVT', 'Configure corrections used by the navigation solution.', 'Before start'],
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
  $('configForm').classList.toggle('hidden', !['source', 'receiver', 'recording'].includes(topic) && ![...(moduleForm?.groups.values() || [])].some(g => g.topic === topic));
  const groups = {source: ['Frontend', 'HackRF', 'RTL-SDR', 'bladeRF', 'File replay'], receiver: ['Receiver', 'Buffers'], acquisition: [], recording: ['Recording']};
  document.querySelectorAll('#fields [data-group]').forEach(group => group.classList.toggle('topic-hidden', !(groups[topic] || []).includes(group.dataset.group) && moduleForm?.groups.get(group.dataset.group)?.topic !== topic));
  $('sourcePresets').classList.toggle('hidden', topic !== 'source');
  $('calibrationStatus').classList.toggle('hidden', topic !== 'source');
  receiverConfiguration?.select(topic);
  $('apply').disabled = !editable() || referenceSettingsOnly();
  for (const [id, key] of [['analysisSettings', 'analysis'], ['displaySettings', 'display'], ['profileSettings', 'profile']]) $(id).classList.toggle('hidden', topic !== key);
  updateSourceFields();
}
function referenceSettingsOnly() { return ['acquisition', 'tracking', 'navigation'].includes(selectedSettings) && ![...(moduleForm?.groups.values() || [])].some(g => g.topic === selectedSettings); }
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
    const scaledFrequency = frequencyUnits(key);
    title.textContent = label; units.className = 'unit'; units.textContent = scaledFrequency?.unit || unit || ''; title.append(units); wrapper.append(title);
    const nativeBandwidth = key === 'hackrf.bandwidth_hz' && values['frontend.type'] !== 'hackrf';
    const input = document.createElement(type === 'select' && !nativeBandwidth ? 'select' : 'input'); input.name = key;
    if (type === 'select' && !nativeBandwidth) {
      const choices = key === 'rtlsdr.gain_tenths_db' ? (deviceCaps?.tuner_gains_tenths_db || [values[key] || '0']) : options;
      if (key === 'rtlsdr.gain_tenths_db' && values['rtlsdr.auto_gain'] !== 'false' &&
          !choices.some(option => String(option) === String(values[key]))) {
        const unused = document.createElement('option');
        unused.value = values[key] || '0'; unused.textContent = 'Automatic (manual gain unused)'; input.append(unused);
      }
      for (const option of choices) { const element = document.createElement('option'); element.value = typeof option === 'object' ? option.value : option; element.textContent = typeof option === 'object' ? option.label : option === 'true' ? 'On' : option === 'false' ? 'Off' : option === 'record' ? 'Record only' : option === 'capture' ? 'Capture' : option; input.append(element); }
    }
    else { input.type = nativeBandwidth ? 'number' : type; if(input.type==='number'){input.step='any';input.min='0'; if(key==='frontend.sample_rate_hz'){input.min='1';input.max='20000000';input.step='1';} if(key.endsWith('frequency_hz')){input.max='6000000000';input.step='1';} } input.inputMode = type === 'number' ? 'decimal' : 'text'; input.autocomplete = 'off'; }
    const limits = {'buffer.window_samples':[1,1048576], 'buffer.ingress_blocks':[2,4096], 'buffer.ring_blocks':[2,4096], 'recording.queue_blocks':[2,4096], 'receiver.idle_timeout_ms':[100,3600000], 'receiver.run_seconds':[0,31536000], 'rtlsdr.ppm':[-1000,1000], 'bladerf.gain_db':[-100,100]};
    if (input.type === 'number') { input.step = key === 'receiver.run_seconds' ? 'any' : '1'; if (limits[key]) [input.min,input.max] = limits[key]; }
    const descriptor = moduleForm?.definitions.get(key);
    if (descriptor && input.type === 'number') {
      input.step = descriptor.integer ? '1' : 'any';
      input.min = descriptor.min ?? ''; input.max = descriptor.max ?? '';
    }
    if (scaledFrequency) {
      input.step = String(10 ** -scaledFrequency.decimals);
      if (input.min !== '') input.min = String(Number(input.min) / scaledFrequency.scale);
      if (input.max !== '') input.max = String(Number(input.max) / scaledFrequency.scale);
    }
    input.id = `config_${key.replaceAll('.', '_')}`;
    const displayed = frequencyFromHz(key, values[key]);
    input.value = input.type === 'number' && input.step === 'any' && /^(?:[+-]?(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?)$/i.test(displayed) &&
      Number.isFinite(Number(displayed)) ? String(Number(displayed)) : displayed;
    input.addEventListener('input', () => {
      values[key] = frequencyToHz(key, input.value) ?? input.value.trim();
      if (key === 'rtlsdr.auto_gain' && input.value === 'false' && deviceCaps?.source === 'rtlsdr') {
        const gains = deviceCaps.tuner_gains_tenths_db || [];
        if (gains.length && !gains.some(gain => String(gain) === values['rtlsdr.gain_tenths_db'])) {
          values['rtlsdr.gain_tenths_db'] = String(gains[0]);
          buildForm();
        }
      }
      if (key === 'frontend.type') {
        values['frontend.stream_format']='auto';
        if (values[key]==='sige') {applySigeProfile();buildForm();}
        profileTouched.add('frontend.sample_rate_hz');
      }
      dirtyState(true); updateSourceFields(); refreshProfileErrors();
    });
    input.addEventListener('blur', () => { profileTouched.add(key); refreshProfileErrors(); });
    wrapper.append(input); if (hint) { const small = document.createElement('small'); small.textContent = hint; wrapper.append(small); } grid.append(wrapper);
    attachFieldFeedback(input, wrapper);
  }
  selectSettings(selectedSettings); controls(); refreshProfileErrors();
}
function applySigeProfile(){ Object.assign(values,{'frontend.type':'sige','frontend.sample_rate_hz':'8183800','frontend.center_frequency_hz':'1575381500','frontend.signal_frequency_hz':'1575420000','hackrf.bandwidth_hz':'0','hackrf.lna_gain_db':'0','hackrf.vga_gain_db':'0','hackrf.rf_amp':'false','hackrf.antenna_power':'false','frontend.stream_format':'auto'}); }
function updateSourceFields() {
  const source = values['frontend.type'];
  const caps = deviceCaps && deviceCaps.source === source && deviceCaps.serial === (values['frontend.serial'] || '') && deviceCaps.tuning === values['frontend.center_frequency_hz'] && (source !== 'bladerf' || deviceCaps.precision === values['bladerf.format']) ? deviceCaps : null;
  if (!caps) deviceCaps = null;
  const rate=document.querySelector('[name="frontend.sample_rate_hz"]');
  if(rate) { rate.min=(caps?.sample_rate_min_hz || (source==='hackrf'?2000000:source==='rtlsdr'?225001:1))/1e6; rate.max=(caps?.sample_rate_max_hz || (source==='hackrf'?20000000:source==='rtlsdr'?3200000:61440000))/1e6; }
  const gain=document.querySelector('[name="bladerf.gain_db"]');
  if(gain){
    gain.min=source==='bladerf' && caps ? caps.gain_min_db : -100;
    gain.max=source==='bladerf' && caps ? caps.gain_max_db : 100;
    gain.step=source==='bladerf' && caps ? caps.gain_step_db : 1;
    gain.disabled=!editable()||source!=='bladerf'||!caps;
  }
  const rtlGain=document.querySelector('[name="rtlsdr.gain_tenths_db"]');
  if(rtlGain)rtlGain.disabled=!editable()||source!=='rtlsdr'||values['rtlsdr.auto_gain']!=='false'||!caps;
  const rtlIfGain=document.querySelector('[name="rtlsdr.if_gain_db"]');
  if(rtlIfGain){
    const available=source==='rtlsdr'&&caps?.e4000_if_gain_available;
    rtlIfGain.closest('.field').classList.toggle('hidden',!available);
    rtlIfGain.disabled=!editable()||!available||values['rtlsdr.auto_gain']!=='false';
  }
  const ppm=document.querySelector('[name="rtlsdr.ppm"]');if(ppm){ppm.min=-1000;ppm.max=1000;ppm.step=1;}
  const tuning=document.querySelector('[name="frontend.center_frequency_hz"]');
  if(tuning){tuning.min=(caps?.frequency_min_hz||0)/1e6;tuning.max=(caps?.frequency_max_hz||(source==='rtlsdr'?4294967295:6000000000))/1e6;}
  for(const key of ['frontend.sample_rate_hz','frontend.center_frequency_hz','frontend.signal_frequency_hz','hackrf.bandwidth_hz']) {const input=document.querySelector(`[name="${key}"]`);if(input)input.disabled=!editable()||source==='sige';}
  const bandwidth=document.querySelector('[name="hackrf.bandwidth_hz"]');if(bandwidth)bandwidth.closest('.field')?.classList.toggle('hidden',source==='sige');
  $('sigeInfo').classList.toggle('hidden',source!=='sige'||selectedSettings!=='source');
  for (const group of document.querySelectorAll('[data-group]')) {
    const kind = {'HackRF':'hackrf','RTL-SDR':'rtlsdr','bladeRF':'bladerf','File replay':'file'}[group.dataset.group];
    group.classList.toggle('hidden', Boolean(kind && source !== kind));
  }
}

function controls() {
  const ready = editable(), running = connected() && status?.state === 'running';
  const commandPending = httpBusy || pendingCommand !== null;
  for(const id of ['showSpectrum','showWaterfall','showWaterfall3D'])$(id).disabled=commandPending||['starting','stopping'].includes(status?.state);
  for (const field of $('configForm').elements) field.disabled = !ready;
  for (const id of ['browseReplay', 'filePreset', 'hackrfPreset', 'rtlPreset', 'bladePreset', 'sigePreset', 'deviceLimits', 'importIni', 'useIni', 'iniEditor', 'apply']) $(id).disabled = !ready;
  $('calibrateGains').disabled = !ready || ['file','sige'].includes(values['frontend.type']) || !capabilities?.[values['frontend.type']];
  $('cancelCalibration').classList.toggle('hidden', !calibrationBusy);
  $('cancelCalibration').disabled = !connected() || !calibrationBusy || calibrationStopRequested;
  $('cancelCalibration').textContent = calibrationStopRequested ? 'Stopping…' : 'Stop calibration';
  $('apply').disabled = !ready || referenceSettingsOnly();
  if (capabilities && !capabilities.hackrf) $('hackrfPreset').disabled = true;
  $('rtlPreset').disabled = !ready || !capabilities?.rtlsdr;
  $('bladePreset').disabled = !ready || !capabilities?.bladerf;
  $('sigePreset').disabled = !ready || !capabilities?.sige;
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
  $('streamDescription').textContent = `${sourceName} · Signal ${frequencyFromHz('frontend.signal_frequency_hz', signal)} GHz · SDR ${(hardware/1e6).toFixed(6)} MHz · Digital IF ${((signal-hardware)/1e6).toFixed(6)} MHz · Fs ${(actual/1e6).toFixed(6)} Msps (configured nominal) · ${next.sample_format||'Unknown format'}`;
  $('processed').textContent = pairs(next.processed_samples); $('windows').textContent = `${pairs(next.windows)} common windows`;
  let rate = null;
  if (lastSample && next.wall_seconds > lastSample.time) rate = Number(BigInt(next.received_samples) - lastSample.samples) / (next.wall_seconds - lastSample.time);
  if (!lastSample || next.wall_seconds !== lastSample.time) lastSample = {samples:BigInt(next.received_samples), time:next.wall_seconds};
  $('rate').textContent = next.state !== 'running' ? '—' : rate === null ? $('rate').textContent : `${(Math.max(0, rate) / 1e6).toFixed(3)} Msps`;
  $('elapsed').textContent = `Elapsed ${duration(next.wall_seconds)}`;
  $('written').textContent = bytes(BigInt(next.written_samples) * (['SC16_Q11','CI16'].includes(next.sample_format)?4n:2n));
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
      if (['.cs8','.cu8','.sc16q11','.ci16'].some(ext=>file.name.endsWith(ext))) {
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
function suspendForTerminalShutdown() {
  terminalShutdown = true;
  quitting = true;
  waiting = false;
  statusRequestPending = false;
  pendingCommand = null;
  $('connection').textContent = 'Suspended';
  $('connection').classList.remove('online');
  notice('Receiver closed from the terminal. This console is suspended; restart ngene --web and reload this page to reconnect.');
  log('Receiver announced terminal shutdown.');
  controls();
  socket?.close(1000, 'service stopping');
}
async function connect() {
  if (quitting) return;
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
    socket.onmessage = event => { waiting=false; lastResponse=performance.now(); try { const message=JSON.parse(event.data); if(message?.event==='service_stopping'&&message.source==='terminal'){suspendForTerminalShutdown();return;} pendingTelemetry=message; if(!renderPending){renderPending=true;requestAnimationFrame(()=>{renderPending=false;const latest=pendingTelemetry;pendingTelemetry=null;if(latest && connected() && !quitting)renderStatus(latest);});} } catch(error){log(`Invalid telemetry: ${error.message}`,true);} };
    socket.onclose = () => { waiting = false; pendingCommand = null; $('connection').textContent = terminalShutdown ? 'Suspended' : quitting ? 'Service closed' : 'Disconnected'; $('connection').classList.remove('online'); controls();};
    socket.onerror = () => socket.close();
  } catch (error) { $('connection').textContent = 'Disconnected'; notice(error.message, true); controls(); }
}
const analysisFields = [
  ['spectrum_enabled','Spectrum enabled',['true','false']],
  ['histogram_enabled','Histogram enabled',['true','false']],
  ['fft_mode','Spectrum FFT arithmetic',['float','integer_precise','integer_scaled']],
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
let waterfallAnimationFrame=0;
const waterfallScrollDuration=180;
const waterfallScroll=createWaterfallScroll(waterfallScrollDuration);
let initialDisplayFit=false, displayRangeTouched=false;
let analysisConfigRevision='0', plotConfigBusy=false, lastSpectrumComputation=null;
const plotIds=['spectrum','waterfall','waterfall3D'];
const plotToggle=id=>$(id==='spectrum'?'showSpectrum':id==='waterfall'?'showWaterfall':'showWaterfall3D');
const plotSelection=()=>Object.fromEntries(plotIds.map(id=>[id,plotToggle(id).checked]));
const anyPlotEnabled=()=>plotIds.some(id=>plotToggle(id).checked);
let previousPlotSelection=plotSelection();
const plotWork={spectrum:0,waterfall:0,waterfall3D:0};
let waterfall3DRender={mode:'3d',style:'mesh',rows:0,columns:0};
function diagnosticVisibility(){return document.hidden||activeWorkspaceView!=='rf'?0:(anyPlotEnabled()?1:0)|($('showHistogram').checked?2:0);}
function diagnosticEnabled(name){const value=analysisConfig?.[name]??savedValues[analysisProfileKey(name)];return value!==false&&value!=='false';}
function plotEnabled(id){return diagnosticEnabled('spectrum_enabled')&&plotToggle(id).checked;}
function syncPlotVisibility(){
  const computation=diagnosticEnabled('spectrum_enabled');
  if(!plotConfigBusy&&computation!==lastSpectrumComputation){
    if(!computation){
      if(anyPlotEnabled())previousPlotSelection=plotSelection();
      for(const id of plotIds)plotToggle(id).checked=false;
    }else if(lastSpectrumComputation===false&&!anyPlotEnabled()){
      for(const id of plotIds)plotToggle(id).checked=previousPlotSelection[id];
    }
    lastSpectrumComputation=computation;
  }
  for(const id of ['spectrum','waterfall','waterfall3D']){
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
function analysisProfileKey(name){return name==='fft_mode'?'dsp.spectrum_fft':`analysis.${name}`;}
function acceptAnalysisConfig(result){
  const iniDraft=$('iniEditor').value,customIni=iniDraft!==toIni(values);
  revision=result.revision;analysisConfigRevision=revision;
  for(const [name,value] of Object.entries(result.analysis)){
    const key=analysisProfileKey(name),previous=savedValues[key],next=String(value),input=$(`analysis_${name}`);
    if(name==='spectrum_enabled'||values[key]===previous)values[key]=next;
    if(input&&(name==='spectrum_enabled'||input.value===previous))input.value=next;
    savedValues[key]=next;
  }
  analysisDirty=analysisFields.some(([name])=>$(`analysis_${name}`)?.value!==savedValues[analysisProfileKey(name)]);
  analysisErrors.delete('spectrum_enabled');fieldFeedback($('analysis_spectrum_enabled'),'');
  dirtyState(true);if(customIni)$('iniEditor').value=iniDraft;resetAnalysis(result.analysis);
}
async function changePlotEnabled(event){
  const id=plotIds.find(id=>plotToggle(id)===event.target),selection=plotSelection();
  const before={...selection,[id]:!event.target.checked};
  const previousSelection={...previousPlotSelection},enabled=Object.values(selection).some(Boolean);
  if(enabled)previousPlotSelection={...selection};
  $('plotNotice').classList.add('hidden');
  syncPlotVisibility();schedulePlotDraw();requestStatus();
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
    for(const id of plotIds)plotToggle(id).checked=before[id];previousPlotSelection=previousSelection;
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
    input.value=savedValues[analysisProfileKey(name)] ?? '';input.addEventListener('input',()=>{analysisDirty=true;setAnalysisNotice(analysisHelp);refreshAnalysisErrors();});
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
  for (const name of ['spectrum_source','histogram_source']) {
    if (savedValues[analysisProfileKey(name)]) body[name]=savedValues[analysisProfileKey(name)];
  }
  for(const [name] of analysisFields){const value=$(`analysis_${name}`).value;body[name]=name.endsWith('_enabled')?value==='true':['fft_mode','window','temporal','units','spectrum_source','histogram_source'].includes(name)?value:Number(value);}
  httpBusy=true;controls();
  try {
    const result=await api('analysis/config',body);revision=result.revision;analysisConfigRevision=revision;
    for(const [name,value] of Object.entries(result.analysis)){savedValues[analysisProfileKey(name)]=String(value);values[analysisProfileKey(name)]=String(value);}
    analysisDirty=false;dirtyState(true);resetAnalysis(result.analysis);
    setAnalysisNotice('Applied to this session. Use Apply & save in Receiver configuration while stopped to persist the full profile.');
    log('Analysis parameters applied without restarting the receiver.');requestStatus();
  } catch(error){setAnalysisNotice(error.message,true);log(error.message,true);}
  finally{httpBusy=false;controls();}
});
function resetAnalysis(config=null){analysisSnapshot=null;analysisConfig=config;analysisSequence='0';histogramSequence='';analysisRevision='';waterfallHistory=[];waterfallTime=-Infinity;waterfallScroll.reset();skippedDisplay=0;rejectedFrames=0;syncPlotVisibility();drawAnalysis();}
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
  if(a.revision!==analysisRevision){waterfallHistory=[];waterfallTime=-Infinity;waterfallScroll.reset();analysisSequence='0';histogramSequence='';analysisRevision=a.revision;}
  const incoming=a.sequence_id||'0';
  const spectrumChanged=Boolean(mask&1)&&incoming!==analysisSequence&&a.dbfs.length>0;
  const histogramIncoming=a.histogram_sequence??`${a.histogram_first_sample}:${a.histogram_count}`;
  const histogramChanged=Boolean(mask&2)&&histogramIncoming!==histogramSequence;
  let waterfallChanged=false,waterfall3DChanged=false;
  if(spectrumChanged){
    if(BigInt(incoming)<BigInt(analysisSequence)||!psdIntegrity(a)){
      rejectedFrames++;$('frameIntegrity').textContent=`Rejected invalid/stale PSD: ${rejectedFrames}`;return;
    }
    if(!initialDisplayFit&&!displayRangeTouched){
      const sorted=[...a.dbfs].sort((x,y)=>x-y),middle=sorted[Math.floor(sorted.length/2)];
      const centre=Math.floor(middle/5)*5,half=Math.max(20,Math.ceil((a.max-centre+5)/5)*5);
      for(const id of ['dbMin','colourMin'])$(id).value=Math.max(-250,centre-half);
      for(const id of ['dbMax','colourMax'])$(id).value=Math.min(100,centre+half);
      initialDisplayFit=true;
    }
  }
  if(!spectrumChanged&&!histogramChanged)return;
  if(spectrumChanged){
    if(analysisSequence!=='0')skippedDisplay+=Number(BigInt(incoming)-BigInt(analysisSequence)-1n);
    analysisSequence=incoming;
    if((plotEnabled('waterfall')||plotEnabled('waterfall3D'))&&a.produced_seconds-waterfallTime>=1/Math.max(0.1,Math.min(30,Number($('waterfallRate').value)))){
      waterfallHistory.unshift({values:Object.freeze([...a.dbfs]),first:a.first_sample,epoch:a.epoch,sequence:incoming,produced:a.produced_seconds});
      waterfallHistory.length=Math.min(waterfallHistory.length,historyRows());waterfallTime=a.produced_seconds;
      waterfallScroll.push(performance.now());
      waterfallChanged=true;waterfall3DChanged=true;
    }
  }

  analysisSnapshot={...a,dbfs:spectrumChanged?[...a.dbfs]:(analysisSnapshot?.dbfs||[]),
    histogram_i:histogramChanged?a.histogram_i:(analysisSnapshot?.histogram_i||[]),histogram_q:histogramChanged?a.histogram_q:(analysisSnapshot?.histogram_q||[])};
  const unit=a.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin';
  if(spectrumChanged){
    $('fftBackend').textContent=`${a.config?.fft_mode || 'integer_scaled'} · ${a.backend || 'receiver backend'}`;
    if(plotEnabled('spectrum')){
    $('spectrumInfo').textContent=`${a.fft_size}-point FFT · Δf ${a.bin_hz.toFixed(2)} Hz · ENBW ${a.enbw_hz.toFixed(2)} Hz · ${a.config.window} · ${a.config.averages} Welch segments · after fixed DC removal`;
    $('spectrumPeak').textContent=`Peak ${displayFrequency(a.peak_hz).toFixed(4)} ${frequencyUnit()} · ${a.peak_dbfs.toFixed(1)} ${unit}`;
    }
    $('frameIntegrity').textContent=`PSD #${incoming} · produced ${a.produced_seconds.toFixed(3)} s · min/max/mean ${a.min.toFixed(2)} / ${a.max.toFixed(2)} / ${a.mean.toFixed(2)} · NaN/Inf ${a.nan_count}/${a.inf_count} · hash ${a.checksum.toString(16)} · rejected ${rejectedFrames}`;
    const offset=waterfallScroll.offset(performance.now());
    drawSpectrum();if(waterfallChanged)drawWaterfall(offset);if(waterfall3DChanged)drawWaterfall3D(offset);if(waterfallChanged||waterfall3DChanged)animateWaterfallScroll();
  }
  if(histogramChanged){
    histogramSequence=histogramIncoming;
    $('histogramInfo').textContent=a.histogram_count?`${pairs(a.histogram_count)} pairs · ${a.histogram_i.length} bins · after fixed DC removal`:'Histogram disabled / waiting';
    for(const [id,t] of [['statsI',a.stats_i],['statsQ',a.stats_q]]){
      const dl=document.createElement('dl');
      for(const [label,value] of [['Mean',t.mean],['Std',t.std],['RMS',t.rms],['Min',t.min],['Max',t.max],['Clipping / rail count',t.clipping_count],['Zero count',t.zero_count]]){
        const div=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=label;dd.textContent=Number.isInteger(value)?pairs(value):value.toFixed(4);div.append(dt,dd);dl.append(div);
      }$(id).replaceChildren(dl);
    }
    $('dcMagnitude').textContent=`Fixed DC offset: I ${a.removed_dc_i ?? 0}, Q ${a.removed_dc_q ?? 0} counts · estimated from ${pairs(a.dc_estimate_pairs ?? 0)} pairs · residual DC ${a.dc_magnitude.toFixed(4)} counts · histogram sample ${a.histogram_first_sample} · age ${a.histogram_age_seconds.toFixed(2)} s`;
    $('histogramRails').textContent=`Input stream rail counts, before DC removal: I ${pairs(a.clipped_i)} · Q ${pairs(a.clipped_q)}`;
    drawHistogram('histogramI',analysisSnapshot.histogram_i,'#39d5b0');drawHistogram('histogramQ',analysisSnapshot.histogram_q,'#62a6ff');
  }
  $('analysisCost').textContent=`Worker ${a.compute_ms.toFixed(2)} ms · ${a.sample_backend} · skipped bursts ${a.skipped_bursts} · contended blocks ${a.contention_blocks}`;
}
function historyRows(){return Math.max(32,Math.min(2048,Math.floor(Number($('waterfallRows').value)||128)));}
function frequencyUnit(){return $('frequencyReference').value==='rf'?'MHz':'kHz';}
function displayFrequency(bin){const ref=$('frequencyReference').value,hardware=status?.hardware_frequency_hz||Number(savedValues['frontend.center_frequency_hz'])||0,di=status?.digital_if_hz||0;return ref==='rf'?(hardware+bin)/1e6:ref==='signal'?(bin-di)/1000:bin/1000;}
function frequencyRange(){
  const fs=analysisSnapshot?.sample_rate_hz||Number(savedValues['frontend.sample_rate_hz'])||100000;
  return [displayFrequency(-fs/2),displayFrequency(fs/2)];
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
  const s=surface('spectrum');if(!s)return;++plotWork.spectrum;const [lo,hi]=range(),a=analysisSnapshot;
  const {ctx,left,right,top,bottom}=s,[flo,fhi]=frequencyRange();
  ctx.fillStyle='#8fa6ba';ctx.fillText(a?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin',6,11);
  for(let j=0;j<=4;j++){const y=top+j*(bottom-top)/4;ctx.strokeStyle='#233446';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((hi-j*(hi-lo)/4).toFixed(0),6,y+4);}
  frequencyAxes(s);if(!a?.dbfs.length){ctx.fillText('Waiting for a complete spectrum burst',left+20,top+35);return;}
  ctx.save();ctx.beginPath();ctx.rect(left,top,right-left,bottom-top);ctx.clip();ctx.strokeStyle='#27b6ff';ctx.lineWidth=1.2;ctx.beginPath();
  a.dbfs.forEach((v,k)=>{const f=displayFrequency((k/a.dbfs.length-0.5)*a.sample_rate_hz),x=left+(f-flo)/(fhi-flo)*(right-left),y=bottom-Math.max(0,Math.min(1,(v-lo)/(hi-lo)))*(bottom-top);if(k)ctx.lineTo(x,y);else ctx.moveTo(x,y);});ctx.stroke();ctx.lineTo(right,bottom);ctx.lineTo(left,bottom);ctx.closePath();const fill=ctx.createLinearGradient(0,top,0,bottom);fill.addColorStop(0,'#2aaaffcc');fill.addColorStop(0.6,'#0062ffbb');fill.addColorStop(1,'#06145800');ctx.fillStyle=fill;ctx.fill();ctx.restore();
}
const waterfallBitmap=document.createElement('canvas');
const waterfallPalette=Array.from({length:256},(_,n)=>{
  const stops=[[0,16,100],[0,105,238],[0,205,175],[15,227,67],[239,237,45],[255,55,24]];
  const x=n/255*(stops.length-1),i=Math.min(stops.length-2,Math.floor(x)),f=x-i;
  return stops[i].map((c,k)=>Math.round(c+(stops[i+1][k]-c)*f));
});
const waterfall3DBitmap=document.createElement('canvas');
let waterfallSurface=null;
let waterfallRender={mode:'2d',rows:0,columns:0};
function drawColourBar(s,palette,lo,hi){
  const {ctx,top,bottom,right}=s,x=right+4,width=8,height=bottom-top;
  for(let j=0;j<height;j++){
    const colour=palette[Math.round(255*(1-j/Math.max(1,height-1)))];
    ctx.fillStyle=`rgb(${colour.join(',')})`;ctx.fillRect(x,top+j,width,1);
  }
  ctx.fillStyle='#9eb6c9';ctx.fillText(String(hi),right-30,top-4);ctx.fillText(String(lo),right-65,top-4);
}
function waterfallAxes(s,frequencyScale=1){
  const {ctx,left,right,top,bottom}=s,[lo,hi]=frequencyRange();
  const duration=(historyRows()-1)/Math.max(0.1,Math.min(30,Number($("waterfallRate").value)));
  ctx.fillStyle="#8fa6ba";ctx.textAlign="right";
  for(let j=0;j<=4;j++){
    const y=(top+bottom)/2+(0.5-j/4)*(bottom-top)*frequencyScale;
    ctx.fillText((lo+j*(hi-lo)/4).toFixed($("frequencyReference").value==="rf"?3:1),left-4,y+3);
    const x=left+j*(right-left)/4;
    ctx.fillText(j===4?"0 s":"-"+(duration*(1-j/4)).toFixed(1)+" s",Math.max(left+32,x),bottom+18);
  }
  ctx.textAlign="left";ctx.fillText(frequencyUnit(),left,top-4);
}
function drawWaterfall(rowOffset=waterfallScroll.offset(performance.now())){
  if(!(diagnosticMask()&1)||!plotEnabled('waterfall'))return;
  $('waterfallInfo').textContent='Newest at right · '+waterfallHistory.length+' snapshots';
  const [lo,hi]=range(true);
  $('colourLegend').textContent=`Global colours ${lo} … ${hi} ${analysisSnapshot?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin'}`;
  const s=surface('waterfall');if(!s)return;
  waterfallRender={mode:'2d',rows:0,columns:0};++plotWork.waterfall;
  const {ctx,left,right,top,bottom}=s;
  const rows=historyRows(),available=Math.min(rows,waterfallHistory.length),columns=analysisSnapshot?.dbfs.length||1024;
  waterfallBitmap.width=rows;waterfallBitmap.height=columns;
  const bitmap=waterfallBitmap.getContext('2d'),pixels=bitmap.createImageData(rows,columns);
  for(let j=0;j<rows&&available;j++){
    const row=waterfallHistory[Math.min(j,available-1)];
    for(let k=0;k<columns;k++){
      const level=Math.max(0,Math.min(255,Math.round(255*(row.values[k]-lo)/(hi-lo))));
      const colour=waterfallPalette[level],offset=4*((columns-1-k)*rows+rows-1-j);
      pixels.data[offset]=colour[0];pixels.data[offset+1]=colour[1];pixels.data[offset+2]=colour[2];pixels.data[offset+3]=255;
    }
  }
  bitmap.putImageData(pixels,0,0);ctx.imageSmoothingEnabled=false;
  const step=(right-left)/(rows-1),shift=Math.min(rows-1,rowOffset)*step;
  ctx.save();ctx.beginPath();ctx.rect(left,top,right-left,bottom-top);ctx.clip();
  ctx.drawImage(waterfallBitmap,left-step/2+shift,top,rows*step,bottom-top);
  if(shift>step/2)ctx.drawImage(waterfallBitmap,0,0,1,columns,left,top,shift-step/2,bottom-top);
  ctx.restore();drawColourBar(s,waterfallPalette,lo,hi);waterfallAxes(s);
  waterfallRender={mode:'2d',rows:available,gridRows:rows,rowOffset,columns,newest:waterfallHistory[0]?.sequence??null,oldest:waterfallHistory.at(-1)?.sequence??null};
}
/** Project the shared history as a solid frequency/time/power surface. */
function drawWaterfall3D(rowOffset=waterfallScroll.offset(performance.now())){
  if(!(diagnosticMask()&1)||!plotEnabled('waterfall3D'))return;
  const s=surface('waterfall3D');if(!s)return;++plotWork.waterfall3D;
  const {ctx,left,right,top,bottom}=s,[lo,hi]=range(true);
  const rows=historyRows(),columns=analysisSnapshot?.dbfs.length||1024;
  const imageColumns=Math.max(2,Math.round(right-left));
  const imageRows=Math.max(2,Math.round(bottom-top));
  let geometry;
  try{
    waterfallSurface??=createWaterfallSurface(waterfall3DBitmap);
    geometry=waterfallSurface.draw(waterfallHistory,columns,rows,lo,hi,imageColumns,imageRows,rowOffset);
  }catch(error){$('waterfall3DInfo').textContent=error.message;ctx.fillStyle='#ffb4b8';ctx.fillText(error.message,left+15,top+25);return;}
  const floor=ctx.createLinearGradient(0,top,0,bottom);floor.addColorStop(0,'#071b42');floor.addColorStop(1,'#031344');
  ctx.fillStyle=floor;ctx.fillRect(left,top,right-left,bottom-top);ctx.imageSmoothingEnabled=true;
  ctx.strokeStyle='#233446';ctx.lineWidth=1;
  for(let j=0;j<=4;j++){
    const y=top+j*(bottom-top)/4,x=left+j*(right-left)/4;
    ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();
    ctx.beginPath();ctx.moveTo(x,top);ctx.lineTo(x,bottom);ctx.stroke();
  }
  ctx.drawImage(waterfall3DBitmap,left,top,right-left,bottom-top);
  drawColourBar(s,reliefPalette,lo,hi);waterfallAxes(s,geometry.projection[1]);
  $('waterfall3DInfo').textContent='3D power surface | '+waterfallHistory.length+' snapshots | live scroll | synchronized with 2D';
  waterfall3DRender={mode:'3d',...geometry,newest:waterfallHistory[0]?.sequence??null,oldest:waterfallHistory.at(-1)?.sequence??null};
}
function animateWaterfallScroll(){
  if(waterfallAnimationFrame||waterfallHistory.length<2||paused||!(plotEnabled('waterfall')||plotEnabled('waterfall3D')))return;
  let last=0;
  const tick=now=>{
    waterfallAnimationFrame=0;
    if(paused||!(diagnosticMask()&1)||!(plotEnabled('waterfall')||plotEnabled('waterfall3D'))||waterfallHistory.length<2)return;
    const offset=waterfallScroll.offset(now),moving=offset>0;
    if(now-last>=32||!moving){drawWaterfall(offset);drawWaterfall3D(offset);last=now;}
    if(moving)waterfallAnimationFrame=requestAnimationFrame(tick);
  };
  waterfallAnimationFrame=requestAnimationFrame(tick);
}
function drawHistogram(id,data,color){
  if(!(diagnosticMask()&2))return;
  const s=surface(id);if(!s)return;const {ctx,w,h,left,right,top,bottom}=s;const percent=$('histScale').value==='percent',count=analysisSnapshot?.histogram_count||1;
  const values=(data||[]).map(v=>percent?100*v/count:v),max=Number($('histMax').value)>0?Number($('histMax').value):Math.max(1,...values);
  ctx.fillStyle='#8fa6ba';ctx.fillText(percent?'%':'count',4,11);
  for(let j=0;j<=2;j++){const y=top+j*(bottom-top)/2;ctx.strokeStyle='#233446';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((max*(1-j/2)).toFixed(percent?1:0),4,y+4);}
  const dw=(right-left)/(values.length||1);ctx.fillStyle=color;
  values.forEach((v,k)=>{const bh=(bottom-top)*Math.min(1,v/max);ctx.fillRect(left+k*dw,bottom-bh,Math.max(1,dw-1),bh);});
  const minimum=analysisSnapshot?.histogram_min??-128,maximum=analysisSnapshot?.histogram_max??127,span=maximum-minimum+1;
  const amplitudeX=value=>left+(value-minimum+0.5)/span*(right-left);
  ctx.save();ctx.fillStyle='#8fa6ba';ctx.textAlign='center';
  for(let j=0;j<=4;j++){
    const value=j===4?maximum:Math.round(minimum+j*span/4);
    ctx.fillText(String(value),amplitudeX(value),h-10);
  }
  if(minimum<=0&&maximum>=0){
    ctx.strokeStyle='#b3c3d0';ctx.setLineDash([3,3]);ctx.beginPath();ctx.moveTo(amplitudeX(0),top);ctx.lineTo(amplitudeX(0),bottom);ctx.stroke();ctx.setLineDash([]);
  }
  const mean=analysisSnapshot?.[id==='histogramI'?'stats_i':'stats_q']?.mean;
  if(Number.isFinite(mean)){
    ctx.strokeStyle=color;ctx.beginPath();ctx.moveTo(amplitudeX(mean),top);ctx.lineTo(amplitudeX(mean),bottom);ctx.stroke();
    ctx.textAlign='right';ctx.fillStyle=color;ctx.fillText(`Mean ${mean.toFixed(4)} · ${(span/(values.length||1)).toFixed(0)} counts/bin`,right,11);
  }
  ctx.restore();
}
function drawAnalysis(){const offset=waterfallScroll.offset(performance.now());drawSpectrum();drawWaterfall(offset);drawWaterfall3D(offset);drawHistogram('histogramI',analysisSnapshot?.histogram_i,'#39d5b0');drawHistogram('histogramQ',analysisSnapshot?.histogram_q,'#62a6ff');}
let plotDrawPending=false;
function schedulePlotDraw(){if(plotDrawPending||!diagnosticMask())return;plotDrawPending=true;requestAnimationFrame(()=>{plotDrawPending=false;drawAnalysis();});}
function clearWaterfallHistory(){waterfallHistory=[];waterfallTime=-Infinity;waterfallScroll.reset();drawWaterfall();drawWaterfall3D();}
$('clearWaterfall').addEventListener('click',clearWaterfallHistory);
$('clearWaterfall3D').addEventListener('click',clearWaterfallHistory);
$('waterfallRows').addEventListener('change',()=>{waterfallHistory.length=Math.min(waterfallHistory.length,historyRows());waterfallScroll.reset();drawAnalysis();});
for(const id of ['dbMin','dbMax','colourMin','colourMax','waterfallRate','histScale','histMax'])$(id).addEventListener('change',()=>{if(['dbMin','dbMax','colourMin','colourMax'].includes(id))displayRangeTouched=true;drawAnalysis();});
$('configForm').addEventListener('submit', event => event.preventDefault());
$('apply').addEventListener('click', () => applyConfig()); $('validate').addEventListener('click', () => applyConfig(true));
$('start').addEventListener('click', () => command('receiver/start')); $('stop').addEventListener('click', () => command('receiver/stop'));
$('recordStart').addEventListener('click', () => command('recording/start')); $('recordStop').addEventListener('click', () => command('recording/stop'));
$('reloadFiles').addEventListener('click', refreshFiles); $('clearLog').addEventListener('click', () => $('events').replaceChildren());
$('pausePlot').addEventListener('click', () => { paused = !paused; if(paused)waterfallScroll.pause(performance.now());else waterfallScroll.resume(performance.now());$('pausePlot').textContent = paused ? 'Resume plots' : 'Pause plots'; if (!paused && status)renderAnalysis(status.analysis,status.revision);if(!paused)animateWaterfallScroll();requestStatus(); });
$('useIni').addEventListener('click', () => { try { values = parseIni($('iniEditor').value); buildForm(); dirtyState(true); } catch (error) { notice(error.message, true); } });
$('importIni').addEventListener('change', async event => { try { const file = event.target.files[0]; if (!file) return; if (file.size > 12000) throw new Error('INI file exceeds 12 kB.'); values = parseIni(await file.text()); buildForm(); dirtyState(true); notice('INI imported into draft. Validate and apply before starting.'); } catch (error) { notice(error.message, true); } finally { event.target.value = ''; } });
$('exportIni').addEventListener('click', () => { const url = URL.createObjectURL(new Blob([toIni(values)], {type:'text/plain'})), link = document.createElement('a'); link.href = url; link.download = 'receiver.ini'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); });
$('rtlPreset').addEventListener('click', () => {
  deviceCaps = null;
  Object.assign(values, {'frontend.type':'rtlsdr','frontend.stream_format':'auto','frontend.sample_rate_hz':'2400000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'2400','hackrf.bandwidth_hz':'0','rtlsdr.auto_gain':'true','rtlsdr.gain_tenths_db':'0','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'});
  buildForm(); dirtyState(true);
});
$('sigePreset').addEventListener('click',()=>{if(!editable())return;applySigeProfile();dirtyState(true);buildForm();});
$('bladePreset').addEventListener('click', () => {
  deviceCaps = null;
  Object.assign(values, {'frontend.type':'bladerf','frontend.stream_format':'auto','frontend.sample_rate_hz':'16000000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'16000','hackrf.bandwidth_hz':'0','bladerf.gain_db':'30','bladerf.format':'SC16_Q11','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'});
  buildForm(); dirtyState(true);
});
let capabilityRequestId = 0;
function calibrationDetails(result) {
  return `${result.device_model ? result.device_model+' · '+result.gain_policy+' · ' : ''}RMS ${(result.rms_fraction * 100).toFixed(1)}% of ADC range · DC ${(result.dc_fraction * 100).toFixed(2)}% · near clipping ${(result.clipped_fraction * 100).toFixed(3)}% · distribution error ${Number(result.gaussian_error).toFixed(2)} · I/Q imbalance ${(result.iq_imbalance * 100).toFixed(1)}%`;
}
function renderCalibration(result) {
  const job = BigInt(result.job_id || '0');
  if (job < calibrationSeenJob) return;
  calibrationSeenJob = job;
  if (!calibrationBusy && result.state === 'running') notice('Gain calibration running. You can cancel it, view the console or exit the service.');
  calibrationBusy = result.state === 'running';
  calibrationStopRequested = calibrationBusy && (calibrationStopRequested || result.cancel_requested);
  if (calibrationBusy) {
    const gains = values['frontend.type'] === 'hackrf' ?
      ` · testing LNA ${result.current_lna_gain_db} dB, VGA ${result.current_vga_gain_db} dB, RF amp ${result.current_rf_amp ? 'on' : 'off'}` :
      values['frontend.type'] === 'rtlsdr' ? ` · testing RF ${result.current_rtl_gain_tenths_db / 10} dB${result.e4000_if_gain_available ? ', IF '+result.current_rtl_if_gain_db+' dB' : ''}` : '';
    $('calibrationStatus').textContent = `${result.cancel_requested ? 'Cancelling; releasing the SDR' : 'Calibrating'}${result.device_model ? ' · '+result.device_model : ''} · ${result.tested / 3} settings completed · ${Math.floor(result.elapsed_seconds)} s${gains}`;
    $('cancelCalibration').dataset.jobId = result.job_id;
    controls();
    return;
  }
  controls();
  if (result.state === 'idle' || calibrationTerminal === `${result.job_id}:${result.state}`) return;
  calibrationTerminal = `${result.job_id}:${result.state}`;
  if (result.state === 'failed' || result.state === 'cancelled') {
    const message = result.state === 'cancelled' ? 'Calibration cancelled. Existing gains were kept.' :
      `Calibration failed: ${result.error || 'SDR measurement failed'}. Existing gains were kept.`;
    $('calibrationStatus').textContent = message;
    notice(message, result.state === 'failed');
  } else if (!result.accepted) {
    const message = `No setting passed all three windows. Gains were kept. Rejected settings (reasons can overlap): weak ${result.rejected_weak}, overload ${result.rejected_overload}, non-Gaussian ${result.rejected_distribution}, I/Q imbalance ${result.rejected_imbalance}. Best measured: ${calibrationDetails(result)}.`;
    $('calibrationStatus').textContent = message;
    notice('Gain calibration finished without an accepted setting. See the measurement details.', true);
  } else if (calibrationJobId === result.job_id && calibrationRequest) {
    if (result.revision !== revision || calibrationRequest.revision !== revision || toIni(values) !== calibrationRequest.ini) {
      $('calibrationStatus').textContent = 'Calibration completed, but the profile changed. Suggestion was not applied.';
    } else {
      const source = calibrationRequest.source;
      if (source === 'hackrf') Object.assign(values, {'hackrf.lna_gain_db':String(result.lna_gain_db), 'hackrf.vga_gain_db':String(result.vga_gain_db), 'hackrf.rf_amp':String(result.rf_amp)});
      if (source === 'rtlsdr') Object.assign(values, {'rtlsdr.auto_gain':String(result.rtl_auto_gain), 'rtlsdr.gain_tenths_db':String(result.rtl_gain_tenths_db), 'rtlsdr.if_gain_db':String(result.rtl_if_gain_db)});
      if (source === 'bladerf') values['bladerf.gain_db'] = String(result.blade_gain_db);
      deviceCaps = source === 'hackrf' ? null : {...result, source, serial:values['frontend.serial'] || '', tuning:values['frontend.center_frequency_hz'], precision:values['bladerf.format']};
      buildForm(); dirtyState(true);
      $('calibrationStatus').textContent = `Suggested ADC loading from ${result.tested / 3} settings. ${calibrationDetails(result)}. GNSS sensitivity is not measured. Review before Apply & save.`;
      notice('Gain calibration completed. Review and save the suggested gains.');
    }
  } else {
    $('calibrationStatus').textContent = `Calibration completed in another page. No draft changes were applied here. ${calibrationDetails(result)}.`;
  }
  calibrationJobId = null; calibrationRequest = null;
}
async function pollCalibration() {
  if (!connected() || calibrationPolling || httpBusy) return;
  calibrationPolling = true;
  try { const result = await api('frontend/calibrate'); if (!httpBusy) renderCalibration(result); }
  catch (error) { if (calibrationBusy) $('calibrationStatus').textContent = `Calibration status unavailable: ${error.message}. Reconnecting does not cancel the SDR job.`; }
  finally { calibrationPolling = false; }
}
$('cancelCalibration').addEventListener('click', async () => {
  calibrationStopRequested = true; controls();
  try { renderCalibration(await api('frontend/calibrate/cancel', {job_id:$('cancelCalibration').dataset.jobId})); }
  catch (error) { calibrationStopRequested = false; controls(); notice(error.message, true); }
});
setInterval(pollCalibration, 750);
$('calibrateGains').addEventListener('click', async () => {
  if (!editable() || ['file','sige'].includes(values['frontend.type']) || !validateProfileFields()) return;
  const requested = {revision, ini:toIni(values), source:values['frontend.type']};
  httpBusy = true; controls();
  $('calibrationStatus').textContent = 'Starting gain calibration…';
  try {
    const result = await api('frontend/calibrate', {revision:requested.revision, ini:requested.ini});
    calibrationJobId = result.job_id; calibrationRequest = requested;
    renderCalibration(result);
  } catch (error) {
    $('calibrationStatus').textContent = error.message;
    notice(error.message, true);
  } finally { httpBusy = false; controls(); }
});
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
$('filePreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'file','frontend.stream_format':'auto','replay.realtime':'true','replay.loop':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('frequencyReference').addEventListener('change',drawAnalysis);
$('autoScale').addEventListener('click',()=>{if(!anyPlotEnabled()||!diagnosticEnabled('spectrum_enabled')||!analysisSnapshot?.dbfs.length)return;const lo=Math.floor(Math.min(...analysisSnapshot.dbfs)/10)*10,hi=Math.ceil(Math.max(...analysisSnapshot.dbfs)/10)*10+10;for(const id of ['dbMin','colourMin'])$(id).value=lo;for(const id of ['dbMax','colourMax'])$(id).value=hi;drawAnalysis();});
for(const id of ['showSpectrum','showWaterfall','showWaterfall3D'])$(id).addEventListener('change',changePlotEnabled);
$('histogramPanel').querySelector('.panel-heading').append($('showHistogram').parentElement);
$('showHistogram').parentElement.classList.add('plot-toggle');
$('showHistogram').parentElement.lastChild.textContent=' Enable';
const histogramOff=document.createElement('p');
histogramOff.className='plot-disabled muted hidden';histogramOff.textContent='I/Q histograms disabled.';
$('histogramPanel').append(histogramOff);
function syncHistogramVisibility(){
  const enabled=$('showHistogram').checked;
  if(!enabled)$('histogramInfo').textContent='Off';
  for(const selector of ['.histogram-grid','.stats-grid','.table-note','.plot-footer'])
    $('histogramPanel').querySelector(selector)?.classList.toggle('hidden',!enabled);
  histogramOff.classList.toggle('hidden',enabled);
}
$('showHistogram').addEventListener('change',()=>{syncHistogramVisibility();
  if($('showHistogram').checked&&!paused&&status)renderAnalysis(status.analysis,status.revision);
  requestStatus();});
syncHistogramVisibility();
document.addEventListener('visibilitychange',requestStatus);
$('hackrfPreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'hackrf','frontend.stream_format':'auto','frontend.sample_rate_hz':'16000000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'16000','hackrf.bandwidth_hz':'0','hackrf.lna_gain_db':'16','hackrf.vga_gain_db':'20','hackrf.rf_amp':'false','hackrf.antenna_power':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('exit').addEventListener('click', async () => { if (!confirm('Stop the receiver, finish recording and close the local NGene service?')) return; try { await api('shutdown', {}); quitting = true; notice('NGene is closing. Restart ngene --web to reconnect.'); } catch (error) { notice(error.message, true); } });
setInterval(() => { if (waiting && performance.now() - lastRequest > 4000) socket?.close(); else if (statusRequestPending||performance.now() - lastRequest >= Number($('refresh').value)) requestStatus(); }, 10);

const plotResizeObserver=new ResizeObserver(schedulePlotDraw);
for(const id of ['spectrum','waterfall','waterfall3D','histogramI','histogramQ'])plotResizeObserver.observe($(id));
let observedChannelId=0,observedSent=0,observedPending=false;
receiverWorkspace = createReceiverWorkspace({
  onChannelInterest(id) {
    if(!connected()||observedPending||id===observedChannelId&&(!id||performance.now()-observedSent<800))return;
    observedChannelId=id;observedSent=performance.now();observedPending=true;
    api('channels/observe',{channel_id:id}).catch(()=>{}).finally(()=>{observedPending=false;});
  },
  onViewChange(view) {
    activeWorkspaceView = view;
    requestStatus();
    if (view === 'rf') requestAnimationFrame(drawAnalysis);
  },
  onOpenSettings(topic) { showView('setup'); selectSettings(topic); },
  onPlotInterestChange() { requestStatus(); },
  onBrowseReplay() {
    if (!editable()) { notice('Stop the receiver before choosing another input file.', true); return; }
    showView('setup'); selectSettings('source'); $('browseReplay').click();
  }
});
connect();

/* Read-only inspection for frame troubleshooting and browser regression tests. */
export function diagnosticSnapshot(){return {sequence:analysisSequence,skipped:skippedDisplay,rejected:rejectedFrames,work:{...plotWork},waterfall:{...waterfallRender},waterfall3D:{...waterfall3DRender,history:waterfallHistory.length},history:waterfallHistory.map(row=>({...row,values:[...row.values]}))};}
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
    lines.push(['Metadata', result.metadata_path], ['Sample rate', `${frequencyFromHz('frontend.sample_rate_hz', settings['frontend.sample_rate_hz'])} Msps (nominal)`],
      ['SDR tuning', `${frequencyFromHz('frontend.center_frequency_hz', settings['frontend.center_frequency_hz'])} MHz`], ['Signal centre', `${frequencyFromHz('frontend.signal_frequency_hz', settings['frontend.signal_frequency_hz'])} GHz`],
      ['Digital IF', `${(Number(settings['frontend.signal_frequency_hz']) - Number(settings['frontend.center_frequency_hz'])) / 1e6} MHz`],
      ['Saved samples', result.recording_point], ['Recorded stream processing', result.recorded_conditioning],
      ['Completion', result.complete ? 'Complete' : 'Incomplete / interrupted'], ['Stream gaps', result.has_gaps ? 'Present — see sample index' : 'See session sample index'],
      ['Replay samples', 'Saved sample values are preserved']);
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
  const keys = ['frontend.type','replay.format','replay.path','replay.start_sample','replay.sample_limit','receiver.run_seconds','receiver.mode','recording.enabled'];
  if (replaySelection.metadata_available) keys.push('frontend.sample_rate_hz','frontend.center_frequency_hz','frontend.signal_frequency_hz','hackrf.bandwidth_hz','buffer.window_samples','replay.spectrum_inverted');
  for (const key of keys) values[key] = draft[key];
  buildForm(); dirtyState(true); selectSettings('source'); $('showConfiguration').click();
  notice(replaySelection.metadata_available ? 'Replay metadata loaded into the draft. Review, Apply & save, then Start receiver.' : 'File selected. Set sampling rate and RF parameters, then Apply & save.');
  $('replayFileDialog').close();
});

/** Fullscreen keeps the original plot mounted so live updates and controls retain their state. */
function addPlotFullscreen() {
  const plots = document.querySelectorAll(
    '.plot-wrap canvas, .histogram-grid canvas, .caf-view canvas, .tracking-panel > svg, [data-pvt-sky], [data-pvt-map]'
  );
  let expanded = null;
  const update = () => {
    for (const button of document.querySelectorAll('.plot-fullscreen-toggle')) {
      const active = button.parentElement === document.fullscreenElement || button.parentElement === expanded;
      button.setAttribute('aria-pressed', String(active));
      button.title = button.getAttribute('aria-label').replace(/^(?:Exit fullscreen|Fullscreen)/, active ? 'Exit fullscreen' : 'Fullscreen');
      button.setAttribute('aria-label', button.title);
      const scatter = button.parentElement.querySelector('[data-tracking-scatter]');
      if (scatter) scatter.setAttribute('viewBox', active ? '185 0 270 260' : '0 0 640 260');
    }
  };
  document.addEventListener('fullscreenchange', update);
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && expanded) {
      const button = expanded.querySelector('.plot-fullscreen-toggle');
      expanded.classList.remove('plot-expanded');
      expanded = null;
      update();
      button.focus();
    }
  });
  for (const plot of plots) {
    const panel = plot.matches('[data-pvt-map]') ? plot : plot.parentElement;
    if (panel.classList.contains('plot-fullscreen')) continue;
    panel.classList.add('plot-fullscreen');
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'plot-fullscreen-toggle';
    const icon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    icon.setAttribute('viewBox', '0 0 24 24');
    icon.setAttribute('aria-hidden', 'true');
    for (const [state, shape] of [['enter','M9 4H4V9 M15 4H20V9 M4 15V20H9 M20 15V20H15'],
                                  ['exit','M4 9H9V4 M20 9H15V4 M9 20V15H4 M15 20V15H20']]) {
      const path = document.createElementNS(icon.namespaceURI, 'path');
      path.setAttribute('d', shape);
      path.setAttribute('class', 'fullscreen-' + state);
      icon.append(path);
    }
    button.append(icon);
    button.setAttribute('aria-label', 'Fullscreen ' + (plot.getAttribute('aria-label') ||
      (plot.matches('[data-pvt-map]') ? 'position map' : 'sky plot')));
    button.addEventListener('click', async () => {
      if (document.fullscreenElement === panel) {
        await document.exitFullscreen();
      } else if (expanded === panel) {
        panel.classList.remove('plot-expanded');
        expanded = null;
      } else {
        try {
          await panel.requestFullscreen();
        } catch {
          expanded?.classList.remove('plot-expanded');
          expanded = panel;
          panel.classList.add('plot-expanded');
        }
      }
      update();
    });
    panel.append(button);
  }
  update();
}
addPlotFullscreen();
