/* Completed coarse observations only. Display reduction never changes detection decisions. */
export function createAcquisitionSearchSpace(root) {
  root.innerHTML = `<div class="caf-controls"><label class="caf-prn-picker">Completed round <select aria-label="Search space satellite"></select></label>
    <div class="caf-mode" role="group" aria-label="Search space view"><button type="button" data-mode="2d" aria-pressed="true">2D · top view</button><button type="button" data-mode="3d" aria-pressed="false">3D · fixed perspective</button></div></div>
    <div class="caf-plot-wrap"><canvas aria-label="Completed code phase and Doppler search space" role="img"></canvas><div class="caf-empty">Waiting for a completed satellite search.</div></div>
    <p class="caf-details" aria-live="polite"></p><p class="caf-details">Power / coarse threshold: 1 is the detection threshold. Colour uses a logarithmic scale. Code-phase cells preserve maxima, including at narrow display widths; the 3D view groups four cells by maximum. Confirmation is a separate test.</p><p class="caf-cursor"></p>`;
  const picker = root.querySelector('select'), canvas = root.querySelector('canvas');
  const empty = root.querySelector('.caf-empty'), details = root.querySelector('.caf-details');
  const cursor = root.querySelector('.caf-cursor'), buttons = [...root.querySelectorAll('[data-mode]')];
  let live = null, visible = false, mode = '2d', map = null, loadedKey = '', requestKey = '', request = null;
  let drawing = false;
  const schedule = () => {
    if (drawing) return;
    drawing = true;
    requestAnimationFrame(() => { drawing = false; if (visible) paint(); });
  };
  const colour = (value, maximum) => {
    const t = Math.max(0, Math.min(1, Math.log1p(value) / Math.log1p(maximum)));
    return `hsl(${240 - 240 * t} 85% ${22 + 38 * t}%)`;
  };
  function paint() {
    if (!map) return;
    const bounds = canvas.getBoundingClientRect(), width = bounds.width, height = bounds.height;
    if (!width || !height) return;
    const dpr = window.devicePixelRatio || 1;
    canvas.width = Math.round(width * dpr); canvas.height = Math.round(height * dpr);
    const ctx = canvas.getContext('2d'); ctx.scale(dpr, dpr);
    ctx.fillStyle = '#0c1a2b'; ctx.fillRect(0, 0, width, height);
    const peak = Math.max(1.2, ...map.power.map(row => Math.max(...row)));
    ctx.font = '11px system-ui'; ctx.lineWidth = 1;
    if (mode === '2d') {
      const x0 = 66, y0 = 25, w = width - 94, h = height - 83;
      const image = document.createElement('canvas'); image.width = Math.max(1, Math.min(map.code_bins, Math.floor(w))); image.height = map.power.length;
      const raster = image.getContext('2d');
      for (let row = 0; row < map.power.length; row++) for (let col = 0; col < image.width; col++) {
        const first = Math.floor(col * map.code_bins / image.width), last = Math.ceil((col + 1) * map.code_bins / image.width);
        raster.fillStyle = colour(Math.max(...map.power[row].slice(first, last)), peak);
        raster.fillRect(col, map.power.length - row - 1, 1, 1);
      }
      ctx.imageSmoothingEnabled = false; ctx.drawImage(image, x0, y0, w, h);
      ctx.strokeStyle = '#8ba7bb'; ctx.strokeRect(x0, y0, w, h);
      ctx.fillStyle = '#b6cddd'; ctx.textAlign = 'center';
      for (const value of [0, 256, 512, 768, 1023]) ctx.fillText(String(value), x0 + w * value / 1023, y0 + h + 18);
      ctx.fillText('Code phase (chips)', x0 + w / 2, height - 13);
      ctx.textAlign = 'right';
      for (const [row, label] of [[0, '-5000'], [7, '-331'], [15, '+5000']])
        ctx.fillText(label, x0 - 8, y0 + h * (1 - (row + .5) / 16) + 4);
      ctx.save(); ctx.translate(14, y0 + h / 2); ctx.rotate(-Math.PI / 2); ctx.textAlign = 'center'; ctx.fillText('Doppler (Hz)', 0, 0); ctx.restore();
      const row = map.doppler_hz.indexOf(map.peak_doppler_hz);
      const px = x0 + w * map.peak_code_phase_chips / 1023, py = y0 + h * (1 - (row + .5) / 16);
      ctx.strokeStyle = '#fff'; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(px - 6, py); ctx.lineTo(px + 6, py); ctx.moveTo(px, py - 6); ctx.lineTo(px, py + 6); ctx.stroke();
    } else {
      // Fixed orthographic camera. Draw far rows first; height is linear power / threshold.
      const project = (x, y, z) => [width * .12 + width * .64 * x + width * .17 * y,
        height * .79 - height * .24 * y - height * .47 * z / peak];
      const line = (a, b, colourValue = '#829eb4') => {
        ctx.strokeStyle = colourValue; ctx.beginPath(); ctx.moveTo(...a); ctx.lineTo(...b); ctx.stroke();
      };
      const reduced = map.power.map(row => Array.from({length: 256}, (_, col) => Math.max(...row.slice(col * 4, col * 4 + 4))));
      for (let row = 14; row >= 0; row--) for (let col = 0; col < 255; col++) {
        const values = [reduced[row][col], reduced[row][col + 1], reduced[row + 1][col + 1], reduced[row + 1][col]];
        const points = [project(col / 255, row / 15, values[0]), project((col + 1) / 255, row / 15, values[1]),
          project((col + 1) / 255, (row + 1) / 15, values[2]), project(col / 255, (row + 1) / 15, values[3])];
        ctx.fillStyle = colour(Math.max(...values), peak); ctx.beginPath(); ctx.moveTo(...points[0]);
        for (const point of points.slice(1)) ctx.lineTo(...point);
        ctx.closePath(); ctx.fill();
      }
      line(project(0, 0, 0), project(1, 0, 0)); line(project(1, 0, 0), project(1, 1, 0)); line(project(0, 0, 0), project(0, 0, peak));
      ctx.fillStyle = '#b6cddd'; ctx.textAlign = 'center';
      for (const value of [0, 512, 1023]) { const [x, y] = project(value / 1023, 0, 0); ctx.fillText(String(value), x, y + 17); }
      ctx.fillText('Code phase (chips)', width * .44, height - 13);
      for (const [y, label] of [[0, '-5000'], [1, '+5000 Hz']]) {
        const point = project(1, y, 0); ctx.textAlign = y ? 'right' : 'left';
        ctx.fillText(label, point[0] + (y ? -2 : 8), point[1] + (y ? 16 : 6));
      }
      ctx.textAlign = 'left'; ctx.fillText('Doppler', width * .81, height * .77);
      ctx.fillText(`Power / threshold · max ${map.peak_ratio.toFixed(2)}`, 15, 20);
      for (const value of [1, peak]) {
        const point = project(0, 0, value); ctx.textAlign = 'right'; ctx.fillText(value.toFixed(2), point[0] - 5, point[1]);
      }
      ctx.setLineDash([4, 4]);
      for (const [a, b] of [[[0, 0], [1, 0]], [[1, 0], [1, 1]], [[1, 1], [0, 1]], [[0, 1], [0, 0]]]) line(project(...a, 1), project(...b, 1), '#f3ba64');
      ctx.setLineDash([]);
    }
    // Shared colour key, with the actual coarse threshold marked.
    const keyX = width - 18, keyY = 30, keyH = height * .4;
    for (let k = 0; k < keyH; k++) {
      ctx.fillStyle = colour(Math.expm1((1 - k / keyH) * Math.log1p(peak)), peak); ctx.fillRect(keyX, keyY + k, 8, 1);
    }
    const thresholdY = keyY + keyH * (1 - Math.log(2) / Math.log1p(peak));
    ctx.strokeStyle = '#fff'; ctx.beginPath(); ctx.moveTo(keyX - 4, thresholdY); ctx.lineTo(keyX + 10, thresholdY); ctx.stroke();
    ctx.fillStyle = '#dceaf4'; ctx.textAlign = 'right'; ctx.fillText('1', keyX - 5, thresholdY + 4);
    canvas.dataset.mode = mode; canvas.dataset.prn = map.prn;
  }
  async function refresh() {
    if (!visible || !live) return;
    const found = live.acquisition?.results?.find(result => String(result.prn) === picker.value);
    if (!found) return;
    const key = `${live.run_id}:${found.prn}:${found.epoch}:${found.first_sample}`;
    if (key === loadedKey || key === requestKey) return;
    request?.abort(); const controller = new AbortController(); request = controller; requestKey = key;
    if (!map || String(map.prn) !== picker.value) { map = null; empty.hidden = false; empty.textContent = 'Loading completed search…'; canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height); }
    try {
      const response = await fetch(`/api/v1/acquisition/search-space?prn=${found.prn}`, {signal: controller.signal, cache: 'no-store'});
      if (!response.ok) throw Error(`HTTP ${response.status}`);
      const next = await response.json();
      if (request !== controller) return;
      if (!next.valid) {
        map = null; details.textContent = ''; canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
        empty.hidden = false; empty.textContent = 'No completed coarse grid available for this search.'; return;
      }
      map = next; loadedKey = key; empty.hidden = true;
      details.textContent = `GPS ${map.prn} · ${map.confirmed ? 'Confirmed' : map.peak_ratio > 1 ? 'Coarse candidate, not confirmed' : 'Below coarse threshold'} · Peak / threshold ${map.peak_ratio.toFixed(2)} · Coarse peak ${map.peak_doppler_hz.toFixed(0)} Hz, ${map.peak_code_phase_chips.toFixed(3)} chips · Capture ${map.epoch} / ${map.first_sample} · ${map.fft_size} original code bins × 16 Doppler bins.`;
      schedule();
    } catch (error) {
      if (error.name !== 'AbortError' && request === controller) { empty.hidden = false; empty.textContent = 'Search space unavailable. Retrying with the next status update.'; }
    } finally { if (request === controller) { requestKey = ''; request = null; } }
  }
  picker.addEventListener('change', () => { cursor.textContent = ''; details.textContent = ''; refresh(); });
  for (const button of buttons) button.addEventListener('click', () => {
    mode = button.dataset.mode; for (const item of buttons) item.setAttribute('aria-pressed', String(item === button)); cursor.textContent = ''; schedule();
  });
  canvas.addEventListener('mousemove', event => {
    if (!map || mode !== '2d') return;
    const rect = canvas.getBoundingClientRect(), x = (event.clientX - rect.left - 66) / (rect.width - 94), y = (event.clientY - rect.top - 25) / (rect.height - 83);
    if (x < 0 || x >= 1 || y < 0 || y >= 1) { cursor.textContent = ''; return; }
    const column = Math.floor(x * map.code_bins), row = 15 - Math.floor(y * 16);
    cursor.textContent = `${(column * 1023 / map.code_bins).toFixed(2)} chips · ${map.doppler_hz[row]} Hz · Power / threshold ${map.power[row][column].toFixed(3)}`;
  });
  canvas.addEventListener('mouseleave', () => { cursor.textContent = ''; });
  new ResizeObserver(schedule).observe(canvas);
  return {
    setVisible(value) { visible = value; if (visible) { refresh(); schedule(); } else { request?.abort(); request = null; requestKey = ''; } },
    update(status) {
      if (!status || live?.run_id !== status.run_id) {
        request?.abort(); request = null; requestKey = ''; loadedKey = ''; map = null; details.textContent = ''; cursor.textContent = '';
        empty.hidden = false; empty.textContent = status ? 'Waiting for a completed satellite search.' : 'Connect to inspect completed searches.';
        canvas.getContext('2d').clearRect(0, 0, canvas.width, canvas.height);
      }
      live = status;
      const results = live?.acquisition?.results || [];
      const previous = picker.value;
      picker.replaceChildren(...results.map(result => {
        const option = document.createElement('option'); option.value = result.prn;
        option.textContent = `GPS ${result.prn} · ${result.confirmed ? 'confirmed' : result.peak_ratio > 1 ? 'not confirmed' : 'below threshold'}`; return option;
      }));
      picker.disabled = !results.length;
      if (results.some(result => String(result.prn) === previous)) picker.value = previous;
      refresh();
    }
  };
}
