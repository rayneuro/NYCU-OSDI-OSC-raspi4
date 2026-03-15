CFILES = $(wildcard *.c)
OFILES = $(CFILES:.c=.o)
CC = aarch64-linux-gnu-gcc
LINKER = aarch64-linux-gnu-ld
OBJ_COPY = aarch64-linux-gnu-objcopy
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check 

SOURCEDIR = src
SRCS 		= $(wildcard $(SRCDIR)/*.c )
OBJS 		= $(patsubst $(SRCDIR)/%.c, %.o, $(SRCS))
LINK_SCRIPT = src/link.ld

vpath %.c %.S $(SOURCEDIR) 

all : clean kernel8.img

boot.o: boot.S
	$(CC) $(CFILES) -c boot.S -o boot.o

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel8.img: boot.o $(OFILES)
	$(LINKER) -nostdlib boot.o $(OFILES) -T src/link.ld -o kernel8.elf
	$(OBJ_COPY) -O binary kernel8.elf kernel8.img

start.o: start.S
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	/bin/rm kernel8.elf *.o *.img > /dev/null 2> /dev/null || true