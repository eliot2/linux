/* Name: Eliot Carney-Seim
   Email:eliot2@umbc.edu
   
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

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>

static char *topics_buffer;

static const struct 

/**
 * babbler_read() - callback invoked when a process reads from
 * /dev/babbler
 * @filp: process's file object that is reading from this device (ignored)
 * @ubuf: destination buffer to store babble
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Write to @ubuf the most recent babble, but only if the babble
 * contains a topic of interest to the caller. Copy the lesser of
 * @count and babble's length.
 *
  * If there are no babbles to be read (or the most recent babble does
 * not contain any topics of interest), do nothing and return 0.
 *
 * Regardless if anything is written to @ubuf or not, always clear the
 * babble.
 *
 * HINT: strstr() may be of use here, but only if your topic and
 * babble buffers are already null-terminated.
 *
 * Return: number of bytes written to @ubuf, or negative on error
 */
static ssize_t babbler_read(struct file *filp, char __user * ubuf,
			    size_t count, loff_t * ppos)
{
  return -EPERM;
}

/**
 * babbler_write() - callback invoked when a process writes to
 * /dev/babbler
 * @filp: process's file object that is writing to this device (ignored)
 * @ubuf: source buffer of bytes from user
 * @count: number of bytes in @ubuf
 * @ppos: file offset (ignored)
 *
 * Copy the contents of @ubuf, up to @count bytes, to the current
 * babble buffer, overwriting any previously stored babble. If @count
 * is greater than 140, copy only the first 140 characters. Note that
 * @ubuf is NOT a string; you can NOT use strcpy() or strlen() on it.
 *
 * Return: @count, or negative on error
 */
static ssize_t babbler_write(struct file *filp, const char __user * ubuf,
			     size_t count, loff_t * ppos)
{
  return -EPERM;
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
 * Otherwise, copy the contents of @ubuf, up to @count bytes, to the
 * list of topics of interest. If @count is greater than 8, copy only
 * the first 8 characters. Note that @ubuf is NOT a string; you can
 * NOT use strcpy() or strlen() on it.
 *
 * Any time the topics of interest changes, update the contents of
 * topics_buffer. Copy the topics of interest (if any) into the
 * buffer. The buffer should only contain the topics; all unused bytes
 * in the buffer should be set to zero.  HINT: memcpy() and memset()
 * may be of use here.
 *
 * Return: @count, or negative on error
 */
static ssize_t babbler_ctl_write(struct file *filp, const char __user * ubuf,
				 size_t count, loff_t * ppos)
{
  return -EPERM;
}

/**
 * babbler_ctl_mmap() - callback invoked when a process mmap()s to
 * /dev/babbler_ctl
 * @filp: process's file object that is writing to this device (ignored)
 * @vma: virtual memory allocation object containing mmap() request
 *
 * Create a read-only mapping from kernel memory (specifically,
 * topics_buffer) into user space.
 *
 * You do not need to modify this function.
 *
 * Code based upon
 * http://bloggar.combitech.se/ldc/2015/01/21/mmap-memory-between-kernel-and-userspace/
 *
 * Return: 0 on success, negative on error.
 */
static int babbler_mmap(struct file *filp, struct vm_area_struct *vma)
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

/**
 * babbler_init() - entry point into the Babbler kernel module
 * Return: 0 on successful initialization, negative on error
 */
static int __init babbler_init(void)
{
	pr_info("Hello, world!\n");
	return 0;
}

/**
 * babbler_exit() - called by kernel to clean up resources
 */
static void __exit babbler_exit(void)
{
	pr_info("Goodbye, world!\n");
}

module_init(babbler_init);
module_exit(babbler_exit);

MODULE_DESCRIPTION("CS421 Babbler driver");
MODULE_LICENSE("GPL");
