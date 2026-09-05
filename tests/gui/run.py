#!/usr/bin/env python3
"""Run each Qt GUI scenario in a separate process and retain its evidence."""
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys

audit = Path(__file__).resolve().parents[2] / 'build/gui-audit'
env = os.environ.copy()
env.update(XDG_CONFIG_HOME=str(audit / 'config'),
           XDG_DATA_HOME=str(audit / 'data'),
           XDG_CACHE_HOME=str(audit / 'cache'), QT_QPA_PLATFORM='xcb')
names = sys.argv[1:] or [s.removesuffix('()') for s in subprocess.check_output(
    [str(audit / 'test-build/tst_gui_functional'), '-functions'], env=env, text=True).splitlines()
    if s.endswith('()')]
results = []
actions = set()
for name in names:
    logfile = audit / (name.replace(':', '-') + '.log')
    xmlfile = logfile.with_suffix('.xml')
    usilog = logfile.with_suffix('.usi.log')
    usilog.write_text('')
    env['AUDIT_USI_LOG'] = str(usilog)
    if name in ('engineStopMate', 'engineImmediateMove', 'engineCancelAnalysis'):
        env['AUDIT_ENGINE_DELAY'] = '1.2'
    else:
        env.pop('AUDIT_ENGINE_DELAY', None)
    with logfile.with_suffix('.stderr').open('w') as stderr:
        proc = subprocess.Popen([str(audit / 'test-build/tst_gui_functional'), name,
            '-o', f'{logfile},txt', '-o', f'{xmlfile},junitxml'], env=env,
            stdout=stderr, stderr=stderr, start_new_session=True)
        try:
            code = proc.wait(timeout=35)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait()
            code = 124
    log = logfile.read_text() if logfile.exists() else ''
    evidence = [s for s in log.splitlines() if re.match(r'(PASS|FAIL|SKIP|Totals)', s)]
    item = {'scenario': name, 'exit_code': code, 'evidence': evidence}
    results.append(item)
    print(json.dumps(item, ensure_ascii=False), flush=True)
    f = audit / 'clicked-actions.json'
    if f.exists():
        actions.update(json.loads(f.read_text()))
(audit / 'run-results.json').write_text(json.dumps(results, ensure_ascii=False, indent=2))
(audit / 'all-clicked-actions.json').write_text(json.dumps(sorted(actions), indent=2))
sys.exit(any(r['exit_code'] for r in results))
