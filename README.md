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

```c
\\ user code result
enter wait queue thread_id: 0
enter wait queue thread_id: 2
enter wait queue thread_id: 1
enter wait queue thread_id: 4
enter wait queue thread_id: 3
enter wait queue thread_id: 7
enter wait queue thread_id: 6
enter wait queue thread_id: 9
enter wait queue thread_id: 5
enter wait queue thread_id: 8
start clean queue ...
exit wait queue thread_id: 0
exit wait queue thread_id: 2
exit wait queue thread_id: 1
exit wait queue thread_id: 4
exit wait queue thread_id: 3
exit wait queue thread_id: 7
exit wait queue thread_id: 6
exit wait queue thread_id: 9
exit wait queue thread_id: 5
exit wait queue thread_id: 8


\\kernel code result
[   46.937032] store a process2283
[   46.937143] store a process2285
[   46.937147] store a process2284
[   46.937222] store a process2287
[   46.937288] store a process2286
[   46.937291] store a process2290
[   46.937346] store a process2289
[   46.937366] store a process2292
[   46.937420] store a process2288
[   46.937519] store a process2291
[   48.034318] [Wake Queue] Process 2283 (task: wait_queue) woken up, remaining tasks: 9
[   48.137808] [Wake Queue] Process 2285 (task: wait_queue) woken up, remaining tasks: 8
[   48.240913] [Wake Queue] Process 2284 (task: wait_queue) woken up, remaining tasks: 7
[   48.343952] [Wake Queue] Process 2287 (task: wait_queue) woken up, remaining tasks: 6
[   48.447477] [Wake Queue] Process 2286 (task: wait_queue) woken up, remaining tasks: 5
[   48.550813] [Wake Queue] Process 2290 (task: wait_queue) woken up, remaining tasks: 4
[   48.654006] [Wake Queue] Process 2289 (task: wait_queue) woken up, remaining tasks: 3
[   48.757331] [Wake Queue] Process 2292 (task: wait_queue) woken up, remaining tasks: 2
[   48.860805] [Wake Queue] Process 2288 (task: wait_queue) woken up, remaining tasks: 1
[   48.964076] [Wake Queue] Process 2291 (task: wait_queue) woken up, remaining tasks: 0
```


## 程式碼片段說明 (Code Highlights)

### 核心休眠邏輯 (Kernel Sleep)
使用 `mutex` 保護共享資源，並將當前 Process 加入管理陣列。
```c
mutex_lock(&condition_mutex);
waiting_tasks[task_count++] = current; // 記錄 task_struct
mutex_unlock(&condition_mutex);
wait_event_interruptible(my_wait_queue, condition != 0); // 讓 Process 進入休眠
```

## 結論以及優化方向

### 1. 實驗結論 (Conclusion)
本實驗成功實作了一個基於 Linux Kernel Character Device 的 FIFO 等待隊列系統。透過對照核心日誌 (dmesg) 的紀錄，我們證實了核心模組 (Kernel Module) 能夠正確地將進入的 Process 依序存入陣列，並在喚醒階段嚴格遵守「先進先出 (First-In, First-Out)」的原則，成功達成實驗目標。

### 2. 現象分析：為何 User Space 出現亂序？ (Analysis of Ordering Disorder)
在實驗數據中，我們觀察到一個有趣的現象：雖然 User Space 程式透過 for 迴圈依序創建執行緒 (Thread 0, 1, 2...)，但在 Log 中卻出現了「Thread 2 比 Thread 1 先進入 Kernel」的亂序情況。

這並非核心模組的 FIFO 機制失效，而是多執行緒環境下的正常現象，原因如下：

+ 排程器的非決定性 (Non-deterministic Scheduling)：pthread_create 僅將執行緒加入系統的 就緒隊列 (Ready Queue)，並非立即執行。Linux 排程器 (Scheduler) 根據 CPU 負載與演算法動態決定誰先獲得 CPU 時間片 (Time Slice)。

+ 競爭條件 (Race Condition)：雖然 Thread 1 先被創建，但若 Thread 2 獲得了較優的排程優先權或 Thread 1 發生了 Context Switch，Thread 2 便可能「後發先至」，搶先到達 Kernel 的 mutex_lock 入口。

+ 互斥鎖的搶佔行為：核心層的 Mutex 正確地保護了 Critical Section，但它保護的是「同時只有一人能寫入」，對於「誰先到達門口」則是忠實反映了 User Space 的競爭結果。

### 3. 優化方向：強制同步機制 (Optimization Strategy)
若在應用場景中需要嚴格保證「創建順序即為進入順序」，單純依賴 sleep() 延遲並非可靠的解決方案。建議引入 POSIX Semaphore (信號量) 技術來實作 「父子同步 (Parent-Child Synchronization)」 機制：

實作原理：在主執行緒 (Main Thread) 創建子執行緒後，立即呼叫 sem_wait() 進入阻塞狀態；子執行緒則在即將呼叫 System Call 前，發送 sem_post() 信號通知主執行緒。

預期效果：透過此「握手 (Handshake)」機制，確保前一個執行緒確認進入 Kernel 等待狀態後，主執行緒才被允許創建下一個執行緒，從而從物理上杜絕 User Space 的搶跑問題，實現絕對的依序進入。
