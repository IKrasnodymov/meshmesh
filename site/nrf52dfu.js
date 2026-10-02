// Firmware install for nRF52 boards (GAT562) from the browser: the Nordic legacy serial DFU of the
// Adafruit nRF52 bootloader, as adafruit-nrfutil speaks it ("dfu serial --singlebank"). SLIP-framed
// HCI packets with CRC16; each packet is acknowledged. Only the application is written: MeshMesh
// storage and the InternalFS stay. I/O is abstract so the same code is checked from a computer
// (tools/nrf52_dfu_check.cjs) before it runs on Web Serial.

const HCI_TYPE = 14, START = 3, INIT = 1, DATA = 4, STOP = 5, MODE_APP = 4, CHUNK = 512;
const PAGE = 4096, PAGE_ERASE = 0.0897, PAGE_WRITE = (PAGE / 4) * 0.0001; // seconds, as nrfutil

export function crc16(bytes, crc = 0xffff) {
  for (const b of bytes) {
    crc = ((crc >> 8) & 0xff) | ((crc << 8) & 0xff00);
    crc ^= b;
    crc ^= (crc & 0xff) >> 4;
    crc ^= (crc << 12) & 0xffff;
    crc ^= ((crc & 0xff) << 5) & 0xffff;
  }
  return crc & 0xffff;
}
const u32 = v => [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];

// One HCI packet for sequence number seq (1..7, then 0): header, payload, CRC16, SLIP escapes.
export function hciPacket(seq, payload) {
  const n = payload.length;
  const head = [seq | (((seq + 1) % 8) << 3) | (1 << 6) | (1 << 7), HCI_TYPE | ((n & 0x0f) << 4), (n & 0xff0) >> 4];
  head.push((~(head[0] + head[1] + head[2]) + 1) & 0xff);
  const body = [...head, ...payload];
  const crc = crc16(body);
  body.push(crc & 0xff, crc >> 8);
  const out = [0xc0];
  for (const b of body) out.push(...(b === 0xc0 ? [0xdb, 0xdc] : b === 0xdb ? [0xdb, 0xdd] : [b]));
  out.push(0xc0);
  return Uint8Array.from(out);
}

// All packets of an application update, in order, with their sequence numbers (as nrfutil numbers them).
export function dfuPackets(firmware, initPacket) {
  let seq = 0;
  const next = payload => hciPacket(seq = (seq + 1) % 8, payload);
  const packets = [{ kind: 'start', data: next([...u32(START), ...u32(MODE_APP), ...u32(0), ...u32(0), ...u32(firmware.length)]) },
                   { kind: 'init', data: next([...u32(INIT), ...initPacket, 0, 0]) }];
  for (let i = 0; i < firmware.length; i += CHUNK) packets.push({ kind: 'data', data: next([...u32(DATA), ...firmware.subarray(i, i + CHUNK)]) });
  packets.push({ kind: 'stop', data: next(u32(STOP)) });
  return packets;
}

const sleep = ms => new Promise(r => setTimeout(r, ms));

// io: { write(Uint8Array), read(timeoutMs) -> Uint8Array (possibly empty) }.
async function ack(io, buffered) {
  const end = Date.now() + 2000;
  while (Date.now() < end) {
    const marks = buffered.bytes.reduce((n, b) => n + (b === 0xc0), 0);
    if (marks >= 2) { buffered.bytes = []; return; }
    const chunk = await io.read(100);
    if (chunk.length) buffered.bytes.push(...chunk);
  }
  throw new Error('No acknowledgement from the bootloader');
}

export async function dfuFlash(io, firmware, initPacket, onProgress = () => {}) {
  const packets = dfuPackets(firmware, initPacket), data = packets.filter(p => p.kind === 'data').length;
  const buffered = { bytes: [] };
  let sent = 0;
  for (const p of packets) {
    await io.write(p.data);
    await ack(io, buffered);
    if (p.kind === 'start') { // the bootloader erases the application area first
      onProgress(0, 'erase');
      await sleep(Math.max(0.5, (Math.floor(firmware.length / PAGE) + 1) * PAGE_ERASE) * 1000);
    }
    if (p.kind === 'data') {
      sent++;
      if (sent % 8 === 1) await sleep(PAGE_WRITE * 1000); // a 4 KB page is written every 8 packets
      onProgress(sent / data, 'write');
    }
    if (p.kind === 'stop') await sleep((PAGE_ERASE + PAGE_WRITE) * 1000); // bootloader settings page
  }
  onProgress(1, 'done');
}

// Web Serial: the running MeshMesh firmware enters the serial bootloader on a 1200-baud "touch"
// (open at 1200 baud, raise and drop DTR). The bootloader then appears as another USB device.
export async function touch1200(port) {
  await port.open({ baudRate: 1200 });
  try {
    await port.setSignals({ dataTerminalReady: true });
    await sleep(100);
    await port.setSignals({ dataTerminalReady: false });
    await sleep(100);
  } finally {
    await port.close().catch(() => {});
  }
}

export async function webSerialIo(port) {
  await port.open({ baudRate: 115200, bufferSize: 4096 });
  await port.setSignals({ dataTerminalReady: true });
  const writer = port.writable.getWriter(), reader = port.readable.getReader();
  let pending = null;
  return {
    async write(bytes) { await writer.write(bytes); },
    async read(timeout) {
      pending = pending || reader.read();
      const result = await Promise.race([pending, sleep(timeout).then(() => null)]);
      if (!result) return new Uint8Array();
      pending = null;
      return result.value || new Uint8Array();
    },
    async close() {
      await reader.cancel().catch(() => {}); reader.releaseLock(); writer.releaseLock();
      await port.close().catch(() => {});
    },
  };
}

// The application's USB product ID has the 0x8000 bit (Adafruit core); the bootloader's has not.
export const isBootloader = port => {
  const info = port.getInfo();
  return info.usbVendorId === 0x239a && !(info.usbProductId & 0x8000);
};

// The device language. The image keeps a 16-byte "MMLANG:--" field (src/I18n.cpp); the site writes the
// chosen language code there, so the first start after this install shows its menus.
const MARK = [...'MMLANG:--'].map(c => c.charCodeAt(0)), FIELD = 16, APP_BASE = 0x26000;
function markAt(bin) {
  let found = -1;
  for (let i = 0; i + MARK.length <= bin.length; i++) {
    let k = 0;
    while (k < MARK.length && bin[i + k] === MARK[k]) k++;
    if (k === MARK.length) { if (found >= 0) throw new Error('two language fields in the image'); found = i; }
  }
  if (found < 0) throw new Error('no language field in the image');
  return found;
}
const field = code => { const f = new Uint8Array(FIELD); f.set([...`MMLANG:${code}`].map(c => c.charCodeAt(0))); return f; };

// firmware.bin and its init packet (device type … CRC16 of the image in the last two bytes).
export function withLanguage(bin, dat, code) {
  const out = bin.slice(), at = markAt(bin);
  out.set(field(code), at);
  const init = dat.slice(), crc = crc16(out);
  init[init.length - 2] = crc & 0xff; init[init.length - 1] = crc >> 8;
  if (crc16(bin) !== (dat[dat.length - 2] | (dat[dat.length - 1] << 8))) throw new Error('init packet does not match the image');
  return [out, init];
}

// firmware.uf2: 512-byte blocks, each with its flash address (offset 12), size (16) and data (32).
export function uf2WithLanguage(uf2, code) {
  const out = uf2.slice(), view = new DataView(out.buffer);
  const app = [];
  for (let b = 0; b + 512 <= out.length; b += 512) {
    const addr = view.getUint32(b + 12, true), size = view.getUint32(b + 16, true);
    for (let i = 0; i < size; i++) app[addr - APP_BASE + i] = b + 32 + i;
  }
  const bytes = Uint8Array.from(app, i => out[i] ?? 0xff), at = markAt(bytes), f = field(code);
  for (let i = 0; i < FIELD; i++) out[app[at + i]] = f[i];
  return out;
}
