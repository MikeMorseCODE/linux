// SPDX-License-Identifier: GPL-2.0

#include <linux/compiler.h>
#include <linux/context_tracking.h>
#include <linux/errno.h>
#include <linux/nospec.h>
#include <linux/ptrace.h>
#include <linux/randomize_kstack.h>
#include <linux/syscalls.h>

#include <asm/debug-monitors.h>
#include <asm/exception.h>
#include <asm/fpsimd.h>
#include <asm/syscall.h>
#include <asm/thread_info.h>
#include <asm/unistd.h>
#include <asm/unistd_compat_32.h>
#include <linux/uaccess.h>

extern void asteroids_boot_mark(const char *stage);

long compat_arm_syscall(struct pt_regs *regs, int scno);
long sys_ni_syscall(void);

static long do_ni_syscall(struct pt_regs *regs, int scno)
{
	if (is_compat_task()) {
		long ret = compat_arm_syscall(regs, scno);
		if (ret != -ENOSYS)
			return ret;
	}

	return sys_ni_syscall();
}

static long __invoke_syscall(struct pt_regs *regs, syscall_fn_t syscall_fn)
{
	return syscall_fn(regs);
}

/* ASTEROIDS PID1 TRACE V2 */
static unsigned int asteroids_pid1_syscall_count;

static void invoke_syscall(struct pt_regs *regs, unsigned int scno,
			   unsigned int sc_nr,
			   const syscall_fn_t syscall_table[])
{

	long ret;
	unsigned int asteroids_pid1_n = 0;
	bool asteroids_pid1_trace = false;

	if (current->pid == 1) {
		char mark[96];

		asteroids_pid1_n = asteroids_pid1_syscall_count++;
		asteroids_pid1_trace = asteroids_pid1_n < 200;

		if (asteroids_pid1_trace) {
			scnprintf(mark, sizeof(mark),
				  "PID1 PRE N%u S%u A0=%llx A1=%llx A2=%llx",
				  asteroids_pid1_n, scno,
				  regs->regs[0],
				  regs->regs[1],
				  regs->regs[2]);


			asteroids_boot_mark(mark);
		}
	}

	/* ASTEROIDS PID1 PAYLOAD TRACE */
	if (current->pid == 1 && scno == 56) {
		char path[72];
		char mark[96];
		long n;

		n = strncpy_from_user(path,
				     (const char __user *)regs->regs[1],
				     sizeof(path) - 1);

		if (n > 0) {
			path[sizeof(path) - 1] = '\0';
			scnprintf(mark, sizeof(mark),
				  "PID1 OPEN N%u %s",
				  asteroids_pid1_n, path);
		} else {
			scnprintf(mark, sizeof(mark),
				  "PID1 OPEN N%u COPYERR=%ld PTR=%llx",
				  asteroids_pid1_n, n,
				  regs->regs[1]);
		}

		asteroids_boot_mark(mark);
	}

	if (current->pid == 1 && scno == 64) {
		const char __user *ubuf =
			(const char __user *)regs->regs[1];
		size_t total = min_t(size_t, regs->regs[2], 160);
		size_t off = 0;

		while (off < total) {
			char data[49];
			char mark[96];
			size_t chunk = min_t(size_t, 48, total - off);
			size_t i;

			if (copy_from_user(data, ubuf + off, chunk)) {
				scnprintf(mark, sizeof(mark),
					  "PID1 WRITE N%u O%zu COPYERR",
					  asteroids_pid1_n, off);
				asteroids_boot_mark(mark);
				break;
			}

			for (i = 0; i < chunk; i++) {
				unsigned char c = data[i];

				if (c < 0x20 || c > 0x7e)
					data[i] = ' ';
			}

			data[chunk] = '\0';

			scnprintf(mark, sizeof(mark),
				  "PID1 WRITE N%u O%zu %s",
				  asteroids_pid1_n, off, data);

			asteroids_boot_mark(mark);
			off += chunk;
		}
	}

	add_random_kstack_offset();

	if (scno < sc_nr) {
		syscall_fn_t syscall_fn;
		syscall_fn = syscall_table[array_index_nospec(scno, sc_nr)];
		ret = __invoke_syscall(regs, syscall_fn);
	} else {
		ret = do_ni_syscall(regs, scno);
	}

	if (asteroids_pid1_trace) {
		char mark[96];

		scnprintf(mark, sizeof(mark),
			  "PID1 POST N%u S%u R=%ld",
			  asteroids_pid1_n, scno, ret);

		asteroids_boot_mark(mark);
	}

	syscall_set_return_value(current, regs, 0, ret);

	/*
	 * This value will get limited by KSTACK_OFFSET_MAX(), which is 10
	 * bits. The actual entropy will be further reduced by the compiler
	 * when applying stack alignment constraints: the AAPCS mandates a
	 * 16-byte aligned SP at function boundaries, which will remove the
	 * 4 low bits from any entropy chosen here.
	 *
	 * The resulting 6 bits of entropy is seen in SP[9:4].
	 */
	choose_random_kstack_offset(get_random_u16());
}

static inline bool has_syscall_work(unsigned long flags)
{
	return unlikely(flags & _TIF_SYSCALL_WORK);
}

static void el0_svc_common(struct pt_regs *regs, int scno, int sc_nr,
			   const syscall_fn_t syscall_table[])
{
	unsigned long flags = read_thread_flags();

	regs->orig_x0 = regs->regs[0];
	regs->syscallno = scno;

	/*
	 * BTI note:
	 * The architecture does not guarantee that SPSR.BTYPE is zero
	 * on taking an SVC, so we could return to userspace with a
	 * non-zero BTYPE after the syscall.
	 *
	 * This shouldn't matter except when userspace is explicitly
	 * doing something stupid, such as setting PROT_BTI on a page
	 * that lacks conforming BTI/PACIxSP instructions, falling
	 * through from one executable page to another with differing
	 * PROT_BTI, or messing with BTYPE via ptrace: in such cases,
	 * userspace should not be surprised if a SIGILL occurs on
	 * syscall return.
	 *
	 * So, don't touch regs->pstate & PSR_BTYPE_MASK here.
	 * (Similarly for HVC and SMC elsewhere.)
	 */

	if (flags & _TIF_MTE_ASYNC_FAULT) {
		/*
		 * Process the asynchronous tag check fault before the actual
		 * syscall. do_notify_resume() will send a signal to userspace
		 * before the syscall is restarted.
		 */
		syscall_set_return_value(current, regs, -ERESTARTNOINTR, 0);
		return;
	}

	if (has_syscall_work(flags)) {
		/*
		 * The de-facto standard way to skip a system call using ptrace
		 * is to set the system call to -1 (NO_SYSCALL) and set x0 to a
		 * suitable error code for consumption by userspace. However,
		 * this cannot be distinguished from a user-issued syscall(-1)
		 * and so we must set x0 to -ENOSYS here in case the tracer doesn't
		 * issue the skip and we fall into trace_exit with x0 preserved.
		 *
		 * This is slightly odd because it also means that if a tracer
		 * sets the system call number to -1 but does not initialise x0,
		 * then x0 will be preserved for all system calls apart from a
		 * user-issued syscall(-1). However, requesting a skip and not
		 * setting the return value is unlikely to do anything sensible
		 * anyway.
		 */
		if (scno == NO_SYSCALL)
			syscall_set_return_value(current, regs, -ENOSYS, 0);
		scno = syscall_trace_enter(regs);
		if (scno == NO_SYSCALL)
			goto trace_exit;
	}

	invoke_syscall(regs, scno, sc_nr, syscall_table);

	/*
	 * The tracing status may have changed under our feet, so we have to
	 * check again. However, if we were tracing entry, then we always trace
	 * exit regardless, as the old entry assembly did.
	 */
	if (!has_syscall_work(flags) && !IS_ENABLED(CONFIG_DEBUG_RSEQ)) {
		flags = read_thread_flags();
		if (!has_syscall_work(flags) && !(flags & _TIF_SINGLESTEP))
			return;
	}

trace_exit:
	syscall_trace_exit(regs);
}

void do_el0_svc(struct pt_regs *regs)
{
	el0_svc_common(regs, regs->regs[8], __NR_syscalls, sys_call_table);
}

#ifdef CONFIG_COMPAT
void do_el0_svc_compat(struct pt_regs *regs)
{
	el0_svc_common(regs, regs->regs[7], __NR_compat32_syscalls,
		       compat_sys_call_table);
}
#endif
