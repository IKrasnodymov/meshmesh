// Runs the site's browser installer code (site/nrf52dfu.js) against a real bootloader: the serial
// port is reached through a raw TCP bridge (tools/nrf52_dfu_check.py starts it).
// node tools/nrf52_dfu_check.mjs TCP_PORT firmware.bin firmware.dat
import net from 'net';
import fs from 'fs';
import { dfuFlash } from '../site/nrf52dfu.js';
const [port, bin, dat] = process.argv.slice(2);
const socket = net.connect(+port, '127.0.0.1');
await new Promise((ok, fail) => { socket.once('connect', ok); socket.once('error', fail); });
const queue = [];let wake = null;
socket.on('data', d => { queue.push(...d); if (wake) { wake(); wake = null; } });
const io = {
  write: bytes => new Promise(ok => socket.write(Buffer.from(bytes), ok)),
  read: timeout => new Promise(ok => {
    const take = () => ok(Uint8Array.from(queue.splice(0)));
    if (queue.length) return take();
    const t = setTimeout(() => { wake = null; take(); }, timeout);
    wake = () => { clearTimeout(t); take(); };
  }),
};
let last = -1;
await dfuFlash(io, new Uint8Array(fs.readFileSync(bin)), new Uint8Array(fs.readFileSync(dat)), (part, stage) => {
  const pct = Math.floor(part * 10) * 10;
  if (stage !== 'write' || pct !== last) { last = pct; console.log(stage, pct + '%'); }
});
socket.end();
console.log('OK written by site/nrf52dfu.js');
