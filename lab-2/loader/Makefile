
CC = aarch64-linux-gnu-gcc
LINKER = aarch64-linux-gnu-ld
OBJ_COPY = aarch64-linux-gnu-objcopy
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check -g

SRCDIR = src
SRCS 		= $(wildcard $(SRCDIR)/*.c )
OBJS 		= $(patsubst $(SRCDIR)/%.c, %.o, $(SRCS))
LINK_SCRIPT = src/link.ld
VPATH = $(SRCDIR)
vpath %.c %.S $(SOURCEDIR) 

all : clean kernel8.img

clean:
	@rm kernel8.elf kernel8.img *.o >/dev/null 2>/dev/null || true

kernel8.img: boot.o $(OBJS)
	$(LINKER) -nostdlib boot.o $(OBJS) -T src/link.ld -o kernel8.elf
	$(OBJ_COPY) -O binary kernel8.elf kernel8.img

boot.o: boot.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

QEMU: # Add -s -S to enable gdb debugging and -device loader to load the kernel at the correct address
	qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -nographic -serial mon:stdio -chardev pty,id=pl011,logfile=pl011.log -serial chardev:pl011 -kernel kernel8.img

QEMU_GDB:
	qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -nographic -serial mon:stdio -chardev pty,id=pl011,logfile=pl011.log -serial chardev:pl011 -s -S -kernel kernel8.img
GDB:
	gdb-multiarch kernel8.elf

GDB_TUI:
	gdb-multiarch -tui kernel8.elf


