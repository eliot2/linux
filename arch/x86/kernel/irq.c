/*
 * Common interrupt code for 32 and 64 bit
 */
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/of.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/ftrace.h>
#include <linux/delay.h>
#include <linux/export.h>

#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/irq.h>
#include <asm/idle.h>
#include <asm/mce.h>
#include <asm/hw_irq.h>
#include <asm/desc.h>

#define CREATE_TRACE_POINTS
#include <asm/trace/irq_vectors.h>

DEFINE_PER_CPU_SHARED_ALIGNED(irq_cpustat_t, irq_stat);
EXPORT_PER_CPU_SYMBOL(irq_stat);

DEFINE_PER_CPU(struct pt_regs *, irq_regs);
EXPORT_PER_CPU_SYMBOL(irq_regs);

atomic_t irq_err_count;

/* Function pointer for generic interrupt vector handling */
void (*x86_platform_ipi_callback)(void) = NULL;

/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves.
 */
void ack_bad_irq(unsigned int irq)
{
	if (printk_ratelimit())
		pr_err("unexpected IRQ trap at vector %02x\n", irq);

	/*
	 * Currently unexpected vectors happen only on SMP and APIC.
	 * We _must_ ack these because every local APIC has only N
	 * irq slots per priority level, and a 'hanging, unacked' IRQ
	 * holds up an irq slot - in excessive cases (when multiple
	 * unexpected vectors occur) that might lock up the APIC
	 * completely.
	 * But only ack when the APIC is enabled -AK
	 */
	ack_APIC_irq();
}

#define irq_stats(x)		(&per_cpu(irq_stat, x))
/*
 * /proc/interrupts printing for arch specific interrupts
 */
int arch_show_interrupts(struct seq_file *p, int prec)
{
	int j;

	seq_printf(p, "%*s: ", prec, "NMI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->__nmi_count);
	seq_puts(p, "  Non-maskable interrupts\n");
#ifdef CONFIG_X86_LOCAL_APIC
	seq_printf(p, "%*s: ", prec, "LOC");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_timer_irqs);
	seq_puts(p, "  Local timer interrupts\n");

	seq_printf(p, "%*s: ", prec, "SPU");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_spurious_count);
	seq_puts(p, "  Spurious interrupts\n");
	seq_printf(p, "%*s: ", prec, "PMI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_perf_irqs);
	seq_puts(p, "  Performance monitoring interrupts\n");
	seq_printf(p, "%*s: ", prec, "IWI");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->apic_irq_work_irqs);
	seq_puts(p, "  IRQ work interrupts\n");
	seq_printf(p, "%*s: ", prec, "RTR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->icr_read_retry_count);
	seq_puts(p, "  APIC ICR read retries\n");
#endif
	if (x86_platform_ipi_callback) {
		seq_printf(p, "%*s: ", prec, "PLT");
		for_each_online_cpu(j)
			seq_printf(p, "%10u ", irq_stats(j)->x86_platform_ipis);
		seq_puts(p, "  Platform interrupts\n");
	}
#ifdef CONFIG_SMP
	seq_printf(p, "%*s: ", prec, "RES");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_resched_count);
	seq_puts(p, "  Rescheduling interrupts\n");
	seq_printf(p, "%*s: ", prec, "CAL");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_call_count -
					irq_stats(j)->irq_tlb_count);
	seq_puts(p, "  Function call interrupts\n");
	seq_printf(p, "%*s: ", prec, "TLB");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_tlb_count);
	seq_puts(p, "  TLB shootdowns\n");
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
	seq_printf(p, "%*s: ", prec, "TRM");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_thermal_count);
	seq_puts(p, "  Thermal event interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
	seq_printf(p, "%*s: ", prec, "THR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_threshold_count);
	seq_puts(p, "  Threshold APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE_AMD
	seq_printf(p, "%*s: ", prec, "DFR");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->irq_deferred_error_count);
	seq_puts(p, "  Deferred Error APIC interrupts\n");
#endif
#ifdef CONFIG_X86_MCE
	seq_printf(p, "%*s: ", prec, "MCE");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", per_cpu(mce_exception_count, j));
	seq_puts(p, "  Machine check exceptions\n");
	seq_printf(p, "%*s: ", prec, "MCP");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", per_cpu(mce_poll_count, j));
	seq_puts(p, "  Machine check polls\n");
#endif
#if IS_ENABLED(CONFIG_HYPERV) || defined(CONFIG_XEN)
	if (test_bit(HYPERVISOR_CALLBACK_VECTOR, used_vectors)) {
		seq_printf(p, "%*s: ", prec, "HYP");
		for_each_online_cpu(j)
			seq_printf(p, "%10u ",
				   irq_stats(j)->irq_hv_callback_count);
		seq_puts(p, "  Hypervisor callback interrupts\n");
	}
#endif
	seq_printf(p, "%*s: %10u\n", prec, "ERR", atomic_read(&irq_err_count));
#if defined(CONFIG_X86_IO_APIC)
	seq_printf(p, "%*s: %10u\n", prec, "MIS", atomic_read(&irq_mis_count));
#endif
#ifdef CONFIG_HAVE_KVM
	seq_printf(p, "%*s: ", prec, "PIN");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ", irq_stats(j)->kvm_posted_intr_ipis);
	seq_puts(p, "  Posted-interrupt notification event\n");

	seq_printf(p, "%*s: ", prec, "PIW");
	for_each_online_cpu(j)
		seq_printf(p, "%10u ",
			   irq_stats(j)->kvm_posted_intr_wakeup_ipis);
	seq_puts(p, "  Posted-interrupt wakeup event\n");
#endif
	return 0;
}

/*
 * /proc/stat helpers
 */
u64 arch_irq_stat_cpu(unsigned int cpu)
{
	u64 sum = irq_stats(cpu)->__nmi_count;

#ifdef CONFIG_X86_LOCAL_APIC
	sum += irq_stats(cpu)->apic_timer_irqs;
	sum += irq_stats(cpu)->irq_spurious_count;
	sum += irq_stats(cpu)->apic_perf_irqs;
	sum += irq_stats(cpu)->apic_irq_work_irqs;
	sum += irq_stats(cpu)->icr_read_retry_count;
#endif
	if (x86_platform_ipi_callback)
		sum += irq_stats(cpu)->x86_platform_ipis;
#ifdef CONFIG_SMP
	sum += irq_stats(cpu)->irq_resched_count;
	sum += irq_stats(cpu)->irq_call_count;
#endif
#ifdef CONFIG_X86_THERMAL_VECTOR
	sum += irq_stats(cpu)->irq_thermal_count;
#endif
#ifdef CONFIG_X86_MCE_THRESHOLD
	sum += irq_stats(cpu)->irq_threshold_count;
#endif
#ifdef CONFIG_X86_MCE
	sum += per_cpu(mce_exception_count, cpu);
	sum += per_cpu(mce_poll_count, cpu);
#endif
	return sum;
}

u64 arch_irq_stat(void)
{
	u64 sum = atomic_read(&irq_err_count);
	return sum;
}


/*
 * do_IRQ handles all normal device IRQ's (the special
 * SMP cross-CPU interrupts have their own specific
 * handlers).
 */
__visible unsigned int __irq_entry do_IRQ(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);
	struct irq_desc * desc;
	/* high bit used in ret_from_ code  */
	unsigned vector = ~regs->orig_ax;

	/*
	 * NB: Unlike exception entries, IRQ entries do not reliably
	 * handle context tracking in the low-level entry code.  This is
	 * because syscall entries execute briefly with IRQs on before
	 * updating context tracking state, so we can take an IRQ from
	 * kernel mode with CONTEXT_USER.  The low-level entry code only
	 * updates the context if we came from user mode, so we won't
	 * switch to CONTEXT_KERNEL.  We'll fix that once the syscall
	 * code is cleaned up enough that we can cleanly defer enabling
	 * IRQs.
	 */

	entering_irq();

	/* entering_irq() tells RCU that we're not quiescent.  Check it. */
	RCU_LOCKDEP_WARN(!rcu_is_watching(), "IRQ failed to wake up RCU");

	desc = __this_cpu_read(vector_irq[vector]);

	if (!handle_irq(desc, regs)) {
		ack_APIC_irq();

		if (desc != VECTOR_RETRIGGERED) {
			pr_emerg_ratelimited("%s: %d.%d No irq handler for vector\n",
					     __func__, smp_processor_id(),
					     vector);
		} else {
			__this_cpu_write(vector_irq[vector], VECTOR_UNUSED);
		}
	}

	exiting_irq();

	set_irq_regs(old_regs);
	return 1;
}

/**
 * trigger_irq() - invoke interrupt associated with requested IRQ
 * @irq: target IRQ
 *
 * Search the interrupt vector table for the requested IRQ. If found,
 * invoke INT opcode for that vector number, so that the kernel will
 * then call the IRQ handler within interrupt context.
 *
 * Return: 0 if @irq was found, negative on error
 */
int trigger_irq(unsigned irq) {
	unsigned vector;
	struct irq_desc *desc;
	for (vector = FIRST_EXTERNAL_VECTOR; vector < NR_VECTORS; vector++) {
		desc = __this_cpu_read(vector_irq[vector]);
		if (!IS_ERR_OR_NULL(desc) && irq == irq_desc_get_irq(desc))
			goto found;
	}
	return -1;
 found:
	switch (vector) {
		case 0: { asm("int $0x00\n"); break; }
		case 1: { asm("int $0x01\n"); break; }
		case 2: { asm("int $0x02\n"); break; }
		case 3: { asm("int $0x03\n"); break; }
		case 4: { asm("int $0x04\n"); break; }
		case 5: { asm("int $0x05\n"); break; }
		case 6: { asm("int $0x06\n"); break; }
		case 7: { asm("int $0x07\n"); break; }
		case 8: { asm("int $0x08\n"); break; }
		case 9: { asm("int $0x09\n"); break; }
		case 10: { asm("int $0x0a\n"); break; }
		case 11: { asm("int $0x0b\n"); break; }
		case 12: { asm("int $0x0c\n"); break; }
		case 13: { asm("int $0x0d\n"); break; }
		case 14: { asm("int $0x0e\n"); break; }
		case 15: { asm("int $0x0f\n"); break; }
		case 16: { asm("int $0x10\n"); break; }
		case 17: { asm("int $0x11\n"); break; }
		case 18: { asm("int $0x12\n"); break; }
		case 19: { asm("int $0x13\n"); break; }
		case 20: { asm("int $0x14\n"); break; }
		case 21: { asm("int $0x15\n"); break; }
		case 22: { asm("int $0x16\n"); break; }
		case 23: { asm("int $0x17\n"); break; }
		case 24: { asm("int $0x18\n"); break; }
		case 25: { asm("int $0x19\n"); break; }
		case 26: { asm("int $0x1a\n"); break; }
		case 27: { asm("int $0x1b\n"); break; }
		case 28: { asm("int $0x1c\n"); break; }
		case 29: { asm("int $0x1d\n"); break; }
		case 30: { asm("int $0x1e\n"); break; }
		case 31: { asm("int $0x1f\n"); break; }
		case 32: { asm("int $0x20\n"); break; }
		case 33: { asm("int $0x21\n"); break; }
		case 34: { asm("int $0x22\n"); break; }
		case 35: { asm("int $0x23\n"); break; }
		case 36: { asm("int $0x24\n"); break; }
		case 37: { asm("int $0x25\n"); break; }
		case 38: { asm("int $0x26\n"); break; }
		case 39: { asm("int $0x27\n"); break; }
		case 40: { asm("int $0x28\n"); break; }
		case 41: { asm("int $0x29\n"); break; }
		case 42: { asm("int $0x2a\n"); break; }
		case 43: { asm("int $0x2b\n"); break; }
		case 44: { asm("int $0x2c\n"); break; }
		case 45: { asm("int $0x2d\n"); break; }
		case 46: { asm("int $0x2e\n"); break; }
		case 47: { asm("int $0x2f\n"); break; }
		case 48: { asm("int $0x30\n"); break; }
		case 49: { asm("int $0x31\n"); break; }
		case 50: { asm("int $0x32\n"); break; }
		case 51: { asm("int $0x33\n"); break; }
		case 52: { asm("int $0x34\n"); break; }
		case 53: { asm("int $0x35\n"); break; }
		case 54: { asm("int $0x36\n"); break; }
		case 55: { asm("int $0x37\n"); break; }
		case 56: { asm("int $0x38\n"); break; }
		case 57: { asm("int $0x39\n"); break; }
		case 58: { asm("int $0x3a\n"); break; }
		case 59: { asm("int $0x3b\n"); break; }
		case 60: { asm("int $0x3c\n"); break; }
		case 61: { asm("int $0x3d\n"); break; }
		case 62: { asm("int $0x3e\n"); break; }
		case 63: { asm("int $0x3f\n"); break; }
		case 64: { asm("int $0x40\n"); break; }
		case 65: { asm("int $0x41\n"); break; }
		case 66: { asm("int $0x42\n"); break; }
		case 67: { asm("int $0x43\n"); break; }
		case 68: { asm("int $0x44\n"); break; }
		case 69: { asm("int $0x45\n"); break; }
		case 70: { asm("int $0x46\n"); break; }
		case 71: { asm("int $0x47\n"); break; }
		case 72: { asm("int $0x48\n"); break; }
		case 73: { asm("int $0x49\n"); break; }
		case 74: { asm("int $0x4a\n"); break; }
		case 75: { asm("int $0x4b\n"); break; }
		case 76: { asm("int $0x4c\n"); break; }
		case 77: { asm("int $0x4d\n"); break; }
		case 78: { asm("int $0x4e\n"); break; }
		case 79: { asm("int $0x4f\n"); break; }
		case 80: { asm("int $0x50\n"); break; }
		case 81: { asm("int $0x51\n"); break; }
		case 82: { asm("int $0x52\n"); break; }
		case 83: { asm("int $0x53\n"); break; }
		case 84: { asm("int $0x54\n"); break; }
		case 85: { asm("int $0x55\n"); break; }
		case 86: { asm("int $0x56\n"); break; }
		case 87: { asm("int $0x57\n"); break; }
		case 88: { asm("int $0x58\n"); break; }
		case 89: { asm("int $0x59\n"); break; }
		case 90: { asm("int $0x5a\n"); break; }
		case 91: { asm("int $0x5b\n"); break; }
		case 92: { asm("int $0x5c\n"); break; }
		case 93: { asm("int $0x5d\n"); break; }
		case 94: { asm("int $0x5e\n"); break; }
		case 95: { asm("int $0x5f\n"); break; }
		case 96: { asm("int $0x60\n"); break; }
		case 97: { asm("int $0x61\n"); break; }
		case 98: { asm("int $0x62\n"); break; }
		case 99: { asm("int $0x63\n"); break; }
		case 100: { asm("int $0x64\n"); break; }
		case 101: { asm("int $0x65\n"); break; }
		case 102: { asm("int $0x66\n"); break; }
		case 103: { asm("int $0x67\n"); break; }
		case 104: { asm("int $0x68\n"); break; }
		case 105: { asm("int $0x69\n"); break; }
		case 106: { asm("int $0x6a\n"); break; }
		case 107: { asm("int $0x6b\n"); break; }
		case 108: { asm("int $0x6c\n"); break; }
		case 109: { asm("int $0x6d\n"); break; }
		case 110: { asm("int $0x6e\n"); break; }
		case 111: { asm("int $0x6f\n"); break; }
		case 112: { asm("int $0x70\n"); break; }
		case 113: { asm("int $0x71\n"); break; }
		case 114: { asm("int $0x72\n"); break; }
		case 115: { asm("int $0x73\n"); break; }
		case 116: { asm("int $0x74\n"); break; }
		case 117: { asm("int $0x75\n"); break; }
		case 118: { asm("int $0x76\n"); break; }
		case 119: { asm("int $0x77\n"); break; }
		case 120: { asm("int $0x78\n"); break; }
		case 121: { asm("int $0x79\n"); break; }
		case 122: { asm("int $0x7a\n"); break; }
		case 123: { asm("int $0x7b\n"); break; }
		case 124: { asm("int $0x7c\n"); break; }
		case 125: { asm("int $0x7d\n"); break; }
		case 126: { asm("int $0x7e\n"); break; }
		case 127: { asm("int $0x7f\n"); break; }
		case 128: { asm("int $0x80\n"); break; }
		case 129: { asm("int $0x81\n"); break; }
		case 130: { asm("int $0x82\n"); break; }
		case 131: { asm("int $0x83\n"); break; }
		case 132: { asm("int $0x84\n"); break; }
		case 133: { asm("int $0x85\n"); break; }
		case 134: { asm("int $0x86\n"); break; }
		case 135: { asm("int $0x87\n"); break; }
		case 136: { asm("int $0x88\n"); break; }
		case 137: { asm("int $0x89\n"); break; }
		case 138: { asm("int $0x8a\n"); break; }
		case 139: { asm("int $0x8b\n"); break; }
		case 140: { asm("int $0x8c\n"); break; }
		case 141: { asm("int $0x8d\n"); break; }
		case 142: { asm("int $0x8e\n"); break; }
		case 143: { asm("int $0x8f\n"); break; }
		case 144: { asm("int $0x90\n"); break; }
		case 145: { asm("int $0x91\n"); break; }
		case 146: { asm("int $0x92\n"); break; }
		case 147: { asm("int $0x93\n"); break; }
		case 148: { asm("int $0x94\n"); break; }
		case 149: { asm("int $0x95\n"); break; }
		case 150: { asm("int $0x96\n"); break; }
		case 151: { asm("int $0x97\n"); break; }
		case 152: { asm("int $0x98\n"); break; }
		case 153: { asm("int $0x99\n"); break; }
		case 154: { asm("int $0x9a\n"); break; }
		case 155: { asm("int $0x9b\n"); break; }
		case 156: { asm("int $0x9c\n"); break; }
		case 157: { asm("int $0x9d\n"); break; }
		case 158: { asm("int $0x9e\n"); break; }
		case 159: { asm("int $0x9f\n"); break; }
		case 160: { asm("int $0xa0\n"); break; }
		case 161: { asm("int $0xa1\n"); break; }
		case 162: { asm("int $0xa2\n"); break; }
		case 163: { asm("int $0xa3\n"); break; }
		case 164: { asm("int $0xa4\n"); break; }
		case 165: { asm("int $0xa5\n"); break; }
		case 166: { asm("int $0xa6\n"); break; }
		case 167: { asm("int $0xa7\n"); break; }
		case 168: { asm("int $0xa8\n"); break; }
		case 169: { asm("int $0xa9\n"); break; }
		case 170: { asm("int $0xaa\n"); break; }
		case 171: { asm("int $0xab\n"); break; }
		case 172: { asm("int $0xac\n"); break; }
		case 173: { asm("int $0xad\n"); break; }
		case 174: { asm("int $0xae\n"); break; }
		case 175: { asm("int $0xaf\n"); break; }
		case 176: { asm("int $0xb0\n"); break; }
		case 177: { asm("int $0xb1\n"); break; }
		case 178: { asm("int $0xb2\n"); break; }
		case 179: { asm("int $0xb3\n"); break; }
		case 180: { asm("int $0xb4\n"); break; }
		case 181: { asm("int $0xb5\n"); break; }
		case 182: { asm("int $0xb6\n"); break; }
		case 183: { asm("int $0xb7\n"); break; }
		case 184: { asm("int $0xb8\n"); break; }
		case 185: { asm("int $0xb9\n"); break; }
		case 186: { asm("int $0xba\n"); break; }
		case 187: { asm("int $0xbb\n"); break; }
		case 188: { asm("int $0xbc\n"); break; }
		case 189: { asm("int $0xbd\n"); break; }
		case 190: { asm("int $0xbe\n"); break; }
		case 191: { asm("int $0xbf\n"); break; }
		case 192: { asm("int $0xc0\n"); break; }
		case 193: { asm("int $0xc1\n"); break; }
		case 194: { asm("int $0xc2\n"); break; }
		case 195: { asm("int $0xc3\n"); break; }
		case 196: { asm("int $0xc4\n"); break; }
		case 197: { asm("int $0xc5\n"); break; }
		case 198: { asm("int $0xc6\n"); break; }
		case 199: { asm("int $0xc7\n"); break; }
		case 200: { asm("int $0xc8\n"); break; }
		case 201: { asm("int $0xc9\n"); break; }
		case 202: { asm("int $0xca\n"); break; }
		case 203: { asm("int $0xcb\n"); break; }
		case 204: { asm("int $0xcc\n"); break; }
		case 205: { asm("int $0xcd\n"); break; }
		case 206: { asm("int $0xce\n"); break; }
		case 207: { asm("int $0xcf\n"); break; }
		case 208: { asm("int $0xd0\n"); break; }
		case 209: { asm("int $0xd1\n"); break; }
		case 210: { asm("int $0xd2\n"); break; }
		case 211: { asm("int $0xd3\n"); break; }
		case 212: { asm("int $0xd4\n"); break; }
		case 213: { asm("int $0xd5\n"); break; }
		case 214: { asm("int $0xd6\n"); break; }
		case 215: { asm("int $0xd7\n"); break; }
		case 216: { asm("int $0xd8\n"); break; }
		case 217: { asm("int $0xd9\n"); break; }
		case 218: { asm("int $0xda\n"); break; }
		case 219: { asm("int $0xdb\n"); break; }
		case 220: { asm("int $0xdc\n"); break; }
		case 221: { asm("int $0xdd\n"); break; }
		case 222: { asm("int $0xde\n"); break; }
		case 223: { asm("int $0xdf\n"); break; }
		case 224: { asm("int $0xe0\n"); break; }
		case 225: { asm("int $0xe1\n"); break; }
		case 226: { asm("int $0xe2\n"); break; }
		case 227: { asm("int $0xe3\n"); break; }
		case 228: { asm("int $0xe4\n"); break; }
		case 229: { asm("int $0xe5\n"); break; }
		case 230: { asm("int $0xe6\n"); break; }
		case 231: { asm("int $0xe7\n"); break; }
		case 232: { asm("int $0xe8\n"); break; }
		case 233: { asm("int $0xe9\n"); break; }
		case 234: { asm("int $0xea\n"); break; }
		case 235: { asm("int $0xeb\n"); break; }
		case 236: { asm("int $0xec\n"); break; }
		case 237: { asm("int $0xed\n"); break; }
		case 238: { asm("int $0xee\n"); break; }
		case 239: { asm("int $0xef\n"); break; }
		case 240: { asm("int $0xf0\n"); break; }
		case 241: { asm("int $0xf1\n"); break; }
		case 242: { asm("int $0xf2\n"); break; }
		case 243: { asm("int $0xf3\n"); break; }
		case 244: { asm("int $0xf4\n"); break; }
		case 245: { asm("int $0xf5\n"); break; }
		case 246: { asm("int $0xf6\n"); break; }
		case 247: { asm("int $0xf7\n"); break; }
		case 248: { asm("int $0xf8\n"); break; }
		case 249: { asm("int $0xf9\n"); break; }
		case 250: { asm("int $0xfa\n"); break; }
		case 251: { asm("int $0xfb\n"); break; }
		case 252: { asm("int $0xfc\n"); break; }
		case 253: { asm("int $0xfd\n"); break; }
		case 254: { asm("int $0xfe\n"); break; }
		case 255: { asm("int $0xff\n"); break; }
	default:
		return -1;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(trigger_irq);

/*
 * Handler for X86_PLATFORM_IPI_VECTOR.
 */
void __smp_x86_platform_ipi(void)
{
	inc_irq_stat(x86_platform_ipis);

	if (x86_platform_ipi_callback)
		x86_platform_ipi_callback();
}

__visible void smp_x86_platform_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	__smp_x86_platform_ipi();
	exiting_irq();
	set_irq_regs(old_regs);
}

#ifdef CONFIG_HAVE_KVM
static void dummy_handler(void) {}
static void (*kvm_posted_intr_wakeup_handler)(void) = dummy_handler;

void kvm_set_posted_intr_wakeup_handler(void (*handler)(void))
{
	if (handler)
		kvm_posted_intr_wakeup_handler = handler;
	else
		kvm_posted_intr_wakeup_handler = dummy_handler;
}
EXPORT_SYMBOL_GPL(kvm_set_posted_intr_wakeup_handler);

/*
 * Handler for POSTED_INTERRUPT_VECTOR.
 */
__visible void smp_kvm_posted_intr_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	inc_irq_stat(kvm_posted_intr_ipis);
	exiting_irq();
	set_irq_regs(old_regs);
}

/*
 * Handler for POSTED_INTERRUPT_WAKEUP_VECTOR.
 */
__visible void smp_kvm_posted_intr_wakeup_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	inc_irq_stat(kvm_posted_intr_wakeup_ipis);
	kvm_posted_intr_wakeup_handler();
	exiting_irq();
	set_irq_regs(old_regs);
}
#endif

__visible void smp_trace_x86_platform_ipi(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	entering_ack_irq();
	trace_x86_platform_ipi_entry(X86_PLATFORM_IPI_VECTOR);
	__smp_x86_platform_ipi();
	trace_x86_platform_ipi_exit(X86_PLATFORM_IPI_VECTOR);
	exiting_irq();
	set_irq_regs(old_regs);
}

EXPORT_SYMBOL_GPL(vector_used_by_percpu_irq);

#ifdef CONFIG_HOTPLUG_CPU

/* These two declarations are only used in check_irq_vectors_for_cpu_disable()
 * below, which is protected by stop_machine().  Putting them on the stack
 * results in a stack frame overflow.  Dynamically allocating could result in a
 * failure so declare these two cpumasks as global.
 */
static struct cpumask affinity_new, online_new;

/*
 * This cpu is going to be removed and its vectors migrated to the remaining
 * online cpus.  Check to see if there are enough vectors in the remaining cpus.
 * This function is protected by stop_machine().
 */
int check_irq_vectors_for_cpu_disable(void)
{
	unsigned int this_cpu, vector, this_count, count;
	struct irq_desc *desc;
	struct irq_data *data;
	int cpu;

	this_cpu = smp_processor_id();
	cpumask_copy(&online_new, cpu_online_mask);
	cpumask_clear_cpu(this_cpu, &online_new);

	this_count = 0;
	for (vector = FIRST_EXTERNAL_VECTOR; vector < NR_VECTORS; vector++) {
		desc = __this_cpu_read(vector_irq[vector]);
		if (IS_ERR_OR_NULL(desc))
			continue;
		/*
		 * Protect against concurrent action removal, affinity
		 * changes etc.
		 */
		raw_spin_lock(&desc->lock);
		data = irq_desc_get_irq_data(desc);
		cpumask_copy(&affinity_new,
			     irq_data_get_affinity_mask(data));
		cpumask_clear_cpu(this_cpu, &affinity_new);

		/* Do not count inactive or per-cpu irqs. */
		if (!irq_desc_has_action(desc) || irqd_is_per_cpu(data)) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		raw_spin_unlock(&desc->lock);
		/*
		 * A single irq may be mapped to multiple cpu's
		 * vector_irq[] (for example IOAPIC cluster mode).  In
		 * this case we have two possibilities:
		 *
		 * 1) the resulting affinity mask is empty; that is
		 * this the down'd cpu is the last cpu in the irq's
		 * affinity mask, or
		 *
		 * 2) the resulting affinity mask is no longer a
		 * subset of the online cpus but the affinity mask is
		 * not zero; that is the down'd cpu is the last online
		 * cpu in a user set affinity mask.
		 */
		if (cpumask_empty(&affinity_new) ||
		    !cpumask_subset(&affinity_new, &online_new))
			this_count++;
	}

	count = 0;
	for_each_online_cpu(cpu) {
		if (cpu == this_cpu)
			continue;
		/*
		 * We scan from FIRST_EXTERNAL_VECTOR to first system
		 * vector. If the vector is marked in the used vectors
		 * bitmap or an irq is assigned to it, we don't count
		 * it as available.
		 *
		 * As this is an inaccurate snapshot anyway, we can do
		 * this w/o holding vector_lock.
		 */
		for (vector = FIRST_EXTERNAL_VECTOR;
		     vector < first_system_vector; vector++) {
			if (!test_bit(vector, used_vectors) &&
			    IS_ERR_OR_NULL(per_cpu(vector_irq, cpu)[vector]))
			    count++;
		}
	}

	if (count < this_count) {
		pr_warn("CPU %d disable failed: CPU has %u vectors assigned and there are only %u available.\n",
			this_cpu, this_count, count);
		return -ERANGE;
	}
	return 0;
}

/* A cpu has been removed from cpu_online_mask.  Reset irq affinities. */
void fixup_irqs(void)
{
	unsigned int irq, vector;
	static int warned;
	struct irq_desc *desc;
	struct irq_data *data;
	struct irq_chip *chip;
	int ret;

	for_each_irq_desc(irq, desc) {
		int break_affinity = 0;
		int set_affinity = 1;
		const struct cpumask *affinity;

		if (!desc)
			continue;
		if (irq == 2)
			continue;

		/* interrupt's are disabled at this point */
		raw_spin_lock(&desc->lock);

		data = irq_desc_get_irq_data(desc);
		affinity = irq_data_get_affinity_mask(data);
		if (!irq_has_action(irq) || irqd_is_per_cpu(data) ||
		    cpumask_subset(affinity, cpu_online_mask)) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		/*
		 * Complete the irq move. This cpu is going down and for
		 * non intr-remapping case, we can't wait till this interrupt
		 * arrives at this cpu before completing the irq move.
		 */
		irq_force_complete_move(desc);

		if (cpumask_any_and(affinity, cpu_online_mask) >= nr_cpu_ids) {
			break_affinity = 1;
			affinity = cpu_online_mask;
		}

		chip = irq_data_get_irq_chip(data);
		/*
		 * The interrupt descriptor might have been cleaned up
		 * already, but it is not yet removed from the radix tree
		 */
		if (!chip) {
			raw_spin_unlock(&desc->lock);
			continue;
		}

		if (!irqd_can_move_in_process_context(data) && chip->irq_mask)
			chip->irq_mask(data);

		if (chip->irq_set_affinity) {
			ret = chip->irq_set_affinity(data, affinity, true);
			if (ret == -ENOSPC)
				pr_crit("IRQ %d set affinity failed because there are no available vectors.  The device assigned to this IRQ is unstable.\n", irq);
		} else {
			if (!(warned++))
				set_affinity = 0;
		}

		/*
		 * We unmask if the irq was not marked masked by the
		 * core code. That respects the lazy irq disable
		 * behaviour.
		 */
		if (!irqd_can_move_in_process_context(data) &&
		    !irqd_irq_masked(data) && chip->irq_unmask)
			chip->irq_unmask(data);

		raw_spin_unlock(&desc->lock);

		if (break_affinity && set_affinity)
			pr_notice("Broke affinity for irq %i\n", irq);
		else if (!set_affinity)
			pr_notice("Cannot set affinity for irq %i\n", irq);
	}

	/*
	 * We can remove mdelay() and then send spuriuous interrupts to
	 * new cpu targets for all the irqs that were handled previously by
	 * this cpu. While it works, I have seen spurious interrupt messages
	 * (nothing wrong but still...).
	 *
	 * So for now, retain mdelay(1) and check the IRR and then send those
	 * interrupts to new targets as this cpu is already offlined...
	 */
	mdelay(1);

	/*
	 * We can walk the vector array of this cpu without holding
	 * vector_lock because the cpu is already marked !online, so
	 * nothing else will touch it.
	 */
	for (vector = FIRST_EXTERNAL_VECTOR; vector < NR_VECTORS; vector++) {
		unsigned int irr;

		if (IS_ERR_OR_NULL(__this_cpu_read(vector_irq[vector])))
			continue;

		irr = apic_read(APIC_IRR + (vector / 32 * 0x10));
		if (irr  & (1 << (vector % 32))) {
			desc = __this_cpu_read(vector_irq[vector]);

			raw_spin_lock(&desc->lock);
			data = irq_desc_get_irq_data(desc);
			chip = irq_data_get_irq_chip(data);
			if (chip->irq_retrigger) {
				chip->irq_retrigger(data);
				__this_cpu_write(vector_irq[vector], VECTOR_RETRIGGERED);
			}
			raw_spin_unlock(&desc->lock);
		}
		if (__this_cpu_read(vector_irq[vector]) != VECTOR_RETRIGGERED)
			__this_cpu_write(vector_irq[vector], VECTOR_UNUSED);
	}
}
#endif
