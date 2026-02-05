CFILES = $(wildcard *.c)
OFILES = $(CFILES:.c=.o)
CC = aarch64-linux-gnu-gcc
LINKER = aarch64-linux-gnu-ld

CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles

all: clean kernel8.img

boot.o: boot.S
	$(CC) --target=aarch64-elf $(CFLAGS) -c boot.S -o boot.o

%.o: %.c
	$(CC) --target=aarch64-elf $(CLAGS) -c $< -o $@

kernel8.img: boot.o $(OFILES)
	ld.lld -m aarch64elf -nostdlib boot.o $(OFILES) -T link.ld -o kernel8.elf
	llvm-objcopy -O binary kernel8.elf kernel8.img

clean:
	/bin/rm kernel8.elf *.o *.img > /dev/null 2> /dev/null || true