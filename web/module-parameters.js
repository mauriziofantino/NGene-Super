/* Declarative module forms. Descriptors contain data, never executable expressions or HTML.
 * Receiver-side parsers/validators remain authoritative for all operational parameters. */
export function moduleParameters(schema) {
  if (schema?.version !== 1 || !Array.isArray(schema.modules) || !schema.references)
    throw new Error('Unsupported receiver module descriptor.');
  const fields = [], definitions = new Map(), groups = new Map(), rules = [];
  const matches = (condition, values) => !condition || Object.entries(condition).every(([k,v]) => String(values[k] ?? definitions.get(k)?.default) === String(v));
  for (const module of schema.modules) {
    if (!Array.isArray(module.groups) || !Array.isArray(module.rules)) throw new Error('Invalid module descriptor.');
    for (const group of module.groups) {
      if (!['receiver','acquisition','tracking','navigation','dsp'].includes(group.topic) || groups.has(group.title))
        throw new Error('Invalid or duplicate module group.');
      groups.set(group.title, group);
      for (const f of group.fields) {
        if (!/^[a-z][a-z0-9_]*\.[a-z][a-z0-9_]*$/.test(f.key) || definitions.has(f.key) ||
            !['number','select','text'].includes(f.type) || typeof f.label !== 'string')
          throw new Error('Invalid or duplicate module parameter.');
        if (f.type === 'select' && (!Array.isArray(f.options) || !f.options.length)) throw new Error('Missing parameter choices.');
        definitions.set(f.key, f);
        fields.push([group.title, f.key, f.label, f.type, f.options, f.unit, f.hint]);
      }
    }
    for (const rule of module.rules) {
      if (!['lessEqual','ratioLess','minimum','sumAtMost'].includes(rule.kind) || !Array.isArray(rule.keys))
        throw new Error('Unsupported module validation rule.');
      rules.push(rule);
    }
  }
  const active = (key, values) => matches(definitions.get(key)?.visibleWhen, values);
  return {
    fields, definitions, groups, references:schema.references, active,
    defaults(values) { for (const [key,f] of definitions) if (f.default !== undefined && values[key] === undefined) values[key] = String(f.default); },
    error(key, values) {
      if (!active(key, values)) return '';
      const f = definitions.get(key), value = Number(values[key]);
      if (f?.type === 'select' && !f.options.some(o => String(o.value) === String(values[key]))) return `${f.label}: select a supported option.`;
      if (f?.exclusiveMin && value <= f.min) return `${f.label}: enter a positive step.`;
      if (f?.exclusiveMax && value >= f.max) return `${f.label}: must be less than ${f.max}.`;
      for (const rule of rules) {
        if (!rule.keys.includes(key) || !matches(rule.when, values)) continue;
        const get = v => typeof v === 'string' ? Number(values[v]) : v;
        const valid = rule.kind === 'sumAtMost' ? get(rule.left) + get(rule.right) + get(rule.extra ?? 0) <= get(rule.total) : rule.kind === 'lessEqual' ? get(rule.left) <= get(rule.right) :
          rule.kind === 'minimum' ? get(rule.key) >= rule.value :
          (get(rule.numerator) * (rule.scale ?? 1) / get(rule.denominator)) < rule.limit;
        if (!valid) return rule.message;
      }
      return '';
    },
    update(form, values, editable) {
      for (const input of form.elements) if (definitions.has(input.name)) {
        const visible = active(input.name, values);
        input.closest('.field').hidden = !visible;
        input.disabled = !editable || !visible;
      }
    }
  };
}
