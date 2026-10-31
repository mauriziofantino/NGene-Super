/* Receiver workspace: navigation and presentation of controller snapshots only. */
const ROUTES = {
  journey: {label: 'Overview', id: 'journeyView'},
  setup: {label: 'Receiver configuration', id: 'configurationView'},
  rf: {label: 'RF observation', id: 'observationView'},
  acquisition: {label: 'Acquisition', id: 'acquisitionView'},
  tracking: {label: 'Tracking', id: 'trackingView'},
  navigation: {label: 'Navigation data', id: 'navigationView'},
  pvt: {label: 'PVT solution', id: 'pvtView'},
  record: {label: 'Recording & replay', id: 'recordingView'},
  health: {label: 'System health', id: 'healthView'}
};
const STAGES = [
  ['setup', 'Receiver configuration', 'Configure the front end, receiver channels, acquisition, tracking and navigation.'],
  ['rf', 'RF observation', 'Check I/Q continuity, spectrum, waterfall and amplitude.'],
  ['acquisition', 'Acquisition', 'Search satellite codes and confirm signal candidates.'],
  ['tracking', 'Tracking', 'Follow carrier and code lock, correlators and loop state.'],
  ['navigation', 'Navigation', 'Decode messages, satellite time and valid ephemerides.'],
  ['pvt', 'PVT solution', 'Inspect position, velocity, time and measurement quality.'],
  ['record', 'Record & replay', 'Save native I/Q and return to a recorded input.']
];
const SETTINGS = new Set(['source', 'receiver', 'acquisition', 'tracking', 'navigation', 'recording', 'analysis', 'display', 'profile']);
const SOURCE_NAMES = {file: 'Recorded I/Q', hackrf: 'HackRF', rtlsdr: 'RTL-SDR', bladerf: 'bladeRF'};
const UNAVAILABLE = 'Unavailable';
const badge = (text = 'Module unavailable') => `<span class="badge workspace-label">${text}</span>`;
const field = (name, placeholder = UNAVAILABLE) => `<span data-workspace-field="${name}">${placeholder}</span>`;
const button = (route, label, primary = false) => `<button type="button" class="${primary ? 'primary' : 'quiet'}" data-go="${route}">${label}</button>`;
const settingsButton = (topic, label) => `<button type="button" data-open-settings="${topic}">${label}</button>`;
const heading = (step, title, description, actions = '') => `<header class="workspace-hero"><div><span class="eyebrow">${step}</span><h1>${title}</h1><p>${description}</p></div>${actions ? `<div class="workspace-actions">${actions}</div>` : ''}</header>`;
const card = (title, description, body, label = '') => `<section class="card workspace-card"><div class="panel-heading"><div><h2>${title}</h2>${description ? `<p class="muted">${description}</p>` : ''}</div>${label}</div>${body}</section>`;
const empty = (title, description, icon = '—') => `<div class="workspace-empty"><span class="workspace-empty-icon" aria-hidden="true">${icon}</span><h3>${title}</h3><p>${description}</p></div>`;
const rows = entries => `<dl class="workspace-fields">${entries.map(([title, value]) => `<div><dt>${title}</dt><dd>${value}</dd></div>`).join('')}</dl>`;
const kpi = (label, value, detail) => `<article class="card workspace-kpi"><span>${label}</span><strong>${value}</strong><small>${detail}</small></article>`;
const table = (columns, message) => `<div class="table-wrap workspace-table"><table><thead><tr>${columns.map(column => `<th scope="col">${column}</th>`).join('')}</tr></thead><tbody><tr><td colspan="${columns.length}" class="empty-table">${message}</td></tr></tbody></table></div>`;
const route = (previous, next) => `<nav class="workspace-route" aria-label="Receiver workflow">${button(previous, `← ${ROUTES[previous].label}`)}<span class="muted">Follow the receiver pipeline</span>${button(next, `${ROUTES[next].label} →`)}</nav>`;
const unavailableNote = text => `<div class="workspace-callout"><strong>Not available in this receiver build.</strong> ${text}</div>`;
function journeyMarkup() {
  return heading('RECEIVER WORKSPACE', 'One path from I/Q to a GNSS solution',
    'Configure the source, inspect the signal and follow each receiver stage. Live information comes from the connected receiver; stages without an integrated module remain empty.',
    `${button('setup', 'Configure receiver', true)}<button type="button" data-tour="start">Start guided tour</button>${button('rf', 'Open RF observation')}`) +
    `<nav class="workspace-pipeline" aria-label="Receiver stages">${STAGES.map(([view, title, description], index) => `<button type="button" class="workspace-step" data-go="${view}"><span class="workspace-step-number">${String(index + 1).padStart(2, '0')}</span><h3>${title}</h3><p>${description}</p><span class="workspace-step-state" data-workspace-field="stage-${view}">${['setup', 'rf', 'record'].includes(view) ? 'Waiting for receiver' : 'Module unavailable'}</span></button>`).join('')}</nav>` +
    `<div class="workspace-kpis">${kpi('INPUT SOURCE', field('sourceName'), field('formatDetail'))}${kpi('PROCESSED I/Q PAIRS', field('processed'), field('windowsDetail'))}${kpi('RECEIVER SERVICE', field('receiverState'), field('connectionDetail'))}${kpi('NATIVE I/Q CAPTURE', field('recordingState'), field('capturePoint'))}</div>` +
    `<div class="workspace-grid workspace-grid-wide">${card('Your next step', 'Operate the real signal path before inspecting GNSS stages.',
      `<div class="workspace-status-strip">${badge('I/Q pipeline available')}${badge('GNSS modules unavailable')}</div><ol class="workspace-next-steps"><li><strong>Prepare the source.</strong> Select a live SDR or a recorded I/Q file, then apply the profile.</li><li><strong>Start reception.</strong> Use Start receiver, then check continuity and native signal statistics in RF observation.</li><li><strong>Inspect the chain.</strong> Acquisition, tracking, navigation decoding and PVT await their receiver modules.</li></ol><div class="workspace-actions">${button('setup', 'Review setup')}${button('health', 'Check system health')}</div>`)}${card('Session at a glance', 'Configured input and the current receiver connection.', rows([
      ['Connection', field('connection')], ['Profile', field('profileState')], ['Native sample format', field('format')], ['Sample rate', field('sampleRate')], ['Signal centre', field('signalFrequency')], ['Position solution', 'Unavailable — PVT module not integrated']
    ]))}</div>`;
}

function acquisitionMarkup() {
  return heading('GNSS', 'Acquisition', 'The previous GPS acquisition implementation has been removed.', badge()) +
    card('Acquisition unavailable', 'A replacement has not been integrated.', empty('No acquisition module', 'RF observation, recording and replay remain available.')) + route('rf', 'tracking');
}

function trackingMarkup() {
  return heading('STAGE 04 / TRACKING', 'Inspect channels, correlators and loops',
    'Follow each signal from pull-in to carrier and code lock. Channel lock, navigation readiness and use in PVT are separate states.',
    `${badge()}${settingsButton('tracking', 'Tracking settings')}${button('acquisition', 'Back to acquisition')}${button('navigation', 'Navigation data →')}`) +
    `<div class="workspace-kpis">${kpi('TRACKING CHANNELS', '—', 'No channel telemetry')}${kpi('CARRIER / CODE LOCK', '—', 'No lock estimates')}${kpi('VALID OBSERVABLES', '—', 'No measurement epochs')}${kpi('USED IN PVT', '—', 'No solution available')}</div>` +
    `<div class="workspace-grid workspace-grid-wide">${card('Receiver channels', 'Select a channel to inspect its signal and measurement state.',
      table(['Satellite / signal', 'Stage', 'C/N₀ (dB-Hz)', 'Doppler (Hz)', 'Navigation', 'In PVT'], 'No tracking channels available. No tracking module is integrated.'), badge())}${card('Selected channel', 'Channel details will follow the selected signal.',
      empty('No channel selected', 'Select an active tracking channel when channel telemetry is available.'), badge())}</div>` +
    `<div class="workspace-grid">${card('Early / prompt / late correlators', 'In-phase and quadrature values for one coherent integration epoch.',
      empty('No correlator samples', 'Correlator plots require valid channel samples; empty space does not indicate zero signal.'))}${card('Loop and synchronization state', 'Measured estimates and flags from the selected channel.', rows([
        ['Carrier loop / code loop', UNAVAILABLE], ['Carrier lock / code lock', UNAVAILABLE], ['Bit / frame synchronization', UNAVAILABLE], ['C/N₀ estimate and validity', UNAVAILABLE], ['Integration epoch', UNAVAILABLE], ['Measurement eligibility', UNAVAILABLE]
      ]))}</div>` + route('acquisition', 'navigation');
}

function navigationMarkup() {
  return heading('STAGE 05 / NAVIGATION', 'Decode time and navigation data',
    'Keep message synchronization, satellite health, ephemerides and observables visible before a position is computed.',
    `${badge()}${settingsButton('navigation', 'Navigation & PVT settings')}${button('tracking', 'Inspect channels')}${button('pvt', 'PVT solution →')}`) +
    `<div class="workspace-kpis">${kpi('MESSAGE DECODING', 'Unavailable', 'Navigation decoder not integrated')}${kpi('VALID EPHEMERIDES', '—', 'No decoded satellite data')}${kpi('RECEIVER TIME', '—', 'No valid GNSS timebase')}${kpi('OBSERVATION EPOCH', '—', 'No synchronized measurements')}</div>` +
    card('Satellite navigation data', 'Validity and age must accompany each decoded data set.',
      table(['Satellite / signal', 'Message sync', 'Time of week', 'Ephemerides', 'Health', 'Age', 'Authentication'], 'No decoded navigation messages available.')) +
    `<div class="workspace-grid">${card('Time and observables', 'Only valid, synchronized measurements can feed a solution.', rows([
      ['GPS time / Galileo time', UNAVAILABLE], ['UTC and conversion status', UNAVAILABLE], ['Pseudorange', UNAVAILABLE], ['Carrier phase / Doppler', UNAVAILABLE], ['Measurement epoch / validity', UNAVAILABLE], ['Clock and ephemeris corrections', UNAVAILABLE]
    ]))}${card('Authentication and assistance', 'Authentication is independent of signal lock and solution availability.', rows([
      ['OSNMA verification', 'Unavailable — authentication module not integrated'], ['Trusted key / verification time', UNAVAILABLE], ['Assistance / almanac', 'No assistance service available'], ['GPS signal authentication', UNAVAILABLE]
    ]) + '<p class="muted">Unavailable data is neither a successful verification nor an authentication failure.</p>', badge())}</div>` + route('tracking', 'pvt');
}

function emptySkyplot() {
  return `<div class="workspace-skyplot"><svg viewBox="0 0 300 276" role="img" aria-label="Empty skyplot: no satellite positions available"><g fill="none" stroke="currentColor" opacity=".25"><circle cx="150" cy="136" r="106"/><circle cx="150" cy="136" r="71"/><circle cx="150" cy="136" r="35"/><path d="M44 136H256M150 30V242"/></g><g fill="currentColor" text-anchor="middle" font-size="12"><text x="150" y="17">N</text><text x="275" y="140">E</text><text x="150" y="264">S</text><text x="25" y="140">W</text></g><text x="150" y="132" fill="currentColor" text-anchor="middle" font-size="12">No satellite positions</text><text x="150" y="150" fill="currentColor" text-anchor="middle" font-size="11">available</text></svg></div>`;
}

function pvtMarkup() {
  return heading('STAGE 06 / POSITION, VELOCITY & TIME', 'Understand the receiver solution',
    'Review a valid solution together with its measurement epoch, satellite geometry and quality. A running I/Q stream alone does not establish a position fix.',
    `${badge()}${settingsButton('navigation', 'Navigation & PVT settings')}${button('navigation', 'Review navigation data')}${button('record', 'Recording & replay →')}`) +
    `<div class="workspace-kpis">${kpi('SOLUTION STATE', 'Unavailable', 'PVT module not integrated')}${kpi('LATITUDE / LONGITUDE', '—', 'No valid coordinates')}${kpi('ELLIPSOIDAL HEIGHT', '—', 'No height estimate')}${kpi('MEASUREMENT EPOCH', '—', 'No GNSS solution time')}</div>` +
    `<div class="workspace-grid">${card('Satellite geometry', 'Sky positions of satellites used or observed by the receiver.', emptySkyplot(), badge())}${card('Solution and quality', 'Quality values are displayed only for an available solution.', rows([
      ['Position / velocity solution', UNAVAILABLE], ['GNSS time / solution age', UNAVAILABLE], ['Satellites used / excluded', UNAVAILABLE], ['GDOP / PDOP / HDOP / VDOP', UNAVAILABLE], ['Receiver clock bias / drift', UNAVAILABLE], ['Residual RMS', UNAVAILABLE], ['Integrity assessment', 'Not computed']
    ]))}</div>` +
    `<div class="workspace-grid">${card('Position history', 'A trajectory requires actual, time-stamped coordinates.',
      empty('No position history', 'A trajectory will appear when valid, time-stamped position fixes are available.'))}${card('Observation residuals', 'Inspect measurement fit, then trace a satellite back to its channel.',
      empty('No measurement residuals', 'Residuals become available after a solution is computed from valid observables.'))}</div>` +
    card('Trace a solution back to its input', 'Move between the views that explain a solution or an excluded measurement.',
      `<div class="workspace-actions">${button('navigation', 'Navigation & observables')}${button('tracking', 'Channels & correlators')}${button('rf', 'RF observation')}${button('record', 'Native I/Q recordings')}</div>`) + route('navigation', 'record');
}

function recordingMarkup() {
  return heading('STAGE 07 / RECORDING & REPLAY', 'Keep the signal, revisit the session',
    'Record native I/Q on the receiver computer, transfer closed captures and select an existing file as the input source.',
    `${settingsButton('recording', 'Capture settings')}<button type="button" data-browse-replay data-workspace-replay>Choose recorded I/Q</button>${button('setup', 'Replay setup')}`) +
    `<div class="workspace-kpis">${kpi('CAPTURE STATE', field('recordingState'), field('recordingName'))}${kpi('CAPTURE POINT', field('capturePoint'), 'Original or conditioned I/Q')}${kpi('NATIVE FORMAT', field('format'), field('sampleRate'))}${kpi('REPLAY INPUT', field('replayState'), field('replayDetail'))}</div>` +
    `<div class="workspace-callout">Start and stop recording with the receiver controls above. Stopping capture drains the writer queue; Stop receiver also ends the input stream. Selecting a replay file prepares a profile draft and does not start reception.</div>`;
}

function healthMarkup() {
  return heading('RECEIVER / SYSTEM HEALTH', 'Inspect the system behind the signal',
    'Read the connected receiver’s platform, stream capabilities and available modules. Queue and continuity measurements below come from the running service.',
    `${button('rf', 'RF observation')}${settingsButton('receiver', 'Receiver & buffers')}`) +
    `<div class="workspace-kpis">${kpi('CONNECTION', field('connection'), field('receiverState'))}${kpi('PLATFORM', field('architecture'), field('operatingSystem'))}${kpi('RECEIVER VERSION', field('version'), field('profileState'))}${kpi('INTEGER FFT', field('fftBackend'), 'Selected by this receiver build')}</div>` +
    `<div class="workspace-grid">${card('Available processing modules', 'The interface keeps source processing and GNSS solution states separate.',
      `<div class="workspace-module-list">${[
        ['Native I/Q input and conditioning', 'module-input'], ['Spectrum / waterfall / I/Q statistics', 'module-analysis'], ['Native I/Q recording and replay', 'module-recording'], ['Satellite acquisition', 'module-acquisition'], ['Channel tracking and navigation decoding', 'module-gnss'], ['PVT / integrity / authentication', 'module-gnss']
      ].map(([title, key]) => `<div class="workspace-module-row"><strong>${title}</strong><span class="badge" data-workspace-field="${key}">${key === 'module-gnss' ? 'Not integrated' : 'Waiting for receiver'}</span></div>`).join('')}</div>`)}${card('Build and measurement capabilities', 'A supported frontend still requires a connected device and valid setup.', rows([
        ['File replay', field('fileSupport')], ['HackRF / RTL-SDR / bladeRF', field('frontendSupport')], ['Native sample format', field('format')], ['Device continuity', field('deviceContinuity')], ['Hardware-only loss counter', field('hardwareLoss')], ['CPU load / per-core timing', 'Unavailable — not reported by the receiver']
      ]))}</div>`;
}

function counter(value) {
  if (value === undefined || value === null || value === '') return '—';
  try { return BigInt(value).toLocaleString('en-US'); } catch { return '—'; }
}
function positive(value) {
  const result = Number(value);
  return Number.isFinite(result) && result > 0 ? result : null;
}
function frequency(value, suffix, scale = 1e6) {
  const result = positive(value);
  return result === null ? UNAVAILABLE : `${(result / scale).toFixed(scale === 1e9 ? 9 : 6)} ${suffix}`;
}

/**
 * Add receiver workspace views without owning controller state or running signal work.
 * The application supplies snapshots; unavailable GNSS modules have no synthetic data.
 */
export function createReceiverWorkspace({onViewChange = () => {}, onOpenSettings = () => {}, onBrowseReplay = () => {}, onPlotInterestChange = () => {}} = {}) {
  const renderers = {journey: journeyMarkup, acquisition: acquisitionMarkup, tracking: trackingMarkup, navigation: navigationMarkup, pvt: pvtMarkup};
  for (const [name, render] of Object.entries(renderers)) {
    const panel = document.getElementById(ROUTES[name].id);
    if (panel) panel.innerHTML = render();
  }
  for (const [name, render] of [['record', recordingMarkup], ['health', healthMarkup]]) {
    const panel = document.getElementById(ROUTES[name].id);
    if (!panel) continue;
    const summary = document.createElement('div'); summary.className = 'workspace-summary'; summary.innerHTML = render(); panel.prepend(summary);
  }
  const tour = document.createElement('section');
  tour.id = 'workspaceTour'; tour.className = 'workspace-tour hidden'; tour.hidden = true; tour.setAttribute('aria-label', 'Guided receiver workflow');
  tour.innerHTML = '<div><span class="eyebrow" data-tour-counter></span><h2 data-tour-title></h2><p data-tour-description></p><small class="muted">This guide changes views. Start receiver and recording controls remain under your control.</small></div><div class="workspace-actions"><button type="button" data-tour="previous">Previous stage</button><button type="button" class="primary" data-tour="next">Next stage</button><button type="button" class="quiet" data-tour="end">End tour</button></div>';
  const journey = document.getElementById(ROUTES.journey.id);
  if (journey) journey.before(tour);
  const bindings = new Map();
  document.querySelectorAll('[data-workspace-field]').forEach(node => {
    const key = node.dataset.workspaceField;
    if (!bindings.has(key)) bindings.set(key, []);
    bindings.get(key).push(node);
  });
  const replayButtons = [...document.querySelectorAll('[data-workspace-replay]')];
  function plotSwitch(id, parent, label) {
    const wrapper = document.createElement('label');
    wrapper.className = 'plot-toggle';
    const input = document.createElement('input');
    input.type = 'checkbox'; input.id = id; input.checked = true;
    input.setAttribute('aria-label', `Enable ${label}`);
    wrapper.append(input, document.createTextNode('Enable'));
    parent?.append(wrapper);
    return input;
  }
  for (const [id, title, selector] of [
    ['showCorrelators', 'Early / prompt / late correlators', '.workspace-empty'],
    ['showSkyplot', 'Satellite geometry', '.workspace-skyplot']
  ]) {
    const cardNode = [...document.querySelectorAll('.workspace-card')]
      .find(node => node.querySelector('.panel-heading h2')?.textContent === title);
    const plot = cardNode?.querySelector(selector);
    if (!plot) continue;
    const toggle = plotSwitch(id, cardNode.querySelector('.panel-heading'), title);
    const off = document.createElement('p');
    off.className = 'plot-disabled muted hidden'; off.textContent = 'Plot disabled.';
    cardNode.append(off);
    toggle.addEventListener('change', () => {
      plot.classList.toggle('hidden', !toggle.checked);
      off.classList.toggle('hidden', toggle.checked);
    });
  }
  const panels = Object.entries(ROUTES).map(([name, info]) => [name, document.getElementById(info.id)]);
  const nav = [...document.querySelectorAll('[data-view]')];
  const crumb = document.getElementById('workspaceCrumb');
  const tourCounter = tour.querySelector('[data-tour-counter]'), tourTitle = tour.querySelector('[data-tour-title]'), tourDescription = tour.querySelector('[data-tour-description]');
  const previous = tour.querySelector('[data-tour="previous"]'), next = tour.querySelector('[data-tour="next"]');
  let selected = 'rf', guided = false, tourIndex = 0;
  const text = (key, value) => {
    const string = String(value ?? UNAVAILABLE);
    for (const node of bindings.get(key) || []) if (node.textContent !== string) node.textContent = string;
  };
  function updateTour() {
    tour.hidden = !guided; tour.classList.toggle('hidden', !guided);
    if (!guided) return;
    const index = STAGES.findIndex(([view]) => view === selected);
    if (index >= 0) tourIndex = index;
    tourCounter.textContent = `GUIDED TOUR · STAGE ${tourIndex + 1} OF ${STAGES.length}`;
    tourTitle.textContent = STAGES[tourIndex][1]; tourDescription.textContent = STAGES[tourIndex][2];
    previous.disabled = tourIndex === 0; next.textContent = tourIndex === STAGES.length - 1 ? 'Finish tour' : 'Next stage';
  }
  function show(view, {focus = false} = {}, writeHistory = true) {
    if (!Object.hasOwn(ROUTES, view)) return false;
    const changed = selected !== view; selected = view;
    for (const [name, panel] of panels) if (panel) {
      const active = name === view;
      panel.classList.toggle('hidden', !active); panel.hidden = !active;
    }
    for (const item of nav) {
      const active = item.dataset.view === view;
      item.classList.toggle('active', active); item.setAttribute('aria-pressed', String(active));
      if (active) item.setAttribute('aria-current', 'page'); else item.removeAttribute('aria-current');
    }
    if (crumb && crumb.textContent !== ROUTES[view].label) crumb.textContent = ROUTES[view].label;
    if (writeHistory && location.hash !== `#${view}` && (changed || location.hash)) history.pushState(null, '', `#${view}`);
    updateTour();
    onViewChange(view);
    if (focus) {
      const panel = document.getElementById(ROUTES[view].id), title = panel?.querySelector('h1,h2');
      if (title) { title.tabIndex = -1; title.focus({preventScroll: true}); }
    }
    return true;
  }
  function fromHash() {
    const view = location.hash.slice(1);
    show(Object.hasOwn(ROUTES, view) ? view : 'rf', {}, false);
  }
  document.addEventListener('click', event => {
    if (!(event.target instanceof Element)) return;
    const control = event.target.closest('[data-view],[data-go],[data-open-settings],[data-browse-replay],[data-tour]');
    if (!control || control.disabled || control.getAttribute('aria-disabled') === 'true') return;
    if (control.hasAttribute('data-tour')) {
      event.preventDefault(); const action = control.dataset.tour;
      if (action === 'start') { guided = true; tourIndex = 0; show(STAGES[0][0], {focus: true}); }
      else if (action === 'end' || action === 'next' && tourIndex === STAGES.length - 1) { guided = false; updateTour(); }
      else if (action === 'next' || action === 'previous') show(STAGES[Math.max(0, Math.min(STAGES.length - 1, tourIndex + (action === 'next' ? 1 : -1)))][0], {focus: true});
      return;
    }
    if (control.hasAttribute('data-open-settings')) {
      event.preventDefault(); const topic = control.dataset.openSettings;
      if (SETTINGS.has(topic)) { show('setup', {focus: true}); onOpenSettings(topic); }
      return;
    }
    if (control.hasAttribute('data-browse-replay')) { event.preventDefault(); onBrowseReplay(); return; }
    const view = control.dataset.view || control.dataset.go;
    if (Object.hasOwn(ROUTES, view)) { event.preventDefault(); show(view, {focus: true}); }
  });
  window.addEventListener('hashchange', fromHash);
  window.addEventListener('popstate', fromHash);

  function update(snapshot = {}) {
    const {status, capabilities, profile = {}, draft = {}, revision, dirty = false, connected = false} = snapshot;
    const live = connected && status ? status : null;
    const configuredSource = profile['frontend.type'];
    const source = SOURCE_NAMES[configuredSource] || configuredSource || UNAVAILABLE;
    const configuredFormat = configuredSource === 'file' ? profile['replay.format'] : configuredSource === 'bladerf' ? profile['bladerf.format'] : {hackrf: 'CI8', rtlsdr: 'CU8'}[configuredSource];
    const started = live && live.active_revision && live.active_revision !== '0';
    const format = started ? live.sample_format || UNAVAILABLE : configuredFormat || UNAVAILABLE;
    const rate = started ? live.actual_sample_rate_hz || profile['frontend.sample_rate_hz'] : profile['frontend.sample_rate_hz'];
    const hardware = started ? live.hardware_frequency_hz || profile['frontend.center_frequency_hz'] : profile['frontend.center_frequency_hz'];
    const signal = started ? live.signal_frequency_hz || positive(profile['frontend.signal_frequency_hz']) || hardware : positive(profile['frontend.signal_frequency_hz']) || hardware;
    const state = live?.state || (connected ? 'Waiting for status' : 'Disconnected');
    const record = live?.recording_state || UNAVAILABLE;
    const profileState = revision ? `Profile r${revision}${dirty ? ' · unsaved changes' : ''}` : 'Waiting for profile';
    const values = {
      sourceName: source, format, formatDetail: format === UNAVAILABLE ? 'No format available' : `${format} · ${started ? 'receiver run' : 'configured input'}`,
      sampleRate: frequency(rate, 'Msps (nominal)'), signalFrequency: frequency(signal, 'GHz', 1e9),
      processed: live ? counter(live.processed_samples) : '—', windowsDetail: live ? `${counter(live.windows)} common windows` : 'Waiting for receiver telemetry',
      receiverState: state, connection: connected ? 'Connected' : 'Disconnected', connectionDetail: connected ? 'Connected receiver service' : 'Current receiver state unknown',
      recordingState: record, recordingName: live?.recording_name || 'No recording session', profileState, revision: revision || UNAVAILABLE,
      capturePoint: profile['recording.point'] === 'pre' ? 'Pre-conditioning' : profile['recording.point'] === 'post' ? 'Post-conditioning' : UNAVAILABLE,
      replayState: configuredSource === 'file' ? (profile['replay.path'] ? 'File selected' : 'No file selected') : 'Live SDR selected',
      replayDetail: configuredSource === 'file' ? profile['replay.path'] || 'Choose recorded I/Q to prepare input' : 'Stop reception before switching input',
      architecture: capabilities?.architecture || UNAVAILABLE, operatingSystem: capabilities?.os || UNAVAILABLE,
      version: capabilities?.version ? `v${capabilities.version}` : UNAVAILABLE, fftBackend: capabilities?.fft_backend || UNAVAILABLE,
      fileSupport: capabilities ? 'Available' : UNAVAILABLE,
      frontendSupport: capabilities ? ['hackrf', 'rtlsdr', 'bladerf'].map(name => `${SOURCE_NAMES[name]}: ${capabilities[name] === true ? 'supported' : 'not in this build'}`).join(' · ') : UNAVAILABLE,
      deviceContinuity: live?.device_continuity_state || UNAVAILABLE,
      hardwareLoss: live?.hardware_loss_state === 'valid' && live.hardware_lost_samples != null ? counter(live.hardware_lost_samples) : UNAVAILABLE,
      'module-input': capabilities ? 'Available' : 'Waiting for receiver',
      'module-analysis': capabilities ? (capabilities.periodogram ? 'Available' : 'Unavailable') : 'Waiting for receiver',
      'module-recording': capabilities ? 'Available' : 'Waiting for receiver',
      'module-acquisition': capabilities?.gps_acquisition ? 'Available' : UNAVAILABLE,
      'module-gnss': 'Not integrated',
      'stage-setup': dirty ? 'Unsaved changes' : revision ? `Profile r${revision}` : 'Waiting for profile',
      'stage-rf': state,
      'stage-acquisition': UNAVAILABLE,
      'stage-record': record
    };
    for (const [key, value] of Object.entries(values)) text(key, value);
    for (const node of replayButtons) {
      const disabled = typeof snapshot.editable === 'boolean' ? !snapshot.editable : !connected || !['stopped', 'error'].includes(live?.state);
      if (node.disabled !== disabled) node.disabled = disabled;
      const hint = !connected ? 'Connect to the receiver first.' : !['stopped', 'error'].includes(live?.state) ? 'Stop reception before choosing a replay input.' : disabled ? 'Wait for the current receiver command to finish.' : 'Choose a capture on the receiver computer.';
      if (node.title !== hint) node.title = hint;
    }
  }
  update();
  fromHash();
  return {setView: show, update, get view() { return selected; }};
}
