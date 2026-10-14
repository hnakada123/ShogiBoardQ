#!/usr/bin/env python3
"""Capture CSA guide screenshots using the built app and a local shogi-server.

Run under xvfb-run, e.g.:
  xvfb-run -a -s '-screen 0 1920x1440x24' python3 scripts/docs/shoot_csa.py \
    --server /path/to/shogi-server --languages ja_JP en zh_CN zh_TW

Settings and server records stay in a temporary directory. The floodgate image
only shows settings; this script never connects to a public server.
"""
import argparse
import asyncio
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import tempfile
import xml.etree.ElementTree as ET

REPO = Path(__file__).resolve().parents[2]
LANG_DIR = {'ja_JP': '', 'en': 'en', 'zh_CN': 'zh-cn', 'zh_TW': 'zh-tw'}


class Capture:
    def __init__(self, root, language):
        self.root, self.language = root, language
        self.name = 'csa-' + language
        self.sock = root / (self.name + '.sock')
        self.dest = REPO / 'docs/images' / LANG_DIR[language] / 'csa-game'

    async def rpc(self, method, **params):
        reader, writer = await asyncio.open_unix_connection(str(self.sock))
        try:
            writer.write((json.dumps(dict(jsonrpc='2.0', id=1, method=method, params=params))+'\n').encode())
            await writer.drain()
            reply = json.loads(await asyncio.wait_for(reader.readline(), 15))
            if 'error' in reply:
                raise RuntimeError(f'{method}: {reply["error"]}')
            await asyncio.sleep(.12)  # queued GUI operations
            return reply['result']
        finally:
            writer.close()
            await writer.wait_closed()

    async def dialog(self, cls):
        for _ in range(60):
            windows = (await self.rpc('dialog.list'))['windows']
            for w in windows:
                if w['class'] == cls:
                    return w['selector']
        raise RuntimeError(windows)

    async def set(self, target, **values):
        for widget, value in values.items():
            await self.rpc('widget.setValue', target=target, widget=widget, value=value)

    async def capture(self, name, target='main'):
        await asyncio.sleep(.25)
        result = await self.rpc('screenshot.capture', target=target, output_dir=str(self.root/'captures'))
        self.dest.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(result['path'], self.dest / name)
        print(f'{self.language}/{name}: {result["width"]}x{result["height"]}', flush=True)

    async def wait_state(self, **values):
        for _ in range(100):
            state = await self.rpc('app.state')
            if all(state.get(k) == v for k, v in values.items()):
                return
        raise RuntimeError(state)

    def translated(self, context, source):
        tree = ET.parse(REPO / f'resources/translations/ShogiBoardQ_{self.language}.ts')
        for ctx in tree.findall('context'):
            if ctx.findtext('name') == context:
                for message in ctx.findall('message'):
                    if message.findtext('source') == source:
                        return message.findtext('translation') or source
        return source

    async def run(self, port):
        await self.rpc('action.trigger', name='actionCSA')
        dialog = await self.dialog('CsaGameDialog')
        await self.set(dialog, lineEditHost='localhost', spinBoxPort=4081, lineEditId='Suzuki',
                       lineEditPassword='sample-trip', comboBoxTransport=0)
        await self.set(dialog, tabWidget=1)
        await self.set(dialog, lineEditGameName='test-600-10', comboBoxTurnPreference=1,
                       radioButtonGamesSingle=True, checkBoxReconnect=False, checkBoxAutoSave=False)
        await self.capture('csa-01-advanced.png', dialog)
        await self.set(dialog, tabWidget=0)
        await self.capture('csa-01-dialog.png', dialog)
        await self.set(dialog, comboBoxTransport=1, lineEditHost='csa.example.net')
        await self.capture('csa-01-tls.png', dialog)
        await self.set(dialog, comboBoxTransport=0, lineEditHost='wdoor.c.u-tokyo.ac.jp',
                       lineEditId='MyShogiEngine', lineEditPassword='floodgate-300-10F,sample-trip')
        await self.set(dialog, tabWidget=1)
        await self.set(dialog, lineEditGameName='', comboBoxTurnPreference=0)
        await self.set(dialog, tabWidget=0)
        await self.capture('csa-01-floodgate.png', dialog)
        # Only the local server is used for the waiting/game screenshots.
        await self.set(dialog, lineEditHost='localhost', spinBoxPort=port, lineEditId='Suzuki',
                       lineEditPassword='sample-trip')
        await self.set(dialog, tabWidget=1)
        await self.set(dialog, lineEditGameName='test-600-10', comboBoxTurnPreference=1)
        await self.set(dialog, tabWidget=0)
        await self.rpc('dialog.clickButton', dialog=dialog, widget='pushButtonStart')
        waiting = await self.dialog('CsaWaitingDialog')
        for _ in range(80):
            state = await self.rpc('app.state')
            if state.get('csa', {}).get('state') in ('WaitingForMatch',):
                break
        else:
            raise RuntimeError(state)
        await self.capture('csa-02-waiting.png', waiting)
        await self.rpc('dialog.clickButton', dialog=waiting, text=self.translated('CsaWaitingDialog', '通信ログ'))
        await self.capture('csa-02-commlog.png', 'csaWaitingLogWindow')
        await self.rpc('dialog.close', dialog='csaWaitingLogWindow')
        reader, writer = await asyncio.open_connection('127.0.0.1', port)
        async def send(line):
            writer.write((line+'\n').encode())
            await writer.drain()
        async def until(prefix):
            async with asyncio.timeout(15):
                while True:
                    line = (await reader.readline()).decode().strip()
                    if not line:
                        raise RuntimeError('server disconnected')
                    if line.startswith(prefix):
                        return
        try:
            await send('LOGIN Yamada test-600-10-w,sample-trip')
            await until('END Game_Summary')
            await send('AGREE')
            await until('START:')
            await self.wait_state(ui_state='csa_game')
            moves = [('7g7f', '+7776FU'), ('3c3d', '-3334FU'), ('2g2f', '+2726FU'),
                     ('8c8d', '-8384FU'), ('2f2e', '+2625FU'), ('8d8e', '-8485FU'),
                     ('6i7h', '+6978KI'), ('4a3b', '-4132KI')]
            for ply, (usi, csa) in enumerate(moves, 1):
                if ply % 2:
                    for file, rank in [(usi[0], usi[1]), (usi[2], usi[3])]:
                        await self.rpc('board.click', file=int(file), rank=ord(rank)-ord('a')+1)
                else:
                    await send(csa)
                await until(csa)
                await self.wait_state(total_plies=ply)
            docks = await self.rpc('dock.list')
            (self.root / (self.name+'-docks.json')).write_text(json.dumps(docks, ensure_ascii=False, indent=2))
            for dock in docks['docks']:
                if 'csa' in dock.get('object_name', '').lower():
                    await self.rpc('dock.configure', widget=dock['object_name'], operation='show')
            await self.capture('csa-03-playing.png')
            await self.rpc('action.trigger', name='actionResign')
            await until('#WIN')
            end = await self.dialog('QMessageBox')
            await self.capture('csa-04-game-end.png', end)
            await self.rpc('dialog.close', dialog=end)
        finally:
            writer.close()
            await writer.wait_closed()


async def main(args):
    root = Path(tempfile.mkdtemp(prefix='sbq-csa-guide-'))
    print(f'WORK={root}', flush=True)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    with (root/'server.log').open('w') as log:
        server = subprocess.Popen(['ruby', str(args.server.resolve()), '--least-time-per-move', '0', 'guide', str(port)],
                                  cwd=root, stdout=log, stderr=log)
        try:
            for _ in range(100):
                if server.poll() is not None:
                    raise RuntimeError((root/'server.log').read_text())
                try:
                    _, writer = await asyncio.open_connection('127.0.0.1', port)
                    writer.close()
                    await writer.wait_closed()
                    break
                except OSError:
                    await asyncio.sleep(.1)
            for language in args.languages:
                cap = Capture(root, language)
                env = dict(os.environ, SBQ_WORK=str(root), SBQ_SOCKET_DIR=str(root), SBQ_ENGINES='1')
                display = os.environ['DISPLAY'].lstrip(':').split('.')[0]
                subprocess.run(['bash', str(REPO/'scripts/docs/launch.sh'), cap.name, language, display, '1300', '980'],
                               env=env, check=True)
                try:
                    await cap.run(port)
                finally:
                    await cap.rpc('app.quit')
        finally:
            server.terminate()
            server.wait(timeout=5)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--languages', nargs='+', choices=LANG_DIR, default=list(LANG_DIR))
    asyncio.run(main(parser.parse_args()))
