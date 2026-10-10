let selectedSession = 'primary';
const sessionPrefix = () => selectedSession === 'replay' ? 'replay/' : '';
/* NGene browser UI: presentation and commands only. No receiver/DSP implementation. */
const $ = id => document.getElementById(id);
const fields = [
  ['Frontend', 'frontend.type', 'Source', 'select', ['file', 'hackrf']],
  ['Frontend', 'frontend.serial', 'Device serial', 'text', null, '', 'Optional when exactly one HackRF is connected'],
  ['Frontend', 'frontend.sample_rate_hz', 'Sample rate', 'number', null, 'pairs/s'],
  ['Frontend', 'frontend.center_frequency_hz', 'SDR hardware tuning frequency', 'number', null, 'Hz'],
  ['Frontend', 'frontend.signal_frequency_hz', 'Signal centre frequency', 'number', null, 'Hz', '0 = follow hardware; digital IF = signal − hardware'],
  ['HackRF', 'hackrf.bandwidth_hz', 'Analogue filter', 'number', null, 'Hz', '0 = automatic supported filter'],
  ['HackRF', 'hackrf.lna_gain_db', 'LNA gain', 'select', [0,8,16,24,32,40], 'dB · step 8'],
  ['HackRF', 'hackrf.vga_gain_db', 'VGA gain', 'select', Array.from({length:32},(_,k)=>k*2), 'dB · step 2'],
  ['HackRF', 'hackrf.rf_amp', 'RF amplifier', 'select', ['false', 'true']],
  ['HackRF', 'hackrf.antenna_power', 'Antenna power', 'select', ['false', 'true']],
  ['File replay', 'replay.path', 'Input file on receiver computer', 'text', null, '', 'Signed interleaved I/Q (.cs8); configure sample rate and RF metadata above'],
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
let waiting = false, lastRequest = 0, lastResponse = -Infinity, lastSample = null, lastMessage = '', lastCommand = '', lastRun = '';
let paused = false, fileBusy = false, lastFileRefresh = 0;
let selectedSettings = 'source', deletingFile = false;
let quitting = false, pendingTelemetry=null, renderPending=false;

function log(message, error = false) {
  const item = document.createElement('li'), time = document.createElement('time'), text = document.createElement('span');
  time.textContent = new Date().toLocaleTimeString('en-GB'); text.textContent = message;
  if (error) item.classList.add('error'); item.append(time, text); $('events').prepend(item);
  while ($('events').children.length > 50) $('events').lastChild.remove();
}
function notice(message, error = false) { $('notice').textContent = message; $('notice').classList.toggle('error', error); }
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
  $('configMessage').textContent = dirty ? 'Unsaved changes. Apply & save before starting.' : `Profile r${revision} · parameters lock during reception.`;
  $('iniEditor').value = toIni(values); controls();
}
const settingsTopics = {
  source: ['Input source', 'Choose the SDR, tuning and gains before starting.', 'Before start'],
  receiver: ['Receiver & buffers', 'Set the shared stream, conditioning and buffering policy.', 'Before start'],
  recording: ['Capture to disk', 'Choose a name, receiver-side folder and pre/post capture point.', 'Before start'],
  analysis: ['Diagnostic processing', 'NGene computes PSD and histograms. Apply these settings during reception.', 'Runtime'],
  display: ['Plot appearance', 'Adjust axes, colours and history in this browser.', 'Display only'],
  profile: ['Profile tools', 'Import, export, validate or inspect the complete receiver profile.', 'INI profile']
};
/** Show a bounded set of settings without changing the INI profile or diagnostics subscription. */
function selectSettings(topic) {
  selectedSettings = topic;
  document.querySelectorAll('[data-settings]').forEach(button => { const active = button.dataset.settings === topic; button.classList.toggle('active', active); button.setAttribute('aria-pressed', String(active)); });
  const [title, hint, mode] = settingsTopics[topic]; $('settingsTitle').textContent = title; $('settingsHint').textContent = hint; $('settingsMode').textContent = mode;
  $('configForm').classList.toggle('hidden', !['source', 'receiver', 'recording'].includes(topic));
  const groups = {source: ['Frontend', 'HackRF', 'File replay'], receiver: ['Receiver', 'Buffers'], recording: ['Recording']};
  document.querySelectorAll('#fields [data-group]').forEach(group => group.classList.toggle('topic-hidden', !(groups[topic] || []).includes(group.dataset.group)));
  $('sourcePresets').classList.toggle('hidden', topic !== 'source');
  for (const [id, key] of [['analysisSettings', 'analysis'], ['displaySettings', 'display'], ['profileSettings', 'profile']]) $(id).classList.toggle('hidden', topic !== key);
  updateSourceFields();
}
function validateProfileFields() {
  const invalid = [...$('configForm').elements].find(input => !input.checkValidity());
  if (!invalid) return true;
  const group = invalid.closest('[data-group]')?.dataset.group;
  selectSettings(group === 'Recording' ? 'recording' : ['Receiver', 'Buffers'].includes(group) ? 'receiver' : 'source');
  invalid.reportValidity(); return false;
}
function buildForm() {
  const holder = $('fields'); holder.replaceChildren(); let group = '', grid;
  for (const [section, key, label, type, options, unit, hint] of fields) {
    if (section !== group) {
      const fieldset = document.createElement('fieldset'), legend = document.createElement('legend');
      fieldset.className = 'config-group'; fieldset.dataset.group = section; legend.textContent = section;
      grid = document.createElement('div'); grid.className = 'field-grid'; fieldset.append(legend, grid); holder.append(fieldset); group = section;
    }
    const wrapper = document.createElement('label'), title = document.createElement('span'), units = document.createElement('span');
    wrapper.className = `field${['frontend.serial', 'frontend.center_frequency_hz', 'recording.prefix', 'recording.directory', 'recording.point', 'recording.enabled', 'buffer.overflow_policy', 'replay.path'].includes(key) ? ' full' : ''}`;
    title.textContent = label; units.className = 'unit'; units.textContent = unit || ''; title.append(units); wrapper.append(title);
    const input = document.createElement(type === 'select' ? 'select' : 'input'); input.name = key;
    if (type === 'select') for (const option of options) { const element = document.createElement('option'); element.value = option; element.textContent = option === 'true' ? 'On' : option === 'false' ? 'Off' : option === 'record' ? 'Record only' : option === 'capture' ? 'Capture' : option; input.append(element); }
    else { input.type = type; if(type==='number'){input.step='any';input.min='0'; if(key==='conditioning.dc_shift'){input.min='4';input.max='24';input.step='1';} if(key==='frontend.sample_rate_hz'){input.min='1';input.max='20000000';input.step='1';} if(key.endsWith('frequency_hz')){input.max='6000000000';input.step='1';} } input.inputMode = type === 'number' ? 'decimal' : 'text'; input.autocomplete = 'off'; }
    input.value = values[key] ?? ''; input.addEventListener('input', () => { values[key] = input.value.trim(); dirtyState(true); updateSourceFields(); });
    wrapper.append(input); if (hint) { const small = document.createElement('small'); small.textContent = hint; wrapper.append(small); } grid.append(wrapper);
  }
  selectSettings(selectedSettings); controls();
}
function updateSourceFields() {
  const rate=document.querySelector('[name="frontend.sample_rate_hz"]');if(rate)rate.min=values['frontend.type']==='hackrf'?2000000:1;
  for (const group of document.querySelectorAll('[data-group]')) {
    const hide = group.dataset.group === 'HackRF' && values['frontend.type'] !== 'hackrf' || group.dataset.group === 'File replay' && values['frontend.type'] !== 'file';
    group.classList.toggle('hidden', hide);
  }
}
function controls() {
  $('receiverSession').disabled = !connected() || httpBusy || pendingCommand !== null || dirty || analysisDirty || fileBusy;
  const ready = editable(), running = connected() && status?.state === 'running';
  const commandPending = httpBusy || pendingCommand !== null;
  for (const field of $('configForm').elements) field.disabled = !ready;
  for (const id of ['browseReplay', 'filePreset', 'hackrfPreset', 'importIni', 'useIni', 'iniEditor', 'apply']) $(id).disabled = !ready;
  if (selectedSession==='replay' || (capabilities && !capabilities.hackrf)) $('hackrfPreset').disabled = true;
  const sourceSelect=document.querySelector('[name="frontend.type"]'); if(sourceSelect){for(const option of sourceSelect.options)option.disabled=option.value==='hackrf' && (selectedSession==='replay'||!capabilities?.hackrf);}
  $('validate').disabled = !connected() || httpBusy;
  $('start').disabled = !ready || dirty || (values['frontend.type']==='file' && !values['replay.path']);
  $('stop').disabled = !connected() || !['running', 'starting'].includes(status?.state) || httpBusy;
  $('recordStart').disabled = !running || commandPending || status.recording_state !== 'off';
  $('recordStop').disabled = !running || commandPending || status.recording_state !== 'recording';
  $('exit').disabled = !connected() || httpBusy;
  for (const input of $('analysisForm').elements) input.disabled = !connected() || httpBusy || pendingCommand !== null || ['starting', 'stopping'].includes(status?.state);
}
async function api(path, body) {
  const options = body === undefined ? {} : {method: 'POST', headers: {'Content-Type': 'application/json', 'X-NGene-Token': token}, body: JSON.stringify(body)};
  const response = await fetch(`/api/v1/${path==='bootstrap'||path==='shutdown'?'':sessionPrefix()}${path}`, options);
  const data = await response.json(); if (!response.ok) throw new Error(data.error || `Request failed (${response.status})`); return data;
}
async function loadConfig(force = false) {
  const data = await api('config');
  if (!dirty || force) { revision = data.revision; savedValues = parseIni(data.ini); values = {...savedValues}; buildForm(); if (!analysisDirty) buildAnalysisForm(); dirtyState(false); selectSettings(selectedSettings); }
  else if (data.revision !== revision) { notice('The server configuration changed. Export your draft, then reload this page before applying.', true); }
}
async function applyConfig(validateOnly = false) {
  if(!validateProfileFields())return;
  httpBusy = true; controls();
  try {
    const result = await api(validateOnly ? 'config/validate' : 'config', {revision, ini: toIni(values)});
    if (!validateOnly) { revision = result.revision; savedValues = parseIni(result.ini); values = {...savedValues}; buildForm(); if (!analysisDirty) buildAnalysisForm(); dirtyState(false); selectSettings(selectedSettings); }
    notice(validateOnly ? 'Configuration is valid. Apply & save to use it.' : `Profile r${revision} saved on the receiver. Recording folder applied.`); log(validateOnly ? 'Configuration validated.' : `Configuration r${revision} saved.`);
  } catch (error) { notice(error.message, true); log(error.message, true); }
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
  const now = performance.now();
  if (!connected() || waiting || now-lastRequest < 34 || now-lastResponse < 34) return;
  waiting = true; lastRequest = performance.now(); socket.send(`${selectedSession==='replay'?'replay':'status'}:${diagnosticInterest()}`);
}
function renderStatus(next) {
  status = next;
  if (pendingCommand && BigInt(next.completed_command_id) >= BigInt(pendingCommand)) pendingCommand = null;
  if (lastRun !== next.run_id) { lastSample = null; lastRun = next.run_id; resetAnalysis(); }
  $('runState').textContent = next.state; $('runState').className = `state ${next.state}`;
  $('revision').textContent = next.active_revision === '0' ? 'Not started yet' : `Active profile r${next.active_revision}`;
  const source = savedValues['frontend.type'] || '—', fs = Number(savedValues['frontend.sample_rate_hz'] || 0), rf = Number(savedValues['frontend.center_frequency_hz'] || 0);
  const hardware=next.hardware_frequency_hz||rf, signal=next.signal_frequency_hz||Number(savedValues['frontend.signal_frequency_hz'])||hardware, actual=next.actual_sample_rate_hz||fs;
  $('streamDescription').textContent = `${source==='hackrf'?'HackRF':'Recorded I/Q'} · Signal ${(signal/1e6).toFixed(6)} MHz · SDR ${(hardware/1e6).toFixed(6)} MHz · Digital IF ${((signal-hardware)/1e6).toFixed(6)} MHz · Fs ${(actual/1e6).toFixed(6)} Msps (configured nominal) · CI8`;
  $('processed').textContent = pairs(next.processed_samples); $('windows').textContent = `${pairs(next.windows)} common windows`;
  let rate = null;
  if (lastSample && next.wall_seconds > lastSample.time) rate = Number(BigInt(next.received_samples) - lastSample.samples) / (next.wall_seconds - lastSample.time);
  if (!lastSample || next.wall_seconds !== lastSample.time) lastSample = {samples:BigInt(next.received_samples), time:next.wall_seconds};
  $('rate').textContent = next.state !== 'running' ? '—' : rate === null ? $('rate').textContent : `${(Math.max(0, rate) / 1e6).toFixed(3)} Msps`;
  $('elapsed').textContent = `Elapsed ${duration(next.wall_seconds)}`;
  $('written').textContent = bytes(BigInt(next.written_samples) * 2n);
  $('recordState').textContent = next.recording_state === 'recording' ? 'Recording active' : `Recording ${next.recording_state}`;
  $('dropped').textContent = pairs(next.known_dropped_samples); $('gaps').textContent = `${pairs(next.gap_events)} gap events`;
  $('inputQueue').textContent = `${pairs(next.input_queued_samples)} pairs`; $('recordQueue').textContent = `${pairs(next.recording_queued_samples)} pairs`;
  $('inputProgress').value = 100 * Number(next.input_queued_samples) / (Number(savedValues['buffer.ingress_blocks']) * Number(savedValues['buffer.window_samples']) || 1);
  $('recordProgress').value = 100 * Number(next.recording_queued_samples) / (Number(savedValues['recording.queue_blocks']) * Number(savedValues['buffer.window_samples']) || 1);
  $('bandwidth').textContent = next.actual_bandwidth_hz ? `${(next.actual_bandwidth_hz / 1e6).toFixed(2)} MHz` : 'Not applicable / not started';
  $('ringHigh').textContent = `${next.ring_high_water} blocks`; $('discarded').textContent = pairs(next.input_discarded_on_stop);
  $('health').textContent = next.state === 'running' ? (next.known_dropped_samples !== '0' ? 'Losses detected' : 'Streaming') : next.state;
  $('recordBadge').textContent = next.recording_state; $('sessionName').textContent = next.recording_name || 'No recording yet';
  $('sessionSamples').textContent = pairs(next.session_written_samples); $('segments').textContent = next.recording_segments;
  $('outputDirectory').textContent = next.output_directory;
  if (next.message !== lastMessage) { lastMessage = next.message; notice(next.message, !next.command_ok || next.state === 'error'); log(next.message, !next.command_ok || next.state === 'error'); }
  if (next.completed_command_id !== lastCommand) { lastCommand = next.completed_command_id; refreshFiles(); }

  renderAnalysis(next.analysis);

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
      if (file.name.endsWith('.cs8')) {
        const play = document.createElement('button'); play.textContent = 'Replay';
        play.disabled = !file.closed || BigInt(file.bytes) === 0n;
        play.title = 'Prepare this file in the independent replay session; live reception continues';
        play.addEventListener('click', async () => {
          try {
            if (selectedSession !== 'replay') {
              if ((dirty || analysisDirty) && !confirm('Discard this session’s unsaved draft and open independent replay?')) return;
              await selectReceiverSession('replay');
            }
            renderStatus(await api('status'));
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
let waterfallTime=-Infinity, skippedDisplay=0, rejectedFrames=0;
function diagnosticInterest(){return document.hidden||paused||!$('configurationView').classList.contains('hidden')?0:($('showSpectrum').checked?1:0)|($('showHistogram').checked?2:0);}
function buildAnalysisForm() {
  $('analysisFields').replaceChildren();
  for (const [name,title,options,min,max,step] of analysisFields) {
    const label=document.createElement('label');label.className='field';
    const span=document.createElement('span');span.textContent=title;label.append(span);
    const input=document.createElement(options?'select':'input');input.id=`analysis_${name}`;input.name=name;
    if(options)for(const value of options){const option=document.createElement('option');option.value=String(value);option.textContent=String(value);input.append(option);}
    else {input.type='number';input.min=min;input.max=max;input.step=step;}
    input.value=savedValues[`analysis.${name}`] ?? '';input.addEventListener('input',()=>{analysisDirty=true;});label.append(input);$('analysisFields').append(label);
  }
  controls();
}
function showView(configuration) {
  $('configurationView').classList.toggle('hidden',!configuration);$('observationView').classList.toggle('hidden',configuration);
  $('showConfiguration').classList.toggle('active',configuration);$('showObservation').classList.toggle('active',!configuration);
  $('showConfiguration').setAttribute('aria-pressed',String(configuration));$('showObservation').setAttribute('aria-pressed',String(!configuration));
  requestStatus();requestAnimationFrame(drawAnalysis);
}
$('showConfiguration').addEventListener('click',()=>showView(true));
$('showObservation').addEventListener('click',()=>showView(false));
$('analysisForm').addEventListener('submit',async event=>{
  event.preventDefault();const body={revision};
  for(const [name] of analysisFields){const value=$(`analysis_${name}`).value;body[name]=name.endsWith('_enabled')?value==='true':['window','temporal','units','spectrum_source','histogram_source'].includes(name)?value:Number(value);}
  httpBusy=true;controls();
  try {
    const result=await api('analysis/config',body);revision=result.revision;
    for(const [name,value] of Object.entries(result.analysis)){savedValues[`analysis.${name}`]=String(value);values[`analysis.${name}`]=String(value);}
    analysisDirty=false;dirtyState(true);resetAnalysis();
    $('analysisNotice').textContent='Applied to this session. Use Apply & save in Receiver configuration while stopped to persist the full profile.';
    log('Analysis parameters applied without restarting the receiver.');requestStatus();
  } catch(error){$('analysisNotice').textContent=error.message;log(error.message,true);}
  finally{httpBusy=false;controls();}
});
function resetAnalysis(){analysisSnapshot=null;analysisSequence='0';analysisRevision='';waterfallHistory=[];waterfallTime=-Infinity;skippedDisplay=0;rejectedFrames=0;drawAnalysis();}
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
function renderAnalysis(a){
  if(!a||paused)return;
  $('fftBackend').textContent=a.backend||'Integer FFT';
  if(a.sequence==='0'){if(analysisSequence!=='0')resetAnalysis();return;}
  if(a.revision!==analysisRevision){waterfallHistory=[];waterfallTime=-Infinity;analysisSequence='0';analysisRevision=a.revision;}
  const incoming=a.sequence_id||'0';
  if(a.dbfs.length){
    if(!psdIntegrity(a)||BigInt(incoming)<BigInt(analysisSequence)){
      rejectedFrames++;$('frameIntegrity').textContent=`Rejected invalid/stale PSD: ${rejectedFrames}`;return;
    }
  }
  if(a.dbfs.length&&incoming!==analysisSequence){
    if(analysisSequence!=='0')skippedDisplay+=Number(BigInt(incoming)-BigInt(analysisSequence)-1n);
    analysisSequence=incoming;
    if(a.produced_seconds-waterfallTime>=1/Math.max(0.1,Math.min(30,Number($('waterfallRate').value)))){
      waterfallHistory.unshift({values:Object.freeze([...a.dbfs]),first:a.first_sample,epoch:a.epoch,sequence:incoming,produced:a.produced_seconds});
      waterfallHistory.length=Math.min(waterfallHistory.length,historyRows());waterfallTime=a.produced_seconds;
    }
  }
  analysisSnapshot={...a,dbfs:[...a.dbfs]};
  const unit=a.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin';
  $('spectrumInfo').textContent=a.dbfs.length?`${a.fft_size}-point FFT · Δf ${a.bin_hz.toFixed(2)} Hz · ENBW ${a.enbw_hz.toFixed(2)} Hz · ${a.config.window} · ${a.config.averages} Welch segments · ${a.config.spectrum_source}`:'Spectrum disabled / waiting';
  $('spectrumPeak').textContent=a.dbfs.length?`Peak ${displayFrequency(a.peak_hz).toFixed(4)} ${frequencyUnit()} · ${a.peak_dbfs.toFixed(1)} ${unit}`:'';
  $('histogramInfo').textContent=a.histogram_count?`${pairs(a.histogram_count)} pairs · ${a.histogram_i.length} bins · ${a.config.histogram_source}`:'Histogram disabled / waiting';
  for(const [id,t] of [['statsI',a.stats_i],['statsQ',a.stats_q]]){
    const dl=document.createElement('dl');
    for(const [label,value] of [['Mean',t.mean],['Std',t.std],['RMS',t.rms],['Min',t.min],['Max',t.max],['Clipping / rail count',t.clipping_count],['Zero count',t.zero_count]]){
      const div=document.createElement('div'),dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=label;dd.textContent=Number.isInteger(value)?pairs(value):value.toFixed(4);div.append(dt,dd);dl.append(div);
    }$(id).replaceChildren(dl);
  }
  $('dcMagnitude').textContent=`DC magnitude √(mean I² + mean Q²): ${a.dc_magnitude.toFixed(4)} counts · histogram sample ${a.histogram_first_sample} · age ${a.histogram_age_seconds.toFixed(2)} s`;
  $('histogramRails').textContent=`Clipping indicator (−128 / +127): I ${pairs(a.clipped_i)} · Q ${pairs(a.clipped_q)}`;
  $('analysisCost').textContent=`Worker ${a.compute_ms.toFixed(2)} ms · ${a.sample_backend} · skipped bursts ${a.skipped_bursts} · contended blocks ${a.contention_blocks}`;
  $('waterfallInfo').textContent=`Newest at top · ${waterfallHistory.length} snapshots · display skips ${skippedDisplay} · sample ${a.first_sample}`;
  $('frameIntegrity').textContent=`PSD #${incoming} · produced ${a.produced_seconds.toFixed(3)} s · min/max/mean ${a.min.toFixed(2)} / ${a.max.toFixed(2)} / ${a.mean.toFixed(2)} · NaN/Inf ${a.nan_count}/${a.inf_count} · hash ${a.checksum.toString(16)} · rejected ${rejectedFrames}`;
  drawAnalysis();
}
function historyRows(){return Math.max(32,Math.min(2048,Number($('waterfallRows').value)||128));}
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
function range(colour=false){let lo=Number($(colour?'colourMin':'dbMin').value),hi=Number($(colour?'colourMax':'dbMax').value);if(!Number.isFinite(lo)||!Number.isFinite(hi)||lo>=hi)return [-100,0];return [Math.max(-250,lo),Math.min(100,hi)];}
function frequencyAxes(s){const {ctx,w,h,left,right}=s,[lo,hi]=frequencyRange();ctx.fillStyle='#8fa6ba';for(let j=0;j<=4;j++)ctx.fillText((lo+j*(hi-lo)/4).toFixed($('frequencyReference').value==='rf'?3:1),Math.min(w-58,Math.max(4,left+j*(right-left)/4-20)),h-10);ctx.fillText(frequencyUnit(),w-30,h-10);}
function drawSpectrum(){
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
function drawWaterfall(){
  const s=surface('waterfall');if(!s)return;const {ctx,w,h,left,right,top,bottom}=s,[lo,hi]=range(true);
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
  ctx.fillStyle='#8fa6ba';ctx.fillText('New',6,top+9);ctx.fillText('Older',6,bottom);frequencyAxes(s);$('colourLegend').textContent=`Global colours ${lo} … ${hi} ${analysisSnapshot?.config.units==='dbfs_hz'?'dBFS/Hz':'dBFS/bin'}`;
}
function drawHistogram(id,data,color){
  const s=surface(id);if(!s)return;const {ctx,w,h,left,right,top,bottom}=s;const percent=$('histScale').value==='percent',count=analysisSnapshot?.histogram_count||1;
  const values=(data||[]).map(v=>percent?100*v/count:v),max=Number($('histMax').value)>0?Number($('histMax').value):Math.max(1,...values);
  ctx.fillStyle='#8fa6ba';ctx.fillText(percent?'%':'count',4,11);
  for(let j=0;j<=2;j++){const y=top+j*(bottom-top)/2;ctx.strokeStyle='#233446';ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((max*(1-j/2)).toFixed(percent?1:0),4,y+4);}
  const dw=(right-left)/(values.length||1);ctx.fillStyle=color;
  values.forEach((v,k)=>{const bh=(bottom-top)*Math.min(1,v/max);ctx.fillRect(left+k*dw,bottom-bh,Math.max(1,dw-1),bh);});
  ctx.fillStyle='#8fa6ba';for(let j=0;j<=4;j++)ctx.fillText(String(j===4?127:-128+j*64),Math.min(w-28,left+j*(right-left)/4-10),h-10);
}
function drawAnalysis(){drawSpectrum();drawWaterfall();drawHistogram('histogramI',analysisSnapshot?.histogram_i,'#39d5b0');drawHistogram('histogramQ',analysisSnapshot?.histogram_q,'#62a6ff');}
$('clearWaterfall').addEventListener('click',()=>{waterfallHistory=[];drawWaterfall();});
for(const id of ['dbMin','dbMax','colourMin','colourMax','waterfallRows','waterfallRate','frequencyMin','frequencyMax','histScale','histMax'])$(id).addEventListener('change',drawAnalysis);
$('configForm').addEventListener('submit', event => event.preventDefault());
$('apply').addEventListener('click', () => applyConfig()); $('validate').addEventListener('click', () => applyConfig(true));
$('start').addEventListener('click', () => command('receiver/start')); $('stop').addEventListener('click', () => command('receiver/stop'));
$('recordStart').addEventListener('click', () => command('recording/start')); $('recordStop').addEventListener('click', () => command('recording/stop'));
$('reloadFiles').addEventListener('click', refreshFiles); $('clearLog').addEventListener('click', () => $('events').replaceChildren());
$('pausePlot').addEventListener('click', () => { paused = !paused; $('pausePlot').textContent = paused ? 'Resume plots' : 'Pause plots'; if (!paused && status)renderAnalysis(status.analysis);requestStatus(); });
$('useIni').addEventListener('click', () => { try { values = parseIni($('iniEditor').value); buildForm(); dirtyState(true); } catch (error) { notice(error.message, true); } });
$('importIni').addEventListener('change', async event => { try { const file = event.target.files[0]; if (!file) return; if (file.size > 12000) throw new Error('INI file exceeds 12 kB.'); values = parseIni(await file.text()); buildForm(); dirtyState(true); notice('INI imported into draft. Validate and apply before starting.'); } catch (error) { notice(error.message, true); } finally { event.target.value = ''; } });
$('exportIni').addEventListener('click', () => { const url = URL.createObjectURL(new Blob([toIni(values)], {type:'text/plain'})), link = document.createElement('a'); link.href = url; link.download = 'receiver.ini'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000); });
$('filePreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'file','replay.realtime':'true','replay.loop':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('frequencyReference').addEventListener('change',()=>{$('frequencyMin').value='';$('frequencyMax').value='';drawAnalysis();});
$('resetFrequency').addEventListener('click',()=>{$('frequencyMin').value='';$('frequencyMax').value='';drawAnalysis();});
$('autoScale').addEventListener('click',()=>{if(!analysisSnapshot?.dbfs.length)return;const lo=Math.floor(Math.min(...analysisSnapshot.dbfs)/10)*10,hi=Math.ceil(Math.max(...analysisSnapshot.dbfs)/10)*10+10;for(const id of ['dbMin','colourMin'])$(id).value=lo;for(const id of ['dbMax','colourMax'])$(id).value=hi;drawAnalysis();});
for(const [id,panel] of [['showSpectrum','spectrumPanel'],['showHistogram','histogramPanel']])$(id).addEventListener('change',()=>{$(panel).classList.toggle('hidden',!$(id).checked);requestStatus();});
document.addEventListener('visibilitychange',requestStatus);
$('hackrfPreset').addEventListener('click', () => { Object.assign(values, {'frontend.type':'hackrf','frontend.sample_rate_hz':'16000000','frontend.center_frequency_hz':'1575420000','frontend.signal_frequency_hz':'1575420000','buffer.window_samples':'16000','hackrf.bandwidth_hz':'0','hackrf.lna_gain_db':'16','hackrf.vga_gain_db':'20','hackrf.rf_amp':'false','hackrf.antenna_power':'false','receiver.run_seconds':'0','receiver.mode':'capture','recording.enabled':'false'}); buildForm(); dirtyState(true); });
$('exit').addEventListener('click', async () => { if (!confirm('Stop the receiver, finish recording and close the local NGene service?')) return; try { await api('shutdown', {}); quitting = true; notice('NGene is closing. Restart ngene --web to reconnect.'); } catch (error) { notice(error.message, true); } });
setInterval(() => { if (waiting && performance.now() - lastRequest > 4000) socket?.close(); else if (performance.now() - lastRequest >= Number($('refresh').value)) requestStatus(); }, 10);

new ResizeObserver(drawAnalysis).observe($('spectrum'));
connect();

/* Read-only inspection for frame troubleshooting and browser regression tests. */
export function diagnosticSnapshot(){return {sequence:analysisSequence,skipped:skippedDisplay,rejected:rejectedFrames,history:waterfallHistory.map(row=>({...row,values:[...row.values]}))};}
export {psdIntegrity, renderAnalysis, displayFrequency};

document.querySelectorAll('[data-settings]').forEach(button => button.addEventListener('click', () => selectSettings(button.dataset.settings)));

async function selectReceiverSession(session) {
  selectedSession = session; $('receiverSession').value = session;
  pendingTelemetry = null;
  // Disconnect the old poll so an in-flight primary snapshot cannot enter replay history.
  status = null; dirty = false; analysisDirty = false; pendingCommand = null;
  lastRun = null; lastCommand = null; lastMessage = null; lastSample = null;
  waiting = false; lastRequest = 0; lastResponse = -Infinity; resetAnalysis();
  if(socket){socket.onclose=null;socket.onmessage=null;socket.onerror=null;socket.close();socket=null;}
  $('events').replaceChildren();
  await connect();
  if (socket && !connected()) await new Promise((resolve, reject) => {
    const target = socket;
    const timer = setTimeout(() => { cleanup(); reject(new Error('Timed out connecting to the selected session.')); }, 5000);
    const opened = () => { cleanup(); resolve(); };
    const closed = () => { cleanup(); reject(new Error('Selected session disconnected.')); };
    const cleanup = () => { clearTimeout(timer); target.removeEventListener('open', opened); target.removeEventListener('close', closed); };
    target.addEventListener('open', opened, {once:true}); target.addEventListener('close', closed, {once:true});
  });
  notice(selectedSession==='replay'?'Independent playback session. The primary stream continues running.':'Primary receiver session. File replay continues independently.');
}
$('receiverSession').addEventListener('change', () => selectReceiverSession($('receiverSession').value));

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
  $('pickerNotice').textContent = result.truncated ? 'Showing at most 500 entries; refine the filename filter or open a subfolder.' : 'Choose a closed .cs8 file. Its adjacent metadata is loaded automatically.';
  renderPickerEntries();
}
/** Open selection for the currently stopped session; a table Replay action chooses replay first. */
async function openReplayPicker(directory, selectedPath) {
  if (!editable()) throw new Error('Stop the selected session before choosing another input file.');
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
  const keys = ['frontend.type','replay.path','replay.start_sample','replay.sample_limit','receiver.run_seconds','receiver.mode','recording.enabled','conditioning.module'];
  if (replaySelection.metadata_available) keys.push('frontend.sample_rate_hz','frontend.center_frequency_hz','frontend.signal_frequency_hz','hackrf.bandwidth_hz','conditioning.dc_shift','buffer.window_samples');
  for (const key of keys) values[key] = draft[key];
  buildForm(); dirtyState(true); selectSettings('source'); $('showConfiguration').click();
  notice(replaySelection.metadata_available ? 'Replay metadata loaded into the draft. Review, Apply & save, then Start receiver.' : 'File selected. Set sampling rate and RF parameters, then Apply & save.');
  $('replayFileDialog').close();
});
