#include "mod.h"

// 睡眠锁初始化
void sleeplock_init(sleeplock_t *lk, char *name)
{
    spinlock_init(&lk->lock, "sleep_lock");
    lk->name = name;
    lk->locked = 0;
    lk->pid = 0;
}

// 检查当前进程是否持有睡眠锁
bool sleeplock_holding(sleeplock_t *lk)
{
    bool r;
    spinlock_acquire(&lk->lock);
    r = (lk->locked && lk->pid == myproc()->pid);
    spinlock_release(&lk->lock);
    return r;
}

// 当前进程尝试获取睡眠锁, 失败进入睡眠状态
void sleeplock_acquire(sleeplock_t *lk)
{
    spinlock_acquire(&lk->lock);
    while (lk->locked) {
        // 睡眠, 以lk为资源, 传入lk->lock
        // proc_sleep会释放lk->lock, 并在返回时重新获取
        proc_sleep(lk, &lk->lock);
    }
    lk->locked = 1;
    lk->pid = myproc()->pid;
    spinlock_release(&lk->lock);
}

// 释放睡眠锁, 唤醒其他等待睡眠锁的进程
void sleeplock_release(sleeplock_t *lk)
{
    spinlock_acquire(&lk->lock);
    lk->locked = 0;
    lk->pid = 0;
    // 唤醒所有等待该锁的进程
    proc_wakeup(lk);
    spinlock_release(&lk->lock);
}
