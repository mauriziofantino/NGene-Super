/* Presentation only. The model currently contains GPS L1 C/A PRNs 1..32;
 * identity determines position, independently of search order and outcomes. */
export function createAcquisitionTable(body) {
  const rows = new Map();
  let placeholder = body.firstElementChild;
  return {
    update(acquisition, profile = {}) {
      const results = new Map((acquisition?.results || []).map(result => [Number(result.prn), result]));
      const active = acquisition?.enabled && acquisition.searching ? Number(acquisition.current_prn) : 0;
      const ids = new Set(results.keys());
      if (active > 0) ids.add(active);
      // Reserve configured slots before completion: a newly tested PRN must not
      // insert a row above the satellite the user is reading.
      if (acquisition?.enabled) {
        const first = Number(profile['acquisition.first_prn'] || 1);
        const last = Number(profile['acquisition.last_prn'] || 32);
        for (let prn = first; prn <= last; prn++) ids.add(prn);
      }
      for (const [prn, row] of rows) if (!ids.has(prn)) { row.remove(); rows.delete(prn); }
      if (ids.size) { placeholder?.remove(); placeholder = null; }
      for (const prn of [...ids].sort((a, b) => a - b)) {
        let row = rows.get(prn);
        if (!row) {
          row = document.createElement('tr'); row.dataset.satellite = `GPS:L1CA:${prn}`;
          row.tabIndex = 0;
          for (let i = 0; i < 7; i++) row.append(document.createElement('td'));
          const next = [...body.children].find(item => Number(item.firstElementChild.textContent) > prn);
          body.insertBefore(row, next || null); rows.set(prn, row);
        }
        const result = results.get(prn), searching = prn === active;
        row.className = searching ? 'acquisition-searching' : result?.confirmed ? 'acquisition-acquired' : '';
        const state = searching ? 'Acquiring' : result ? (result.confirmed ? 'Confirmed' : result.peak_ratio > 1 ? 'Rejected in confirmation' : 'Below threshold') : 'Waiting for search';
        // Active-search telemetry has no stage/progress fields. Do not invent
        // intermediate states or present the previous round's numbers as current.
        const values = [prn, state, ...(result && !searching ? [Number(result.doppler_hz).toFixed(1), Number(result.code_phase_chips).toFixed(4), Number(result.peak_ratio).toFixed(2), `${result.successes}/4`, `${result.epoch} / ${result.first_sample}`] : Array(5).fill('—'))];
        values.forEach((value, index) => {
          const cell = row.children[index];
          // Preserve unchanged text nodes as well as focus and row identity.
          if (cell.textContent !== String(value)) cell.textContent = value;
        });
      }
      if (!ids.size) {
        if (!placeholder) {
          placeholder = document.createElement('tr');
          const cell = document.createElement('td'); cell.colSpan = 7;
          placeholder.append(cell); body.append(placeholder);
        }
        placeholder.firstElementChild.textContent = 'Enable acquisition in settings, apply the profile and start reception.';
      }
    }
  };
}
