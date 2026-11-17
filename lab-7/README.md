# Lab 7 — Virtual File System

Based on the existing lab-6 kernel. Requirements: https://nycu-caslab.github.io/OSC2025/labs/lab7.html

## Build and run

```sh
make -j4
make QEMU_HEADLESS
```

Requires the AArch64 GNU toolchain, cpio and QEMU with `raspi4b` support. If QEMU is outside PATH:

```sh
make QEMU_HEADLESS EMULATOR=/home/ray/qemu-11.0.0-rc1/build/qemu-system-aarch64
```

At the kernel shell, enter `exec`, then `/initramfs/vfs1.img`. In the official user shell:

- `vfs`: Basic 1–4, UART stdin/stdout/stderr; press `x` when prompted.
- `fork`: framebuffer video through open/write/lseek64/ioctl.
- `exec`: reload via VFS.
- `exit`: return to kernel shell.

Bare archive filenames remain supported by the kernel shell for lab-6 compatibility.

## Implementation

- `inc/vfs.h`, `src/vfs.c`: filesystem registration, vnode/file operations, tmpfs root, mounts and pathname traversal. Lookup supports absolute/relative paths, empty paths, `.`, `..`, repeated slashes, directory checks and crossing mounts. Paths and components are limited to 255 bytes; directories to 16 entries; tmpfs file data to 4096 bytes. Components longer than the lab's required 15 bytes are supported to accommodate inherited test filenames.
- `/initramfs`: read-only newc CPIO tree, with immutable archive-backed file data. Write/create/mkdir fail. Nested directories are imported.
- `/dev`: mounted devfs, with UART and write-only framebuffer file operations. Devices are dispatched through operation tables.
- Each task stores root, cwd and 16 reusable fd slots. User processes start with UART in fd 0/1/2. Fork copies cwd and shares reference-counted open handles/offsets; exec preserves descriptors; exit releases them.
- Syscalls 11–19 implement open/close/write/read/mkdir/mount/chdir/lseek64/ioctl. User pointers are checked and copied with VM helpers, including demand paging and COW. Framebuffer ioctl request 0 returns four 32-bit fields: width, height, pitch, isrgb.
- Exec resolves immutable executable backing through a VFS file operation; lazy VM pages retain the reserved archive. Executing mutable tmpfs files is not supported. Existing lab-6 mailbox and VM interfaces remain available.
- Single-core preemption guards serialize metadata and nonblocking file operations. UART reads yield while waiting. Open access flags are retained but permissions are not enforced, matching the official test's O_CREAT-only read/write use.

## Validation

```sh
make test
# Override QEMU for the new tests if needed:
QEMU=/path/to/qemu-system-aarch64 make test
python3 tests/vm_qemu_test.py --qemu /path/to/qemu-system-aarch64 --only vm
```

`tests/vfs_qemu_test.py` exercises real EL0 syscalls, relative paths, mounted `..`, EOF, readonly failures, invalid pointers, fd reuse/exhaustion, fork descriptor lifetime, framebuffer bounds/seek/ioctl. `tests/official_vfs_qemu_test.py` runs the unmodified supplied `rootfs/vfs1.img`, asserts every VFS test succeeds and compares two framebuffer video captures. Logs and captures are saved in `tests/results/`.

The official binary was downloaded from the Lab 7 page; user source for it is not supplied. No real Raspberry Pi hardware was used for these checks.
