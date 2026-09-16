"""Compile source-backed coast regressions on a host with Python 3 and GCC.

Run: python3 tests/run_coast_tests.py
Hardware peripherals are mocked; this does not verify electrical gate timing.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    return re.sub(r'^\s*#(?:include|pragma once)[^\n]*', '', (ROOT / path).read_text(), flags=re.M)


def function(text, name):
    start = re.search(r'^(?:static )?void ' + name + r'\([^)]*\)\s*\{', text, re.M).start()
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


main = source('Src/main.c')
coast = ''.join(function(main, name) for name in
                ('resetPidState', 'requestCoast', 'coastMotor', 'releaseCoast', 'stopCoastTracking'))
arm = function(main, 'tenKhzRoutine').split('    if (eepromBuffer.telemetry_on_interval)')[0]
arm = arm.replace('void tenKhzRoutine()', 'void armTick()') + '}\n'
fixture = (ROOT / 'tests/coast_fixture.c').read_text()
combined = fixture.replace('/* EEPROM */', source('Inc/eeprom.h'))
combined = combined.replace('/* DSHOT HEADER */', source('Inc/dshot.h'))
dma = function(source('Mcu/v203/Src/ch32v20x_it.c'), 'DMA1_Channel5_IRQHandler')
combined = combined.replace('/* PRODUCTION */', source('Src/dshot.c') + coast + source('Src/signal.c') + arm + dma)
with tempfile.TemporaryDirectory(prefix='am32-coast-') as temp:
    phase = (ROOT / 'tests/phase_fixture.c').read_text().replace(
        '/* PRODUCTION */', source('Mcu/v203/Src/phaseouts.c'))
    for name, code in [('coast', combined), ('phase', phase)]:
        c = Path(temp) / (name + '.c')
        exe = Path(temp) / (name + '-tests')
        c.write_text(code)
        subprocess.run(['gcc', '-std=gnu11', '-O2', '-Wall', '-Wextra',
                        '-Wno-sign-compare', str(c), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
