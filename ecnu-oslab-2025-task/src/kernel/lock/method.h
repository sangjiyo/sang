#pragma once

/* spinlock.c: 中断开关与自旋锁 */

void push_off();
void pop_off();

void spinlock_init(spinlock_t *lk, char *name);
bool spinlock_holding(spinlock_t *lk);
void spinlock_acquire(spinlock_t *lk);
void spinlock_release(spinlock_t *lk);

/* sleeplock.c: 睡眠锁 */

void sleeplock_init(sleeplock_t *lk, char *name);
bool sleeplock_holding(sleeplock_t *lk);
void sleeplock_acquire(sleeplock_t *lk);
void sleeplock_release(sleeplock_t *lk);