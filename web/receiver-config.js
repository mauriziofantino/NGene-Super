/** Render once; module availability must be implemented before these can become editable. */
export function createReceiverConfiguration(container, sectionsFromFile) {
  const sections = new Map();
  for (const [topic, groups] of Object.entries(sectionsFromFile)) {
    const section = document.createElement('section');
    section.dataset.receiverSettings = topic;
    section.hidden = true;
    const note = document.createElement('p'); note.className = 'receiver-reference-note';
    note.textContent = ['receiver','acquisition'].includes(topic)
      ? 'Historical NGene settings for other signals and future modules. Operational GPS L1 C/A and Galileo E1-B settings are in the acquisition forms. These reference values are not applied or saved.'
      : 'Classic reference parameters are shown separately from the applied receiver settings.';
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
    if (['receiver','acquisition'].includes(topic)) {
      const details = document.createElement('details'), summary = document.createElement('summary');
      details.className = 'receiver-reference-note';
      summary.textContent = 'Original NGene settings for unavailable modules';
      details.append(summary, note, holder); section.append(details);
    } else section.append(note, holder);
    container.append(section); sections.set(topic, section);
  }
  return {
    select(topic) {
      container.classList.toggle('hidden', !sections.has(topic));
      for (const [key, section] of sections) section.hidden = key !== topic;
    }
  };
}
