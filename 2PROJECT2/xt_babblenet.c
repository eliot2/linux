/*
 * Raise an interrupt upon each target skb, storing it for later
 * processing by babbler2 driver.
 *
 * Copyright(c) 2016 Jason Tang <jtang@umbc.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "BabbleNet: " fmt

#include <linux/completion.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_LOG.h>

#include <linux/ip.h>
#include <linux/tcp.h>

/* defined in arch/x86/kernel/irq.c */
extern int trigger_irq(unsigned);

#define BABBLENET_IRQ 6
static DEFINE_SPINLOCK(babble_lock);
static DECLARE_COMPLETION(babble_retrieved);
static bool babblenet_enabled;
struct workqueue_struct *babblenet_wq;

struct babble {
	char *data;
	size_t len;
	struct list_head list;
};
static LIST_HEAD(babble_list);

/**
 * babblenet_enable() - start capturing babbles from the network
 *
 * This device will now raise interrupts when babbles arrive.
 */
void babblenet_enable(void)
{
	babblenet_enabled = true;
}

EXPORT_SYMBOL(babblenet_enable);

/**
 * babblenet_disable() - stop capturing babbles from the network
 *
 * This device will stop raising interrupts when babbles arrive.
 */
void babblenet_disable(void)
{
	babblenet_enabled = false;
	complete_all(&babble_retrieved);
}

EXPORT_SYMBOL(babblenet_disable);

/**
 * babblenet_get_babble() - retrieve the oldest pending babble
 *
 * This function is safe to be called from within interrupt context.
 *
 * If all babbles have been retrieved, then this function returns
 * NULL. Otherwise, it returns the oldest babble.
 *
 * WARNING: The returned buffer is NOT A STRING; it is not necessarily
 * null-terminated.
 *
 * @len: pointer to where to write the babble length
 * Return: buffer containing babble, or %NULL if none pending
 */
const char *babblenet_get_babble(size_t * const len)
{
	struct babble *oldest;
	unsigned long flags;

	spin_lock_irqsave(&babble_lock, flags);
	oldest = list_first_entry_or_null(&babble_list, struct babble, list);
	if (!oldest) {
		spin_unlock_irqrestore(&babble_lock, flags);
		*len = 0;
		return NULL;
	}
	spin_unlock_irqrestore(&babble_lock, flags);

	complete(&babble_retrieved);
	*len = oldest->len;
	return oldest->data;
}

EXPORT_SYMBOL(babblenet_get_babble);

/**
 * babblenet_work_func() - function invoked by the workqueue
 *
 * Raise an interrupt, then wait for the interrupt handler to
 * acknowledge the interrupt.
 */
static void babblenet_work_func(struct work_struct *work)
{
	struct babble *oldest;
	unsigned long flags;

	pr_info("Raising interrupt %u\n", BABBLENET_IRQ);
	if (trigger_irq(BABBLENET_IRQ) < 0) {
		pr_err("Could not generate interrupt\n");
	} else {
		wait_for_completion_interruptible(&babble_retrieved);
		pr_info("Interrupt was cleared\n");
		reinit_completion(&babble_retrieved);
	}

	spin_lock_irqsave(&babble_lock, flags);
	oldest = list_first_entry_or_null(&babble_list, struct babble, list);
	if (oldest) {
		list_del(&oldest->list);
		kfree(oldest->data);
		kfree(oldest);
	}
	spin_unlock_irqrestore(&babble_lock, flags);
}

static DECLARE_WORK(babblenet_work, babblenet_work_func);

/**
 * filter_tg() - perform packet mangling
 *
 * For each skb, if it is a TCP packet then add it to the payload list
 * and schedule an interrupt.
 */
static unsigned int
filter_tg(struct sk_buff *skb, const struct xt_action_param *par)
{
	struct iphdr *iph = ip_hdr(skb);
	u8 hdr_len;
	struct babble *babble;
	unsigned long flags;

	if (!babblenet_enabled || !iph || iph->protocol != IPPROTO_TCP)
		return XT_CONTINUE;
	hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
	if (skb->len > hdr_len) {
		babble = kmalloc(sizeof(*babble), GFP_ATOMIC);
		if (!babble)
			goto out;
		babble->len = skb->len - hdr_len;
		babble->data = kmalloc(babble->len, GFP_ATOMIC);
		if (!babble->data) {
			kfree(babble);
			goto out;
		}
		memcpy(babble->data, skb->data + hdr_len, babble->len);
		spin_lock_irqsave(&babble_lock, flags);
		list_add_tail(&babble->list, &babble_list);
		spin_unlock_irqrestore(&babble_lock, flags);
		queue_work(babblenet_wq, &babblenet_work);
	}
out:
	return XT_CONTINUE;
}

static struct xt_target filter_tg_reg __read_mostly = {
	.name = "LOG",
	.family = NFPROTO_IPV4,
	.target = filter_tg,
	.targetsize = sizeof(struct xt_log_info),
	.table = "mangle",
	.me = THIS_MODULE,
};

static int __init babblenet_init(void)
{
	int retval;

	babblenet_wq = create_singlethread_workqueue("BabbleNet");
	if (!babblenet_wq) {
		retval = -ENOMEM;
		goto out;
	}
	retval = xt_register_target(&filter_tg_reg);
	if (retval < 0)
		destroy_workqueue(babblenet_wq);
out:
	pr_info("initialization returning %d\n", retval);
	return retval;
}

static void __exit babblenet_exit(void)
{
	struct babble *babble, *tmp;

	babblenet_enabled = false;
	xt_unregister_target(&filter_tg_reg);
	complete_all(&babble_retrieved);
	cancel_work_sync(&babblenet_work);
	destroy_workqueue(babblenet_wq);
	list_for_each_entry_safe(babble, tmp, &babble_list, list) {
		kfree(babble->data);
		kfree(babble);
	}
	pr_info("exited\n");
}

module_init(babblenet_init);
module_exit(babblenet_exit);

MODULE_DESCRIPTION("CS421 BabbleNet");
MODULE_LICENSE("GPL");
