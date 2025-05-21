/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note */
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

#ifndef _UAPI__RTS493XA_TIMER__
#define _UAPI__RTS493XA_TIMER__

#define RTS_TIMER_GO	  _IO('h', 0x01)
#define RTS_TIMER_STOP	  _IO('h', 0x02)
#define RTS_TIMER_EPI	  _IO('h', 0x03) /* enable periodic */
#define RTS_TIMER_DPI	  _IO('h', 0x04) /* disable periodic */
#define RTS_TIMER_IRQFREQ _IOW('h', 0x5, unsigned long) /* IRQ FREQ */

#endif /* _UAPI__RTS493XA_TIMER__ */
