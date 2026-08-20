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

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/iomatrix_ioctl.h>
#include <linux/mfd/iomatrix.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sizes.h>
#include <linux/uaccess.h>

#define IOMATRIX_REG_ALIGN_BYTES   4U
#define IOMATRIX_SPIC_WRITE_BYTES  128U
#define IOMATRIX_SECTOR_BYTES	   SZ_4K
#define IOMATRIX_FW_SECTOR_RETRIES 3U

static DEFINE_IDA(iomatrix_uapi_ida);

struct iomatrix_uapi_priv {
	struct regmap *map;
	struct miscdevice miscdev;
	struct fw_upload *fw_upload;
	int id;
	atomic_t cancel;
	bool update_locked;
	const u8 *image;
	u32 image_size;
};

static inline bool iomatrix_is_aligned_32(u32 addr)
{
	return !(addr & (IOMATRIX_REG_ALIGN_BYTES - 1));
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
	unsigned int val;
	int ret;

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

static long iomatrix_uapi_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	struct miscdevice *mdev = filp->private_data;
	struct iomatrix_uapi_priv *priv = dev_get_drvdata(mdev->this_device);
	struct iomatrix_uapi_rw_req rwreq;
	int ret;

	if (!priv || !priv->map)
		return -ENODEV;

	switch (cmd) {
	case IOMATRIX_IOC_WRITE_MEM:
		if (copy_from_user(&rwreq, (void __user *)arg, sizeof(rwreq)))
			return -EFAULT;
		return iomatrix_do_write_mem(priv, &rwreq);

	case IOMATRIX_IOC_READ_MEM:
		if (copy_from_user(&rwreq, (void __user *)arg, sizeof(rwreq)))
			return -EFAULT;

		ret = iomatrix_do_read_mem(priv, &rwreq);
		if (ret)
			return ret;
		if (copy_to_user((void __user *)arg, &rwreq, sizeof(rwreq)))
			return -EFAULT;
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

static const struct file_operations iomatrix_uapi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = iomatrix_uapi_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
	.llseek = no_llseek,
};

static enum fw_upload_err iomatrix_fw_err(int ret)
{
	if (ret == -ETIMEDOUT)
		return FW_UPLOAD_ERR_TIMEOUT;
	if (ret == -ERANGE || ret == -EINVAL)
		return FW_UPLOAD_ERR_INVALID_SIZE;
	return FW_UPLOAD_ERR_RW_ERROR;
}

static u32 iomatrix_fw_sector_bytes(const struct iomatrix_uapi_priv *priv,
				    u32 sector)
{
	u32 offset = sector * IOMATRIX_SECTOR_BYTES;

	return min_t(u32, priv->image_size - offset, IOMATRIX_SECTOR_BYTES);
}

/*
 * Fill one sector's RAM buffer on the EC with write frames (<=128 bytes each at
 * an absolute Flash offset, each ACKed), then commit it with a single update:
 * the EC compares the buffer against Flash and erases + programs as needed.
 * Returns the raw transport error so the caller can decide whether to retry.
 */
static int iomatrix_fw_send_sector(struct iomatrix_uapi_priv *priv, u32 base,
				   u32 bytes)
{
	u32 done = 0;
	int ret;

	while (done < bytes) {
		u32 len = min_t(u32, bytes - done, IOMATRIX_SPIC_WRITE_BYTES);

		ret = iomatrix_regmap_spic_write(priv->map, base + done,
						 priv->image + base + done, len,
						 IOMATRIX_SPIC_SEQ_SINGLE);
		if (ret)
			return ret;
		done += len;
	}

	return iomatrix_regmap_spic_update(priv->map);
}

/*
 * Stream one sector to the EC, one acknowledged frame at a time, then commit it.
 * A lost frame or update surfaces as -ETIMEDOUT; that is the only recoverable
 * case and is retried by retransmitting the whole sector (re-filling the EC
 * buffer before the update).  Other errors are fatal.
 */
static enum fw_upload_err
iomatrix_fw_write_sector(struct iomatrix_uapi_priv *priv, u32 sector)
{
	u32 base = sector * IOMATRIX_SECTOR_BYTES;
	u32 bytes = iomatrix_fw_sector_bytes(priv, sector);
	unsigned int tries;
	int ret = 0;

	for (tries = 0; tries <= IOMATRIX_FW_SECTOR_RETRIES; tries++) {
		ret = iomatrix_fw_send_sector(priv, base, bytes);
		if (!ret)
			return FW_UPLOAD_ERR_NONE;
		if (ret != -ETIMEDOUT)
			break;
	}

	return iomatrix_fw_err(ret);
}

/*
 * Send every sector, in order from 0, as a write burst followed by an update.
 * Cancellation is honoured only at a sector boundary: once a sector's writes
 * and update have started, the sector is finished before stopping.
 */
static enum fw_upload_err
iomatrix_fw_sector_update(struct iomatrix_uapi_priv *priv)
{
	u32 sectors = DIV_ROUND_UP(priv->image_size, IOMATRIX_SECTOR_BYTES);
	enum fw_upload_err err = FW_UPLOAD_ERR_NONE;
	u32 sector;

	for (sector = 0; sector < sectors; sector++) {
		if (atomic_read(&priv->cancel)) {
			err = FW_UPLOAD_ERR_CANCELED;
			break;
		}

		err = iomatrix_fw_write_sector(priv, sector);
		if (err != FW_UPLOAD_ERR_NONE)
			break;
	}
	return err;
}

static enum fw_upload_err iomatrix_fw_prepare(struct fw_upload *fw_upload,
					      const u8 *data, u32 size)
{
	struct iomatrix_uapi_priv *priv = fw_upload->dd_handle;
	struct device *dev = priv->miscdev.this_device;
	enum fw_upload_err err;
	int ret;

	if (!data || !size || size > U32_MAX - (IOMATRIX_SECTOR_BYTES - 1))
		return FW_UPLOAD_ERR_INVALID_SIZE;

	atomic_set(&priv->cancel, 0);
	ret = iomatrix_regmap_update_lock(priv->map);
	if (ret)
		return iomatrix_fw_err(ret);

	priv->update_locked = true;
	priv->image = data;
	priv->image_size = size;
	err = iomatrix_fw_sector_update(priv);
	if (err != FW_UPLOAD_ERR_NONE) {
		iomatrix_regmap_update_unlock(priv->map);
		priv->update_locked = false;
		priv->image = NULL;
		priv->image_size = 0;
		return err;
	}

	/*
	 * All sectors committed to Flash.  Reboot the EC into the new image
	 * while the channel is still held exclusively, and wait for the EC to
	 * acknowledge its address again: only then is the reboot confirmed.  If
	 * it never comes back the Flash write did succeed, but report the error
	 * so the update is not declared complete without a running new image.
	 */
	ret = iomatrix_regmap_spic_reboot(priv->map);
	if (ret) {
		dev_err(dev, "EC reboot confirmation failed: %d\n", ret);
		iomatrix_regmap_update_unlock(priv->map);
		priv->update_locked = false;
		priv->image = NULL;
		priv->image_size = 0;
		return iomatrix_fw_err(ret);
	}

	return FW_UPLOAD_ERR_NONE;
}

static enum fw_upload_err iomatrix_fw_write(struct fw_upload *fw_upload,
					    const u8 *data, u32 offset,
					    u32 size, u32 *written)
{
	struct iomatrix_uapi_priv *priv = fw_upload->dd_handle;

	(void)data;
	(void)offset;

	if (atomic_read(&priv->cancel))
		return FW_UPLOAD_ERR_CANCELED;

	*written = size;
	return FW_UPLOAD_ERR_NONE;
}

static enum fw_upload_err iomatrix_fw_poll_complete(struct fw_upload *fw_upload)
{
	struct iomatrix_uapi_priv *priv = fw_upload->dd_handle;

	return atomic_read(&priv->cancel) ? FW_UPLOAD_ERR_CANCELED :
					    FW_UPLOAD_ERR_NONE;
}

static void iomatrix_fw_cancel(struct fw_upload *fw_upload)
{
	struct iomatrix_uapi_priv *priv = fw_upload->dd_handle;

	atomic_set(&priv->cancel, 1);
}

static void iomatrix_fw_cleanup(struct fw_upload *fw_upload)
{
	struct iomatrix_uapi_priv *priv = fw_upload->dd_handle;

	if (priv->update_locked) {
		iomatrix_regmap_update_unlock(priv->map);
		priv->update_locked = false;
	}
	priv->image = NULL;
	priv->image_size = 0;
}

static const struct fw_upload_ops iomatrix_fw_upload_ops = {
	.prepare = iomatrix_fw_prepare,
	.write = iomatrix_fw_write,
	.poll_complete = iomatrix_fw_poll_complete,
	.cancel = iomatrix_fw_cancel,
	.cleanup = iomatrix_fw_cleanup,
};

static void iomatrix_fw_upload_unregister(void *data)
{
	firmware_upload_unregister(data);
}

static void iomatrix_uapi_misc_deregister(void *data)
{
	struct iomatrix_uapi_priv *priv = data;

	misc_deregister(&priv->miscdev);
}

static void iomatrix_uapi_ida_free(void *data)
{
	struct iomatrix_uapi_priv *priv = data;

	ida_free(&iomatrix_uapi_ida, priv->id);
}

static int iomatrix_uapi_probe(struct platform_device *pdev)
{
	struct iomatrix_uapi_priv *priv;
	struct regmap *map;
	char *fw_name;
	int ret;

	map = dev_get_regmap(pdev->dev.parent, NULL);
	if (!map)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->map = map;
	atomic_set(&priv->cancel, 0);

	priv->id = ida_alloc(&iomatrix_uapi_ida, GFP_KERNEL);
	if (priv->id < 0)
		return priv->id;
	ret = devm_add_action_or_reset(&pdev->dev, iomatrix_uapi_ida_free,
				       priv);
	if (ret)
		return ret;

	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = devm_kasprintf(&pdev->dev, GFP_KERNEL,
					    "iomatrix-uapi%d", priv->id);
	if (!priv->miscdev.name)
		return -ENOMEM;
	priv->miscdev.fops = &iomatrix_uapi_fops;
	priv->miscdev.parent = &pdev->dev;

	ret = misc_register(&priv->miscdev);
	if (ret)
		return ret;

	dev_set_drvdata(priv->miscdev.this_device, priv);
	dev_set_drvdata(&pdev->dev, priv);
	ret = devm_add_action_or_reset(&pdev->dev,
				       iomatrix_uapi_misc_deregister, priv);
	if (ret)
		return ret;

	fw_name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "rts591x-ec%d",
				 priv->id);
	if (!fw_name)
		return -ENOMEM;

	priv->fw_upload =
		firmware_upload_register(THIS_MODULE, &pdev->dev, fw_name,
					 &iomatrix_fw_upload_ops, priv);
	if (IS_ERR(priv->fw_upload))
		return PTR_ERR(priv->fw_upload);

	return devm_add_action_or_reset(
		&pdev->dev, iomatrix_fw_upload_unregister, priv->fw_upload);
}

static const struct of_device_id iomatrix_uapi_of_match[] = {
	{ .compatible = "realtek,iomatrix-uapi" },
	{}
};
MODULE_DEVICE_TABLE(of, iomatrix_uapi_of_match);

static struct platform_driver iomatrix_uapi_driver = {
	.driver = {
		.name = "iomatrix-uapi",
		.of_match_table = iomatrix_uapi_of_match,
	},
	.probe = iomatrix_uapi_probe,
};
module_platform_driver(iomatrix_uapi_driver);

MODULE_DESCRIPTION("IOMatrix register and firmware update driver");
MODULE_LICENSE("GPL v2");
