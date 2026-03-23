CFILES = $(wildcard *.c)
OFILES = $(CFILES:.c=.o)
CC = aarch64-linux-gnu-gcc
LINKER = aarch64-linux-gnu-ld
OBJ_COPY = aarch64-linux-gnu-objcopy
CFLAGS = -Wall -O2 -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a72+nosimd -fno-stack-protector -fno-stack-check 

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
	$(LINKER) -nostdlib boot.o $^ -T src/link.ld -o kernel8.elf
	$(OBJ_COPY) -O binary kernel8.elf kernel8.img

boot.o: boot.S
	$(CC) $(CFLAGS) -c $< -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@


