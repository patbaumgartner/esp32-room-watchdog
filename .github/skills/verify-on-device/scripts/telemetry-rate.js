// Measures the live telemetry rate over the WebSocket. A healthy node tracks
// the sensor loop (one reading per SOUND_SAMPLE_WINDOW_MS), so a collapse to
// roughly the heartbeat rate means something is stalling the loop — that is
// exactly how a blocking serial write once went unnoticed.
//
// Usage: node telemetry-rate.js [host] [seconds]
// Exits non-zero if the rate falls below MIN_HZ.
const net = require('net');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const HOST = process.argv[2] || 'watchdog.local';
const SECONDS = Number(process.argv[3] || 10);
const MIN_HZ = 8; // heartbeat alone is 0.5Hz; a healthy loop gives ~14Hz

let repo;
try {
    repo = execSync('git rev-parse --show-toplevel', { cwd: __dirname }).toString().trim();
} catch {
    repo = path.resolve(__dirname, '../../../..');
}
const secretsPath = path.join(repo, 'src/secrets.h');
if (!fs.existsSync(secretsPath)) {
    console.error('src/secrets.h not found — copy it from src/secrets.h.example');
    process.exit(1);
}
const token = fs.readFileSync(secretsPath, 'utf8').match(/#define\s+API_TOKEN\s+"([^"]+)"/)[1];

const sock = net.connect(80, HOST, () => {
    sock.write(
        `GET /ws HTTP/1.1\r\nHost: ${HOST}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n` +
        `Sec-WebSocket-Key: ${crypto.randomBytes(16).toString('base64')}\r\n` +
        `Sec-WebSocket-Version: 13\r\nAuthorization: Bearer ${token}\r\n\r\n`);
});

let handshake = false, buf = Buffer.alloc(0), frames = 0, t0 = 0;
const gaps = [];
let last = 0;

sock.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    if (!handshake) {
        const end = buf.indexOf('\r\n\r\n');
        if (end < 0) return;
        const head = buf.slice(0, buf.indexOf('\r\n')).toString();
        if (!head.includes('101')) { console.error(`handshake failed: ${head}`); process.exit(1); }
        buf = buf.slice(end + 4);
        handshake = true;
    }
    while (buf.length >= 2) {
        const opcode = buf[0] & 0x0f;
        let len = buf[1] & 0x7f, off = 2;
        if (len === 126) { len = buf.readUInt16BE(2); off = 4; }
        if (buf.length < off + len) return;
        const body = buf.slice(off, off + len).toString();
        buf = buf.slice(off + len);
        if (opcode !== 1 || !body.includes('"telemetry"')) continue;
        const now = Date.now();
        if (!t0) { t0 = now; } else { gaps.push(now - last); }
        last = now;
        frames++;
    }
});
sock.on('error', (e) => { console.error('error:', e.message); process.exit(1); });

setTimeout(() => {
    sock.destroy();
    if (!frames) { console.error('no telemetry frames received'); process.exit(1); }
    const secs = (Date.now() - t0) / 1000;
    const hz = frames / secs;
    const sorted = [...gaps].sort((a, b) => a - b);
    console.log(`  telemetry ${hz.toFixed(1)} Hz over ${secs.toFixed(1)}s ` +
        `(gap p50=${sorted[Math.floor(sorted.length / 2)] || 0}ms) — expect >= ${MIN_HZ} Hz`);
    if (hz < MIN_HZ) {
        console.error(`    ^ TELEMETRY RATE COLLAPSED — something is stalling the sensor loop`);
        process.exit(1);
    }
    process.exit(0);
}, SECONDS * 1000);
