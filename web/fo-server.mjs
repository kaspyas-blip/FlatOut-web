// FlatOut web: a dependency-free stand-in for web/server-rs (no cargo/npm here).
//   node fo-server.mjs --data <install folder> --wasm <flatout.wasm> [--port 8080] [--dist dist]
// Serves the page (dist/), /game.json + /game.wasm, the install folder read-only as
// /data/<path> (HTTP ranges) with /data-manifest listing it, and the headers
// SharedArrayBuffer needs (COOP/COEP). Only the game's own files are listed:
// FlatOut.exe, *.bfs, filesystem.ini, and the folders the game writes (savegame/).
// The virtual LAN switch (/lan) comes with multiplayer.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import zlib from 'node:zlib';

const arg = (k, d) => { const i = process.argv.indexOf(k); return i > 0 ? process.argv[i + 1] : d; };
const DATA = path.resolve(arg('--data', '.'));
const WASM = path.resolve(arg('--wasm', 'flatout.wasm'));
const DIST = path.resolve(arg('--dist', 'dist'));
const PORT = +arg('--port', '8080');
const HOST = arg('--host', '127.0.0.1');
const GUEST_END = 0x28000000, INITIAL_MB = 1024;

const GAME_FILE = /^(flatout\.exe|[^/]+\.bfs|filesystem\.ini|savegame(\/.*)?)$/i;
function manifest() {
  const files = [];
  const walk = (dir, rel) => {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
      const r = rel ? `${rel}/${e.name}` : e.name;
      if (!GAME_FILE.test(r)) continue;
      const st = fs.statSync(path.join(dir, e.name));
      if (e.isDirectory()) { files.push({ p: r, d: 1 }); walk(path.join(dir, e.name), r); }
      else files.push({ p: r, s: st.size, m: Math.floor(st.mtimeMs / 1000) });
    }
  };
  walk(DATA, '');
  return { files };
}

// the module: flatout.wasm, or flatout.wasm.gz (unpacked into memory once; the release ships it packed)
let wasmInfo = null;
function gameInfo() {
  const st = fs.statSync(WASM);
  if (!wasmInfo || wasmInfo.mtime !== st.mtimeMs) {
    let bytes = fs.readFileSync(WASM);
    if (WASM.endsWith('.gz')) bytes = zlib.gunzipSync(bytes);
    const h = crypto.createHash('sha256').update(bytes).digest('hex').slice(0, 16);
    wasmInfo = { mtime: st.mtimeMs, bytes, json: { pages: INITIAL_MB * 16, maxPages: 65536, guestEnd: GUEST_END, rev: h, size: bytes.length, opt: '-O2', lift: 'plain', module: h } };
  }
  return wasmInfo.json;
}
function sendWasm(req, res, immutable) {
  gameInfo();
  const b = wasmInfo.bytes;
  res.writeHead(200, { ...SEC, 'Content-Type': 'application/wasm', 'Content-Length': b.length, 'Cache-Control': immutable ? 'public, max-age=31536000, immutable' : 'no-cache' });
  res.end(req.method === 'HEAD' ? undefined : b);
}

const TYPES = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css',
  '.json': 'application/json', '.png': 'image/png', '.wasm': 'application/wasm', '.txt': 'text/plain; charset=utf-8', '.svg': 'image/svg+xml' };
const SEC = { 'Cross-Origin-Opener-Policy': 'same-origin', 'Cross-Origin-Embedder-Policy': 'require-corp',
  'Cross-Origin-Resource-Policy': 'same-origin', 'X-Content-Type-Options': 'nosniff' };

function sendFile(req, res, file, type, cache) {
  let st; try { st = fs.statSync(file); } catch { console.log(`404 ${req.url}`); res.writeHead(404, SEC); return res.end('not found'); }
  if (!st.isFile()) { res.writeHead(404, SEC); return res.end('not found'); }
  const h = { ...SEC, 'Content-Type': type || TYPES[path.extname(file).toLowerCase()] || 'application/octet-stream',
    'Accept-Ranges': 'bytes', 'Cache-Control': cache || 'no-cache', 'Last-Modified': st.mtime.toUTCString() };
  const m = /^bytes=(\d*)-(\d*)$/.exec(req.headers.range || '');
  if (m) {
    let a = m[1] === '' ? st.size - +m[2] : +m[1], b = m[1] === '' || m[2] === '' ? st.size - 1 : +m[2];
    if (b >= st.size) b = st.size - 1;
    if (a < 0 || a > b) { res.writeHead(416, { ...h, 'Content-Range': `bytes */${st.size}` }); return res.end(); }
    res.writeHead(206, { ...h, 'Content-Range': `bytes ${a}-${b}/${st.size}`, 'Content-Length': b - a + 1 });
    if (req.method === 'HEAD') return res.end();
    return fs.createReadStream(file, { start: a, end: b }).pipe(res);
  }
  res.writeHead(200, { ...h, 'Content-Length': st.size });
  if (req.method === 'HEAD') return res.end();
  fs.createReadStream(file).pipe(res);
}

const server = http.createServer((req, res) => {
  const u = new URL(req.url, 'http://x');
  let p; try { p = decodeURIComponent(u.pathname); } catch { res.writeHead(400, SEC); return res.end(); }
  if (p.includes('\0') || p.split('/').includes('..')) { res.writeHead(400, SEC); return res.end(); }
  if (p === '/game.json') { const j = JSON.stringify(gameInfo()); res.writeHead(200, { ...SEC, 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }); return res.end(j); }
  if (p === '/game.wasm') return sendWasm(req, res, u.searchParams.has('v'));
  if (p === '/data-manifest') { const j = JSON.stringify(manifest()); res.writeHead(200, { ...SEC, 'Content-Type': 'application/json', 'Cache-Control': 'no-cache' }); return res.end(j); }
  if (p.startsWith('/data/')) {
    const rel = p.slice(6);
    if (!GAME_FILE.test(rel)) { res.writeHead(404, SEC); return res.end('not a game file'); }
    return sendFile(req, res, path.join(DATA, rel), 'application/octet-stream');
  }
  if (p === '/players') { res.writeHead(200, { ...SEC, 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }); return res.end('{"players":[]}'); }
  if (p.startsWith('/artifacts/') && req.method === 'PUT' && process.env.FO_ARTIFACTS) {
    const out = path.join(process.env.FO_ARTIFACTS, path.basename(p));
    const w = fs.createWriteStream(out); req.pipe(w); w.on('finish', () => { res.writeHead(201, SEC); res.end(); }); return;
  }
  const file = path.join(DIST, p.endsWith('/') ? p + 'index.html' : p);
  if (!file.startsWith(DIST)) { res.writeHead(400, SEC); return res.end(); }
  sendFile(req, res, file);
});

// ---- the virtual LAN switch (/lan): what web/server-rs/crates/lan does, minus bundles,
// queues, bans and free roam. Each page gets 10.77.0.<2..250>; a binary frame is
// [0] kind [4..7] src (written here) [8..11] dst [12..13] sport [14..15] dport [16..] data,
// sent to its destination, or to every other page for a broadcast (LAN kinds: only to
// pages that have LAN play open).
const VIP_NET = 0x0a4d0000, BCASTS = new Set([0xffffffff, 0x0a4d00ff, 0x0a4dffff]);
const peers = new Map();                       // vip -> { sock, name, lan, token }
const tokens = new Map();                      // token -> vip (a reconnect gets its address back)
function wsSend(sock, data, binary) {
  if (sock.destroyed) return;
  const b = binary ? data : Buffer.from(data);
  const n = b.length;
  const h = n < 126 ? Buffer.from([binary ? 0x82 : 0x81, n])
    : n < 65536 ? Buffer.from([binary ? 0x82 : 0x81, 126, n >> 8, n & 255])
    : (() => { const x = Buffer.alloc(10); x[0] = binary ? 0x82 : 0x81; x[1] = 127; x.writeBigUInt64BE(BigInt(n), 2); return x; })();
  sock.write(Buffer.concat([h, b]));
}
function peerList() { return [...peers].map(([vip, p]) => ({ vip, name: p.name })); }
function announce() { const m = JSON.stringify({ t: 'peers', peers: peerList() }); for (const p of peers.values()) wsSend(p.sock, m); }
function route(from, f) {
  if (f.length < 16) return;
  f.writeUInt32BE(from, 4);
  const dst = f.readUInt32BE(8), kind = f[0];
  if (BCASTS.has(dst)) {
    for (const [vip, p] of peers) if (vip !== from && (kind < 3 || kind > 5 || p.lan)) wsSend(p.sock, f, true);
  } else { const p = peers.get(dst); if (p) wsSend(p.sock, f, true); }
}
server.on('upgrade', (req, sock) => {
  const u = new URL(req.url, 'http://x');
  if (u.pathname !== '/lan' || req.headers['upgrade']?.toLowerCase() !== 'websocket') { sock.destroy(); return; }
  const acc = crypto.createHash('sha1').update(req.headers['sec-websocket-key'] + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  sock.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${acc}\r\n\r\n`);
  sock.setNoDelay(true);
  let buf = Buffer.alloc(0), vip = 0, parts = [], ptype = 0;
  const leave = () => { if (vip && peers.get(vip)?.sock === sock) { peers.delete(vip); console.log(`[lan] ${vip >>> 8 & 255}.${vip & 255} left`); announce(); } vip = 0; };
  const onMsg = (op, data) => {
    if (op === 2) { if (vip) route(vip, data); return; }
    let m; try { m = JSON.parse(data.toString()); } catch { return; }
    if (m.t === 'hello' && !vip) {
      let want = m.token && tokens.get(m.token);
      if (!want || peers.has(want)) { want = 0; for (let h = 2; h < 251; h++) if (![...peers.keys()].includes(VIP_NET + h)) { want = VIP_NET + h; break; } }
      if (!want) { sock.end(); return; }
      vip = want;
      const token = m.token && tokens.get(m.token) === vip ? m.token : crypto.randomBytes(12).toString('hex');
      tokens.set(token, vip);
      peers.set(vip, { sock, name: String(m.name ?? 'player').slice(0, 32), lan: !!m.lan });
      wsSend(sock, JSON.stringify({ t: 'welcome', vip, mask: '255.255.255.0', token }));
      console.log(`[lan] ${peers.get(vip).name} joined as 10.77.0.${vip & 255}`);
      announce();
    } else if (m.t === 'ping') wsSend(sock, JSON.stringify({ t: 'pong', id: m.id }));
    else if (m.t === 'lan' && vip) peers.get(vip).lan = !!m.on;
  };
  sock.on('data', d => {
    buf = Buffer.concat([buf, d]);
    for (;;) {
      if (buf.length < 2) return;
      const fin = buf[0] & 0x80, op = buf[0] & 15, masked = buf[1] & 0x80;
      let n = buf[1] & 127, o = 2;
      if (n === 126) { if (buf.length < 4) return; n = buf.readUInt16BE(2); o = 4; }
      else if (n === 127) { if (buf.length < 10) return; n = Number(buf.readBigUInt64BE(2)); o = 10; }
      if (n > 1 << 20) { sock.destroy(); return; }
      const mo = o; if (masked) o += 4;
      if (buf.length < o + n) return;
      const pl = Buffer.from(buf.subarray(o, o + n));
      if (masked) for (let i = 0; i < n; i++) pl[i] ^= buf[mo + (i & 3)];
      buf = buf.subarray(o + n);
      if (op === 8) { sock.end(Buffer.from([0x88, 0])); leave(); return; }
      if (op === 9) { sock.write(Buffer.concat([Buffer.from([0x8a, pl.length]), pl])); continue; }
      if (op === 10) continue;
      if (op === 0) { parts.push(pl); if (fin) { onMsg(ptype, Buffer.concat(parts)); parts = []; } continue; }
      if (!fin) { ptype = op; parts = [pl]; continue; }
      onMsg(op, pl);
    }
  });
  sock.on('close', leave); sock.on('error', leave);
});
server.on('error', e => {
  if (e.code === 'EADDRINUSE') {
    console.log(`\nPort ${PORT} uz pouziva jiny program - nejspis server FlatOut Web, ktery jeste bezi.`);
    console.log('Hra pobezi s nim: otevri http://localhost:' + PORT + '/');
    console.log('Pokud chces server vypnout, spust stop.bat (nebo zavri jeho okno).');
  } else console.log('Server se nepodarilo spustit:', e.message);
  process.exit(1);
});
server.listen(PORT, HOST, () => {
  console.log(`fo-server: http://${HOST}:${PORT}/  data=${DATA}  wasm=${WASM}`);
  console.log('Server bezi. Vypnuti: napis "stop" a Enter, nebo Ctrl+C, nebo zavri toto okno.');
});
// "stop" (or "exit"/"quit") on the console ends the server cleanly
process.stdin.setEncoding('utf8');
process.stdin.on('data', d => {
  const cmd = String(d).trim().toLowerCase();
  if (cmd === 'stop' || cmd === 'exit' || cmd === 'quit') {
    console.log('Vypinam server...');
    server.close(); setTimeout(() => process.exit(0), 300);
  }
});
process.on('SIGINT', () => { console.log('Vypinam server...'); process.exit(0); });
