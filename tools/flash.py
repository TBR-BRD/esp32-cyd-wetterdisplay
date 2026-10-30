#!/usr/bin/env python3
"""Build and flash the firmware and LittleFS assets to a connected CYD board."""
import argparse
import glob
from pathlib import Path
import shutil
import subprocess
import sys
from build import FQBN, ROOT, SKETCH_NAME

# Offset and size of the spiffs partition in the core's no_ota.csv.
LITTLEFS_OFFSET = '0x210000'
LITTLEFS_SIZE = 0x1E0000

def find_port():
    ports = sorted(glob.glob('/dev/cu.usbserial*') + glob.glob('/dev/cu.wchusbserial*') + glob.glob('/dev/cu.SLAB_USBtoUART*'))
    if len(ports) != 1:
        raise SystemExit(f'Expected one CYD serial port, found {ports or "none"}. Connect the board or pass --port.')
    return ports[0]

def find_tool(cli, name):
    data_dir = subprocess.run([cli, 'config', 'get', 'directories.data'], check=True, capture_output=True, text=True).stdout.strip()
    matches = glob.glob(f'{data_dir}/packages/esp32/tools/*/*/{name}')
    if not matches:
        raise SystemExit(f'{name} not found. Install the esp32:esp32@3.2.0 core first.')
    return sorted(matches)[-1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Serial port, detected automatically if omitted')
    parser.add_argument('--firmware-only', action='store_true', help='Skip the LittleFS assets')
    args = parser.parse_args()
    cli = shutil.which('arduino-cli')
    if not cli:
        raise SystemExit('Install Arduino CLI first.')
    sketch = ROOT / SKETCH_NAME
    secrets = sketch / 'Secrets.h'
    if not secrets.exists() or 'YOUR_' in secrets.read_text():
        raise SystemExit(f'Fill in your Wi-Fi credentials and API key in {secrets} first.')
    port = args.port or find_port()

    subprocess.run([sys.executable, str(ROOT / 'tools' / 'build.py')], check=True)
    subprocess.run([cli, 'upload', '--fqbn', FQBN, '--port', port, '--input-dir', str(ROOT / '.build' / 'firmware'), str(sketch)], check=True)
    if args.firmware_only:
        return
    image = ROOT / '.build' / 'littlefs.bin'
    subprocess.run([find_tool(cli, 'mklittlefs'), '-c', str(sketch / 'data'), '-p', '256', '-b', '4096', '-s', str(LITTLEFS_SIZE), str(image)], check=True)
    subprocess.run([find_tool(cli, 'esptool'), '--chip', 'esp32', '--port', port, '--baud', '115200', 'write_flash', LITTLEFS_OFFSET, str(image)], check=True)
    print('Flash complete. Watch the serial output with: arduino-cli monitor --port', port, '--config 115200')

if __name__ == '__main__':
    main()
