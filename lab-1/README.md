# Lab 1 — Hello World

## Exercise

| 要求 | TODO |
| --- | --- |
| Basic 1：Basic Initialization | bootloader 載入核心後，清零 BSS 並設定 stack pointer，完成開機初始化 |
| Basic 2：Mini UART | 設定 Mini UART，讓開發主機與 Raspberry Pi 透過序列埠通訊 |
| Basic 3：Simple Shell | 實作簡易 shell，至少支援 `help` 與 `hello` 指令 |
| Basic 4：Mailbox | 透過 mailbox 取得並印出硬體資訊，至少包含 board revision、ARM 記憶體起始位址與大小 |

## Files

| File    | Content       |
|---------| --------------|
| link.ld | Linker Script |
| boot.S  | Source code (assembly) |
| io.c    | uart gpio cod |
| kernel.c| kernel code   |
| shell.c | Execute start shell |
| string.c| Deal with string |

## Makefile build
make


## Run on QEMU
make QEMU



## Shell command
| Command    | Description       |
|---------| ------------------------|
| hello      | Hello World          |
| timestamp  | Print current timestamp  |
| help       | Print all avaliable commands | 
| reboot     | reset raspi4b           |



## Questions

1. Is it reasonable to accelerate booting speed by parallel programming during the initialization stage?

2. Point out the difference between bare-metal programming and programming on top of operating system.


