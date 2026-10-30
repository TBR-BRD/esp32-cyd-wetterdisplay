#!/usr/bin/env python3
"""Run the pure C++ model and JSON-fixture regression tests (g++ required)."""
from pathlib import Path
import shutil
import subprocess
from build import ROOT, SKETCH_NAME, unpack_libraries

compiler = shutil.which('g++')
if not compiler:
    raise SystemExit('Install g++ to run these optional host tests.')
libraries = unpack_libraries()
sketch = ROOT / SKETCH_NAME
for name, sources in [
    ('weather_model_test', ['WeatherModel.cpp']),
    ('weather_json_test', ['WeatherJson.cpp', 'WeatherModel.cpp']),
]:
    binary = ROOT / '.build' / name
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-I', str(sketch), '-I', str(libraries / 'ArduinoJson' / 'src'),
                    str(ROOT / 'tests' / (name + '.cpp')),
                    *[str(sketch / source) for source in sources], '-o', str(binary)],
                   cwd=ROOT, check=True)
    subprocess.run([str(binary)], cwd=ROOT, check=True)
