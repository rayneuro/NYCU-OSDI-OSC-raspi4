#!/usr/bin/env python3
import tempfile
import os
import shutil
from pathlib import Path
from vm_qemu_test import Machine
QEMU=os.environ.get('QEMU') or shutil.which('qemu-system-aarch64') or '/home/ray/qemu-11.0.0-rc1/build/qemu-system-aarch64'
Path('tests/results').mkdir(exist_ok=True)
with tempfile.TemporaryDirectory() as tmp:
	m=Machine(QEMU,tmp)
	try:
		m.launch('/initramfs/vfs1.img')
		m.until(lambda b: b'This is user process' in b or b'Process start' in b)
		m.send(b'vfs\r')
		data=m.until(lambda b: b"press 'x'" in b)
		m.send(b'x')
		data+=m.until(lambda b: b'Test End' in b)
		assert b'failed' not in data,data
		print(data.decode(errors='replace'))
		m.send(b'fork\r')
		m.until(lambda b: b'Child pid:' in b)
		first=m.screenshot('lab7-frame-0')
		import time
		time.sleep(.4)
		second=m.screenshot('lab7-frame-1')
		assert first != second
		print('Official framebuffer video changed: PASS')
		for shot in Path(tmp).glob('lab7-frame-*.ppm'):
			shutil.copyfile(shot,Path('tests/results')/shot.name)
	finally:
		m.proc.terminate();m.proc.wait(timeout=5)
		Path('tests/results').mkdir(exist_ok=True)
		Path('tests/results/official-vfs-serial.log').write_bytes(m.transcript)
