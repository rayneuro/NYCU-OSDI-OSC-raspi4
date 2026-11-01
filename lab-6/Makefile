CC = aarch64-linux-gnu-gcc
LD = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy
EMULATOR ?= qemu-system-aarch64
CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check -fno-pie -fno-pic -g
OBJS = start.o switch.o $(patsubst src/%.c,%.o,$(wildcard src/*.c))
USER_PROGRAMS = el0_demo process_test exec_test syscall_test syscall_exec_test vm_test
USER_BINS = $(addprefix rootfs/,$(addsuffix .bin,$(USER_PROGRAMS)))
QEMU_FLAGS = -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -kernel kernel8.img -initrd initramfs.cpio -dtb bcm2711-rpi-4-b.dtb
.DELETE_ON_ERROR:
.SECONDARY:
.PHONY: all clean initramfs process-fixtures syscall-fixtures syscall-initramfs test QEMU QEMU_GDB QEMU_HEADLESS
all: kernel8.img initramfs.cpio
kernel8.elf: $(OBJS) link.ld
	$(LD) -nostdlib $(OBJS) -T link.ld -o $@
kernel8.img: kernel8.elf
	$(OBJCOPY) -O binary $< $@
%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -Iinc -c $< -o $@
%.o: src/%.S
	$(CC) $(CFLAGS) -MMD -MP -Iinc -c $< -o $@
user/%.elf: user/%.c user/syscall.h user/link.ld
	$(CC) $(CFLAGS) -mstrict-align -nostdlib -static -Wl,--build-id=none -Wl,-T,user/link.ld $< -o $@
user/process_test.elf: user/process_test.c user/register_probe.S user/syscall.h user/link.ld
	$(CC) $(CFLAGS) -mstrict-align -nostdlib -static -Wl,--build-id=none -Wl,-T,user/link.ld user/process_test.c user/register_probe.S -o $@
rootfs/%.bin: user/%.elf
	$(OBJCOPY) -O binary $< $@
initramfs.cpio: $(USER_BINS) rootfs/vm.img rootfs/file1.txt rootfs/file2.txt
	cd rootfs && find . -type f -printf '%P\n' | LC_ALL=C sort | cpio -o -H newc > ../$@
initramfs: initramfs.cpio
process-fixtures: rootfs/process_test.bin rootfs/exec_test.bin
syscall-fixtures: rootfs/syscall_test.bin rootfs/syscall_exec_test.bin
syscall-initramfs: syscall-fixtures
	cd rootfs && printf '%s\n' syscall_test.bin syscall_exec_test.bin | cpio -o -H newc > ../syscall_initramfs.cpio
QEMU: all
	$(EMULATOR) $(QEMU_FLAGS) -serial stdio
QEMU_HEADLESS: all
	$(EMULATOR) $(QEMU_FLAGS) -display none -monitor none -serial stdio
QEMU_GDB: all
	$(EMULATOR) $(QEMU_FLAGS) -display none -serial stdio -s -S
QEMU_SYSCALL: kernel8.img syscall-initramfs
	$(EMULATOR) -M raspi4b -m 2G -display none -monitor none -serial stdio -kernel kernel8.img -initrd syscall_initramfs.cpio -dtb bcm2711-rpi-4-b.dtb
test: all
	python3 tests/vm_qemu_test.py
.PHONY: test-all test-allocator
test-all: test test-allocator
	python3 tests/thread_qemu_test.py
	python3 tests/process_qemu_test.py
	python3 tests/syscall_qemu_test.py
test-allocator:
	cc -Wall -Wextra -Wno-sign-compare -g -DVM_HOST_TEST -fsanitize=address,undefined -ffunction-sections -fdata-sections -Iinc tests/startup_test.c src/startup.c src/mm.c -Wl,--gc-sections -o tests/startup_test
	ASAN_OPTIONS=detect_leaks=0 ./tests/startup_test
clean:
	rm -f *.o *.d kernel8.elf kernel8.img user/*.elf rootfs/*.bin initramfs.cpio syscall_initramfs.cpio tests/startup_test
-include $(OBJS:.o=.d)
