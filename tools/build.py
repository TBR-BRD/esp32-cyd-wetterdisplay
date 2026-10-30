#!/usr/bin/env python3
"""Compile this source release using its bundled, hash-verified libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent
SKETCH_NAME = 'Esp32_CYD_TFT_eSPI_OpenWeather_LittleFS_v02'
FQBN = 'esp32:esp32:esp32:PartitionScheme=no_ota,FlashMode=dio,FlashFreq=40,FlashSize=4M,UploadSpeed=115200,PSRAM=disabled'

def unpack_libraries():
    destination = ROOT / '.build' / 'libraries'
    destination.mkdir(parents=True, exist_ok=True)
    records = json.loads((ROOT / 'libraries' / 'manifest.json').read_text())
    for record in records:
        archive = ROOT / record['archive']
        if hashlib.sha256(archive.read_bytes()).hexdigest() != record['sha256']:
            raise RuntimeError(f'Library checksum mismatch: {archive.name}')
        with zipfile.ZipFile(archive) as bundle:
            for entry in bundle.infolist():
                output = (destination / entry.filename).resolve()
                if not output.is_relative_to(destination.resolve()):
                    raise RuntimeError(f'Unsafe archive path: {entry.filename}')
            bundle.extractall(destination)
    return destination

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', default=shutil.which('arduino-cli'), help='Path to Arduino CLI')
    args = parser.parse_args()
    if not args.cli:
        parser.error('Install Arduino CLI, or pass --cli /path/to/arduino-cli')
    libraries = unpack_libraries()
    subprocess.run([args.cli, 'compile', '--fqbn', FQBN, '--libraries', str(libraries),
                    '--build-path', str(ROOT / '.build' / 'firmware'), '--jobs', '4',
                    str(ROOT / SKETCH_NAME)], check=True, cwd=ROOT)
    print('Build passed. Firmware is in .build/firmware. Upload data/ separately using the IDE LittleFS uploader.')

if __name__ == '__main__':
    main()
