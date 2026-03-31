CC			= aarch64-linux-gnu-gcc
LD 			= aarch64-linux-gnu-ld
OBJ_CPY 	= aarch64-linux-gnu-objcopy
EMULATOR	= qemu-system-aarch64

CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check 

SRCDIR 		= src
INCLUDE_DIR = inc
LIB_DIR 	= lib
LIB_HEADER	= lib/headers

SRCS 		= $(wildcard $(SRCDIR)/*.c )
OBJS 		= $(patsubst $(SRCDIR)/%.c, %.o, $(SRCS))
LINK_SCRIPT = link.ld

VPATH = $(SRCDIR)
vpath %.c %.S $(SRCDIR) 

.PHONY: all clean loader

all: clean kernel8.img

clean:
	@rm kernel8.elf kernel8.img raspbootcom *.o >/dev/null 2>/dev/null || true

loader: loader/raspbootcom.cc
	gcc loader/raspbootcom.cc -o raspbootcom

kernel8.img: start.o $(OBJS)
	$(LD) -nostdlib start.o $(OBJS) -T $(LINK_SCRIPT) -o kernel8.elf
	$(OBJ_CPY) -O binary kernel8.elf $@

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

QEMU:
	qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -nographic -serial mon:stdio -chardev pty,id=pl011,logfile=pl011.log -serial chardev:pl011 -kernel kernel8.img

QEMU_GDB:
	qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -nographic -serial mon:stdio -chardev pty,id=pl011,logfile=pl011.log -serial chardev:pl011 -kernel kernel8.img -s -S