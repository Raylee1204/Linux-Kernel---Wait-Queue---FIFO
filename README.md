# Linux-Kernel---Wait-Queue---FIFO
## 專案簡介 (Introduction)
本專案深入探討 Linux Kernel 的排程與同步機制，透過實作一個 **Custom System Call (自定義系統呼叫)**，驗證核心層級的等待隊列 (Wait Queue) 行為。

專案目標是設計一套機制，讓 User Space 的多個執行緒 (Threads) 能進入 Kernel Space 進行睡眠 (Sleep)，並由管理執行緒依照 **FIFO (先進先出)** 的順序喚醒，藉此驗證 Linux Kernel 的 `task_struct` 管理與 Context Switch 行為。

## 核心技術 (Key Technologies)
- **Linux Kernel Development**: System Call 實作、Kernel Module 編譯。
- **Process Synchronization**: 使用 `Mutex Lock` 保護 Critical Section，避免 Race Condition。
- **Wait Queue Mechanism**: 深入理解 `wait_event_interruptible` 與 `wake_up_process` 的運作原理。
- **Task Scheduling**: 手動維護 `task_struct` 陣列以實現嚴格的 FIFO 喚醒順序。

## 系統架構與實作 (Architecture)

### 1. User Space (使用者空間)
- 建立 10 個 Worker Threads，透過系統呼叫 (`syscall 451`) 進入核心等待模式。
- 主執行緒 (Main Thread) 在一段時間後發送喚醒指令，觸發核心進行清理與喚醒。

### 2. Kernel Space (核心空間)
核心模組維護了一個全域的 `waiting_tasks` 陣列與 `wait_queue_head`。

- **進入休眠 (Enter Queue, ID=1)**:
    1. 獲取 `mutex` 鎖。
    2. 將當前行程 (`current task_struct`) 存入 FIFO 陣列。
    3. 設定 `condition = 0` 並呼叫 `wait_event_interruptible()` 進入 `TASK_INTERRUPTIBLE` 狀態 (釋放 CPU)。

- **喚醒機制 (Wake Up, ID=2)**:
    1. 獲取 `mutex` 鎖並設定 `condition = 1`。
    2. **FIFO 邏輯**: 依照陣列索引順序 (0 to N)，逐一呼叫 `wake_up_process()` 喚醒執行緒。
    3. 使用 `msleep(100)` 確保喚醒順序在 `dmesg` Log 中清晰可見。

## 開發環境 (Environment)
- **OS**: Ubuntu 22.04 LTS
- **Kernel Version**: Linux 5.15.137
- **Platform**: VMware Virtual Machine

## 驗證結果 (Verification)

透過 `dmesg` 查看 Kernel Log，可證實執行緒依照 **進入順序 (FIFO)** 被依序喚醒。

### Kernel Log 分析
如下圖所示，Process 加入與喚醒的順序完全一致，證明 FIFO 佇列實作成功。
- **Store Order**: PID 2283 -> 2285 -> 2284 ...
- **Wake Order**: PID 2283 -> 2285 -> 2284 ...

![Kernel FIFO Log](./screenshots/dmesg_fifo_log.png)

## 程式碼片段說明 (Code Highlights)

### 核心休眠邏輯 (Kernel Sleep)
使用 `mutex` 保護共享資源，並將當前 Process 加入管理陣列。
```c
mutex_lock(&condition_mutex);
waiting_tasks[task_count++] = current; // 記錄 task_struct
mutex_unlock(&condition_mutex);
wait_event_interruptible(my_wait_queue, condition != 0); // 讓 Process 進入休眠
