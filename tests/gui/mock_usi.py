#!/usr/bin/env python3
"""Deterministic USI protocol fixture; never represents real engine strength."""
import os
from pathlib import Path
import sys
import time

sequence = ['7g7f', '3c3d', '2g2f', '8c8d']
moves = []
pending = 'resign'
log = Path(os.environ.get('AUDIT_USI_LOG', str(Path(__file__).with_name('mock-usi-commands.log'))))
for raw in sys.stdin:
    command = raw.strip()
    with log.open('a') as f:
        f.write(f'{os.getpid()} {command}\n')
    if command == 'usi':
        print('id name Audit USI\nid author GUI audit fixture\noption name MultiPV type spin default 1 min 1 max 5\nusiok', flush=True)
    elif command == 'isready':
        print('readyok', flush=True)
    elif command.startswith('position '):
        moves = command.split(' moves ', 1)[1].split() if ' moves ' in command else []
    elif command.startswith('go mate'):
        time.sleep(float(os.environ.get('AUDIT_ENGINE_DELAY', '0.05')))
        print('checkmate nomate', flush=True)
    elif command.startswith('go'):
        pending = sequence[len(moves)] if len(moves) < len(sequence) else 'resign'
        pv = sequence[len(moves):] or ['7f7e']
        print(f'info depth 10 seldepth 12 time 100 nodes 1000 nps 10000 multipv 1 score cp 30 pv {" ".join(pv)}', flush=True)
        if 'infinite' not in command:
            time.sleep(float(os.environ.get('AUDIT_ENGINE_DELAY', '0.1')))
            print('bestmove ' + pending, flush=True)
    elif command == 'stop':
        print('bestmove ' + pending, flush=True)
    elif command == 'quit':
        break
