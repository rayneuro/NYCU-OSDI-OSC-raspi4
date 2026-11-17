# Lab 5 — Thread and User Process

依 [OSC2025 Lab 5](https://nycu-caslab.github.io/OSC2025/labs/lab5.html) 整理各 Exercise 的要求與實作簡述。

## Exercise

| 要求 | 實作簡述 |
| --- | --- |
| Basic 1：Thread | 為每個 thread 配置 TCB 與獨立 stack；context switch 保存並還原暫存器，以 FIFO run queue 做 round-robin 排程，由 idle thread 回收已結束的 thread |
| Basic 2：User Process and System Call | 從 CPIO 載入 raw binary 到 EL0，以 `svc #0` 進入核心並透過 x8 分派 syscall；fork 複製 stack 與 trap frame、共用程式映像；timer interrupt 提供核心與使用者行程搶佔 |
| Video Player | 以 timer interrupt 輪流排程行程，提供 UART 與 mailbox syscall；mailbox 使用對齊的私有 bounce buffer。課程影片測試程式尚未驗證 |
| Advanced 1：POSIX Signal | 尚未實作 signal handler 註冊、訊號傳送與處理 |
