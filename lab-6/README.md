# Lab 6 — Virtual Memory（Raspberry Pi 4）

以 `../lab-5` 的 UART、GIC、timer、buddy/slab allocator、thread scheduler、CPIO loader 與 syscall 架構為基礎，實作 [OSC2025 Lab 6](https://nycu-caslab.github.io/OSC2025/labs/lab6.html) 的 Basic Exercise 1／2、Video Player，以及 Advanced Exercise 1／2／3。`lab-5` 保持原狀，`lab-6` 可獨立編譯。

## 編譯與執行

需要 `aarch64-linux-gnu-gcc`、binutils、`cpio`、Python 3，以及支援 `raspi4b` 的 QEMU。

```sh
cd /home/ray/NYCU-OSDI-OSC-raspi4/lab-6
make -j4
make QEMU                 # 有顯示介面的 QEMU，可看影片
# 或
make QEMU_HEADLESS        # UART-only；仍可測試 framebuffer RAM
```

開機後輸入：

```text
# exec
Program: vm.img
Process start, pid is: ...
# help
# fork
Child pid: ...
# pid
# kill <child-pid>
# exec
# demand
# mmap_w
```

第一層 `#` 是核心 shell；執行 `vm.img` 後進入官方使用者 shell。`fork` 啟動播放影片的子行程，父行程仍可接受指令。`exec` 替換目前行程，`exit` 返回核心 shell。`mmap_w` 故意寫唯讀頁；`mmap_r` 故意讀越界，兩者應輸出 segmentation fault 並返回核心 shell。

核心 shell 也保留 `thread_test`、`preempt_test`、`syscall_test`、`quantum 1`／`10`／`50`、`ls`、`cat` 等 lab-5 指令。`exec` → `el0_demo.bin` 可執行簡單 EL0 範例。舊 `loadimg` 會提示改用 bootloader／重新啟動：啟用 MMU 後，不能沿用直接覆寫正在執行核心的實體位址跳轉方式。

## 實作對照

| 要求 | 主要檔案 | 行為 |
| --- | --- | --- |
| Basic 1：核心虛擬記憶體 | `src/start.S`、`link.ld`、`inc/address.h` | 設定 TCR／MAIR／TTBR／SCTLR；啟用 MMU 後跳到高位址，移除低位址 identity mapping |
| Basic 2：使用者空間 | `src/vm.c`、`src/process.c`、`src/thread.c` | 每行程獨立 PGD、四層 4 KiB 頁表；context switch 切 TTBR0 並清 TLB；fork／exec 保留一致 VA |
| Video Player | `src/process.c `、`src/mailbox.c` | 安全複製 syscall buffer；mailbox 使用實體 bounce buffer；GPU framebuffer 以共享外部頁映射給 EL0 |
| Advanced 1：mmap | `vm_mmap()`、syscall 10 | anonymous、hint、頁對齊、長度進位、權限、MAP_POPULATE |
| Advanced 2：需求分頁 | `vm_fault()`、`src/except.c` | code／stack／anonymous 首次存取才配置；輸出 fault 位址；非法存取終止該行程 |
| Advanced 3：COW | `vm_fork()`、`resolve()`、`page_t.refs` | fork 共用頁框並改唯讀；寫入時複製；最後一個引用直接恢復可寫；exit／exec 正確回收 |

## 位址與權限

- 核心載入 PA `0x80000`，連結 VA `0xffff000000080000`。TTBR1 將低 4 GiB 實體範圍線性映射到 `0xffff000000000000 + PA`，使用 2 MiB blocks。Pi 4 的 `0xfc000000..0xffffffff` peripheral window 為 Device nGnRnE，其餘為 Normal Non-cacheable。I-cache／D-cache 關閉，48-bit VA，沒有 ASID。
- 核心 thread 的 TTBR0 指向空頁表，使用者行程各自擁有 TTBR0。EL0 無法存取核心映射；使用者頁設定 PXN。所有頁表 descriptor 與 mailbox 寫入的地址都是 PA，核心指標透過 `phys_to_virt()`／`virt_to_phys()` 轉換。
- 使用者 raw binary 自 VA `0x0` 開始。stack 是 `[0xffffffffb000, 0xfffffffff000)`，初始 SP 為 `0xfffffffff000`；stack 下方沒有映射。預設 mmap 從 `0x10000000` 尋找空間。
- Raw binary 沒有 ELF segment 權限資訊，因此 image region 為 RWX；自己的 user linker 將 BSS 零值包含在 binary 裡。`mmap` 的唯讀、不可執行與 PROT_NONE 各自生效。AArch64 的一般 AP 欄位無法提供 write-only／execute-only 的資料讀取隔離；PROT_WRITE／PROT_EXEC 映射在硬體上仍可被讀取。
- `mmap(addr, len, prot, flags, fd, offset)` 使用 syscall 10、x0–x5 傳參、x0 回傳，失敗回 `(void *)-1`。支援 `PROT_NONE=0`、`READ=1`、`WRITE=2`、`EXEC=4`，`MAP_ANONYMOUS=0x20`、`MAP_POPULATE=0x8000`，也接受 `MAP_PRIVATE=2`。非 anonymous 不支援；anonymous 忽略 fd／offset。未指定 POPULATE 時只建立 VMA。
- fork 同時複製 VMA 和頁表，保留尚未配置的洞。可寫頁使用軟體 COW bit，真正唯讀頁不能藉由 write fault 升級權限。頁框參考計數及 VM 操作由單核心 preemption guard 保護。
- `copyin`／`copyout` 先檢查完整 user range，再逐頁轉譯。kernel copyout 也會觸發 COW，避免 UART 或 GPU 回應修改其他行程的資料。mailbox 接受 property channel 8，檢查緩衝區及 tag 邊界，使用核心私有連續實體頁；回傳的 framebuffer 必須位於 GPU 記憶體範圍。
- Framebuffer 是實際硬體共享區域，fork 後保留相同 PA、可寫且不可執行，不做 COW，也不歸還 buddy。一般 mailbox buffer 仍受 COW 保護。
- 其餘 CPU 停在 WFE；配置器管理 firmware ARM-memory mailbox 回報的連續低位址 RAM。Lab 5 的可選 POSIX signal（syscall 8／9）不在本次 Lab 6 範圍內。

## 自動驗證

```sh
make test                # VM + 官方 vm.img
make test-all            # 再加 allocator、thread、process、interactive syscall 回歸測試
# 指定 QEMU：
python3 tests/vm_qemu_test.py --qemu /path/to/qemu-system-aarch64
```

VM 測試在 1／10／50 ms quantum 下檢查 mmap hints、進位、零值、512 MiB sparse region、MAP_POPULATE、巢狀 COW、parent/global/stack 隔離、mailbox 與跨頁 UART COW、exec replacement、可執行頁及 7 種非法存取。QEMU monitor 直接檢查 buddy 可用頁數和 zombie queue，確認退出後回收穩定；初次建立的 slab cache 允許保留最多 3 頁。

官方程式測試會比較多張 framebuffer RAM 擷取，確認影片畫面持續改變，同時送入 `pid` 驗證 UART 與排程；另外驗證 help／fork／kill／exec／demand／mmap_r／mmap_w。結果寫到 `tests/results/`（不加入版本控制）。不需要圖形介面或第三方 Python 套件。

本次已在 QEMU 10.2.91（本機 qemu-11.0.0-rc1 build）驗證；尚未在實體 Raspberry Pi 4 執行。

## 官方測試程式來源

`rootfs/vm.img` 是課程提供的原始 binary，沒有修改：

- [官方下載](https://nycu-caslab.github.io/OSC2025/_downloads/4a3ff2431ab7fa74536c184270dbe5c0/vm.img)
- SHA-256：`0f640d7c5a7759399b27103eff842e8fd509b0107b7bf14590e068989ae49e98`

## GDB

```sh
make QEMU_GDB
# 另一個終端：
gdb-multiarch kernel8.elf
(gdb) target remote :1234
(gdb) break main
(gdb) continue
```

MMU 啟用後符號在高 VA。QEMU monitor 的 `xp`／`pmemsave` 使用 PA，需減去 `0xffff000000000000`；GDB 連到 guest 後依 CPU 的位址轉譯讀取 VA。
