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

struct iomatrix_uapi_write_req {
	__u32 base_reg; /* start address for linear write */
	__u32 size; /* bytes to write (e.g., up to 128KB) */
	__u32 flags; /* bit0: addr_autoinc (reserved; bus layer auto-increments) */
	__u32 uptr; /* user-space pointer to image buffer */
};

enum iomatrix_erase_type {
	IOMATRIX_ERASE_4K = 0, /* 4KB block */
	IOMATRIX_ERASE_32K = 1, /* 32KB block */
	IOMATRIX_ERASE_64K = 2, /* 64KB block */
};

struct iomatrix_uapi_erase_req {
	__u32 addr; /* start address (must be aligned to block size) */
	__u32 len; /* total bytes to erase (must be multiple of block size) */
	__u32 type; /* erase block type: 4K/32K/64K */
};

/* Write firmware image: arg points to struct iomatrix_uapi_write_req */
#define IOMATRIX_IOC_WRITE \
	_IOW(IOMATRIX_IOC_MAGIC, 0x01, struct iomatrix_uapi_write_req)

/* FSPI block erase: arg points to struct iomatrix_uapi_erase_req */
#define IOMATRIX_IOC_ERASE \
	_IOW(IOMATRIX_IOC_MAGIC, 0x02, struct iomatrix_uapi_erase_req)

#endif /* _UAPI_IOMATRIX_IOCTL_H_ */
