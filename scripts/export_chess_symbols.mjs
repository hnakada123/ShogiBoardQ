#!/usr/bin/env node
// 承認済みHTMLの配置・赤色フィルターをそのまま透明PNGに書き出す。
// Node.js 22+ / Chromium。出力先を指定し、続いて generate_chess_pieces.py を実行する。
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';
import {spawn} from 'node:child_process';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = process.argv[2];
if (!output) throw new Error('Usage: node scripts/export_chess_symbols.mjs OUTPUT_DIRECTORY');
const profile = await fs.mkdtemp(path.join(os.tmpdir(), 'shogi-chess-export-'));
const browser = spawn(process.env.CHROMIUM || 'chromium', [
  '--headless', '--disable-gpu', '--no-first-run', '--no-default-browser-check',
  '--remote-debugging-port=0', `--user-data-dir=${profile}`, 'about:blank'
], {stdio:'ignore'});
let ws;
try {
  let port;
  for (let attempt = 0; attempt < 100; ++attempt) {
    try { port = (await fs.readFile(path.join(profile, 'DevToolsActivePort'), 'utf8')).split('\n')[0]; break; }
    catch { await new Promise(resolve => setTimeout(resolve, 100)); }
  }
  if (!port) throw new Error('Chromium did not start');
  const pages = await (await fetch(`http://127.0.0.1:${port}/json`)).json();
  ws = new WebSocket(pages.find(page => page.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  let serial = 0;
  const pending = new Map();
  ws.onmessage = event => {
    const result = JSON.parse(event.data);
    const request = pending.get(result.id);
    if (request) {
      pending.delete(result.id);
      result.error ? request.reject(result.error) : request.resolve(result.result);
    }
  };
  const call = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++serial;
    pending.set(id, {resolve, reject});
    ws.send(JSON.stringify({id, method, params}));
  });
  const evaluate = async expression => {
    const result = await call('Runtime.evaluate', {expression, returnByValue:true, awaitPromise:true});
    if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  await call('Page.navigate', {url:pathToFileURL(path.join(repo, 'design/chess-shogi/index.html')).href});
  for (let attempt = 0; attempt < 100; ++attempt) {
    if (await evaluate('typeof piece === "function" && document.readyState === "complete"')) break;
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  const sources = {};
  for (const design of ['facet', 'atelier', 'ribbon']) {
    for (const suffix of ['', '-pawn-lance-v2']) {
      const name = `${design}${suffix}.png`;
      sources[`assets/${name}`] = 'data:image/png;base64,' +
        (await fs.readFile(path.join(repo, 'design/chess-shogi/assets', name))).toString('base64');
    }
  }
  await evaluate(`window.exportSources = ${JSON.stringify(sources)}`);
  await fs.mkdir(output, {recursive:true});
  for (const design of ['facet', 'atelier', 'ribbon']) {
    for (const role of ['K','R','B','G','S','N','L','P']) {
      for (const promoted of ['K','G'].includes(role) ? [false] : [false, true]) {
        const data = await evaluate(`(async () => {
          const container = document.createElement('div');
          container.innerHTML = piece(${JSON.stringify(role)}, {design:${JSON.stringify(design)}, promoted:${promoted}});
          const ink = container.querySelector('.piece-ink');
          for (const image of ink.querySelectorAll('image')) image.setAttribute('href', window.exportSources[image.getAttribute('href')]);
          const defs = document.getElementById('promoted-flat').parentElement.outerHTML;
          const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="720" height="720" viewBox="0 0 45 45">'
            + '<style>.symbol{display:block;overflow:hidden}</style>' + defs + ink.outerHTML + '</svg>';
          const image = new Image();
          image.src = 'data:image/svg+xml;charset=utf-8,' + encodeURIComponent(svg);
          await image.decode();
          const canvas = document.createElement('canvas');
          canvas.width = canvas.height = 720;
          canvas.getContext('2d').drawImage(image, 0, 0);
          return canvas.toDataURL('image/png').split(',')[1];
        })()`);
        await fs.writeFile(path.join(output, `${design}-${role}${promoted ? '-promoted' : ''}.png`), Buffer.from(data, 'base64'));
      }
    }
    console.log(`${design}: 14 symbols exported`);
  }
} finally {
  ws?.close();
  browser.kill();
  await new Promise(resolve => browser.once('exit', resolve));
  await fs.rm(profile, {recursive:true, force:true});
}
