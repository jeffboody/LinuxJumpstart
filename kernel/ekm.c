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
#define EKM_IOCTL_READ  _IOR(EKM_IOC_MAGIC, 1, struct ekm_data)
#define EKM_IOCTL_WRITE _IOW(EKM_IOC_MAGIC, 2, struct ekm_data)

struct ekm {
	struct cdev cdev;
	struct device *device;
	struct ekm_data data;
	dev_t dev;
	spinlock_t spinlock;
};

static int ekm_open(struct inode *inode, struct file *file) {
	struct ekm *ekm = container_of(inode->i_cdev, struct ekm, cdev);
	file->private_data = ekm;

	pr_info("ekm_open: success\n");

	return 0;
}

static int ekm_release(struct inode *inode, struct file *file) {
	pr_info("ekm_release: success\n");

	return 0;
}

static long ekm_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
	struct ekm *ekm = file->private_data;
	struct ekm_data temp;
	int ret = 0;

	switch (cmd) {
	case EKM_IOCTL_READ:
		spin_lock(&ekm->spinlock);
		temp = ekm->data;
		spin_unlock(&ekm->spinlock);
		if (copy_to_user((struct ekm_data __user *)arg, &temp, sizeof(temp))) {
			return -EFAULT;
		}
		pr_info("ekm_ioctl: EKM_IOCTL_READ %i\n", temp.value);
		break;

	case EKM_IOCTL_WRITE:
		if (copy_from_user(&temp, (struct ekm_data __user *)arg, sizeof(temp))) {
			return -EFAULT;
		}
		spin_lock(&ekm->spinlock);
		ekm->data = temp;
		spin_unlock(&ekm->spinlock);
		pr_info("ekm_ioctl: EKM_IOCTL_WRITE %i\n", temp.value);
		break;

	default:
		pr_err("ekm_ioctl: %u failed\n", cmd);
		return -ENOTTY;
	}

	return ret;
}

static const struct file_operations ekm_fops = {
	.owner = THIS_MODULE,
	.open = ekm_open,
	.release = ekm_release,
	.unlocked_ioctl = ekm_ioctl,
};

int ekm_probe(struct platform_device *pdev) {
    struct ekm *ekm;
    int ret;
    struct device *device;

    ekm = kzalloc(sizeof(*ekm), GFP_KERNEL);
    if (!ekm) {
        return -ENOMEM;
	}

    ret = alloc_chrdev_region(&ekm->dev, 0, 1, "ekm");
    if (ret < 0) {
        pr_err("ekm_probe: alloc_chrdev_region failed\n");
        goto err_alloc_chrdev;
    }

    cdev_init(&ekm->cdev, &ekm_fops);
    ekm->cdev.owner = THIS_MODULE;
    ret = cdev_add(&ekm->cdev, ekm->dev, 1);
    if (ret < 0) {
        pr_err("ekm_probe: cdev_add failed\n");
        goto err_cdev_add;
    }

    device = device_create(ekm_class, NULL, ekm->dev, NULL, "ekm%d", MINOR(ekm->dev));
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        pr_err("ekm_probe: device_create failed\n");
        goto err_device_create;
    }

    ekm->device = &pdev->dev;
    ekm->data.value = 42;

	spin_lock_init(&ekm->spinlock);

    platform_set_drvdata(pdev, ekm);

	pr_info("ekm_probe: success\n");

    return 0;

err_device_create:
    cdev_del(&ekm->cdev);
err_cdev_add:
    unregister_chrdev_region(ekm->dev, 1);
err_alloc_chrdev:
    kfree(ekm);
    return ret;
}

int ekm_remove(struct platform_device *pdev)
{
    struct ekm *ekm = platform_get_drvdata(pdev);

    device_destroy(ekm_class, ekm->dev);
    cdev_del(&ekm->cdev);
    unregister_chrdev_region(ekm->dev, 1);
    kfree(ekm);

    pr_info("ekm_remove: success\n");

    return 0;
}

static struct platform_device *ekm_device;

static struct platform_driver ekm_driver = {
	.probe  = ekm_probe,
	.remove = ekm_remove,
	.driver = {
		.name  = "ekm",
		.owner = THIS_MODULE,
	},
};

static int __init ekm_init(void)
{
	int ret = 0;

	ekm_device = platform_device_register_simple("ekm", -1, NULL, 0);
	if (IS_ERR(ekm_device)) {
		pr_err("ekm_init: platform_device_register_simple failed\n");
		return PTR_ERR(ekm_device);
	}

    ekm_class = class_create(THIS_MODULE, "ekm");
    if (IS_ERR(ekm_class)) {
        pr_err("ekm_init: class_create failed\n");
        ret = PTR_ERR(ekm_class);
		goto err_class_create;
    }

    ret = platform_driver_register(&ekm_driver);
    if (ret < 0) {
        pr_err("ekm_init: platform_driver_register failed\n");
        goto err_platform_driver_register;
    }

	pr_info("ekm_init: success\n");

    return 0;

err_platform_driver_register:
    class_destroy(ekm_class);
err_class_create:
	platform_device_unregister(ekm_device);
	return ret;
}

static void __exit ekm_exit(void)
{
    platform_driver_unregister(&ekm_driver);
    class_destroy(ekm_class);
	platform_device_unregister(ekm_device);

	pr_info("ekm_exit: success\n");
}

module_init(ekm_init);
module_exit(ekm_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Jeff Boody <jeffboody@gmail.com>");
MODULE_DESCRIPTION("Echo Kernel Module (EKM)");
