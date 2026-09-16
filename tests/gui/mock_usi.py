#!/usr/bin/env python3
"""Deterministic USI protocol fixture; never represents real engine strength."""
import os
from pathlib import Path
import sys
import time

sequence = ['7g7f', '3c3d', '2g2f', '8c8d']
moves = []
pending = 'resign'
pondering = False
predict = os.environ.get('AUDIT_ENGINE_PONDER') == '1'
log = Path(os.environ.get('AUDIT_USI_LOG', str(Path(__file__).with_name('mock-usi-commands.log'))))


def bestmove():
    if moves and moves[-1] in ('8h2b+', '8h2b'):
        return 'bestmove 3a2b'
    if moves and moves[-1] in ('2b8h+', '2b8h'):
        return 'bestmove 7i8h'
    result = 'bestmove ' + pending
    if predict and len(moves) + 1 < len(sequence):
        predicted = sequence[len(moves) + 1]
        if os.environ.get('AUDIT_PONDER_PROMOTION') == '1':
            if len(moves) == 1:
                predicted = '8h2b+'
            elif len(moves) == 2:
                predicted = '2b8h+'
        result += ' ponder ' + predicted
    return result


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
        if pondering:
            with log.open('a') as f:
                f.write(f'{os.getpid()} ERROR go during ponder\n')
            continue
        pending = sequence[len(moves)] if len(moves) < len(sequence) else 'resign'
        pv = sequence[len(moves):] or ['7f7e']
        print(f'info depth 10 seldepth 12 time 100 nodes 1000 nps 10000 multipv 1 score cp 30 pv {" ".join(pv)}', flush=True)
        if command.startswith('go ponder'):
            pondering = True
        elif 'infinite' not in command and os.environ.get('AUDIT_ENGINE_WAIT_FOR_STOP') != '1':
            time.sleep(float(os.environ.get('AUDIT_ENGINE_DELAY', '0.1')))
            print(bestmove(), flush=True)
    elif command == 'ponderhit':
        if not pondering:
            with log.open('a') as f:
                f.write(f'{os.getpid()} ERROR ponderhit while idle\n')
            continue
        pondering = False
        time.sleep(float(os.environ.get('AUDIT_ENGINE_DELAY', '0.1')))
        print(bestmove(), flush=True)
    elif command == 'stop':
        print('bestmove resign' if pondering else bestmove(), flush=True)
        pondering = False
    elif command == 'quit':
        break
