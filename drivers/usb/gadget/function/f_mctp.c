// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * f_mctp.c -- USB MCTP function driver
 *
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

#include <linux/cdev.h>
#include <linux/configfs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/netdevice.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/usb/mctp-usb.h>
#include <uapi/linux/if_arp.h>
#include <net/mctp.h>
#include <net/mctpdevice.h>
#include <net/pkt_sched.h>

#include "configfs.h"
#include "u_f.h"
#include "u_fs.h"

#define USB_SUBCLASS_MCTP_MGMT 0x00
#define USB_SUBCLASS_MCTP_HOST 0x01
#define USB_PROTOCOL_MCTP_1X   0x01
#define STATUS_BYTECOUNT       4
#define MCTP_USB_TX_WORK_LEN   1000

/**
 * struct f_mctp_opts - Configuration options for MCTP function
 * @func_inst: USB function instance structure
 * @lock: Mutex for protecting access to options
 * @refcnt: Reference count for active instances
 * @istr: Interface string description
 * @devnum: Device number identifier
 */
struct f_mctp_opts {
	struct usb_function_instance func_inst;
	struct mutex lock;
	int refcnt;
	char *istr;
	int devnum;
};

/*-------------------------------------------------------------------------*/
/*                            MCTP gadget struct                           */

struct f_mctpg_req_list {
	struct usb_request *req;
	struct list_head list;
};

struct f_mctpg {
	struct list_head completed_out_req;
	spinlock_t out_spinlock;
	unsigned int out_qlen;

	spinlock_t in_spinlock;
	bool write_pending;
	struct usb_request *bulk_in_req;

	struct net_device *netdev;
	bool bound;
	struct usb_function func;

	struct usb_ep *bulk_in_ep;
	struct usb_ep *bulk_out_ep;
	struct work_struct rx_work;
	struct sk_buff_head tx_queue;
	wait_queue_head_t tx_wq;
	bool tx_pending;
	struct task_struct *poll_thread;
	void *bulk_in_buf;

	struct sk_buff *in_skb;
};

static int mctp_devnum_counter = 0;

static inline struct f_mctpg *func_to_mctpg(struct usb_function *f)
{
	return container_of(f, struct f_mctpg, func);
}

/*-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
/*                           Static descriptors                            */

/* Interface descriptor for MCTP */
static struct usb_interface_descriptor mctpg_interface_desc = {
	.bLength = sizeof mctpg_interface_desc,
	.bDescriptorType = USB_DT_INTERFACE,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_MCTP,
	.bInterfaceSubClass = USB_SUBCLASS_MCTP_MGMT,
	.bInterfaceProtocol = USB_PROTOCOL_MCTP_1X,
};

/* Bulk IN endpoint descriptor */
static struct usb_endpoint_descriptor hs_in_ep_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_IN | 0x02,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(MCTP_USB_XFER_SIZE),
	.bInterval = 0x01,
};

/* Bulk OUT endpoint descriptor */
static struct usb_endpoint_descriptor hs_out_ep_desc = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,

	.bEndpointAddress = USB_DIR_OUT | 0x02,
	.bmAttributes = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize = cpu_to_le16(MCTP_USB_XFER_SIZE),
	.bInterval = 0x01,
};

/* High-speed descriptor set */
static struct usb_descriptor_header *mctp_hs_descriptors[] = {
	(struct usb_descriptor_header *)&mctpg_interface_desc,
	(struct usb_descriptor_header *)&hs_in_ep_desc,
	(struct usb_descriptor_header *)&hs_out_ep_desc,
	NULL,
};

/*---------------------------string---------------------*/
#define CT_FUNC_MCTP_IDX 0

/* Default interface string */
static char def_str[USB_MAX_STRING_LEN + 1] = "MCTP Interface";

static struct usb_string ct_func_string_defs[CT_FUNC_MCTP_IDX + 1];

/* USB string definitions */
static struct usb_gadget_strings ct_func_string_table = {
	.language = 0x0409, /* en-US */
	.strings = ct_func_string_defs,
};

static struct usb_gadget_strings *ct_func_strings[] = {
	&ct_func_string_table,
	NULL,
};
/*--------------------------------------------------------------*/

/* Convert config item to MCTP options */
static inline struct f_mctp_opts *to_f_mctp_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_mctp_opts,
			    func_inst.group);
}

#define F_MCTP_OPT(name, prec, limit)                                      \
	static ssize_t f_mctp_opts_##name##_show(struct config_item *item, \
						 char *page)               \
	{                                                                  \
		struct f_mctp_opts *opts = to_f_mctp_opts(item);           \
		int result;                                                \
                                                                           \
		mutex_lock(&opts->lock);                                   \
		result = sprintf(page, "%d\n", opts->name);                \
		mutex_unlock(&opts->lock);                                 \
                                                                           \
		return result;                                             \
	}                                                                  \
                                                                           \
	static ssize_t f_mctp_opts_##name##_store(                         \
		struct config_item *item, const char *page, size_t len)    \
	{                                                                  \
		struct f_mctp_opts *opts = to_f_mctp_opts(item);           \
		int ret;                                                   \
		u##prec num;                                               \
                                                                           \
		mutex_lock(&opts->lock);                                   \
		if (opts->refcnt) {                                        \
			ret = -EBUSY;                                      \
			goto end;                                          \
		}                                                          \
                                                                           \
		ret = kstrtou##prec(page, 0, &num);                        \
		if (ret)                                                   \
			goto end;                                          \
                                                                           \
		if (num > limit) {                                         \
			ret = -EINVAL;                                     \
			goto end;                                          \
		}                                                          \
		opts->name = num;                                          \
		ret = len;                                                 \
                                                                           \
end:                                                                       \
		mutex_unlock(&opts->lock);                                 \
		return ret;                                                \
	}                                                                  \
                                                                           \
	CONFIGFS_ATTR(f_mctp_opts_, name)

F_MCTP_OPT(devnum, 8, 255);

#define USB_MAX_STRING_LEN_WITH_NULL (USB_MAX_STRING_LEN + 1)
#define F_STR_ATTR(fname, oname, cname)                                     \
	static ssize_t f_##fname##_##cname##_show(struct config_item *item, \
						  char *page)               \
	{                                                                   \
		struct oname *opts = to_##oname(item);                      \
		int ret;                                                    \
                                                                            \
		mutex_lock(&opts->lock);                                    \
		ret = sprintf(page, "%s\n", opts->cname ?: "");             \
		mutex_unlock(&opts->lock);                                  \
                                                                            \
		return ret;                                                 \
	}                                                                   \
                                                                            \
	static ssize_t f_##fname##_##cname##_store(                         \
		struct config_item *item, const char *page, size_t len)     \
	{                                                                   \
		struct oname *opts = to_##oname(item);                      \
		int ret;                                                    \
		char *str;                                                  \
		char *copy = opts->cname;                                   \
                                                                            \
		mutex_lock(&opts->lock);                                    \
		if (opts->refcnt) {                                         \
			ret = -EBUSY;                                       \
			goto end;                                           \
		}                                                           \
                                                                            \
		ret = strlen(page);                                         \
		if (ret > USB_MAX_STRING_LEN) {                             \
			ret = -EOVERFLOW;                                   \
			goto end;                                           \
		}                                                           \
                                                                            \
		if (copy) {                                                 \
			str = copy;                                         \
		} else {                                                    \
			str = kmalloc(USB_MAX_STRING_LEN_WITH_NULL,         \
				      GFP_KERNEL);                          \
			if (!str) {                                         \
				ret = -ENOMEM;                              \
				goto end;                                   \
			}                                                   \
		}                                                           \
		strcpy(str, page);                                          \
		if (str[ret - 1] == '\n')                                   \
			str[ret - 1] = '\0';                                \
		opts->cname = str;                                          \
		ret = len;                                                  \
end:                                                                        \
		mutex_unlock(&opts->lock);                                  \
		return ret;                                                 \
	}                                                                   \
                                                                            \
	CONFIGFS_ATTR(f_##fname##_, cname)

F_STR_ATTR(mctp_opts, f_mctp_opts, istr);

static struct configfs_attribute *mctp_attrs[] = {
	&f_mctp_opts_attr_istr,
	&f_mctp_opts_attr_devnum,
	NULL,
};

static void mctp_attr_release(struct config_item *item)
{
	struct f_mctp_opts *opts = to_f_mctp_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

/* ConfigFS item operations */
static struct configfs_item_operations mctpg_item_ops = {
	.release = mctp_attr_release,
};

/* ConfigFS item type definition */
static const struct config_item_type mctp_func_type = {
	.ct_item_ops = &mctpg_item_ops,
	.ct_attrs = mctp_attrs,
	.ct_owner = THIS_MODULE,
};

/**
 * mctp_usb_header_create - Create MCTP over USB header
 * @skb: Socket buffer to add header to
 * @dev: Network device
 * @type: Ethernet type
 * @daddr: Destination address
 * @saddr: Source address
 * @len: Packet length
 *
 * Prepends MCTP USB header to the skb. Returns 0 on success, error code otherwise.
 */
static int mctp_usb_header_create(struct sk_buff *skb, struct net_device *dev,
				  unsigned short type, const void *daddr,
				  const void *saddr, unsigned int len)
{
	struct mctp_usb_hdr *hdr = NULL;
	unsigned int plen;
	int rc;

	plen = skb->len;

	hdr = skb_push(skb, sizeof(*hdr));
	if (!hdr) {
		pr_err("MCTP-USB: Failed to allocate hdr (size=%zu), skb len=%u\n",
		       sizeof(*hdr), plen);
		rc = -ENOMEM;
		goto err_out;
	}
	hdr->id = cpu_to_be16(MCTP_USB_DMTF_ID);
	hdr->rsvd = 0;
	hdr->len = plen + sizeof(*hdr);

	return 0;

err_out:
	return rc;
}

/**
 * skb_read - Prepare next SKB for transmission
 * @mctpg: MCTP gadget instance
 *
 * Dequeues next SKB from TX queue and prepares USB request.
 * Returns 0 on success, error code otherwise.
 */
static int skb_read(struct f_mctpg *mctpg)
{
	unsigned long flags;
	struct sk_buff *skb;
	int ret;

	spin_lock_irqsave(&mctpg->in_spinlock, flags);
	skb = __skb_dequeue(&mctpg->tx_queue);
	spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
	if (!skb)
		return -ENODATA;

	if (skb->len > MCTP_USB_XFER_SIZE) {
		ERROR(mctpg->func.config->cdev,
		      "skb size larger than transfer size!\n");
		kfree_skb(skb);
		return -EMSGSIZE;
	}
	if (!mctpg->bulk_in_req->buf)
		mctpg->bulk_in_req->buf = mctpg->bulk_in_buf;
	/* Copy skb data to USB request buffer */
	ret = skb_copy_bits(skb, 0, mctpg->bulk_in_req->buf, skb->len);
	if (ret) {
		pr_err("MCTP-USB: skb_copy_bits fail, ret = %d\n", ret);
		kfree_skb(skb);
		return ret;
	}
	mctpg->bulk_in_req->length = skb->len;

	mctpg->in_skb = skb;

	pr_debug("MCTP-USB: skb_read, skb len = %d", skb->len);
	return 0;
}

/*if req->status != 0, send zero packet to end the BULK IN transmission*/
static void mctp_bulk_in_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mctpg *mctpg = (struct f_mctpg *)ep->driver_data;
	struct net_device *netdev = mctpg->netdev;
	struct net_device_stats *stats = &netdev->stats;
	unsigned long flags;
	int ret;

	if (req->status != 0) {
		ERROR(mctpg->func.config->cdev,
		      " Bulk In EndPoint Request ERROR: %d\n", req->status);
		if (req->actual)
			stats->tx_dropped++;
		if (mctpg->in_skb) {
			kfree_skb(mctpg->in_skb);
			mctpg->in_skb = NULL;
		}
		spin_lock_irqsave(&mctpg->in_spinlock, flags);
		mctpg->tx_pending = false;
		spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
		wake_up(&mctpg->tx_wq);
		return;
	}

	if (netif_queue_stopped(netdev))
		netif_wake_queue(netdev);

	if (mctpg->in_skb) {
		stats->tx_packets++;
		stats->tx_bytes += req->actual;
		consume_skb(mctpg->in_skb);
		mctpg->in_skb = NULL;
	}

	ret = skb_read(mctpg);
	if (!ret) {
		ret = usb_ep_queue(mctpg->bulk_in_ep, mctpg->bulk_in_req,
				   GFP_ATOMIC);
		if (ret) {
			ERROR(mctpg->func.config->cdev,
			      "Chain queue failed in complete: %d\n", ret);
			kfree_skb(mctpg->in_skb);
			mctpg->in_skb = NULL;

			spin_lock_irqsave(&mctpg->in_spinlock, flags);
			mctpg->tx_pending = false;
			spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
			wake_up(&mctpg->tx_wq);
		}

	} else {
		if (ret != -ENODATA)
			stats->tx_dropped++;

		spin_lock_irqsave(&mctpg->in_spinlock, flags);
		mctpg->tx_pending = false;
		spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
		wake_up(&mctpg->tx_wq);
	}
}

/**
 * mctpg_poll_thread - TX polling thread
 * @data: MCTP gadget instance
 *
 * Monitors TX queue and triggers USB transfers when data is available.
 * Returns 0 when stopped.
 */
static int mctpg_poll_thread(void *data)
{
	struct f_mctpg *mctpg = data;
	unsigned long in_flags;
	int ret = 0;

	for (;;) {
		if (kthread_should_stop())
			break;

		wait_event_interruptible(mctpg->tx_wq,
					 (!skb_queue_empty(&mctpg->tx_queue) &&
					  !mctpg->tx_pending) ||
						 kthread_should_stop());

		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&mctpg->in_spinlock, in_flags);
		if (mctpg->tx_pending || skb_queue_empty(&mctpg->tx_queue)) {
			spin_unlock_irqrestore(&mctpg->in_spinlock, in_flags);
			continue;
		}

		mctpg->tx_pending = true;
		spin_unlock_irqrestore(&mctpg->in_spinlock, in_flags);

		ret = skb_read(mctpg);
		if (!ret) {
			ret = usb_ep_queue(mctpg->bulk_in_ep,
					   mctpg->bulk_in_req, GFP_KERNEL);
			if (ret) {
				ERROR(mctpg->func.config->cdev,
				      "usb_ep_queue bulk in fail, ret = %d",
				      ret);
				kfree_skb(mctpg->in_skb);
				mctpg->in_skb = NULL;

				spin_lock_irqsave(&mctpg->in_spinlock,
						  in_flags);
				mctpg->tx_pending = false;
				spin_unlock_irqrestore(&mctpg->in_spinlock,
						       in_flags);
			}
		} else {
			spin_lock_irqsave(&mctpg->in_spinlock, in_flags);
			mctpg->tx_pending = false;
			spin_unlock_irqrestore(&mctpg->in_spinlock, in_flags);
		}
	}
	return 0;
}

/**
 * mctp_usb_start_xmit - Network device transmit handler
 * @skb: Socket buffer to transmit
 * @dev: Network device
 *
 * Queues outgoing packets for USB transmission. Returns NETDEV_TX_OK on success.
 */
static netdev_tx_t mctp_usb_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct f_mctpg *mctpg = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&mctpg->in_spinlock, flags);

	if (skb_queue_len(&mctpg->tx_queue) >= MCTP_USB_TX_WORK_LEN) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
		netdev_err(dev, "BUG! Tx Ring full when queue awake!\n");
		return NETDEV_TX_BUSY;
	}

	__skb_queue_tail(&mctpg->tx_queue, skb);
	if (skb_queue_len(&mctpg->tx_queue) == MCTP_USB_TX_WORK_LEN)
		netif_stop_queue(dev);
	spin_unlock_irqrestore(&mctpg->in_spinlock, flags);

	/* Wake up polling thread */
	wake_up(&mctpg->tx_wq);
	return NETDEV_TX_OK;
}

static const struct net_device_ops mctp_usb_netdev_ops = {
	.ndo_start_xmit = mctp_usb_start_xmit,
};

static const struct header_ops mctp_usb_headops = {
	.create = mctp_usb_header_create,
};

static void mctp_usb_net_setup(struct net_device *dev)
{
	dev->type = ARPHRD_MCTP;

	/* MTU configuration */
	dev->mtu = MCTP_USB_MTU_MIN * 3;
	dev->min_mtu = MCTP_USB_MTU_MIN;
	dev->max_mtu = MCTP_USB_MTU_MAX;

	/* Network device parameters */
	dev->hard_header_len = sizeof(struct mctp_usb_hdr);
	dev->tx_queue_len = DEFAULT_TX_QUEUE_LEN;
	dev->flags = IFF_NOARP;
	dev->netdev_ops = &mctp_usb_netdev_ops;
	dev->header_ops = &mctp_usb_headops;
}

static inline struct usb_request *mctpg_alloc_ep_req(struct usb_ep *ep,
						     unsigned length)
{
	return alloc_ep_req(ep, length);
}

/**
 * mctp_usb_dev_rx_work - Work function for processing received packets
 * @work: Work structure containing MCTP gadget instance
 *
 * Processes completed OUT requests and forwards data to network stack.
 */
static void mctp_usb_dev_rx_work(struct work_struct *work)
{
	struct f_mctpg *mctpg = container_of(work, struct f_mctpg, rx_work);
	struct usb_composite_dev *cdev = mctpg->func.config->cdev;
	struct f_mctpg_req_list *list;
	struct usb_request *req;
	struct sk_buff *skb;
	struct net_device *netdev = mctpg->netdev;
	unsigned long flags;
	unsigned int data_length;
	int net_status;
	struct mctp_usb_hdr *hdr;
	struct mctp_skb_cb *cb;
	struct net_device_stats *stats = &netdev->stats;
	int rc;

	spin_lock_irqsave(&mctpg->out_spinlock, flags);

	/* pick the first one */
	while (!list_empty(&mctpg->completed_out_req)) {
		list = list_first_entry(&mctpg->completed_out_req,
					struct f_mctpg_req_list, list);
		list_del(&list->list);
		req = list->req;
		spin_unlock_irqrestore(&mctpg->out_spinlock, flags);

		data_length = req->actual;
		skb = __netdev_alloc_skb(mctpg->netdev, data_length,
					 GFP_ATOMIC);
		if (!skb)
			goto err;

		if (skb_tailroom(skb) < data_length) {
			if (pskb_expand_head(skb, 0,
					     data_length - skb_tailroom(skb),
					     GFP_ATOMIC)) {
				ERROR(cdev, "SKB expansion failed\n");
				goto free;
			}
		}

		skb_put_data(skb, req->buf, data_length);

		skb_reset_mac_header(skb);
		hdr = (void *)skb_mac_header(skb);
		if (!hdr) {
			ERROR(cdev, "skb pull header failed\n");
			goto free;
		}
		skb_pull(skb, sizeof(*hdr));

		if (be16_to_cpu(hdr->id) != MCTP_USB_DMTF_ID) {
			netdev_dbg(netdev, "rx: invalid id 0x%04x\n",
				   be16_to_cpu(hdr->id));
			goto free;
		}

		if (hdr->len <
		    sizeof(struct mctp_hdr) + sizeof(struct mctp_usb_hdr)) {
			netdev_dbg(netdev, "rx: short packet (hdr) %d\n",
				   hdr->len);
			goto free;
		}

		skb->protocol = htons(ETH_P_MCTP);
		skb_reset_network_header(skb);
		cb = __mctp_cb(skb);

		net_status = netif_rx(skb);

		if (net_status == NET_RX_SUCCESS) {
			stats->rx_packets++;
			stats->rx_bytes += skb->len;
		} else {
			stats->rx_dropped++;
		}

		req->length = MCTP_USB_XFER_SIZE;
		rc = usb_ep_queue(mctpg->bulk_out_ep, req, GFP_ATOMIC);
		if (rc < 0)
			free_ep_req(mctpg->bulk_out_ep, req);

		spin_lock_irqsave(&mctpg->out_spinlock, flags);
		kfree(list);
		continue;

free:
		kfree_skb(skb);
err:
		stats->rx_dropped++;
		kfree(list);
		rc = usb_ep_queue(mctpg->bulk_out_ep, req, GFP_ATOMIC);
		if (rc < 0) {
			free_ep_req(mctpg->bulk_out_ep, req);
			req = NULL;
		}

		spin_lock_irqsave(&mctpg->out_spinlock, flags);
		break;
	}

	spin_unlock_irqrestore(&mctpg->out_spinlock, flags);
}

static void mctp_bulk_out_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_mctpg *mctpg = (struct f_mctpg *)req->context;
	struct usb_composite_dev *cdev = mctpg->func.config->cdev;
	struct f_mctpg_req_list *req_list;
	unsigned long flags;

	switch (req->status) {
	case 0:
		req_list = kzalloc(sizeof(*req_list), GFP_ATOMIC);
		if (!req_list) {
			ERROR(cdev, "Unable to allocate mem for req_list\n");
			goto free_req;
		}

		req_list->req = req;

		spin_lock_irqsave(&mctpg->out_spinlock, flags);
		list_add_tail(&req_list->list, &mctpg->completed_out_req);
		spin_unlock_irqrestore(&mctpg->out_spinlock, flags);

		schedule_work(&mctpg->rx_work);
		break;
	default:
		ERROR(cdev, "read data failed %d\n", req->status);
		/* FALLTHROUGH */
	case -ECONNABORTED: /* hardware forced ep reset */
	case -ECONNRESET: /* request dequeued */
	case -ESHUTDOWN: /* disconnect from host */

free_req:
		if (req) {
			free_ep_req(ep, req);
			req = NULL;
		}
		return;
	}
}

/*
 * allocate a bunch of read buffers and queue them all at once.
 */
static int alloc_read_req(struct f_mctpg *mctpg)
{
	struct usb_ep *ep = mctpg->bulk_out_ep;
	int i;
	int status;
	struct usb_composite_dev *cdev = mctpg->func.config->cdev;

	for (i = 0; i < mctpg->out_qlen; i++) {
		struct usb_request *req =
			mctpg_alloc_ep_req(ep, MCTP_USB_XFER_SIZE);
		if (!req)
			goto fail;
		req->context = mctpg;
		req->complete = mctp_bulk_out_complete;
		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status) {
			ERROR(cdev, "%s queue req --> %d\n", ep->name, status);
			free_ep_req(ep, req);
			return status;
		}
	}
	return 0;

fail:
	status = -ENOMEM;
	return status;
}

static int mctpg_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_mctpg *mctpg = func_to_mctpg(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int status;
	unsigned long flags;
	struct usb_request *req_in = NULL;

	DBG(cdev, "mctpg_set_alt intf:%d alt:%d\n", intf, alt);

	if (mctpg->bulk_in_ep != NULL) {
		/* restart endpoint */
		usb_ep_disable(mctpg->bulk_in_ep);
		status = config_ep_by_speed(f->config->cdev->gadget, f,
					    mctpg->bulk_in_ep);
		if (status) {
			ERROR(cdev, "config_ep_by_speed BULK IN FAILED!\n");
			goto fail;
		}
		status = usb_ep_enable(mctpg->bulk_in_ep);
		if (status) {
			ERROR(cdev, "Enable BULK IN endpoint FAILED!\n");
			goto fail;
		}
		mctpg->bulk_in_ep->driver_data = mctpg;

		req_in = mctpg_alloc_ep_req(mctpg->bulk_in_ep,
					    MCTP_USB_XFER_SIZE);
		if (!req_in) {
			status = -ENOMEM;
			goto disable_ep_bulk_in;
		}
		req_in->context = mctpg;
		req_in->complete = mctp_bulk_in_complete;
	}

	if (mctpg->bulk_out_ep != NULL) {
		/* restart endpoint */
		usb_ep_disable(mctpg->bulk_out_ep);
		status = config_ep_by_speed(f->config->cdev->gadget, f,
					    mctpg->bulk_out_ep);
		if (status) {
			ERROR(cdev, "config_ep_by_speed BULK OUT FAILED!\n");
			goto free_req_bulk_in;
		}
		status = usb_ep_enable(mctpg->bulk_out_ep);
		if (status) {
			ERROR(cdev, "Enable BULK OUT endpoint FAILED!\n");
			goto free_req_bulk_in;
		}
		mctpg->bulk_out_ep->driver_data = mctpg;

		status = alloc_read_req(mctpg);
		if (status) {
			ERROR(cdev, "alloc_read_req fail\n");
			goto disable_ep_bulk_out;
		}
	}

	if (mctpg->bulk_in_ep != NULL) {
		spin_lock_irqsave(&mctpg->in_spinlock, flags);
		mctpg->bulk_in_req = req_in;
		mctpg->bulk_in_buf = mctpg->bulk_in_req->buf;
		spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
	}

	mctpg->tx_pending = false;

	return 0;

disable_ep_bulk_out:
	if (mctpg->bulk_out_ep)
		usb_ep_disable(mctpg->bulk_out_ep);

free_req_bulk_in:
	if (req_in)
		free_ep_req(mctpg->bulk_in_ep, req_in);

disable_ep_bulk_in:
	if (mctpg->bulk_in_ep)
		usb_ep_disable(mctpg->bulk_in_ep);

fail:
	return status;
}

static void mctpg_disable(struct usb_function *f)
{
	struct f_mctpg *mctpg = func_to_mctpg(f);
	struct f_mctpg_req_list *list, *next;
	unsigned long flags;

	spin_lock_irqsave(&mctpg->in_spinlock, flags);
	if (mctpg->bulk_in_ep && mctpg->bulk_in_req) {
		if (!mctpg->bulk_in_req->buf) {
			mctpg->bulk_in_req->length = MCTP_USB_XFER_SIZE;
			mctpg->bulk_in_req->buf = mctpg->bulk_in_buf;
		}
		usb_ep_dequeue(mctpg->bulk_in_ep, mctpg->bulk_in_req);
		free_ep_req(mctpg->bulk_in_ep, mctpg->bulk_in_req);
		mctpg->bulk_in_req = NULL;
		mctpg->bulk_in_buf = NULL;
	}
	spin_unlock_irqrestore(&mctpg->in_spinlock, flags);

	spin_lock_irqsave(&mctpg->out_spinlock, flags);
	list_for_each_entry_safe(list, next, &mctpg->completed_out_req, list) {
		if (mctpg->bulk_out_ep && list->req) {
			usb_ep_dequeue(mctpg->bulk_out_ep, list->req);
			free_ep_req(mctpg->bulk_out_ep, list->req);
			list->req = NULL;
		}
		list_del(&list->list);
		kfree(list);
	}
	spin_unlock_irqrestore(&mctpg->out_spinlock, flags);

	if (mctpg->bulk_in_ep)
		usb_ep_disable(mctpg->bulk_in_ep);
	if (mctpg->bulk_out_ep)
		usb_ep_disable(mctpg->bulk_out_ep);

	spin_lock_irqsave(&mctpg->in_spinlock, flags);
	mctpg->tx_pending = true;
	spin_unlock_irqrestore(&mctpg->in_spinlock, flags);
}

static int mctpg_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_ep *ep;
	struct f_mctpg *mctpg = func_to_mctpg(f);
	struct net_device *ndev = mctpg->netdev;
	struct usb_string *us;
	int status;

	us = usb_gstrings_attach(c->cdev, ct_func_strings,
				 ARRAY_SIZE(ct_func_string_defs));
	if (IS_ERR(us))
		return PTR_ERR(us);
	mctpg_interface_desc.iInterface = us[CT_FUNC_MCTP_IDX].id;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	mctpg_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/*bulk in ep*/
	ep = usb_ep_autoconfig(c->cdev->gadget, &hs_in_ep_desc);
	if (!ep) {
		ERROR(f->config->cdev, "bulk in ep autoconfig error\n");
		goto fail;
	}

	hs_in_ep_desc.wMaxPacketSize = cpu_to_le16(MCTP_USB_XFER_SIZE);
	mctpg->bulk_in_ep = ep;

	/*bulk out ep*/
	ep = usb_ep_autoconfig(c->cdev->gadget, &hs_out_ep_desc);
	if (!ep) {
		ERROR(f->config->cdev, "bulk out ep autoconfig error\n");
		goto fail;
	}
	hs_out_ep_desc.wMaxPacketSize = cpu_to_le16(MCTP_USB_XFER_SIZE);
	mctpg->bulk_out_ep = ep;

	status = usb_assign_descriptors(f, NULL, mctp_hs_descriptors, NULL,
					NULL);
	if (status) {
		ERROR(f->config->cdev,
		      "%s: usb_assign_descriptors fail, err %d\n", f->name,
		      status);
		goto fail;
	}

	spin_lock_init(&mctpg->out_spinlock);
	spin_lock_init(&mctpg->in_spinlock);
	INIT_LIST_HEAD(&mctpg->completed_out_req);
	INIT_WORK(&mctpg->rx_work, mctp_usb_dev_rx_work);
	mctpg->poll_thread =
		kthread_run(mctpg_poll_thread, mctpg, "mctpg_poll");
	netif_wake_queue(ndev);

	printk("MCTP-USB: %s speed IN/%s OUT/%s\n",
	       gadget_is_superspeed(c->cdev->gadget) ? "super" :
	       gadget_is_dualspeed(c->cdev->gadget)  ? "dual" :
						       "full",
	       mctpg->bulk_in_ep->name, mctpg->bulk_out_ep->name);

	return 0;

fail:
	ERROR(f->config->cdev, "%s: can't bind, err %d\n", f->name, status);
	return status;
}

static void mctpg_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_mctpg *mctpg = func_to_mctpg(f);
	struct net_device *netdev = mctpg->netdev;
	unsigned long flags;

	netif_stop_queue(netdev);

	spin_lock_irqsave(&mctpg->in_spinlock, flags);
	skb_queue_purge(&mctpg->tx_queue);
	spin_unlock_irqrestore(&mctpg->in_spinlock, flags);

	wake_up(&mctpg->tx_wq);
	if (mctpg->poll_thread) {
		kthread_stop(mctpg->poll_thread);
		mctpg->poll_thread = NULL;
	}

	cancel_work_sync(&mctpg->rx_work);

	usb_free_all_descriptors(f);

	mctpg->bulk_in_ep = NULL;
	mctpg->bulk_out_ep = NULL;
}

static void mctpg_free(struct usb_function *f)
{
	struct f_mctpg *mctpg;
	struct f_mctp_opts *opts;

	mctpg = func_to_mctpg(f);
	opts = container_of(f->fi, struct f_mctp_opts, func_inst);
	if (mctpg->bound) {
		mctp_unregister_netdev(mctpg->netdev);
		mctpg->bound = false;
	}
	if (mctpg->netdev) {
		free_netdev(mctpg->netdev);
		mctpg->netdev = NULL;
	}
	mutex_lock(&opts->lock);
	--opts->refcnt;
	mutex_unlock(&opts->lock);
}

static void mctp_free_inst(struct usb_function_instance *f)
{
	struct f_mctp_opts *opts;

	opts = container_of(f, struct f_mctp_opts, func_inst);
	kfree(opts);
}

static struct usb_function_instance *mctp_alloc_inst(void)
{
	struct f_mctp_opts *opts;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);
	mutex_init(&opts->lock);
	opts->istr = def_str;
	opts->devnum = mctp_devnum_counter++;
	opts->func_inst.free_func_inst = mctp_free_inst;

	config_group_init_type_name(&opts->func_inst.group, "",
				    &mctp_func_type);

	return &opts->func_inst;
}

static struct usb_function *mctp_alloc(struct usb_function_instance *fi)
{
	struct f_mctpg *mctpg;
	struct f_mctp_opts *opts;
	char namebuf[30];
	int devnum;
	struct net_device *ndev;
	int status;

	opts = container_of(fi, struct f_mctp_opts, func_inst);
	mutex_lock(&opts->lock);
	opts->refcnt++;
	devnum = opts->devnum;
	ct_func_string_defs[CT_FUNC_MCTP_IDX].s = opts->istr;

	mutex_unlock(&opts->lock);

	snprintf(namebuf, sizeof(namebuf), "mctpusb%d", devnum);
	ndev = alloc_netdev(sizeof(*mctpg), namebuf, NET_NAME_ENUM,
			    mctp_usb_net_setup);
	if (!ndev) {
		return ERR_PTR(-ENOMEM);
	}

	mctpg = netdev_priv(ndev);
	mctpg->netdev = ndev;
	mctpg->tx_pending = true;
	mctpg->bound = false;
	init_waitqueue_head(&mctpg->tx_wq);
	skb_queue_head_init(&mctpg->tx_queue);

	mctpg->func.name = "mctp";
	mctpg->func.bind = mctpg_bind;
	mctpg->func.unbind = mctpg_unbind;
	mctpg->func.set_alt = mctpg_set_alt;
	mctpg->func.free_func = mctpg_free;
	mctpg->func.disable = mctpg_disable;

	mctpg->out_qlen = 4;

	if (!mctpg->bound) {
		status = mctp_register_netdev(ndev, NULL);
		if (status) {
			ERROR(mctpg->func.config->cdev,
			      "mctp_register_netdev FAILED\n");
			free_netdev(ndev);
			return ERR_PTR(status);
		}
		mctpg->bound = true;
	}

	return &mctpg->func;
}

DECLARE_USB_FUNCTION_INIT(mctp, mctp_alloc_inst, mctp_alloc);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MCTP USB Composite Function");
