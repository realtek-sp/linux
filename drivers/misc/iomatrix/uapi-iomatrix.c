// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * This software is a confidential and proprietary property of Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in
 * whole or in part, of this work and its derivatives without express
 * permission is prohibited.
 *
 * Realtek Semiconductor Corp. reserves the right to update, modify, or
 * discontinue this software at any time without notice. This software is
 * provided "as is" and any express or implied warranties, including, but
 * not limited to, the implied warranties of merchantability and fitness for
 * a particular purpose are disclaimed. In no event shall Realtek
 * Semiconductor Corp. be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited
 * to, procurement of substitute goods or services; loss of use, data, or
 * profits; or business interruption) however caused and on any theory of
 * liability, whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this software,
 * even if advised of the possibility of such damage.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regmap.h>
#include <linux/compat.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iomatrix_ioctl.h>
#include <linux/mfd/iomatrix.h>

#define IOMATRIX_WRITE_MAX_CHUNK_B (4U * 1024U) //4k
/* 32-bit register access requires 4-byte alignment */
#define IOMATRIX_REG_ALIGN_BYTES   (4)

struct iomatrix_uapi_priv {
	struct regmap *map;
	struct miscdevice miscdev;
};

static int iomatrix_do_write_mems(struct iomatrix_uapi_priv *priv,
				  const struct iomatrix_uapi_write_req *wreq)
{
	int ret;
	void __user *uptr;
	void *kbuf;

	/* Basic validation */
	if (wreq->size == 0 || wreq->size > IOMATRIX_WRITE_MAX_CHUNK_B)
		return -EINVAL;

	uptr = (void __user *)(uintptr_t)wreq->uptr;

	kbuf = kvmalloc(wreq->size, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;

	if (copy_from_user(kbuf, uptr, wreq->size)) {
		ret = -EFAULT;
		goto out;
	}

	/* Linear write; fragmentation handled by regmap bus layer */
	ret = iomatrix_regmap_block_write_protected(priv->map, wreq->base_reg,
						    kbuf, wreq->size);

out:
	kvfree(kbuf);
	return ret;
}

static inline bool iomatrix_is_aligned_32(u32 addr)
{
	return (addr & (IOMATRIX_REG_ALIGN_BYTES - 1)) == 0;
}

static int iomatrix_do_write_mem(struct iomatrix_uapi_priv *priv,
				 const struct iomatrix_uapi_rw_req *rwreq)
{
	struct device *dev = priv->miscdev.this_device;

	if (!iomatrix_is_aligned_32(rwreq->addr)) {
		dev_err(dev, "Write addr 0x%08x is not 4-byte aligned\n",
			rwreq->addr);
		return -EINVAL;
	}

	return regmap_write(priv->map, rwreq->addr, rwreq->val);
}

static int iomatrix_do_read_mem(struct iomatrix_uapi_priv *priv,
				struct iomatrix_uapi_rw_req *rwreq)
{
	struct device *dev = priv->miscdev.this_device;
	int ret;
	u32 val = 0;

	if (!iomatrix_is_aligned_32(rwreq->addr)) {
		dev_err(dev, "Read addr 0x%08x is not 4-byte aligned\n",
			rwreq->addr);
		return -EINVAL;
	}

	ret = regmap_read(priv->map, rwreq->addr, &val);
	if (ret)
		return ret;

	rwreq->val = val;

	return 0;
}

static int iomatrix_do_erase_fspi(struct iomatrix_uapi_priv *priv,
				  const struct iomatrix_uapi_erase_req *ereq)
{
	u32 sz;
	int ret;
	u32 type = ereq->type;
	u32 addr = ereq->addr;
	u32 len = ereq->len;
	const u32 iomatrix_erase_sz_table[] = {
		4U * 1024U, /* 4K */
		32U * 1024U, /* 32K */
		64U * 1024U, /* 64K */
	};

	/* Validate type */
	if (type > IOMATRIX_ERASE_64K)
		return -EINVAL;

	sz = iomatrix_erase_sz_table[type];

	/* Validate range and alignment */
	if (len == 0)
		return -EINVAL;

	if (addr & (sz - 1)) {
		dev_err(priv->miscdev.this_device,
			"Erase addr 0x%08x not %u-byte aligned (type %u)\n",
			addr, sz, type);
		return -EINVAL;
	}
	if (len % sz) {
		dev_err(priv->miscdev.this_device,
			"Erase len %u not multiple of block %u (type %u)\n",
			len, sz, type);
		return -EINVAL;
	}

	/* Erase by blocks */
	for (; len; addr += sz, len -= sz) {
		ret = iomatrix_regmap_fspi_erase_protected(priv->map, addr,
							   type);
		if (ret) {
			dev_err(priv->miscdev.this_device,
				"Erase block failed at 0x%08x (size %u, ret %d)\n",
				addr, sz, ret);
			return ret;
		}
	}

	return 0;
}

static long iomatrix_uapi_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct miscdevice *mdev = filp->private_data;
	struct device *cdev = mdev->this_device;
	struct iomatrix_uapi_priv *priv = dev_get_drvdata(cdev);
	struct iomatrix_uapi_write_req wreq;
	struct iomatrix_uapi_erase_req ereq;
	struct iomatrix_uapi_rw_req rwreq;
	int ret;

	if (!priv || !priv->map)
		return -ENODEV;

	switch (cmd) {
	case IOMATRIX_IOC_WRITE_MEMS:
		if (copy_from_user(&wreq, (void __user *)arg, sizeof(wreq)))
			return -EFAULT;

		ret = iomatrix_do_write_mems(priv, &wreq);
		return ret;

	case IOMATRIX_IOC_ERASE_FSPI:
		if (copy_from_user(&ereq, (void __user *)arg, sizeof(ereq)))
			return -EFAULT;

		ret = iomatrix_do_erase_fspi(priv, &ereq);
		return ret;

	case IOMATRIX_IOC_WRITE_MEM:
		if (copy_from_user(&rwreq, (void __user *)arg, sizeof(rwreq)))
			return -EFAULT;

		ret = iomatrix_do_write_mem(priv, &rwreq);
		return ret;

	case IOMATRIX_IOC_READ_MEM: {
		if (copy_from_user(&rwreq, (void __user *)arg, sizeof(rwreq)))
			return -EFAULT;

		ret = iomatrix_do_read_mem(priv, &rwreq);
		if (ret)
			return ret;

		if (copy_to_user((void __user *)arg, &rwreq, sizeof(rwreq)))
			return -EFAULT;

		return 0;
	}

	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long iomatrix_uapi_compat_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	return iomatrix_uapi_ioctl(file, cmd, arg);
}
#endif

static int iomatrix_uapi_open(struct inode *inode, struct file *filp)
{
	/* filp->private_data is miscdevice* for misc devices */
	return 0;
}

static int iomatrix_uapi_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static const struct file_operations iomatrix_uapi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = iomatrix_uapi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = iomatrix_uapi_compat_ioctl,
#endif
	.open = iomatrix_uapi_open,
	.release = iomatrix_uapi_release,
	.llseek = no_llseek,
};

static void iomatrix_uapi_misc_deregister(void *data)
{
	struct iomatrix_uapi_priv *priv = data;

	misc_deregister(&priv->miscdev);
}

static int iomatrix_uapi_probe(struct platform_device *pdev)
{
	struct iomatrix_uapi_priv *priv;
	struct regmap *map;
	int ret;

	/* Get regmap from parent (MFD/I2C driver must have initialized it) */
	map = dev_get_regmap(pdev->dev.parent, NULL);
	if (!map) {
		dev_err(&pdev->dev, "Failed to get parent regmap\n");
		return -ENODEV;
	}

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->map = map;

	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = "iomatrix-uapi"; /* /dev/iomatrix-uapi */
	priv->miscdev.fops = &iomatrix_uapi_fops;
	priv->miscdev.parent = &pdev->dev;

	ret = misc_register(&priv->miscdev);
	if (ret) {
		dev_err(&pdev->dev, "misc_register failed: %d\n", ret);
		return ret;
	}

	/* Allow open/ioctl to fetch priv via miscdev.this_device */
	dev_set_drvdata(priv->miscdev.this_device, priv);
	dev_set_drvdata(&pdev->dev, priv);

	/* Ensure automatic cleanup on remove or probe failure rollback */
	ret = devm_add_action_or_reset(&pdev->dev,
				       iomatrix_uapi_misc_deregister, priv);
	if (ret)
		return ret;

	return 0;
}

static int iomatrix_uapi_remove(struct platform_device *pdev)
{
	/* Cleanup handled by devm_add_action_or_reset */
	return 0;
}

static const struct of_device_id iomatrix_uapi_of_match[] = {
	{ .compatible = "realtek,iomatrix-uapi" },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, iomatrix_uapi_of_match);

static struct platform_driver iomatrix_uapi_driver = {
	.driver = {
		.name           = "iomatrix-uapi",
		.of_match_table = iomatrix_uapi_of_match,
	},
	.probe  = iomatrix_uapi_probe,
	.remove = iomatrix_uapi_remove,
};
module_platform_driver(iomatrix_uapi_driver);

MODULE_DESCRIPTION("IOMatrix UAPI driver (ioctl)");
MODULE_LICENSE("GPL v2");
