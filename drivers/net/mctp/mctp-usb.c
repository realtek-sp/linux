// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Management Controller Transport Protocol (MCTP)
 *
 * A driver to access MCTP devices over USB transport,
 * from DMTF specification DSP0283.
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

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/usb.h>
#include <net/mctp.h>
#include <net/mctpdevice.h>
#include <net/pkt_sched.h>
#include <linux/skbuff.h>
#include <uapi/linux/if_arp.h>
#include <linux/mutex.h>
#include <linux/kthread.h>

#define MCTP_USB_XFER_SIZE     512
#define MCTP_USB_BTU	       68 /* base mtu (64) + mctp header */
#define MCTP_USB_MTU_MIN       MCTP_USB_BTU
#define MCTP_USB_MTU_MAX       (U8_MAX - sizeof(struct mctp_usb_hdr))
#define MCTP_USB_DMTF_ID       0x1AB4
#define USB_SUBCLASS_MCTP_MGMT 0x00
#define USB_SUBCLASS_MCTP_HOST 0x01
#define USB_PROTOCOL_MCTP_1X   0x01
#define MCTP_USB_TX_WORK_LEN   1000
#define PHYSICAL_ADDR_SIZE     2
#define RX_RETRY_DELAY	       msecs_to_jiffies(100)

/**
 * struct mctp_usb_hdr - MCTP over USB packet header
 * @id: DMTF identifier (always set to 0x1AB4)
 * @rsvd: Reserved field
 * @len: Length of the MCTP over USB packet in Bytes
 */
struct mctp_usb_hdr {
	__be16 id;
	u8 rsvd;
	u8 len;
} __packed;

/**
 * struct mctp_usb_internal_hdr - Internal addressing header
 * @dest: Destination physical address
 * @source: Source physical address
 *
 * Used for routing devices on the MCTP USB bus
 */
struct mctp_usb_internal_hdr {
	u8 dest[PHYSICAL_ADDR_SIZE];
	u8 source[PHYSICAL_ADDR_SIZE];
} __packed;

/**
 * struct mctp_usb_bus - Represents an MCTP USB bus
 * @busnum: USB bus number
 * @netdev: Associated network device
 * @list: List entry for global bus list
 * @devices: List of devices on this bus
 * @lock: Spinlock for protecting device list
 * @tx_queue: Queue for outgoing packets
 * @tx_thread: Kernel thread for transmitting packets
 * @tx_wq: Wait queue for TX thread
 * @rx_queue: Queue for incoming packets
 * @rx_work: Work item for processing received packets
 *
 */
struct mctp_usb_bus {
	int busnum;
	struct net_device *netdev;

	struct list_head list;
	struct list_head devices;
	spinlock_t lock;

	struct sk_buff_head tx_queue;
	struct task_struct *tx_thread;
	wait_queue_head_t tx_wq;

	struct sk_buff_head rx_queue;
	struct work_struct rx_work;
};

/**
 * struct mctp_usb_device - Represents an MCTP-capable USB device
 * @udev: USB device instance
 * @intf: USB interface instance
 * @list: List entry in bus's device list
 * @bus: Associated MCTP USB bus
 * @bulk_in_ep: Bulk IN endpoint address
 * @bulk_out_ep: Bulk OUT endpoint address
 * @int_in_ep: Interrupt IN endpoint address
 * @int_buf: Buffer for interrupt transfers
 * @bulk_out_urb: URB for bulk out transfers
 * @bulk_in_urb: URB for bulk in transfers
 * @int_in_urb: URB for interrupt in transfers
 * @dev_addr: Device address on MCTP bus
 * @lock: Mutex for device synchronization
 * @rx_retry: Delayed work for RX retry mechanism
 */
struct mctp_usb_device {
	struct usb_device *udev;
	struct usb_interface *intf;

	struct list_head list;
	struct mctp_usb_bus *bus;

	u8 bulk_in_ep;
	u8 bulk_out_ep;
	u8 int_in_ep;
	u8 *int_buf;

	struct urb *bulk_out_urb;
	struct urb *bulk_in_urb;
	struct urb *int_in_urb;

	int dev_addr;
	struct mutex lock;

	struct delayed_work rx_retry;
};

struct bulk_info {
	struct sk_buff *skb;
	struct mctp_usb_device *mdev;
};

static LIST_HEAD(mctp_usb_buses); /* Global list of MCTP USB buses */
static DEFINE_MUTEX(mctp_usb_bus_mutex);

static void mctp_usb_remove_device(struct mctp_usb_device *mdev)
{
	struct mctp_usb_bus *mbus = mdev->bus;
	unsigned long flags;

	if (mdev->int_in_urb) {
		usb_kill_urb(mdev->int_in_urb);
		usb_free_urb(mdev->int_in_urb);
	}
	if (mdev->bulk_in_urb) {
		usb_kill_urb(mdev->bulk_in_urb);
		usb_free_urb(mdev->bulk_in_urb);
	}
	if (mdev->bulk_out_urb) {
		usb_kill_urb(mdev->bulk_out_urb);
		usb_free_urb(mdev->bulk_out_urb);
	}

	cancel_delayed_work_sync(&mdev->rx_retry);

	kfree(mdev->int_buf);
	mdev->int_buf = NULL;

	spin_lock_irqsave(&mbus->lock, flags);
	list_del(&mdev->list);
	spin_unlock_irqrestore(&mbus->lock, flags);

	usb_set_intfdata(mdev->intf, NULL);

	kfree(mdev);
}

/**
 * mctp_usb_header_create - Create MCTP USB packet header
 *
 * Prepends MCTP USB header and internal addressing header to skb
 * Returns 0 on success, negative error code on failure
 */
static int mctp_usb_header_create(struct sk_buff *skb, struct net_device *dev,
				  unsigned short type, const void *daddr,
				  const void *saddr, unsigned int len)
{
	struct mctp_usb_internal_hdr *ihdr = NULL;
	struct mctp_usb_hdr *hdr = NULL;
	unsigned int plen;
	int rc;

	plen = skb->len;

	hdr = skb_push(skb, sizeof(*hdr));
	if (!hdr) {
		rc = -ENOMEM;
		pr_err("MCTP-USB: Failed to allocate hdr (size=%zu), skb len=%u\n",
		       sizeof(*hdr), plen);

		goto err_out;
	}
	hdr->id = cpu_to_be16(MCTP_USB_DMTF_ID);
	hdr->rsvd = 0;
	hdr->len = plen + sizeof(*hdr);

	ihdr = skb_push(skb, sizeof(struct mctp_usb_internal_hdr));
	if (!ihdr) {
		rc = -ENOMEM;
		pr_err("MCTP-USB: Failed to allocate internal hdr (size=%zu), skb len=%u\n",
		       sizeof(struct mctp_usb_internal_hdr), skb->len);
		goto err_out;
	}
	skb_reset_mac_header(skb);
	pr_debug("%s: daddr = 0x%02x 0x%02x\n", __func__, ((u8 *)daddr)[0],
		 ((u8 *)daddr)[1]);
	pr_debug("%s: saddr = 0x%02x 0x%02x\n", __func__, ((u8 *)saddr)[0],
		 ((u8 *)saddr)[1]);
	if (daddr)
		memcpy(ihdr->dest, daddr, PHYSICAL_ADDR_SIZE);
	if (saddr)
		memcpy(ihdr->source, saddr, PHYSICAL_ADDR_SIZE);
	return 0;

err_out:
	return rc;
}

static int mctp_usb_interrupt_start(struct mctp_usb_device *mdev, gfp_t gfp);

static void rx_retry_work(struct work_struct *work)
{
	struct mctp_usb_device *mdev =
		container_of(work, struct mctp_usb_device, rx_retry.work);

	mctp_usb_interrupt_start(mdev, GFP_KERNEL);
}

/**
 * mctp_usb_start_xmit - Network device transmit handler
 * @skb: Socket buffer to transmit
 * @dev: Network device
 *
 * Queues packet for transmission by TX thread
 * Returns NETDEV_TX_OK on success, NETDEV_TX_BUSY on queue full
 */
static netdev_tx_t mctp_usb_start_xmit(struct sk_buff *skb,
				       struct net_device *dev)
{
	struct mctp_usb_bus *mbus = netdev_priv(dev);
	unsigned long flags;

	spin_lock_irqsave(&mbus->tx_queue.lock, flags);
	if (skb_queue_len(&mbus->tx_queue) >= MCTP_USB_TX_WORK_LEN - 1) {
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&mbus->tx_queue.lock, flags);
		netdev_err(dev, "BUG! Tx Ring full when queue awake!\n");
		return NETDEV_TX_BUSY;
	}

	__skb_queue_tail(&mbus->tx_queue, skb);
	if (skb_queue_len(&mbus->tx_queue) == MCTP_USB_TX_WORK_LEN)
		netif_stop_queue(dev);
	spin_unlock_irqrestore(&mbus->tx_queue.lock, flags);

	wake_up(&mbus->tx_wq);
	return NETDEV_TX_OK;
}

/**
 * mctp_usb_out_complete - Completion handler for bulk OUT URBs
 * @urb: Completed URB
 *
 * Handles transmission results and updates network statistics
 */
static void mctp_usb_out_complete(struct urb *urb)
{
	struct bulk_info *info = urb->context;
	struct sk_buff *skb = info->skb;
	struct net_device *netdev = skb->dev;
	struct net_device_stats *stats = &netdev->stats;
	struct mctp_usb_device *mdev = info->mdev;
	int status;

	if (!mdev) {
		pr_err("%s error: urb context invalid (mdev is NULL)\n",
		       __func__);
		goto err_free;
	}

	status = urb->status;

	switch (status) {
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -EPROTO:
		stats->tx_errors++;
		break;
	case 0:
		stats->tx_packets++;
		stats->tx_bytes += urb->actual_length;
		netif_wake_queue(netdev);
		consume_skb(skb);
		kfree(info);
		mutex_unlock(&mdev->lock);
		return;
	default:
		netdev_dbg(netdev, "unexpected tx urb status: %d\n", status);
		stats->tx_dropped++;
	}

err_free:
	kfree_skb(skb);
	kfree(info);
	mutex_unlock(&mdev->lock);
}

static struct mctp_usb_device *mctp_usb_lookup_device(struct mctp_usb_bus *mbus,
						      int dev_addr)
{
	struct mctp_usb_device *mdev = NULL, *ret = NULL;

	mutex_lock(&mctp_usb_bus_mutex);
	list_for_each_entry(mdev, &mbus->devices, list) {
		if (mdev->dev_addr == dev_addr) {
			ret = mdev;
			mutex_lock(&mdev->lock);
			break;
		}
	}
	mutex_unlock(&mctp_usb_bus_mutex);
	return ret;
}

/**
 * mctp_usb_xmit - Transmit packet to specific device
 * @mbus: MCTP USB bus
 * @skb: Socket buffer to transmit
 *
 * Processes packet from TX queue and submits to USB device
 */
static void mctp_usb_xmit(struct mctp_usb_bus *mbus, struct sk_buff *skb)
{
	struct net_device_stats *stats = &mbus->netdev->stats;
	struct mctp_usb_internal_hdr *ihdr;
	unsigned int plen;
	struct urb *urb;
	struct mctp_usb_device *mdev;
	int rc, dev_addr;
	struct bulk_info *info;
	u8 dest_ep;

	ihdr = (void *)skb_mac_header(skb);
	dev_addr = ihdr->dest[1];
	dest_ep = ihdr->dest[0];
	pr_debug("%s: dev_addr = %x", __func__, ihdr->dest[1]);
	pr_debug("%s: dest_ep = %x", __func__, ihdr->dest[0]);
	skb_pull(skb, sizeof(struct mctp_usb_internal_hdr));

	plen = skb->len;

	pr_debug("%s: skb->len = %d", __func__, plen);
	if (plen > MCTP_USB_XFER_SIZE)
		goto err_drop;

	mdev = mctp_usb_lookup_device(mbus, dev_addr);
	if (!mdev) {
		goto err_drop;
	}

	if ((dest_ep & 0x0f) != mdev->bulk_out_ep) {
		pr_err("MCTP-USB: Endpoint mismatch (dest_ep&0x0f = 0x%2x, bulk_out_ep = 0x%2x)\n",
		       dest_ep & 0x0f, mdev->bulk_out_ep);
		mutex_unlock(&mdev->lock);
		goto err_drop;
	}

	urb = mdev->bulk_out_urb;

	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	info->skb = skb;
	info->mdev = mdev;

	usb_fill_bulk_urb(urb, mdev->udev,
			  usb_sndbulkpipe(mdev->udev, mdev->bulk_out_ep),
			  skb->data, skb->len, mctp_usb_out_complete, info);

	rc = usb_submit_urb(urb, GFP_ATOMIC);
	if (rc) {
		mutex_unlock(&mdev->lock);
		goto err_drop;
	}
	return;

err_drop:
	pr_debug("%s: error, tx_dropped\n", __func__);
	stats->tx_dropped++;
	kfree_skb(skb);
}

/**
 * mctp_usb_tx_thread - Kernel thread for packet transmission
 * @data: MCTP USB bus structure
 *
 * Processes TX queue and transmits packets to devices
 */
static int mctp_usb_tx_thread(void *data)
{
	struct mctp_usb_bus *mbus = data;
	struct sk_buff *skb;
	unsigned long flags;

	for (;;) {
		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&mbus->tx_queue.lock, flags);
		skb = __skb_dequeue(&mbus->tx_queue);
		if (netif_queue_stopped(mbus->netdev))
			netif_wake_queue(mbus->netdev);
		spin_unlock_irqrestore(&mbus->tx_queue.lock, flags);

		if (skb)
			mctp_usb_xmit(mbus, skb);
		else
			wait_event(mbus->tx_wq,
				   !skb_queue_empty(&mbus->tx_queue) ||
					   kthread_should_stop());
	}

	return 0;
}

/* Network device operations */
static const struct net_device_ops mctp_usb_netdev_ops = {
	.ndo_start_xmit = mctp_usb_start_xmit,
};

/* Header operations */
static const struct header_ops mctp_usb_headops = {
	.create = mctp_usb_header_create,
};

/*Initialize network device*/
static void mctp_usb_net_setup(struct net_device *dev)
{
	dev->type = ARPHRD_MCTP;

	dev->mtu = MCTP_USB_BTU * 3;
	dev->min_mtu = MCTP_USB_MTU_MIN;
	dev->max_mtu = MCTP_USB_MTU_MAX;

	dev->tx_queue_len = DEFAULT_TX_QUEUE_LEN;
	dev->flags = IFF_NOARP;
	dev->hard_header_len = sizeof(struct mctp_usb_hdr) +
			       sizeof(struct mctp_usb_internal_hdr);
	dev->addr_len = PHYSICAL_ADDR_SIZE;
	dev->netdev_ops = &mctp_usb_netdev_ops;
	dev->header_ops = &mctp_usb_headops;
}

/**
 * mctp_usb_rx_work - Work function for processing received packets
 * @work: Work structure
 *
 * Dequeues packets from RX queue and passes them to network stack
 */
static void mctp_usb_rx_work(struct work_struct *work)
{
	struct mctp_usb_bus *mbus =
		container_of(work, struct mctp_usb_bus, rx_work);
	struct sk_buff *skb;
	struct net_device *netdev = mbus->netdev;
	unsigned long flags;
	int net_status;
	struct mctp_skb_cb *cb;
	int ihdr_enable = 0;
	struct net_device_stats *stats = &netdev->stats;
	int len;

	spin_lock_irqsave(&mbus->rx_queue.lock, flags);
	while ((skb = __skb_dequeue(&mbus->rx_queue)) != NULL) {
		struct mctp_usb_internal_hdr *ihdr = NULL;

		spin_unlock_irqrestore(&mbus->rx_queue.lock, flags);
		ihdr_enable = 1;

		while (skb) {
			struct sk_buff *skb2 = NULL;
			struct mctp_usb_hdr *hdr;
			u8 pkt_len;

			if (!ihdr) {
				skb_reset_mac_header(skb);
				ihdr = (void *)skb_mac_header(skb);
				if (!ihdr) {
					netdev_err(netdev, "pull ihdr fail\n");
					break;
				}
				skb_pull(skb, sizeof(*ihdr));
			}

			skb_reset_mac_header(skb);
			hdr = (void *)skb_mac_header(skb);
			if (!hdr) {
				netdev_err(netdev, "pull hdr fail\n");
				break;
			}
			skb_pull(skb, sizeof(*hdr));

			if (be16_to_cpu(hdr->id) != MCTP_USB_DMTF_ID) {
				netdev_dbg(netdev, "rx: invalid id %04x\n",
					   be16_to_cpu(hdr->id));
				break;
			}

			if (hdr->len < sizeof(struct mctp_hdr) +
					       sizeof(struct mctp_usb_hdr)) {
				netdev_dbg(netdev,
					   "rx: short packet (hdr) %d\n",
					   hdr->len);
				break;
			}

			pkt_len = hdr->len - sizeof(struct mctp_usb_hdr);
			if (pkt_len > skb->len) {
				netdev_dbg(
					netdev,
					"rx: short packet (xfer) %d, actual %d\n",
					hdr->len, skb->len);
				break;
			}

			if (pkt_len < skb->len) {
				skb2 = skb_clone(skb, GFP_ATOMIC);
				if (skb2) {
					if (!skb_pull(skb2, pkt_len)) {
						kfree_skb(skb2);
						skb2 = NULL;
					}
				}
				skb_trim(skb, pkt_len);
			}

			skb->protocol = htons(ETH_P_MCTP);
			skb_reset_network_header(skb);
			cb = __mctp_cb(skb);
			cb->halen = PHYSICAL_ADDR_SIZE;
			memcpy(cb->haddr, ihdr->source, PHYSICAL_ADDR_SIZE);
			len = skb->len;
			net_status = netif_rx(skb);
			if (net_status == NET_RX_SUCCESS) {
				skb = NULL;
				stats->rx_packets++;
				stats->rx_bytes += len;
			} else {
				stats->rx_dropped++;
			}

			skb = skb2;
		}

		if (skb)
			kfree_skb(skb);
		if (ihdr)
			ihdr = NULL;
		spin_lock_irqsave(&mbus->rx_queue.lock, flags);
	}
	spin_unlock_irqrestore(&mbus->rx_queue.lock, flags);
}

static void mctp_usb_bus_free(struct mctp_usb_bus *mbus)
	__must_hold(&mctp_usb_bus_mutex)
{
	struct mctp_usb_device *mdev = NULL, *tmp = NULL;

	if (mbus->tx_thread) {
		kthread_stop(mbus->tx_thread);
		mbus->tx_thread = NULL;
	}

	/* Remove any child devices */
	list_for_each_entry_safe(mdev, tmp, &mbus->devices, list) {
		mctp_usb_remove_device(mdev);
	}

	wake_up(&mbus->tx_wq);
	skb_queue_purge(&mbus->tx_queue);
	skb_queue_purge(&mbus->rx_queue);
	if (mbus->netdev)
		mctp_unregister_netdev(mbus->netdev);
	list_del(&mbus->list);
}

static int mctp_usb_rx(struct mctp_usb_device *mdev, gfp_t gfp);

/**
 * mctp_usb_bulk_in_complete - Completion handler for bulk IN URBs
 * @urb: Completed URB
 */
static void mctp_usb_bulk_in_complete(struct urb *urb)
{
	struct bulk_info *info = urb->context;
	struct sk_buff *skb = info->skb;
	struct mctp_usb_device *mdev = info->mdev;
	struct net_device *netdev = skb->dev;
	struct mctp_usb_bus *mbus = netdev_priv(netdev);
	unsigned long flags;
	unsigned int len;

	int status = urb->status;

	switch (status) {
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
	case -EPROTO:
		kfree_skb(skb);
		kfree(info);
		return;
	case 0:
		break;
	default:
		pr_debug(
			"MCTP-USB: Bulk IN transfer failed (status=%d, dev=%s)\n",
			status, netdev ? netdev->name : "NULL");
		kfree_skb(skb);
		/*retry*/
		mctp_usb_rx(mdev, GFP_ATOMIC);
		kfree(info);
		return;
	}

	len = urb->actual_length;
	pr_debug("MCTP-USB: Bulk IN transfer len = %d\n", len);

	if (len) {
		__skb_put(skb, len);

		/* Add to RX queue */
		spin_lock_irqsave(&mbus->rx_queue.lock, flags);
		__skb_queue_tail(&mbus->rx_queue, skb);
		spin_unlock_irqrestore(&mbus->rx_queue.lock, flags);

		schedule_work(&mbus->rx_work);
		urb->actual_length = 0;

		/* Restart RX */
		mctp_usb_rx(mdev, GFP_ATOMIC);
	} else {
		if (skb)
			consume_skb(skb);

		mctp_usb_interrupt_start(mdev, GFP_ATOMIC);
	}
	kfree(info);
}

/**
 * mctp_usb_rx - Initiate USB bulk IN transfer
 * @mdev: MCTP USB device
 * @gfp: Allocation flags
 *
 * Submits URB for receiving data from device
 */
static int mctp_usb_rx(struct mctp_usb_device *mdev, gfp_t gfp)
{
	struct sk_buff *skb = NULL;
	int rc;
	struct mctp_usb_internal_hdr *ihdr;
	struct bulk_info *info;
	const size_t header_len = sizeof(struct mctp_usb_internal_hdr);

	if (!mdev) {
		pr_err("MCTP-USB: mctp_usb_rx Invalid NULL parameter mdev\n");
		return -ENODEV;
	}

	/* Allocate skb for receive */
	skb = __netdev_alloc_skb(mdev->bus->netdev,
				 MCTP_USB_XFER_SIZE + header_len, gfp);

	if (!skb) {
		rc = -ENOMEM;
		pr_err("MCTP-USB: Failed to allocate SKB\n");
		return rc;
	}

	skb->protocol = htons(ETH_P_MCTP);
	skb_reset_mac_header(skb);
	ihdr = skb_put(skb, header_len);
	if (!ihdr) {
		pr_err("MCTP-USB: Invalid header pointer (skb=%px, expected_len=%zu)\n",
		       skb, header_len);
		kfree_skb(skb);
		return -ENOMEM;
	}
	/* Set source/destination addresses */
	ihdr->source[1] = mdev->dev_addr & 0x7f;
	ihdr->source[0] = mdev->bulk_in_ep & 0x0f;
	ihdr->dest[1] = 0xff;
	ihdr->dest[0] = mdev->bus->busnum;
	info = kzalloc(sizeof(*info), GFP_ATOMIC);
	info->skb = skb;
	info->mdev = mdev;

	usb_fill_bulk_urb(mdev->bulk_in_urb, mdev->udev,
			  usb_rcvbulkpipe(mdev->udev, mdev->bulk_in_ep),
			  skb->data + header_len, MCTP_USB_XFER_SIZE,
			  mctp_usb_bulk_in_complete, info);

	rc = usb_submit_urb(mdev->bulk_in_urb, gfp);
	if (rc) {
		netdev_dbg(mdev->bus->netdev,
			   "can't submit bulk in urb, usb-%d-%s, status %d\n",
			   mdev->udev->bus->busnum, dev_name(&mdev->udev->dev),
			   rc);
		kfree_skb(skb);
		kfree(info);
		mctp_usb_interrupt_start(mdev, GFP_ATOMIC);
	}

	return rc;
}

static void mctp_usb_int_in_complete(struct urb *urb)
{
	struct mctp_usb_device *mdev = urb->context;
	int ret;

	switch (urb->status) {
	case 0: /* success */
		break;
	case -ECONNRESET: /* unlink */
	case -ENOENT:
	case -ESHUTDOWN:
	case -EPROTO:
		return;
	default: /* error */
		goto resubmit;
	}

	if (*(int *)mdev->int_buf != 0) {
		mctp_usb_rx(mdev, GFP_ATOMIC);
		return;
	}

resubmit:
	netdev_dbg(mdev->bus->netdev, "resubmit intr\n");
	ret = usb_submit_urb(urb, GFP_ATOMIC);
	if (ret)
		netdev_dbg(mdev->bus->netdev,
			   "can't resubmit intr, usb-%d-%s, status %d\n",
			   mdev->udev->bus->busnum, dev_name(&mdev->udev->dev),
			   ret);
}

/**
 * mctp_usb_interrupt_start - Start interrupt endpoint processing
 * @mdev: MCTP USB device
 * @gfp: Allocation flags
 *
 * Submits interrupt URB for device notification
 */
static int mctp_usb_interrupt_start(struct mctp_usb_device *mdev, gfp_t gfp)
{
	int rc;

	if (!mdev) {
		pr_err("MCTP-USB: mctp_usb_interrupt_start Invalid NULL parameter mdev\n");
		return -EINVAL;
	}
	rc = usb_submit_urb(mdev->int_in_urb, gfp);
	if (rc) {
		pr_err("%s: submit interrupt urb failed, ret = %d\n", __func__,
		       rc);
		netdev_dbg(mdev->bus->netdev,
			   "int in urb submit failure: usb-%d-%s, status %d\n",
			   mdev->udev->bus->busnum, dev_name(&mdev->udev->dev),
			   rc);
		if (rc == -ENOMEM)
			goto retry;
	}
	return rc;
retry:
	schedule_delayed_work(&mdev->rx_retry, RX_RETRY_DELAY);
	return rc;
}

/**
 * mctp_usb_add_bus - Create MCTP USB bus
 * @udev: USB device
 * @busnum: USB bus number
 * @out_mbus: Output pointer for created bus
 *
 * Creates network device and associated structures for a bus
 */
static int mctp_usb_add_bus(struct usb_device *udev, int busnum,
			    struct mctp_usb_bus **out_mbus)
{
	struct device *bus_controller = udev->bus->controller;
	struct mctp_usb_bus *mbus;
	struct net_device *ndev;
	u8 addr[PHYSICAL_ADDR_SIZE];
	char namebuf[30];
	int rc;

	snprintf(namebuf, sizeof(namebuf), "mctpusb%d", busnum);
	ndev = alloc_netdev(sizeof(*mbus), namebuf, NET_NAME_ENUM,
			    mctp_usb_net_setup);
	if (!ndev) {
		dev_err(bus_controller, "alloc netdev failed\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, bus_controller);
	addr[1] = 0xff;
	addr[0] = busnum;
	dev_addr_set(ndev, addr);
	mbus = netdev_priv(ndev);
	mbus->busnum = busnum;
	mbus->netdev = ndev;
	INIT_LIST_HEAD(&mbus->devices);

	spin_lock_init(&mbus->lock);
	init_waitqueue_head(&mbus->tx_wq);
	skb_queue_head_init(&mbus->tx_queue);
	mbus->tx_thread =
		kthread_run(mctp_usb_tx_thread, mbus, "%s/tx", ndev->name);
	if (IS_ERR(mbus->tx_thread)) {
		dev_warn(bus_controller, "Error creating thread: %pe\n",
			 mbus->tx_thread);
		rc = PTR_ERR(mbus->tx_thread);
		goto err_free_netdev;
	}
	skb_queue_head_init(&mbus->rx_queue);
	INIT_WORK(&mbus->rx_work, mctp_usb_rx_work);
	/* Register network device */
	rc = mctp_register_netdev(ndev, NULL);
	if (rc) {
		pr_err("%s: mctp_register_netdev err, rc = %d\n", __func__, rc);
		goto err_stop_thread;
	}
	/* Add to global bus list */
	list_add(&mbus->list, &mctp_usb_buses);

	*out_mbus = mbus;
	return 0;

err_stop_thread:
	kthread_stop(mbus->tx_thread);
err_free_netdev:
	mctp_usb_bus_free(mbus);
	free_netdev(ndev);
	return rc;
}

/**
 * mctp_usb_add_device - Add device to MCTP USB bus
 * @intf: USB interface
 * @mbus: MCTP USB bus
 * @out_mdev: Output pointer for created device
 *
 * Initializes device structure and URBs, adds to bus device list
 */
static int mctp_usb_add_device(struct usb_interface *intf,
			       struct mctp_usb_bus *mbus,
			       struct mctp_usb_device **out_mdev)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct mctp_usb_device *mdev;
	struct usb_host_interface *iface = intf->cur_altsetting;
	unsigned long flags;
	u8 int_in_interval;
	int int_ep_size;
	int i, rc = 0;

	mdev = kzalloc(sizeof(*mdev), GFP_KERNEL);
	if (!mdev)
		return -ENOMEM;

	mdev->udev = udev;
	mdev->intf = intf;
	mdev->bus = mbus;
	mdev->dev_addr = udev->devnum;
	mutex_init(&mdev->lock);

	/* Find endpoints */
	for (i = 0; i < iface->desc.bNumEndpoints; i++) {
		struct usb_endpoint_descriptor *ep = &iface->endpoint[i].desc;
		if (usb_endpoint_is_bulk_in(ep))
			mdev->bulk_in_ep = ep->bEndpointAddress;
		else if (usb_endpoint_is_bulk_out(ep))
			mdev->bulk_out_ep = ep->bEndpointAddress;
		else if (usb_endpoint_is_int_in(ep)) {
			mdev->int_in_ep = ep->bEndpointAddress;
			int_in_interval = ep->bInterval;
			int_ep_size = le16_to_cpu(ep->wMaxPacketSize);
		}
	}

	mdev->int_buf = kzalloc(int_ep_size, GFP_KERNEL);
	mdev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	mdev->bulk_out_urb = usb_alloc_urb(0, GFP_KERNEL);
	mdev->int_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!mdev->int_buf || !mdev->bulk_in_urb || !mdev->bulk_out_urb ||
	    !mdev->int_in_urb) {
		rc = -ENOMEM;
		goto err_free_res;
	}

	usb_fill_int_urb(mdev->int_in_urb, mdev->udev,
			 usb_rcvintpipe(mdev->udev, mdev->int_in_ep),
			 mdev->int_buf, int_ep_size, mctp_usb_int_in_complete,
			 mdev, int_in_interval);

	INIT_DELAYED_WORK(&mdev->rx_retry, rx_retry_work);
	usb_set_intfdata(intf, mdev);
	pr_debug("%s: mdev->dev_addr = %d", __func__, mdev->dev_addr);

	/* Add to bus device list */
	spin_lock_irqsave(&mbus->lock, flags);
	list_add_tail(&mdev->list, &mbus->devices);
	spin_unlock_irqrestore(&mbus->lock, flags);

	*out_mdev = mdev;
	return 0;
err_free_res:
	usb_free_urb(mdev->bulk_in_urb);
	usb_free_urb(mdev->bulk_out_urb);
	usb_free_urb(mdev->int_in_urb);
	kfree(mdev->int_buf);
	kfree(mdev);
	return rc;
}

/**
 * mctp_usb_probe - USB driver probe function
 * @intf: USB interface
 * @id: USB device ID
 *
 * Called when a compatible USB device is detected
 */
static int mctp_usb_probe(struct usb_interface *intf,
			  const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(intf);
	struct mctp_usb_bus *mbus = NULL;
	struct mctp_usb_device *mdev = NULL;
	int busnum = udev->bus->busnum;
	bool exists = false;
	int rc;

	/* Check if bus already exists */
	mutex_lock(&mctp_usb_bus_mutex);
	list_for_each_entry(mbus, &mctp_usb_buses, list) {
		if (mbus->busnum == busnum) {
			exists = true;
			break;
		}
	}

	/* Create new bus if needed */
	if (!exists) {
		pr_debug("new bus detected, busnum %d\n", busnum);
		rc = mctp_usb_add_bus(udev, busnum, &mbus);
		if (rc)
			goto err_unlock;
	}

	mutex_unlock(&mctp_usb_bus_mutex);
	rc = mctp_usb_add_device(intf, mbus, &mdev);
	if (rc)
		goto err_cleanup_bus;

	mctp_usb_interrupt_start(mdev, GFP_KERNEL);

	return 0;

err_cleanup_bus:
	if (!exists) {
		mutex_lock(&mctp_usb_bus_mutex);
		mctp_usb_bus_free(mbus);
		mutex_unlock(&mctp_usb_bus_mutex);
	}
err_unlock:
	mutex_unlock(&mctp_usb_bus_mutex);
	return rc;
}

static void mctp_usb_disconnect(struct usb_interface *intf)
{
	struct mctp_usb_device *mdev = usb_get_intfdata(intf);

	mctp_usb_remove_device(mdev);
}

static const struct usb_device_id mctp_usb_devices[] = {
	{ USB_INTERFACE_INFO(USB_CLASS_MCTP, USB_SUBCLASS_MCTP_MGMT,
			     USB_PROTOCOL_MCTP_1X) },
	{ 0 },
};

MODULE_DEVICE_TABLE(usb, mctp_usb_devices);

static struct usb_driver mctp_usb_driver = {
	.name = "mctp-usb",
	.id_table = mctp_usb_devices,
	.probe = mctp_usb_probe,
	.disconnect = mctp_usb_disconnect,
};

module_usb_driver(mctp_usb_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mia Lu");
MODULE_DESCRIPTION("MCTP USB transport");
