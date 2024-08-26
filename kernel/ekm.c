/*
 * Copyright (c) 2024 Jeff Boody
 *
 * License: Dual MIT/GPL
 *
 */

#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

static struct class *ekm_class;

struct ekm_data {
	int value;
};

#define EKM_IOC_MAGIC 'k'
#define EKM_IOCTL_GET_DATA _IOR(EKM_IOC_MAGIC, 1, struct ekm_data)
#define EKM_IOCTL_SET_DATA _IOW(EKM_IOC_MAGIC, 2, struct ekm_data)

struct ekm_device {
	struct cdev cdev;
	struct device *device;
	struct ekm_data data;
	dev_t dev;
	spinlock_t spinlock;
};

static int ekm_cdev_open(struct inode *inode, struct file *file) {
	struct ekm_device *ekm_dev = container_of(inode->i_cdev, struct ekm_device,
		cdev);
	file->private_data = ekm_dev;

	pr_info("ekm_cdev_open: success\n");

	return 0;
}

static int ekm_cdev_release(struct inode *inode, struct file *file) {
	pr_info("ekm_cdev_release: success\n");

	return 0;
}

static long ekm_cdev_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg) {
	struct ekm_device *ekm_dev = file->private_data;
	struct ekm_data temp;
	int ret = 0;

	switch (cmd) {
	case EKM_IOCTL_GET_DATA:
		spin_lock(&ekm_dev->spinlock);
		temp = ekm_dev->data;
		spin_unlock(&ekm_dev->spinlock);
		if (copy_to_user((struct ekm_data __user *)arg, &temp, sizeof(temp))) {
			return -EFAULT;
		}
		pr_info("ekm_cdev_ioctl: EKM_IOCTL_GET_DATA %i\n", temp.value);
		break;

	case EKM_IOCTL_SET_DATA:
		if (copy_from_user(&temp, (struct ekm_data __user *)arg, sizeof(temp))) {
			return -EFAULT;
		}
		spin_lock(&ekm_dev->spinlock);
		ekm_dev->data = temp;
		spin_unlock(&ekm_dev->spinlock);
		pr_info("ekm_cdev_ioctl: EKM_IOCTL_SET_DATA %i\n", temp.value);
		break;

	default:
		pr_err("ekm_cdev_ioctl: %u failed\n", cmd);
		return -ENOTTY;
	}

	return ret;
}

static const struct file_operations ekm_cdev_fops = {
	.owner = THIS_MODULE,
	.open = ekm_cdev_open,
	.release = ekm_cdev_release,
	.unlocked_ioctl = ekm_cdev_ioctl,
};

int ekm_platform_driver_probe(struct platform_device *pdev) {
	struct ekm_device *ekm_dev;
	int ret;
	struct device *device;

	ekm_dev = kzalloc(sizeof(*ekm_dev), GFP_KERNEL);
	if (!ekm_dev) {
		return -ENOMEM;
	}

	ret = alloc_chrdev_region(&ekm_dev->dev, 0, 1, "ekm");
	if (ret < 0) {
		pr_err("ekm_platform_driver_probe: alloc_chrdev_region failed\n");
		goto err_alloc_chrdev;
	}

	cdev_init(&ekm_dev->cdev, &ekm_cdev_fops);
	ekm_dev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&ekm_dev->cdev, ekm_dev->dev, 1);
	if (ret < 0) {
		pr_err("ekm_platform_driver_probe: cdev_add failed\n");
		goto err_cdev_add;
	}

	device = device_create(ekm_class, NULL, ekm_dev->dev, NULL, "ekm%d",
		MINOR(ekm_dev->dev));
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("ekm_platform_driver_probe: device_create failed\n");
		goto err_device_create;
	}

	ekm_dev->device = &pdev->dev;
	ekm_dev->data.value = 42;

	spin_lock_init(&ekm_dev->spinlock);

	platform_set_drvdata(pdev, ekm_dev);

	pr_info("ekm_platform_driver_probe: success\n");

	return 0;

err_device_create:
	cdev_del(&ekm_dev->cdev);
err_cdev_add:
	unregister_chrdev_region(ekm_dev->dev, 1);
err_alloc_chrdev:
	kfree(ekm_dev);
	return ret;
}

int ekm_platform_driver_remove(struct platform_device *pdev)
{
	struct ekm_device *ekm_dev = platform_get_drvdata(pdev);

	device_destroy(ekm_class, ekm_dev->dev);
	cdev_del(&ekm_dev->cdev);
	unregister_chrdev_region(ekm_dev->dev, 1);
	kfree(ekm_dev);

	pr_info("ekm_platform_driver_remove: success\n");

	return 0;
}

static struct platform_device *ekm_platform_device;

static struct platform_driver ekm_platform_driver = {
	.probe  = ekm_platform_driver_probe,
	.remove = ekm_platform_driver_remove,
	.driver = {
		.name  = "ekm",
		.owner = THIS_MODULE,
	},
};

static int __init ekm_module_init(void)
{
	int ret = 0;

	ekm_platform_device = platform_device_register_simple("ekm", -1, NULL, 0);
	if (IS_ERR(ekm_platform_device)) {
		pr_err("ekm_module_init: platform_device_register_simple failed\n");
		return PTR_ERR(ekm_platform_device);
	}

	ekm_class = class_create(THIS_MODULE, "ekm");
	if (IS_ERR(ekm_class)) {
		pr_err("ekm_module_init: class_create failed\n");
		ret = PTR_ERR(ekm_class);
		goto err_class_create;
	}

	ret = platform_driver_register(&ekm_platform_driver);
	if (ret < 0) {
		pr_err("ekm_module_init: platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	pr_info("ekm_module_init: success\n");

	return 0;

err_platform_driver_register:
	class_destroy(ekm_class);
err_class_create:
	platform_device_unregister(ekm_platform_device);
	return ret;
}

static void __exit ekm_module_exit(void)
{
	platform_driver_unregister(&ekm_platform_driver);
	class_destroy(ekm_class);
	platform_device_unregister(ekm_platform_device);

	pr_info("ekm_module_exit: success\n");
}

module_init(ekm_module_init);
module_exit(ekm_module_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Jeff Boody <jeffboody@gmail.com>");
MODULE_DESCRIPTION("Echo Kernel Module (EKM)");
