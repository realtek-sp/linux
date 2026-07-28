// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 Realtek Semiconductor Corp. All rights reserved.
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
#include <linux/bitfield.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/kfifo.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "rts591x-espi-snoop"

#define ESPI_SNOOP_FIFO_SIZE	     2048
#define ESPI_SNOOP_CHANNEL_MAX	     2
#define ESPI_SNOOP_REMOVE_TIMEOUT_MS 30000
#define ESPI_SNOOP_FETCH_SIZE	     16
#define ESPI_SNOOP_EC_DATA_BYTES     4

/* EC .priv_data layout: int status first, followed by PORT80 buffer regs. */
#define ESPI_SNOOP_EC_PRIV_BASE	 0x20075000
#define ESPI_SNOOP_EC_BUF_STATUS (ESPI_SNOOP_EC_PRIV_BASE + 0x04)
#define ESPI_SNOOP_EC_BUF_DATA	 (ESPI_SNOOP_EC_PRIV_BASE + 0x08)
#define ESPI_SNOOP_EC_BUF_CTRL	 (ESPI_SNOOP_EC_PRIV_BASE + 0x0c)

#define ESPI_SNOOP_EC_BUF_STATUS_COUNT	  GENMASK(7, 0)
#define ESPI_SNOOP_EC_BUF_STATUS_OVERFLOW BIT(8)

#define ESPI_SNOOP_EC_BUF_CTRL_CLEAR_OVERFLOW BIT(0)
#define ESPI_SNOOP_EC_BUF_CTRL_RESET_FIFO     BIT(1)

struct rts591x_espi_snoop {
	struct regmap *regmap;
	u32 chan;

	struct kref refcount;
	struct kfifo fifo;
	spinlock_t fifo_lock;
	struct mutex read_lock;
	struct mutex state_lock;
	wait_queue_head_t wq;
	struct miscdevice miscdev;
	atomic_t open_count;
	struct completion no_open;
	bool removing;
	bool removed;
};

static struct rts591x_espi_snoop *snoop_file_to_priv(struct file *file)
{
	return container_of(file->private_data, struct rts591x_espi_snoop,
			    miscdev);
}

static void rts591x_espi_snoop_release_priv(struct kref *ref)
{
	struct rts591x_espi_snoop *priv =
		container_of(ref, struct rts591x_espi_snoop, refcount);

	kfifo_free(&priv->fifo);
	kfree(priv);
}

static bool rts591x_espi_snoop_fifo_empty(struct rts591x_espi_snoop *priv)
{
	bool empty;
	unsigned long flags;

	spin_lock_irqsave(&priv->fifo_lock, flags);
	empty = kfifo_is_empty(&priv->fifo);
	spin_unlock_irqrestore(&priv->fifo_lock, flags);

	return empty;
}

static int rts591x_espi_snoop_open(struct inode *inode, struct file *file)
{
	struct rts591x_espi_snoop *priv = snoop_file_to_priv(file);
	int ret = 0;

	mutex_lock(&priv->state_lock);
	if (priv->removing) {
		ret = -ENODEV;
	} else {
		kref_get(&priv->refcount);
		if (atomic_inc_return(&priv->open_count) == 1)
			reinit_completion(&priv->no_open);
	}
	mutex_unlock(&priv->state_lock);

	return ret;
}

static int rts591x_espi_snoop_release(struct inode *inode, struct file *file)
{
	struct rts591x_espi_snoop *priv = snoop_file_to_priv(file);

	if (atomic_dec_and_test(&priv->open_count))
		complete(&priv->no_open);

	kref_put(&priv->refcount, rts591x_espi_snoop_release_priv);

	return 0;
}

static ssize_t rts591x_espi_snoop_read(struct file *file, char __user *buffer,
				       size_t count, loff_t *ppos)
{
	struct rts591x_espi_snoop *priv = snoop_file_to_priv(file);
	unsigned int copied, size;
	unsigned long flags;
	u8 *buf;
	int ret;

	if (!count)
		return 0;

	size = min_t(size_t, count, ESPI_SNOOP_FIFO_SIZE);
	buf = kmalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = mutex_lock_interruptible(&priv->read_lock);
	if (ret)
		goto out_free;

	while (true) {
		if (rts591x_espi_snoop_fifo_empty(priv)) {
			if (READ_ONCE(priv->removing)) {
				ret = -ENODEV;
				break;
			}
			if (file->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			mutex_unlock(&priv->read_lock);
			ret = wait_event_interruptible(
				priv->wq,
				READ_ONCE(priv->removing) ||
					!rts591x_espi_snoop_fifo_empty(priv));
			if (ret == -ERESTARTSYS)
				ret = -EINTR;
			if (ret)
				goto out_free;

			ret = mutex_lock_interruptible(&priv->read_lock);
			if (ret)
				goto out_free;
			continue;
		}

		spin_lock_irqsave(&priv->fifo_lock, flags);
		copied = kfifo_out(&priv->fifo, buf, size);
		spin_unlock_irqrestore(&priv->fifo_lock, flags);
		if (!copied)
			continue;

		if (copy_to_user(buffer, buf, copied))
			ret = -EFAULT;
		else
			ret = copied;
		break;
	}

	mutex_unlock(&priv->read_lock);

out_free:
	kfree(buf);

	return ret;
}

static __poll_t rts591x_espi_snoop_poll(struct file *file,
					struct poll_table_struct *pt)
{
	struct rts591x_espi_snoop *priv = snoop_file_to_priv(file);
	__poll_t mask = 0;

	poll_wait(file, &priv->wq, pt);
	if (!rts591x_espi_snoop_fifo_empty(priv))
		mask |= EPOLLIN;
	if (READ_ONCE(priv->removing))
		mask |= EPOLLERR | EPOLLHUP;

	return mask;
}

static const struct file_operations rts591x_espi_snoop_fops = {
	.owner = THIS_MODULE,
	.open = rts591x_espi_snoop_open,
	.release = rts591x_espi_snoop_release,
	.read = rts591x_espi_snoop_read,
	.poll = rts591x_espi_snoop_poll,
	.llseek = noop_llseek,
};

static bool rts591x_espi_snoop_put_fifo(struct rts591x_espi_snoop *priv,
					const u8 *buf, unsigned int len)
{
	unsigned int copied;
	unsigned long flags;

	if (READ_ONCE(priv->removed))
		return false;

	spin_lock_irqsave(&priv->fifo_lock, flags);
	copied = kfifo_in(&priv->fifo, buf, len);
	spin_unlock_irqrestore(&priv->fifo_lock, flags);

	if (copied)
		wake_up_interruptible(&priv->wq);

	return copied == len;
}

static int rts591x_espi_snoop_fetch(struct rts591x_espi_snoop *priv)
{
	u8 buf[ESPI_SNOOP_FETCH_SIZE];
	u32 sts, data;
	unsigned int avail, copied = 0;
	int ret;

	ret = regmap_read(priv->regmap, ESPI_SNOOP_EC_BUF_STATUS, &sts);
	if (ret)
		return ret;

	if (sts & ESPI_SNOOP_EC_BUF_STATUS_OVERFLOW) {
		dev_warn_ratelimited(priv->miscdev.parent,
				     "EC port80 buffer overflow\n");
		regmap_write(priv->regmap, ESPI_SNOOP_EC_BUF_CTRL,
			     ESPI_SNOOP_EC_BUF_CTRL_CLEAR_OVERFLOW);
	}

	avail = FIELD_GET(ESPI_SNOOP_EC_BUF_STATUS_COUNT, sts);
	avail = min_t(unsigned int, avail, ESPI_SNOOP_FETCH_SIZE);
	avail = min_t(unsigned int, avail, kfifo_avail(&priv->fifo));

	while (copied < avail) {
		unsigned int chunk, i;

		ret = regmap_read(priv->regmap, ESPI_SNOOP_EC_BUF_DATA, &data);
		if (ret)
			return ret;

		chunk = min_t(unsigned int, ESPI_SNOOP_EC_DATA_BYTES,
			      avail - copied);
		for (i = 0; i < chunk; i++)
			buf[copied++] = (data >> (i * 8)) & 0xff;
	}

	ret = copied;
	if (copied && !rts591x_espi_snoop_put_fifo(priv, buf, copied)) {
		dev_warn_ratelimited(priv->miscdev.parent,
				     "kernel snoop fifo full\n");
		ret = 0;
	}

	return ret;
}

static irqreturn_t rts591x_espi_snoop_irq(int irq, void *arg)
{
	struct rts591x_espi_snoop *priv = arg;
	int ret, fetched = 0;

	do {
		ret = rts591x_espi_snoop_fetch(priv);
		if (ret < 0)
			return IRQ_HANDLED;
		fetched += ret;
	} while (ret == ESPI_SNOOP_FETCH_SIZE);

	return fetched ? IRQ_HANDLED : IRQ_NONE;
}

static int rts591x_espi_snoop_hw_init(struct rts591x_espi_snoop *priv)
{
	return regmap_write(priv->regmap, ESPI_SNOOP_EC_BUF_CTRL,
			    ESPI_SNOOP_EC_BUF_CTRL_RESET_FIFO |
				    ESPI_SNOOP_EC_BUF_CTRL_CLEAR_OVERFLOW);
}

static void rts591x_espi_snoop_put_priv(void *data)
{
	struct rts591x_espi_snoop *priv = data;

	kref_put(&priv->refcount, rts591x_espi_snoop_release_priv);
}

static void rts591x_espi_snoop_deregister(void *data)
{
	struct rts591x_espi_snoop *priv = data;

	misc_deregister(&priv->miscdev);
}

static int rts591x_espi_snoop_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *parent = dev->parent;
	struct rts591x_espi_snoop *priv;
	int irq, ret;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	kref_init(&priv->refcount);
	ret = devm_add_action_or_reset(dev, rts591x_espi_snoop_put_priv, priv);
	if (ret)
		return ret;

	priv->regmap = dev_get_regmap(parent, NULL);
	if (!priv->regmap)
		return -ENODEV;

	ret = device_property_read_u32(dev, "snoop-channel", &priv->chan);
	if (ret || priv->chan >= ESPI_SNOOP_CHANNEL_MAX) {
		dev_err(dev, "no valid 'snoop-channel' configured\n");
		return -ENODEV;
	}

	spin_lock_init(&priv->fifo_lock);
	mutex_init(&priv->read_lock);
	mutex_init(&priv->state_lock);
	init_waitqueue_head(&priv->wq);
	atomic_set(&priv->open_count, 0);
	init_completion(&priv->no_open);
	complete(&priv->no_open);

	ret = kfifo_alloc(&priv->fifo, ESPI_SNOOP_FIFO_SIZE, GFP_KERNEL);
	if (ret)
		return ret;

	priv->miscdev.minor = MISC_DYNAMIC_MINOR;
	priv->miscdev.name = devm_kasprintf(dev, GFP_KERNEL, "%s%u",
					    DEVICE_NAME, priv->chan);
	if (!priv->miscdev.name)
		return -ENOMEM;
	priv->miscdev.fops = &rts591x_espi_snoop_fops;
	priv->miscdev.parent = dev;

	ret = rts591x_espi_snoop_hw_init(priv);
	if (ret)
		return ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "Failed to get IRQ: %d\n", irq);
		return irq;
	}

	ret = devm_request_threaded_irq(dev, irq, NULL, rts591x_espi_snoop_irq,
					IRQF_ONESHOT, dev_name(dev), priv);
	if (ret) {
		dev_err(dev, "Failed to request IRQ: %d\n", ret);
		return ret;
	}

	ret = misc_register(&priv->miscdev);
	if (ret)
		return ret;

	ret = devm_add_action_or_reset(dev, rts591x_espi_snoop_deregister,
				       priv);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, priv);

	dev_info(dev, "Initialised channel %u\n", priv->chan);

	return 0;
}

static int rts591x_espi_snoop_remove(struct platform_device *pdev)
{
	struct rts591x_espi_snoop *priv = platform_get_drvdata(pdev);
	unsigned long timeout;

	mutex_lock(&priv->state_lock);
	WRITE_ONCE(priv->removing, true);
	WRITE_ONCE(priv->removed, true);
	mutex_unlock(&priv->state_lock);

	devm_release_action(&pdev->dev, rts591x_espi_snoop_deregister, priv);
	wake_up_interruptible(&priv->wq);
	timeout = wait_for_completion_timeout(
		&priv->no_open, msecs_to_jiffies(ESPI_SNOOP_REMOVE_TIMEOUT_MS));
	if (!timeout)
		dev_warn(&pdev->dev, "timeout waiting for open snoop fds\n");

	return 0;
}

static const struct of_device_id rts591x_espi_snoop_match[] = {
	{ .compatible = "realtek,rts591x-espi-snoop" },
	{}
};
MODULE_DEVICE_TABLE(of, rts591x_espi_snoop_match);

static struct platform_driver rts591x_espi_snoop_driver = {
	.driver = {
		.name = DEVICE_NAME,
		.of_match_table = rts591x_espi_snoop_match,
	},
	.probe = rts591x_espi_snoop_probe,
	.remove = rts591x_espi_snoop_remove,
};
module_platform_driver(rts591x_espi_snoop_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTS591x eSPI Snoop Sub-Driver Using MFD");
