#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/delay.h>  // 加入 udelay 的標頭檔

// 1. 宣告一個標準的等待隊列頭 (Wait Queue Head)
// 名字叫 my_wait_queue。這就像是一個「公車站牌」，大家都在這裡等。
static DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);

// 2. 條件變數 (Condition Flag)
// 0 代表紅燈 (要睡覺)，1 代表綠燈 (可以醒來)。
static int condition = 0;

// 3. 互斥鎖 (Mutex)
// 用來保護下面的 waiting_tasks 陣列，避免多個執行緒同時寫入造成資料錯亂。
static DEFINE_MUTEX(condition_mutex);

// 4. 自定義的 FIFO 陣列 (關鍵！)
// 不完全依賴 my_wait_queue 的內部鏈結串列，而是自己開一個陣列來存「人頭」。
// task_struct 是 Linux Kernel 用來記錄每一個 Process/Thread 的結構體。
static struct task_struct *waiting_tasks[1024];
static int task_count = 0;

SYSCALL_DEFINE1(call_my_wait_queue, int, id)
{
    int i;  // 將變數宣告移到function開頭

    switch (id) {
    case 1:
        // 1. 上鎖：進入 Critical Section，因為我們要操作全域陣列 waiting_tasks
        mutex_lock(&condition_mutex);
		// 2. 設定條件為 0 (False)，確保等等 wait_event 會真的睡著
        condition = 0;
        // 儲存目前執行緒的 task_struct
        waiting_tasks[task_count++] = current;
        printk("store a process%d\n", current->pid);
        mutex_unlock(&condition_mutex);

        // 等待條件變為非零
        wait_event_interruptible(my_wait_queue, condition != 0);

        return 1;

    case 2:
        mutex_lock(&condition_mutex);
        condition = 1;
        
        // 依序喚醒每個執行緒
        for (i = 0; i < task_count; i++) {
            if (waiting_tasks[i]) {
                wake_up_process(waiting_tasks[i]);
                msleep(100);  // 短暫延遲確保順序
                printk(KERN_INFO "[Wake Queue] Process %d (task: %s) woken up, remaining tasks: %d\n",
		       waiting_tasks[i]->pid,           // 進程 ID
		       waiting_tasks[i]->comm,          // 進程名稱
		       task_count - i - 1); 
            }
        }
        
        // 重置計數器
        task_count = 0;
        mutex_unlock(&condition_mutex);

        return 1;

    default:
        return -EINVAL;
    }
}
