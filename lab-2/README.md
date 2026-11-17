# Lab 2 — Booting

依 [OSC2025 Lab 2](https://nycu-caslab.github.io/OSC2025/labs/lab2.html) 整理各 Exercise 的 TODO。

## Exercise

| 要求 | TODO |
| --- | --- |
| Basic 1：Reboot | 新增 `reboot` 指令，重新啟動系統 |
| Basic 2：UART Bootloader | 實作 bootloader，透過 UART 接收並載入 kernel image |
| Basic 3：Initial Ramdisk | 解析 `newc` 格式的 CPIO archive，依檔案路徑讀取內容 |
| Basic 4：Simple Allocator | 新增 `memAlloc` 指令；配置指定大小的連續記憶體，回傳指標 |
| Advanced 1：Bootloader Self Relocation | bootloader 起初與 kernel 載入位址重疊，載入 kernel 前自行搬移；不依賴 `config.txt` 的 `kernel_address=` |
| Advanced 2：Devicetree | 實作遍歷 DTB 的 parser 與 callback API；從 `/chosen` 的 `linux,initrd-start` 取得 initramfs 位址；bootloader 傳遞 DTB 位址給 kernel，並將 `config.txt` 設定改為 `initramfs initramfs.cpio`，移除固定位址 |
