/* Display units only; receiver profiles and APIs retain integer hertz. */
const units = {
  'frontend.signal_frequency_hz': {scale:1e9, decimals:9, unit:'GHz'},
  'frontend.center_frequency_hz': {scale:1e6, decimals:6, unit:'MHz'},
  'frontend.sample_rate_hz': {scale:1e6, decimals:6, unit:'Msps'},
  'hackrf.bandwidth_hz': {scale:1e6, decimals:6, unit:'MHz'}
};

export function frequencyUnits(key) { return units[key]; }

/** Exact decimal conversion avoids floating multiplication errors at one-hertz steps. */
export function frequencyToHz(key, value) {
  const text = String(value).trim(), spec = units[key];
  if (!spec || text === '') return text;
  const match = /^(\d*)(?:\.(\d*))?$/.exec(text);
  if (!match || !(match[1] || match[2])) return null;
  const fraction = (match[2] || '').replace(/0+$/, '');
  if (fraction.length > spec.decimals) return null;
  return String(BigInt(match[1] || '0') * BigInt(spec.scale) +
    BigInt(fraction.padEnd(spec.decimals, '0')));
}

export function frequencyFromHz(key, value) {
  const text = String(value ?? ''), spec = units[key];
  if (!spec || !/^\d+$/.test(text)) return text;
  const hz = BigInt(text), scale = BigInt(spec.scale);
  const fraction = String(hz % scale).padStart(spec.decimals, '0').replace(/0+$/, '');
  return `${hz / scale}${fraction ? `.${fraction}` : ''}`;
}
