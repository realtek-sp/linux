/* SPDX-License-Identifier: GPL-2.0-or-later */
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

/************************************************************************************
 *	This product is covered by one or more of the following patents:
 *	US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

#ifndef _LINUX_R8168_REALWOW_H
#define _LINUX_R8168_REALWOW_H

#define SIOCDEVPRIVATE_RTLREALWOW SIOCDEVPRIVATE + 3

#define MAX_RealWoW_KCP_SIZE (100)
#define MAX_RealWoW_Payload  (64)

#define KA_TX_PACKET_SIZE      (100)
#define KA_WAKEUP_PATTERN_SIZE (120)

//HwSuppKeepAliveOffloadVer
#define HW_SUPPORT_KCP_OFFLOAD(_M) ((_M)->HwSuppKCPOffloadVer > 0)

enum rtl_realwow_cmd {

	RTL_REALWOW_SET_KCP_DISABLE = 0,
	RTL_REALWOW_SET_KCP_INFO,
	RTL_REALWOW_SET_KCP_CONTENT,

	RTL_REALWOW_SET_KCP_ACKPKTINFO,
	RTL_REALWOW_SET_KCP_WPINFO,
	RTL_REALWOW_SET_KCPDHCP_TIMEOUT,

	RTLT_REALWOW_COMMAND_INVALID
};

struct rtl_realwow_ioctl_struct {
	__u32 cmd;
	__u32 offset;
	__u32 len;
	union {
		__u32 data;
		void *data_buffer;
	};
};

typedef struct _MP_KCPInfo {
	u8 DIPv4[4];
	u8 MacID[6];
	u16 UdpPort[2];
	u8 PKTLEN[2];

	u16 ackLostCnt;
	u8 KCP_WakePattern[MAX_RealWoW_Payload];
	u8 KCP_AckPacket[MAX_RealWoW_Payload];
	u32 KCP_interval;
	u8 KCP_WakePattern_Len;
	u8 KCP_AckPacket_Len;
	u8 KCP_TxPacket[2][KA_TX_PACKET_SIZE];
} MP_KCP_INFO, *PMP_KCP_INFO;

typedef struct _KCPInfo {
	u32 nId; // = id
	u8 DIPv4[4];
	u8 MacID[6];
	u16 UdpPort;
	u16 PKTLEN;
} KCPInfo, *PKCPInfo;

typedef struct _KCPContent {
	u32 id; // = id
	u32 mSec; // = msec
	u32 size; // =size
	u8 bPacket[MAX_RealWoW_KCP_SIZE]; // put packet here
} KCPContent, *PKCPContent;

typedef struct _RealWoWAckPktInfo {
	u16 ackLostCnt;
	u16 patterntSize;
	u8 pattern[MAX_RealWoW_Payload];
} RealWoWAckPktInfo, *PRealWoWAckPktInfo;

typedef struct _RealWoWWPInfo {
	u16 patterntSize;
	u8 pattern[MAX_RealWoW_Payload];
} RealWoWWPInfo, *PRealWoWWPInfo;

int rtl8168_realwow_ioctl(struct net_device *dev, struct ifreq *ifr);
void rtl8168_realwow_hw_init(struct net_device *dev);
void rtl8168_get_realwow_hw_version(struct net_device *dev);
void rtl8168_set_realwow_d3_para(struct net_device *dev);

#endif /* _LINUX_R8168_REALWOW_H */
