### Files

| File    | Content       |
|---------| --------------|
| link.ld | Linker Script |
| boot.S  | Source code (assembly) |
| io.c    | uart gpio cod |
| kernel.c| kernel code   |
| shell.c | Execute start shell |
| string.c| Deal with string |

### Makefile build
make


### Run on QEMU
make QEMU



### Shell command
| Command    | Description       |
|---------| ------------------------|
| hello      | Hello World          |
| timestamp  | Print current timestamp  |
| help       | Print all avaliable commands | 
| reboot     | reset raspi4b           |



### Questions

1. Is it reasonable to accelerate booting speed by parallel programming during the initialization stage?

2. Point out the difference between bare-metal programming and programming on top of operating system.


