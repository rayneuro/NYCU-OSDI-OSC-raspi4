CFILES = $(wildcard *.c)
OFILES = $(CFILES:.c=.o)
CC = aarch64-linux-gnu-gcc
LINKER = aarch64-linux-gnu-ld
OBJ_COPY = aarch64-linux-gnu-objcopy

CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check 

all: clean kernel8.img

boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel8.img: boot.o $(OFILES)
	$(LINKER) -nostdlib boot.o $(OFILES) -T link.ld -o kernel8.elf
	$(OBJ_COPY) -O binary kernel8.elf kernel8.img

clean:
	/bin/rm kernel8.elf *.o *.img > /dev/null 2> /dev/null || true

QEMU:
	qemu-system-aarch64 -M raspi4b -cpu cortex-a72 -smp 4 -m 2G -nographic -serial mon:stdio -chardev pty,id=miniuart,logfile=mini_uart.log -serial chardev:miniuart -kernel kernel8.img