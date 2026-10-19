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
const SVG_NS = 'http://www.w3.org/2000/svg';
const CAF_COLOUR_STOPS = [
  [-40, [36, 57, 87]], [-30, [24, 97, 145]], [-20, [22, 156, 172]],
  [-10, [91, 213, 168]], [-5, [234, 210, 99]], [0, [255, 119, 74]]
];

function svgElement(name, attributes = {}, textContent = '') {
  const element = document.createElementNS(SVG_NS, name);
  for (const [attribute, value] of Object.entries(attributes)) element.setAttribute(attribute, String(value));
  if (textContent) element.textContent = textContent;
  return element;
}

function searchSpacePlot({title, xLabel, yLabel, xMin, xMax, yMin, yMax, xBins, yBins, points = []}) {
  const svg = svgElement('svg', {viewBox: '0 0 760 280', role: 'img', 'aria-label': title});
  const left = 67, right = 735, top = 18, bottom = 228;
  const width = right - left, height = bottom - top;
  const xAt = value => left + (value - xMin) / (xMax - xMin || 1) * width;
  const yAt = value => bottom - (value - yMin) / (yMax - yMin || 1) * height;
  const boundedGrid = count => Math.max(1, Math.min(count, 40));
  svg.append(svgElement('title', {}, title));
  svg.append(svgElement('desc', {}, 'Configured search grid; candidate markers show receiver estimates, not per-cell correlation power.'));
  for (let index = 0, count = boundedGrid(xBins); index <= count; ++index) {
    const x = left + width * index / count;
    svg.append(svgElement('line', {x1: x, y1: top, x2: x, y2: bottom, class: 'search-space-gridline'}));
  }
  for (let index = 0, count = boundedGrid(yBins); index <= count; ++index) {
    const y = top + height * index / count;
    svg.append(svgElement('line', {x1: left, y1: y, x2: right, y2: y, class: 'search-space-gridline'}));
  }
  svg.append(svgElement('rect', {x: left, y: top, width, height, class: 'search-space-frame'}));
  svg.append(svgElement('text', {x: left, y: 255, class: 'search-space-axis-label'}, xLabel));
  svg.append(svgElement('text', {
    x: 17, y: (top + bottom) / 2, class: 'search-space-axis-label',
    transform: `rotate(-90 17 ${(top + bottom) / 2})`
  }, yLabel));
  for (let index = 0; index <= 4; ++index) {
    const fraction = index / 4;
    const xValue = xMin + (xMax - xMin) * fraction;
    const yValue = yMax - (yMax - yMin) * fraction;
    svg.append(svgElement('text', {
      x: left + width * fraction, y: 245, class: 'search-space-tick', 'text-anchor': 'middle'
    }, Number(xValue.toPrecision(4)).toString()));
    svg.append(svgElement('text', {
      x: left - 8, y: top + height * fraction + 3, class: 'search-space-tick', 'text-anchor': 'end'
    }, Number(yValue.toPrecision(4)).toString()));
  }
  for (const point of points) {
    if (!Number.isFinite(point.x) || !Number.isFinite(point.y) ||
        point.x < xMin || point.x > xMax || point.y < yMin || point.y > yMax) continue;
    const marker = svgElement('circle', {
      cx: xAt(point.x), cy: yAt(point.y), r: 5,
      class: point.acquired ? 'search-space-point acquired' : 'search-space-point'
    });
    marker.append(svgElement('title', {},
      `GPS ${point.prn} · ${point.phase} · Doppler ${point.y.toFixed(1)} Hz · code phase ${point.x.toFixed(2)} chips`));
    svg.append(marker);
  }
  return svg;
}

function positiveConfigNumber(profile, key, fallback) {
  const value = Number(profile[key]);
  return Number.isFinite(value) && value > 0 ? value : fallback;
}

function renderSearchSpaces(coarse, refinement, summary, profile, acquisition, satellites, sampleRate,
                            coarseEnabled = true, refinementEnabled = true) {
  if (!coarse || !refinement || !summary) return;
  const dopplerMax = Math.max(0, Number(profile['acquisition.doppler_max_hz']) || 0);
  const dopplerStep = positiveConfigNumber(profile, 'acquisition.doppler_step_hz', 1);
  const fftSize = positiveConfigNumber(
    {fftSize: acquisition?.fft_size}, 'fftSize',
    positiveConfigNumber(profile, 'acquisition.fft_size', 0)
  );
  const codeStep = positiveConfigNumber(profile, 'acquisition.code_step_chips', 1);
  const dopplerBins = Math.floor(2 * dopplerMax / dopplerStep + 0.5) + 1;
  const delayOffsets = fftSize ? Math.max(1, Math.ceil((1023 / fftSize) / codeStep)) : 1;
  const codeBins = fftSize * delayOffsets;
  const points = (coarseEnabled ? satellites : []).filter(satellite =>
    Number.isFinite(satellite.doppler_hz) && Number.isFinite(satellite.code_phase_chips) &&
    satellite.code_phase_chips >= 0 && satellite.code_phase_chips <= 1023
  ).map(satellite => ({
    x: satellite.code_phase_chips, y: satellite.doppler_hz,
    prn: satellite.prn, phase: satellite.phase.replaceAll('_', ' '),
    acquired: satellite.phase === 'acquired'
  }));
  if (coarseEnabled) coarse.replaceChildren(searchSpacePlot({
    title: 'GPS coarse search space: code phase by Doppler',
    xLabel: 'Code phase (chips)', yLabel: 'Doppler (Hz)',
    xMin: 0, xMax: 1023, yMin: -dopplerMax || -1, yMax: dopplerMax || 1,
    xBins: codeBins, yBins: dopplerBins, points
  }));

  const refinementDopplerSpan = Math.max(0, Number(profile['acquisition.refinement_doppler_span_hz']) || 0);
  const refinementDopplerStep = positiveConfigNumber(profile, 'acquisition.refinement_doppler_step_hz', 1);
  const refinementCodeSpan = Math.max(0, Number(profile['acquisition.refinement_code_span_chips']) || 0);
  const refinementCodeStepSamples = positiveConfigNumber(profile, 'acquisition.refinement_code_step_samples', 1);
  const rate = Number(sampleRate);
  const codeSpanSamples = Number.isFinite(rate) && rate > 0
    ? Math.floor(refinementCodeSpan * rate / 1023000 + 0.5) : 0;
  const refinementCodeBins = Math.floor(2 * codeSpanSamples / refinementCodeStepSamples + 0.5) + 1;
  const refinementDopplerBins = Math.floor(2 * refinementDopplerSpan / refinementDopplerStep + 0.5) + 1;
  if (refinementEnabled) refinement.replaceChildren(searchSpacePlot({
    title: 'GPS refinement search space around each coarse candidate',
    xLabel: 'Doppler offset from coarse candidate (Hz)',
    yLabel: 'Code phase offset from coarse candidate (chips)',
    xMin: -refinementDopplerSpan || -1, xMax: refinementDopplerSpan || 1,
    yMin: -refinementCodeSpan || -1, yMax: refinementCodeSpan || 1,
    xBins: refinementDopplerBins, yBins: refinementCodeBins
  }));
  const gridDescription = fftSize
    ? `${dopplerBins.toLocaleString()} Doppler bins × ${codeBins.toLocaleString()} coarse code cells per PRN; FFT ${acquisition?.fft_size || fftSize} points.`
    : 'Waiting for the saved GPS acquisition profile.';
  const candidateDescription = points.length
    ? ` ${points.length} candidate ${points.length === 1 ? 'position' : 'positions'} from receiver telemetry are marked on the coarse map.`
    : ' No receiver candidate positions yet.';
  const refinementDescription = `${refinementDopplerBins.toLocaleString()} Doppler refinement bins × ${refinementCodeBins.toLocaleString()} code offsets per candidate.`;
  summary.textContent = `${gridDescription} ${refinementDescription}${candidateDescription} These diagrams show configured geometry; measured correlation power appears in the cross ambiguity view above.`;
}

/** Validate the bounded, measured coarse-search grid before any plotting. */
function parseCaf(raw) {
  if (raw?.available !== true) return null;
  const columns = Number(raw.code_bins), rows = Number(raw.doppler_bins);
  const code = raw.code_delay_samples, doppler = raw.doppler_hz, power = raw.power;
  const completed = Number(raw.completed_doppler_bins), total = Number(raw.total_doppler_bins);
  if (!Number.isInteger(columns) || !Number.isInteger(rows) || columns < 1 || rows < 1 ||
      columns > 64 || rows > 64 || !Array.isArray(code) || !Array.isArray(doppler) ||
      !Array.isArray(power) || code.length !== columns || doppler.length !== rows ||
      power.length !== columns * rows || !Number.isInteger(raw.prn) || raw.prn < 1 || raw.prn > 32 ||
      !Number.isInteger(completed) || !Number.isInteger(total) || completed < 0 || completed > total || total < 1) return null;
  if (code.some((value, index) => !Number.isFinite(value) || index && value <= code[index - 1]) ||
      doppler.some((value, index) => !Number.isFinite(value) || index && value <= doppler[index - 1])) return null;
  const cells = power.map(value => value === null ? null : Number(value));
  if (cells.some(value => value !== null && (!Number.isFinite(value) || value < 0))) return null;
  const peak = Math.max(0, ...cells.filter(value => value !== null));
  return {
    prn: raw.prn, sequence: String(raw.sequence ?? ''), epoch: String(raw.epoch ?? ''),
    referenceSample: String(raw.reference_sample ?? ''), columns, rows, code, doppler,
    power: cells, peak, completed, total, complete: raw.complete === true,
    sums: Number(raw.noncoherent_sums) || 0, fftSize: Number(raw.fft_size) || 0,
    source: raw.source, aggregation: raw.aggregation
  };
}

function cafAxisEdges(values) {
  if (values.length === 1) return [values[0] - 0.5, values[0] + 0.5];
  const edges = [values[0] - (values[1] - values[0]) / 2];
  for (let index = 1; index < values.length; ++index) edges.push((values[index - 1] + values[index]) / 2);
  edges.push(values.at(-1) + (values.at(-1) - values.at(-2)) / 2);
  return edges;
}

function cafNumber(value) {
  const absolute = Math.abs(value);
  return Number(value.toPrecision(absolute >= 1000 ? 5 : absolute >= 10 ? 4 : 3)).toString();
}

/** Colour is relative to the strongest measured cell, never a detection probability. */
function cafColour(power, peak) {
  const level = peak > 0 && power > 0 ? Math.max(-40, Math.min(0, 10 * Math.log10(power / peak))) : -40;
  for (let index = 1; index < CAF_COLOUR_STOPS.length; ++index) {
    if (level > CAF_COLOUR_STOPS[index][0] && index !== CAF_COLOUR_STOPS.length - 1) continue;
    const [lo, a] = CAF_COLOUR_STOPS[index - 1], [hi, b] = CAF_COLOUR_STOPS[index];
    const fraction = Math.max(0, Math.min(1, (level - lo) / (hi - lo)));
    return `rgb(${a.map((channel, part) => Math.round(channel + (b[part] - channel) * fraction)).join(',')})`;
  }
  return 'rgb(255,119,74)';
}

function cafCanvas(canvas) {
  const width = Math.floor(canvas.clientWidth), height = Math.floor(canvas.clientHeight);
  if (width < 100 || height < 100) return null;
  const ratio = Math.min(window.devicePixelRatio || 1, 2);
  const pixelsWide = Math.round(width * ratio), pixelsHigh = Math.round(height * ratio);
  if (canvas.width !== pixelsWide || canvas.height !== pixelsHigh) {
    canvas.width = pixelsWide; canvas.height = pixelsHigh;
  }
  const ctx = canvas.getContext('2d');
  if (!ctx) return null;
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#0c1a2b'; ctx.fillRect(0, 0, width, height);
  ctx.font = '11px system-ui, sans-serif';
  return {ctx, width, height};
}

function cafPlot2d(surface, data) {
  const {ctx, width, height} = surface;
  const left = width < 500 ? 53 : 73, right = width < 500 ? 64 : 76, top = 25, bottom = 52;
  const plotWidth = width - left - right, plotHeight = height - top - bottom;
  if (plotWidth < 40 || plotHeight < 40) return null;
  const codeEdges = cafAxisEdges(data.code), dopplerEdges = cafAxisEdges(data.doppler);
  const xAt = value => left + (value - codeEdges[0]) / (codeEdges.at(-1) - codeEdges[0]) * plotWidth;
  const yAt = value => top + (dopplerEdges.at(-1) - value) / (dopplerEdges.at(-1) - dopplerEdges[0]) * plotHeight;
  ctx.fillStyle = '#152337'; ctx.fillRect(left, top, plotWidth, plotHeight);
  const xBounds = codeEdges.map(xAt), yBounds = dopplerEdges.map(yAt);
  for (let row = 0; row < data.rows; ++row) for (let column = 0; column < data.columns; ++column) {
    const value = data.power[row * data.columns + column];
    const x = xBounds[column], y = yBounds[row + 1];
    const cellWidth = xBounds[column + 1] - x, cellHeight = yBounds[row] - y;
    if (value === null) continue;
    ctx.fillStyle = cafColour(value, data.peak);
    ctx.fillRect(x, y, Math.ceil(cellWidth + 0.4), Math.ceil(cellHeight + 0.4));
  }
  ctx.strokeStyle = '#7190a8'; ctx.lineWidth = 1;
  ctx.strokeRect(left + 0.5, top + 0.5, plotWidth, plotHeight);
  ctx.fillStyle = '#abc3d2'; ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  for (const fraction of [0, 0.5, 1]) {
    const value = codeEdges[0] + (codeEdges.at(-1) - codeEdges[0]) * fraction;
    ctx.fillText(cafNumber(value), left + plotWidth * fraction, top + plotHeight + 6);
  }
  ctx.fillText('Code delay (samples)', left + plotWidth / 2, height - 20);
  ctx.textAlign = 'right'; ctx.textBaseline = 'middle';
  for (const fraction of [0, 0.5, 1]) {
    const value = dopplerEdges.at(-1) - (dopplerEdges.at(-1) - dopplerEdges[0]) * fraction;
    ctx.fillText(cafNumber(value), left - 7, top + plotHeight * fraction);
  }
  ctx.save(); ctx.translate(12, top + plotHeight / 2); ctx.rotate(-Math.PI / 2);
  ctx.textAlign = 'center'; ctx.fillText('Doppler (Hz)', 0, 0); ctx.restore();
  const legendX = left + plotWidth + 18, legendY = top + 13, legendHeight = Math.max(50, plotHeight - 32);
  for (let pixel = 0; pixel < legendHeight; ++pixel) {
    const db = -40 * pixel / Math.max(1, legendHeight - 1);
    ctx.fillStyle = cafColour(Math.pow(10, db / 10), 1);
    ctx.fillRect(legendX, legendY + pixel, 11, 1);
  }
  ctx.textAlign = 'left'; ctx.textBaseline = 'middle'; ctx.fillStyle = '#a8c0cf';
  ctx.fillText('0', legendX + 15, legendY + 3);
  ctx.fillText('-40', legendX + 15, legendY + legendHeight - 1);
  ctx.save(); ctx.translate(legendX + 9, legendY + legendHeight / 2); ctx.rotate(-Math.PI / 2);
  ctx.textAlign = 'center'; ctx.fillText('dB rel. peak', 0, 0); ctx.restore();
  return {xBounds, yBounds, left, top, plotWidth, plotHeight};
}

function cafPlot3d(surface, data) {
  const {ctx, width, height} = surface;
  const compact = width < 500;
  const originX = compact ? 39 : 78, originY = height - (compact ? 54 : 65);
  const codeWidth = width * (compact ? 0.57 : 0.60), dopplerWidth = width * (compact ? 0.18 : 0.19);
  const dopplerRise = height * 0.24, powerRise = height * 0.43;
  const xyz = (column, row, power) => {
    const x = data.columns > 1 ? column / (data.columns - 1) : 0.5;
    const y = data.rows > 1 ? row / (data.rows - 1) : 0.5;
    const db = data.peak > 0 && power > 0 ? 10 * Math.log10(power / data.peak) : -40;
    const z = Math.max(0, Math.min(1, (db + 40) / 40));
    return [originX + codeWidth * x + dopplerWidth * y,
      originY - dopplerRise * y - powerRise * z];
  };
  const base = (column, row) => xyz(column, row, 0);
  ctx.strokeStyle = '#405d74'; ctx.lineWidth = 1;
  for (const [a, b] of [[base(0, 0), base(data.columns - 1, 0)],
    [base(0, 0), base(0, data.rows - 1)],
    [base(data.columns - 1, 0), base(data.columns - 1, data.rows - 1)],
    [base(0, data.rows - 1), base(data.columns - 1, data.rows - 1)]]) {
    ctx.beginPath(); ctx.moveTo(...a); ctx.lineTo(...b); ctx.stroke();
  }
  const vertical = xyz(0, 0, data.peak);
  ctx.beginPath(); ctx.moveTo(...base(0, 0)); ctx.lineTo(...vertical); ctx.stroke();
  /* Filled faces connect only four actually measured neighbouring bin centres. */
  for (let row = data.rows - 2; row >= 0; --row) for (let column = 0; column < data.columns - 1; ++column) {
    const values = [data.power[(row + 1) * data.columns + column],
      data.power[(row + 1) * data.columns + column + 1],
      data.power[row * data.columns + column + 1], data.power[row * data.columns + column]];
    if (values.some(value => value === null)) continue;
    const corners = [xyz(column, row + 1, values[0]), xyz(column + 1, row + 1, values[1]),
      xyz(column + 1, row, values[2]), xyz(column, row, values[3])];
    ctx.beginPath(); ctx.moveTo(...corners[0]);
    for (let index = 1; index < corners.length; ++index) ctx.lineTo(...corners[index]);
    ctx.closePath(); ctx.fillStyle = cafColour((values[0] + values[1] + values[2] + values[3]) / 4, data.peak);
    ctx.fill(); ctx.strokeStyle = '#b5dbec22'; ctx.stroke();
  }
  /* A single measured Doppler row still appears before any surface face exists. */
  for (let row = data.rows - 1; row >= 0; --row) {
    ctx.beginPath(); let started = false;
    for (let column = 0; column < data.columns; ++column) {
      const value = data.power[row * data.columns + column];
      if (value === null) { started = false; continue; }
      const point = xyz(column, row, value);
      if (started) ctx.lineTo(...point); else ctx.moveTo(...point);
      started = true;
    }
    ctx.strokeStyle = '#d6eff288'; ctx.stroke();
  }
  let peakIndex = -1;
  if (data.peak > 0) peakIndex = data.power.findIndex(value => value === data.peak);
  if (peakIndex >= 0) {
    const row = Math.floor(peakIndex / data.columns), column = peakIndex % data.columns;
    const point = xyz(column, row, data.peak);
    ctx.beginPath(); ctx.arc(...point, 4, 0, Math.PI * 2);
    ctx.fillStyle = '#fff3b4'; ctx.fill();
  }
  ctx.fillStyle = '#abc3d2'; ctx.font = compact ? '9px system-ui, sans-serif' : '11px system-ui, sans-serif';
  ctx.textAlign = 'center'; ctx.textBaseline = 'top';
  ctx.fillText('Code delay (samples)', originX + codeWidth / 2, originY + 24);
  ctx.fillText(cafNumber(data.code[0]), originX, originY + 5);
  ctx.fillText(cafNumber(data.code.at(-1)), originX + codeWidth, originY + 5);
  ctx.save(); ctx.translate(originX + dopplerWidth / 2 - 20, originY - dopplerRise / 2 - 4);
  ctx.rotate(-Math.atan2(dopplerRise, dopplerWidth));
  ctx.fillText('Doppler (Hz)', 0, 0); ctx.restore();
  const far = base(0, data.rows - 1);
  ctx.fillText(cafNumber(data.doppler[0]), originX - 7, originY - 12);
  ctx.fillText(cafNumber(data.doppler.at(-1)), far[0] + 8, far[1] - 15);
  ctx.textAlign = 'left'; ctx.textBaseline = 'middle';
  ctx.fillText('0 dB', vertical[0] + 6, vertical[1]);
  ctx.fillText('-40 dB', originX + 6, originY - 11);
  ctx.save(); ctx.translate(originX - 29, originY - powerRise / 2);
  ctx.rotate(-Math.PI / 2); ctx.textAlign = 'center';
  ctx.fillText('Power relative to peak', 0, 0); ctx.restore();
}

function drawCaf(canvas, data, mode) {
  const surface = cafCanvas(canvas);
  if (!surface || !data) return null;
  return mode === '3d' ? (cafPlot3d(surface, data), null) : cafPlot2d(surface, data);
}

function journeyMarkup() {
  return heading('RECEIVER WORKSPACE', 'One path from I/Q to a GNSS solution',
    'Configure the source, inspect the signal and follow each receiver stage. Live information comes from the connected receiver; stages without an integrated module remain empty.',
    `${button('setup', 'Configure receiver', true)}<button type="button" data-tour="start">Start guided tour</button>${button('rf', 'Open RF observation')}`) +
    `<nav class="workspace-pipeline" aria-label="Receiver stages">${STAGES.map(([view, title, description], index) => `<button type="button" class="workspace-step" data-go="${view}"><span class="workspace-step-number">${String(index + 1).padStart(2, '0')}</span><h3>${title}</h3><p>${description}</p><span class="workspace-step-state" data-workspace-field="stage-${view}">${['setup', 'rf', 'record'].includes(view) ? 'Waiting for receiver' : 'Module unavailable'}</span></button>`).join('')}</nav>` +
    `<div class="workspace-kpis">${kpi('INPUT SOURCE', field('sourceName'), field('formatDetail'))}${kpi('PROCESSED I/Q PAIRS', field('processed'), field('windowsDetail'))}${kpi('RECEIVER SERVICE', field('receiverState'), field('connectionDetail'))}${kpi('NATIVE I/Q CAPTURE', field('recordingState'), field('capturePoint'))}</div>` +
    `<div class="workspace-grid workspace-grid-wide">${card('Your next step', 'Operate the real signal path before inspecting GNSS stages.',
      `<div class="workspace-status-strip">${badge('I/Q pipeline available')}${badge('GPS acquisition available')}</div><ol class="workspace-next-steps"><li><strong>Prepare the source.</strong> Select a live SDR or a recorded I/Q file, then configure GPS acquisition and apply the full profile.</li><li><strong>Start reception.</strong> Use Start receiver, then check continuity and native signal statistics in RF observation.</li><li><strong>Inspect the chain.</strong> Follow GPS search and confirmation in Acquisition. Tracking, navigation decoding and PVT await their receiver modules.</li></ol><div class="workspace-actions">${button('setup', 'Review setup')}${button('health', 'Check system health')}</div>`)}${card('Session at a glance', 'Configured input and the current receiver connection.', rows([
      ['Connection', field('connection')], ['Profile', field('profileState')], ['Native sample format', field('format')], ['Sample rate', field('sampleRate')], ['Signal centre', field('signalFrequency')], ['Position solution', 'Unavailable — PVT module not integrated']
    ]))}</div>`;
}

function acquisitionMarkup() {
  return heading('STAGE 03 / ACQUISITION', 'Find and confirm satellite signals',
    'Inspect GPS L1 C/A coarse search, Doppler and code refinement, and optional confirmation on the native I/Q stream.',
    `${settingsButton('acquisition', 'Acquisition settings')}${button('rf', 'Check RF input')}${button('tracking', 'Open tracking →')}`) +
    `<div class="workspace-kpis">${kpi('SEARCH STATE', field('acqState'), field('acqDetail'))}${kpi('ATTEMPTED GPS PRNS', field('acqSearched'), 'Current receiver run')}${kpi('ACQUIRED SIGNALS', field('acqConfirmed'), 'Acquired · ready for tracking')}${kpi('COARSE FFT', field('acqGrid'), 'NGene compression · 1 ms coherent periods')}</div>` +
    card('Cross ambiguity function', 'Measured GPS L1 C/A correlation power across code phase and Doppler.',
      `<div class="caf-controls"><div class="caf-mode" role="group" aria-label="Cross ambiguity view"><button type="button" data-caf-mode="2d" aria-pressed="true">2D heatmap</button><button type="button" data-caf-mode="3d" aria-pressed="false">3D view</button></div><label class="caf-prn-picker" for="cafPrnSelect">GPS PRN <select id="cafPrnSelect"><option value="">Latest measured</option></select></label></div><div id="cafPlotWrap" class="caf-plot-wrap"><canvas id="cafPlot" role="img" aria-label="No measured cross ambiguity data available"></canvas><div id="cafEmpty" class="caf-empty">No measured cross ambiguity data yet. Start GPS acquisition to inspect a search.</div></div><p id="cafDetails" class="caf-details" role="status" aria-live="polite">Only correlation values computed by the receiver are shown.</p><p id="cafCursor" class="caf-cursor" aria-live="off"></p>`) +
    card('GPS search spaces', 'Configured Doppler/code-phase coverage and the refinement neighborhood around each coarse peak.',
      `<div class="search-space-grid"><section class="search-space-panel"><h3>Coarse search · full code period</h3><div id="coarseSearchSpace" class="search-space-plot"></div><p>Markers show candidate estimates; measured coarse correlation power is plotted above.</p></section><section class="search-space-panel"><h3>Refinement · local offsets</h3><div id="refinementSearchSpace" class="search-space-plot"></div><p>Grid bounds and spacing are configured values around the coarse candidate.</p></section></div><p id="searchSpaceSummary" class="search-space-summary" role="status" aria-live="polite">Waiting for the receiver profile.</p>`) +
    card('GPS acquisition results', 'Delay and code phase refer to the reported sample epoch. Acquisition does not establish tracking or a position fix. Confirmation is optional.',
      `<div class="table-wrap"><table><thead><tr>${['GPS PRN','Stage','Doppler (Hz)','Delay (samples)','Metric / threshold','Confirmation','Epoch / reference sample'].map(title=>`<th scope="col">${title}</th>`).join('')}</tr></thead><tbody id="acquisitionResults"><tr><td colspan="7" class="empty-table">Enable GPS acquisition in Receiver configuration, then start the receiver.</td></tr></tbody></table></div>`) +
    card('Search control', 'Acquisition starts with the receiver when enabled in its profile.',
      `<div class="workspace-actions">${settingsButton('receiver', 'GPS channels & PRNs')}${settingsButton('acquisition', 'Search and confirmation settings')}</div><p class="muted">Galileo acquisition and tracking are not integrated. Acquired GPS results remain available for the future tracking module. RF display enables control spectrum diagnostics independently of GPS acquisition.</p>`) + route('rf', 'tracking');
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
        ['Native I/Q input and conditioning', 'module-input'], ['Spectrum / waterfall / I/Q statistics', 'module-analysis'], ['Native I/Q recording and replay', 'module-recording'], ['GPS L1 C/A acquisition', 'module-acquisition'], ['Channel tracking and navigation decoding', 'module-gnss'], ['PVT / integrity / authentication', 'module-gnss']
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
  return result === null ? UNAVAILABLE : `${(result / scale).toFixed(6)} ${suffix}`;
}

/**
 * Add receiver workspace views without owning controller state or running signal work.
 * The application supplies snapshots; unavailable GNSS modules have no synthetic data.
 */
export function createReceiverWorkspace({onViewChange = () => {}, onOpenSettings = () => {}, onBrowseReplay = () => {}, onFetchCaf = async () => ({available:false}), onPlotInterestChange = () => {}} = {}) {
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
  const acquisitionRows = document.getElementById('acquisitionResults');
  const coarseSearchSpace = document.getElementById('coarseSearchSpace');
  const refinementSearchSpace = document.getElementById('refinementSearchSpace');
  const searchSpaceSummary = document.getElementById('searchSpaceSummary');
  const cafCanvasNode = document.getElementById('cafPlot');
  const cafWrap = document.getElementById('cafPlotWrap');
  const cafEmpty = document.getElementById('cafEmpty');
  const cafDetails = document.getElementById('cafDetails');
  const cafCursor = document.getElementById('cafCursor');
  const cafPrnSelect = document.getElementById('cafPrnSelect');
  const cafModeButtons = [...document.querySelectorAll('[data-caf-mode]')];
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
  const cafCard = cafWrap?.closest('.workspace-card');
  const cafToggle = plotSwitch('showCaf', cafCard?.querySelector('.panel-heading'), 'cross ambiguity function');
  const cafOff = document.createElement('p');
  cafOff.className = 'plot-disabled muted hidden'; cafOff.textContent = 'Cross ambiguity plot disabled; acquisition continues.';
  cafCard?.append(cafOff);
  const coarsePanel = coarseSearchSpace?.closest('.search-space-panel');
  const refinementPanel = refinementSearchSpace?.closest('.search-space-panel');
  const coarseToggle = plotSwitch('showCoarseSearch', coarsePanel?.querySelector('h3'), 'coarse search plot');
  const refinementToggle = plotSwitch('showRefinementSearch', refinementPanel?.querySelector('h3'), 'refinement search plot');
  function toggleSearchPanel(panel, plot, enabled) {
    if (!panel || !plot) return;
    plot.classList.toggle('hidden', !enabled);
    panel.querySelector('p')?.classList.toggle('hidden', !enabled);
    let off = panel.querySelector('.plot-disabled');
    if (!off) {
      off = document.createElement('p'); off.className = 'plot-disabled muted';
      off.textContent = 'Plot disabled.'; panel.append(off);
    }
    off.classList.toggle('hidden', enabled);
    if (!enabled) plot.replaceChildren();
  }
  function syncPlotSwitches() {
    const enabled = cafToggle.checked;
    for (const node of [cafCard?.querySelector('.caf-controls'), cafWrap, cafDetails, cafCursor])
      node?.classList.toggle('hidden', !enabled);
    cafOff.classList.toggle('hidden', enabled);
    toggleSearchPanel(coarsePanel, coarseSearchSpace, coarseToggle.checked);
    toggleSearchPanel(refinementPanel, refinementSearchSpace, refinementToggle.checked);
    if (!coarseToggle.checked && !refinementToggle.checked && searchSpaceSummary)
      searchSpaceSummary.textContent = 'Search-space plots disabled.';
  }
  cafToggle.addEventListener('change', () => {
    syncPlotSwitches();
    clearCaf(cafToggle.checked ?
      'Waiting for measured GPS search cells. Enable this plot before or during a new search; past cells cannot be reconstructed.' :
      'Cross ambiguity plot disabled.');
    cafFetchedStopped = false;
    onPlotInterestChange();
    if (cafToggle.checked) maybePollCaf(true);
  });
  for (const toggle of [coarseToggle, refinementToggle]) toggle.addEventListener('change', () => {
    searchSpaceSignature = '';
    syncPlotSwitches();
    update(lastSnapshot);
  });
  const acquisitionDot = document.querySelector('[data-view="acquisition"] .workspace-nav-dot');
  let acquisitionSignature = '';
  let searchSpaceSignature = '';
  let caf = null, cafMode = '2d', cafGeometry = null, cafRun = null, cafSelection = '';
  let cafPollAt = 0, cafPending = false, cafGeneration = 0, cafFetchedStopped = false, cafLiveState = 'stopped';
  let cafCanFetch = false, cafStatusText = 'No measured cross ambiguity data yet. Start GPS acquisition to inspect a search.';
  let lastSnapshot = {};
  const panels = Object.entries(ROUTES).map(([name, info]) => [name, document.getElementById(info.id)]);
  const nav = [...document.querySelectorAll('[data-view]')];
  const crumb = document.getElementById('workspaceCrumb');
  const tourCounter = tour.querySelector('[data-tour-counter]'), tourTitle = tour.querySelector('[data-tour-title]'), tourDescription = tour.querySelector('[data-tour-description]');
  const previous = tour.querySelector('[data-tour="previous"]'), next = tour.querySelector('[data-tour="next"]');
  let selected = 'rf', guided = false, tourIndex = 0;
  syncPlotSwitches();
  const text = (key, value) => {
    const string = String(value ?? UNAVAILABLE);
    for (const node of bindings.get(key) || []) if (node.textContent !== string) node.textContent = string;
  };
  function paintCaf() {
    if (!cafToggle.checked || !cafCanvasNode || !cafEmpty || !cafDetails) return;
    cafEmpty.hidden = Boolean(caf);
    if (!caf) {
      cafEmpty.textContent = cafStatusText;
      cafDetails.textContent = 'The plot shows measured coarse-search correlation power only when the receiver publishes it.';
      cafCanvasNode.setAttribute('aria-label', 'No measured cross ambiguity data available');
      cafGeometry = null;
      if (selected === 'acquisition') cafCanvas(cafCanvasNode);
      if (cafCursor) cafCursor.textContent = '';
      return;
    }
    const cells = caf.power.reduce((count, value) => count + (value !== null), 0);
    const progress = caf.complete ? 'Complete sweep' : `Partial sweep: ${caf.completed} of ${caf.total} Doppler bins computed`;
    const peakIndex = caf.peak > 0 ? caf.power.findIndex(value => value === caf.peak) : -1;
    const peakPosition = peakIndex < 0 ? 'No positive measured peak' :
      `Strongest measured bin: ${cafNumber(caf.code[peakIndex % caf.columns])} samples, ` +
      `${cafNumber(caf.doppler[Math.floor(peakIndex / caf.columns)])} Hz, raw power ${caf.peak.toPrecision(5)}`;
    cafCanvasNode.setAttribute('aria-label',
      `GPS ${caf.prn} ${cafMode.toUpperCase()} cross ambiguity function; ${progress.toLowerCase()}; ${cells} measured display cells`);
    cafDetails.textContent = `GPS ${caf.prn} · ${progress} · ${cells} measured ${caf.columns} × ${caf.rows} display cells · ` +
      `${caf.sums} noncoherent 1 ms ${caf.sums === 1 ? 'period' : 'periods'} · ` +
      `FFT ${caf.fftSize} points · ${caf.aggregation === 'max' ? 'maximum power per display cell' : 'receiver-provided cell power'} · ` +
      `${peakPosition}. Colour/height is relative to the strongest displayed bin (−40 to 0 dB). ` +
      `Epoch ${caf.epoch}, reference sample ${caf.referenceSample}. ` +
      `This plot does not show detector threshold or acquisition confirmation.`;
    cafGeometry = selected === 'acquisition' ? drawCaf(cafCanvasNode, caf, cafMode) : null;
  }
  function clearCaf(message, invalidate = true) {
    if (invalidate) ++cafGeneration;
    caf = null; cafStatusText = message;
    paintCaf();
  }
  function populateCafPrns(satellites) {
    if (!cafPrnSelect) return;
    const prns = [...new Set(satellites.map(satellite => Number(satellite.prn))
      .filter(prn => Number.isInteger(prn) && prn >= 1 && prn <= 32))].sort((a, b) => a - b);
    const signature = prns.join(',');
    if (cafPrnSelect.dataset.prns === signature) return;
    cafPrnSelect.dataset.prns = signature;
    const options = [new Option('Latest measured', '')];
    for (const prn of prns) options.push(new Option(`GPS ${prn}`, String(prn)));
    cafPrnSelect.replaceChildren(...options);
    if (cafSelection && !prns.includes(Number(cafSelection))) {
      cafSelection = '';
      clearCaf('Waiting for a measured GPS search.');
    }
    cafPrnSelect.value = cafSelection;
  }
  function maybePollCaf(force = false) {
    if (!cafToggle.checked || selected !== 'acquisition' || !cafCanFetch || cafPending) return;
    const now = performance.now();
    if (!force && (now - cafPollAt < 850 || cafFetchedStopped)) return;
    cafPollAt = now;
    if (cafLiveState !== 'running') cafFetchedStopped = true;
    cafPending = true;
    const generation = cafGeneration, run = cafRun, choice = cafSelection;
    Promise.resolve().then(() => onFetchCaf(choice ? Number(choice) : undefined)).then(raw => {
      if (generation !== cafGeneration || run !== cafRun || choice !== cafSelection) return;
      if (raw?.available === true) {
        const next = parseCaf(raw);
        if (!next) { clearCaf('The receiver returned an invalid cross ambiguity grid.', false); return; }
        if (!caf || next.prn !== caf.prn || next.sequence !== caf.sequence || next.epoch !== caf.epoch ||
            next.completed !== caf.completed || next.complete !== caf.complete) {
          caf = next; paintCaf();
        }
      } else if (caf) {
        clearCaf(choice ? `No measured CAF is available for GPS ${choice}.` : 'Waiting for a measured GPS search.', false);
      } else {
        cafStatusText = choice ? `No measured CAF is available for GPS ${choice}.` : 'Waiting for a measured GPS search.';
        paintCaf();
      }
    }).catch(error => {
      if (generation !== cafGeneration || run !== cafRun || choice !== cafSelection) return;
      if (!caf) { cafStatusText = `Cross ambiguity data unavailable: ${error.message}`; paintCaf(); }
    }).finally(() => {
      cafPending = false;
      if (generation !== cafGeneration && selected === 'acquisition') maybePollCaf(true);
    });
  }
  cafPrnSelect?.addEventListener('change', () => {
    cafSelection = cafPrnSelect.value;
    clearCaf(cafSelection ? `Waiting for measured CAF cells for GPS ${cafSelection}.` : 'Waiting for the latest measured GPS search.');
    cafFetchedStopped = false;
    maybePollCaf(true);
  });
  for (const modeButton of cafModeButtons) modeButton.addEventListener('click', () => {
    const mode = modeButton.dataset.cafMode;
    if (!['2d', '3d'].includes(mode) || mode === cafMode) return;
    cafMode = mode;
    for (const button of cafModeButtons) button.setAttribute('aria-pressed', String(button.dataset.cafMode === mode));
    if (cafCursor) cafCursor.textContent = '';
    paintCaf();
  });
  cafCanvasNode?.addEventListener('pointermove', event => {
    if (cafMode !== '2d' || !caf || !cafGeometry || !cafCursor) return;
    const bounds = cafCanvasNode.getBoundingClientRect();
    const x = event.clientX - bounds.left, y = event.clientY - bounds.top;
    const {xBounds, yBounds, left, top, plotWidth, plotHeight} = cafGeometry;
    if (x < left || x > left + plotWidth || y < top || y > top + plotHeight) { cafCursor.textContent = ''; return; }
    const column = xBounds.findIndex((edge, index) => index < caf.columns && x >= edge && x < xBounds[index + 1]);
    const row = yBounds.findIndex((edge, index) => index < caf.rows && y <= edge && y > yBounds[index + 1]);
    if (row < 0 || column < 0) { cafCursor.textContent = ''; return; }
    const value = caf.power[row * caf.columns + column];
    const power = value === null ? 'not yet measured' : value > 0 && caf.peak > 0
      ? `${(10 * Math.log10(value / caf.peak)).toFixed(1)} dB relative to peak; raw power ${value.toPrecision(5)}`
      : `raw power ${value}`;
    cafCursor.textContent = `GPS ${caf.prn} · code delay ${cafNumber(caf.code[column])} samples · Doppler ${cafNumber(caf.doppler[row])} Hz · ${power}`;
  });
  cafCanvasNode?.addEventListener('pointerleave', () => { if (cafCursor) cafCursor.textContent = ''; });
  if (cafWrap && typeof ResizeObserver !== 'undefined') new ResizeObserver(() => {
    if (selected === 'acquisition') paintCaf();
  }).observe(cafWrap);
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
    if (view === 'acquisition') requestAnimationFrame(() => {
      if (selected === 'acquisition') { paintCaf(); maybePollCaf(true); }
    });
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
    lastSnapshot = snapshot;
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
    const acq = live?.acquisition;
    const satellites = acq?.satellites || [];
    const searchProfile = {...profile, ...draft};
    const confirmed = satellites.filter(satellite=>satellite.phase==='acquired').length;
    const enabled = profile['acquisition.enabled']==='true';
    const runId = live?.run_id ?? null;
    if (cafRun !== runId) {
      cafRun = runId; cafPollAt = 0; cafFetchedStopped = false;
      clearCaf('Waiting for measured GPS search cells in this receiver run.');
    }
    if (cafLiveState === 'running' && live?.state !== 'running') cafFetchedStopped = false;
    cafLiveState = live?.state || 'stopped';
    cafCanFetch = Boolean(connected && capabilities?.gps_acquisition && acq?.enabled &&
      (live?.state === 'running' || live?.state === 'stopped'));
    if (!cafCanFetch && (caf || cafStatusText !== (!connected ? 'Connect to the receiver to inspect measured CAF data.' :
      !enabled ? 'Enable GPS acquisition in Receiver configuration to inspect measured CAF data.' :
      'Waiting for GPS acquisition to start.'))) {
      clearCaf(!connected ? 'Connect to the receiver to inspect measured CAF data.' :
        !enabled ? 'Enable GPS acquisition in Receiver configuration to inspect measured CAF data.' :
        'Waiting for GPS acquisition to start.');
    }
    populateCafPrns(satellites);
    const acquisitionState = !capabilities?.gps_acquisition ? UNAVAILABLE : !connected ? 'Disconnected' :
      live?.state==='running' && profile['receiver.mode']==='record' ? 'Record only' :
      !live || !acq?.enabled ? enabled ? 'Ready' : 'Disabled' : live.state!=='running' ? 'Last run' :
      confirmed>=acq.channel_count ? 'Acquired' : 'Searching';
    if (acquisitionDot) {
      acquisitionDot.classList.toggle('available', !!capabilities?.gps_acquisition);
      acquisitionDot.title = capabilities?.gps_acquisition ? 'GPS L1 C/A acquisition available' : 'GPS acquisition unavailable in this receiver';
    }
    const values = {
      sourceName: source, format, formatDetail: format === UNAVAILABLE ? 'No format available' : `${format} · ${started ? 'receiver run' : 'configured input'}`,
      sampleRate: frequency(rate, 'Msps (nominal)'), signalFrequency: frequency(signal, 'MHz'),
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
      'stage-acquisition': acquisitionState,
      acqState: acquisitionState, acqDetail: acq?.enabled ? `${acq.search_limit ?? acq.channel_count} admitted search ${acq.search_limit===1 ? 'slot' : 'slots'} · current policy ${acq.noncoherent_sums} × 1 ms` : 'Enable GPS acquisition before starting reception',
      acqSearched: acq ? satellites.length : '—', acqConfirmed: acq ? confirmed : '—',
      acqGrid: (acq?.fft_size || profile['acquisition.fft_size']) ? `${acq?.fft_size || profile['acquisition.fft_size']} points` : UNAVAILABLE,
      'stage-record': record
    };
    for (const [key, value] of Object.entries(values)) text(key, value);
    const searchSpaceKey = JSON.stringify([
      selected === 'acquisition', coarseToggle.checked, refinementToggle.checked,
      Object.entries(searchProfile).filter(([key]) => key.startsWith('acquisition.')).sort(([a], [b]) => a.localeCompare(b)),
      acq?.fft_size, live?.actual_sample_rate_hz || searchProfile['frontend.sample_rate_hz'],
      satellites.map(satellite => [satellite.prn, satellite.phase, satellite.code_phase_chips, satellite.doppler_hz])
    ]);
    if (selected === 'acquisition' && (coarseToggle.checked || refinementToggle.checked) &&
        searchSpaceKey !== searchSpaceSignature) {
      searchSpaceSignature = searchSpaceKey;
      renderSearchSpaces(coarseSearchSpace, refinementSearchSpace, searchSpaceSummary,
        searchProfile, acq, satellites, live?.actual_sample_rate_hz || searchProfile['frontend.sample_rate_hz'],
        coarseToggle.checked, refinementToggle.checked);
    }
    maybePollCaf();
    if (acquisitionRows) {
      const signature = JSON.stringify(satellites);
      if (signature !== acquisitionSignature) {
        acquisitionSignature = signature;
        const rows = [];
        for (const satellite of satellites) {
          const row = document.createElement('tr');
          const number = value=>Number.isFinite(value) ? value.toFixed(3) : '—';
          const power = value=>Number.isFinite(value) ? value.toPrecision(6) : '—';
          const cells = [`GPS ${satellite.prn}`, satellite.phase.replaceAll('_',' '), number(satellite.doppler_hz),
            number(satellite.code_delay_samples), `${power(satellite.metric)} / ${power(satellite.threshold)}`,
            satellite.confirmation_method === 'none' ? 'Not requested' : `${satellite.confirmation_hits} / ${satellite.confirmation_trials}`, `${satellite.epoch} / ${satellite.reference_sample}`];
          for (const value of cells) { const cell = document.createElement('td'); cell.textContent=value; row.append(cell); }
          rows.push(row);
        }
        if (!rows.length) { const row=document.createElement('tr'), cell=document.createElement('td'); cell.colSpan=7; cell.className='empty-table'; cell.textContent='No GPS acquisition results in the current receiver run.'; row.append(cell); rows.push(row); }
        acquisitionRows.replaceChildren(...rows);
      }
    }
    for (const node of replayButtons) {
      const disabled = typeof snapshot.editable === 'boolean' ? !snapshot.editable : !connected || !['stopped', 'error'].includes(live?.state);
      if (node.disabled !== disabled) node.disabled = disabled;
      const hint = !connected ? 'Connect to the receiver first.' : !['stopped', 'error'].includes(live?.state) ? 'Stop reception before choosing a replay input.' : disabled ? 'Wait for the current receiver command to finish.' : 'Choose a capture on the receiver computer.';
      if (node.title !== hint) node.title = hint;
    }
  }
  update();
  fromHash();
  return {setView: show, update, get view() { return selected; },
    get cafEnabled() { return cafToggle.checked; }};
}
