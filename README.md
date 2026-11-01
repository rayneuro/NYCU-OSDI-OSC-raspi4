# NYCU-OSDI-OSC-raspi4

由國立陽明交通大學（NYCU）**2025 年春季作業系統總整與實作（Operating System Capstone, OSC）**課程的個人實作與學習紀錄，以 **Raspberry Pi 4 Model B（Rpi4B）** 為目標硬體，改編原課程的 labs，逐步建立自己的 bare-metal 作業系統核心。

原課程以 Raspberry Pi 3 系列為平台。本專案沿用課程的核心概念與實作方向，針對 Rpi4B 的硬體差異調整底層程式，探索從硬體啟動到核心服務的完整流程。

## 實作方向

從不依賴既有作業系統的 bare-metal 程式開始，透過 C 與 AArch64 組合語言，逐步學習硬體初始化、周邊存取、中斷處理、記憶體管理、行程排程，以及檔案系統等核心機制。

移植到 Rpi4B 時，需要依實際硬體確認啟動設定、周邊暫存器位址、UART／GPIO 設定、中斷控制器，以及裝置樹與記憶體配置等差異。這些平台相關設定是改編實作的一部分，原課程的 Rpi3 範例需依 Rpi4B 的環境調整。

## 課程 Lab 涵蓋範圍

[OSC 2025 官方 labs](https://nycu-caslab.github.io/OSC2025/) labs overview ，包含部分進階主題(Advanced Exercise)

| Lab | 主題 | 涵蓋內容 |
| --- | --- | --- |
| [Lab 0](https://nycu-caslab.github.io/OSC2025/labs/lab0.html) | Environment Setup：開發環境建置 | 建立交叉編譯環境、產生可開機映像檔，準備硬體與序列埠開發環境。 |
| [Lab 1](https://nycu-caslab.github.io/OSC2025/labs/lab1.html) | Hello World：Bare-metal 入門 | 核心基本初始化、UART 輸入輸出、簡易 shell，以及透過 mailbox 與硬體溝通。 |
| [Lab 2](https://nycu-caslab.github.io/OSC2025/labs/lab2.html) | Booting：開機流程 | UART bootloader、初期記憶體配置、CPIO 格式的 initramfs；進階包含 bootloader 自我重定位與裝置樹解析。 |
| [Lab 3](https://nycu-caslab.github.io/OSC2025/labs/lab3.html) | Exception and Interrupt：例外與中斷 | ARM 例外層級、例外處理、中斷控制、計時器與非同步 I/O。 |
| [Lab 4](https://nycu-caslab.github.io/OSC2025/labs/lab4.html) | Allocator：記憶體配置器 | 啟動階段配置器、實體頁框配置器與動態記憶體配置器，支援後續核心功能。 |
| [Lab 5](https://nycu-caslab.github.io/OSC2025/labs/lab5.html) | Thread and User Process：執行緒與使用者行程 | 執行緒建立、上下文切換、排程與搶佔，以及使用者行程、系統呼叫和訊號處理。 |
| [Lab 6](https://nycu-caslab.github.io/OSC2025/labs/lab6.html) | Virtual Memory：虛擬記憶體 | MMU 初始化、頁表與位址轉換、核心與使用者位址空間隔離；進階包含按需分頁與寫入時複製。 |
| [Lab 7](https://nycu-caslab.github.io/OSC2025/labs/lab7.html) | Virtual File System：虛擬檔案系統 | 統一檔案操作介面、tmpfs、目錄與路徑解析、掛載、檔案描述符、initramfs 整合，以及裝置檔案。 |


## 課程來源與參考資料 (Reference)

- [NYCU OSC 2025 課程與 Lab 網站](https://nycu-caslab.github.io/OSC2025/)
- [2025 Spring Operating System Capstone 課程資訊與講義](https://people.cs.nycu.edu.tw/~ttyeh/course/2025_Spring/IOC5226/outline.html)


