// SPDX-License-Identifier: GPL-2.0
/*
 * hello_char.c - Minimal Linux character device driver skeleton.
 *
 * Creates /dev/hello_char0 .. /dev/hello_char{num_minors-1} (module
 * parameter num_minors, default 4, max 8). Each minor owns an independent
 * message buffer and I/O statistics. File operations: open/read/write
 * plus ioctls RESET, GET_LEN, SET_MSG, GET_MSG, GET_STATS (see
 * hello_ioctl.h for the shared interface definition).
 *
 * Intended as a learning/bring-up skeleton, not a production driver.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/printk.h>
#include <linux/version.h>

#include "hello_ioctl.h"

#define DEVICE_NAME "hello_char"
#define CLASS_NAME  "hello"
#define BUF_SIZE    HELLO_MSG_MAX

#define HELLO_MAX_MINORS 8

static int num_minors = 4;
module_param(num_minors, int, 0444);
MODULE_PARM_DESC(num_minors,
		 "Number of /dev/hello_charN minors to create (1-8, default 4)");

static int major;
static struct class *hello_class;
static int hello_num_devs;

struct hello_dev {
	struct cdev cdev;
	struct mutex lock;
	char buf[BUF_SIZE];
	size_t len;
	unsigned long long bytes_read;
	unsigned long long bytes_written;
	unsigned long long opens;
	unsigned int minor;
};

static struct hello_dev hello_devices[HELLO_MAX_MINORS];

static int hello_open(struct inode *inode, struct file *file)
{
	struct hello_dev *hdev =
		container_of(inode->i_cdev, struct hello_dev, cdev);

	if (mutex_lock_interruptible(&hdev->lock))
		return -ERESTARTSYS;
	hdev->opens++;
	mutex_unlock(&hdev->lock);

	file->private_data = hdev;
	pr_info("hello_char%u: opened (opens=%llu)\n",
		hdev->minor, hdev->opens);
	return 0;
}

static int hello_release(struct inode *inode, struct file *file)
{
	struct hello_dev *hdev = file->private_data;

	(void)inode;
	pr_info("hello_char%u: closed\n", hdev->minor);
	return 0;
}

static ssize_t hello_read(struct file *file, char __user *buf,
			  size_t len, loff_t *off)
{
	struct hello_dev *hdev = file->private_data;
	ssize_t ret;

	if (mutex_lock_interruptible(&hdev->lock))
		return -ERESTARTSYS;

	if (*off >= (loff_t)hdev->len) {
		ret = 0; /* EOF */
		goto out;
	}

	if (len > hdev->len - *off)
		len = hdev->len - *off;

	if (copy_to_user(buf, hdev->buf + *off, len)) {
		ret = -EFAULT;
		goto out;
	}

	*off += len;
	ret = len;
	hdev->bytes_read += len;

out:
	mutex_unlock(&hdev->lock);
	return ret;
}

static ssize_t hello_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *off)
{
	struct hello_dev *hdev = file->private_data;
	ssize_t ret;

	(void)off; /* append-only device: the write offset is ignored */

	if (mutex_lock_interruptible(&hdev->lock))
		return -ERESTARTSYS;

	if (len > BUF_SIZE - hdev->len)
		len = BUF_SIZE - hdev->len;

	if (len == 0) {
		ret = -ENOSPC;
		goto out;
	}

	if (copy_from_user(hdev->buf + hdev->len, buf, len)) {
		ret = -EFAULT;
		goto out;
	}

	hdev->len += len;
	ret = len;
	hdev->bytes_written += len;

out:
	mutex_unlock(&hdev->lock);
	return ret;
}

static long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hello_dev *hdev = file->private_data;
	struct hello_msg kmsg;
	struct hello_stats kstats;
	long ret = 0;

	if (mutex_lock_interruptible(&hdev->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case HELLO_IOCTL_RESET:
		hdev->len = 0;
		memset(hdev->buf, 0, BUF_SIZE);
		break;
	case HELLO_IOCTL_GET_LEN:
		if (put_user(hdev->len, (size_t __user *)arg))
			ret = -EFAULT;
		break;
	case HELLO_IOCTL_SET_MSG:
		if (copy_from_user(&kmsg, (void __user *)arg, sizeof(kmsg))) {
			ret = -EFAULT;
			break;
		}
		if (kmsg.len > BUF_SIZE) {
			ret = -EINVAL;
			break;
		}
		memcpy(hdev->buf, kmsg.text, kmsg.len);
		hdev->len = kmsg.len;
		break;
	case HELLO_IOCTL_GET_MSG:
		kmsg.len = (hello_u32)hdev->len;
		memcpy(kmsg.text, hdev->buf, hdev->len);
		if (copy_to_user((void __user *)arg, &kmsg, sizeof(kmsg)))
			ret = -EFAULT;
		break;
	case HELLO_IOCTL_GET_STATS:
		kstats.bytes_read = hdev->bytes_read;
		kstats.bytes_written = hdev->bytes_written;
		kstats.opens = hdev->opens;
		kstats.minor = hdev->minor;
		kstats._pad = 0;
		if (copy_to_user((void __user *)arg, &kstats, sizeof(kstats)))
			ret = -EFAULT;
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&hdev->lock);
	return ret;
}

static const struct file_operations hello_fops = {
	.owner          = THIS_MODULE,
	.open           = hello_open,
	.release        = hello_release,
	.read           = hello_read,
	.write          = hello_write,
	.unlocked_ioctl = hello_ioctl,
};

static int __init hello_init(void)
{
	dev_t dev;
	int ret, i;

	if (num_minors < 1 || num_minors > HELLO_MAX_MINORS) {
		pr_err("hello_char: num_minors=%d out of range 1..%d\n",
		       num_minors, HELLO_MAX_MINORS);
		return -EINVAL;
	}

	ret = alloc_chrdev_region(&dev, 0, num_minors, DEVICE_NAME);
	if (ret < 0) {
		pr_err("hello_char: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}
	major = MAJOR(dev);

	for (i = 0; i < num_minors; i++) {
		struct hello_dev *hdev = &hello_devices[i];

		hdev->minor = i;
		mutex_init(&hdev->lock);
		cdev_init(&hdev->cdev, &hello_fops);
		hdev->cdev.owner = THIS_MODULE;
		ret = cdev_add(&hdev->cdev, MKDEV(major, i), 1);
		if (ret < 0) {
			pr_err("hello_char: cdev_add failed for minor %d: %d\n",
			       i, ret);
			goto err_cdev;
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	hello_class = class_create(CLASS_NAME);
#else
	hello_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
	if (IS_ERR(hello_class)) {
		ret = PTR_ERR(hello_class);
		pr_err("hello_char: class_create failed: %d\n", ret);
		goto err_cdev;
	}

	for (i = 0; i < num_minors; i++) {
		if (IS_ERR(device_create(hello_class, NULL, MKDEV(major, i),
					 NULL, "%s%d", DEVICE_NAME, i))) {
			pr_err("hello_char: device_create failed for minor %d\n",
			       i);
			ret = -ENODEV;
			goto err_device;
		}
	}

	hello_num_devs = num_minors;
	pr_info("hello_char: loaded, major=%d, %d minors (/dev/%s0..%d)\n",
		major, num_minors, DEVICE_NAME, num_minors - 1);
	return 0;

err_device:
	for (i--; i >= 0; i--)
		device_destroy(hello_class, MKDEV(major, i));
	class_destroy(hello_class);
err_cdev:
	for (i--; i >= 0; i--)
		cdev_del(&hello_devices[i].cdev);
	unregister_chrdev_region(dev, num_minors);
	return ret;
}

static void __exit hello_exit(void)
{
	int i;

	for (i = 0; i < hello_num_devs; i++)
		device_destroy(hello_class, MKDEV(major, i));
	class_destroy(hello_class);
	for (i = 0; i < hello_num_devs; i++)
		cdev_del(&hello_devices[i].cdev);
	unregister_chrdev_region(MKDEV(major, 0), hello_num_devs);
	pr_info("hello_char: unloaded (%d minors)\n", hello_num_devs);
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sai Bontha");
MODULE_DESCRIPTION("Minimal character device driver skeleton with ioctls and multiple minors");
MODULE_VERSION("0.2");
