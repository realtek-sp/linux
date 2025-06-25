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
#include <linux/scatterlist.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/internal/aead.h>
#include <crypto/scatterwalk.h>
#include <crypto/skcipher.h>
#include <crypto/internal/skcipher.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include "../crypto/internal.h"

#include "rts_crypto.h"

#ifdef CIPHER_DEBUG
#define buf_table_printf(table)                                     \
	do {                                                        \
		int i;                                              \
		pr_info("buf table printf:\n");                     \
		for (i = 0; i < (table).cur; i++)                   \
			pr_info("0x%08x\n", *((table).v_addr + i)); \
	} while (0)

#define sg_printf(sg)                                                \
	do {                                                         \
		int i;                                               \
		pr_info("sg printf:\n");                             \
		for (i = 0; i < (sg)->length; i++)                   \
			pr_info("%02x", *((u8 *)sg_virt((sg)) + i)); \
	} while (0)

#define dump_regs(s, n)                                        \
	do {                                                   \
		int i;                                         \
		pr_info("dump regs:\n");                       \
		for (i = (s) * 4; i < ((s) + (n)) * 4; i += 4) \
			pr_info(" 0xb860%04x= 0x%08x\n", i,    \
				readl(0xb8600000 + i));        \
	} while (0)
#else
#define buf_table_printf(table)
#define sg_printf(sg)
#define dump_regs(s, n)
#endif

#define FLAGS_ENCRYPT BIT(0)
#define FLAGS_DECRYPT BIT(1)
#define FLAGS_AES     BIT(2)
#define FLAGS_DES     BIT(3)
#define FLAGS_DES3    BIT(4)
#define FLAGS_ECB     BIT(5)
#define FLAGS_CBC     BIT(6)
#define FLAGS_CBCCS1  BIT(7)
#define FLAGS_CBCCS2  BIT(8)
#define FLAGS_CBCCS3  BIT(9)
#define FLAGS_CTR     BIT(10)
#define FLAGS_CCM     BIT(11)
#define FLAGS_GCM     BIT(12)

#define CIPHER_STS_ERR 0x0
#define CIPHER_STS_OK  0x1

#define RLX_DMA_TABLE_SIZE 256
#define RLX_KEY_LENGTH	   32

#define RTS_CIPHER_IOC_MAGIC	     0x81
#define RTS_CIPHER_IOC_SET_NORMALKEY _IO(RTS_CIPHER_IOC_MAGIC, 0xA0)
#define RTS_CIPHER_IOC_SET_EFUSEKEY  _IO(RTS_CIPHER_IOC_MAGIC, 0xA1)

#define set_cap(caps, index) ((caps) |= (1u << (index)))
#define get_cap(caps, index) ((caps) & (1u << (index)))

#define IRQ_LIMIT	  64
#define AEAD_IRQ_LIMIT	  16
#define AES_GENERIC_LIMIT 64

enum {
	ALG_AES = 0,
	ALG_DES = 1,
	ALG_DES3 = 2,
	ALG_COUNT,
};

enum {
	MODE_ECB = 0,
	MODE_CBC = 1,
	MODE_CBCCS1 = 2,
	MODE_CBCCS2 = 3,
	MODE_CBCCS3 = 4,
	MODE_CTR = 5,
	MODE_COUNT,
};

enum {
	AEAD_AES_GCM = 0,
	AEAD_AES_CCM = 1,
	AEAD_COUNT,
};

struct buf_table_mapinfo {
	struct scatterlist *sg;
	unsigned int nents;
	enum dma_data_direction dir;
	struct list_head list;
};

struct sg_map {
	struct scatterlist sg;
	struct buf_table_mapinfo mapinfo;
};

struct rts_buf_table {
	u32 *v_addr;
	dma_addr_t dma_addr;
	unsigned int cur;
	struct list_head mapinfo;
};

struct rts_crypto_data {
	struct platform_device *pdev;
	struct cdev cdev;
	void __iomem *addr;
	unsigned long size;
	u32 base;
	int irq;
	struct reset_control *rst;
	struct reset_control *sd;
	struct mutex dma_mutex;
	struct completion crypto_complete;
	struct clk *cipher_clk;
	unsigned int ekey;
	unsigned int mode_cap[ALG_COUNT];
	unsigned int aead_cap;
};

struct rts_crypto_ctx {
	struct crypto_skcipher *tfm;
	u8 key[RLX_KEY_LENGTH];
	unsigned int keylen;
	u8 *iv;
	int ivflag;
	unsigned int ivlen;
	unsigned int mask;
	unsigned int ekey;
	unsigned int ekey_idx;
	struct rts_buf_table table_in;
	struct rts_buf_table table_out;
	struct buf_table_mapinfo mapinfo[2];
};

struct rts_aead_reqctx {
	u8 a_tag[AES_BLOCK_SIZE];
	u8 ia_tag[AES_BLOCK_SIZE];
	struct sg_map sg_iv;
	struct scatterlist sg_in[2];
	struct scatterlist sg_out[2];
	struct sg_map sg_a_f;
	struct sg_map sg_align;
	struct sg_map sg_align_p;
	u8 b0[16];
	u8 a_f[6];
	u8 align[16]; //align ad
	u8 align_p[16]; //align payload
	struct buf_table_mapinfo ad_mapinfo;
};

#ifdef CONFIG_RTS_OTP
extern int rts_otp_load_aes_key(unsigned int k);
#else
static inline int rts_otp_load_aes_key(unsigned int k)
{
	return -ENODEV;
}
#endif

static struct rts_crypto_data *rts_cdata;
static dev_t devno = MKDEV(124, 0);

static unsigned int rts_crypto_read(struct rts_crypto_data *cdata,
				    unsigned int reg)
{
	return readl(cdata->addr + reg);
}

static int rts_crypto_write(struct rts_crypto_data *cdata, unsigned int reg,
			    unsigned int value)
{
	writel(value, cdata->addr + reg);

	return 0;
}

static int rts_sg_nents(struct scatterlist *sg, unsigned int nbytes,
			unsigned int *offset)
{
	int nents;

	for (nents = 0; sg && nbytes >= sg->length; sg = sg_next(sg)) {
		++nents;
		nbytes -= sg->length;
	}

	if (nbytes) {
		if (!sg)
			return -EINVAL;

		nents++;
	}

	if (offset)
		*offset = nbytes;

	return nents;
}

static void rts_sg_copy(struct scatterlist *dst, struct scatterlist *src,
			unsigned int nbytes)
{
	int in_nents, out_nents;
	void *buf = NULL;

	in_nents = sg_nents_for_len(src, nbytes);
	out_nents = sg_nents_for_len(dst, nbytes);
	buf = kmalloc(nbytes, GFP_KERNEL);
	sg_copy_to_buffer(src, in_nents, buf, nbytes);
	sg_copy_from_buffer(dst, out_nents, buf, nbytes);
	kfree(buf);
	buf = NULL;
}

static int buf_table_init(struct rts_crypto_data *cdata,
			  struct rts_buf_table *table)
{
	if (!cdata || !table)
		return -EINVAL;

	memset(table, 0, sizeof(struct rts_buf_table));

	if (!table->v_addr)
		table->v_addr = dma_alloc_coherent(&cdata->pdev->dev,
						   RLX_DMA_TABLE_SIZE,
						   &table->dma_addr,
						   GFP_KERNEL);
	if (!table->v_addr) {
		dev_err(&cdata->pdev->dev,
			"Unable to allocate dma table in/out buffer\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&table->mapinfo);

	return 0;
}

static inline void __buf_table_map(struct rts_buf_table *table,
				   dma_addr_t dma_addr, unsigned int dma_len)
{
	if (table && table->v_addr) {
		*(table->v_addr + table->cur) = (u32)dma_addr;
		*(table->v_addr + table->cur + 1) = dma_len;
		table->cur += 2;
	}
}

static int buf_table_map_sg(struct rts_crypto_data *cdata,
			    struct rts_buf_table *table,
			    struct buf_table_mapinfo *mapinfo,
			    struct scatterlist *sg, unsigned int nbytes,
			    enum dma_data_direction dir)
{
	int maps = 0, i;
	struct scatterlist *tmp;
	dma_addr_t d_addr;
	unsigned int d_len, offset;

	if (!cdata || !table || !mapinfo || !sg)
		return -EINVAL;

	/* hw requirement*/
	if (!nbytes) {
		__buf_table_map(table, 0, 0);
		return 0;
	}

	mapinfo->sg = sg;
	mapinfo->nents = rts_sg_nents(sg, nbytes, &offset);
	mapinfo->dir = dir;

	maps = dma_map_sg(&cdata->pdev->dev, mapinfo->sg, mapinfo->nents, dir);
	if (!maps) {
		maps = -EIO;
		return maps;
	}

	for_each_sg(mapinfo->sg, tmp, maps, i) {
		d_addr = sg_dma_address(tmp);
		d_len = sg_dma_len(tmp);

		if ((i == maps - 1) && offset) {
			d_len = offset;
			dev_dbg(&cdata->pdev->dev, "d_len = %d\n", d_len);
		}

		__buf_table_map(table, d_addr, d_len);
	}

	list_add(&mapinfo->list, &table->mapinfo);

	return maps;
}

static void buf_table_unmap_sg(struct rts_crypto_data *cdata,
			       struct rts_buf_table *table)
{
	struct buf_table_mapinfo *mapinfo = NULL, *next = NULL;

	if (!cdata || !table)
		return;

	list_for_each_entry_safe(mapinfo, next, &table->mapinfo, list) {
		dma_unmap_sg(&cdata->pdev->dev, mapinfo->sg, mapinfo->nents,
			     mapinfo->dir);
		list_del(&mapinfo->list);
	}

	table->cur = 0;
}

static void buf_table_uninit(struct rts_crypto_data *cdata,
			     struct rts_buf_table *table)
{
	buf_table_unmap_sg(cdata, table);

	if (cdata && table && table->v_addr) {
		dma_free_coherent(&cdata->pdev->dev, RLX_DMA_TABLE_SIZE,
				  table->v_addr, table->dma_addr);
		table->v_addr = NULL;
	}
}

static inline void rts_crypto_init(struct rts_crypto_data *cdata, bool irq)
{
	clk_prepare_enable(cdata->cipher_clk);
	rts_crypto_write(cdata, RLX_REG_CIPHER_INT_FLAG, 0x5);
	rts_crypto_write(cdata, RLX_REG_CIPHER_INT_EN, irq ? 0x5 : 0x0);
}

static inline void rts_crypto_uninit(struct rts_crypto_data *cdata)
{
	rts_crypto_write(cdata, RLX_REG_CIPHER_INT_EN, 0x0);
	clk_disable(cdata->cipher_clk);
}

static inline int rts_crypto_done(struct rts_crypto_data *cdata)
{
	return rts_crypto_read(cdata, RLX_REG_CIPHER_INT_FLAG) & 0x01;
}

/* init CTL */
static int rts_crypto_set_ctl(struct rts_crypto_data *cdata,
			      struct rts_crypto_ctx *ctx)
{
	u32 val;

	if (!cdata || !ctx)
		return -EINVAL;

	val = rts_crypto_read(cdata, RLX_REG_CIPHER_CTL);
	val = val & 0xFFFFF007;

	/* key mode */
	if (ctx->mask & FLAGS_AES) {
		val = val | ((u32)0x0 << RLX_ALGORITHM_SEL);

		if (ctx->keylen == 16)
			val = val | ((u32)0x0 << RLX_AES_MODE_SEL);
		else if (ctx->keylen == 24)
			val = val | ((u32)0x1 << RLX_AES_MODE_SEL);
		else if (ctx->keylen == 32)
			val = val | ((u32)0x2 << RLX_AES_MODE_SEL);
		else
			return -EINVAL;
	} else if (ctx->mask & FLAGS_DES) {
		val = val | ((u32)0x1 << RLX_ALGORITHM_SEL);
	} else if (ctx->mask & FLAGS_DES3) {
		val = val | ((u32)0x2 << RLX_ALGORITHM_SEL);
	} else {
		return -EINVAL;
	}

	/* op mode */
	if (ctx->mask & FLAGS_ECB)
		val = val | ((u32)0x0 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CBC)
		val = val | ((u32)0x1 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CBCCS1)
		val = val | ((u32)0x2 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CBCCS2)
		val = val | ((u32)0x3 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CBCCS3)
		val = val | ((u32)0x4 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CTR)
		val = val | ((u32)0x5 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_CCM)
		val = val | ((u32)0x6 << RLX_OPERATION_MODE);
	else if (ctx->mask & FLAGS_GCM)
		val = val | ((u32)0x7 << RLX_OPERATION_MODE);
	else
		return -EINVAL;

	/* padding mode*/
	val = val | ((u32)0x0 << RLX_PADDING_SEL);

	/* en/decrypt mode*/
	if (ctx->mask & FLAGS_ENCRYPT)
		val = val | ((u32)0x0 << RLX_ENCTYPT_DECTYPT_SEL);
	else if (ctx->mask & FLAGS_DECRYPT)
		val = val | ((u32)0x1 << RLX_ENCTYPT_DECTYPT_SEL);
	else
		return -EINVAL;

	return rts_crypto_write(cdata, RLX_REG_CIPHER_CTL, val);
}

static int rts_crypto_set_key(struct rts_crypto_data *cdata,
			      struct rts_crypto_ctx *ctx)
{
	u32 *reg_val;
	int ret;

	if (!cdata || !ctx)
		return -EINVAL;

	if (!cdata->ekey && !ctx->ekey) {
		dev_dbg(&cdata->pdev->dev, "rts normal key\n");
		rts_crypto_write(cdata, RLX_REG_CIPHER_KEY, 0);

		reg_val = (u32 *)ctx->key;
		rts_crypto_write(cdata, RLX_REG_KEY_DATA0,
				 cpu_to_be32(reg_val[0]));
		rts_crypto_write(cdata, RLX_REG_KEY_DATA1,
				 cpu_to_be32(reg_val[1]));
		if (ctx->keylen > 8) {
			rts_crypto_write(cdata, RLX_REG_KEY_DATA2,
					 cpu_to_be32(reg_val[2]));
			rts_crypto_write(cdata, RLX_REG_KEY_DATA3,
					 cpu_to_be32(reg_val[3]));
		}
		if (ctx->keylen > 16) {
			rts_crypto_write(cdata, RLX_REG_KEY_DATA4,
					 cpu_to_be32(reg_val[4]));
			rts_crypto_write(cdata, RLX_REG_KEY_DATA5,
					 cpu_to_be32(reg_val[5]));
		}
		if (ctx->keylen > 24) {
			rts_crypto_write(cdata, RLX_REG_KEY_DATA6,
					 cpu_to_be32(reg_val[6]));
			rts_crypto_write(cdata, RLX_REG_KEY_DATA7,
					 cpu_to_be32(reg_val[7]));
		}
	} else { /* KEYMODE_EFUSE */
		dev_dbg(&cdata->pdev->dev, "rts efuse key\n");
		if (cdata->ekey) {
			rts_crypto_write(cdata, RLX_REG_CIPHER_KEY, 1);
		} else {
			ret = rts_otp_load_aes_key(ctx->ekey_idx);
			if (ret) {
				dev_err(&cdata->pdev->dev,
					"load otp aes key failed\n");
				return ret;
			}

			rts_crypto_write(cdata, RLX_REG_CIPHER_KEY, ctx->ekey);
		}
	}

	return 0;
}

static inline void rts_crypto_clear_iv(struct rts_crypto_data *cdata)
{
	rts_crypto_write(cdata, RLX_REG_IV_IN_DATA0, 0);
	rts_crypto_write(cdata, RLX_REG_IV_IN_DATA1, 0);
	rts_crypto_write(cdata, RLX_REG_IV_IN_DATA2, 0);
	rts_crypto_write(cdata, RLX_REG_IV_IN_DATA3, 0);
}

static int rts_crypto_set_iv(struct rts_crypto_data *cdata,
			     struct rts_crypto_ctx *ctx)
{
	u32 *reg_val;

	if (!cdata || !ctx)
		return -EINVAL;

	reg_val = (u32 *)ctx->iv;
	if (ctx->ivlen > 0) {
		rts_crypto_write(cdata, RLX_REG_IV_IN_DATA0,
				 cpu_to_be32(reg_val[0]));
		rts_crypto_write(cdata, RLX_REG_IV_IN_DATA1,
				 cpu_to_be32(reg_val[1]));
	}
	if (ctx->ivlen > 8) {
		rts_crypto_write(cdata, RLX_REG_IV_IN_DATA2,
				 cpu_to_be32(reg_val[2]));
		rts_crypto_write(cdata, RLX_REG_IV_IN_DATA3,
				 cpu_to_be32(reg_val[3]));
	}

	return 0;
}

static void rts_crypto_get_iv(struct rts_crypto_data *cdata,
			      struct rts_crypto_ctx *ctx)
{
	u32 *reg_val;

	if (cdata && ctx) {
		reg_val = (u32 *)ctx->iv;
		if (ctx->ivlen > 0) {
			reg_val[0] = be32_to_cpu(
				rts_crypto_read(cdata, RLX_REG_IV_OUT_DATA0));
			reg_val[1] = be32_to_cpu(
				rts_crypto_read(cdata, RLX_REG_IV_OUT_DATA1));
		}
		if (ctx->ivlen > 8) {
			reg_val[2] = be32_to_cpu(
				rts_crypto_read(cdata, RLX_REG_IV_OUT_DATA2));
			reg_val[3] = be32_to_cpu(
				rts_crypto_read(cdata, RLX_REG_IV_OUT_DATA3));
		}
	}
}

static void rts_crypto_get_atag(struct rts_crypto_data *cdata,
				struct rts_aead_reqctx *reqctx)
{
	u32 *reg_val;

	if (cdata && reqctx) {
		reg_val = (u32 *)reqctx->a_tag;
		reg_val[0] = be32_to_cpu(
			rts_crypto_read(cdata, RLX_REG_MAC_OUT_DATA0));
		reg_val[1] = be32_to_cpu(
			rts_crypto_read(cdata, RLX_REG_MAC_OUT_DATA1));
		reg_val[2] = be32_to_cpu(
			rts_crypto_read(cdata, RLX_REG_MAC_OUT_DATA2));
		reg_val[3] = be32_to_cpu(
			rts_crypto_read(cdata, RLX_REG_MAC_OUT_DATA3));
	}

	dev_dbg(&rts_cdata->pdev->dev, "%08x%08x%08x%08x\n", reg_val[0],
		reg_val[1], reg_val[2], reg_val[3]);
}

static inline void rts_crypto_buf_table_init(struct rts_crypto_data *cdata,
					     struct rts_crypto_ctx *ctx)
{
	rts_crypto_write(cdata, RLX_REG_IN_TABLE_ADDR, ctx->table_in.dma_addr);
	rts_crypto_write(cdata, RLX_REG_OUT_TABLE_ADDR,
			 ctx->table_out.dma_addr);
}

static int rts_blkcipher_map_in_data(struct rts_crypto_data *cdata,
				     struct rts_crypto_ctx *ctx,
				     struct scatterlist *src,
				     unsigned int nbytes)
{
	int maps = 0;

	maps = buf_table_map_sg(cdata, &ctx->table_in, &ctx->mapinfo[0], src,
				nbytes, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_IN_BUF_NUM, maps);
	rts_crypto_write(cdata, RLX_REG_DATA_IN_LENGTH, nbytes);

	/* debug */
	buf_table_printf(ctx->table_in);

	return maps;
}

static int rts_blkcipher_map_out_data(struct rts_crypto_data *cdata,
				      struct rts_crypto_ctx *ctx,
				      struct scatterlist *dst,
				      unsigned int nbytes)
{
	int maps = 0;

	maps = buf_table_map_sg(cdata, &ctx->table_out, &ctx->mapinfo[1], dst,
				nbytes, DMA_FROM_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_OUT_BUF_NUM, maps);

	/* debug */
	buf_table_printf(ctx->table_out);

	return maps;
}

static int rts_gcm_map_in_data(struct rts_crypto_data *cdata,
			       struct rts_crypto_ctx *ctx,
			       struct aead_request *req,
			       struct rts_aead_reqctx *reqctx)
{
	int maps = 0;
	struct scatterlist *sg;

	dev_dbg(&cdata->pdev->dev, "map in data,= %d\n", req->cryptlen);

	sg = scatterwalk_ffwd(reqctx->sg_in, req->src, req->assoclen);

	maps = buf_table_map_sg(cdata, &ctx->table_in, &ctx->mapinfo[0], sg,
				req->cryptlen, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_IN_BUF_NUM, maps);
	rts_crypto_write(cdata, RLX_REG_DATA_IN_LENGTH, req->cryptlen);

	dev_dbg(&cdata->pdev->dev, "map in data, maps = %d, len = %d\n", maps,
		req->cryptlen);

	/* debug */
	buf_table_printf(ctx->table_in);

	return maps;
}

static int rts_ccm_map_in_data(struct rts_crypto_data *cdata,
			       struct rts_crypto_ctx *ctx,
			       struct aead_request *req,
			       struct rts_aead_reqctx *reqctx)
{
	int maps = 0, maps_all;
	struct scatterlist *sg;
	unsigned int len, align_len;

	dev_dbg(&cdata->pdev->dev, "map in data,= %d\n", req->cryptlen);

	sg = scatterwalk_ffwd(reqctx->sg_in, req->src, req->assoclen);

	maps = buf_table_map_sg(cdata, &ctx->table_in, &ctx->mapinfo[0], sg,
				req->cryptlen, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	len = req->cryptlen;
	maps_all = maps;

	/* payload align*/
	align_len = ALIGN(len, 16) - len;
	if (align_len) {
		dev_dbg(&cdata->pdev->dev, "map in align len = %d\n",
			align_len);

		memset(reqctx->align_p, 0, sizeof(reqctx->align_p));

		sg = &reqctx->sg_align_p.sg;
		sg_init_one(sg, reqctx->align_p, align_len);

		maps = buf_table_map_sg(cdata, &ctx->table_in,
					&reqctx->sg_align_p.mapinfo, sg,
					align_len, DMA_TO_DEVICE);
		if (maps < 0)
			return maps;

		maps_all += maps;
		len += align_len;
	}

	rts_crypto_write(cdata, RLX_REG_IN_BUF_NUM, maps_all);
	rts_crypto_write(cdata, RLX_REG_DATA_IN_LENGTH, len);

	dev_dbg(&cdata->pdev->dev, "map in data, maps = %d, len = %d\n",
		maps_all, len);

	/* debug */
	buf_table_printf(ctx->table_in);

	return maps;
}

static int rts_aead_map_out_data(struct rts_crypto_data *cdata,
				 struct rts_crypto_ctx *ctx,
				 struct aead_request *req,
				 struct rts_aead_reqctx *reqctx)
{
	int maps = 0;
	struct scatterlist *sg;

	dev_dbg(&cdata->pdev->dev, "map out data,= %d\n", req->cryptlen);

	sg = scatterwalk_ffwd(reqctx->sg_out, req->dst, req->assoclen);

	maps = buf_table_map_sg(cdata, &ctx->table_out, &ctx->mapinfo[1], sg,
				req->cryptlen, DMA_FROM_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_OUT_BUF_NUM, maps);

	dev_dbg(&cdata->pdev->dev, "map out data, maps = %d\n", maps);

	/* debug */
	buf_table_printf(ctx->table_out);

	return maps;
}

static inline int rts_ccm_check_iv(const u8 *iv)
{
	dev_dbg(&rts_cdata->pdev->dev, "iv[0] = %d\n", iv[0]);
	/* Note: rfc 3610 and NIST 800-38C require */
	/* 2 <= L <= 8, so 1 <= L' <= 7. */
	if (iv[0] < 1 || iv[0] > 7)
		return -EINVAL;

	return 0;
}

static int set_msg_len(u8 *block, unsigned int msglen, int csize)
{
	__be32 data;

	memset(block, 0, csize);
	block += csize;

	if (csize >= 4)
		csize = 4;
	else if (msglen > (1 << (8 * csize)))
		return -EOVERFLOW;

	data = cpu_to_be32(msglen);
	memcpy(block - csize, (u8 *)&data + 4 - csize, csize);

	return 0;
}

static int format_b0(u8 *info, struct aead_request *req, unsigned int cryptlen)
{
	struct crypto_aead *aead = crypto_aead_reqtfm(req);
	unsigned int lp = req->iv[0];
	unsigned int l = lp + 1;
	unsigned int m;

	m = crypto_aead_authsize(aead);

	memcpy(info, req->iv, 16);

	/* format control info per RFC 3610 and
	 * NIST Special Publication 800-38C
	 */
	*info |= lp; /* bit 0-2 */
	*info |= (8 * ((m - 2) / 2)); /* bit 3-5 */
	if (req->assoclen)
		*info |= 64; /* bit 6*/

	return set_msg_len(info + 16 - l, cryptlen, l);
}

static int format_adata(u8 *adata, unsigned int a)
{
	int len = 0;

	/* add control info for associated data
	 * RFC 3610 and NIST Special Publication 800-38C
	 */
	if (a < 65280) {
		*(__be16 *)adata = cpu_to_be16(a);
		len = 2;
	} else {
		*(__be16 *)adata = cpu_to_be16(0xfffe);
		*(__be32 *)&adata[2] = cpu_to_be32(a);
		len = 6;
	}

	return len;
}

static int rts_ccm_map_nonce_ad(struct rts_crypto_data *cdata,
				struct rts_crypto_ctx *ctx,
				struct aead_request *req,
				struct rts_aead_reqctx *reqctx)
{
	int ret, maps, maps_all;
	unsigned int n, len, a_f_len, align_len;
	struct scatterlist *sg = &reqctx->sg_iv.sg;

	/* Note: rfc 3610 and NIST 800-38C require */
	n = 15 - req->iv[0] - 1;

	dev_dbg(&cdata->pdev->dev, "map in nonce len = %d\n", n);

	/* b0 */
	ret = format_b0(reqctx->b0, req, req->cryptlen);
	if (ret) {
		dev_err(&cdata->pdev->dev, "format b0 err = %d\n", ret);
		return ret;
	}

	sg_init_one(sg, reqctx->b0, 16);

	maps = buf_table_map_sg(cdata, &ctx->table_in, &reqctx->sg_iv.mapinfo,
				sg, 16, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	maps_all = maps;
	len = 16;
	rts_crypto_write(cdata, RLX_REG_CCM_N_GCM_IV_LENGTH, n);

	/* in ad */
	if (req->src && req->assoclen) {
		sg = &reqctx->sg_a_f.sg;
		a_f_len = format_adata(reqctx->a_f, req->assoclen);

		dev_dbg(&cdata->pdev->dev,
			"map in a format len = %d, assoclen = %d\n", a_f_len,
			req->assoclen);

		sg_init_one(sg, reqctx->a_f, a_f_len);

		maps = buf_table_map_sg(cdata, &ctx->table_in,
					&reqctx->sg_a_f.mapinfo, sg, a_f_len,
					DMA_TO_DEVICE);
		if (maps < 0)
			return maps;

		maps_all += maps;

		maps = buf_table_map_sg(cdata, &ctx->table_in,
					&reqctx->ad_mapinfo, req->src,
					req->assoclen, DMA_TO_DEVICE);
		if (maps < 0)
			return maps;

		maps_all += maps;
		len += (a_f_len + req->assoclen);
	}

	/* ad align*/
	align_len = ALIGN(len, 16) - len;
	if (align_len) {
		dev_dbg(&cdata->pdev->dev, "map in align len = %d\n",
			align_len);

		memset(reqctx->align, 0, sizeof(reqctx->align));

		sg = &reqctx->sg_align.sg;
		sg_init_one(sg, reqctx->align, align_len);

		maps = buf_table_map_sg(cdata, &ctx->table_in,
					&reqctx->sg_align.mapinfo, sg,
					align_len, DMA_TO_DEVICE);
		if (maps < 0)
			return maps;

		maps_all += maps;
		len += align_len;
	}

	rts_crypto_write(cdata, RLX_REG_CCM_NA_GCM_A_LENGTH, len);
	rts_crypto_write(cdata, RLX_REG_CCM_NA_GCM_A_BUF_NUM, maps_all);

	return maps_all;
}

static inline void rts_ccm_set_ctr_flag(struct rts_crypto_data *cdata,
					struct aead_request *req)
{
	unsigned int flag = 0;

	/* Note: rfc 3610 and NIST 800-38C require */
	flag |= (req->iv[0] & 0x7);

	rts_crypto_write(cdata, RLX_REG_CCM_CTR_FLAG, flag);
}

static int rts_gcm_map_iv(struct rts_crypto_data *cdata,
			  struct rts_crypto_ctx *ctx, struct aead_request *req,
			  struct rts_aead_reqctx *reqctx)
{
	int maps = 0;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	unsigned int ivsize = crypto_aead_ivsize(tfm);
	struct scatterlist *sg = &reqctx->sg_iv.sg;

	dev_dbg(&cdata->pdev->dev, "map in iv = %d\n", ivsize);

	sg_init_one(sg, req->iv, ivsize);

	maps = buf_table_map_sg(cdata, &ctx->table_in, &reqctx->sg_iv.mapinfo,
				sg, ivsize, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_CCM_N_GCM_IV_LENGTH, ivsize);
	rts_crypto_write(cdata, RLX_REG_GCM_IV_BUF_NUM, maps);

	return maps;
}

static int rts_gcm_map_ad(struct rts_crypto_data *cdata,
			  struct rts_crypto_ctx *ctx, struct aead_request *req,
			  struct rts_aead_reqctx *reqctx)
{
	int maps = 0;

	dev_dbg(&cdata->pdev->dev, "map associated data = %d\n", req->assoclen);

	/* in */
	maps = buf_table_map_sg(cdata, &ctx->table_in, &reqctx->ad_mapinfo,
				req->src, req->assoclen, DMA_TO_DEVICE);
	if (maps < 0)
		return maps;

	rts_crypto_write(cdata, RLX_REG_CCM_NA_GCM_A_LENGTH, req->assoclen);
	rts_crypto_write(cdata, RLX_REG_CCM_NA_GCM_A_BUF_NUM, maps);

	return maps;
}

static void rts_aead_output_atag(struct rts_crypto_data *cdata,
				 struct aead_request *req,
				 struct rts_aead_reqctx *reqctx)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	unsigned int authsize = crypto_aead_authsize(tfm);

	dev_dbg(&cdata->pdev->dev, "output mac\n");

	scatterwalk_map_and_copy(reqctx->a_tag, req->dst,
				 req->assoclen + req->cryptlen, authsize, 1);
}

static int rts_aead_verify_atag(struct rts_crypto_data *cdata,
				struct aead_request *req,
				struct rts_aead_reqctx *reqctx)
{
	int ret;
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	unsigned int authsize = crypto_aead_authsize(tfm);

	dev_dbg(&cdata->pdev->dev, "verify mac\n");

	scatterwalk_map_and_copy(reqctx->ia_tag, req->src,
				 req->assoclen + req->cryptlen, authsize, 0);

	ret = crypto_memneq(reqctx->ia_tag, reqctx->a_tag, authsize);
	if (ret) {
		ret = -EBADMSG;
		dev_err(&cdata->pdev->dev,
			"the authentication of the ciphertext was unsuccessful!\n");
	}

	return ret;
}

static inline void rts_crypto_unmap_table(struct rts_crypto_data *cdata,
					  struct rts_crypto_ctx *ctx)
{
	buf_table_unmap_sg(cdata, &ctx->table_in);
	buf_table_unmap_sg(cdata, &ctx->table_out);
}

static inline int rts_crypto_get_status(struct rts_crypto_data *cdata)
{
	return rts_crypto_read(cdata, RLX_REG_CIPHER_STS);
}

static inline int rts_crypto_start_crypt(struct rts_crypto_data *cdata)
{
	u32 val = rts_crypto_read(cdata, RLX_REG_CIPHER_CTL);

	return rts_crypto_write(cdata, RLX_REG_CIPHER_CTL, val | 0x1);
}

static inline int rts_crypto_stop_crypt(struct rts_crypto_data *cdata)
{
	u32 val = rts_crypto_read(cdata, RLX_REG_CIPHER_CTL);

	return rts_crypto_write(cdata, RLX_REG_CIPHER_CTL, val | 0x2);
}

static int rts_setkey(struct crypto_skcipher *tfm, const u8 *key,
		      unsigned int len)
{
	int i = 0, ret = 0;
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	dev_dbg(&rts_cdata->pdev->dev, "rts setkey\n");

	/* efuse key: key[i] == 0, i ∈ [0, len -1)  */
	while (!key[i] && i < len - 1)
		i++;

	ctx->keylen = len;
	/* use efuse key */
	if (i == len - 1) {
		if (len == 16) {
			if (key[i] > 7)
				return -EINVAL;
			ctx->ekey = key[i] % 2 + 1;
			ctx->ekey_idx = key[i] / 2;
		} else if (len == 32) {
			if (key[i] > 3)
				return -EINVAL;
			ctx->ekey = 3;
			ctx->ekey_idx = key[i];
		} else {
			dev_err(&rts_cdata->pdev->dev,
				"efuse key format failed\n");
			return -EINVAL;
		}
		/* use normal key */
	} else {
		ctx->ekey = 0;
		memcpy(ctx->key, key, len);

		if (ctx->tfm)
			ret = crypto_skcipher_setkey(ctx->tfm, key, len);
	}

	dev_dbg(&rts_cdata->pdev->dev, "ekey=%d, ekey_idx=%d\n", ctx->ekey,
		ctx->ekey_idx);

	return ret;
}

static int rts_crypto_do(struct rts_crypto_data *cdata,
			 struct rts_crypto_ctx *ctx, bool irq)
{
	int ret = 0;
	u32 val;
	int t = 1000; //1ms

	dev_dbg(&cdata->pdev->dev, "rts crypto do\n");

	/* start crypto */
	rts_crypto_start_crypt(cdata);

	if (irq) {
		ret = wait_for_completion_timeout(&cdata->crypto_complete,
						  msecs_to_jiffies(10000));
		if (ret == 0) {
			dev_err(&cdata->pdev->dev, "timed out\n");
			ret = -ETIMEDOUT;
			goto do_out;
		}
	} else {
		while (!rts_crypto_done(cdata) && t--)
			udelay(1);

		if (!t) {
			dev_err(&cdata->pdev->dev, "device busy, timed out!\n");
			return -ETIMEDOUT;
		}
	}

	/* crypto status */
	val = rts_crypto_get_status(cdata);
	if (!(val & CIPHER_STS_OK)) {
		dev_err(&cdata->pdev->dev, "crypto err\n");
		ret = -EIO;
		goto do_out;
	}

	dev_dbg(&cdata->pdev->dev, "crypto ok\n");

	ret = 0;
do_out:
	/* stop crypto */
	rts_crypto_stop_crypt(cdata);

	return ret;
}

static int aes_generic_crypt(struct skcipher_request *req, bool encrypt)
{
	int ret = 0;
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	skcipher_request_set_tfm(req, ctx->tfm);

	if (encrypt)
		ret = crypto_skcipher_encrypt(req);
	else
		ret = crypto_skcipher_decrypt(req);

	skcipher_request_set_tfm(req, tfm);
	return ret;
}

static int rts_crypto(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct rts_crypto_data *cdata = rts_cdata;
	struct scatterlist *dst = req->dst;
	struct scatterlist *src = req->src;
	unsigned int nbytes = req->cryptlen;
	int ret = 0;
	unsigned int blocksize;
	bool irq = nbytes > IRQ_LIMIT ? true : false;

	dev_dbg(&cdata->pdev->dev, "rts crypto\n");

	/* check align */
	blocksize = crypto_skcipher_blocksize(tfm);
	if (!IS_ALIGNED(nbytes, blocksize)) {
		dev_err(&cdata->pdev->dev,
			"cryptlen must be multiple of the block size.\n");
		return -EINVAL;
	}

	/* nbytes < BLOCK_SIZE for cbccs1/cs2/cs3 */
	if (((ctx->mask & FLAGS_AES) && (nbytes < AES_BLOCK_SIZE)) ||
	    ((ctx->mask & (FLAGS_DES | FLAGS_DES3)) &&
	     (nbytes < DES_BLOCK_SIZE))) {
		if (ctx->mask & (FLAGS_CBCCS1 | FLAGS_CBCCS2 | FLAGS_CBCCS3)) {
			rts_sg_copy(dst, src, nbytes);
			return 0;
		}
	}

	/* ctx */
	ctx->ivlen = crypto_skcipher_ivsize(tfm);
	dev_dbg(&cdata->pdev->dev, "ctx->ivlen = %d\n", ctx->ivlen);
	if (ctx->ivlen)
		ctx->iv = (u8 *)req->iv;

	/* aes_generic */
	if (nbytes <= AES_GENERIC_LIMIT && ctx->tfm && !ctx->ekey) {
		ret = aes_generic_crypt(req, ctx->mask & FLAGS_ENCRYPT ? true :
									 false);
		if (ret)
			dev_err(&cdata->pdev->dev, "aes generic crypt err!\n");
		return ret;
	}

	mutex_lock(&cdata->dma_mutex);

	/* init */
	rts_crypto_init(cdata, irq);

	/* init CTL */
	ret = rts_crypto_set_ctl(cdata, ctx);
	if (ret) {
		dev_err(&cdata->pdev->dev, "set ctl err\n");
		goto out;
	}

	/* key */
	rts_crypto_set_key(cdata, ctx);

	/* set iv */
	rts_crypto_set_iv(cdata, ctx);

	/* data in & out */
	rts_crypto_buf_table_init(cdata, ctx);

	ret = rts_blkcipher_map_in_data(cdata, ctx, src, nbytes);
	if (ret < 0) {
		dev_err(&cdata->pdev->dev, "map in data err\n");
		goto out;
	}

	ret = rts_blkcipher_map_out_data(cdata, ctx, dst, nbytes);
	if (ret < 0) {
		dev_err(&cdata->pdev->dev, "map out data err\n");
		goto out;
	}

	ret = rts_crypto_do(cdata, ctx, irq);
	if (ret)
		goto out;

	/* get iv */
	rts_crypto_get_iv(cdata, ctx);

out:
	/* unmap in & out */
	rts_crypto_unmap_table(cdata, ctx);

	rts_crypto_uninit(cdata);
	mutex_unlock(&cdata->dma_mutex);

	return ret;
}

static int rts_crypt_skcipher_init(struct crypto_skcipher *tfm)
{
	int ret = 0;
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct rts_crypto_data *cdata = rts_cdata;
	char name[CRYPTO_MAX_ALG_NAME];
	char *p;

	dev_dbg(&cdata->pdev->dev, "%s\n", __func__);
	/* init buf table */
	ret = buf_table_init(cdata, &ctx->table_in);
	if (ret)
		goto dma_err;
	ret = buf_table_init(cdata, &ctx->table_out);
	if (ret)
		goto dma_err;

	strcpy(name, crypto_tfm_alg_name(crypto_skcipher_tfm(tfm)));
	p = strstr(name, "(aes");
	if (p) {
		strcpy(p + 4, "-arm)");
		ctx->tfm = crypto_alloc_skcipher(name, 0, 0);
		if (IS_ERR(ctx->tfm)) {
			strcpy(p + 4, "-generic)");
			ctx->tfm = crypto_alloc_skcipher(name, 0, 0);
			if (IS_ERR(ctx->tfm)) {
				dev_dbg(&cdata->pdev->dev,
					"Error allocating '%s' transparent: %ld\n",
					name, PTR_ERR(ctx->tfm));
				ctx->tfm = NULL;
			}
		}
	}
	return ret;
dma_err:
	buf_table_uninit(cdata, &ctx->table_in);
	buf_table_uninit(cdata, &ctx->table_out);

	return ret;
}

static void rts_crypt_skcipher_exit(struct crypto_skcipher *tfm)
{
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);
	struct rts_crypto_data *cdata = rts_cdata;

	buf_table_uninit(cdata, &ctx->table_in);
	buf_table_uninit(cdata, &ctx->table_out);

	if (ctx->tfm) {
		crypto_free_skcipher(ctx->tfm);
		ctx->tfm = NULL;
	}
}

/* aes */
static int rts_aes_ecb_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_aes_ecb_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_aes_cbc_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_aes_cbc_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_aes_cbccs1_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_aes_cbccs1_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_aes_cbccs2_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_aes_cbccs2_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_aes_cbccs3_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_aes_cbccs3_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_aes_ctr_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CTR;
	return rts_crypto(req);
}

static int rts_aes_ctr_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CTR;
	return rts_crypto(req);
}

/* des */
static int rts_des_ecb_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_des_ecb_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_des_cbc_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_des_cbc_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_des_cbccs1_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_des_cbccs1_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_des_cbccs2_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_des_cbccs2_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_des_cbccs3_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_des_cbccs3_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_des_ctr_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES | FLAGS_CTR;
	return rts_crypto(req);
}

static int rts_des_ctr_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES | FLAGS_CTR;
	return rts_crypto(req);
}

/* 3des */
static int rts_des3_ecb_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_des3_ecb_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_ECB;
	return rts_crypto(req);
}

static int rts_des3_cbc_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_des3_cbc_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_CBC;
	return rts_crypto(req);
}

static int rts_des3_cbccs1_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_des3_cbccs1_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_CBCCS1;
	return rts_crypto(req);
}

static int rts_des3_cbccs2_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_des3_cbccs2_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_CBCCS2;
	return rts_crypto(req);
}

static int rts_des3_cbccs3_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_des3_cbccs3_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_CBCCS3;
	return rts_crypto(req);
}

static int rts_des3_ctr_encrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_DES3 | FLAGS_CTR;
	return rts_crypto(req);
}

static int rts_des3_ctr_decrypt(struct skcipher_request *req)
{
	struct crypto_skcipher *tfm = crypto_skcipher_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_skcipher_ctx(tfm);

	ctx->mask = FLAGS_DECRYPT | FLAGS_DES3 | FLAGS_CTR;
	return rts_crypto(req);
}

static struct skcipher_alg rts_algs_aes[] = {
	[MODE_ECB] = {
		.base = {
			.cra_name = "ecb(aes)",
			.cra_driver_name = "ecb-aes-rlx",
			.cra_blocksize = AES_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_ecb_encrypt,
		.decrypt = rts_aes_ecb_decrypt,
	},
	[MODE_CBC] = {
		.base = {
			.cra_name = "cbc(aes)",
			.cra_driver_name = "cbc-aes-rlx",
			.cra_blocksize = AES_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_cbc_encrypt,
		.decrypt = rts_aes_cbc_decrypt,
		.ivsize = AES_MIN_KEY_SIZE,
	},
	[MODE_CBCCS1] = {
		.base = {
			.cra_name = "cbc-cs1(aes)",
			.cra_driver_name = "cbccs1-aes-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_cbccs1_encrypt,
		.decrypt = rts_aes_cbccs1_decrypt,
		.ivsize = AES_MIN_KEY_SIZE,
	},
	[MODE_CBCCS2] = {
		.base = {
			.cra_name = "cbc-cs2(aes)",
			.cra_driver_name = "cbccs2-aes-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_cbccs2_encrypt,
		.decrypt = rts_aes_cbccs2_decrypt,
		.ivsize = AES_MIN_KEY_SIZE,
	},
	[MODE_CBCCS3] = {
		.base = {
			.cra_name = "cbc-cs3(aes)",
			.cra_driver_name = "cbccs3-aes-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_cbccs3_encrypt,
		.decrypt = rts_aes_cbccs3_decrypt,
		.ivsize = AES_MIN_KEY_SIZE,
	},
	[MODE_CTR] = {
		.base = {
			.cra_name = "ctr(aes)",
			.cra_driver_name = "ctr-aes-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = AES_MIN_KEY_SIZE,
		.max_keysize = AES_MAX_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_aes_ctr_encrypt,
		.decrypt = rts_aes_ctr_decrypt,
		.ivsize = AES_MIN_KEY_SIZE,
	},
};

static struct skcipher_alg rts_algs_des[] = {
	[MODE_ECB] = {
		.base = {
			.cra_name = "ecb(des)",
			.cra_driver_name = "ecb-des-rlx",
			.cra_blocksize = DES_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_ecb_encrypt,
		.decrypt = rts_des_ecb_decrypt,
	},
	[MODE_CBC] = {
		.base = {
			.cra_name = "cbc(des)",
			.cra_driver_name = "cbc-des-rlx",
			.cra_blocksize = DES_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_cbc_encrypt,
		.decrypt = rts_des_cbc_decrypt,
		.ivsize = DES_BLOCK_SIZE,
	},
	[MODE_CBCCS1] = {
		.base = {
			.cra_name = "cbc-cs1(des)",
			.cra_driver_name = "cbccs1-des-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_cbccs1_encrypt,
		.decrypt = rts_des_cbccs1_decrypt,
		.ivsize = DES_BLOCK_SIZE,
	},
	[MODE_CBCCS2] = {
		.base = {
			.cra_name = "cbc-cs2(des)",
			.cra_driver_name = "cbccs2-des-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_cbccs2_encrypt,
		.decrypt = rts_des_cbccs2_decrypt,
		.ivsize = DES_BLOCK_SIZE,
	},
	[MODE_CBCCS3] = {
		.base = {
			.cra_name = "cbc-cs3(des)",
			.cra_driver_name = "cbccs3-des-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_cbccs3_encrypt,
		.decrypt = rts_des_cbccs3_decrypt,
		.ivsize = DES_BLOCK_SIZE,
	},
	[MODE_CTR] = {
		.base = {
			.cra_name = "ctr(des)",
			.cra_driver_name = "ctr-des-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES_KEY_SIZE,
		.max_keysize = DES_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des_ctr_encrypt,
		.decrypt = rts_des_ctr_decrypt,
		.ivsize = DES_BLOCK_SIZE,
	},
};

static struct skcipher_alg rts_algs_des3[] = {
	[MODE_ECB] = {
		.base = {
			.cra_name = "ecb(des3_ede)",
			.cra_driver_name = "ecb-des3_ede-rlx",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_ecb_encrypt,
		.decrypt = rts_des3_ecb_decrypt,
	},
	[MODE_CBC] = {
		.base = {
			.cra_name = "cbc(des3_ede)",
			.cra_driver_name = "cbc-des3_ede-rlx",
			.cra_blocksize = DES3_EDE_BLOCK_SIZE,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_cbc_encrypt,
		.decrypt = rts_des3_cbc_decrypt,
		.ivsize = DES3_EDE_BLOCK_SIZE,
	},
	[MODE_CBCCS1] = {
		.base = {
			.cra_name = "cbc-cs1(des3_ede)",
			.cra_driver_name = "cbccs1-des3_ede-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_cbccs1_encrypt,
		.decrypt = rts_des3_cbccs1_decrypt,
		.ivsize = DES3_EDE_BLOCK_SIZE,
	},
	[MODE_CBCCS2] = {
		.base = {
			.cra_name = "cbc-cs2(des3_ede)",
			.cra_driver_name = "cbccs2-des3_ede-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_cbccs2_encrypt,
		.decrypt = rts_des3_cbccs2_decrypt,
		.ivsize = DES3_EDE_BLOCK_SIZE,
	},
	[MODE_CBCCS3] = {
		.base = {
			.cra_name = "cbc-cs3(des3_ede)",
			.cra_driver_name = "cbccs3-des3_ede-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_cbccs3_encrypt,
		.decrypt = rts_des3_cbccs3_decrypt,
		.ivsize = DES3_EDE_BLOCK_SIZE,
	},
	[MODE_CTR] = {
		.base = {
			.cra_name = "ctr(des3_ede)",
			.cra_driver_name = "ctr-des3_ede-rlx",
			.cra_blocksize = 1,
		},
		.init = rts_crypt_skcipher_init,
		.exit = rts_crypt_skcipher_exit,
		.min_keysize = DES3_EDE_KEY_SIZE,
		.max_keysize = DES3_EDE_KEY_SIZE,
		.setkey = rts_setkey,
		.encrypt = rts_des3_ctr_encrypt,
		.decrypt = rts_des3_ctr_decrypt,
		.ivsize = DES3_EDE_BLOCK_SIZE,
	}
};

static struct skcipher_alg *rts_algs[] = {
	[ALG_AES] = rts_algs_aes,
	[ALG_DES] = rts_algs_des,
	[ALG_DES3] = rts_algs_des3,
};

/* aead */
static int rts_aead_crypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);
	struct rts_aead_reqctx *reqctx = aead_request_ctx(req);
	struct rts_crypto_data *cdata = rts_cdata;
	unsigned int blocksize;
	int ret = 0;
	bool irq = req->cryptlen > AEAD_IRQ_LIMIT ? true : false;

	dev_dbg(&cdata->pdev->dev, "rts aead crypto\n");

	/* check align */
	blocksize = crypto_aead_blocksize(tfm);
	if (!IS_ALIGNED(req->cryptlen, blocksize)) {
		dev_err(&cdata->pdev->dev,
			"cryptlen must be multiple of the block size.\n");
		return -EINVAL;
	}

	if (ctx->mask & FLAGS_CCM) {
		ret = rts_ccm_check_iv(req->iv);
		if (ret) {
			dev_err(&cdata->pdev->dev, "ccm check iv err\n");
			return -EINVAL;
		}
	}

	mutex_lock(&cdata->dma_mutex);

	/* init */
	rts_crypto_init(cdata, irq);

	/* init CTL */
	ret = rts_crypto_set_ctl(cdata, ctx);
	if (ret) {
		dev_err(&cdata->pdev->dev, "set ctl err\n");
		goto out;
	}

	/* key */
	rts_crypto_set_key(cdata, ctx);

	/* data in & out */
	rts_crypto_buf_table_init(cdata, ctx);

	/* clear iv regs */
	rts_crypto_clear_iv(cdata);

	if (ctx->mask & FLAGS_GCM) {
		/* iv */
		ret = rts_gcm_map_iv(cdata, ctx, req, reqctx);
		if (ret < 0) {
			dev_err(&cdata->pdev->dev, "map iv err\n");
			goto out;
		}

		/* associated data*/
		ret = rts_gcm_map_ad(cdata, ctx, req, reqctx);
		if (ret < 0) {
			dev_err(&cdata->pdev->dev,
				"map in associated data err\n");
			goto out;
		}

		/* in plaintext/ciphertext data*/
		ret = rts_gcm_map_in_data(cdata, ctx, req, reqctx);
		if (ret < 0) {
			dev_err(&cdata->pdev->dev, "map in data err\n");
			goto out;
		}
	} else if (ctx->mask & FLAGS_CCM) {
		ret = rts_ccm_map_nonce_ad(cdata, ctx, req, reqctx);
		if (ret < 0) {
			dev_err(&cdata->pdev->dev,
				"map nonce and associated data err\n");
			goto out;
		}

		rts_ccm_set_ctr_flag(cdata, req);

		/* in plaintext/ciphertext data*/
		ret = rts_ccm_map_in_data(cdata, ctx, req, reqctx);
		if (ret < 0) {
			dev_err(&cdata->pdev->dev, "map in data err\n");
			goto out;
		}
	}

	/* out plaintext/ciphertext data*/
	ret = rts_aead_map_out_data(cdata, ctx, req, reqctx);
	if (ret < 0) {
		dev_err(&cdata->pdev->dev, "map out data err\n");
		goto out;
	}

	dump_regs(0, 128);
	ret = rts_crypto_do(cdata, ctx, irq);
	if (ret)
		goto out;

	/*
	 * unmap in & out & iv
	 * Must be called before the driver modifies sg buf
	 */
	rts_crypto_unmap_table(cdata, ctx);

	/* authentication tag */
	rts_crypto_get_atag(cdata, reqctx);

	if (ctx->mask & FLAGS_ENCRYPT)
		rts_aead_output_atag(cdata, req, reqctx);
	else if (ctx->mask & FLAGS_DECRYPT)
		ret = rts_aead_verify_atag(cdata, req, reqctx);

	sg_printf(req->src);
	sg_printf(req->dst);

out:
	/* unmap in & out & iv */
	rts_crypto_unmap_table(cdata, ctx);

	rts_crypto_uninit(cdata);
	mutex_unlock(&cdata->dma_mutex);

	return ret;
}

static int rts_aes_gcm_setkey(struct crypto_aead *tfm, const u8 *key,
			      unsigned int keylen)
{
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);

	dev_dbg(&rts_cdata->pdev->dev, "rts setkey = %d\n", keylen);

	if (keylen != AES_KEYSIZE_256 && keylen != AES_KEYSIZE_192 &&
	    keylen != AES_KEYSIZE_128) {
		return -EINVAL;
	}

	ctx->keylen = keylen;
	memcpy(ctx->key, key, keylen);

	return 0;
}

static int rts_aes_gcm_setauthsize(struct crypto_aead *tfm,
				   unsigned int authsize)
{
	dev_dbg(&rts_cdata->pdev->dev, "rts set authsize = %d\n", authsize);

	switch (authsize) {
	case 4:
	case 8:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rts_aes_gcm_encrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_GCM;

	return rts_aead_crypt(req);
}

static int rts_aes_gcm_decrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);
	unsigned int authsize = crypto_aead_authsize(tfm);

	req->cryptlen -= authsize;
	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_GCM;

	return rts_aead_crypt(req);
}

static int rts_aes_gcm_init(struct crypto_aead *tfm)
{
	int ret = 0;
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);
	struct rts_crypto_data *cdata = rts_cdata;

	crypto_aead_set_reqsize(tfm, sizeof(struct rts_aead_reqctx));

	/* init buf table */
	ret = buf_table_init(cdata, &ctx->table_in);
	if (ret)
		goto dma_err;
	ret = buf_table_init(cdata, &ctx->table_out);
	if (ret)
		goto dma_err;

	return ret;
dma_err:
	buf_table_uninit(cdata, &ctx->table_in);
	buf_table_uninit(cdata, &ctx->table_out);

	return ret;
}

static void rts_aes_gcm_exit(struct crypto_aead *tfm)
{
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);
	struct rts_crypto_data *cdata = rts_cdata;

	buf_table_uninit(cdata, &ctx->table_in);
	buf_table_uninit(cdata, &ctx->table_out);
}

static int rts_aes_ccm_setauthsize(struct crypto_aead *tfm,
				   unsigned int authsize)
{
	switch (authsize) {
	case 4:
	case 6:
	case 8:
	case 10:
	case 12:
	case 14:
	case 16:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rts_aes_ccm_encrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);

	ctx->mask = FLAGS_ENCRYPT | FLAGS_AES | FLAGS_CCM;

	return rts_aead_crypt(req);
}

static int rts_aes_ccm_decrypt(struct aead_request *req)
{
	struct crypto_aead *tfm = crypto_aead_reqtfm(req);
	struct rts_crypto_ctx *ctx = crypto_aead_ctx(tfm);
	unsigned int authsize = crypto_aead_authsize(tfm);

	req->cryptlen -= authsize;
	ctx->mask = FLAGS_DECRYPT | FLAGS_AES | FLAGS_CCM;

	return rts_aead_crypt(req);
}

static struct aead_alg rts_algs_aead[] = {
	[AEAD_AES_GCM] = {
		.base = {
			.cra_name = "gcm(aes)",
			.cra_driver_name = "gcm-aes-rlx",
			.cra_priority = 300,
			.cra_blocksize = 1,
			.cra_ctxsize = sizeof(struct rts_crypto_ctx),
			.cra_alignmask = 0x3,
			.cra_module = THIS_MODULE,
		},

		.setkey = rts_aes_gcm_setkey,
		.setauthsize = rts_aes_gcm_setauthsize,
		.encrypt = rts_aes_gcm_encrypt,
		.decrypt = rts_aes_gcm_decrypt,
		.init = rts_aes_gcm_init,
		.exit = rts_aes_gcm_exit,
		.ivsize = 12,
		.maxauthsize = AES_BLOCK_SIZE,
	},
	[AEAD_AES_CCM] = {
		.base = {
			.cra_name = "ccm(aes)",
			.cra_driver_name = "ccm-aes-rlx",
			.cra_priority = 300,
			.cra_blocksize = 1,
			.cra_ctxsize = sizeof(struct rts_crypto_ctx),
			.cra_alignmask = 0x3,
			.cra_module = THIS_MODULE,
		},

		.setkey = rts_aes_gcm_setkey,
		.setauthsize = rts_aes_ccm_setauthsize,
		.encrypt = rts_aes_ccm_encrypt,
		.decrypt = rts_aes_ccm_decrypt,
		.init = rts_aes_gcm_init,
		.exit = rts_aes_gcm_exit,
		.ivsize = AES_BLOCK_SIZE,
		.maxauthsize = AES_BLOCK_SIZE,
	},
};

static int crypto_open(struct inode *inode, struct file *file)
{
	struct rts_crypto_data *cdata =
		container_of(inode->i_cdev, struct rts_crypto_data, cdev);

	file->private_data = cdata;
	return 0;
}

static int crypto_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long crypto_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct rts_crypto_data *cdata = file->private_data;

	if (!cdata)
		return -EINVAL;

	switch (cmd) {
	case RTS_CIPHER_IOC_SET_NORMALKEY:
		cdata->ekey = 0;
		break;
	case RTS_CIPHER_IOC_SET_EFUSEKEY:
		cdata->ekey = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations crypto_fops = {
	.owner = THIS_MODULE,
	.open = crypto_open,
	.release = crypto_release,
	.unlocked_ioctl = crypto_ioctl,
};

static int parse_dts(struct platform_device *pdev)
{
	struct rts_crypto_data *cdata;
	int ret, i, j;
	static const char *const alg[] = {
		[ALG_AES] = "aes", [ALG_DES] = "des", [ALG_DES3] = "3des"
	};
	static const char *const mode[] = {
		[MODE_ECB] = "ecb",	  [MODE_CBC] = "cbc",
		[MODE_CBCCS1] = "cbccs1", [MODE_CBCCS2] = "cbccs2",
		[MODE_CBCCS3] = "cbccs3", [MODE_CTR] = "ctr"
	};
	static const char *const aead_alg[] = {
		[AEAD_AES_GCM] = "gcm(aes)", [AEAD_AES_CCM] = "ccm(aes)"
	};

	if (!pdev)
		return -EINVAL;

	cdata = platform_get_drvdata(pdev);
	if (!cdata) {
		dev_err(&pdev->dev, "get platform_drvdata failed.\n");
		return -EINVAL;
	}

	/* skcipher alg */
	for (i = 0; i < ALG_COUNT && alg[i]; i++) {
		for (j = 0; j < MODE_COUNT && mode[j]; j++) {
			ret = of_property_match_string(pdev->dev.of_node,
						       alg[i], mode[j]);
			if (ret >= 0)
				set_cap(cdata->mode_cap[i], j);
		}
	}

	/* aead alg */
	for (i = 0; i < AEAD_COUNT && aead_alg[i]; i++) {
		ret = of_property_match_string(pdev->dev.of_node, "aead",
					       aead_alg[i]);
		if (ret >= 0)
			set_cap(cdata->aead_cap, i);
	}

	return 0;
}

static void unregister_algs(struct rts_crypto_data *cdata)
{
	int i, j;

	/* unregister skcipher algs */
	for (i = 0; i < ALG_COUNT; i++) {
		for (j = 0; j < MODE_COUNT; j++) {
			if (!get_cap(cdata->mode_cap[i], j))
				continue;

			crypto_unregister_skcipher(&rts_algs[i][j]);
		}
	}

	/* unregister aead algs */
	for (i = 0; i < AEAD_COUNT; i++) {
		if (!get_cap(cdata->aead_cap, i))
			continue;

		crypto_unregister_aead(&rts_algs_aead[i]);
	}
}

static int register_algs(struct rts_crypto_data *cdata)
{
	int i, j, ret;

	/* register skcipher algs */
	for (i = 0; i < ALG_COUNT; i++) {
		for (j = 0; j < MODE_COUNT; j++) {
			if (!get_cap(cdata->mode_cap[i], j))
				continue;

			rts_algs[i][j].base.cra_priority = 300;
			rts_algs[i][j].base.cra_flags = 0,
			rts_algs[i][j].base.cra_ctxsize =
				sizeof(struct rts_crypto_ctx);
			rts_algs[i][j].base.cra_alignmask = 3;
			rts_algs[i][j].base.cra_module = THIS_MODULE;

			ret = crypto_register_skcipher(&rts_algs[i][j]);
			if (ret)
				goto err;
		}
	}

	/* register aead algs */
	for (i = 0; i < AEAD_COUNT; i++) {
		if (!get_cap(cdata->aead_cap, i))
			continue;

		ret = crypto_register_aead(&rts_algs_aead[i]);
		if (ret)
			goto err;
	}

	return 0;
err:
	unregister_algs(cdata);
	return ret;
}

static const struct of_device_id rts_crypto_dt_ids[] = {
	{ .compatible = "realtek,rts493xa-crypto" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_crypto_dt_ids);

static irqreturn_t rts_crypto_irq_handler(int irq, void *data)
{
	struct rts_crypto_data *crypto_data;
	u32 val;

	crypto_data = (struct rts_crypto_data *)data;

	dev_dbg(&crypto_data->pdev->dev, "IRQ %d handler\n", irq);
	val = rts_crypto_read(crypto_data, RLX_REG_CIPHER_INT_FLAG);

	val &= 0x5;
	if (!val) {
		dev_dbg(&crypto_data->pdev->dev, "IRQ not from this device\n");
		return IRQ_NONE;
	}

	rts_crypto_write(crypto_data, RLX_REG_CIPHER_INT_FLAG, val);
	complete(&crypto_data->crypto_complete);

	return IRQ_HANDLED;
}

static int rts_crypto_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct rts_crypto_data *cdata;
	struct crypto_alg *alg;

	cdata = devm_kzalloc(&pdev->dev, sizeof(*cdata), GFP_KERNEL);
	if (cdata == NULL)
		return -ENOMEM;

	cdata->pdev = pdev;

	platform_set_drvdata(pdev, cdata);

	ret = parse_dts(pdev);
	if (ret) {
		dev_err(&pdev->dev, "parse dts failed.\n");
		goto mem_err;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&cdata->pdev->dev, "unable to get crypto address\n");
		ret = -ENXIO;
		goto mem_err;
	}

	cdata->base = res->start;
	cdata->size = res->end - res->start + 1;

	cdata->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(cdata->addr)) {
		dev_err(&cdata->pdev->dev, "unable to ioremap\n");
		ret = -ENXIO;
		goto mem_err;
	}

	cdata->cipher_clk = devm_clk_get(&pdev->dev, "cipher_ck");
	if (IS_ERR(cdata->cipher_clk)) {
		dev_err(&pdev->dev, "clock initialization failed.\n");
		goto mem_err;
	}

	ret = clk_prepare_enable(cdata->cipher_clk);
	if (ret) {
		dev_err(&pdev->dev, "clock prepare failed.\n");
		goto mem_err;
	}

	cdata->rst = devm_reset_control_get(&pdev->dev, "rst");
	if (IS_ERR(cdata->rst)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		goto mem_err;
	}

	/* reset crypto */
	reset_control_reset(cdata->rst);

	cdata->sd = devm_reset_control_get(&pdev->dev, "sd");
	if (IS_ERR(cdata->rst)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		goto mem_err;
	}

	reset_control_deassert(cdata->sd);

	cdata->irq = platform_get_irq(pdev, 0);
	if (cdata->irq < 0) {
		dev_err(&pdev->dev, "can't get IRQ resource\n");
		goto mem_err;
	}

	ret = devm_request_irq(&pdev->dev, cdata->irq, rts_crypto_irq_handler,
			       IRQF_SHARED, dev_name(&pdev->dev), cdata);

	dev_dbg(&pdev->dev, "using IRQ channel %d\n", cdata->irq);

	/* create dev node */
	ret = register_chrdev_region(devno, 1, "crypto");
	if (ret) {
		dev_err(&pdev->dev, "register_chrdev_region failed.\n");
		goto reg_err;
	}

	cdev_init(&cdata->cdev, &crypto_fops);

	ret = cdev_add(&cdata->cdev, devno, 1);
	if (ret) {
		dev_err(&pdev->dev, "cdev_add failed.\n");
		goto cdev_err;
	}

	rts_cdata = cdata;

	mutex_init(&rts_cdata->dma_mutex);
	init_completion(&rts_cdata->crypto_complete);

	/* register algs */
	ret = register_algs(cdata);
	if (ret)
		goto reg_err;

	mdelay(5);

	/*
	 * fscrypt requires xts(aes) alg. However, if attempt to load a
	 * non-registered alg will trigger a request_module. fscrypt can quickly
	 * overwhelm the concurrency limit in kmod. So pre-register ths alg here
	 */
	alg = crypto_alg_mod_lookup("xts(ecb-aes-rlx)", 0, 0);
	if (!IS_ERR(alg))
		crypto_mod_put(alg);
	else
		dev_warn(&pdev->dev, "lookup xts(ecb-aes-rlx) failed!\n");

	dev_info(&pdev->dev, "Realtek RLX crypto driver initialized\n");

	return 0;

cdev_err:
	unregister_chrdev_region(devno, 1);
reg_err:
	unregister_algs(cdata);
mem_err:
	devm_kfree(&pdev->dev, cdata);
	cdata = NULL;

	return ret;
}

static int rts_crypto_remove(struct platform_device *pdev)
{
	struct rts_crypto_data *cdata;
	struct resource *res;

	cdata = platform_get_drvdata(pdev);

	/* remove dev node */
	cdev_del(&cdata->cdev);
	unregister_chrdev_region(devno, 1);

	/* memory sd down */
	reset_control_assert(cdata->sd);

	/* unregister algs */
	unregister_algs(cdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(cdata->base, resource_size(res));

	reset_control_assert(cdata->rst);
	clk_disable_unprepare(cdata->cipher_clk);

	devm_kfree(&pdev->dev, cdata);
	dev_set_drvdata(&pdev->dev, NULL);
	cdata = NULL;
	rts_cdata = NULL;

	return 0;
}

static struct platform_driver rts_crypto_driver = {
	.probe = rts_crypto_probe,
	.remove = rts_crypto_remove,
	.driver = {
		.name = "rts-crypto",
		.of_match_table = of_match_ptr(rts_crypto_dt_ids),
	},
};
module_platform_driver(rts_crypto_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek RLX crypto driver");
