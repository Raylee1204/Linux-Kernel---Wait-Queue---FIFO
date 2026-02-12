#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/delay.h>  // 加入 udelay 的標頭檔

// 宣告等待隊列
static DECLARE_WAIT_QUEUE_HEAD(my_wait_queue);
static int condition = 0;

// Mutex 保護共享變數
static DEFINE_MUTEX(condition_mutex);

// 追蹤等待中的執行緒
static struct task_struct *waiting_tasks[1024];
static int task_count = 0;

SYSCALL_DEFINE1(call_my_wait_queue, int, id)
{
    int i;  // 將變數宣告移到function開頭

    switch (id) {
    case 1:
        // 加入等待隊列，進入休眠
        mutex_lock(&condition_mutex);
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
