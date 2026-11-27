/* Local B1C primary correlation reference for RTL-SDR CI8 (zero IF, direct spectrum). */
const assert = require('node:assert/strict');
const fs = require('node:fs');

const chipRate = 1023000;
const carrier = 1575420000;
const codeLength = 10230;
const periodCount = 24;
// B1C SIS ICD primary parameters, indexed by the two PRNs in the RTL fixture.
const parameters = {
  pilot: {12: [4408, 5557], 32: [3283, 5844]},
  data: {12: [4800, 9973], 32: [2553, 6139]}
};
const wrap = phase => ((phase % codeLength) + codeLength) % codeLength;

function primary(prn, component = 'pilot') {
  const [shift, start] = parameters[component][prn];
  const length = 10243;
  const residues = new Uint8Array(length);
  for (let k = 1; k < length; k++) residues[k * k % length] = 1;
  return Int8Array.from({length: codeLength}, (_, n) => {
    const k = (start - 1 + n) % length;
    return residues[k] === residues[(k + shift) % length] ? 1 : -1;
  });
}

// Omit both edges of each nominal code period so secondary symbols may change sign.
function reference(iq, rate, chips, phase, doppler) {
  const period = rate / 100;
  const margin = Math.ceil(rate / 5000);
  const mixed = new Float64Array(iq.length);

  function mix(frequency) {
    const angle = 2 * Math.PI * frequency / rate;
    const stepI = Math.cos(angle), stepQ = -Math.sin(angle);
    let i = 1, q = 0;
    for (let n = 0; n < iq.length; n += 2) {
      mixed[n] = iq[n] * i - iq[n + 1] * q;
      mixed[n + 1] = iq[n + 1] * i + iq[n] * q;
      [i, q] = [i * stepI - q * stepQ, q * stepI + i * stepQ];
    }
  }
  function power(offset, frequency, firstPeriod, lastPeriod) {
    const step = chipRate * (1 + frequency / carrier) / rate;
    let total = 0;
    for (let p = firstPeriod; p < lastPeriod; p++) {
      const first = p * period + margin;
      const end = (p + 1) * period - margin;
      let i = 0, q = 0;
      for (let n = first; n < end; n++) {
        const half = Math.floor(2 * (phase + offset + step * n));
        const sign = chips[(half >>> 1) % codeLength] * (half % 2 ? -1 : 1);
        i += sign * mixed[2 * n];
        q += sign * mixed[2 * n + 1];
      }
      total += i * i + q * q;
    }
    return total;
  }

  function estimate(first, last) {
    let best = {power: -1};
    // Joint coarse search protects against a code side peak at the input frequency.
    for (let df = -60; df <= 60; df += 10) {
      const frequency = doppler + df;
      mix(frequency);
      for (let k = -12; k <= 12; k++) {
        const offset = k / 8;
        const score = power(offset, frequency, first, last);
        if (score > best.power) best = {offset, frequency, power: score};
      }
    }
    const coarse = best;
    for (let df = -6; df <= 6; df++) {
      const frequency = coarse.frequency + df;
      mix(frequency);
      for (let k = -4; k <= 4; k++) {
        const offset = coarse.offset + k / 32;
        const score = power(offset, frequency, first, last);
        if (score > best.power) best = {offset, frequency, power: score};
      }
    }
    return best;
  }

  return [estimate(0, periodCount), estimate(0, periodCount / 2),
    estimate(periodCount / 2, periodCount)];
}

function selfTest() {
  const rate = 2400000, phase = 0.12, trueOffset = 0.34, trueFrequency = -1944;
  const chips = primary(32);
  const iq = new Int8Array(2 * periodCount * rate / 100);
  for (let n = 0; n < iq.length / 2; n++) {
    const chip = wrap(phase + trueOffset + n * chipRate * (1 + trueFrequency / carrier) / rate);
    const sign = chips[Math.floor(chip)] * (chip % 1 < 0.5 ? 1 : -1);
    const data = Math.floor(chipRate * n / rate / codeLength) % 2 ? -1 : 1;
    const angle = 2 * Math.PI * trueFrequency * n / rate;
    iq[2 * n] = Math.round(32 * data * sign * Math.cos(angle));
    iq[2 * n + 1] = Math.round(32 * data * sign * Math.sin(angle));
  }
  for (const result of reference(iq, rate, chips, phase + codeLength, -1960)) {
    assert(Math.abs(result.offset - trueOffset) <= 1 / 32, JSON.stringify(result));
    assert(Math.abs(result.frequency - trueFrequency) <= 1, JSON.stringify(result));
  }
  console.log('PASS: local pilot reference recovers known phase and Doppler');
}

if (process.argv[2] === '--self-test') {
  selfTest();
} else {
  const [file, rate, prn, referenceSample, referencePhase, doppler, trackingStart] =
    process.argv.slice(2, 9).map((value, index) => index === 0 ? value : Number(value));
  const component = process.argv[9] ?? 'pilot';
  assert(file && Number.isSafeInteger(rate) && rate >= 2048000 && rate % 100 === 0 &&
    Object.hasOwn(parameters, component) && parameters[component][prn] && Number.isSafeInteger(referenceSample) && referenceSample >= 0 &&
    Number.isSafeInteger(trackingStart) && trackingStart >= referenceSample &&
    Number.isFinite(referencePhase) && Number.isFinite(doppler),
  'Usage: node pilot-reference.cjs CI8 RATE PRN(12|32) ACQ_SAMPLE PHASE_CHIPS DOPPLER TRACK_START [pilot|data]');
  const codeHz = chipRate * (1 + doppler / carrier);
  const phaseAtTrackingStart = wrap(referencePhase + (trackingStart - referenceSample) * codeHz / rate);
  const first = trackingStart - Math.round(phaseAtTrackingStart * rate / codeHz);
  assert(first >= 0);
  const phase = codeLength + phaseAtTrackingStart + (first - trackingStart) * codeHz / rate;
  const bytes = Buffer.alloc(2 * rate / 100 * periodCount);
  const fd = fs.openSync(file, 'r');
  try {
    assert.equal(fs.readSync(fd, bytes, 0, bytes.length, 2 * first), bytes.length, 'Incomplete I/Q window');
  } finally {
    fs.closeSync(fd);
  }
  const estimates = reference(new Int8Array(bytes.buffer, bytes.byteOffset, bytes.length),
    rate, primary(prn, component), phase, doppler);
  console.log(JSON.stringify({prn, component, first, trackingStart, phaseAtTrackingStart, doppler, estimates}, null, 2));
}
