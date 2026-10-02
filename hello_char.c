// SPDX-License-Identifier: GPL-2.0
/*
 * hello_char.c - Minimal Linux character device driver skeleton.
 *
 * Exposes /dev/hello_char. Supports open/read/write plus two ioctls:
 *   HELLO_IOCTL_RESET    - clear the internal buffer
 *   HELLO_IOCTL_GET_LEN  - report bytes currently stored
 *
 * Intended as a learning/bring-up skeleton, not a production driver.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/version.h>

#define DEVICE_NAME "hello_char"
#define CLASS_NAME  "hello"
#define BUF_SIZE    256

/* ioctl interface */
#define HELLO_IOCTL_RESET   _IO('h', 0)
#define HELLO_IOCTL_GET_LEN _IOR('h', 1, size_t)

static int major;
static struct class *hello_class;
static struct cdev hello_cdev;

static char dev_buf[BUF_SIZE];
static size_t dev_len;
static DEFINE_MUTEX(dev_lock);

static int hello_open(struct inode *inode, struct file *file)
{
	pr_info("hello_char: device opened\n");
	return 0;
}

static int hello_release(struct inode *inode, struct file *file)
{
	pr_info("hello_char: device closed\n");
	return 0;
}

static ssize_t hello_read(struct file *file, char __user *buf,
			  size_t len, loff_t *off)
{
	ssize_t ret;

	if (mutex_lock_interruptible(&dev_lock))
		return -ERESTARTSYS;

	if (*off >= dev_len) {
		ret = 0; /* EOF */
		goto out;
	}

	if (len > dev_len - *off)
		len = dev_len - *off;

	if (copy_to_user(buf, dev_buf + *off, len)) {
		ret = -EFAULT;
		goto out;
	}

	*off += len;
	ret = len;

out:
	mutex_unlock(&dev_lock);
	return ret;
}

static ssize_t hello_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *off)
{
	ssize_t ret;

	if (mutex_lock_interruptible(&dev_lock))
		return -ERESTARTSYS;

	if (len > BUF_SIZE - dev_len)
		len = BUF_SIZE - dev_len;

	if (len == 0) {
		ret = -ENOSPC;
		goto out;
	}

	if (copy_from_user(dev_buf + dev_len, buf, len)) {
		ret = -EFAULT;
		goto out;
	}

	dev_len += len;
	ret = len;

out:
	mutex_unlock(&dev_lock);
	return ret;
}

static long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (mutex_lock_interruptible(&dev_lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case HELLO_IOCTL_RESET:
		dev_len = 0;
		memset(dev_buf, 0, BUF_SIZE);
		break;
	case HELLO_IOCTL_GET_LEN:
		if (put_user(dev_len, (size_t __user *)arg))
			ret = -EFAULT;
		break;
	default:
		ret = -ENOTTY;
		break;
	}

	mutex_unlock(&dev_lock);
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
	int ret;

	ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("hello_char: alloc_chrdev_region failed: %d\n", ret);
		return ret;
	}
	major = MAJOR(dev);

	cdev_init(&hello_cdev, &hello_fops);
	hello_cdev.owner = THIS_MODULE;
	ret = cdev_add(&hello_cdev, dev, 1);
	if (ret < 0) {
		pr_err("hello_char: cdev_add failed: %d\n", ret);
		goto err_region;
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

	if (IS_ERR(device_create(hello_class, NULL, dev, NULL, DEVICE_NAME))) {
		ret = -ENODEV;
		pr_err("hello_char: device_create failed\n");
		goto err_class;
	}

	pr_info("hello_char: loaded, major=%d, node=/dev/%s\n",
		major, DEVICE_NAME);
	return 0;

err_class:
	class_destroy(hello_class);
err_cdev:
	cdev_del(&hello_cdev);
err_region:
	unregister_chrdev_region(dev, 1);
	return ret;
}

static void __exit hello_exit(void)
{
	dev_t dev = MKDEV(major, 0);

	device_destroy(hello_class, dev);
	class_destroy(hello_class);
	cdev_del(&hello_cdev);
	unregister_chrdev_region(dev, 1);
	pr_info("hello_char: unloaded\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sai Bontha");
MODULE_DESCRIPTION("Minimal character device driver skeleton");
MODULE_VERSION("0.1");
