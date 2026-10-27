// SPDX-License-Identifier: GPL-3.0-or-later
// Generates the chess piece icons for the Teensy chess sketch.
//
// Each piece is described with simple vector shapes on a 26 x 26 grid, rasterized
// with 8x8 supersampling and stored as a 2-bit-per-pixel mask:
//   0 = transparent, 1 = body, 2 = outline / detail line
// The sketch colors the body and outline differently for white and black pieces.
//
// Usage: node make_icons.js <output pieces.h> [preview.png]

'use strict';
const fs = require('fs');
const zlib = require('zlib');

const SIZE = 26;   // icon size in pixels
const SS = 8;      // supersampling factor

// ---------- shapes ----------
const rect = (x0, y0, x1, y1) => (x, y) => x >= x0 && x <= x1 && y >= y0 && y <= y1;
const circle = (cx, cy, r) => (x, y) => (x - cx) ** 2 + (y - cy) ** 2 <= r * r;
const ellipse = (cx, cy, rx, ry) => (x, y) => ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1;
const poly = (pts) => (x, y) => {
  let inside = false;
  for (let i = 0, j = pts.length - 1; i < pts.length; j = i++) {
    const [xi, yi] = pts[i], [xj, yj] = pts[j];
    if ((yi > y) !== (yj > y) && x < ((xj - xi) * (y - yi)) / (yj - yi) + xi) inside = !inside;
  }
  return inside;
};
// Thick line segment (for detail strokes)
const line = (x0, y0, x1, y1, w) => (x, y) => {
  const dx = x1 - x0, dy = y1 - y0;
  const t = Math.max(0, Math.min(1, ((x - x0) * dx + (y - y0) * dy) / (dx * dx + dy * dy)));
  const px = x0 + t * dx, py = y0 + t * dy;
  return (x - px) ** 2 + (y - py) ** 2 <= (w / 2) ** 2;
};

const base = rect(5, 21.5, 21, 24.5);

// body: shapes that form the silhouette; cut: shapes removed from it;
// detail: lines drawn in the outline color inside the body
const PIECES = {
  pawn: {
    body: [
      rect(5.5, 21.5, 20.5, 24.5),
      poly([[8, 21.6], [18, 21.6], [15.2, 13.6], [10.8, 13.6]]),
      ellipse(13, 13.6, 4.7, 1.7),
      circle(13, 8.6, 4.1),
    ],
    cut: [],
    detail: [],
  },
  knight: {
    body: [
      base,
      poly([
        [7.6, 21.6], [8.2, 19.0], [10.6, 16.2], [12.4, 13.4], [10.0, 13.9], [7.2, 15.1],
        [5.0, 14.4], [4.2, 12.6], [5.4, 10.2], [8.4, 7.0], [10.8, 5.2], [11.8, 2.6],
        [13.6, 4.4], [16.8, 5.8], [19.0, 8.8], [20.2, 13.0], [20.2, 21.6],
      ]),
    ],
    cut: [],
    detail: [
      circle(10.0, 8.5, 0.95),
      line(5.6, 12.4, 6.6, 12.0, 0.9),
      line(14.6, 5.6, 18.2, 9.8, 0.9),
      line(18.2, 9.8, 18.9, 15.0, 0.9),
    ],
  },
  bishop: {
    body: [
      base,
      poly([[8.5, 21.6], [17.5, 21.6], [15.5, 17.0], [10.5, 17.0]]),
      ellipse(13, 17.0, 5.2, 1.6),
      ellipse(13, 11.6, 4.9, 5.4),
      poly([[9.0, 9.0], [17.0, 9.0], [13.0, 4.0]]),
      circle(13, 3.3, 2.0),
    ],
    cut: [],
    detail: [
      line(15.0, 7.8, 11.8, 11.8, 1.1),
      line(9.0, 17.0, 17.0, 17.0, 0.9),
    ],
  },
  rook: {
    body: [
      base,
      rect(6.5, 19.5, 19.5, 21.6),
      poly([[8.0, 19.6], [18.0, 19.6], [17.3, 10.4], [8.7, 10.4]]),
      rect(6.5, 5.0, 19.5, 10.5),
    ],
    cut: [rect(9.2, 4.0, 11.4, 7.6), rect(14.6, 4.0, 16.8, 7.6)],
    detail: [
      line(8.7, 10.6, 17.3, 10.6, 0.9),
      line(8.0, 19.6, 18.0, 19.6, 0.9),
    ],
  },
  queen: {
    body: [
      base,
      poly([
        [7.0, 21.6], [19.0, 21.6], [21.6, 8.6], [17.9, 12.6], [17.0, 6.4], [14.6, 11.6],
        [13.0, 5.2], [11.4, 11.6], [9.0, 6.4], [8.1, 12.6], [4.4, 8.6],
      ]),
      circle(4.4, 7.4, 1.9),
      circle(9.0, 5.4, 1.9),
      circle(13.0, 4.3, 1.95),
      circle(17.0, 5.4, 1.9),
      circle(21.6, 7.4, 1.9),
    ],
    cut: [],
    detail: [line(7.4, 18.8, 18.6, 18.8, 0.9)],
  },
  king: {
    body: [
      base,
      poly([
        [7.0, 21.6], [19.0, 21.6], [21.0, 13.8], [18.6, 10.8], [13.0, 12.8],
        [7.4, 10.8], [5.0, 13.8],
      ]),
      circle(9.0, 14.2, 4.0),
      circle(17.0, 14.2, 4.0),
      rect(11.4, 1.0, 14.6, 12.2),
      rect(9.2, 3.0, 16.8, 6.2),
    ],
    cut: [],
    detail: [line(7.4, 18.8, 18.6, 18.8, 0.9), line(13.0, 13.4, 13.0, 18.6, 0.9)],
  },
};

// ---------- rasterize ----------
function coverage(shapes, cut, px, py) {
  let n = 0;
  for (let sy = 0; sy < SS; sy++)
    for (let sx = 0; sx < SS; sx++) {
      const x = px + (sx + 0.5) / SS, y = py + (sy + 0.5) / SS;
      if (shapes.some((f) => f(x, y)) && !cut.some((f) => f(x, y))) n++;
    }
  return n / (SS * SS);
}

function rasterize(p) {
  const body = [], detail = [];
  for (let y = 0; y < SIZE; y++)
    for (let x = 0; x < SIZE; x++) {
      body.push(coverage(p.body, p.cut, x, y) >= 0.5);
      detail.push(p.detail.length ? coverage(p.detail, [], x, y) >= 0.35 : false);
    }
  const at = (x, y) => x >= 0 && y >= 0 && x < SIZE && y < SIZE && body[y * SIZE + x];
  const px = [];
  for (let y = 0; y < SIZE; y++)
    for (let x = 0; x < SIZE; x++) {
      const i = y * SIZE + x;
      if (!body[i]) px.push(0);
      else if (!at(x - 1, y) || !at(x + 1, y) || !at(x, y - 1) || !at(x, y + 1) || detail[i]) px.push(2);
      else px.push(1);
    }
  return px;
}

const ORDER = ['pawn', 'knight', 'bishop', 'rook', 'queen', 'king'];
const icons = ORDER.map((n) => rasterize(PIECES[n]));

// ---------- C header ----------
function pack(px) {
  const bytes = [];
  for (let i = 0; i < px.length; i += 4) {
    let b = 0;
    for (let k = 0; k < 4; k++) b |= (px[i + k] || 0) << (6 - 2 * k);
    bytes.push(b);
  }
  return bytes;
}

let h = '';
h += '// SPDX-License-Identifier: GPL-3.0-or-later\n';
h += '// Chess piece icons, generated by tools/make_icons.js - do not edit by hand.\n';
h += `// ${SIZE} x ${SIZE} pixels, 2 bits per pixel, 4 pixels per byte, MSB first, row major.\n`;
h += '// Pixel values: 0 = transparent, 1 = body, 2 = outline.\n\n';
h += '#pragma once\n\n#include <Arduino.h>\n\n';
h += `#define PIECE_ICON_SIZE ${SIZE}\n`;
h += `#define PIECE_ICON_BYTES ${Math.ceil((SIZE * SIZE) / 4)}\n\n`;
h += '// Indexed by piece type - 1 (pawn, knight, bishop, rook, queen, king)\n';
h += 'static const uint8_t PIECE_ICONS[6][PIECE_ICON_BYTES] PROGMEM = {\n';
icons.forEach((px, i) => {
  const bytes = pack(px);
  h += `  { // ${ORDER[i]}\n`;
  for (let j = 0; j < bytes.length; j += 16)
    h += '    ' + bytes.slice(j, j + 16).map((b) => '0x' + b.toString(16).padStart(2, '0')).join(', ') + ',\n';
  h += '  },\n';
});
h += '};\n';
fs.writeFileSync(process.argv[2], h);

// ---------- ASCII preview ----------
icons.forEach((px, i) => {
  console.log(ORDER[i]);
  for (let y = 0; y < SIZE; y++) {
    let s = '';
    for (let x = 0; x < SIZE; x++) s += ['.', '#', 'o'][px[y * SIZE + x]];
    console.log(s);
  }
});

// ---------- PNG preview ----------
if (process.argv[3]) {
  const SQ = 30, SCALE = 4;
  const light = [192, 192, 192], dark = [60, 100, 200];
  const W = 6 * SQ, H = 2 * SQ;
  const img = Buffer.alloc(W * H * 3);
  const put = (x, y, c) => { const o = (y * W + x) * 3; img[o] = c[0]; img[o + 1] = c[1]; img[o + 2] = c[2]; };
  for (let row = 0; row < 2; row++)
    for (let col = 0; col < 6; col++) {
      const bg = (row + col) % 2 ? dark : light;
      const white = row === 0;
      const bodyC = white ? [255, 255, 255] : [0, 0, 0];
      const lineC = white ? [0, 0, 0] : [150, 150, 150];
      const off = (SQ - SIZE) / 2;
      for (let y = 0; y < SQ; y++)
        for (let x = 0; x < SQ; x++) {
          let c = bg;
          const ix = x - off, iy = y - off;
          if (ix >= 0 && iy >= 0 && ix < SIZE && iy < SIZE) {
            const v = icons[col][iy * SIZE + ix];
            if (v === 1) c = bodyC;
            else if (v === 2) c = lineC;
          }
          put(col * SQ + x, row * SQ + y, c);
        }
    }
  // scale up
  const SW = W * SCALE, SH = H * SCALE;
  const raw = Buffer.alloc((SW * 3 + 1) * SH);
  for (let y = 0; y < SH; y++) {
    raw[y * (SW * 3 + 1)] = 0;
    for (let x = 0; x < SW; x++) {
      const so = (Math.floor(y / SCALE) * W + Math.floor(x / SCALE)) * 3;
      const d = y * (SW * 3 + 1) + 1 + x * 3;
      raw[d] = img[so]; raw[d + 1] = img[so + 1]; raw[d + 2] = img[so + 2];
    }
  }
  const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  const crc = (buf) => { let c = 0xffffffff; for (const b of buf) c = crcTable[(c ^ b) & 0xff] ^ (c >>> 8); return (c ^ 0xffffffff) >>> 0; };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const c = Buffer.alloc(4); c.writeUInt32BE(crc(td));
    return Buffer.concat([len, td, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(SW, 0); ihdr.writeUInt32BE(SH, 4); ihdr[8] = 8; ihdr[9] = 2;
  const png = Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0)),
  ]);
  fs.writeFileSync(process.argv[3], png);
}
