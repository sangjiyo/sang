#include "mod.h"

#define BACKSPACE 0x100
#define Ctrl(x) ((x)-'@')

static console_t cons;

void cons_init()
{
	spinlock_init(&cons.lk, "console");
	cons.read_idx = 0;
	cons.writ_idx = 0;
	cons.edit_idx = 0;
}

static void cons_putc(int c)
{
	if (c == BACKSPACE) {
		uart_putc_sync('\b');
		uart_putc_sync(' ');
		uart_putc_sync('\b');
	} else {
		uart_putc_sync(c);
	}
}

/* 数据输出: src -> UART */
uint32 cons_write(uint32 len, uint64 src, bool is_user_src)
{
	char tmp[64];
	uint32 write_len = 0, cut_len;
	proc_t *p = myproc();

	spinlock_acquire(&cons.lk);
	while (write_len < len)
	{
		cut_len = MIN(len-write_len, sizeof(tmp));
		if (is_user_src)
			uvm_copyin(p->pgtbl, (uint64)tmp, src, cut_len);
		else
			memmove(tmp, (void*)src, cut_len);

		for (uint32 i = 0; i < cut_len; i++)
			cons_putc(tmp[i]);
		
		src += cut_len;
		write_len += cut_len;
	}
	spinlock_release(&cons.lk);

	return write_len;
}

/* 数据读取: cons.buf -> dst */
uint32 cons_read(uint32 len, uint64 dst, bool is_user_dst)
{
	uint32 read_len = 0;
	proc_t *p = myproc();
	char c;

	spinlock_acquire(&cons.lk);
	while (read_len < len)
	{
		while (cons.read_idx == cons.writ_idx)
			proc_sleep(&cons.read_idx, &cons.lk);
		
		c = cons.buf[cons.read_idx++ % CONSOLE_INPUT_BUF];

		if (is_user_dst)
			uvm_copyout(p->pgtbl, dst, (uint64)&c, 1);
		else
			memmove((void*)dst, &c, 1);
		
		dst++;
		read_len++;

		if (c == '\n')
			break;
	}
	spinlock_release(&cons.lk);

	return read_len;
}

/* 数据输入: UART -> cons.buf */
void cons_edit(int c)
{
	spinlock_acquire(&cons.lk);

	switch (c)
	{
	case Ctrl('U'): // 删除当前行
		while (cons.edit_idx != cons.writ_idx &&
			cons.buf[(cons.edit_idx - 1) % CONSOLE_INPUT_BUF] != '\n')
		{
			cons.edit_idx--;
			cons_putc(BACKSPACE);
		}
		break;
	
	case Ctrl('H'): // 处理Backspace
	case '\x7f':
		if (cons.edit_idx != cons.writ_idx) {
			cons.edit_idx--;
			cons_putc(BACKSPACE);
		}
		break;

	default:
		if (c != 0 && cons.edit_idx - cons.read_idx < CONSOLE_INPUT_BUF) {
			if (c == '\r')
				c = '\n';
			
			cons_putc(c);

			cons.buf[cons.edit_idx++ % CONSOLE_INPUT_BUF] = c;

			// 唤醒读者的情况: 读到行尾 / 缓冲区已满
			if (c == '\n' || cons.edit_idx == cons.read_idx + CONSOLE_INPUT_BUF) {
				cons.writ_idx = cons.edit_idx;
				proc_wakeup(&cons.read_idx);
			}
		}
		break;
	}

	spinlock_release(&cons.lk);
}