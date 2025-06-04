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

#ifndef __DRIVERS_USB_REALTEK_REGS_H__
#define __DRIVERS_USB_REALTEK_REGS_H__

#define USB_CTRL	    0x0000
#define USB_ADDR	    0x0004
#define USB_IRQ_EN	    0x0008
#define USB_IRQ_STATUS	    0x0010
#define USB_FORCE_CMD	    0x0014
#define USB_UTMI_CTRL	    0x0018
#define USB_UTMI_CFG	    0x001C
#define USB_UTMI_STAT	    0x0020
#define USB_PHY_CTRL	    0x0024
#define USB_DPHY_CFG	    0x0028
#define USB_SLBTEST	    0x0030
#define USB_PKERR_CNT	    0x0034
#define USB_RXERR_CNT	    0x0038
#define USB_SIE_STATUS	    0x0040
#define USB_EP_CFG0	    0x0048
#define USB_EP_CTL0	    0x004C
#define USB_EP_CTL1	    0x0050
#define USB_EP_MAXPKT0	    0x0054
#define USB_EP0_SETUP_DATA0 0x0060
#define USB_EP0_SETUP_DATA1 0x0064
#define USB_LPM_CFG0	    0x0068
#define USB_LPM_DUMMY	    0x0070
#define USB_DUMMY0	    0x0080
#define USB_DUMMY1	    0x0084

#define USB_BULKINEPA_CFG	  0x0200
#define USB_BULKINEPA_CTL	  0x0204
#define USB_BULKINEPA_TOGGLE_CTRL 0x020C
#define USB_BULKINEPA_IRQ_EN	  0x0210
#define USB_BULKINEPA_IRQ_STATUS  0x0214

#define USB_BULKINEPB_CFG	  0x0240
#define USB_BULKINEPB_CTL	  0x0244
#define USB_BULKINEPB_TOGGLE_CTRL 0x024C
#define USB_BULKINEPB_IRQ_EN	  0x0250
#define USB_BULKINEPB_IRQ_STATUS  0x0254

#define USB_BULKINEPC_CFG	  0x0280
#define USB_BULKINEPC_CTL	  0x0284
#define USB_BULKINEPC_TOGGLE_CTRL 0x028C
#define USB_BULKINEPC_IRQ_EN	  0x0290
#define USB_BULKINEPC_IRQ_STATUS  0x0294

#define USB_BULKOUTEPA_CFG	   0x0300
#define USB_BULKOUTEPA_CTL	   0x0304
#define USB_BULKOUTEPA_TOGGLE_CTRL 0x030C
#define USB_BULKOUTEPA_IRQ_EN	   0x0310
#define USB_BULKOUTEPA_IRQ_STATUS  0x0314

#define USB_BULKOUTEPB_CFG	   0x0340
#define USB_BULKOUTEPB_CTL	   0x0344
#define USB_BULKOUTEPB_TOGGLE_CTRL 0x034C
#define USB_BULKOUTEPB_IRQ_EN	   0x0350
#define USB_BULKOUTEPB_IRQ_STATUS  0x0354

#define USB_BULKOUTEPC_CFG	   0x0380
#define USB_BULKOUTEPC_CTL	   0x0384
#define USB_BULKOUTEPC_TOGGLE_CTRL 0x038C
#define USB_BULKOUTEPC_IRQ_EN	   0x0390
#define USB_BULKOUTEPC_IRQ_STATUS  0x0394

#define USB_UACINEP_CFG		  0x03C0
#define USB_UACINEP_CTL		  0x03C4
#define USB_UACINEP_IRQ_EN	  0x03C8
#define USB_UACINEP_IRQ_STATUS	  0x03CC
#define USB_UACINEP_PKTSIZE_INDEX 0x03D0
#define USB_UACINEP_PKTSIZE0	  0x03D4
#define USB_UACINEP_PKTSIZE1	  0x03D8
#define USB_UACINEP_PKTSIZE2	  0x03DC
#define USB_UACINEP_PKTSIZE3	  0x03E0
#define USB_UACINEP_PKTSIZE4	  0x03E4
#define USB_UACINEP_PKTSIZE5	  0x03E8
#define USB_UACINEP_PKTSIZE6	  0x03EC
#define USB_UACINEP_PKTSIZE7	  0x03F0
#define USB_UACINEP_PKTSIZE8	  0x03F4
#define USB_UACINEP_PKTSIZE9	  0x03F8
#define USB_UACINEP_CFG1	  0x03FC

#define USB_UACOUTEP_CFG	0x0400
#define USB_UACOUTEP_CTL	0x0404
#define USB_UACOUTEP_IRQ_EN	0x0410
#define USB_UACOUTEP_IRQ_STATUS 0x0414
#define USB_UACOUTEP_CFG1	0x0418

#define USB_UVCINEPA_CFG	 0x0440
#define USB_UVCINEPA_CTL	 0x0444
#define USB_UVCINEPA_TOGGLE_CTRL 0x044C
#define USB_UVCINEPA_IRQ_EN	 0x0450
#define USB_UVCINEPA_IRQ_STATUS	 0x0454
#define USB_UVCINEPA_CFG2	 0x0458
#define USB_UVCINEPA_CFG3	 0x045C
#define USB_UVCINEPA_CFG4	 0x0460
#define USB_UVCINEPA_CFG5	 0x0464
#define USB_UVCINEPA_PHINF0	 0x0468

#define USB_UVCINEPB_CFG	 0x0480
#define USB_UVCINEPB_CTL	 0x0484
#define USB_UVCINEPB_TOGGLE_CTRL 0x048C
#define USB_UVCINEPB_IRQ_EN	 0x0490
#define USB_UVCINEPB_IRQ_STATUS	 0x0494
#define USB_UVCINEPB_CFG2	 0x0498
#define USB_UVCINEPB_CFG3	 0x049C
#define USB_UVCINEPB_CFG4	 0x04A0
#define USB_UVCINEPB_CFG5	 0x04A4
#define USB_UVCINEPB_PHINF0	 0x04A8

#define USB_INTEREPA_CFG	 0x0500
#define USB_INTEREPA_CTL	 0x0504
#define USB_INTEREPA_BC		 0x0508
#define USB_INTEREPA_TOGGLE_CTRL 0x050C
#define USB_INTEREPA_IRQ_EN	 0x0510
#define USB_INTEREPA_IRQ_STATUS	 0x0514
#define USB_INTEREPA_DAT0	 0x0520
#define USB_INTEREPA_DAT1	 0x0524
#define USB_INTEREPA_DAT2	 0x0528
#define USB_INTEREPA_DAT3	 0x052C
#define USB_INTEREPA_DAT4	 0x0530
#define USB_INTEREPA_DAT5	 0x0534
#define USB_INTEREPA_DAT6	 0x0538
#define USB_INTEREPA_DAT7	 0x053C
#define USB_INTEREPA_DAT8	 0x0540
#define USB_INTEREPA_DAT9	 0x0544
#define USB_INTEREPA_DAT10	 0x0548
#define USB_INTEREPA_DAT11	 0x054C
#define USB_INTEREPA_DAT12	 0x0550
#define USB_INTEREPA_DAT13	 0x0554
#define USB_INTEREPA_DAT14	 0x0558
#define USB_INTEREPA_DAT15	 0x055C
#define UBS_INTEREPB_CFG	 0x0580
#define USB_INTEREPB_CTL	 0x0584
#define USB_INTEREPB_BC		 0x0588
#define USB_INTEREPB_TOGGLE_CTRL 0x058C
#define USB_INTEREPB_IRQ_EN	 0x0590
#define USB_INTEREPB_IRQ_STATUS	 0x0594
#define USB_INTEREPB_DAT0	 0x05A0
#define USB_INTEREPB_DAT1	 0x05A4
#define USB_INTEREPB_DAT2	 0x05A8
#define USB_INTEREPB_DAT3	 0x05AC
#define USB_INTEREPB_DAT4	 0x05B0
#define USB_INTEREPB_DAT5	 0x05B4
#define USB_INTEREPB_DAT6	 0x05B8
#define USB_INTEREPB_DAT7	 0x05BC
#define USB_INTEREPB_DAT8	 0x05D0
#define USB_INTEREPB_DAT9	 0x05D4
#define USB_INTEREPB_DAT10	 0x05D8
#define USB_INTEREPB_DAT11	 0x05DC
#define USB_INTEREPB_DAT12	 0x05E0
#define USB_INTEREPB_DAT13	 0x05E4
#define USB_INTEREPB_DAT14	 0x05E8
#define USB_INTEREPB_DAT15	 0x05EC
#define UBS_INTEREPC_CFG	 0x0600
#define USB_INTEREPC_CTL	 0x0604
#define USB_INTEREPC_BC		 0x0608
#define USB_INTEREPC_TOGGLE_CTRL 0x060C
#define USB_INTEREPC_IRQ_EN	 0x0610
#define USB_INTEREPC_IRQ_STATUS	 0x0614
#define USB_INTEREPC_DAT0	 0x0620
#define USB_INTEREPC_DAT1	 0x0624
#define USB_INTEREPC_DAT2	 0x0628
#define USB_INTEREPC_DAT3	 0x062C
#define USB_INTEREPC_DAT4	 0x0630
#define USB_INTEREPC_DAT5	 0x0634
#define USB_INTEREPC_DAT6	 0x0638
#define USB_INTEREPC_DAT7	 0x063C
#define USB_INTEREPC_DAT8	 0x0640
#define USB_INTEREPC_DAT9	 0x0644
#define USB_INTEREPC_DAT10	 0x0648
#define USB_INTEREPC_DAT11	 0x064C
#define USB_INTEREPC_DAT12	 0x0650
#define USB_INTEREPC_DAT13	 0x0654
#define USB_INTEREPC_DAT14	 0x0658
#define USB_INTEREPC_DAT15	 0x065C
#define UBS_INTEREPD_CFG	 0x0680
#define USB_INTEREPD_CTL	 0x0684
#define USB_INTEREPD_BC		 0x0688
#define USB_INTEREPD_TOGGLE_CTRL 0x068C
#define USB_INTEREPD_IRQ_EN	 0x0690
#define USB_INTEREPD_IRQ_STATUS	 0x0694
#define USB_INTEREPD_DAT0	 0x06A0
#define USB_INTEREPD_DAT1	 0x06A4
#define USB_INTEREPD_DAT2	 0x06A8
#define USB_INTEREPD_DAT3	 0x06AC
#define USB_INTEREPD_DAT4	 0x06B0
#define USB_INTEREPD_DAT5	 0x06B4
#define USB_INTEREPD_DAT6	 0x06B8
#define USB_INTEREPD_DAT7	 0x06BC
#define USB_INTEREPD_DAT8	 0x06C0
#define USB_INTEREPD_DAT9	 0x06C4
#define USB_INTEREPD_DAT10	 0x06C8
#define USB_INTEREPD_DAT11	 0x06CC
#define USB_INTEREPD_DAT12	 0x06D0
#define USB_INTEREPD_DAT13	 0x06D4
#define USB_INTEREPD_DAT14	 0x06D8
#define USB_INTEREPD_DAT15	 0x06DC
#define UBS_INTEREPE_CFG	 0x0700
#define USB_INTEREPE_CTL	 0x0704
#define USB_INTEREPE_BC		 0x0708
#define USB_INTEREPE_TOGGLE_CTRL 0x070C
#define USB_INTEREPE_IRQ_EN	 0x0710
#define USB_INTEREPE_IRQ_STATUS	 0x0714
#define USB_INTEREPE_DAT0	 0x0720
#define USB_INTEREPE_DAT1	 0x0724
#define USB_INTEREPE_DAT2	 0x0728
#define USB_INTEREPE_DAT3	 0x072C
#define USB_INTEREPE_DAT4	 0x0730
#define USB_INTEREPE_DAT5	 0x0734
#define USB_INTEREPE_DAT6	 0x0738
#define USB_INTEREPE_DAT7	 0x073C
#define USB_INTEREPE_DAT8	 0x0740
#define USB_INTEREPE_DAT9	 0x0744
#define USB_INTEREPE_DAT10	 0x0748
#define USB_INTEREPE_DAT11	 0x074C
#define USB_INTEREPE_DAT12	 0x0750
#define USB_INTEREPE_DAT13	 0x0754
#define USB_INTEREPE_DAT14	 0x0758
#define USB_INTEREPE_DAT15	 0x075C
#define UBS_INTEREPF_CFG	 0x0780
#define USB_INTEREPF_CTL	 0x0784
#define USB_INTEREPF_BC		 0x0788
#define USB_INTEREPF_TOGGLE_CTRL 0x078C
#define USB_INTEREPF_IRQ_EN	 0x0790
#define USB_INTEREPF_IRQ_STATUS	 0x0794
#define USB_INTEREPF_DAT0	 0x07A0
#define USB_INTEREPF_DAT1	 0x07A4
#define USB_INTEREPF_DAT2	 0x07A8
#define USB_INTEREPF_DAT3	 0x07AC
#define USB_INTEREPF_DAT4	 0x07B0
#define USB_INTEREPF_DAT5	 0x07B4
#define USB_INTEREPF_DAT6	 0x07B8
#define USB_INTEREPF_DAT7	 0x07BC
#define USB_INTEREPF_DAT8	 0x07C0
#define USB_INTEREPF_DAT9	 0x07C4
#define USB_INTEREPF_DAT10	 0x07C8
#define USB_INTEREPF_DAT11	 0x07CC
#define USB_INTEREPF_DAT12	 0x07D0
#define USB_INTEREPF_DAT13	 0x07D4
#define USB_INTEREPF_DAT14	 0x07D8
#define USB_INTEREPF_DAT15	 0x07DC
#define USB_DPHYCFG		 0x0800
#define USB_DPHYCFG1		 0x0804
#define USB_DPHYCFG1		 0x0804
#define USB_DPHYCFG2		 0x0808
#define USB_DPHY_STS		 0x080C

/* USB_CTRL 0x0000 */
#define CONNECT_EN_OFFSET		   0
#define CONNECT_EN_BITS			   1
#define CONNECT_EN_MASK			   (((1 << 1) - 1) << 0)
#define WAKEUP_EN_OFFSET		   1
#define WAKEUP_EN_BITS			   1
#define WAKEUP_EN_MASK			   (((1 << 1) - 1) << 1)
#define SUSPND_EN_OFFSET		   2
#define SUSPND_EN_BITS			   1
#define SUSPND_EN_MASK			   (((1 << 1) - 1) << 2)
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_OFFSET 3
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_BITS   1
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_MASK   (((1 << 1) - 1) << 3)
#define MODE_HS_OFFSET			   4
#define MODE_HS_BITS			   1
#define MODE_HS_MASK			   (((1 << 1) - 1) << 4)
#define CFG_FORCE_FW_SUSPEND_OFFSET	   5
#define CFG_FORCE_FW_SUSPEND_BITS	   1
#define CFG_FORCE_FW_SUSPEND_MASK	   (((1 << 1) - 1) << 5)
#define CFG_FORCE_FW_REMOTE_WAKEUP_OFFSET  6
#define CFG_FORCE_FW_REMOTE_WAKEUP_BITS	   1
#define CFG_FORCE_FW_REMOTE_WAKEUP_MASK	   (((1 << 1) - 1) << 6)
#define CFG_SOF_INTERVAL_OFFSET		   8
#define CFG_SOF_INTERVAL_BITS		   8
#define CFG_SOF_INTERVAL_MASK		   (((1 << 8) - 1) << 8)
#define SOF_CNT_EN_OFFSET		   16
#define SOF_CNT_EN_BITS			   1
#define SOF_CNT_EN_MASK			   (((1 << 1) - 1) << 16)
/* USB_ADDR 0x0004 */
#define DEVADDR_SET_OFFSET		   0
#define DEVADDR_SET_BITS		   7
#define DEVADDR_SET_MASK		   (((1 << 7) - 1) << 0)
#define FORCE_DEVADDR_OFFSET		   7
#define FORCE_DEVADDR_BITS		   1
#define FORCE_DEVADDR_MASK		   (((1 << 1) - 1) << 7)
#define DEVADDR_OFFSET			   8
#define DEVADDR_BITS			   7
#define DEVADDR_MASK			   (((1 << 7) - 1) << 8)
/* USB_IRQ_EN 0x0008 */
#define IE_LS_OFFSET			   0
#define IE_LS_BITS			   1
#define IE_LS_MASK			   (((1 << 1) - 1) << 0)
#define IE_SOF_OFFSET			   1
#define IE_SOF_BITS			   1
#define IE_SOF_MASK			   (((1 << 1) - 1) << 1)
#define IE_SUSPND_OFFSET		   2
#define IE_SUSPND_BITS			   1
#define IE_SUSPND_MASK			   (((1 << 1) - 1) << 2)
#define IE_RESUME_OFFSET		   3
#define IE_RESUME_BITS			   1
#define IE_RESUME_MASK			   (((1 << 1) - 1) << 3)
#define IE_SE0RST_OFFSET		   4
#define IE_SE0RST_BITS			   1
#define IE_SE0RST_MASK			   (((1 << 1) - 1) << 4)
#define IE_L1SLEEP_OFFSET		   5
#define IE_L1SLEEP_BITS			   1
#define IE_L1SLEEP_MASK			   (((1 << 1) - 1) << 5)
#define IE_L1RESUME_OFFSET		   6
#define IE_L1RESUME_BITS		   1
#define IE_L1RESUME_MASK		   (((1 << 1) - 1) << 6)
#define IE_EP0INT_OFFSET		   7
#define IE_EP0INT_BITS			   1
#define IE_EP0INT_MASK			   (((1 << 1) - 1) << 7)
#define IE_EP0OUTT_OFFSET		   8
#define IE_EP0OUTT_BITS			   1
#define IE_EP0OUTT_MASK			   (((1 << 1) - 1) << 8)
#define IE_EP0IN_OFFSET			   9
#define IE_EP0IN_BITS			   1
#define IE_EP0IN_MASK			   (((1 << 1) - 1) << 9)
#define IE_EP0OUT_OFFSET		   10
#define IE_EP0OUT_BITS			   1
#define IE_EP0OUT_MASK			   (((1 << 1) - 1) << 10)
#define IE_EP0OSHT_OFFSET		   11
#define IE_EP0OSHT_BITS			   1
#define IE_EP0OSHT_MASK			   (((1 << 1) - 1) << 11)
#define IE_EP0CSEND_OFFSET		   12
#define IE_EP0CSEND_BITS		   1
#define IE_EP0CSEND_MASK		   (((1 << 1) - 1) << 12)
#define IE_SETUP_OFFSET			   13
#define IE_SETUP_BITS			   1
#define IE_SETUP_MASK			   (((1 << 1) - 1) << 13)
#define IE_EP0CS_OFFSET			   14
#define IE_EP0CS_BITS			   1
#define IE_EP0CS_MASK			   (((1 << 1) - 1) << 14)
#define IE_SOF_INTERVAL_OFFSET		   15
#define IE_SOF_INTERVAL_BITS		   1
#define IE_SOF_INTERVAL_MASK		   (((1 << 1) - 1) << 15)
/* USB_IRQ_STATUS 0x0010 */
#define I_LSF_OFFSET			   0
#define I_LSF_BITS			   1
#define I_LSF_MASK			   (((1 << 1) - 1) << 0)
#define I_SOFF_OFFSET			   1
#define I_SOFF_BITS			   1
#define I_SOFF_MASK			   (((1 << 1) - 1) << 1)
#define I_SUSPNDF_OFFSET		   2
#define I_SUSPNDF_BITS			   1
#define I_SUSPNDF_MASK			   (((1 << 1) - 1) << 2)
#define I_RESUMEF_OFFSET		   3
#define I_RESUMEF_BITS			   1
#define I_RESUMEF_MASK			   (((1 << 1) - 1) << 3)
#define I_SE0RSTF_OFFSET		   4
#define I_SE0RSTF_BITS			   1
#define I_SE0RSTF_MASK			   (((1 << 1) - 1) << 4)
#define I_L1SLEEPF_OFFSET		   5
#define I_L1SLEEPF_BITS			   1
#define I_L1SLEEPF_MASK			   (((1 << 1) - 1) << 5)
#define I_L1RESUMEF_OFFSET		   6
#define I_L1RESUMEF_BITS		   1
#define I_L1RESUMEF_MASK		   (((1 << 1) - 1) << 6)
#define I_EP0INTF_OFFSET		   7
#define I_EP0INTF_BITS			   1
#define I_EP0INTF_MASK			   (((1 << 1) - 1) << 7)
#define I_EP0OUTTF_OFFSET		   8
#define I_EP0OUTTF_BITS			   1
#define I_EP0OUTTF_MASK			   (((1 << 1) - 1) << 8)
#define I_EP0INF_OFFSET			   9
#define I_EP0INF_BITS			   1
#define I_EP0INF_MASK			   (((1 << 1) - 1) << 9)
#define I_EP0OUTF_OFFSET		   10
#define I_EP0OUTF_BITS			   1
#define I_EP0OUTF_MASK			   (((1 << 1) - 1) << 10)
#define I_EP0OSHTF_OFFSET		   11
#define I_EP0OSHTF_BITS			   1
#define I_EP0OSHTF_MASK			   (((1 << 1) - 1) << 11)
#define I_EP0CSENDF_OFFSET		   12
#define I_EP0CSENDF_BITS		   1
#define I_EP0CSENDF_MASK		   (((1 << 1) - 1) << 12)
#define I_SETUPF_OFFSET			   13
#define I_SETUPF_BITS			   1
#define I_SETUPF_MASK			   (((1 << 1) - 1) << 13)
#define I_EP0CSF_OFFSET			   14
#define I_EP0CSF_BITS			   1
#define I_EP0CSF_MASK			   (((1 << 1) - 1) << 14)
#define I_SOF_INTERVAL_OFFSET		   15
#define I_SOF_INTERVAL_BITS		   1
#define I_SOF_INTERVAL_MASK		   (((1 << 1) - 1) << 15)
/* USB_FORCE_CMD 0x0014 */
#define FORCE_UTMI_RST_OFFSET		   0
#define FORCE_UTMI_RST_BITS		   1
#define FORCE_UTMI_RST_MASK		   (((1 << 1) - 1) << 0)
#define FORCE_PCE_RST_OFFSET		   1
#define FORCE_PCE_RST_BITS		   1
#define FORCE_PCE_RST_MASK		   (((1 << 1) - 1) << 1)
#define FORCE_PA_RST_OFFSET		   2
#define FORCE_PA_RST_BITS		   1
#define FORCE_PA_RST_MASK		   (((1 << 1) - 1) << 2)
#define FORCE_PCE_CMD_OFFSET		   3
#define FORCE_PCE_CMD_BITS		   1
#define FORCE_PCE_CMD_MASK		   (((1 << 1) - 1) << 3)
/* USB_UTMI_CTRL 0x0018 */
#define FORCE_DBSN_OFFSET		   0
#define FORCE_DBSN_BITS			   1
#define FORCE_DBSN_MASK			   (((1 << 1) - 1) << 0)
#define FORCE_NORM_OFFSET		   1
#define FORCE_NORM_BITS			   1
#define FORCE_NORM_MASK			   (((1 << 1) - 1) << 1)
#define FORCE_FSTERM_OFFSET		   2
#define FORCE_FSTERM_BITS		   1
#define FORCE_FSTERM_MASK		   (((1 << 1) - 1) << 2)
#define FORCE_HSTERM_OFFSET		   3
#define FORCE_HSTERM_BITS		   1
#define FORCE_HSTERM_MASK		   (((1 << 1) - 1) << 3)
#define FORCE_FSXCVR_OFFSET		   4
#define FORCE_FSXCVR_BITS		   1
#define FORCE_FSXCVR_MASK		   (((1 << 1) - 1) << 4)
#define FORCE_HSXCVR_OFFSET		   5
#define FORCE_HSXCVR_BITS		   1
#define FORCE_HSXCVR_MASK		   (((1 << 1) - 1) << 5)
#define FORCE_FS_OFFSET			   6
#define FORCE_FS_BITS			   1
#define FORCE_FS_MASK			   (((1 << 1) - 1) << 6)
#define FORCE_HS_OFFSET			   7
#define FORCE_HS_BITS			   1
#define FORCE_HS_MASK			   (((1 << 1) - 1) << 7)
/* USB_UTMI_CFG 0x001C */
#define CFG_FAST_CHIRP_K_OFFSET		   0
#define CFG_FAST_CHIRP_K_BITS		   1
#define CFG_FAST_CHIRP_K_MASK		   (((1 << 1) - 1) << 0)
#define CFG_CHIRP_K_LENGTH_OFFSET	   1
#define CFG_CHIRP_K_LENGTH_BITS		   2
#define CFG_CHIRP_K_LENGTH_MASK		   (((1 << 2) - 1) << 1)
/* USB_UTMI_STAT 0x0020 */
#define UTMI_OPMODE_OFFSET		   0
#define UTMI_OPMODE_BITS		   2
#define UTMI_OPMODE_MASK		   (((1 << 2) - 1) << 0)
#define UTMI_TERMSEL_OFFSET		   2
#define UTMI_TERMSEL_BITS		   1
#define UTMI_TERMSEL_MASK		   (((1 << 1) - 1) << 2)
#define UTMI_XCVRSEL_OFFSET		   3
#define UTMI_XCVRSEL_BITS		   1
#define UTMI_XCVRSEL_MASK		   (((1 << 1) - 1) << 3)
#define UTMI_LINESTATE_OFFSET		   4
#define UTMI_LINESTATE_BITS		   2
#define UTMI_LINESTATE_MASK		   (((1 << 2) - 1) << 4)
/* USB_PHY_CTRL 0x0024 */
#define SOF_EN_OFFSET			   0
#define SOF_EN_BITS			   1
#define SOF_EN_MASK			   (((1 << 1) - 1) << 0)
#define FW_SEL_SERCV_OFFSET		   1
#define FW_SEL_SERCV_BITS		   1
#define FW_SEL_SERCV_MASK		   (((1 << 1) - 1) << 1)
#define RPU_FW_EN_OFFSET		   2
#define RPU_FW_EN_BITS			   1
#define RPU_FW_EN_MASK			   (((1 << 1) - 1) << 2)
#define SERCV_FW_EN_OFFSET		   3
#define SERCV_FW_EN_BITS		   1
#define SERCV_FW_EN_MASK		   (((1 << 1) - 1) << 3)
#define RPU_HIGH_OFFSET			   4
#define RPU_HIGH_BITS			   2
#define RPU_HIGH_MASK			   (((1 << 2) - 1) << 4)
#define RPU_LOW_OFFSET			   6
#define RPU_LOW_BITS			   2
#define RPU_LOW_MASK			   (((1 << 2) - 1) << 6)
/* USB_DPHY_CFG 0x0028 */
#define CLK60_NEGALIGN_OFFSET		   0
#define CLK60_NEGALIGN_BITS		   1
#define CLK60_NEGALIGN_MASK		   (((1 << 1) - 1) << 0)
#define LATE_DLLEN_OFFSET		   1
#define LATE_DLLEN_BITS			   1
#define LATE_DLLEN_MASK			   (((1 << 1) - 1) << 1)
#define FS_XCVR_POW_SAV_OFFSET		   2
#define FS_XCVR_POW_SAV_BITS		   1
#define FS_XCVR_POW_SAV_MASK		   (((1 << 1) - 1) << 2)
#define HS_ANA_TX_PWD_EN_OFFSET		   3
#define HS_ANA_TX_PWD_EN_BITS		   1
#define HS_ANA_TX_PWD_EN_MASK		   (((1 << 1) - 1) << 3)
#define HS_XMT_PWD_EN_OFFSET		   4
#define HS_XMT_PWD_EN_BITS		   1
#define HS_XMT_PWD_EN_MASK		   (((1 << 1) - 1) << 4)
#define CFG_RXACT_EARLY_OFFSET		   5
#define CFG_RXACT_EARLY_BITS		   1
#define CFG_RXACT_EARLY_MASK		   (((1 << 1) - 1) << 5)
#define FS_PHASE_SEL_OFFSET		   8
#define FS_PHASE_SEL_BITS		   4
#define FS_PHASE_SEL_MASK		   (((1 << 4) - 1) << 8)
#define CFG_EB_DEPTH_OFFSET		   12
#define CFG_EB_DEPTH_BITS		   4
#define CFG_EB_DEPTH_MASK		   (((1 << 4) - 1) << 12)
#define USB2_PHY_DEBUG_ADDR_OFFSET	   16
#define USB2_PHY_DEBUG_ADDR_BITS	   3
#define USB2_PHY_DEBUG_ADDR_MASK	   (((1 << 3) - 1) << 16)
/* USB_SLBTEST 0x0030 */
#define SLB_SEED_OFFSET			   0
#define SLB_SEED_BITS			   8
#define SLB_SEED_MASK			   (((1 << 8) - 1) << 0)
#define SLB_PSL_OFFSET			   8
#define SLB_PSL_BITS			   2
#define SLB_PSL_MASK			   (((1 << 2) - 1) << 8)
#define SLB_DONE_OFFSET			   10
#define SLB_DONE_BITS			   1
#define SLB_DONE_MASK			   (((1 << 1) - 1) << 10)
#define SLB_FAIL_OFFSET			   11
#define SLB_FAIL_BITS			   1
#define SLB_FAIL_MASK			   (((1 << 1) - 1) << 11)
#define SLB_RST_OFFSET			   12
#define SLB_RST_BITS			   1
#define SLB_RST_MASK			   (((1 << 1) - 1) << 12)
#define SLB_EN_OFFSET			   13
#define SLB_EN_BITS			   1
#define SLB_EN_MASK			   (((1 << 1) - 1) << 13)
#define USBTMOD_OFFSET			   16
#define USBTMOD_BITS			   3
#define USBTMOD_MASK			   (((1 << 3) - 1) << 16)
/* USB_PKERR_CNT 0x0034 */
#define PKTERR_CNT1_OFFSET		   0
#define PKTERR_CNT1_BITS		   8
#define PKTERR_CNT1_MASK		   (((1 << 8) - 1) << 0)
#define PKTERR_CNT2_OFFSET		   8
#define PKTERR_CNT2_BITS		   8
#define PKTERR_CNT2_MASK		   (((1 << 8) - 1) << 8)
#define PKTERR_CLR_OFFSET		   16
#define PKTERR_CLR_BITS			   1
#define PKTERR_CLR_MASK			   (((1 << 1) - 1) << 16)
/* USB_RXERR_CNT 0x0038 */
#define RXERR_CNT1_OFFSET		   0
#define RXERR_CNT1_BITS			   8
#define RXERR_CNT1_MASK			   (((1 << 8) - 1) << 0)
#define RXERR_CNT2_OFFSET		   8
#define RXERR_CNT2_BITS			   8
#define RXERR_CNT2_MASK			   (((1 << 8) - 1) << 8)
#define RXERR_CLR_OFFSET		   16
#define RXERR_CLR_BITS			   1
#define RXERR_CLR_MASK			   (((1 << 1) - 1) << 16)
/* USB_SIE_STATUS 0x0040 */
// #define MODE_HS_OFFSET 0
// #define MODE_HS_BITS 1
// #define MODE_HS_MASK (((1 << 1) - 1) << 0)
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_OFFSET 1
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_BITS 1
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_MASK (((1 << 1) - 1) << 1)
// #define FORCE_FS_OFFSET 2
// #define FORCE_FS_BITS 1
// #define FORCE_FS_MASK (((1 << 1) - 1) << 2)
/* USB_EP_CFG0 0x0048 */
#define EP0_NAKOUT_MODE_OFFSET		   0
#define EP0_NAKOUT_MODE_BITS		   1
#define EP0_NAKOUT_MODE_MASK		   (((1 << 1) - 1) << 0)
#define CFG_LFOE_LFVO_GLITCH_OFFSET	   6
#define CFG_LFOE_LFVO_GLITCH_BITS	   1
#define CFG_LFOE_LFVO_GLITCH_MASK	   (((1 << 1) - 1) << 6)
#define CFG_LFSE0_LFVO_GLITCH_OFFSET	   7
#define CFG_LFSE0_LFVO_GLITCH_BITS	   1
#define CFG_LFSE0_LFVO_GLITCH_MASK	   (((1 << 1) - 1) << 7)
/* USB_EP_CTL0 0x004C */
#define EP0_DIR_OFFSET			   0
#define EP0_DIR_BITS			   1
#define EP0_DIR_MASK			   (((1 << 1) - 1) << 0)
#define EP0_STALL_OFFSET		   1
#define EP0_STALL_BITS			   1
#define EP0_STALL_MASK			   (((1 << 1) - 1) << 1)
#define EP0_RESET_OFFSET		   2
#define EP0_RESET_BITS			   1
#define EP0_RESET_MASK			   (((1 << 1) - 1) << 2)
#define EP0_CONTROL_STAGE_OFFSET	   3
#define EP0_CONTROL_STAGE_BITS		   2
#define EP0_CONTROL_STAGE_MASK		   (((1 << 2) - 1) << 3)
/* USB_EP_CTL1 0x0050 */
#define EP0_CSH_OFFSET			   0
#define EP0_CSH_BITS			   1
#define EP0_CSH_MASK			   (((1 << 1) - 1) << 0)
/* USB_EP_MAXPKT0 0x0054 */
#define EP0_MAXPKT_OFFSET		   0
#define EP0_MAXPKT_BITS			   7
#define EP0_MAXPKT_MASK			   (((1 << 7) - 1) << 0)
/* USB_EP0_SETUP_DATA0 0x0060 */
/* USB_EP0_SETUP_DATA1 0x0064 */
/* USB_LPM_CFG0 0x0068 */
#define LPM_HIRD_SAVE_OFFSET		   0
#define LPM_HIRD_SAVE_BITS		   4
#define LPM_HIRD_SAVE_MASK		   (((1 << 4) - 1) << 0)
#define CFG_LPM_READY_OFFSET		   4
#define CFG_LPM_READY_BITS		   1
#define CFG_LPM_READY_MASK		   (((1 << 1) - 1) << 4)
#define U_LPM_AUTO_OFFSET		   5
#define U_LPM_AUTO_BITS			   1
#define U_LPM_AUTO_MASK			   (((1 << 1) - 1) << 5)
#define CFG_LPM_SUPPORT_OFFSET		   6
#define CFG_LPM_SUPPORT_BITS		   1
#define CFG_LPM_SUPPORT_MASK		   (((1 << 1) - 1) << 6)
#define LPM_AUTO_EN_OFFSET		   7
#define LPM_AUTO_EN_BITS		   1
#define LPM_AUTO_EN_MASK		   (((1 << 1) - 1) << 7)
#define U_FORCE_LPM_L1_RESUME_OFFSET	   8
#define U_FORCE_LPM_L1_RESUME_BITS	   1
#define U_FORCE_LPM_L1_RESUME_MASK	   (((1 << 1) - 1) << 8)
#define U_FRC_EP0_BUSY_REJECT_LPM_OFFSET   9
#define U_FRC_EP0_BUSY_REJECT_LPM_BITS	   1
#define U_FRC_EP0_BUSY_REJECT_LPM_MASK	   (((1 << 1) - 1) << 9)
#define U_HOST_REMOTEWAKEUP_VALID_OFFSET   13
#define U_HOST_REMOTEWAKEUP_VALID_BITS	   1
#define U_HOST_REMOTEWAKEUP_VALID_MASK	   (((1 << 1) - 1) << 13)
/* USB_LPM_DUMMY 0x0070 */
#define U_LPM_DMY0_OFFSET		   0
#define U_LPM_DMY0_BITS			   8
#define U_LPM_DMY0_MASK			   (((1 << 8) - 1) << 0)
#define U_LPM_DMY1_OFFSET		   8
#define U_LPM_DMY1_BITS			   8
#define U_LPM_DMY1_MASK			   (((1 << 8) - 1) << 8)
#define U_LPM_DMY2_OFFSET		   16
#define U_LPM_DMY2_BITS			   8
#define U_LPM_DMY2_MASK			   (((1 << 8) - 1) << 16)
#define U_LPM_DMY3_OFFSET		   24
#define U_LPM_DMY3_BITS			   8
#define U_LPM_DMY3_MASK			   (((1 << 8) - 1) << 24)
/* USB_DUMMY0 0x0080 */
#define SIE_DUMMY0_OFFSET		   0
#define SIE_DUMMY0_BITS			   8
#define SIE_DUMMY0_MASK			   (((1 << 8) - 1) << 0)
#define SIE_DUMMY1_OFFSET		   8
#define SIE_DUMMY1_BITS			   8
#define SIE_DUMMY1_MASK			   (((1 << 8) - 1) << 8)
#define SIE_DUMMY2_OFFSET		   16
#define SIE_DUMMY2_BITS			   8
#define SIE_DUMMY2_MASK			   (((1 << 8) - 1) << 16)
#define SIE_DUMMY3_OFFSET		   24
#define SIE_DUMMY3_BITS			   8
#define SIE_DUMMY3_MASK			   (((1 << 8) - 1) << 24)
/* USB_DUMMY1 0x0084 */
#define SIE_DUMMY4_OFFSET		   0
#define SIE_DUMMY4_BITS			   8
#define SIE_DUMMY4_MASK			   (((1 << 8) - 1) << 0)
#define SIE_DUMMY5_OFFSET		   8
#define SIE_DUMMY5_BITS			   8
#define SIE_DUMMY5_MASK			   (((1 << 8) - 1) << 8)
#define SIE_DUMMY6_OFFSET		   16
#define SIE_DUMMY6_BITS			   8
#define SIE_DUMMY6_MASK			   (((1 << 8) - 1) << 16)
#define SIE_DUMMY7_OFFSET		   24
#define SIE_DUMMY7_BITS			   8
#define SIE_DUMMY7_MASK			   (((1 << 8) - 1) << 24)
/* USB_BULKINEPA_CFG 0x0200 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
/* USB_BULKINEPA_CTL 0x0204 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPA_TOGGLE_CTRL 0x020C */
#define EP_ITOGL_OFFSET			   0
#define EP_ITOGL_BITS			   1
#define EP_ITOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKINEPA_IRQ_EN 0x0210 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPA_IRQ_STATUS 0x0214 */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPB_CFG 0x0240 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
/* USB_BULKINEPB_CTL 0x0244 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPB_TOGGLE_CTRL 0x024C */
#define EP_ITOGL_OFFSET			   0
#define EP_ITOGL_BITS			   1
#define EP_ITOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKINEPB_IRQ_EN 0x0250 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPB_IRQ_STATUS 0x0254 */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPC_CFG 0x0280 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
/* USB_BULKINEPC_CTL 0x0284 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPC_TOGGLE_CTRL 0x028C */
#define EP_ITOGL_OFFSET			   0
#define EP_ITOGL_BITS			   1
#define EP_ITOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKINEPC_IRQ_EN 0x0290 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKINEPC_IRQ_STATUS 0x0294 */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKOUTEPA_CFG 0x0300 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
#define BULKOUT_OLD_MODE_OFFSET		   27
#define BULKOUT_OLD_MODE_BITS		   1
#define BULKOUT_OLD_MODE_MASK		   (((1 << 1) - 1) << 27)
#define U_FRC_BULKOUT_DISABLE_NYET_OFFSET  28
#define U_FRC_BULKOUT_DISABLE_NYET_BITS	   1
#define U_FRC_BULKOUT_DISABLE_NYET_MASK	   (((1 << 1) - 1) << 28)
#define U_FRC_BULKOUT_SHTPKT_ACK_OFFSET	   29
#define U_FRC_BULKOUT_SHTPKT_ACK_BITS	   1
#define U_FRC_BULKOUT_SHTPKT_ACK_MASK	   (((1 << 1) - 1) << 29)
#define U_FRC_BULKOUT_LAST_DPKT_ACK_OFFSET 30
#define U_FRC_BULKOUT_LAST_DPKT_ACK_BITS   1
#define U_FRC_BULKOUT_LAST_DPKT_ACK_MASK   (((1 << 1) - 1) << 30)
#define EP_NAKOUT_MODE_OFFSET		   31
#define EP_NAKOUT_MODE_BITS		   1
#define EP_NAKOUT_MODE_MASK		   (((1 << 1) - 1) << 31)
/* USB_BULKOUTEPA_CTL 0x0304 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKOUTEPA_TOGGLE_CTRL 0x030C */
#define EP_OTOGL_OFFSET			   0
#define EP_OTOGL_BITS			   1
#define EP_OTOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKOUTEPA_IRQ_EN 0x0310 */
#define IE_EPOUTT_OFFSET		   0
#define IE_EPOUTT_BITS			   1
#define IE_EPOUTT_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPOUT_OFFSET			   1
#define IE_EPOUT_BITS			   1
#define IE_EPOUT_MASK			   (((1 << 1) - 1) << 1)
#define IE_EPSHT_OFFSET			   2
#define IE_EPSHT_BITS			   1
#define IE_EPSHT_MASK			   (((1 << 1) - 1) << 2)
#define IE_EPPING_OFFSET		   3
#define IE_EPPING_BITS			   1
#define IE_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_BULKOUTEPA_IRQ_STATUS 0x0314 */
#define I_EPOUTTF_OFFSET		   0
#define I_EPOUTTF_BITS			   1
#define I_EPOUTTF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPOUTF_OFFSET			   1
#define I_EPOUTF_BITS			   1
#define I_EPOUTF_MASK			   (((1 << 1) - 1) << 1)
#define I_EPOSHTF_OFFSET		   2
#define I_EPOSHTF_BITS			   1
#define I_EPOSHTF_MASK			   (((1 << 1) - 1) << 2)
#define I_EPPING_OFFSET			   3
#define I_EPPING_BITS			   1
#define I_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_BULKOUTEPB_CFG 0x0340 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
#define BULKOUT_OLD_MODE_OFFSET		   27
#define BULKOUT_OLD_MODE_BITS		   1
#define BULKOUT_OLD_MODE_MASK		   (((1 << 1) - 1) << 27)
#define U_FRC_BULKOUT_DISABLE_NYET_OFFSET  28
#define U_FRC_BULKOUT_DISABLE_NYET_BITS	   1
#define U_FRC_BULKOUT_DISABLE_NYET_MASK	   (((1 << 1) - 1) << 28)
#define U_FRC_BULKOUT_SHTPKT_ACK_OFFSET	   29
#define U_FRC_BULKOUT_SHTPKT_ACK_BITS	   1
#define U_FRC_BULKOUT_SHTPKT_ACK_MASK	   (((1 << 1) - 1) << 29)
#define U_FRC_BULKOUT_LAST_DPKT_ACK_OFFSET 30
#define U_FRC_BULKOUT_LAST_DPKT_ACK_BITS   1
#define U_FRC_BULKOUT_LAST_DPKT_ACK_MASK   (((1 << 1) - 1) << 30)
#define EP_NAKOUT_MODE_OFFSET		   31
#define EP_NAKOUT_MODE_BITS		   1
#define EP_NAKOUT_MODE_MASK		   (((1 << 1) - 1) << 31)
/* USB_BULKOUTEPB_CTL 0x0344 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKOUTEPB_TOGGLE_CTRL 0x034C */
#define EP_OTOGL_OFFSET			   0
#define EP_OTOGL_BITS			   1
#define EP_OTOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKOUTEPB_IRQ_EN 0x0350 */
#define IE_EPOUTT_OFFSET		   0
#define IE_EPOUTT_BITS			   1
#define IE_EPOUTT_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPOUT_OFFSET			   1
#define IE_EPOUT_BITS			   1
#define IE_EPOUT_MASK			   (((1 << 1) - 1) << 1)
#define IE_EPSHT_OFFSET			   2
#define IE_EPSHT_BITS			   1
#define IE_EPSHT_MASK			   (((1 << 1) - 1) << 2)
#define IE_EPPING_OFFSET		   3
#define IE_EPPING_BITS			   1
#define IE_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_BULKOUTEPB_IRQ_STATUS 0x0354 */
#define I_EPOUTTF_OFFSET		   0
#define I_EPOUTTF_BITS			   1
#define I_EPOUTTF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPOUTF_OFFSET			   1
#define I_EPOUTF_BITS			   1
#define I_EPOUTF_MASK			   (((1 << 1) - 1) << 1)
#define I_EPOSHTF_OFFSET		   2
#define I_EPOSHTF_BITS			   1
#define I_EPOSHTF_MASK			   (((1 << 1) - 1) << 2)
#define I_EPPING_OFFSET			   3
#define I_EPPING_BITS			   1
#define I_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_BULKOUTEPC_CFG 0x0380 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_EPNUM_OFFSET			   4
#define EP_EPNUM_BITS			   4
#define EP_EPNUM_MASK			   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT_OFFSET		   16
#define EP_MAXPKT_BITS			   10
#define EP_MAXPKT_MASK			   (((1 << 10) - 1) << 16)
#define BULKOUT_OLD_MODE_OFFSET		   27
#define BULKOUT_OLD_MODE_BITS		   1
#define BULKOUT_OLD_MODE_MASK		   (((1 << 1) - 1) << 27)
#define U_FRC_BULKOUT_DISABLE_NYET_OFFSET  28
#define U_FRC_BULKOUT_DISABLE_NYET_BITS	   1
#define U_FRC_BULKOUT_DISABLE_NYET_MASK	   (((1 << 1) - 1) << 28)
#define U_FRC_BULKOUT_SHTPKT_ACK_OFFSET	   29
#define U_FRC_BULKOUT_SHTPKT_ACK_BITS	   1
#define U_FRC_BULKOUT_SHTPKT_ACK_MASK	   (((1 << 1) - 1) << 29)
#define U_FRC_BULKOUT_LAST_DPKT_ACK_OFFSET 30
#define U_FRC_BULKOUT_LAST_DPKT_ACK_BITS   1
#define U_FRC_BULKOUT_LAST_DPKT_ACK_MASK   (((1 << 1) - 1) << 30)
#define EP_NAKOUT_MODE_OFFSET		   31
#define EP_NAKOUT_MODE_BITS		   1
#define EP_NAKOUT_MODE_MASK		   (((1 << 1) - 1) << 31)
/* USB_BULKOUTEPC_CTL 0x0384 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
#define EP_RESET_OFFSET			   1
#define EP_RESET_BITS			   1
#define EP_RESET_MASK			   (((1 << 1) - 1) << 1)
/* USB_BULKOUTEPC_TOGGLE_CTRL 0x038C */
#define EP_OTOGL_OFFSET			   0
#define EP_OTOGL_BITS			   1
#define EP_OTOGL_MASK			   (((1 << 1) - 1) << 0)
/* USB_BULKOUTEPC_IRQ_EN 0x0390 */
#define IE_EPOUTT_OFFSET		   0
#define IE_EPOUTT_BITS			   1
#define IE_EPOUTT_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPOUT_OFFSET			   1
#define IE_EPOUT_BITS			   1
#define IE_EPOUT_MASK			   (((1 << 1) - 1) << 1)
#define IE_EPSHT_OFFSET			   2
#define IE_EPSHT_BITS			   1
#define IE_EPSHT_MASK			   (((1 << 1) - 1) << 2)
#define IE_EPPING_OFFSET		   3
#define IE_EPPING_BITS			   1
#define IE_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_BULKOUTEPC_IRQ_STATUS 0x0394 */
#define I_EPOUTTF_OFFSET		   0
#define I_EPOUTTF_BITS			   1
#define I_EPOUTTF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPOUTF_OFFSET			   1
#define I_EPOUTF_BITS			   1
#define I_EPOUTF_MASK			   (((1 << 1) - 1) << 1)
#define I_EPOSHTF_OFFSET		   2
#define I_EPOSHTF_BITS			   1
#define I_EPOSHTF_MASK			   (((1 << 1) - 1) << 2)
#define I_EPPING_OFFSET			   3
#define I_EPPING_BITS			   1
#define I_EPPING_MASK			   (((1 << 1) - 1) << 3)
/* USB_UACINEP_CFG 0x03C0 */
#define UACEP_EN_OFFSET			   0
#define UACEP_EN_BITS			   1
#define UACEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define UACEP_EPNUM_OFFSET		   4
#define UACEP_EPNUM_BITS		   4
#define UACEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define UACEP_BUF_EN_SEL_OFFSET		   8
#define UACEP_BUF_EN_SEL_BITS		   8
#define UACEP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define UACINEP_MAXPKT_OFFSET		   16
#define UACINEP_MAXPKT_BITS		   11
#define UACINEP_MAXPKT_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_CTL 0x03C4 */
#define UACEP_STALL_OFFSET		   0
#define UACEP_STALL_BITS		   1
#define UACEP_STALL_MASK		   (((1 << 1) - 1) << 0)
#define UACEP_RESET_OFFSET		   1
#define UACEP_RESET_BITS		   1
#define UACEP_RESET_MASK		   (((1 << 1) - 1) << 1)
/* USB_UACINEP_IRQ_EN 0x03C8 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
#define IE_ERR_OFFSET			   2
#define IE_ERR_BITS			   1
#define IE_ERR_MASK			   (((1 << 1) - 1) << 2)
#define IE_INTOKEN_NUM_OFFSET		   3
#define IE_INTOKEN_NUM_BITS		   1
#define IE_INTOKEN_NUM_MASK		   (((1 << 1) - 1) << 3)
/* USB_UACINEP_IRQ_STATUS 0x03CC */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
#define I_ERR_OFFSET			   2
#define I_ERR_BITS			   1
#define I_ERR_MASK			   (((1 << 1) - 1) << 2)
#define I_INTOKEN_NUM_OFFSET		   3
#define I_INTOKEN_NUM_BITS		   1
#define I_INTOKEN_NUM_MASK		   (((1 << 1) - 1) << 3)
/* USB_UACINEP_PKTSIZE_INDEX 0x03D0 */
#define UACIN_PKTSIZE_INDEX_OFFSET	   0
#define UACIN_PKTSIZE_INDEX_BITS	   5
#define UACIN_PKTSIZE_INDEX_MASK	   (((1 << 5) - 1) << 0)
/* USB_UACINEP_PKTSIZE0 0x03D4 */
#define UACIN_PKTSIZE010_0_OFFSET	   0
#define UACIN_PKTSIZE010_0_BITS		   11
#define UACIN_PKTSIZE010_0_MASK		   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE110_0_OFFSET	   16
#define UACIN_PKTSIZE110_0_BITS		   11
#define UACIN_PKTSIZE110_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE1 0x03D8 */
#define UACIN_PKTSIZE210_0_OFFSET	   0
#define UACIN_PKTSIZE210_0_BITS		   11
#define UACIN_PKTSIZE210_0_MASK		   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE310_0_OFFSET	   16
#define UACIN_PKTSIZE310_0_BITS		   11
#define UACIN_PKTSIZE310_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE2 0x03DC */
#define UACIN_PKTSIZE410_0_OFFSET	   0
#define UACIN_PKTSIZE410_0_BITS		   11
#define UACIN_PKTSIZE410_0_MASK		   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE510_0_OFFSET	   16
#define UACIN_PKTSIZE510_0_BITS		   11
#define UACIN_PKTSIZE510_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE3 0x03E0 */
#define UACIN_PKTSIZE610_0_OFFSET	   0
#define UACIN_PKTSIZE610_0_BITS		   11
#define UACIN_PKTSIZE610_0_MASK		   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE710_0_OFFSET	   16
#define UACIN_PKTSIZE710_0_BITS		   11
#define UACIN_PKTSIZE710_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE4 0x03E4 */
#define UACIN_PKTSIZE810_0_OFFSET	   0
#define UACIN_PKTSIZE810_0_BITS		   11
#define UACIN_PKTSIZE810_0_MASK		   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE910_0_OFFSET	   16
#define UACIN_PKTSIZE910_0_BITS		   11
#define UACIN_PKTSIZE910_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE5 0x03E8 */
#define UACIN_PKTSIZE1010_0_OFFSET	   0
#define UACIN_PKTSIZE1010_0_BITS	   11
#define UACIN_PKTSIZE1010_0_MASK	   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE1110_0_OFFSET	   16
#define UACIN_PKTSIZE1110_0_BITS	   11
#define UACIN_PKTSIZE1110_0_MASK	   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE6 0x03EC */
#define UACIN_PKTSIZE1210_0_OFFSET	   0
#define UACIN_PKTSIZE1210_0_BITS	   11
#define UACIN_PKTSIZE1210_0_MASK	   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE1310_0_OFFSET	   16
#define UACIN_PKTSIZE1310_0_BITS	   11
#define UACIN_PKTSIZE1310_0_MASK	   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE7 0x03F0 */
#define UACIN_PKTSIZE1410_0_OFFSET	   0
#define UACIN_PKTSIZE1410_0_BITS	   11
#define UACIN_PKTSIZE1410_0_MASK	   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE1510_0_OFFSET	   16
#define UACIN_PKTSIZE1510_0_BITS	   11
#define UACIN_PKTSIZE1510_0_MASK	   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE8 0x03F4 */
#define UACIN_PKTSIZE1610_0_OFFSET	   0
#define UACIN_PKTSIZE1610_0_BITS	   11
#define UACIN_PKTSIZE1610_0_MASK	   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE1710_0_OFFSET	   16
#define UACIN_PKTSIZE1710_0_BITS	   11
#define UACIN_PKTSIZE1710_0_MASK	   (((1 << 11) - 1) << 16)
/* USB_UACINEP_PKTSIZE9 0x03F8 */
#define UACIN_PKTSIZE1810_0_OFFSET	   0
#define UACIN_PKTSIZE1810_0_BITS	   11
#define UACIN_PKTSIZE1810_0_MASK	   (((1 << 11) - 1) << 0)
#define UACIN_PKTSIZE1910_0_OFFSET	   16
#define UACIN_PKTSIZE1910_0_BITS	   11
#define UACIN_PKTSIZE1910_0_MASK	   (((1 << 11) - 1) << 16)
/* USB_UACINEP_CFG1 0x03FC */
#define CFG_INTOKEN_NUM7_0_OFFSET	   0
#define CFG_INTOKEN_NUM7_0_BITS		   8
#define CFG_INTOKEN_NUM7_0_MASK		   (((1 << 8) - 1) << 0)
#define INTOKEN_CNT_EN_OFFSET		   8
#define INTOKEN_CNT_EN_BITS		   1
#define INTOKEN_CNT_EN_MASK		   (((1 << 1) - 1) << 8)
/* USB_UACOUTEP_CFG 0x0400 */
#define UACEP_EN_OFFSET			   0
#define UACEP_EN_BITS			   1
#define UACEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define UACEP_EPNUM_OFFSET		   4
#define UACEP_EPNUM_BITS		   4
#define UACEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define UACEP_BUF_EN_SEL_OFFSET		   8
#define UACEP_BUF_EN_SEL_BITS		   8
#define UACEP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define UACOUTEP_MAXPKT_OFFSET		   16
#define UACOUTEP_MAXPKT_BITS		   11
#define UACOUTEP_MAXPKT_MASK		   (((1 << 11) - 1) << 16)
/* USB_UACOUTEP_CTL 0x0404 */
#define UACEP_FIFO_CLR_OFFSET		   0
#define UACEP_FIFO_CLR_BITS		   1
#define UACEP_FIFO_CLR_MASK		   (((1 << 1) - 1) << 0)
#define U_FIFO_FLUSH_EN_OFFSET		   1
#define U_FIFO_FLUSH_EN_BITS		   1
#define U_FIFO_FLUSH_EN_MASK		   (((1 << 1) - 1) << 1)
/* USB_UACOUTEP_IRQ_EN 0x0410 */
#define IE_UACEPOUT_OFFSET		   0
#define IE_UACEPOUT_BITS		   1
#define IE_UACEPOUT_MASK		   (((1 << 1) - 1) << 0)
#define IE_UACEPOUTT_OFFSET		   1
#define IE_UACEPOUTT_BITS		   1
#define IE_UACEPOUTT_MASK		   (((1 << 1) - 1) << 1)
#define IE_EPOUT_ERRF_OFFSET		   2
#define IE_EPOUT_ERRF_BITS		   1
#define IE_EPOUT_ERRF_MASK		   (((1 << 1) - 1) << 2)
#define IE_OUT_NUM_OFFSET		   3
#define IE_OUT_NUM_BITS			   1
#define IE_OUT_NUM_MASK			   (((1 << 1) - 1) << 3)
/* USB_UACOUTEP_IRQ_STATUS 0x0414 */
#define I_UACEPOUTF_OFFSET		   0
#define I_UACEPOUTF_BITS		   1
#define I_UACEPOUTF_MASK		   (((1 << 1) - 1) << 0)
#define I_UACEPOUTTF_OFFSET		   1
#define I_UACEPOUTTF_BITS		   1
#define I_UACEPOUTTF_MASK		   (((1 << 1) - 1) << 1)
#define I_UACEPOUT_ERRF_OFFSET		   2
#define I_UACEPOUT_ERRF_BITS		   1
#define I_UACEPOUT_ERRF_MASK		   (((1 << 1) - 1) << 2)
#define I_OUT_NUM_OFFSET		   3
#define I_OUT_NUM_BITS			   1
#define I_OUT_NUM_MASK			   (((1 << 1) - 1) << 3)
/* USB_UACOUTEP_CFG1 0x0418 */
#define CFG_OUT_NUM7_0_OFFSET		   0
#define CFG_OUT_NUM7_0_BITS		   8
#define CFG_OUT_NUM7_0_MASK		   (((1 << 8) - 1) << 0)
#define OUT_CNT_EN_OFFSET		   8
#define OUT_CNT_EN_BITS			   1
#define OUT_CNT_EN_MASK			   (((1 << 1) - 1) << 8)
/* USB_UVCINEPA_CFG 0x0440 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_ISBULK_OFFSET		   1
#define EP_ISBULK_BITS			   1
#define EP_ISBULK_MASK			   (((1 << 1) - 1) << 1)
#define EP_BULKMODE_OFFSET		   2
#define EP_BULKMODE_BITS		   1
#define EP_BULKMODE_MASK		   (((1 << 1) - 1) << 2)
#define EP_EPNUM3_0_OFFSET		   4
#define EP_EPNUM3_0_BITS		   4
#define EP_EPNUM3_0_MASK		   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT10_0_OFFSET		   16
#define EP_MAXPKT10_0_BITS		   11
#define EP_MAXPKT10_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UVCINEPA_CTL 0x0444 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
/* USB_UVCINEPA_TOGGLE_CTRL 0x044C */
// #define EP_FORCE_TOGL_TRIG_OFFSET 0
// #define EP_FORCE_TOGL_TRIG_BITS 1
// #define EP_FORCE_TOGL_TRIG_MASK (((1 << 1) - 1) << 0)
// #define EP_FORCE_TOGL_VAL_OFFSET 1
// #define EP_FORCE_TOGL_VAL_BITS 1
// #define EP_FORCE_TOGL_VAL_MASK (((1 << 1) - 1) << 1)
// #define EP_ITOGL_OFFSET 2
// #define EP_ITOGL_BITS 1
// #define EP_ITOGL_MASK (((1 << 1) - 1) << 2)
/* USB_UVCINEPA_IRQ_EN 0x0450 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
#define IE_EPENDFRAME_OFFSET		   2
#define IE_EPENDFRAME_BITS		   1
#define IE_EPENDFRAME_MASK		   (((1 << 1) - 1) << 2)
/* USB_UVCINEPA_IRQ_STATUS 0x0454 */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
#define I_EPENDFRAME_OFFSET		   2
#define I_EPENDFRAME_BITS		   1
#define I_EPENDFRAME_MASK		   (((1 << 1) - 1) << 2)
/* USB_UVCINEPA_CFG2 0x0458 */
#define EP_CFG_HWM17_0_OFFSET		   0
#define EP_CFG_HWM17_0_BITS		   18
#define EP_CFG_HWM17_0_MASK		   (((1 << 18) - 1) << 0)
#define EP_CFG_INTERVAL6_0_OFFSET	   24
#define EP_CFG_INTERVAL6_0_BITS		   7
#define EP_CFG_INTERVAL6_0_MASK		   (((1 << 7) - 1) << 24)
/* USB_UVCINEPA_CFG3 0x045C */
#define EP_CFG_TRANS_NUM15_0_OFFSET	   0
#define EP_CFG_TRANS_NUM15_0_BITS	   16
#define EP_CFG_TRANS_NUM15_0_MASK	   (((1 << 16) - 1) << 0)
#define EP_TRANSFER_SIZE7_0_OFFSET	   16
#define EP_TRANSFER_SIZE7_0_BITS	   8
#define EP_TRANSFER_SIZE7_0_MASK	   (((1 << 8) - 1) << 16)
#define EP_CFG_FPS5_0_OFFSET		   24
#define EP_CFG_FPS5_0_BITS		   6
#define EP_CFG_FPS5_0_MASK		   (((1 << 6) - 1) << 24)
/* USB_UVCINEPA_CFG4 0x0460 */
#define EP_CFG_PTS_INTERVAL_N23_0_OFFSET   0
#define EP_CFG_PTS_INTERVAL_N23_0_BITS	   24
#define EP_CFG_PTS_INTERVAL_N23_0_MASK	   (((1 << 24) - 1) << 0)
#define EP_CFG_PTS_INTERVAL_F5_0_OFFSET	   24
#define EP_CFG_PTS_INTERVAL_F5_0_BITS	   6
#define EP_CFG_PTS_INTERVAL_F5_0_MASK	   (((1 << 6) - 1) << 24)
/* USB_UVCINEPA_CFG5 0x0464 */
#define STOP_STILL_IMAGE_EN_OFFSET	   0
#define STOP_STILL_IMAGE_EN_BITS	   1
#define STOP_STILL_IMAGE_EN_MASK	   (((1 << 1) - 1) << 0)
#define EP_ISOTYPE1_0_OFFSET		   1
#define EP_ISOTYPE1_0_BITS		   2
#define EP_ISOTYPE1_0_MASK		   (((1 << 2) - 1) << 1)
#define EP_CFG_UVC1P5_OFFSET		   3
#define EP_CFG_UVC1P5_BITS		   1
#define EP_CFG_UVC1P5_MASK		   (((1 << 1) - 1) << 3)
#define EP_CFG_STILL_SEL_OFFSET		   4
#define EP_CFG_STILL_SEL_BITS		   1
#define EP_CFG_STILL_SEL_MASK		   (((1 << 1) - 1) << 4)
#define EP_ISO_XACT_EN_OFFSET		   5
#define EP_ISO_XACT_EN_BITS		   1
#define EP_ISO_XACT_EN_MASK		   (((1 << 1) - 1) << 5)
#define EP_TRANSFER_EN_OFFSET		   6
#define EP_TRANSFER_EN_BITS		   1
#define EP_TRANSFER_EN_MASK		   (((1 << 1) - 1) << 6)
#define EP_ISO_0BYTE_MODE_N_OFFSET	   7
#define EP_ISO_0BYTE_MODE_N_BITS	   1
#define EP_ISO_0BYTE_MODE_N_MASK	   (((1 << 1) - 1) << 7)
#define EP_CFG_TRANS_CNT_OFFSET		   8
#define EP_CFG_TRANS_CNT_BITS		   1
#define EP_CFG_TRANS_CNT_MASK		   (((1 << 1) - 1) << 8)
#define EP_RF_MAXPKT_DENOISE_OFFSET	   9
#define EP_RF_MAXPKT_DENOISE_BITS	   1
#define EP_RF_MAXPKT_DENOISE_MASK	   (((1 << 1) - 1) << 9)
#define EP_RF_CNT_EN_OFFSET		   10
#define EP_RF_CNT_EN_BITS		   1
#define EP_RF_CNT_EN_MASK		   (((1 << 1) - 1) << 10)
#define EP_RF_TIMER_EN_OFFSET		   11
#define EP_RF_TIMER_EN_BITS		   1
#define EP_RF_TIMER_EN_MASK		   (((1 << 1) - 1) << 11)
#define EP_RF_TIMER_END7_0_OFFSET	   16
#define EP_RF_TIMER_END7_0_BITS		   8
#define EP_RF_TIMER_END7_0_MASK		   (((1 << 8) - 1) << 16)
#define EP_FCNT_SEC_POWER2_0_OFFSET	   24
#define EP_FCNT_SEC_POWER2_0_BITS	   3
#define EP_FCNT_SEC_POWER2_0_MASK	   (((1 << 3) - 1) << 24)
#define EP_FW_SIE_FCNT_EN_OFFSET	   27
#define EP_FW_SIE_FCNT_EN_BITS		   1
#define EP_FW_SIE_FCNT_EN_MASK		   (((1 << 1) - 1) << 27)
#define EP_CFG_PTS_FIX_INTERVAL_OFFSET	   28
#define EP_CFG_PTS_FIX_INTERVAL_BITS	   1
#define EP_CFG_PTS_FIX_INTERVAL_MASK	   (((1 << 1) - 1) << 28)
/* USB_UVCINEPA_PHINF0 0x0468 */
#define EP_FRAME_ID_OFFSET		   0
#define EP_FRAME_ID_BITS		   1
#define EP_FRAME_ID_MASK		   (((1 << 1) - 1) << 0)
#define EP_END_OF_FRAME_OFFSET		   1
#define EP_END_OF_FRAME_BITS		   1
#define EP_END_OF_FRAME_MASK		   (((1 << 1) - 1) << 1)
#define EP_PTS_INCLUDE_OFFSET		   2
#define EP_PTS_INCLUDE_BITS		   1
#define EP_PTS_INCLUDE_MASK		   (((1 << 1) - 1) << 2)
#define EP_SCR_INCLUDE_OFFSET		   3
#define EP_SCR_INCLUDE_BITS		   1
#define EP_SCR_INCLUDE_MASK		   (((1 << 1) - 1) << 3)
#define EP_END_OF_FRAME_EN_OFFSET	   4
#define EP_END_OF_FRAME_EN_BITS		   1
#define EP_END_OF_FRAME_EN_MASK		   (((1 << 1) - 1) << 4)
#define EP_STILL_IMAGE_T_OFFSET		   5
#define EP_STILL_IMAGE_T_BITS		   1
#define EP_STILL_IMAGE_T_MASK		   (((1 << 1) - 1) << 5)
#define EP_END_OF_HEAD_OFFSET		   7
#define EP_END_OF_HEAD_BITS		   1
#define EP_END_OF_HEAD_MASK		   (((1 << 1) - 1) << 7)
/* USB_UVCINEPB_CFG 0x0480 */
#define EP_EN_OFFSET			   0
#define EP_EN_BITS			   1
#define EP_EN_MASK			   (((1 << 1) - 1) << 0)
#define EP_ISBULK_OFFSET		   1
#define EP_ISBULK_BITS			   1
#define EP_ISBULK_MASK			   (((1 << 1) - 1) << 1)
#define EP_BULKMODE_OFFSET		   2
#define EP_BULKMODE_BITS		   1
#define EP_BULKMODE_MASK		   (((1 << 1) - 1) << 2)
#define EP_EPNUM3_0_OFFSET		   4
#define EP_EPNUM3_0_BITS		   4
#define EP_EPNUM3_0_MASK		   (((1 << 4) - 1) << 4)
#define EP_BUF_EN_SEL_OFFSET		   8
#define EP_BUF_EN_SEL_BITS		   8
#define EP_BUF_EN_SEL_MASK		   (((1 << 8) - 1) << 8)
#define EP_MAXPKT10_0_OFFSET		   16
#define EP_MAXPKT10_0_BITS		   11
#define EP_MAXPKT10_0_MASK		   (((1 << 11) - 1) << 16)
/* USB_UVCINEPB_CTL 0x0484 */
#define EP_STALL_OFFSET			   0
#define EP_STALL_BITS			   1
#define EP_STALL_MASK			   (((1 << 1) - 1) << 0)
/* USB_UVCINEPB_TOGGLE_CTRL 0x048C */
// #define EP_FORCE_TOGL_TRIG_OFFSET 0
// #define EP_FORCE_TOGL_TRIG_BITS 1
// #define EP_FORCE_TOGL_TRIG_MASK (((1 << 1) - 1) << 0)
// #define EP_FORCE_TOGL_VAL_OFFSET 1
// #define EP_FORCE_TOGL_VAL_BITS 1
// #define EP_FORCE_TOGL_VAL_MASK (((1 << 1) - 1) << 1)
// #define EP_ITOGL_OFFSET 2
// #define EP_ITOGL_BITS 1
// #define EP_ITOGL_MASK (((1 << 1) - 1) << 2)
/* USB_UVCINEPB_IRQ_EN 0x0490 */
#define IE_EPIN_OFFSET			   0
#define IE_EPIN_BITS			   1
#define IE_EPIN_MASK			   (((1 << 1) - 1) << 0)
#define IE_EPINT_OFFSET			   1
#define IE_EPINT_BITS			   1
#define IE_EPINT_MASK			   (((1 << 1) - 1) << 1)
#define IE_EPENDFRAME_OFFSET		   2
#define IE_EPENDFRAME_BITS		   1
#define IE_EPENDFRAME_MASK		   (((1 << 1) - 1) << 2)
/* USB_UVCINEPB_IRQ_STATUS 0x0494 */
#define I_EPINF_OFFSET			   0
#define I_EPINF_BITS			   1
#define I_EPINF_MASK			   (((1 << 1) - 1) << 0)
#define I_EPINTF_OFFSET			   1
#define I_EPINTF_BITS			   1
#define I_EPINTF_MASK			   (((1 << 1) - 1) << 1)
#define I_EPENDFRAME_OFFSET		   2
#define I_EPENDFRAME_BITS		   1
#define I_EPENDFRAME_MASK		   (((1 << 1) - 1) << 2)
/* USB_UVCINEPB_CFG2 0x0498 */
#define EP_CFG_HWM17_0_OFFSET		   0
#define EP_CFG_HWM17_0_BITS		   18
#define EP_CFG_HWM17_0_MASK		   (((1 << 18) - 1) << 0)
#define EP_CFG_INTERVAL6_0_OFFSET	   24
#define EP_CFG_INTERVAL6_0_BITS		   7
#define EP_CFG_INTERVAL6_0_MASK		   (((1 << 7) - 1) << 24)
/* USB_UVCINEPB_CFG3 0x049C */
#define EP_CFG_TRANS_NUM15_0_OFFSET	   0
#define EP_CFG_TRANS_NUM15_0_BITS	   16
#define EP_CFG_TRANS_NUM15_0_MASK	   (((1 << 16) - 1) << 0)
#define EP_TRANSFER_SIZE7_0_OFFSET	   16
#define EP_TRANSFER_SIZE7_0_BITS	   8
#define EP_TRANSFER_SIZE7_0_MASK	   (((1 << 8) - 1) << 16)
#define EP_CFG_FPS5_0_OFFSET		   24
#define EP_CFG_FPS5_0_BITS		   6
#define EP_CFG_FPS5_0_MASK		   (((1 << 6) - 1) << 24)
/* USB_UVCINEPB_CFG4 0x04A0 */
#define EP_CFG_PTS_INTERVAL_N23_0_OFFSET   0
#define EP_CFG_PTS_INTERVAL_N23_0_BITS	   24
#define EP_CFG_PTS_INTERVAL_N23_0_MASK	   (((1 << 24) - 1) << 0)
#define EP_CFG_PTS_INTERVAL_F5_0_OFFSET	   24
#define EP_CFG_PTS_INTERVAL_F5_0_BITS	   6
#define EP_CFG_PTS_INTERVAL_F5_0_MASK	   (((1 << 6) - 1) << 24)
/* USB_UVCINEPB_CFG5 0x04A4 */
#define STOP_STILL_IMAGE_EN_OFFSET	   0
#define STOP_STILL_IMAGE_EN_BITS	   1
#define STOP_STILL_IMAGE_EN_MASK	   (((1 << 1) - 1) << 0)
#define EP_ISOTYPE1_0_OFFSET		   1
#define EP_ISOTYPE1_0_BITS		   2
#define EP_ISOTYPE1_0_MASK		   (((1 << 2) - 1) << 1)
#define EP_CFG_UVC1P5_OFFSET		   3
#define EP_CFG_UVC1P5_BITS		   1
#define EP_CFG_UVC1P5_MASK		   (((1 << 1) - 1) << 3)
#define EP_CFG_STILL_SEL_OFFSET		   4
#define EP_CFG_STILL_SEL_BITS		   1
#define EP_CFG_STILL_SEL_MASK		   (((1 << 1) - 1) << 4)
#define EP_ISO_XACT_EN_OFFSET		   5
#define EP_ISO_XACT_EN_BITS		   1
#define EP_ISO_XACT_EN_MASK		   (((1 << 1) - 1) << 5)
#define EP_TRANSFER_EN_OFFSET		   6
#define EP_TRANSFER_EN_BITS		   1
#define EP_TRANSFER_EN_MASK		   (((1 << 1) - 1) << 6)
#define EP_ISO_0BYTE_MODE_N_OFFSET	   7
#define EP_ISO_0BYTE_MODE_N_BITS	   1
#define EP_ISO_0BYTE_MODE_N_MASK	   (((1 << 1) - 1) << 7)
#define EP_CFG_TRANS_CNT_OFFSET		   8
#define EP_CFG_TRANS_CNT_BITS		   1
#define EP_CFG_TRANS_CNT_MASK		   (((1 << 1) - 1) << 8)
#define EP_RF_MAXPKT_DENOISE_OFFSET	   9
#define EP_RF_MAXPKT_DENOISE_BITS	   1
#define EP_RF_MAXPKT_DENOISE_MASK	   (((1 << 1) - 1) << 9)
#define EP_RF_CNT_EN_OFFSET		   10
#define EP_RF_CNT_EN_BITS		   1
#define EP_RF_CNT_EN_MASK		   (((1 << 1) - 1) << 10)
#define EP_RF_TIMER_EN_OFFSET		   11
#define EP_RF_TIMER_EN_BITS		   1
#define EP_RF_TIMER_EN_MASK		   (((1 << 1) - 1) << 11)
#define EP_RF_TIMER_END7_0_OFFSET	   16
#define EP_RF_TIMER_END7_0_BITS		   8
#define EP_RF_TIMER_END7_0_MASK		   (((1 << 8) - 1) << 16)
#define EP_FCNT_SEC_POWER2_0_OFFSET	   24
#define EP_FCNT_SEC_POWER2_0_BITS	   3
#define EP_FCNT_SEC_POWER2_0_MASK	   (((1 << 3) - 1) << 24)
#define EP_FW_SIE_FCNT_EN_OFFSET	   27
#define EP_FW_SIE_FCNT_EN_BITS		   1
#define EP_FW_SIE_FCNT_EN_MASK		   (((1 << 1) - 1) << 27)
#define EP_CFG_PTS_FIX_INTERVAL_OFFSET	   28
#define EP_CFG_PTS_FIX_INTERVAL_BITS	   1
#define EP_CFG_PTS_FIX_INTERVAL_MASK	   (((1 << 1) - 1) << 28)
/* USB_UVCINEPB_PHINF0 0x04A8 */
#define EP_FRAME_ID_OFFSET		   0
#define EP_FRAME_ID_BITS		   1
#define EP_FRAME_ID_MASK		   (((1 << 1) - 1) << 0)
#define EP_END_OF_FRAME_OFFSET		   1
#define EP_END_OF_FRAME_BITS		   1
#define EP_END_OF_FRAME_MASK		   (((1 << 1) - 1) << 1)
#define EP_PTS_INCLUDE_OFFSET		   2
#define EP_PTS_INCLUDE_BITS		   1
#define EP_PTS_INCLUDE_MASK		   (((1 << 1) - 1) << 2)
#define EP_SCR_INCLUDE_OFFSET		   3
#define EP_SCR_INCLUDE_BITS		   1
#define EP_SCR_INCLUDE_MASK		   (((1 << 1) - 1) << 3)
#define EP_END_OF_FRAME_EN_OFFSET	   4
#define EP_END_OF_FRAME_EN_BITS		   1
#define EP_END_OF_FRAME_EN_MASK		   (((1 << 1) - 1) << 4)
#define EP_STILL_IMAGE_T_OFFSET		   5
#define EP_STILL_IMAGE_T_BITS		   1
#define EP_STILL_IMAGE_T_MASK		   (((1 << 1) - 1) << 5)
#define EP_END_OF_HEAD_OFFSET		   7
#define EP_END_OF_HEAD_BITS		   1
#define EP_END_OF_HEAD_MASK		   (((1 << 1) - 1) << 7)
/* USB_INTEREPA_CFG 0x0500 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPA_CTL 0x0504 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPA_BC 0x0508 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPA_TOGGLE_CTRL 0x050C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPA_IRQ_EN 0x0510 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPA_IRQ_STATUS 0x0514 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPA_DAT0 0x0520 */
/* USB_INTEREPA_DAT1 0x0524 */
/* USB_INTEREPA_DAT2 0x0528 */
/* USB_INTEREPA_DAT3 0x052C */
/* USB_INTEREPA_DAT4 0x0530 */
/* USB_INTEREPA_DAT5 0x0534 */
/* USB_INTEREPA_DAT6 0x0538 */
/* USB_INTEREPA_DAT7 0x053C */
/* USB_INTEREPA_DAT8 0x0540 */
/* USB_INTEREPA_DAT9 0x0544 */
/* USB_INTEREPA_DAT10 0x0548 */
/* USB_INTEREPA_DAT11 0x054C */
/* USB_INTEREPA_DAT12 0x0550 */
/* USB_INTEREPA_DAT13 0x0554 */
/* USB_INTEREPA_DAT14 0x0558 */
/* USB_INTEREPA_DAT15 0x055C */
/* UBS_INTEREPB_CFG 0x0580 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPB_CTL 0x0584 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPB_BC 0x0588 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPB_TOGGLE_CTRL 0x058C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPB_IRQ_EN 0x0590 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPB_IRQ_STATUS 0x0594 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPB_DAT0 0x05A0 */
/* USB_INTEREPB_DAT1 0x05A4 */
/* USB_INTEREPB_DAT2 0x05A8 */
/* USB_INTEREPB_DAT3 0x05AC */
/* USB_INTEREPB_DAT4 0x05B0 */
/* USB_INTEREPB_DAT5 0x05B4 */
/* USB_INTEREPB_DAT6 0x05B8 */
/* USB_INTEREPB_DAT7 0x05BC */
/* USB_INTEREPB_DAT8 0x05D0 */
/* USB_INTEREPB_DAT9 0x05D4 */
/* USB_INTEREPB_DAT10 0x05D8 */
/* USB_INTEREPB_DAT11 0x05DC */
/* USB_INTEREPB_DAT12 0x05E0 */
/* USB_INTEREPB_DAT13 0x05E4 */
/* USB_INTEREPB_DAT14 0x05E8 */
/* USB_INTEREPB_DAT15 0x05EC */
/* UBS_INTEREPC_CFG 0x0600 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPC_CTL 0x0604 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPC_BC 0x0608 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPC_TOGGLE_CTRL 0x060C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPC_IRQ_EN 0x0610 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPC_IRQ_STATUS 0x0614 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPC_DAT0 0x0620 */
/* USB_INTEREPC_DAT1 0x0624 */
/* USB_INTEREPC_DAT2 0x0628 */
/* USB_INTEREPC_DAT3 0x062C */
/* USB_INTEREPC_DAT4 0x0630 */
/* USB_INTEREPC_DAT5 0x0634 */
/* USB_INTEREPC_DAT6 0x0638 */
/* USB_INTEREPC_DAT7 0x063C */
/* USB_INTEREPC_DAT8 0x0640 */
/* USB_INTEREPC_DAT9 0x0644 */
/* USB_INTEREPC_DAT10 0x0648 */
/* USB_INTEREPC_DAT11 0x064C */
/* USB_INTEREPC_DAT12 0x0650 */
/* USB_INTEREPC_DAT13 0x0654 */
/* USB_INTEREPC_DAT14 0x0658 */
/* USB_INTEREPC_DAT15 0x065C */
/* UBS_INTEREPD_CFG 0x0680 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPD_CTL 0x0684 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPD_BC 0x0688 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPD_TOGGLE_CTRL 0x068C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPD_IRQ_EN 0x0690 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPD_IRQ_STATUS 0x0694 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPD_DAT0 0x06A0 */
/* USB_INTEREPD_DAT1 0x06A4 */
/* USB_INTEREPD_DAT2 0x06A8 */
/* USB_INTEREPD_DAT3 0x06AC */
/* USB_INTEREPD_DAT4 0x06B0 */
/* USB_INTEREPD_DAT5 0x06B4 */
/* USB_INTEREPD_DAT6 0x06B8 */
/* USB_INTEREPD_DAT7 0x06BC */
/* USB_INTEREPD_DAT8 0x06C0 */
/* USB_INTEREPD_DAT9 0x06C4 */
/* USB_INTEREPD_DAT10 0x06C8 */
/* USB_INTEREPD_DAT11 0x06CC */
/* USB_INTEREPD_DAT12 0x06D0 */
/* USB_INTEREPD_DAT13 0x06D4 */
/* USB_INTEREPD_DAT14 0x06D8 */
/* USB_INTEREPD_DAT15 0x06DC */
/* UBS_INTEREPE_CFG 0x0700 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPE_CTL 0x0704 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPE_BC 0x0708 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPE_TOGGLE_CTRL 0x070C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPE_IRQ_EN 0x0710 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPE_IRQ_STATUS 0x0714 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPE_DAT0 0x0720 */
/* USB_INTEREPE_DAT1 0x0724 */
/* USB_INTEREPE_DAT2 0x0728 */
/* USB_INTEREPE_DAT3 0x072C */
/* USB_INTEREPE_DAT4 0x0730 */
/* USB_INTEREPE_DAT5 0x0734 */
/* USB_INTEREPE_DAT6 0x0738 */
/* USB_INTEREPE_DAT7 0x073C */
/* USB_INTEREPE_DAT8 0x0740 */
/* USB_INTEREPE_DAT9 0x0744 */
/* USB_INTEREPE_DAT10 0x0748 */
/* USB_INTEREPE_DAT11 0x074C */
/* USB_INTEREPE_DAT12 0x0750 */
/* USB_INTEREPE_DAT13 0x0754 */
/* USB_INTEREPE_DAT14 0x0758 */
/* USB_INTEREPE_DAT15 0x075C */
/* UBS_INTEREPF_CFG 0x0780 */
#define INTEP_EN_OFFSET			   0
#define INTEP_EN_BITS			   1
#define INTEP_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_EPNUM_OFFSET		   4
#define INTEP_EPNUM_BITS		   4
#define INTEP_EPNUM_MASK		   (((1 << 4) - 1) << 4)
#define INTEP_MAXPKT_OFFSET		   8
#define INTEP_MAXPKT_BITS		   8
#define INTEP_MAXPKT_MASK		   (((1 << 8) - 1) << 8)
/* USB_INTEREPF_CTL 0x0784 */
#define INT_BUF_EN_OFFSET		   0
#define INT_BUF_EN_BITS			   1
#define INT_BUF_EN_MASK			   (((1 << 1) - 1) << 0)
#define INTEP_STALL_OFFSET		   1
#define INTEP_STALL_BITS		   1
#define INTEP_STALL_MASK		   (((1 << 1) - 1) << 1)
#define INTEP_RESET_OFFSET		   2
#define INTEP_RESET_BITS		   1
#define INTEP_RESET_MASK		   (((1 << 1) - 1) << 2)
/* USB_INTEREPF_BC 0x0788 */
#define INT_BUF_TX_BC_OFFSET		   0
#define INT_BUF_TX_BC_BITS		   8
#define INT_BUF_TX_BC_MASK		   (((1 << 8) - 1) << 0)
/* USB_INTEREPF_TOGGLE_CTRL 0x078C */
#define INTERP_ITOGL_OFFSET		   0
#define INTERP_ITOGL_BITS		   1
#define INTERP_ITOGL_MASK		   (((1 << 1) - 1) << 0)
/* USB_INTEREPF_IRQ_EN 0x0790 */
#define IE_INTEP_IN_OFFSET		   0
#define IE_INTEP_IN_BITS		   1
#define IE_INTEP_IN_MASK		   (((1 << 1) - 1) << 0)
#define IE_INTEP_INT_OFFSET		   1
#define IE_INTEP_INT_BITS		   1
#define IE_INTEP_INT_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPF_IRQ_STATUS 0x0794 */
#define I_INTEP_INF_OFFSET		   0
#define I_INTEP_INF_BITS		   1
#define I_INTEP_INF_MASK		   (((1 << 1) - 1) << 0)
#define I_INTEP_INTF_OFFSET		   1
#define I_INTEP_INTF_BITS		   1
#define I_INTEP_INTF_MASK		   (((1 << 1) - 1) << 1)
/* USB_INTEREPF_DAT0 0x07A0 */
/* USB_INTEREPF_DAT1 0x07A4 */
/* USB_INTEREPF_DAT2 0x07A8 */
/* USB_INTEREPF_DAT3 0x07AC */
/* USB_INTEREPF_DAT4 0x07B0 */
/* USB_INTEREPF_DAT5 0x07B4 */
/* USB_INTEREPF_DAT6 0x07B8 */
/* USB_INTEREPF_DAT7 0x07BC */
/* USB_INTEREPF_DAT8 0x07C0 */
/* USB_INTEREPF_DAT9 0x07C4 */
/* USB_INTEREPF_DAT10 0x07C8 */
/* USB_INTEREPF_DAT11 0x07CC */
/* USB_INTEREPF_DAT12 0x07D0 */
/* USB_INTEREPF_DAT13 0x07D4 */
/* USB_INTEREPF_DAT14 0x07D8 */
/* USB_INTEREPF_DAT15 0x07DC */
/* USB_DPHY_CFG 0x0800 */
#define CLK60_NEGALIGN_OFFSET		   0
#define CLK60_NEGALIGN_BITS		   1
#define CLK60_NEGALIGN_MASK		   (((1 << 1) - 1) << 0)
#define LATE_DLLEN_OFFSET		   1
#define LATE_DLLEN_BITS			   1
#define LATE_DLLEN_MASK			   (((1 << 1) - 1) << 1)
#define FS_XCVR_POW_SAV_OFFSET		   2
#define FS_XCVR_POW_SAV_BITS		   1
#define FS_XCVR_POW_SAV_MASK		   (((1 << 1) - 1) << 2)
#define HS_ANA_TX_PWD_EN_OFFSET		   3
#define HS_ANA_TX_PWD_EN_BITS		   1
#define HS_ANA_TX_PWD_EN_MASK		   (((1 << 1) - 1) << 3)
#define HS_XMT_PWD_EN_OFFSET		   4
#define HS_XMT_PWD_EN_BITS		   1
#define HS_XMT_PWD_EN_MASK		   (((1 << 1) - 1) << 4)
#define CFG_RXACT_EARLY_OFFSET		   5
#define CFG_RXACT_EARLY_BITS		   1
#define CFG_RXACT_EARLY_MASK		   (((1 << 1) - 1) << 5)
#define FS_PHASE_SEL_OFFSET		   8
#define FS_PHASE_SEL_BITS		   4
#define FS_PHASE_SEL_MASK		   (((1 << 4) - 1) << 8)
#define CFG_EB_DEPTH_OFFSET		   12
#define CFG_EB_DEPTH_BITS		   4
#define CFG_EB_DEPTH_MASK		   (((1 << 4) - 1) << 12)
#define USB2_PHY_DEBUG_ADDR_OFFSET	   16
#define USB2_PHY_DEBUG_ADDR_BITS	   3
#define USB2_PHY_DEBUG_ADDR_MASK	   (((1 << 3) - 1) << 16)
/* USB_DPHYCFG1 0x0804 */
#define CFG_UTMI_TXDATA_OFFSET		   0
#define CFG_UTMI_TXDATA_BITS		   8
#define CFG_UTMI_TXDATA_MASK		   (((1 << 8) - 1) << 0)
#define CFG_CHECK_CHIRPK_OFFSET		   8
#define CFG_CHECK_CHIRPK_BITS		   1
#define CFG_CHECK_CHIRPK_MASK		   (((1 << 1) - 1) << 8)
#define CFG_FORCE_UTMI_OFFSET		   9
#define CFG_FORCE_UTMI_BITS		   1
#define CFG_FORCE_UTMI_MASK		   (((1 << 1) - 1) << 9)
#define CFG_UTMI_NSUSPND_OFFSET		   10
#define CFG_UTMI_NSUSPND_BITS		   1
#define CFG_UTMI_NSUSPND_MASK		   (((1 << 1) - 1) << 10)
#define CFG_UTMI_OPMODE_OFFSET		   11
#define CFG_UTMI_OPMODE_BITS		   2
#define CFG_UTMI_OPMODE_MASK		   (((1 << 2) - 1) << 11)
#define CFG_UTMI_XCVRSEL_OFFSET		   13
#define CFG_UTMI_XCVRSEL_BITS		   2
#define CFG_UTMI_XCVRSEL_MASK		   (((1 << 2) - 1) << 13)
#define CFG_UTMI_TERMSEL_OFFSET		   15
#define CFG_UTMI_TERMSEL_BITS		   1
#define CFG_UTMI_TERMSEL_MASK		   (((1 << 1) - 1) << 15)
#define CFG_UTMI_FSLSSERIALMODE_OFFSET	   16
#define CFG_UTMI_FSLSSERIALMODE_BITS	   1
#define CFG_UTMI_FSLSSERIALMODE_MASK	   (((1 << 1) - 1) << 16)
#define CFG_UTMI_TXVALID_OFFSET		   17
#define CFG_UTMI_TXVALID_BITS		   1
#define CFG_UTMI_TXVALID_MASK		   (((1 << 1) - 1) << 17)
#define DISCON_ENABLE_OFFSET		   18
#define DISCON_ENABLE_BITS		   1
#define DISCON_ENABLE_MASK		   (((1 << 1) - 1) << 18)
#define CFG_EB_CHECK_SYNC_OFFSET	   19
#define CFG_EB_CHECK_SYNC_BITS		   1
#define CFG_EB_CHECK_SYNC_MASK		   (((1 << 1) - 1) << 19)
#define SLB_EN_OTG_OFFSET		   20
#define SLB_EN_OTG_BITS			   1
#define SLB_EN_OTG_MASK			   (((1 << 1) - 1) << 20)
#define SLB_RST_OTG_OFFSET		   21
#define SLB_RST_OTG_BITS		   1
#define SLB_RST_OTG_MASK		   (((1 << 1) - 1) << 21)
#define SLB_PSL_OTG_OFFSET		   22
#define SLB_PSL_OTG_BITS		   2
#define SLB_PSL_OTG_MASK		   (((1 << 2) - 1) << 22)
#define SLB_SEED_OTG_OFFSET		   24
#define SLB_SEED_OTG_BITS		   8
#define SLB_SEED_OTG_MASK		   (((1 << 8) - 1) << 24)
/* USB_DPHYCFG2 0x0808 */
#define VBUS_DOWN_INTR_OFFSET		   0
#define VBUS_DOWN_INTR_BITS		   1
#define VBUS_DOWN_INTR_MASK		   (((1 << 1) - 1) << 0)
#define VBUS_ON_INTR_OFFSET		   1
#define VBUS_ON_INTR_BITS		   1
#define VBUS_ON_INTR_MASK		   (((1 << 1) - 1) << 1)
#define VBUS_INT_EN_OFFSET		   2
#define VBUS_INT_EN_BITS		   1
#define VBUS_INT_EN_MASK		   (((1 << 1) - 1) << 2)
#define CFG_VBUS_DEGLITCH_TIME_OFFSET	   3
#define CFG_VBUS_DEGLITCH_TIME_BITS	   8
#define CFG_VBUS_DEGLITCH_TIME_MASK	   (((1 << 8) - 1) << 3)
#define SS_SCALEDOWN_MODE_OFFSET	   11
#define SS_SCALEDOWN_MODE_BITS		   2
#define SS_SCALEDOWN_MODE_MASK		   (((1 << 2) - 1) << 11)
#define CFG_EP0_INTOKEN_TEST_OFFSET	   13
#define CFG_EP0_INTOKEN_TEST_BITS	   1
#define CFG_EP0_INTOKEN_TEST_MASK	   (((1 << 1) - 1) << 13)
#define USB_DEVICE_RESUME_GLITCH_OFFSET	   14
#define USB_DEVICE_RESUME_GLITCH_BITS	   1
#define USB_DEVICE_RESUME_GLITCH_MASK	   (((1 << 1) - 1) << 14)
#define USB_DEVICE_DEBUG_SEL_OFFSET	   16
#define USB_DEVICE_DEBUG_SEL_BITS	   5
#define USB_DEVICE_DEBUG_SEL_MASK	   (((1 << 5) - 1) << 16)
/* USB_DPHY_STS 0x080C */
#define SLB_DONE_OTG_OFFSET		   0
#define SLB_DONE_OTG_BITS		   1
#define SLB_DONE_OTG_MASK		   (((1 << 1) - 1) << 0)
#define SLB_FAIL_OTG_OFFSET		   1
#define SLB_FAIL_OTG_BITS		   1
#define SLB_FAIL_OTG_MASK		   (((1 << 1) - 1) << 1)
#define UTMI_STS_OFFSET			   2
#define UTMI_STS_BITS			   8
#define UTMI_STS_MASK			   (((1 << 8) - 1) << 2)
#define CKUSABLE_OFFSET			   10
#define CKUSABLE_BITS			   1
#define CKUSABLE_MASK			   (((1 << 1) - 1) << 10)

#endif /* __DRIVERS_USB_REALTEK_REGS_H__ */
