// Builds the page without esbuild (no npm in this environment): every src/**/*.ts is
// turned into an ES module with Node's own TypeScript transform (node:module
// stripTypeScriptTypes, mode 'transform': enums and parameter properties too) and
// written to dist/ at the same relative path. The browser loads them unbundled as
// native modules; import specifiers are rewritten to .js, text assets (.txt) become
// small modules that export the text. The entry names match build.mjs's, so the
// page's own Worker/addModule URLs (/host/game-worker.js, /audio/worklet.js ...) work.
import fs from 'node:fs';
import path from 'node:path';
import { stripTypeScriptTypes } from 'node:module';

const SRC = 'src', DIST = process.env.DIST || 'dist';
fs.rmSync(DIST, { recursive: true, force: true });
fs.mkdirSync(DIST, { recursive: true });

function walk(d) {
  const out = [];
  for (const e of fs.readdirSync(d, { withFileTypes: true })) {
    const p = path.join(d, e.name);
    if (e.isDirectory()) out.push(...walk(p)); else out.push(p);
  }
  return out;
}

const spec = (from, s) => {
  if (!s.startsWith('.') && !s.startsWith('/')) return s;
  if (s.endsWith('.txt')) return s + '.js';
  if (s.endsWith('.png')) return s + '.js';
  if (s.endsWith('.ts')) return s.slice(0, -3) + '.js';
  if (s.endsWith('.js') || s.endsWith('.mjs') || s.endsWith('.json') || s.endsWith('.css')) return s;
  const base = path.resolve(path.dirname(from), s);
  if (fs.existsSync(base + '.ts')) return s + '.js';
  if (fs.existsSync(path.join(base, 'index.ts'))) return s + '/index.js';
  return s + '.js';
};

let n = 0, fail = 0;
const assets = new Set();
for (const f of walk(SRC)) {
  if (f.endsWith('.mjs') || (f.endsWith('.js') && !f.includes('/static/'))) {      // plain modules: as they are
    const out = path.join(DIST, path.relative(SRC, f)); fs.mkdirSync(path.dirname(out), { recursive: true }); fs.copyFileSync(f, out); continue; }
  if (!f.endsWith('.ts') || f.endsWith('.d.ts')) continue;
  const rel = path.relative(SRC, f);
  let code = fs.readFileSync(f, 'utf8');
  try {
    code = stripTypeScriptTypes(code, { mode: 'transform', sourceUrl: rel });
  } catch (e) { console.error('FAIL', rel, e.message.split('\n')[0]); fail++; continue; }
  code = code.replace(/^\s*import\s*(['"])[^'"]+\.css\1;?\s*$/gm, '');
  code = code.replace(/(\bfrom\s*|\bimport\s*\(\s*|\bimport\s+)(['"])([^'"]+)\2/g, (m, a, q, s) => {
    const t = spec(f, s);
    if (/\.(txt|png)\.js$/.test(t)) assets.add(JSON.stringify([path.resolve(path.dirname(f), s), path.posix.join('/', path.posix.dirname(rel.split(path.sep).join('/')), t)]));
    return a + q + t + q;
  });
  const out = path.join(DIST, rel.replace(/\.ts$/, '.js'));
  fs.mkdirSync(path.dirname(out), { recursive: true });
  fs.writeFileSync(out, code); n++;
}
for (const aj of assets) {
  // the URL the importing module asks for (above the site's root it stays at the root, as in a browser)
  const [a, url] = JSON.parse(aj);
  const out = path.join(DIST, path.posix.normalize(url));
  fs.mkdirSync(path.dirname(out), { recursive: true });
  const body = a.endsWith('.png')
    ? `export default ${JSON.stringify('data:image/png;base64,' + fs.readFileSync(a).toString('base64'))};\n`
    : `export default ${JSON.stringify(fs.readFileSync(a, 'utf8'))};\n`;
  fs.writeFileSync(out, body);
}
// the stylesheet (main.ts imports app/style.css; esbuild made it main.css)
fs.copyFileSync('src/app/style.css', path.join(DIST, 'main.css'));
// the start page, its static files and languages, as build.mjs
fs.copyFileSync('index.html', path.join(DIST, 'index.html'));
for (const f of fs.readdirSync('src/static')) fs.copyFileSync(path.join('src/static', f), path.join(DIST, f));
fs.mkdirSync(path.join(DIST, 'lang'), { recursive: true });
for (const f of fs.readdirSync('src/app/lang')) if (f.endsWith('.json'))
  fs.writeFileSync(path.join(DIST, 'lang', f), JSON.stringify(JSON.parse(fs.readFileSync(path.join('src/app/lang', f), 'utf8'))));
const page = name => `<!doctype html><meta charset=utf-8><title>${name}</title><script type=module src="/test/${name}.js"></script>`;
for (const name of ['play', 'hostcheck', 'shadercheck', 'aicheck']) if (fs.existsSync(`src/test/${name}.ts`)) fs.writeFileSync(path.join(DIST, 'test', `${name}.html`), page(name));
console.log(`fo-build: ${n} modules, ${assets.size} assets, ${fail} failed -> ${DIST}`);
process.exit(fail ? 1 : 0);
