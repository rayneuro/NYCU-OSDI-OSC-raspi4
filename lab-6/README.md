# Lab 6 — Virtual Memory

[OSC2025 Lab 6](https://nycu-caslab.github.io/OSC2025/labs/lab6.html) Exercise and Advanced exercise 。

## Exercise

| 要求 | 實作簡述 |
| --- | --- |
| Basic 1：Virtual Memory in Kernel Space | 設定 TCR／MAIR、頁表與 MMU；建立 identity mapping，再將核心映射到高位址；區分一般 RAM 與 MMIO 的記憶體屬性 |
| Basic 2：Virtual Memory in User Space | 為每個行程建立獨立 PGD 與四層頁表；切換行程時更新 TTBR0 並清除 TLB，讓 fork／exec 使用一致的虛擬位址 |
| Video Player | 以 copyin／copyout 逐頁轉譯使用者 buffer；mailbox 使用實體 bounce buffer，framebuffer 以共享頁映射給 EL0，讓課程測試程式播放影片 |
| Advanced 1：mmap | 以 VMA 記錄匿名映射範圍與權限，處理位址 hint、頁對齊與長度進位；MAP_POPULATE 立即配置頁框，否則延後至首次存取 |
| Advanced 2：Page Fault Handler & Demand Paging | page fault handler 依 VMA 檢查位址與權限，在首次存取 code／stack／anonymous region 時配置頁框並建立映射；非法存取時終止行程 |
| Advanced 3：Copy-on-write | fork 時共用頁框、將可寫頁改為唯讀並標記 COW；write fault 時複製頁框，若只剩一個引用則直接恢復可寫；以參考計數於 exit／exec 時回收 |
