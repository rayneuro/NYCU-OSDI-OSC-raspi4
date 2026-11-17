#!/usr/bin/env python3
import tempfile
import os
import shutil
from pathlib import Path
from vm_qemu_test import Machine
with tempfile.TemporaryDirectory() as tmp:
	m=Machine(os.environ.get('QEMU') or shutil.which('qemu-system-aarch64') or '/home/ray/qemu-11.0.0-rc1/build/qemu-system-aarch64',tmp)
	try:
		m.launch('/initramfs/vfs_test.bin')
		data=m.until(lambda b: b'VFS FAIL' in b or b.count(b'VFS PASS')==2)
		assert b'VFS FAIL' not in data,data
		print(data.decode(errors='replace'))
	finally:
		m.proc.terminate();m.proc.wait(timeout=5)
		Path('tests/results').mkdir(exist_ok=True)
		Path('tests/results/vfs-serial.log').write_bytes(m.transcript)
