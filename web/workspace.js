/* Receiver workspace: navigation and presentation of controller snapshots only. */
import {createAcquisitionSearchSpace} from './acquisition-search-space.js';
import {createPvtMap} from './pvt-map.js';
let satelliteSearchSpace = null;
const ROUTES = {
  journey: {label: 'Overview', id: 'journeyView'},
  setup: {label: 'Receiver configuration', id: 'configurationView'},
  rf: {label: 'RF observation', id: 'observationView'},
  tracking: {label: 'Channels', id: 'trackingView'},
  navigation: {label: 'Navigation data', id: 'navigationView'},
  pvt: {label: 'PVT solution', id: 'pvtView'},
  record: {label: 'Recording & replay', id: 'recordingView'},
  health: {label: 'System health', id: 'healthView'}
};
const STAGES = [
  ['setup', 'Receiver configuration', 'Configure the front end, receiver channels, acquisition, tracking and navigation.'],
  ['rf', 'RF observation', 'Check I/Q continuity, spectrum, waterfall and amplitude.'],
  ['tracking', 'Channels', 'Inspect active channels, acquisition and tracking state.'],
  ['navigation', 'Navigation', 'Decode messages, satellite time and valid ephemerides.'],
  ['pvt', 'PVT solution', 'Inspect position, velocity, time and measurement quality.'],
  ['record', 'Record & replay', 'Save common signed I/Q and return to a recorded input.']
];
const SETTINGS = new Set(['source', 'receiver', 'acquisition', 'tracking', 'navigation', 'recording', 'analysis', 'display', 'profile']);
const SOURCE_NAMES = {file: 'Recorded I/Q', hackrf: 'HackRF', rtlsdr: 'RTL-SDR', sige:'SiGe native I/Q', bladerf: 'bladeRF'};
const UNAVAILABLE = 'Unavailable';
const CONSTELLATIONS = {
  1: {name: 'GPS', prefix: 'G', colour: '#69c6f4', acquisition: 'acquisition'},
  2: {name: 'Galileo', prefix: 'E', colour: '#f4bc68', acquisition: 'galileo_acquisition'},
  3: {name: 'BeiDou B1C', prefix: 'C', colour: '#c8a4ff', acquisition: 'beidou_acquisition'}
};
const previewButton = '<button type="button" data-beidou-preview aria-pressed="false">Preview BeiDou B1C</button>';
const previewNotice = '<p class="workspace-callout" data-beidou-preview-notice role="status" hidden>BeiDou B1C preview · Synthetic telemetry. Receiver controls still apply to the live session.</p>';
const beidouAvailability = '<p class="muted" data-beidou-availability hidden>Live BeiDou B1C unavailable in this receiver build.</p>';

/** Presentation fixture. It never enters receiver telemetry or sends observation requests. */
function createBeidouPreview() {
  const epoch = '1', sample = '8000000';
  const beidouSky = [
    {prn: 24, azimuth_deg: 20, elevation_deg: 58},
    {prn: 33, azimuth_deg: 106, elevation_deg: 39},
    {prn: 61, azimuth_deg: 210, elevation_deg: 50},
    {prn: 63, azimuth_deg: 301, elevation_deg: 27}
  ].map(sat => ({...sat, constellation: 3, used: true}));
  const tracking = [12, 44, 35, ...beidouSky.map(sat => sat.prn)].map((prn, index) => {
    const active = index > 0, decoded = index > 1;
    return {
      channel_id: index + 1, constellation: 3, reservation: 0, prn, active,
      state: active ? 2 : 1, pll: decoded, cn0_valid: active, cn0_db_hz: 36 + index,
      doppler_hz: 1200 - 220 * index, code_rate_hz: 1023000.5, code_delay_chips: 0,
      main_peak_verified: decoded, epoch, end_sample: sample, first_sample: '7920000',
      tracking_start_sample: '7500000',
      integrations: active ? '1' : '0', sample_rate_hz: 8000000, bit_synchronized: decoded,
      navigation_symbols: decoded ? '1800' : '0', valid_subframes: decoded ? '3' : '0',
      tow_valid: decoded, tow_seconds: 12340, ephemeris_valid: index > 2, subframe_id: decoded ? 3 : 0,
      taps: active ? [{offset_chips: -0.25, i: 0.8, q: 0.4}, {offset_chips: 0, i: 2.0, q: 0.6},
        {offset_chips: 0.25, i: 0.9, q: 0.3}] : []
    };
  });
  const otherSky = [
    {prn: 12, constellation: 1, azimuth_deg: 55, elevation_deg: 35, used: true},
    {prn: 24, constellation: 2, azimuth_deg: 153, elevation_deg: 66, used: true}
  ];
  const sky = [...beidouSky, ...otherSky];
  return {
    preview: true, state: 'running', run_id: 'beidou-preview',
    tracking: [...tracking, ...otherSky.map(({prn, constellation}, index) => ({
      ...tracking[3], channel_id: tracking.length + index + 1, prn, constellation
    }))],
    beidou_acquisition: {results: tracking.map(({prn, doppler_hz}) => ({
      prn, doppler_hz, epoch, confirmed: true, code_phase_chips: 190.125, successes: 3
    }))},
    pvt: {available: true, satellites: sky.length, gps: 1, galileo: 1, beidou: beidouSky.length,
      latitude_deg: 45.07, longitude_deg: 7.69, height_m: 250, residual_rms_m: 1.8,
      tow_seconds: 12354.08, clock_gps_m: 30.2, clock_galileo_m: 33.5, clock_beidou_m: 27.6,
      epoch, sample, sky,
      dop_available: true, gdop: 3.4, hdop: 1.6, vdop: 2.7,
      model: 'Synthetic GPS, Galileo and BeiDou solution for interface testing.'}
  };
}


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
      `<div class="workspace-status-strip">${badge('I/Q pipeline available')}${badge('GPS / Galileo acquisition available')}</div><ol class="workspace-next-steps"><li><strong>Prepare the source.</strong> Select a live SDR or a recorded I/Q file, then apply the profile.</li><li><strong>Start reception.</strong> Use Start receiver, then check continuity and native signal statistics in RF observation.</li><li><strong>Inspect the chain.</strong> Choose the receiver channel count and optional reservations, then inspect Channels. Acquisition and tracking run automatically. GPS/Galileo navigation and broadcast PVT follow validated messages.</li></ol><div class="workspace-actions">${button('setup', 'Review setup')}${button('health', 'Check system health')}</div>`)}${card('Session at a glance', 'Configured input and the current receiver connection.', rows([
      ['Connection', field('connection')], ['Profile', field('profileState')], ['Native sample format', field('format')], ['Sample rate', field('sampleRate')], ['Signal centre', field('signalFrequency')], ['Position solution', field('stage-pvt')]
    ]))}</div>`;
}

function trackingMarkup() {
  return heading('GNSS / RECEIVER', 'Channels', 'One receiver pool: reserved slots and first-confirmed assignments share the same channel state machine.', settingsButton('receiver', 'Channel allocation') + previewButton) + previewNotice + beidouAvailability +
    '<section class="card tracking-panel" data-channel-overview><div data-tracking-table></div><p>Select a channel to open its satellite page.</p></section>' +
    '<section data-satellite-view hidden><div class="card tracking-panel"><button type="button" data-channel-back>Back to channels</button><h2 data-satellite-title>Satellite</h2><p data-satellite-stages></p><div class="caf-controls"><label>Channel <select data-tracking-channel aria-label="Tracking channel"></select></label><label>Vertical range (ADC units) <input data-tracking-range type="number" min="0.000001" step="any" aria-label="Correlator vertical range"></label><button type="button" data-tracking-autoscale>Automatic range</button></div><p data-tracking-detail></p><p data-satellite-acquisition></p></div>' +
    '<section class="card tracking-panel"><h2>Acquisition of this satellite</h2><div data-channel-search-space></div></section>' +
    '<div class="workspace-grid"><section class="card tracking-panel"><h2>Measured correlators</h2><p>Magnitude in ADC units per sample, with a range fitted to the channel.</p><svg data-tracking-shape viewBox="0 0 640 260" role="img" aria-label="Measured correlation points"></svg></section><section class="card tracking-panel"><h2>Correlators versus signal time</h2><p>Magnitude over a rolling 20 s window.</p><svg data-tracking-history viewBox="0 0 640 260" role="img" aria-label="Correlation history in signal seconds"></svg></section></div>' +
    '<div class="workspace-grid"><section class="card tracking-panel"><h2>Prompt I/Q scatter</h2><svg data-tracking-scatter viewBox="0 0 640 260" role="img" aria-label="Prompt I versus Q"></svg></section><section class="card tracking-panel"><h2>Signed prompt I versus time</h2><p>Coherent prompt samples received by the GUI.</p><svg data-tracking-prompt viewBox="0 0 640 260" role="img" aria-label="Signed prompt I in signal seconds"></svg></section></div></section>';
}
function navigationMarkup() {
  return heading('NAVIGATION DATA', 'Broadcast navigation messages',
    'Parity and CRC validate messages. Consistent orbit and clock issues feed common-epoch observables.',
    settingsButton('navigation','Navigation & PVT settings')+button('tracking','Inspect channels')+button('pvt','PVT solution')+previewButton)+previewNotice+beidouAvailability+
    '<section class="card tracking-panel"><h2>Broadcast navigation</h2><div class="channel-list-wrap" data-navigation-status></div><p>UTC conversion, authentication and integrity qualification are not available.</p></section>';
}

function emptySkyplot() {
  return `<div class="workspace-skyplot"><svg viewBox="0 0 300 276" role="img" aria-label="Empty skyplot: no satellite positions available"><g fill="none" stroke="currentColor" opacity=".25"><circle cx="150" cy="136" r="106"/><circle cx="150" cy="136" r="71"/><circle cx="150" cy="136" r="35"/><path d="M44 136H256M150 30V242"/></g><g fill="currentColor" text-anchor="middle" font-size="12"><text x="150" y="17">N</text><text x="275" y="140">E</text><text x="150" y="264">S</text><text x="25" y="140">W</text></g><text x="150" y="132" fill="currentColor" text-anchor="middle" font-size="12">No satellite positions</text><text x="150" y="150" fill="currentColor" text-anchor="middle" font-size="11">available</text></svg></div>`;
}

function pvtMarkup() {
  return heading('POSITION, VELOCITY & TIME', 'Broadcast PVT',
    'Code and Doppler measurements share one input epoch. Each contributing constellation has its own code clock bias.',
    settingsButton('navigation', 'Navigation & PVT settings') + button('tracking', 'Inspect channels') + previewButton) + previewNotice + beidouAvailability +
    '<div class="pvt-overview"><section class="card tracking-panel"><h2 data-pvt-state>No solution available</h2><p data-pvt-counts></p><dl data-pvt-values></dl><p data-pvt-model></p></section>' +
    '<section class="card tracking-panel pvt-sky-panel"><h2>Satellite skyplot</h2><p>GPS in blue, Galileo in amber, BeiDou in violet. The edge is the horizon.</p><div data-pvt-sky></div></section></div>' +
    '<section class="card tracking-panel"><h2>Receiver position and track</h2><div data-pvt-map></div></section>';
}

/** Draw only real broadcast geometry, and only while the PVT view is visible. */
function renderSkyplot(pvt) {
  const host=document.querySelector('[data-pvt-sky]');
  if(document.getElementById('pvtView').hidden || document.getElementById('pvtView').classList.contains('hidden'))return;
  const key=JSON.stringify([pvt?.epoch,pvt?.sample,pvt?.available,pvt?.sky]);
  if(host.dataset.geometry===key)return;
  host.dataset.geometry=key;host.replaceChildren();
  if(!pvt?.available || !pvt.sky?.length){host.textContent='Waiting for a position and valid broadcast geometry.';return;}
  const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg');
  svg.setAttribute('viewBox','0 0 360 360');svg.setAttribute('role','img');
  svg.setAttribute('aria-label','Satellite azimuth and elevation');svg.classList.add('pvt-skyplot');
  const add=(tag,attrs,text)=>{const e=document.createElementNS(ns,tag);for(const [k,v] of Object.entries(attrs))e.setAttribute(k,v);if(text!==undefined)e.textContent=text;svg.append(e);return e;};
  for(const r of [50,100,150])add('circle',{cx:180,cy:180,r,fill:'none',stroke:'#476078'});
  add('path',{d:'M30 180H330M180 30V330',stroke:'#476078',fill:'none'});
  for(const [label,x,y] of [['N',180,18],['E',346,185],['S',180,352],['W',14,185],['30°',188,84],['60°',188,134]])add('text',{x,y,fill:'#9bb8cc','text-anchor':'middle'},label);
  for(const sat of pvt.sky){
    if(!Number.isFinite(sat.azimuth_deg)||!Number.isFinite(sat.elevation_deg)||sat.elevation_deg<0)continue;
    const r=150*(1-sat.elevation_deg/90),az=sat.azimuth_deg*Math.PI/180,x=180+r*Math.sin(az),y=180-r*Math.cos(az);
    const system=CONSTELLATIONS[sat.constellation];
    if(!system)continue;
    const colour=system.colour,label=system.prefix+String(sat.prn).padStart(2,'0');
    const dot=add('circle',{cx:x,cy:y,r:7,fill:colour});
    const title=document.createElementNS(ns,'title');title.textContent=`${label}: azimuth ${sat.azimuth_deg.toFixed(1)}°, elevation ${sat.elevation_deg.toFixed(1)}°`;dot.append(title);
    add('text',{x,y:y-12,fill:colour,'text-anchor':'middle','font-size':12},label);
  }
  host.append(svg);
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
    `<div class="workspace-kpis">${kpi('CONNECTION', field('connection'), field('receiverState'))}${kpi('PLATFORM', field('architecture'), field('operatingSystem'))}${kpi('RECEIVER VERSION', field('version'), field('profileState'))}${kpi('FFT BACKEND', field('fftBackend'), 'Available integer backend in this receiver build')}</div>` +
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
export function createReceiverWorkspace({onViewChange = () => {}, onOpenSettings = () => {}, onBrowseReplay = () => {}, onPlotInterestChange = () => {}, onChannelInterest = () => {}} = {}) {
  const renderers = {journey: journeyMarkup, tracking: trackingMarkup, navigation: navigationMarkup, pvt: pvtMarkup};
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
  const previewStatus = createBeidouPreview();
  const isPreviewUrl = () => new URL(location.href).searchParams.get('preview') === 'beidou';
  let beidouPreview = isPreviewUrl();
  const previewButtons = [...document.querySelectorAll('[data-beidou-preview]')];
  const previewNotices = [...document.querySelectorAll('[data-beidou-preview-notice]')];
  const availabilityNotices = [...document.querySelectorAll('[data-beidou-availability]')];
  const mapPanel = document.querySelector('[data-pvt-map]').closest('section');
  for (const control of previewButtons) control.addEventListener('click', () => {
    beidouPreview = !beidouPreview;
    const url = new URL(location.href);
    if (beidouPreview) url.searchParams.set('preview', 'beidou'); else url.searchParams.delete('preview');
    history.replaceState(null, '', url);
    update(acquisitionSnapshot);
  });
  satelliteSearchSpace = createAcquisitionSearchSpace(document.querySelector('[data-channel-search-space]'));
  const pvtMap = createPvtMap(document.querySelector('[data-pvt-map]'));
  document.querySelector('[data-channel-search-space] select').setAttribute('aria-label','Channel search space satellite');
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
    satelliteSearchSpace?.setVisible(view === 'tracking' && /^#tracking[/][0-9]+$/.test(location.hash));
    onViewChange(view);
    pvtMap.setVisible(view === 'pvt' && !beidouPreview);
    if(view!=='tracking')onChannelInterest(0);
    if (focus) {
      const panel = document.getElementById(ROUTES[view].id), title = panel?.querySelector('h1,h2');
      if (title) { title.tabIndex = -1; title.focus({preventScroll: true}); }
    }
    return true;
  }
  function fromHash() {
    beidouPreview = isPreviewUrl();
    const requested=location.hash.slice(1).split('/')[0];
    const view=requested==='acquisition'?'tracking':requested;
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
    const display = beidouPreview ? previewStatus : live;
    for (const control of previewButtons) {
      control.setAttribute('aria-pressed', String(beidouPreview));
      control.textContent = beidouPreview ? 'Exit BeiDou preview' : 'Preview BeiDou B1C';
    }
    for (const notice of previewNotices) notice.hidden = !beidouPreview;
    for (const notice of availabilityNotices)
      notice.hidden = !connected || beidouPreview || capabilities?.beidou_b1c_operational === true;
    let channelRows=display?.tracking||[];
    if(!channelRows.length && connected && display?.state==='stopped') {
      const total=Number(profile['receiver.channels']||16),gps=Number(profile['receiver.gps_reserved']||0),gal=Number(profile['receiver.galileo_reserved']||0),bds=Number(profile['receiver.beidou_reserved']||0);
      channelRows=Array.from({length:total},(_,k)=>({channel_id:k+1,reservation:k<gps?1:k<gps+gal?2:k<gps+gal+bds?3:0,state:0,constellation:0,prn:0,active:false,taps:[]}));
    }
    updateTracking(channelRows, display?.run_id, display);
    const navigationTable=document.querySelector('[data-navigation-status]');navigationTable.replaceChildren();
    const nt=document.createElement('table'),nh=nt.createTHead().insertRow(),nb=nt.createTBody();
    for(const title of ['Channel','Signal / PRN','Decoding','Message parts valid','TOW (s)','Ephemerides']){const th=document.createElement('th');th.scope='col';th.textContent=title;nh.append(th);}
    for (const t of channelRows.filter(t=>t.active)) {
      const row=nb.insertRow();
      const system=CONSTELLATIONS[t.constellation]?.name || 'Unassigned';
      const parts=t.constellation===3 && Number(t.valid_subframes)
        ? `B-CNAV1 · ${t.valid_subframes}${t.subframe_id ? ` · Page ${t.subframe_id}` : ''}`
        : t.valid_subframes || '0';
      for (const value of [t.channel_id, `${system} ${t.prn}`, channelStages(t, display).decoding, parts,
        t.tow_valid ? `${t.tow_seconds}${t.constellation===3 ? ' BDT' : ''}` : 'Unavailable',
        t.ephemeris_valid ? 'Consistent issue received' : 'Incomplete'])
        row.insertCell().textContent=String(value);
    }
    if(!nb.rows.length){const cell=nb.insertRow().insertCell();cell.colSpan=nh.cells.length;cell.textContent='No navigation data available';}
    navigationTable.append(nt);
    const observed=!beidouPreview&&selected==='tracking'&&live?.state==='running'?location.hash.match(/^#tracking[/]([0-9]+)$/):null;
    onChannelInterest(observed?Number(observed[1]):0);
    const pvt=display?.pvt, pvtState=document.querySelector('[data-pvt-state]');
    mapPanel.hidden = beidouPreview;
    pvtMap.setVisible(selected === 'pvt' && !beidouPreview);
    pvtMap.update(live?.pvt, live?.run_id, live?.state === 'running');
    renderSkyplot(pvt);
    pvtState.textContent=pvt?.available?'Broadcast least-squares solution':'No solution available';
    document.querySelector('[data-pvt-counts]').textContent=pvt?`${pvt.available?'Used measurements':'Eligible measurements'}: ${pvt.satellites ?? 0} (GPS ${pvt.gps ?? 0}, Galileo ${pvt.galileo ?? 0}, BeiDou ${pvt.beidou ?? 0}) | Epoch ${pvt.epoch ?? '—'}, sample ${pvt.sample ?? '—'}`:'Waiting for receiver';
    document.querySelector('[data-pvt-model]').textContent=pvt?.model||'Waiting for validated navigation data and consistent ephemerides.';
    const pvtValues=document.querySelector('[data-pvt-values]');pvtValues.replaceChildren();
    if(pvt?.available) for(const [label,value] of [['Latitude',pvt.latitude_deg.toFixed(8)+' deg'],['Longitude',pvt.longitude_deg.toFixed(8)+' deg'],['Ellipsoidal height',pvt.height_m.toFixed(2)+' m'],['Residual RMS',pvt.residual_rms_m.toFixed(2)+' m'],['Nominal receiver TOW',pvt.tow_seconds.toFixed(6)+' s'],['Velocity ECEF',pvt.velocity_available?[pvt.velocity_x_mps,pvt.velocity_y_mps,pvt.velocity_z_mps].map(v=>v.toFixed(3)).join(', ')+' m/s':'Unavailable']]) {
      const row=document.createElement('div'),term=document.createElement('dt'),description=document.createElement('dd');term.textContent=label;description.textContent=value;row.append(term,description);pvtValues.append(row);
    }
    if(pvt?.available) for(const label of ['GDOP','HDOP','VDOP']) {
      const row=document.createElement('div'),term=document.createElement('dt'),description=document.createElement('dd');
      term.textContent=label;description.textContent=pvt.dop_available?Number(pvt[label.toLowerCase()]).toFixed(2):'Unavailable';row.append(term,description);pvtValues.append(row);
    }
    if (pvt?.available) for (const [system, label] of [['gps', 'GPS'], ['galileo', 'Galileo'], ['beidou', 'BeiDou']]) {
      const bias = pvt[`clock_${system}_m`];
      if (!(pvt[system] > 0) || !Number.isFinite(bias)) continue;
      const row=document.createElement('div'),term=document.createElement('dt'),value=document.createElement('dd');
      term.textContent=`${label} clock bias`;value.textContent=`${bias.toFixed(2)} m`;
      row.append(term,value);pvtValues.append(row);
    }
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
      'stage-tracking': live?.tracking?.some(t => t.active && Number(t.integrations) > 0) ? 'Tracking observations' : 'Waiting for tracking observations',
      'stage-navigation': live?.tracking?.some(t => t.active && (t.tow_valid || Number(t.valid_subframes) > 0)) ? 'Decoded messages' : 'Waiting for decoded messages',
      'stage-pvt': live?.pvt?.available ? 'Broadcast solution available' : 'No solution available',
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
const trackingWindowSeconds = 20;
let trackingRun;

function acquisitionResult(channel, status) {
  const acquisition = status?.[CONSTELLATIONS[channel?.constellation]?.acquisition];
  return acquisition?.results?.find(result => result.prn === channel.prn &&
    (channel.epoch == null || result.epoch == null || String(result.epoch) === String(channel.epoch)));
}

function channelStages(channel, status) {
  const pvt = status?.pvt;
  const acquired = channel.active || acquisitionResult(channel, status)?.confirmed;
  const integrated = channel.active && Number(channel.integrations) > 0;
  const sameEpoch = channel.epoch != null && String(channel.epoch) === String(pvt?.epoch);
  const sameAssignment = channel.tracking_start_sample == null ||
    (pvt?.sample != null && BigInt(pvt.sample) > BigInt(channel.tracking_start_sample));
  const used = integrated && pvt?.available && sameEpoch && sameAssignment &&
    pvt.sky?.some(sat => sat.used !== false && sat.constellation === channel.constellation &&
      sat.prn === channel.prn);
  return {
    acquisition: acquired ? 'Confirmed' : channel.state === 1 ? 'Searching' : '—',
    tracking: integrated ? 'Tracking' : channel.active ? 'Starting' : '—',
    decoding: channel.active && (channel.tow_valid || Number(channel.valid_subframes) > 0) ? 'Decoded' :
      channel.bit_synchronized ? 'Demodulating' : '—',
    pvt: used ? 'Used in PVT' : '—'
  };
}

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
    option.textContent=`CH ${t.channel_id} | ${CONSTELLATIONS[t.constellation]?.name || 'Automatic'} ${t.prn||''}`;
    return option;
  }));
  if(channels.some(t=>String(t.channel_id)===selected))picker.value=selected;
  for(const t of channels) {
    if(!t.active || !Number(t.integrations))continue;
    const key=String(t.channel_id);
    let history=trackingHistory.get(key)||[];
    if(history.length && (history.at(-1).prn!==t.prn || history.at(-1).constellation!==t.constellation || history.at(-1).epoch!==t.epoch || history.at(-1).tracking_start_sample!==t.tracking_start_sample))history=[];
    const last=history.at(-1);
    if(!last || last.epoch!==t.epoch || last.end_sample!==t.end_sample){history.push(t);}
    const cutoff=Number(t.end_sample)-trackingWindowSeconds*t.sample_rate_hz;
    while(history.length>1 && Number(history[0].end_sample)<cutoff)history.shift();
    trackingHistory.set(key,history);
  }
  const table=document.querySelector('[data-tracking-table]');table.replaceChildren();
  const counts=document.createElement('p');
  const running=status?.state==='running';
  counts.textContent=running?`${channels.length} channels | ${channels.filter(t=>t.state===2).length} tracking | ${channels.filter(t=>t.state===1).length} acquiring | ${channels.filter(t=>t.state===0).length} waiting`:`${channels.length} configured channels | Receiver ${status?.state||'disconnected'}`;
  table.append(counts);
  const wrap=document.createElement('div');wrap.className='channel-list-wrap';
  const grid=document.createElement('table');grid.className='channel-list';
  const head=grid.createTHead().insertRow();
  for(const label of ['Channel','Signal','PRN','Acquisition','Tracking','Loop','C/N0 (dB-Hz)','Doppler (Hz)','Allocation','Decoding','TOW (s)','PVT']) {
    const cell=document.createElement('th');cell.scope='col';cell.textContent=label;head.append(cell);
  }
  const body=grid.createTBody();
  const open=id=>{location.hash=`tracking/${id}`;};
  for(const t of channels) {
    const row=body.insertRow();
    row.dataset.channelId=String(t.channel_id);
    const system=CONSTELLATIONS[t.constellation]?.name || 'Unassigned';
    const reserved=CONSTELLATIONS[t.reservation] ? `${CONSTELLATIONS[t.reservation].name} reserved` : 'Automatic';
    const first=row.insertCell(),link=document.createElement('a');
    link.href=`#tracking/${t.channel_id}`;link.textContent=`CH ${t.channel_id}`;first.append(link);
    const stages=channelStages(t,status);
    for(const value of [system,t.prn||'—',stages.acquisition,stages.tracking,
      t.active?(t.pll?'PLL / DLL':'FLL / DLL'):'—',t.cn0_valid?t.cn0_db_hz.toFixed(1):'—',
      t.active?Number(t.doppler_hz).toFixed(1):'—',reserved,stages.decoding,
      t.tow_valid?String(t.tow_seconds):'—',stages.pvt])row.insertCell().textContent=value;
    row.cells[4].classList.toggle('tracking-active', running && stages.tracking === 'Tracking');
    row.onclick=event=>{if(!event.target.closest('a'))open(t.channel_id);};
  }
  for(const row of body.rows) {
    const t=channels.find(t=>String(t.channel_id)===row.dataset.channelId), cell=row.cells[6];
    const value=cell.textContent;cell.replaceChildren();
    const meter=document.createElement('meter'),number=document.createElement('span');
    meter.min=0;meter.max=60;meter.low=25;meter.high=35;meter.optimum=45;
    meter.value=t.cn0_valid?Math.max(0,Math.min(60,t.cn0_db_hz)):0;
    meter.setAttribute('aria-label',t.cn0_valid?'C/N0 '+value+' dB-Hz':'C/N0 unavailable');
    number.textContent=value;cell.className='channel-cn0';cell.append(meter,number);
  }
  wrap.append(grid);table.append(wrap);
  picker.onchange=()=>open(picker.value);
  document.querySelector('[data-channel-back]').onclick=()=>{location.hash='tracking';};
  const render=()=>{
    if(page.hidden){satelliteSearchSpace?.setVisible(false);return;}
    const t=channels.find(t=>String(t.channel_id)===picker.value);
    renderTracking(t,trackingHistory.get(picker.value)||[],status);
  };
  document.querySelector('[data-tracking-range]').onchange=()=>{
    const key=`${run}:${picker.value}`;
    trackingRanges.set(key,Number(document.querySelector('[data-tracking-range]').value));render();
  };
  document.querySelector('[data-tracking-autoscale]').onclick=()=>{
    trackingRanges.delete(`${run}:${picker.value}`);render();
  };
  render();
}
/** Absolute ADC-value geometry; no Prompt division or forced triangle. */
export function trackingPlotPoints(t,component,range) {
  return (t?.taps||[]).map(p=>({x:(p.offset_chips+(t.code_delay_chips||0))*(t.constellation===2?2:1),
    value:component==='magnitude'?Math.hypot(p.i,p.q):p[component],
    i:p.i,q:p.q})).map(p=>({...p,y:component==='magnitude'?225-200*p.value/range:125-100*p.value/range}));
}
function renderTracking(t,history,status) {
  const shape=document.querySelector('[data-tracking-shape]'),time=document.querySelector('[data-tracking-history]');
  const detail=document.querySelector('[data-tracking-detail]');
  const scatter=document.querySelector('[data-tracking-scatter]'),prompt=document.querySelector('[data-tracking-prompt]');
  for(const svg of [shape,time,scatter,prompt])svg.replaceChildren();
  const system=t?.constellation===2?'galileo':t?.constellation===3?'beidou':'gps';
  document.querySelector('[data-satellite-title]').textContent=t?`Channel ${t.channel_id} | ${CONSTELLATIONS[t.constellation]?.name || 'Unassigned'} ${t.prn||''}`:'Channel unavailable';
  const acq=status?.[CONSTELLATIONS[t?.constellation]?.acquisition];
  const found=acquisitionResult(t,status);
  const stages=t&&channelStages(t,status);
  document.querySelector('[data-satellite-stages]').textContent=stages?
    `Acquisition: ${stages.acquisition} | Tracking: ${stages.tracking} | Decoding: ${stages.decoding} | PVT: ${stages.pvt}`:'';
  const note=document.querySelector('[data-satellite-acquisition]');
  note.textContent=found?`Acquisition: ${found.confirmed?'confirmed':'not confirmed'} | Doppler ${found.doppler_hz.toFixed(2)} Hz | code ${found.code_phase_chips.toFixed(4)} chips | ${found.successes}/4 confirmations`:
    t?.state===1?'Acquisition search/refinement/confirmation in progress.':
      t?.active?'Acquisition confirmed for this assignment.':'No completed acquisition for this assignment.';
  const showMap=!!t?.prn && system!=='beidou' && !status?.preview;
  document.querySelector('[data-channel-search-space]').closest('section').hidden=!showMap;
  satelliteSearchSpace?.update(showMap&&status?{...status,signal:system,acquisition:{...acq,results:found?[found]:[]}}:null);
  satelliteSearchSpace?.setVisible(showMap);
  if(!t || !t.active || !Number(t.integrations)) {
    detail.textContent=t?`${t.active?'Waiting for first coherent integration':stages.acquisition==='Confirmed'?'Waiting for tracking assignment':t.state===1?'Acquisition in progress':'Waiting for an eligible search'} | C/N0 unavailable`:'No configured channels.';return;
  }
  detail.textContent=`${status?.state!=='running'?'Receiver stopped; last observation | ':''}${t.pll?'PLL':'FLL pull-in'} / DLL | Doppler ${t.doppler_hz.toFixed(2)} Hz | code ${t.code_rate_hz.toFixed(3)} chip/s | C/N0 ${t.cn0_valid?`${t.cn0_db_hz.toFixed(1)} dB-Hz`:'unavailable'} | ${t.constellation!==1?(t.main_peak_verified?'BOC main peak verified':'BOC peak not yet consistent with the acquisition handoff'):'GPS C/A'} | epoch ${t.epoch}, samples ${t.first_sample}-${t.end_sample}`;
  const component='magnitude';
  const key=`${trackingRun}:${t.channel_id}`;
  const manual=trackingRanges.get(key);
  const limit=(samples)=>1.15*samples.reduce((max,h)=>
    h.taps.reduce((peak,p)=>Math.max(peak,Math.hypot(p.i,p.q)),max),1e-6);
  const shapeRange=manual>0 && Number.isFinite(manual)?manual:limit([t]);
  const range=manual>0 && Number.isFinite(manual)?manual:limit(history);
  document.querySelector('[data-tracking-range]').value=Number(shapeRange.toPrecision(4));
  const element=(svg,tag,attrs,text)=>{const e=document.createElementNS('http://www.w3.org/2000/svg',tag);for(const[k,v]of Object.entries(attrs))e.setAttribute(k,String(v));if(text)e.textContent=text;svg.append(e);return e;};
  const colors=['#60a5fa','#34d399','#fb923c','#c084fc','#f472b6'];
  const points=trackingPlotPoints(t,component,shapeRange);
  const center=(t.code_delay_chips||0)*(t.constellation===2?2:1);
  const extent=Math.max(0.25,...points.map(p=>Math.abs(p.x-center)))*1.35;
  const x=d=>45+(d-center+extent)/(2*extent)*580;
  const axis=(svg,label,range,signed=false)=>{
    element(svg,'path',{d:'M45 15V225H625',stroke:'#64748b',fill:'none'});
    element(svg,'text',{x:48,y:12,fill:'currentColor','font-size':11},`${signed?"Prompt I":"Magnitude"} (ADC units/sample)`);
    element(svg,'text',{x:190,y:252,fill:'currentColor','font-size':11},label);
    for(const value of signed?[-range,0,range]:[0,range/2,range]) {
      const y=signed?125-100*value/range:225-200*value/range;
      element(svg,'path',{d:`M45 ${y}H625`,stroke:'#263c52','stroke-dasharray':'3 4'});
      element(svg,'text',{x:2,y:y+4,fill:'currentColor','font-size':10},value.toPrecision(3));
    }
  };
  axis(shape,`Code delay relative to acquisition (${t.constellation===2?'half-chip slots':'chips'})`,shapeRange);axis(time,'Signal time (s)',range);axis(prompt,'Signal time (s)',range,true);
  element(shape,'polyline',{points:points.map(p=>`${x(p.x)},${p.y}`).join(' '),fill:'none',stroke:'#6ecbdd','stroke-width':2});
  for(let n=0;n<points.length;++n) {
    const p=points[n],mid=points.length>>1,name=n===mid?'P':n===mid-1?'E':n===mid+1?'L':String(n);
    const dot=element(shape,'circle',{cx:x(p.x),cy:p.y,r:4,fill:colors[n%colors.length],'data-tap':name,'data-value':p.value});
    element(dot,'title',{},`${name}: delay=${p.x}, I=${p.i}, Q=${p.q}`);
    element(shape,'text',{x:x(p.x)+4,y:p.y-8,fill:colors[n%colors.length],'font-size':11},name);
  }
  for(const d of[center-extent,center,center+extent])element(shape,'text',{x:x(d),y:240,fill:'currentColor','font-size':10},d.toFixed(2));
  const recent=history.filter(h=>h.epoch===t.epoch&&h.prn===t.prn&&h.taps.length===points.length);
  if(!recent.length||!t.sample_rate_hz)return;
  const end=Number(t.end_sample)/t.sample_rate_hz;
  const signalX=h=>625-580*(Number(t.end_sample)-Number(h.end_sample))/t.sample_rate_hz/trackingWindowSeconds;
  const plot=(svg,value,colour)=>{
    let path='',previous;
    for(const h of recent){
      const px=signalX(h),y=value(h);
      if(px<45 || y<25 || y>225)continue;
      element(svg,'circle',{cx:px,cy:y,r:2,fill:colour});
      path+=(previous?.end_sample===h.first_sample?'L':'M')+px+' '+y+' ';
      previous=h;
    }
    if(path)element(svg,'path',{d:path,fill:'none',stroke:colour,'stroke-width':1.5});
  };
  for(let n=0;n<points.length;++n)
    plot(time,h=>225-200*Math.hypot(h.taps[n].i,h.taps[n].q)/range,colors[n%colors.length]);
  const mid=points.length>>1,colour=colors[mid%colors.length];
  plot(prompt,h=>125-100*h.taps[mid].i/range,colour);
  for(const svg of [time,prompt])for(let n=0;n<=4;n++){
    const seconds=end-(4-n)*trackingWindowSeconds/4;
    if(seconds>=0)element(svg,'text',{x:45+145*n,y:240,'text-anchor':n===4?'end':'start',fill:'currentColor','font-size':10},seconds.toFixed(2));
  }
  element(scatter,'path',{d:'M220 125H420 M320 25V225',stroke:'#64748b',fill:'none'});
  for(const [x,y,label] of [[424,129,'I'],[324,21,'Q'],[220,239,(-range).toPrecision(3)],[396,239,range.toPrecision(3)]])
    element(scatter,'text',{x,y,fill:'currentColor','font-size':11},label);
  for(const h of recent){
    const p=h.taps[mid];
    if(Math.abs(p.i)<=range && Math.abs(p.q)<=range)
      element(scatter,'circle',{cx:320+100*p.i/range,cy:125-100*p.q/range,r:2.5,
        fill:colour,opacity:h===recent.at(-1)?1:0.45});
  }
}
