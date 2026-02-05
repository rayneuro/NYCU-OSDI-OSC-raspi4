### Files


| File    | Content       |
|---------| --------------|
| lab0.ld | Linker Script |
| lab0.s  | Source code (assembly) |


### Generate the object file 
aarch64-linux-gnu-gcc -c lab0.s -o lab0.o


### Generate elf from object file
aarch64-linux-gnu-ld -nostdlib lab0.o  -T lab0.ld -o kernel8.elf

### Generate img file from elf file
aarch64-linux-gnu-objcopy -O binary kernel8.elf kernel8.img 

### Display
qemu-system-aarch64 -M raspi4 -kernel kernel8.img -display none -d in_asm
