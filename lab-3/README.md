Need to modify the config.txt to enable uart0

EL0 initramfs program:

```sh
make kernel8.img initramfs
make QEMU
```

In the UART shell, enter `exec`, then `el0_demo.bin` at the `Program:` prompt.
The command executes the file directly from initramfs (no copy), using a separate
16 KiB stack aligned to 16 bytes. Programs must be position-independent AArch64
raw binaries with the entry instruction at offset zero; ELF loading, relocation,
and BSS initialization are not implemented. Only run trusted programs.

The included `user/el0_demo.c` exercises the user stack and issues `svc #0`.
The existing exception handler prints `spsr_el1: 0x000003c0` and
`ec: 0x00000015`, then halts. There is no exit syscall or return to the shell yet.
The requested SPSR value masks interrupts while running in EL0.



printk rpi4b 會檢查 
MESS:00:00:05.347148:0: 'console=ttyAMA0,115200 earlyprintk loglevel=7'
MESS:00:00:05.473610:0: brfs: File read: 45 bytes
MESS:00:00:05.481116:0: brfs: File read: /mfs/sd/bootloader.img
MESS:00:00:05.483922:0: Loaded 'bootloader.img' to 0x60000 size 0xbcc
MESS:00:00:05.490104:0: Kernel relocated to 0x80000
