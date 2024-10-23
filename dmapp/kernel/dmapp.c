/*
 * Copyright (c) 2024 Jeff Boody
 *
 * License: Dual MIT/GPL
 *
 */

#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/dma-fence.h>
#include <linux/dma-mapping.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#define DMAPP_BUFFER_SIZE 10

static struct class *dmapp_class;

#define DMAPP_IOC_MAGIC 'd'
#define DMAPP_IOCTL_GET_BUFFER_SIZE _IO(DMAPP_IOC_MAGIC, 1)
#define DMAPP_IOCTL_GET_BUFFER_FD _IO(DMAPP_IOC_MAGIC, 2)
#define DMAPP_IOCTL_GET_BUFFER_PARITY _IO(DMAPP_IOC_MAGIC, 3)
#define DMAPP_IOCTL_BUFFER_LOCK _IO(DMAPP_IOC_MAGIC, 4)
#define DMAPP_IOCTL_BUFFER_UNLOCK _IO(DMAPP_IOC_MAGIC, 5)

struct dmapp_user;

struct dmapp_device {
	struct cdev cdev;
	struct device *device;
	dev_t dev;
	spinlock_t spinlock;
	struct dmapp_user *user[2];
	struct dma_fence *fence[2];
	struct dma_buf *buf;
};

struct dmapp_user {
	struct dmapp_device *dmapp_dev;
	bool is_locked;
};

struct dmapp_buffer {
	void *vaddr;
	dma_addr_t paddr;
	size_t size;
	struct device *dev;
};

static struct sg_table *dmapp_buf_map(struct dma_buf_attachment *attachment,
	enum dma_data_direction dir) {
	struct dmapp_buffer *buffer = attachment->dmabuf->priv;
	struct sg_table *sgt;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		return ERR_PTR(-ENOMEM);
	}

	if (sg_alloc_table(sgt, 1, GFP_KERNEL)) {
		goto err_sg_alloc_table;
	}

	sg_set_page(sgt->sgl, virt_to_page(buffer->vaddr), buffer->size, 0);

	if (dma_map_sg(attachment->dev, sgt->sgl, sgt->nents, dir) == 0) {
		goto err_dma_map_sg;
	}

	return sgt;

err_dma_map_sg:
	sg_free_table(sgt);
err_sg_alloc_table:
	kfree(sgt);
	return ERR_PTR(-ENOMEM);
}

static void dmapp_buf_unmap(struct dma_buf_attachment *attachment,
	struct sg_table *sgt, enum dma_data_direction dir) {
	dma_unmap_sg(attachment->dev, sgt->sgl, sgt->nents, dir);
	sg_free_table(sgt);
	kfree(sgt);
}

static void dmapp_buf_release(struct dma_buf *dmabuf) {
	struct dmapp_buffer *buffer = dmabuf->priv;
	dma_free_coherent(buffer->dev, buffer->size, buffer->vaddr, buffer->paddr);
	kfree(buffer);
}

static int dmapp_buf_begin_cpu_access(struct dma_buf *dmabuf,
	enum dma_data_direction dir) {
	struct dmapp_buffer *buffer = dmabuf->priv;
	dma_sync_single_for_cpu(buffer->dev, buffer->paddr, buffer->size, dir);
	return 0;
}

static int dmapp_buf_end_cpu_access(struct dma_buf *dmabuf,
	enum dma_data_direction dir) {
	struct dmapp_buffer *buffer = dmabuf->priv;
	dma_sync_single_for_device(buffer->dev, buffer->paddr, buffer->size, dir);
	return 0;
}

static struct dma_buf_ops dmapp_dmabuf_ops = {
	.map_dma_buf = dmapp_buf_map,
	.unmap_dma_buf = dmapp_buf_unmap,
	.release = dmapp_buf_release,
	.begin_cpu_access = dmapp_buf_begin_cpu_access,
	.end_cpu_access = dmapp_buf_end_cpu_access,
};

static int dmapp_cdev_open(struct inode *inode, struct file *file) {
	struct dmapp_device *dmapp_dev;
	struct dmapp_user *user;
	int parity;

	dmapp_dev = container_of(inode->i_cdev, struct dmapp_device, cdev);

	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (!user) {
		return -ENOMEM;
	}

	spin_lock(&dmapp_dev->spinlock);

	/* Assign parity to user */
	if (dmapp_dev->user[0] == NULL) {
		parity = 0;
	} else if (dmapp_dev->user[1] == NULL) {
		parity = 1;
	} else {
		spin_unlock(&dmapp_dev->spinlock);
		pr_err("dmapp_cdev_open: invalid user\n");
		goto err_user;
	}

	user->dmapp_dev = dmapp_dev;
	dmapp_dev->user[parity] = user;

	spin_unlock(&dmapp_dev->spinlock);

	file->private_data = user;

	pr_info("dmapp_cdev_open: success\n");

	return 0;

err_user:
	kfree(user);
	return -EINVAL;
}

static int dmapp_cdev_release(struct inode *inode, struct file *file) {
	int ret = 0;
	struct dmapp_user *user = (struct dmapp_user *) file->private_data;
	struct dmapp_device *dmapp_dev = user->dmapp_dev;
	struct dma_fence *signal_fence = NULL;

	spin_lock(&dmapp_dev->spinlock);

	/* Disconnect the user */
	if (dmapp_dev->user[0] == user) {
		dmapp_dev->user[0] = NULL;
		if (user->is_locked) {
			signal_fence = dmapp_dev->fence[1];
		}
	} else if (dmapp_dev->user[1] == user) {
		dmapp_dev->user[1] = NULL;
		if (user->is_locked) {
			signal_fence = dmapp_dev->fence[0];
		}
	}

	spin_unlock(&dmapp_dev->spinlock);

	file->private_data = NULL;
	kfree(user);

	/* Optionally reset the signal_fence to prevent deadlocks */
	if (signal_fence && !dma_fence_is_signaled(signal_fence)) {
		ret = dma_fence_signal(signal_fence);
		if (ret < 0) {
			/* Signal may fail if the fence was previously signaled */
			pr_err("dmapp_cdev_release: dma_fence_signal failed (%i)\n", ret);
		}
	}

	if (!ret) {
		pr_info("dmapp_cdev_release: success\n");
	}

	return ret;
}

static const char *dmapp_fence_get_driver_name(struct dma_fence *fence)
{
	return "dmapp";
}

static const char *dmapp_fence_get_timeline_name(struct dma_fence *fence)
{
	return "dmapp_timeline";
}

static const struct dma_fence_ops dmapp_fence_ops = {
	.get_driver_name = dmapp_fence_get_driver_name,
	.get_timeline_name = dmapp_fence_get_timeline_name,
};

static long dmapp_cdev_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg) {
	struct dmapp_user *user = file->private_data;
	struct dmapp_device *dmapp_dev = user->dmapp_dev;
	struct dma_fence *signal_fence;
	struct dma_fence *wait_fence;
	int ret = 0;
	int parity;

	spin_lock(&dmapp_dev->spinlock);

	/* Validate user, determine parity and assign fences */
	if (dmapp_dev->user[0] == user) {
		parity = 0;
		signal_fence = dmapp_dev->fence[1];
		wait_fence = dmapp_dev->fence[0];
	} else if (dmapp_dev->user[1] == user) {
		parity = 1;
		signal_fence = dmapp_dev->fence[0];
		wait_fence = dmapp_dev->fence[1];
	} else {
		spin_unlock(&dmapp_dev->spinlock);
		pr_err("dmapp_cdev_ioctl: invalid user\n");
		return -EINVAL;
	}

	/* Handle commands that require synchronization */
	switch (cmd) {
	case DMAPP_IOCTL_BUFFER_LOCK:
		if (user->is_locked) {
			/* Ignore ioctl when already in locked state */
			spin_unlock(&dmapp_dev->spinlock);
			return 0;
		}
		break;
	case DMAPP_IOCTL_BUFFER_UNLOCK:
		if (!user->is_locked) {
			/* Ignore ioctl when already in unlocked state */
			spin_unlock(&dmapp_dev->spinlock);
			return 0;
		}
		break;
	}

	spin_unlock(&dmapp_dev->spinlock);

	/* Handle commands that might sleep or do not require synchronization */
	switch (cmd) {
	case DMAPP_IOCTL_GET_BUFFER_SIZE:
		pr_info("DMAPP_IOCTL_GET_BUFFER_SIZE\n");
		return DMAPP_BUFFER_SIZE;
	case DMAPP_IOCTL_GET_BUFFER_PARITY:
		pr_info("DMAPP_IOCTL_GET_BUFFER_PARITY\n");
		return parity;
	case DMAPP_IOCTL_GET_BUFFER_FD:
		pr_info("DMAPP_IOCTL_GET_BUFFER_FD\n");
		ret = dma_buf_fd(dmapp_dev->buf, 0);
		if (ret < 0) {
			pr_err("dmapp_cdev_ioctl: dma_buf_fd failed with %i\n", ret);
		}
		break;
	case DMAPP_IOCTL_BUFFER_LOCK:
		pr_info("DMAPP_IOCTL_BUFFER_LOCK\n");
		/* Wait for our turn to process the buffer */
		ret = dma_fence_wait(wait_fence, true);
		if (ret < 0) {
			/* Wait may fail if interrupted by a signal */
			pr_err("dmapp_cdev_ioctl: dma_fence_wait failed (%i)\n", ret);
		} else {
			/* Reinitialize wait_fence for the next pass and set the locked flag */
			spin_lock(&dmapp_dev->spinlock);
			dma_fence_init(wait_fence, &dmapp_fence_ops, &dmapp_dev->spinlock, 0,
				parity);
			user->is_locked = true;
			spin_unlock(&dmapp_dev->spinlock);
		}
		break;
	case DMAPP_IOCTL_BUFFER_UNLOCK:
		pr_info("DMAPP_IOCTL_BUFFER_UNLOCK\n");
		/* Signal the next user that it may begin */
		ret = dma_fence_signal(signal_fence);
		if (ret < 0) {
			/* Signal may fail if the fence was previously signaled */
			pr_err("dmapp_cdev_ioctl: dma_fence_signal failed (%i)\n", ret);
		}

		/* Clear the locked flag */
		spin_lock(&dmapp_dev->spinlock);
		user->is_locked = false;
		spin_unlock(&dmapp_dev->spinlock);
		break;
	default:
		pr_err("dmapp_cdev_ioctl: %u failed\n", cmd);
		ret = -ENOTTY;
	}

	return ret;
}

static const struct file_operations dmapp_cdev_fops = {
	.owner = THIS_MODULE,
	.open = dmapp_cdev_open,
	.release = dmapp_cdev_release,
	.unlocked_ioctl = dmapp_cdev_ioctl,
};

static int dmapp_platform_driver_probe(struct platform_device *pdev) {
	struct dmapp_device *dmapp_dev;
	int ret;
	u64 context;
	struct device *device;
	struct dmapp_buffer *buffer;
	void *vaddr;
	dma_addr_t paddr;
	size_t buf_size = DMAPP_BUFFER_SIZE * sizeof(int);
	struct dma_buf_export_info exp_info = {
		.exp_name = "dmapp_buffer",
	};

	dmapp_dev = kzalloc(sizeof(*dmapp_dev), GFP_KERNEL);
	if (!dmapp_dev) {
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dmapp_dev);

	ret = alloc_chrdev_region(&dmapp_dev->dev, 0, 1, "dmapp");
	if (ret < 0) {
		pr_err("dmapp_platform_driver_probe: alloc_chrdev_region failed\n");
		goto err_alloc_chrdev;
	}

	cdev_init(&dmapp_dev->cdev, &dmapp_cdev_fops);

	device = device_create(dmapp_class, NULL, dmapp_dev->dev, NULL, "dmapp%d",
		MINOR(dmapp_dev->dev));
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		pr_err("dmapp_platform_driver_probe: device_create failed\n");
		goto err_device_create;
	}

	dmapp_dev->device = &pdev->dev;

	spin_lock_init(&dmapp_dev->spinlock);

	/* Create and initialize DMA fences */
	context = dma_fence_context_alloc(1);
	dmapp_dev->fence[0] = kzalloc(sizeof(*dmapp_dev->fence[0]),
		GFP_KERNEL);
	if (!dmapp_dev->fence[0]) {
		ret = -ENOMEM;
		pr_err("dmapp_platform_driver_probe: fence[0] allocation failed\n");
		goto err_fence_0_alloc;
	}
	dma_fence_init(dmapp_dev->fence[0], &dmapp_fence_ops, &dmapp_dev->spinlock,
		context, 0);

	dmapp_dev->fence[1] = kzalloc(sizeof(*dmapp_dev->fence[1]),
		GFP_KERNEL);
	if (!dmapp_dev->fence[1]) {
		ret = -ENOMEM;
		pr_err("dmapp_platform_driver_probe: fence[1] allocation failed\n");
		goto err_fence_1_alloc;
	}
	dma_fence_init(dmapp_dev->fence[1], &dmapp_fence_ops, &dmapp_dev->spinlock,
		context, 1);

	/* Signal fence[1] immediately after initialization since the buffer is
	 * initialized to even (0) allowing the odd pass to start
	 */
	ret = dma_fence_signal(dmapp_dev->fence[1]);
	if (ret < 0) {
		pr_err("dmapp_platform_driver_probe: Failed to signal fence[1]\n");
		goto err_fence_signal;
	}

	/* Allocate and initialize buffer */
	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer) {
		ret = -ENOMEM;
		goto err_alloc_buffer;
	}

	vaddr = dma_alloc_coherent(dmapp_dev->device, buf_size, &paddr, GFP_KERNEL);
	if (!vaddr) {
		ret = -ENOMEM;
		/* Avoid double free if cdev_add fails */
		kfree(buffer);
		goto err_alloc_coherent;
	}

	buffer->vaddr = vaddr;
	buffer->paddr = paddr;
	buffer->size = buf_size;
	buffer->dev = dmapp_dev->device;

	/* Export DMA buffer */
	exp_info.ops = &dmapp_dmabuf_ops;
	exp_info.size = buf_size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;

	dmapp_dev->buf = dma_buf_export(&exp_info);
	if (IS_ERR(dmapp_dev->buf)) {
		ret = PTR_ERR(dmapp_dev->buf);
		pr_err("dmapp_platform_driver_probe: dma_buf_export failed\n");
		/* Avoid double free if cdev_add fails */
		dma_free_coherent(dmapp_dev->device, buffer->size, buffer->vaddr,
			buffer->paddr);
		kfree(buffer);
		goto err_dma_buf_export;
	}

	ret = cdev_add(&dmapp_dev->cdev, dmapp_dev->dev, 1);
	if (ret < 0) {
		pr_err("dmapp_platform_driver_probe: cdev_add failed\n");
		goto err_cdev_add;
	}

	pr_info("dmapp_platform_driver_probe: success\n");

	return 0;

err_cdev_add:
	dma_buf_put(dmapp_dev->buf);
err_dma_buf_export:
err_alloc_coherent:
err_alloc_buffer:
err_fence_signal:
	dma_fence_put(dmapp_dev->fence[1]);
err_fence_1_alloc:
	dma_fence_put(dmapp_dev->fence[0]);
err_fence_0_alloc:
	device_destroy(dmapp_class, dmapp_dev->dev);
err_device_create:
	unregister_chrdev_region(dmapp_dev->dev, 1);
err_alloc_chrdev:
	kfree(dmapp_dev);
	return ret;
}

static int dmapp_platform_driver_remove(struct platform_device *pdev)
{
	struct dmapp_device *dmapp_dev = platform_get_drvdata(pdev);

	cdev_del(&dmapp_dev->cdev);
	dma_buf_put(dmapp_dev->buf);
	dma_fence_put(dmapp_dev->fence[1]);
	dma_fence_put(dmapp_dev->fence[0]);
	device_destroy(dmapp_class, dmapp_dev->dev);
	unregister_chrdev_region(dmapp_dev->dev, 1);
	kfree(dmapp_dev);

	pr_info("dmapp_platform_driver_remove: success\n");

	return 0;
}

static struct platform_device *dmapp_platform_device;

static struct platform_driver dmapp_platform_driver = {
	.probe  = dmapp_platform_driver_probe,
	.remove = dmapp_platform_driver_remove,
	.driver = {
		.name  = "dmapp",
		.owner = THIS_MODULE,
	},
};

static int __init dmapp_module_init(void)
{
	int ret = 0;

	dmapp_platform_device = platform_device_register_simple("dmapp", -1, NULL, 0);
	if (IS_ERR(dmapp_platform_device)) {
		pr_err("dmapp_module_init: platform_device_register_simple failed\n");
		return PTR_ERR(dmapp_platform_device);
	}

	dmapp_class = class_create(THIS_MODULE, "dmapp");
	if (IS_ERR(dmapp_class)) {
		pr_err("dmapp_module_init: class_create failed\n");
		ret = PTR_ERR(dmapp_class);
		goto err_class_create;
	}

	ret = platform_driver_register(&dmapp_platform_driver);
	if (ret < 0) {
		pr_err("dmapp_module_init: platform_driver_register failed\n");
		goto err_platform_driver_register;
	}

	pr_info("dmapp_module_init: success\n");

	return 0;

err_platform_driver_register:
	class_destroy(dmapp_class);
err_class_create:
	platform_device_unregister(dmapp_platform_device);
	return ret;
}

static void __exit dmapp_module_exit(void)
{
	platform_driver_unregister(&dmapp_platform_driver);
	class_destroy(dmapp_class);
	platform_device_unregister(dmapp_platform_device);

	pr_info("dmapp_module_exit: success\n");
}

module_init(dmapp_module_init);
module_exit(dmapp_module_exit);

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Jeff Boody <jeffboody@gmail.com>");
MODULE_DESCRIPTION("DMA Ping Pong");
