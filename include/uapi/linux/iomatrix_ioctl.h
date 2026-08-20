/* SPDX-License-Identifier: GPL-2.0-or-later WITH Linux-syscall-note*/
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

#ifndef _UAPI_IOMATRIX_IOCTL_H_
#define _UAPI_IOMATRIX_IOCTL_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define IOMATRIX_IOC_MAGIC 0x44

struct iomatrix_uapi_rw_req {
	__u32 addr; /* register/memory address to write or read */
	__u32 val; /* value to write; for read, val will be filled by kernel */
};

/* Read a value: arg points to struct iomatrix_uapi_rw_req (addr in, val out) */
#define IOMATRIX_IOC_READ_MEM \
	_IOWR(IOMATRIX_IOC_MAGIC, 0x03, struct iomatrix_uapi_rw_req)

/* Write a value: arg points to struct iomatrix_uapi_rw_req (addr/val in) */
#define IOMATRIX_IOC_WRITE_MEM \
	_IOW(IOMATRIX_IOC_MAGIC, 0x04, struct iomatrix_uapi_rw_req)

#endif /* _UAPI_IOMATRIX_IOCTL_H_ */
