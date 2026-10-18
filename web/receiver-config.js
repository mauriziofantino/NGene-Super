/* GNSS configuration reference, pending integration of the original receiver modules.
 * Values come from original NGene sampleConfigurationFiles/configuration:
 * GNSS_RX.conf, Acquisition.conf, Tracking.conf, NavigationMessages.conf and PVT.conf. They are shipped
 * example values, not defaults of this receiver or assumptions about parser ranges.
 * Semantics checked against receiver/manageConfiguration.c, acquisition/manageAcqConfiguration.c,
 * tracking/manageTrkConfiguration.c and pvt/managePvtConfiguration.c.
 * These controls have no INI names and never enter the operational profile.
 */
const parameter = (key, label, value, unit = '', hint = '', options = null) => ({key, label, value, unit, hint, options});
const toggle = (key, label, value, hint = '') => parameter(key, label, value, '', hint, [['0', 'Disabled'], ['1', 'Enabled']]);
const group = (title, fields, note = '') => ({title, fields, note});
const acquisition = (system, doppler, step, sums) => group(system, [
  parameter(`${system} false detection probability`, 'False detection probability', '1e-9'),
  parameter(`${system} Maximum Doppler Frequency (Hz)`, 'Doppler search half-width', doppler, 'Hz', 'Search from −this value to +this value around the expected signal.'),
  parameter(`${system} Doppler Frequency step (Hz)`, 'Doppler search step', step, 'Hz'),
  parameter(`${system} Code delay step (Chip)`, 'Code delay step', '0.25', 'chip'),
  parameter(`${system} non coherent sums (code periods)`, 'Noncoherent accumulation', sums, 'code periods'),
  parameter(`${system} CN N - number of attempts`, 'Confirmation attempts (N)', '4'),
  parameter(`${system} CN M - number of required successes`, 'Required successes (M)', '3')
]);
const loops = system => [
  parameter(`${system} DLLbandwidth`, 'DLL bandwidth', '2', 'Hz'),
  parameter(`${system} DLLgain`, 'DLL gain', '1'),
  parameter(`${system} FLLbandwidth`, 'FLL bandwidth', '15', 'Hz'),
  parameter(`${system} FLLgain`, 'FLL gain', '1'),
  parameter(`${system} PLLbandwidth`, 'PLL bandwidth', '15', 'Hz'),
  parameter(`${system} PLLgain`, 'PLL gain', '1')
];
const SECTIONS = {
  receiver: [
    group('Constellations & channel allocation', [
      parameter('Number of supported GPS channels', 'GPS channels', '8', 'channels'),
      parameter('Number of supported Galileo channels', 'Galileo channels', '8', 'channels'),
      parameter('Number of supported EGNOS channels', 'SBAS / EGNOS channels', '1', 'channel', 'The original receiver supports at most one EGNOS channel.')
    ], 'The original automatic allocation example reserves 17 channels across GPS, Galileo and EGNOS.'),
    group('Signal selection & receiver clock', [
      parameter('Automatic Configuration', 'PRN allocation', '1', '', 'Manual allocation uses the original PRN_List.conf signal list.', [['1', 'Automatic PRN search'], ['0', 'Manual PRN list']]),
      toggle('Enable all Galileo PRNs', 'Search all Galileo PRNs', '1', 'When disabled, the original receiver uses its configured Galileo PRN list.'),
      toggle('Receiver clock emulation', 'Use host clock as receiver time', '0', 'The original receiver disables clock emulation during file replay.'),
      parameter('Receiver clock error', 'Maximum host-clock error', '120', 's')
    ])
  ],
  acquisition: [
    acquisition('GPS', '5000', '667', '8'), acquisition('Galileo', '5000', '87', '8'), acquisition('EGNOS', '4000', '250', '1'),
    group('Fine acquisition / refinement', [
      parameter('R Delta for Doppler Frequency (Hz)', 'Doppler refinement half-width', '667', 'Hz'),
      parameter('R Doppler Step (Hz)', 'Doppler refinement step', '10', 'Hz'),
      parameter('R Delta for Code Delay (chip)', 'Code delay refinement half-width', '1', 'chip'),
      parameter('R Code Delay Step (samples)', 'Code delay refinement step', '1', 'sample')
    ])
  ],
  tracking: [
    group('GPS tracking loops', [
      parameter('GPS userIntperiod', 'Coherent integration period', '1', 'ms', '', ['1', '2', '4', '5', '10', '20'].map(value => [value, value])),
      ...loops('GPS'),
      parameter('GPS spacing', 'Early–late spacing', '0.8', 'chip', 'Total distance between Early and Late; original range is greater than 0 and at most 2 chips.')
    ]),
    group('Galileo tracking loops', [
      ...loops('Galileo'),
      parameter('Galileo spacing', 'Early–late spacing', '0.4', 'chip', 'Total distance between Early and Late; original range is greater than 0 and at most 2 chips.')
    ], 'The original Galileo tracking integration period is fixed at 4 ms.'),
    group('Lock and integration control', [
      parameter('CN0 cutoff', 'C/N₀ cutoff', '30', 'dB-Hz'),
      parameter('FLLPLLChoiceFilter Length', 'FLL / PLL decision filter length', '20', 'updates'),
      parameter('FLLPLLChoiceFilterUpdateTime', 'FLL / PLL decision interval', '100', 'ms'),
      parameter('PLLFLLthreshold', 'FLL / PLL transition threshold', '1', 'Hz'),
      parameter('changePeriodCtrlLength', 'Integration transition checks', '5', 'transitions', 'Checks valid data transitions before switching to the requested integration period.'),
      parameter('EGNOS userIntperiod', 'EGNOS integration period', '1', 'ms', 'EGNOS reuses the GPS tracking loop parameters.')
    ]),
    group('Additional correlators', [
      toggle('Multiple Correlators Flag', 'Enable multiple correlators', '0'),
      parameter('Number Of Multiple Correlators', 'Number of correlators', '5', '', 'The original parser uses an odd count of at least three.'),
      parameter('Multiple Correlators GPS Spacing', 'GPS correlator spacing', '4', 'samples'),
      parameter('Multiple Correlators Galileo Spacing', 'Galileo correlator spacing', '2', 'samples'),
      parameter('Multiple Correlators EGNOS Spacing', 'EGNOS correlator spacing', '4', 'samples')
    ])
  ],
  navigation: [
    group('Navigation time', [
      parameter('GPS WN correction', 'GPS week-number rollover correction', '2048', 'weeks', 'Original NavigationMessages.conf reference; choose the rollover epoch for the recorded or live signal when decoding is integrated.'),
      parameter('Default Leap Seconds', 'Fallback leap seconds', '18', 's', 'Original reference value used until valid navigation time data is available.')
    ]),
    group('Navigation solution', [
      parameter('PVT rate', 'Solution / pseudorange rate', '1', 'Hz'),
      parameter('PVT method', 'PVT method', '0', '', '', [['0', 'Least squares (LMS)'], ['1', 'Kalman filter']]),
      parameter('Max GDOP', 'Maximum GDOP', '30'),
      parameter('CSM filter length', 'Carrier-smoothing filter length', '32', '', '0 disables carrier smoothing.'),
      toggle('Static Receiver', 'Static receiver', '1', 'Disable for a moving receiver.')
    ]),
    group('Satellite selection & corrections', [
      toggle('Use elevation mask', 'Use elevation mask', '1'),
      parameter('Elevation mask', 'Minimum elevation', '5', 'degrees'),
      toggle('Use urban canyon mask', 'Use urban canyon mask', '0', 'The original configuration recommends disabling the elevation mask when using the urban canyon mask.'),
      toggle('Use EGNOS', 'Apply EGNOS corrections', '1'),
      parameter('Not Authenticated Sats in PVT', 'OSNMA satellite selection', '2', '', '', [['0', 'Require authentication'], ['1', 'Exclude failed authentication'], ['2', 'No authentication-based exclusion']]),
      parameter('Enable RedCED in PVT', 'Galileo reduced ephemerides (RedCED)', '0', '', '', [['0', 'Disabled'], ['1', 'Use when full CED is unavailable'], ['2', 'RedCED only']])
    ])
  ]
};

/** Render once; module availability must be implemented before these can become editable. */
export function createReceiverConfiguration(container) {
  const sections = new Map();
  for (const [topic, groups] of Object.entries(SECTIONS)) {
    const section = document.createElement('section');
    section.dataset.receiverSettings = topic;
    section.hidden = true;
    const note = document.createElement('p'); note.className = 'receiver-reference-note';
    note.textContent = 'GNSS module not integrated. These are original NGene reference values; they are not applied or saved by this receiver build.';
    const holder = document.createElement('div'); holder.className = 'receiver-parameter-groups';
    for (const {title, fields, note: description} of groups) {
      const fieldset = document.createElement('fieldset'); fieldset.className = 'receiver-parameter-group'; fieldset.disabled = true;
      const legend = document.createElement('legend'); legend.textContent = title; fieldset.append(legend);
      if (description) { const hint = document.createElement('p'); hint.className = 'muted'; hint.textContent = description; fieldset.append(hint); }
      const grid = document.createElement('div'); grid.className = 'receiver-parameter-grid';
      for (const {key, label, value, unit, hint, options} of fields) {
        const wrapper = document.createElement('label'); wrapper.className = 'field';
        const caption = document.createElement('span'); caption.append(document.createTextNode(label));
        if (unit) { const units = document.createElement('span'); units.className = 'unit'; units.textContent = unit; caption.append(units); }
        const input = document.createElement(options ? 'select' : 'input');
        input.dataset.referenceParam = key; input.disabled = true;
        if (options) for (const [id, text] of options) { const option = document.createElement('option'); option.value = id; option.textContent = text; input.append(option); }
        else input.type = 'text';
        input.value = value; wrapper.append(caption, input);
        if (hint) { const detail = document.createElement('small'); detail.textContent = hint; wrapper.append(detail); }
        grid.append(wrapper);
      }
      fieldset.append(grid); holder.append(fieldset);
    }
    section.append(note, holder); container.append(section); sections.set(topic, section);
  }
  return {
    select(topic) {
      container.classList.toggle('hidden', !sections.has(topic));
      for (const [key, section] of sections) section.hidden = key !== topic;
    }
  };
}
