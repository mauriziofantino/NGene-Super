/* Export versioned Markdown to a GitHub Wiki checkout. No network, commits or pushes. */
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '..');
const source = path.join(root, 'docs/wiki');
const args = process.argv.slice(2);
const options = {repository:'https://github.com/links-ads/nav-ngene-evo', ref:'main'};
for (let n = 0; n < args.length; n += 2) {
  const key = args[n].replace(/^--/, '');
  if (!['output', 'repository', 'ref'].includes(key) || !args[n + 1]) {
    throw new Error('Usage: node scripts/prepare-wiki.cjs --output WIKI_CHECKOUT [--repository URL] [--ref main]');
  }
  options[key] = args[n + 1];
}
if (!options.output) throw new Error('--output is required; use a separate wiki checkout or staging directory.');
const output = path.resolve(options.output);
if (output === root || output === source || source.startsWith(output + path.sep) || output.startsWith(source + path.sep)) {
  throw new Error('Output must be separate from the receiver repository root and wiki source tree.');
}
const repository = options.repository.replace(/\.git\/?$/, '').replace(/\/$/, '');
if (!/^https:\/\/github\.com\/[^/]+\/[^/]+$/.test(repository)) throw new Error('Expected a GitHub repository URL.');
const slug = repository.slice('https://github.com/'.length);
const ref = encodeURIComponent(options.ref);
const encodePath = value => value.split('/').map(encodeURIComponent).join('/');
function rewrite(text, filename) {
  return text.replace(/(!?\[[^\]\n]*\]\()([^\s)]+)(\))/g, (full, before, target, after) => {
    if (/^(?:[a-z]+:|#|\/)/i.test(target)) return full;
    const [relative, fragment] = target.split('#');
    const resolved = path.resolve(path.dirname(filename), decodeURIComponent(relative));
    if (!fs.existsSync(resolved)) throw new Error(`Broken source link in ${filename}: ${target}`);
    const inWiki = path.relative(source, resolved).replaceAll('\\', '/');
    const anchor = fragment ? `#${fragment}` : '';
    let link;
    if (!inWiki.startsWith('../') && path.extname(resolved) === '.md') {
      link = `${repository}/wiki/${encodePath(inWiki.replace(/\.md$/, ''))}${anchor}`;
    } else if (!inWiki.startsWith('../')) {
      link = `https://raw.githubusercontent.com/wiki/${slug}/${encodePath(inWiki)}${anchor}`;
    } else {
      const relativeRoot = path.relative(root, resolved).replaceAll('\\', '/');
      if (relativeRoot.startsWith('../')) throw new Error(`Link escapes repository: ${target}`);
      link = `${repository}/blob/${ref}/${encodePath(relativeRoot)}${anchor}`;
    }
    return before + link + after;
  });
}
fs.mkdirSync(output, {recursive:true});
const pages = fs.readdirSync(source).filter(name => name.endsWith('.md')).sort();
if (!pages.includes('Home.md')) throw new Error('Missing wiki Home.md.');
for (const name of pages) {
  const filename = path.join(source, name);
  fs.writeFileSync(path.join(output, name), rewrite(fs.readFileSync(filename, 'utf8'), filename));
}
fs.cpSync(path.join(source, 'images'), path.join(output, 'images'), {recursive:true});
console.log(`Prepared ${pages.length} pages and GUI images in ${output}. Review git diff in the wiki checkout before committing and pushing.`);
