/* Receiver workspace: navigation and presentation of controller snapshots only. */
import {createAcquisitionSearchSpace} from './acquisition-search-space.js';
import {createAcquisitionTable} from './acquisition-table.js';
let satelliteSearchSpace = null;
const ROUTES = {
  journey: {label: 'Overview', id: 'journeyView'},
  setup: {label: 'Receiver configuration', id: 'configurationView'},
  rf: {label: 'RF observation', id: 'observationView'},
  acquisition: {label: 'Acquisition', id: 'acquisitionView'},
  tracking: {label: 'Channels', id: 'trackingView'},
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
  ['record', 'Record & replay', 'Save common signed I/Q and return to a recorded input.']
];
const SETTINGS = new Set(['source', 'receiver', 'acquisition', 'tracking', 'navigation', 'recording', 'analysis', 'display', 'profile']);
const SOURCE_NAMES = {file: 'Recorded I/Q', hackrf: 'HackRF', rtlsdr: 'RTL-SDR', sige:'SiGe native I/Q', bladerf: 'bladeRF'};
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
      `<div class="workspace-status-strip">${badge('I/Q pipeline available')}${badge('GPS / Galileo acquisition available')}</div><ol class="workspace-next-steps"><li><strong>Prepare the source.</strong> Select a live SDR or a recorded I/Q file, then apply the profile.</li><li><strong>Start reception.</strong> Use Start receiver, then check continuity and native signal statistics in RF observation.</li><li><strong>Inspect the chain.</strong> Choose the receiver channel count and optional reservations, then inspect Channels. Acquisition and tracking run automatically. Navigation and PVT are not yet integrated.</li></ol><div class="workspace-actions">${button('setup', 'Review setup')}${button('health', 'Check system health')}</div>`)}${card('Session at a glance', 'Configured input and the current receiver connection.', rows([
      ['Connection', field('connection')], ['Profile', field('profileState')], ['Native sample format', field('format')], ['Sample rate', field('sampleRate')], ['Signal centre', field('signalFrequency')], ['Position solution', 'Unavailable — PVT module not integrated']
    ]))}</div>`;
}

function acquisitionMarkup() {
  return heading('GNSS / DIAGNOSTICS', 'Search archive', 'Coarse search, Doppler and code refinement, then independent 3-of-4 confirmation.', settingsButton('acquisition', 'Acquisition settings')) +
    '<label class="caf-controls caf-signal-picker">Signal <select aria-label="Acquisition signal" data-acquisition-signal><option value="gps">GPS L1 C/A</option><option value="galileo">Galileo E1-B</option></select></label>' +
    card('Satellite searches', 'Completed searches are referenced to their captured sample epoch. Current receiver states and satellite details are in Channels.',
      rows([['Search worker',field('acquisitionState','Waiting for receiver')],['Satellite searches',field('acquisitionSearches','0')],['Capture gaps',field('acquisitionGaps','0')],['Samples outside capture',field('acquisitionSkipped','0')]]) +
      '<p class="muted" data-acquisition-message></p><div class="table-wrap workspace-table"><table><thead><tr><th>PRN</th><th>Result</th><th>Doppler (Hz)</th><th>Code phase (chips)</th><th>Peak / threshold</th><th>Confirmation</th><th>Capture epoch / sample</th></tr></thead><tbody data-acquisition-results><tr><td colspan="7">Start the receiver to inspect completed searches.</td></tr></tbody></table></div>') +
    card('Completed search space', 'Inspect the last completed coarse round for each satellite, including searches below threshold.',
      '<div data-search-space></div>') + route('rf', 'tracking');
}

function trackingMarkup() {
  return heading('GNSS / RECEIVER', 'Channels', 'One receiver pool: reserved slots and first-confirmed assignments share the same channel state machine.', settingsButton('receiver', 'Channel allocation')) +
    '<section class="card tracking-panel" data-channel-overview><div data-tracking-table></div><p>Select a channel to open its satellite page.</p></section>' +
    '<section data-satellite-view hidden><div class="card tracking-panel"><button type="button" data-channel-back>Back to channels</button><h2 data-satellite-title>Satellite</h2><div class="caf-controls"><label>Channel <select data-tracking-channel aria-label="Tracking channel"></select></label><label>Component <select data-tracking-component aria-label="Correlator component"><option value="magnitude">Magnitude</option><option value="i">I</option><option value="q">Q</option></select></label><label>Vertical range (ADC units) <input data-tracking-range type="number" min="0.000001" step="any" aria-label="Correlator vertical range"></label><button type="button" data-tracking-autoscale>Fit observed range</button></div><p data-tracking-detail></p><p data-satellite-acquisition></p></div>' +
    '<div class="workspace-grid"><section class="card tracking-panel"><h2>Measured correlators</h2><p>Raw coherent values on a common ADC axis. All tap positions follow the code delay relative to the acquisition prediction.</p><svg data-tracking-shape viewBox="0 0 640 260" role="img" aria-label="Measured correlation points"></svg></section><section class="card tracking-panel"><h2>Correlators versus signal time</h2><p>Actual coherent values; breaks mark missing display epochs.</p><svg data-tracking-history viewBox="0 0 640 260" role="img" aria-label="Correlation history in signal seconds"></svg></section></div>' +
    '<section class="card tracking-panel"><h2>Acquisition of this satellite</h2><div data-channel-search-space></div></section></section>';
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
    'Record common signed I/Q on the receiver computer, transfer closed captures and select an existing file as the input source.',
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
        ['Native input and common signed stream', 'module-input'], ['Spectrum / waterfall / I/Q statistics', 'module-analysis'], ['Native I/Q recording and replay', 'module-recording'], ['Satellite acquisition', 'module-acquisition'], ['Channel tracking and navigation decoding', 'module-gnss'], ['PVT / integrity / authentication', 'module-gnss']
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
  let acquisitionSnapshot = {};
  const signalPicker = document.querySelector('[data-acquisition-signal]');
  signalPicker.addEventListener('change', () => update(acquisitionSnapshot));
  const acquisitionTable = createAcquisitionTable(document.querySelector('[data-acquisition-results]'));
  satelliteSearchSpace = createAcquisitionSearchSpace(document.querySelector('[data-channel-search-space]'));
  document.querySelector('[data-channel-search-space] select').setAttribute('aria-label','Channel search space satellite');
  const searchSpace = createAcquisitionSearchSpace(document.querySelector('[data-search-space]'));
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
    searchSpace.setVisible(view === 'acquisition');
    satelliteSearchSpace?.setVisible(view === 'tracking' && /^#tracking[/][0-9]+$/.test(location.hash));
    onViewChange(view);
    if (focus) {
      const panel = document.getElementById(ROUTES[view].id), title = panel?.querySelector('h1,h2');
      if (title) { title.tabIndex = -1; title.focus({preventScroll: true}); }
    }
    return true;
  }
  function fromHash() {
    const view=location.hash.slice(1).split('/')[0];
    show(Object.hasOwn(ROUTES,view)?view:'rf',{},false);
    update(acquisitionSnapshot);
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
    acquisitionSnapshot = snapshot;
    const {status, capabilities, profile = {}, draft = {}, revision, dirty = false, connected = false} = snapshot;
    const live = connected && status ? status : null;
    let channelRows=live?.tracking||[];
    if(!channelRows.length && connected && live?.state==='stopped') {
      const total=Number(profile['receiver.channels']||16),gps=Number(profile['receiver.gps_reserved']||0),gal=Number(profile['receiver.galileo_reserved']||0);
      channelRows=Array.from({length:total},(_,k)=>({channel_id:k+1,reservation:k<gps?1:k<gps+gal?2:0,state:0,constellation:0,prn:0,active:false,taps:[]}));
    }
    updateTracking(channelRows, live?.run_id, live);
    const system = signalPicker.value;
    const signalName = system === 'galileo' ? 'Galileo' : 'GPS';
    const acquisition = system === 'galileo' ? live?.galileo_acquisition : live?.acquisition;
    searchSpace.update(live ? {...live, acquisition, signal:system} : null);
    text('acquisitionState', acquisition?.enabled ? (acquisition.searching ? `Searching ${signalName} ${acquisition.current_prn || ''}` : 'Waiting for capture') : 'Waiting for receiver');
    text('acquisitionSearches', acquisition?.searches || '0');
    text('acquisitionGaps', acquisition?.gaps || '0');
    text('acquisitionSkipped', acquisition?.skipped_samples || '0');
    acquisitionTable.update(acquisition, profile, system);
    const acquisitionMessage = document.querySelector('[data-acquisition-message]');
    if (acquisitionMessage) acquisitionMessage.textContent = acquisition?.message || '';
    const configuredSource = profile['frontend.type'];
    const source = SOURCE_NAMES[configuredSource] || configuredSource || UNAVAILABLE;
    const configuredFormat = configuredSource === 'file' ? profile['replay.format'] : configuredSource === 'bladerf' ? profile['bladerf.format'] : {hackrf: 'CI8', rtlsdr: 'CU8', sige:'CI8'}[configuredSource];
    const started = live && live.active_revision && live.active_revision !== '0';
    const commonFormat=profile['frontend.stream_format']==='CI8'?'CI8':profile['frontend.stream_format']==='CI16'?'CI16':['SC16_Q11','CI16'].includes(configuredFormat)?'CI16':'CI8';
    const format = started ? live.sample_format || UNAVAILABLE : commonFormat;
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
      capturePoint: 'Original stream samples',
      replayState: configuredSource === 'file' ? (profile['replay.path'] ? 'File selected' : 'No file selected') : 'Live SDR selected',
      replayDetail: configuredSource === 'file' ? profile['replay.path'] || 'Choose recorded I/Q to prepare input' : 'Stop reception before switching input',
      architecture: capabilities?.architecture || UNAVAILABLE, operatingSystem: capabilities?.os || UNAVAILABLE,
      version: capabilities?.version ? `v${capabilities.version}` : UNAVAILABLE, fftBackend: capabilities?.fft_backend || UNAVAILABLE,
      fileSupport: capabilities ? 'Available' : UNAVAILABLE,
      frontendSupport: capabilities ? ['hackrf', 'rtlsdr', 'bladerf', 'sige'].map(name => `${SOURCE_NAMES[name]}: ${capabilities[name] === true ? 'supported' : 'not in this build'}`).join(' · ') : UNAVAILABLE,
      deviceContinuity: live?.device_continuity_state || UNAVAILABLE,
      hardwareLoss: live?.hardware_loss_state === 'valid' && live.hardware_lost_samples != null ? counter(live.hardware_lost_samples) : UNAVAILABLE,
      'module-input': capabilities ? 'Available' : 'Waiting for receiver',
      'module-analysis': capabilities ? (capabilities.periodogram ? 'Available' : 'Unavailable') : 'Waiting for receiver',
      'module-recording': capabilities ? 'Available' : 'Waiting for receiver',
      'module-acquisition': capabilities?.gps_acquisition ? 'Available' : UNAVAILABLE,
      'module-gnss': 'Not integrated',
      'stage-setup': dirty ? 'Unsaved changes' : revision ? `Profile r${revision}` : 'Waiting for profile',
      'stage-rf': state,
      'stage-acquisition': acquisition?.enabled ? (acquisition.searching ? `Searching ${signalName}` : `${signalName} enabled`) : `${signalName} awaiting receiver`,
      'stage-tracking': live?.tracking?.some(t => t.active) ? 'Tracking observations' : 'Waiting for confirmed acquisitions',
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

const trackingHistory = new Map();
const trackingRanges = new Map();
let trackingRun;
function updateTracking(channels, run, status) {
  const picker=document.querySelector('[data-tracking-channel]'); if(!picker)return;
  if(trackingRun!==run){trackingHistory.clear();trackingRanges.clear();trackingRun=run;}
  const overview=document.querySelector('[data-channel-overview]');
  const page=document.querySelector('[data-satellite-view]');
  const match=location.hash.match(/^#tracking[/]([0-9]+)$/);
  const selected=match?match[1]:picker.value;
  overview.hidden=!!match;page.hidden=!match;
  picker.replaceChildren(...channels.map(t=>{
    const option=document.createElement('option');option.value=String(t.channel_id);
    option.textContent=`CH ${t.channel_id} | ${t.constellation===1?'GPS':t.constellation===2?'Galileo':'Automatic'} ${t.prn||''}`;
    return option;
  }));
  if(channels.some(t=>String(t.channel_id)===selected))picker.value=selected;
  for(const t of channels) {
    if(!t.active || !Number(t.integrations))continue;
    const key=String(t.channel_id);
    let history=trackingHistory.get(key)||[];
    if(history.length && history.at(-1).prn!==t.prn)history=[];
    const last=history.at(-1);
    if(!last || last.epoch!==t.epoch || last.end_sample!==t.end_sample){history.push(t);if(history.length>240)history.shift();}
    trackingHistory.set(key,history);
  }
  const table=document.querySelector('[data-tracking-table]');table.replaceChildren();
  const counts=document.createElement('p');
  const running=status?.state==='running';
  counts.textContent=running?`${channels.length} channels | ${channels.filter(t=>t.state===2).length} tracking | ${channels.filter(t=>t.state===1).length} acquiring | ${channels.filter(t=>t.state===0).length} waiting`:`${channels.length} configured channels | Receiver ${status?.state||'disconnected'}`;
  table.append(counts);
  const grid=document.createElement('div');grid.className='channel-status-grid';
  const open=id=>{location.hash=`tracking/${id}`;};
  for(const t of channels) {
    const row=document.createElement('button');row.type='button';
    row.dataset.channelId=String(t.channel_id);row.dataset.channelState=running?String(t.state):'0';
    const system=t.constellation===1?'GPS':t.constellation===2?'Galileo':'Unassigned';
    const reserved=t.reservation===1?'GPS reserved':t.reservation===2?'Galileo reserved':'Automatic';
    row.textContent=`CH ${t.channel_id} | ${system} ${t.prn||''} | ${running?['Waiting','Acquisition','Tracking'][t.state]:'Stopped'} | ${reserved}${t.cn0_valid?` | ${t.cn0_db_hz.toFixed(1)} dB-Hz`:''}`;
    row.onclick=()=>open(t.channel_id);grid.append(row);
  }
  table.append(grid);
  picker.onchange=()=>open(picker.value);
  document.querySelector('[data-channel-back]').onclick=()=>{location.hash='tracking';};
  const render=()=>{
    if(page.hidden){satelliteSearchSpace?.setVisible(false);return;}
    const t=channels.find(t=>String(t.channel_id)===picker.value);
    renderTracking(t,trackingHistory.get(picker.value)||[],status);
  };
  document.querySelector('[data-tracking-component]').onchange=render;
  document.querySelector('[data-tracking-range]').onchange=()=>{
    const key=`${run}:${picker.value}:${document.querySelector('[data-tracking-component]').value}`;
    trackingRanges.set(key,Number(document.querySelector('[data-tracking-range]').value));render();
  };
  document.querySelector('[data-tracking-autoscale]').onclick=()=>{
    trackingRanges.delete(`${run}:${picker.value}:${document.querySelector('[data-tracking-component]').value}`);render();
  };
  render();
}
/** Absolute ADC-value geometry; no Prompt division or forced triangle. */
export function trackingPlotPoints(t,component,range) {
  return (t?.taps||[]).map(p=>({x:p.offset_chips+(t.code_delay_chips||0),
    value:component==='magnitude'?Math.hypot(p.i,p.q):p[component],
    i:p.i,q:p.q})).map(p=>({...p,y:component==='magnitude'?225-200*p.value/range:125-100*p.value/range}));
}
function renderTracking(t,history,status) {
  const shape=document.querySelector('[data-tracking-shape]'),time=document.querySelector('[data-tracking-history]');
  const detail=document.querySelector('[data-tracking-detail]');shape.replaceChildren();time.replaceChildren();
  const system=t?.constellation===2?'galileo':'gps';
  document.querySelector('[data-satellite-title]').textContent=t?`Channel ${t.channel_id} | ${t.constellation===1?'GPS':t.constellation===2?'Galileo':'Unassigned'} ${t.prn||''}`:'Channel unavailable';
  const acq=system==='galileo'?status?.galileo_acquisition:status?.acquisition;
  const found=acq?.results?.find(result=>result.prn===t?.prn);
  const note=document.querySelector('[data-satellite-acquisition]');
  note.textContent=found?`Acquisition: ${found.confirmed?'confirmed':'not confirmed'} | Doppler ${found.doppler_hz.toFixed(2)} Hz | code ${found.code_phase_chips.toFixed(4)} chips | ${found.successes}/4 confirmations`:
    t?.state===1?'Acquisition search/refinement/confirmation in progress.':'No completed acquisition for this assignment.';
  satelliteSearchSpace?.update(t?.prn&&status?{...status,signal:system,acquisition:{...acq,results:found?[found]:[]}}:null);
  satelliteSearchSpace?.setVisible(!!t?.prn);
  if(!t || !t.active || !Number(t.integrations)) {
    detail.textContent=t?`${['Waiting for an eligible search','Acquisition in progress','Waiting for first coherent integration'][t.state]} | C/N0 unavailable`:'No configured channels.';return;
  }
  detail.textContent=`${status?.state!=='running'?'Receiver stopped; last observation | ':''}${t.pll?'PLL':'FLL pull-in'} / DLL | Doppler ${t.doppler_hz.toFixed(2)} Hz | code ${t.code_rate_hz.toFixed(3)} chip/s | C/N0 ${t.cn0_valid?`${t.cn0_db_hz.toFixed(1)} dB-Hz`:'unavailable'} | ${t.constellation===2?(t.main_peak_verified?'BOC main peak verified':'BOC peak not yet consistent with the acquisition handoff'):'GPS C/A'} | epoch ${t.epoch}, samples ${t.first_sample}-${t.end_sample}`;
  const component=document.querySelector('[data-tracking-component]').value;
  const key=`${trackingRun}:${t.channel_id}:${component}`;
  let range=trackingRanges.get(key);
  if(!(range>0)) {
    const maximum=Math.max(1e-6,...history.flatMap(h=>(h.taps||[]).map(p=>component==='magnitude'?Math.hypot(p.i,p.q):Math.abs(p[component]))));
    range=2**Math.ceil(Math.log2(maximum*1.5));trackingRanges.set(key,range);
  }
  document.querySelector('[data-tracking-range]').value=String(range);
  const element=(svg,tag,attrs,text)=>{const e=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const[k,v]of Object.entries(attrs))e.setAttribute(k,String(v));if(text)e.textContent=text;svg.append(e);return e;};
  const colors=['#60a5fa','#34d399','#fb923c','#c084fc','#f472b6'];
  const points=trackingPlotPoints(t,component,range);
  const extent=Math.max(1,...(t.taps||[]).map(p=>Math.abs(p.offset_chips)+1));
  const x=d=>45+(d+extent)/(2*extent)*580;
  const axis=(svg,label)=>{
    element(svg,'path',{d:'M45 15V225H625',stroke:'#64748b',fill:'none'});
    element(svg,'text',{x:48,y:12,fill:'currentColor','font-size':11},`${component} (ADC units/sample)`);
    element(svg,'text',{x:190,y:252,fill:'currentColor','font-size':11},label);
    for(const value of component==='magnitude'?[0,range/2,range]:[-range,0,range]) {
      const y=component==='magnitude'?225-200*value/range:125-100*value/range;
      element(svg,'text',{x:2,y:y+4,fill:'currentColor','font-size':10},value.toPrecision(3));
    }
  };
  axis(shape,'Code delay relative to acquisition prediction (chips)');axis(time,'Signal time (seconds within epoch)');
  for(let n=0;n<points.length;++n) {
    const p=points[n],mid=points.length>>1,name=n===mid?'P':n===mid-1?'E':n===mid+1?'L':String(n);
    const dot=element(shape,'circle',{cx:x(p.x),cy:p.y,r:4,fill:colors[n%colors.length],'data-tap':name,'data-value':p.value});
    element(dot,'title',{},`${name}: delay=${p.x}, I=${p.i}, Q=${p.q}`);
    element(shape,'text',{x:x(p.x)+4,y:p.y-8,fill:colors[n%colors.length],'font-size':11},name);
  }
  for(const d of[-extent,0,extent])element(shape,'text',{x:x(d),y:240,fill:'currentColor','font-size':10},d.toFixed(2));
  const recent=history.filter(h=>h.epoch===t.epoch&&h.prn===t.prn&&h.taps.length===points.length);
  if(!recent.length||!t.sample_rate_hz)return;
  const first=Number(recent[0].first_sample)/t.sample_rate_hz,last=Number(t.end_sample)/t.sample_rate_hz;
  for(let n=0;n<points.length;++n) {
    let path='',previous;
    for(const h of recent) {
      const p=trackingPlotPoints(h,component,range)[n];
      const px=45+580*(Number(h.end_sample)/t.sample_rate_hz-first)/Math.max(last-first,1e-6);
      element(time,'circle',{cx:px,cy:p.y,r:2,fill:colors[n%colors.length]});
      path+=`${!previous||previous.end_sample!==h.first_sample?'M':'L'}${px} ${p.y} `;previous=h;
    }
    element(time,'path',{d:path,fill:'none',stroke:colors[n%colors.length],'stroke-width':1.5});
  }
  element(time,'text',{x:45,y:240,fill:'currentColor','font-size':10},first.toFixed(3));
  element(time,'text',{x:570,y:240,fill:'currentColor','font-size':10},last.toFixed(3));
}