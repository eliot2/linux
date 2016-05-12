/* Name: Eliot Carney-Seim
   Email:eliot2@umbc.edu
   Sources:
   http://www.makelinux.net/books/lkd2/ch11lev1sec5
   http://stackoverflow.com/questions/6515227/source-code-example-from-linux-kernel-programming
*/

/* 
 * This file uses kernel-doc style comments, which is similar to
 * Javadoc and Doxygen-style comments.  See
 * ~/linux/Documentation/kernel-doc-nano-HOWTO.txt for details.
 */

/*
 * Getting compilation warnings?  The Linux kernel is written against
 * C89, which means:
 *  - No // comments, and
 *  - All variables must be declared at the top of functions.
 * Read ~/linux/Documentation/CodingStyle to ensure your project
 * compiles without warnings.
 */

#define pr_fmt(fmt) "Babbler: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>	/* for threads */
#include <linux/sched.h>	/* for task_struct */
#include <asm/uaccess.h>
#include <linux/cred.h>

#define MAX_TOPIC_LEN 8
#define MAX_BABBLE_LEN 140

static char *topics_buffer;
static int babble_size;
const int BABBLE_LEN = 140;
const int TOPIC_LEN = 8;
const int BYTE_SIZE = 8;
static char *BABBLE;
int writeAmt;
static int overflow;

static DEFINE_SPINLOCK(my_lock);

struct topics_list {
	char topic[9];
	struct list_head list;
};
static struct list_head topics_head;
static char *topics_ptr; /*latest locally*/

/**
 * cpyStr() - No return.
 * @to: cstring to copy to
 * @from: cstring to copy from
 * @toLen: length of @to 
 * @fromLen: length of @from
 *
 * Can copy strings given length control. Smaller 
 * length is copy amount.
 */
void cpyStr(char *to, char const *from, int toLen, int fromLen)
{
	int i;
	if (toLen < fromLen) {
		i = toLen;
	} else {
		i = fromLen;
	}

	for (; i > 0; i--) {
		to[i - 1] = from[i - 1];
	}
}

/*
 * Read xt_babblenet.c to learn how to use these functions.
 */
#define BABBLENET_IRQ 6
extern void babblenet_enable(void);
extern void babblenet_disable(void);
extern const char *babblenet_get_babble(size_t * const);

/**
 * babbler_read() - callback invoked when a process reads from
 * /dev/babbler
 * @ubuf: destination to store babble
 *
 * If the most recent babble begins with an '@' character followed by
 * an integer, the babble is requesting privacy. If the calling uid
 * does not match the value, simply return 0; do not clear the babble.
 *
 * Otherwise, upon a match or if the babble is not requesting privacy,
 * write to @ubuf the most recent babble, but only if the babble
 * contains a topic of interest to the caller. Copy the lesser of
 * @count and babble's length.
 *
 * If there are no babbles to be read (or the most recent babble does
 * not contain any topics of interest), do nothing and return 0.
 *
 * Clear the babble if the babble matches the calling user or if
 * privacy is not requested, regardless if it had a matching topic or
 * not. If privacy was requested and the current user does not match,
 * do not clear the babble.
 *
 * HINT: strstr() may be of use here, but only if your topics and
 * babble buffers are already null-terminated.
 *
 * HINT: kstrtoul(), current_uid(), and __kuid_val() may be of use
 * here.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t babbler_read(struct file *filp, char __user * ubuf,
			    size_t count, loff_t * ppos)
{
	int attempts = 0;
	int ret = 0;
	struct topics_list *find_entry;
	struct list_head *pos, *n;

	unsigned int myUID = current_uid().val;

	int somethingFound = 0;
	
	/*
	 * In part 4, if the babble's privacy option does not match
	 * the current user, then return 0.
	 */
	/* YOUR CODE HERE */
	writeAmt = (babble_size < count) ? babble_size : count;

	/*
	 * In part 3, copy to the user if the babble contains any
	 * topic of interest. If there are no topics, then return 0.
	 */

	while (!ret) {
		attempts++;
		ret = spin_trylock(&my_lock);
		if(attempts > 20){
			/*Lock shouldn't be held that long*/
			printk(KERN_INFO "Unable to hold lock");
			return -EDEADLK;
		}		
	}

	printk("Lock Acquired in Read; Attempts: %d\n", attempts);
	attempts = 0;

	list_for_each_safe(pos, n, &topics_head) { 
		find_entry = list_entry(pos, struct topics_list, list);
		printk(KERN_INFO "Testing find: %s | against | %s\n", BABBLE,
		       find_entry->topic);
		if(strstr(BABBLE, find_entry->topic) != NULL){
			ret = copy_to_user(ubuf, BABBLE, writeAmt);
			if(ret != 0){
				pr_err("Failed copying '%s' to user.\n", BABBLE);
				pr_err("Fail on return was: %d.\n", ret);
				spin_unlock(&my_lock);
				return -EFAULT;
			}
			somethingFound = 1;
		}
	}

	if(!somethingFound)
		pr_info("No match found.\n");

	memset(BABBLE, 0, BABBLE_LEN);
	babble_size = 0;
	spin_unlock(&my_lock);

	return writeAmt;
}

/**
 * babbler_write() - helper invoked when a process writes to
 * /dev/babbler
 * @ubuf: source buffer of bytes from user
 * @: number of bytes in @ubuf
 *
 * Copy the contents of @ubuf, up to @count bytes, to the current
 * babble buffer, overwriting any previously stored babbles. If @count
 * is greater than 140, copy only the first 140 characters. Note that
 * @ubuf is NOT a string; you can NOT use strcpy() or strlen() on it.
 *
 * Return: @count, or negative on error
 */
static ssize_t babbler_write(struct file *filp, const char __user * ubuf,
			     size_t count, loff_t * ppos)
{
	int attempts = 0;
	int ret;
	if (count > BABBLE_LEN)
		count = BABBLE_LEN;

	ret = 0;

	while (!ret) {
		attempts++;
		ret = spin_trylock(&my_lock);
		if(attempts > 20){
			/*If Lock can't be got, do nothing.*/
			printk(KERN_INFO "Unable to hold lock");
			return -EDEADLK;
		}		
	}

	printk("Lock Acquired in Write; Attempts: %d\n", attempts);
	attempts = 0;
	babble_size = count;

	memset(BABBLE, 0, BABBLE_LEN);
	overflow = (int)copy_from_user(BABBLE, ubuf, count);
	overflow++;
	overflow--;

	spin_unlock(&my_lock);
	return babble_size;
}

/**
 * babbler_ctl_write() - callback invoked when a process writes to
 * /dev/babbler_ctl
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * If @ubuf does not begin with an octothorpe, ignore the request and
 * do nothing; this is not an error.
 *
 * Otherwise, add the contents of @ubuf, up to @count bytes, to the
 * list of topics of interest. If @count is greater than 8, copy only
 * the first 8 characters. Note that @ubuf is NOT a string; you can
 * NOT use strcpy() or strlen() on it.
 *
 * Any time the topics of interest changes, update the contents of
 * topics_buffer. Copy the topics of interest (if any) into the
 * buffer, newline separated. Do not put a newline after the last
 * topic. Set all unused bytes in the buffer to zero. HINT: memcpy()
 * and memset() may be of use here.
 *
 * Return: @count, or negative on error
 */
static ssize_t babbler_ctl_write(struct file *filp, const char __user * ubuf,
				 size_t count, loff_t * ppos)
{
	/*
	 * In part 3, update this code to dynamically allocate space
	 * for each topic and append it to a list. Then update the
	 * contents of topics_buffer. Assume the user will never add a
	 * topic that was already added.
	 */
	int attempts = 0;
	int ret;
	char octo = '#';
	struct topics_list *entry = (struct topics_list *) kmalloc(sizeof(*entry), 0);

	if (octo != *ubuf)
		return count;

	if (count > TOPIC_LEN)
		count = TOPIC_LEN;	
		
	ret = 0;

	while (!ret) {
		attempts++;
		ret = spin_trylock(&my_lock);
		if(attempts > 20){
			/*Lock shouldn't be held that long */
			printk(KERN_INFO "Unable to hold lock");
			return -EDEADLK;
		}		
	}

	printk("Lock Acquired in Ctl Write; Attempts: %d\n", attempts);
	attempts = 0;

	memset(entry->topic, 0, TOPIC_LEN);
	overflow = (int)copy_from_user(entry->topic, ubuf, count);
	if(overflow != 0){
		pr_info("Error in 'copy_from_user' attempt. Value is: %d\n", overflow);
		spin_unlock(&my_lock);
		return -EFAULT;
	}

	

	INIT_LIST_HEAD(&entry->list);
	list_add_tail(&entry->list, &topics_head);

	/*Don't add newline on first ctl write */
	if(topics_buffer != topics_ptr){
		memcpy(topics_ptr, "\n", 1);
		topics_ptr++;
	}
	memcpy(topics_ptr, ubuf, count);
	topics_ptr += count;
	
	pr_info("Current topics buffer location: %p\n", topics_buffer);
	pr_info("Topics head location is at    : %p\n", topics_ptr);
	spin_unlock(&my_lock);
	return count;
}

/**
 * babbler_ctl_mmap() - callback invoked when a process mmap()s to
 * /dev/babbler_ctl
 * @filp: process's file object that is mapping to this device (ignored)
 * @vma: virtual memory allocation object containing mmap() request
 *
 * Create a read-only mapping from kernel memory (specifically,
 * topics_buffer) into user space.
 *
 * You do not need to modify this function.
 *
 * Code based upon
 * http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace
 *
 * Return: 0 on success, negative on error.
 */
static int babbler_ctl_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long size = (unsigned long)(vma->vm_end - vma->vm_start);
	unsigned long page = vmalloc_to_pfn(topics_buffer);
	if (size > PAGE_SIZE)
		return -EIO;
	vma->vm_pgoff = 0;
	vma->vm_page_prot = PAGE_READONLY;
	if (remap_pfn_range(vma, vma->vm_start, page, size, vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

static const struct file_operations babbler_fops = {
	.read = babbler_read,
	.write = babbler_write,
};

static struct miscdevice babbler_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "babbler",
	.fops = &babbler_fops,
	.mode = 0666
};

static const struct file_operations babbler_ctl_fops = {
	.write = babbler_ctl_write,
	.mmap = babbler_ctl_mmap,
};

static struct miscdevice babbler_ctl_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "babbler_ctl",
	.fops = &babbler_ctl_fops,
	.mode = 0666
};

/**
 * babblenet_top() - top-half of BabbleNet ISR
 * @irq: IRQ that was invoked (ignored)
 * @cookie: Pointer to data that was passed into
 * request_threaded_irq() (ignored)
 *
 * If @irq is BABBLENET_IRQ, then wake up the bottom-half. Otherwise,
 * return IRQ_NONE.
 */
static irqreturn_t babblenet_top(int irq, void *cookie)
{
	if (irq == BABBLENET_IRQ)
		return IRQ_WAKE_THREAD;
	else
		return IRQ_NONE;
}

/**
 * babblenet_bottom() - bottom-half to BabbleNet ISR
 * @irq: IRQ that was invoked (ignore)
 * @cookie: Pointer that was passed into request_threaded_irq()
 * (ignored)
 *
 * Fetch the incoming packet, via babblenet_get_babble(), and
 * overwrite any previously stored babble. As per babbler_write(),
 * truncate the payload to the first 140 characters. Remember to add
 * appropriate spin lock calls in this function.
 *
 * Note that the incoming payload is NOT a string; you can NOT use
 * strcpy() or strlen() on it.
 *
 * Return: always IRQ_HANDLED
 */
static irqreturn_t babblenet_bottom(int irq, void *cookie)
{
	int ret;
	int attempts = 0;
	size_t len = 0;
	int count = 0;	
	char const *buf = babblenet_get_babble(&len);
	
	count = (int)len;	
	

	if (count > BABBLE_LEN)
		count = BABBLE_LEN;

	ret = 0;

	while (!ret) {
		attempts++;
		ret = spin_trylock(&my_lock);
		if(attempts > 20){
			/*Lock shouldn't be held that long*/
			printk(KERN_INFO "Unable to hold lock");
			return -EDEADLK;
		}		
	}

	printk("Lock Acquired in Bottom; Attempts: %d\n", attempts);
	attempts = 0;
	babble_size = count;

	memset(BABBLE, 0, BABBLE_LEN);
	cpyStr(BABBLE, buf, 140, count);

	spin_unlock(&my_lock);

	return IRQ_HANDLED;
}

/**
 * babbler_init() - entry point into the Babbler kernel driver
 * Return: 0 on successful initialization, negative on error
 */
static int __init babbler_init(void) {
	int retval;
	int err = 0;

	INIT_LIST_HEAD(&topics_head);
	

	topics_buffer = (char *)vmalloc(1 * PAGE_SIZE);
	topics_buffer[PAGE_SIZE - 1] = '\0';
	BABBLE = (char *)vmalloc(1 * PAGE_SIZE);
	BABBLE[PAGE_SIZE - 1] = '\0';

	memset(topics_buffer, 0, 1 * PAGE_SIZE);
	memset(BABBLE, 0, 1 * PAGE_SIZE);
	topics_ptr = topics_buffer;

	babble_size = 0;
	if (!topics_buffer) {
		pr_info("Failed to allocate memory for 1 topics Page\n");
		return -ENOMEM;
	}
	if (!BABBLE) {
		pr_info("Failed to allocate memory for 1 BABBLE Page\n");
		return -ENOMEM;
	}

	retval = misc_register(&babbler_dev);
	if (retval) {
		pr_err("Could not register babbler\n");
		goto err_vfree;
	}

	retval = misc_register(&babbler_ctl_dev);
	if (retval) {
		pr_err("Could not register babbler_ctl\n");
		goto err_deregister_babbler;
	}

	/*
	 * In part 2, register ISR and enable network integration.
	 */

	err = request_threaded_irq(BABBLENET_IRQ,
				   babblenet_top,
				   babblenet_bottom, 
				   0, 
				   "babble_IRQ", 
				   NULL);
	babblenet_enable();
	printk("Babbletnet init'd\n");

	if (err < 0) {
		printk("Could not assign device IRQ at %d", BABBLENET_IRQ);
		free_irq(BABBLENET_IRQ, NULL);
		babblenet_disable();
		misc_deregister(&babbler_ctl_dev);
		goto err_deregister_babbler;	/*PROPER ERROR HANDLING?! */
	}

	return 0;

err_deregister_babbler:
	misc_deregister(&babbler_dev);
err_vfree:
	vfree(BABBLE);
	vfree(topics_buffer);
	return retval;
}

/**
 * babbler_exit() - called by kernel to clean up resources
 */
static void __exit babbler_exit(void) {
	/*
	 * In part 3, free all memory associated with topics list.
	 */
	struct topics_list *old_entry;
	struct list_head *pos, *n;

	list_for_each_safe(pos, n, &topics_head) {
		old_entry = list_entry(pos, struct topics_list, list);
		pr_info("Removing: %s\n", old_entry->topic);
		list_del(pos);
		kfree(old_entry);
	} 
	pr_info("List emptied\n");

	/*
	 * In part 2, disable network integration and remove the ISR.
	 */
	babblenet_disable();
	free_irq(BABBLENET_IRQ, NULL);

	misc_deregister(&babbler_ctl_dev);
	misc_deregister(&babbler_dev);
	vfree(topics_buffer);
	vfree(BABBLE);
	pr_info("Goodbye, world!\n");
} module_init(babbler_init);
module_exit(babbler_exit);

MODULE_DESCRIPTION("CS421 Babbler driver - project 2");
MODULE_LICENSE("GPL");
