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
#include <crypto/internal/hash.h>
#include <crypto/sha2.h>
#include <crypto/scatterwalk.h>
#include <crypto/if_alg.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/of.h>
#include <asm/highmem.h>
#include <asm/fixmap.h>
#include <asm/pgtable.h>

#include "rts_sha.h"

//#define SHA_DEBUG

#ifdef SHA_DEBUG
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
			pr_info(" 0xb920%04x= 0x%08x\n", i,    \
				readl(0xb9200000 + i));        \
	} while (0)
#else
#define sg_printf(sg)
#define dump_regs(s, n)
#endif

#define MODE_SHA256 0
#define MODE_HMAC   1

struct rts_sha_data {
	struct platform_device *pdev;
	void __iomem *addr;
	int irq;
	struct reset_control *rst;
	struct mutex sha_mutex;
	spinlock_t splock; /* synchronize access to the context */
	struct completion sha_complete;
	struct clk *sha_clk;
};

struct rts_sha_ctx {
	struct rts_sha_data *sdata;
	struct delayed_work crypt_work;
	int mode;
	struct ahash_request *req;
	u8 key[SHA256_BLOCK_SIZE];
	unsigned int keylen;
};

struct rts_sha_reqctx {
	int init_iv;
	int final;
	u64 tlen;
	u32 state[SHA256_DIGEST_SIZE / 4];
	u8 buf[SHA256_BLOCK_SIZE * 2];
	unsigned int bufcnt;
};

static struct rts_sha_data *rts_sdata;

static unsigned int rts_sha_read(struct rts_sha_data *sdata, unsigned int reg)
{
	return readl(sdata->addr + reg);
}

static int rts_sha_write(struct rts_sha_data *sdata, unsigned int reg,
			 unsigned int value)
{
	writel(value, sdata->addr + reg);

	return 0;
}

/* regs setting */
static inline void rts_sha_init(struct rts_sha_data *sdata, int async)
{
	if (async) {
		clk_prepare_enable(sdata->sha_clk);
		rts_sha_write(sdata, RLX_REG_SHA_IRQ_EN,
			      RLX_SHA256_DONE_INT_EN);
	} else {
		clk_enable(sdata->sha_clk);
	}

	rts_sha_write(sdata, RLX_REG_SHA_IRQ_FLAG, RLX_SHA256_DONE_INT);
}

static inline void rts_sha_uninit(struct rts_sha_data *sdata, int async)
{
	if (async) {
		rts_sha_write(sdata, RLX_REG_SHA_IRQ_EN, 0x0);
		clk_disable_unprepare(sdata->sha_clk);
	} else {
		clk_disable(sdata->sha_clk);
	}
}

static inline void rts_sha_set_mode(struct rts_sha_data *sdata, int mode)
{
	rts_sha_write(sdata, RLX_REG_HMAC_MODE, mode);
}

static inline void rts_sha_start(struct rts_sha_data *sdata)
{
	rts_sha_write(sdata, RLX_REG_SHA_CTL, RLX_CFG_SHA_START);
}

static inline int rts_sha_is_done(struct rts_sha_data *sdata)
{
	u32 val;

	val = rts_sha_read(sdata, RLX_REG_SHA_IRQ_FLAG) & RLX_SHA256_DONE_INT;

	if (!val)
		return 0;

	rts_sha_write(sdata, RLX_REG_SHA_IRQ_FLAG, val);
	return 1;
}

static inline void rts_sha_map_data(struct rts_sha_data *sdata,
				    dma_addr_t d_addr, unsigned int len)
{
	rts_sha_write(sdata, RLX_REG_SHA_DATA_ADDR, d_addr);
	rts_sha_write(sdata, RLX_REG_SHA_DATA_LEN, len);
}

static inline void rts_sha_set_total_len(struct rts_sha_data *sdata,
					 unsigned int len)
{
	rts_sha_write(sdata, RLX_REG_SHA_TOTAL_LEN, len);
}

static inline void rts_sha_enable_padding(struct rts_sha_data *sdata,
					  int enable)
{
	rts_sha_write(sdata, RLX_REG_SHA_PADDING_CTL, enable);
}

static inline void rts_sha_set_init_iv(struct rts_sha_data *sdata)
{
	rts_sha_write(sdata, RLX_REG_SHA_SET_IV, RLX_SHA_SET_IV);
}

static int rts_sha_set_iv(struct rts_sha_data *sdata, u32 *iv)
{
	u32 *val = iv;

	rts_sha_write(sdata, RLX_REG_SHA_IV_N0, cpu_to_be32(val[0]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N1, cpu_to_be32(val[1]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N2, cpu_to_be32(val[2]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N3, cpu_to_be32(val[3]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N4, cpu_to_be32(val[4]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N5, cpu_to_be32(val[5]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N6, cpu_to_be32(val[6]));
	rts_sha_write(sdata, RLX_REG_SHA_IV_N7, cpu_to_be32(val[7]));

	return 0;
}

static int rts_sha_get_result(struct rts_sha_data *sdata, u32 *result)
{
	u32 *val = result;

	val[0] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N0));
	val[1] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N1));
	val[2] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N2));
	val[3] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N3));
	val[4] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N4));
	val[5] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N5));
	val[6] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N6));
	val[7] = be32_to_cpu(rts_sha_read(sdata, RLX_REG_SHA_RESULT_N7));

	return 0;
}
static int rts_sha_do_crypt(struct rts_sha_data *sdata,
			    struct rts_sha_reqctx *reqctx, dma_addr_t d_addr,
			    unsigned int nbytes, u8 *result, int async)
{
	int ret = 1001; //1ms

	dev_dbg(&sdata->pdev->dev, "rts sha do crypt, nbytes = %d\n", nbytes);

	if (!nbytes) {
		dev_warn(&sdata->pdev->dev, "nbytes == 0\n");
		return 0;
	}

	/* set total len */
	rts_sha_set_total_len(sdata, reqctx->tlen);

	/* map data */
	rts_sha_map_data(sdata, d_addr, nbytes);

	/* set iv */
	if (reqctx->init_iv) {
		rts_sha_set_init_iv(sdata);
		reqctx->init_iv = 0;
		dev_dbg(&sdata->pdev->dev, "set init iv\n");
	} else {
		rts_sha_set_iv(sdata, reqctx->state);
		dev_dbg(&sdata->pdev->dev, "set iv\n");
	}

	dump_regs(0, 64);

	/* start */
	rts_sha_start(sdata);

	if (async) {
		/* ahash */
		dev_dbg(&sdata->pdev->dev, "wait irq complete\n");
		ret = wait_for_completion_timeout(&sdata->sha_complete,
						  msecs_to_jiffies(10000));
	} else {
		/* shash */
		dev_dbg(&sdata->pdev->dev, "wait polling complete\n");
		while (!rts_sha_is_done(sdata) && --ret)
			udelay(1);
	}

	if (ret == 0) {
		dev_err(&sdata->pdev->dev, "timed out\n");
		ret = -ETIMEDOUT;
		goto out;
	}
	ret = 0;

	/* get result */
	rts_sha_get_result(sdata, result ? (u32 *)result : reqctx->state);
out:
	dma_unmap_single(&sdata->pdev->dev, d_addr, nbytes, DMA_TO_DEVICE);

	dev_dbg(&sdata->pdev->dev, "rts sha do crypt complete\n");
	return ret;
}

static int rts_sha_update(struct rts_sha_data *sdata,
			  struct rts_sha_reqctx *reqctx, const u8 *data,
			  unsigned int nbytes, struct page *page, size_t offset,
			  unsigned int blocksize, int async)
{
	unsigned int a_of = 0, b_of = 0;
	int ret = 0;
	dma_addr_t d_addr;

	dev_dbg(&sdata->pdev->dev, "rts sha update, nbytes=%d\n", nbytes);

	if (!nbytes) {
		dev_warn(&sdata->pdev->dev, "nbytes == 0\n");
		return 0;
	}

	reqctx->tlen += nbytes;
	rts_sha_enable_padding(sdata, 0);

	/* update */
	a_of = b_of = 0;
	/* reqctx buf */
	if (reqctx->bufcnt) {
		dev_dbg(&sdata->pdev->dev, "reqctx buf, bufcnt = %d\n",
			reqctx->bufcnt);

		b_of = min_t(unsigned int, nbytes, blocksize - reqctx->bufcnt);
		if (b_of) {
			nbytes -= b_of;
			memcpy(reqctx->buf + reqctx->bufcnt, data, b_of);
			reqctx->bufcnt += b_of;
			dev_dbg(&sdata->pdev->dev,
				"reqctx buf, nbytes = %d, bufcnt = %d, b_of = %d\n",
				nbytes, reqctx->bufcnt, b_of);
		}

		if (!nbytes)
			return 0;

		d_addr = dma_map_single(&sdata->pdev->dev, (void *)reqctx->buf,
					reqctx->bufcnt, DMA_TO_DEVICE);

		ret = rts_sha_do_crypt(sdata, reqctx, d_addr, reqctx->bufcnt,
				       NULL, async);
		if (ret) {
			dev_err(&sdata->pdev->dev, "do crypt err\n");
			return ret;
		}

		reqctx->bufcnt = 0;
	}

	/* align */
	a_of = nbytes % blocksize;
	if (a_of) {
		nbytes -= a_of;
		memcpy(reqctx->buf + reqctx->bufcnt, data + b_of + nbytes,
		       a_of);
		reqctx->bufcnt += a_of;
		dev_dbg(&sdata->pdev->dev,
			"align, nbytes = %d, bufcnt = %d, a_of = %d\n", nbytes,
			reqctx->bufcnt, a_of);
	}

	if (nbytes) {
		if (page) {
			d_addr = dma_map_page(&sdata->pdev->dev, page,
					      offset + b_of, nbytes,
					      DMA_TO_DEVICE);
		} else {
			d_addr = dma_map_single(&sdata->pdev->dev,
						(void *)(data + b_of), nbytes,
						DMA_TO_DEVICE);
		}

		ret = rts_sha_do_crypt(sdata, reqctx, d_addr, nbytes, NULL,
				       async);
		if (ret) {
			dev_err(&sdata->pdev->dev, "do crypt err\n");
			return ret;
		}
	}

	dev_dbg(&sdata->pdev->dev, "rts sha update complete\n");
	return ret;
}

static int rts_sha_final(struct rts_sha_data *sdata,
			 struct rts_sha_reqctx *reqctx, u8 *result, int async)
{
	unsigned int nbytes = 0;
	int ret = 0;
	int i;
	dma_addr_t d_addr;

	nbytes = reqctx->bufcnt;

	dev_dbg(&sdata->pdev->dev, "rts sha final, nbytes 0x%x\n", nbytes);

	// padding: 448bit, 100000... + 64bit(nbytes):0
	rts_sha_enable_padding(sdata, 0);
	memset(reqctx->buf + nbytes, 0,
	       (nbytes < 56) ? (64 - nbytes) : (128 - nbytes));
	reqctx->buf[nbytes] = 0x80;
	reqctx->tlen <<= 3;
	nbytes = (nbytes < 56) ? 64 : 128;
	for (i = nbytes - 1; i > nbytes - 8; i--) {
		reqctx->buf[i] = reqctx->tlen & 0xff;
		reqctx->tlen >>= 8;
	}

	d_addr = dma_map_single(&sdata->pdev->dev, (void *)reqctx->buf, nbytes,
				DMA_TO_DEVICE);

	ret = rts_sha_do_crypt(sdata, reqctx, d_addr, nbytes, result, async);
	if (ret) {
		dev_err(&sdata->pdev->dev, "do crypt err\n");
		return ret;
	}

	dev_dbg(&sdata->pdev->dev, "rts sha final complete\n");

	return ret;
}

static int rts_ahash_sha_update(struct rts_sha_data *sdata,
				struct ahash_request *req)
{
	unsigned int nbytes = 0;
	struct crypto_hash_walk walk;
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	unsigned int blocksize = crypto_ahash_blocksize(ahash);

	dev_dbg(&sdata->pdev->dev, "rts ahash sha update\n");

	for (nbytes = crypto_hash_walk_first(req, &walk); nbytes > 0;
	     nbytes = crypto_hash_walk_done(&walk, nbytes)) {
		nbytes = rts_sha_update(sdata, reqctx, walk.data, nbytes,
					walk.pg, walk.offset, blocksize, 1);
	}

	dev_dbg(&sdata->pdev->dev, "rts ahash sha update complete\n");
	return nbytes;
}

static int rts_ahash_sha_crypt(struct rts_sha_data *sdata,
			       struct ahash_request *req)
{
	int ret = 0;
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct rts_sha_ctx *ctx = crypto_ahash_ctx(ahash);
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);

	dev_dbg(&sdata->pdev->dev, "rts ahash sha crypt\n");

	mutex_lock(&sdata->sha_mutex);

	rts_sha_init(sdata, 1);
	rts_sha_set_mode(sdata, ctx->mode);

	if (reqctx->final)
		/* final */
		ret = rts_sha_final(sdata, reqctx, req->result, 1);
	else
		/* update */
		ret = rts_ahash_sha_update(sdata, req);

	rts_sha_uninit(sdata, 1);
	mutex_unlock(&sdata->sha_mutex);

	dev_dbg(&sdata->pdev->dev, "rts ahash sha crypt complete\n");
	return ret;
}

static int highmem_vaddr_to_page(const void *vaddr, struct page **page,
				 size_t *offset)
{
	void *paddr = NULL;
	pte_t *pte = NULL;

	dev_dbg(&rts_sdata->pdev->dev,
		"HIGHMEM: VMALLOC[%p-%p], PKMAP[%p-%p], FIXADDR[%p-%p](FIXKMAP[%d-%d])\n",
		(void *)VMALLOC_START, (void *)VMALLOC_END, (void *)PKMAP_BASE,
		(void *)(PKMAP_BASE + (LAST_PKMAP * PAGE_SIZE)),
		(void *)FIXADDR_START, (void *)FIXADDR_TOP, FIX_KMAP_BEGIN,
		FIX_KMAP_END);

	if (is_vmalloc_addr(vaddr)) {
		dev_dbg(&rts_sdata->pdev->dev, "HIGHMEM: VMALLOC vaddr\n");
		*page = vmalloc_to_page(vaddr);

	} else if ((unsigned long)vaddr >= PKMAP_BASE &&
		   (unsigned long)vaddr <
			   (PKMAP_BASE + (LAST_PKMAP * PAGE_SIZE))) {
		dev_dbg(&rts_sdata->pdev->dev, "HIGHMEM: PKMAP vaddr\n");
		*page = kmap_to_page((void *)vaddr);

	} else if ((unsigned long)vaddr >= FIXADDR_START &&
		   (unsigned long)vaddr < FIXADDR_TOP) {
		dev_dbg(&rts_sdata->pdev->dev,
			"HIGHMEM: FIXADDR(FIXKMAP) vaddr\n");
		pte = pte_offset_kernel(pmd_off_k((unsigned long)vaddr),
					(unsigned long)vaddr);
		*page = pte_page(*pte);

	} else {
		dev_err(&rts_sdata->pdev->dev, "LOWMEM\n");
		return -EINVAL;
	}

	paddr = (void *)page_to_phys(*page);
	*offset = offset_in_page(vaddr);
	dev_dbg(&rts_sdata->pdev->dev,
		"vaddr=0x%p, paddr=0x%p, page=0x%p, offset=%d\n", vaddr, paddr,
		*page, *offset);

	return 0;
}

static int rts_shash_sha_crypt(struct rts_sha_data *sdata,
			       struct shash_desc *desc, const u8 *data,
			       unsigned int len, u8 *out)
{
	int ret = 0;
	struct rts_sha_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct rts_sha_reqctx *reqctx = shash_desc_ctx(desc);
	unsigned int blocksize = crypto_shash_blocksize(desc->tfm);
	unsigned long irq_flags;
	struct page *page = NULL;
	size_t offset = 0;

	dev_dbg(&sdata->pdev->dev, "rts shash sha crypt: data=0x%p, out=0x%p\n",
		data, out);

	spin_lock_irqsave(&sdata->splock, irq_flags);

	rts_sha_init(sdata, 0);
	rts_sha_set_mode(sdata, ctx->mode);

	if (reqctx->final) {
		/* final */
		ret = rts_sha_final(sdata, reqctx, out, 0);
	} else {
		/* update */
		if ((unsigned long)data >= VMALLOC_START) {
			ret = highmem_vaddr_to_page(data, &page, &offset);
			if (ret)
				return ret;
		}

		ret = rts_sha_update(sdata, reqctx, data, len, page, offset,
				     blocksize, 0);
	}

	rts_sha_uninit(sdata, 0);
	spin_unlock_irqrestore(&sdata->splock, irq_flags);

	dev_dbg(&sdata->pdev->dev, "rts shash sha crypt complete\n");
	return ret;
}

static void rts_sha_crypt_work(struct work_struct *work)
{
	int ret;
	struct rts_sha_ctx *ctx =
		container_of(work, struct rts_sha_ctx, crypt_work.work);

	ret = rts_ahash_sha_crypt(ctx->sdata, ctx->req);
	ahash_request_complete(ctx->req, ret);
}

/* alg member func */
/* ahash */
static int rts_ahash_sha256_init(struct ahash_request *req)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct rts_sha_ctx *ctx = crypto_ahash_ctx(ahash);
	struct rts_sha_data *sdata = ctx->sdata;
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);

	dev_dbg(&sdata->pdev->dev, "rts ahash sha256 init\n");

	memset(reqctx, 0, sizeof(struct rts_sha_reqctx));
	reqctx->init_iv = 1;

	return 0;
}

static int rts_ahash_sha256_update(struct ahash_request *req)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct rts_sha_ctx *ctx = crypto_ahash_ctx(ahash);
	struct rts_sha_data *sdata = ctx->sdata;

	dev_dbg(&sdata->pdev->dev, "rts ahash sha256 update\n");

	ctx->req = req;
	schedule_delayed_work(&ctx->crypt_work, 0);

	return -EINPROGRESS;
}

static int rts_ahash_sha256_final(struct ahash_request *req)
{
	struct crypto_ahash *ahash = crypto_ahash_reqtfm(req);
	struct rts_sha_ctx *ctx = crypto_ahash_ctx(ahash);
	struct rts_sha_data *sdata = ctx->sdata;
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);

	dev_dbg(&sdata->pdev->dev, "rts ahash sha256 final\n");

	ctx->req = req;
	reqctx->final = 1;
	schedule_delayed_work(&ctx->crypt_work, 0);

	return -EINPROGRESS;
}

static int rts_ahash_sha256_finup(struct ahash_request *req)
{
	int ret;

	ret = crypto_wait_req(rts_ahash_sha256_update(req),
			      (struct crypto_wait *)req->base.data);
	if (ret)
		goto err;

	ret = crypto_wait_req(rts_ahash_sha256_final(req),
			      (struct crypto_wait *)req->base.data);

err:
	return ret;
}

static int rts_ahash_sha256_digest(struct ahash_request *req)
{
	rts_ahash_sha256_init(req);

	return rts_ahash_sha256_finup(req);
}

static int rts_ahash_export(struct ahash_request *req, void *out)
{
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);

	memcpy(out, reqctx, sizeof(*reqctx));
	return 0;
}

static int rts_ahash_import(struct ahash_request *req, const void *in)
{
	struct rts_sha_reqctx *reqctx = ahash_request_ctx(req);

	memcpy(reqctx, in, sizeof(*reqctx));
	return 0;
}

static int rts_ahash_sha256_cra_init(struct crypto_tfm *tfm)
{
	struct crypto_ahash *ahash = __crypto_ahash_cast(tfm);
	struct rts_sha_ctx *ctx = crypto_ahash_ctx(ahash);

	INIT_DELAYED_WORK(&ctx->crypt_work, rts_sha_crypt_work);
	ctx->sdata = rts_sdata;
	ctx->mode = MODE_SHA256;
	crypto_ahash_set_reqsize(ahash, sizeof(struct rts_sha_reqctx));

	return 0;
}

static void rts_ahash_sha256_cra_exit(struct crypto_tfm *tfm)
{
	struct rts_sha_ctx *ctx = crypto_tfm_ctx(tfm);

	cancel_delayed_work(&ctx->crypt_work);
	flush_delayed_work(&ctx->crypt_work);
}

/* shash */
static int rts_shash_sha256_init(struct shash_desc *desc)
{
	struct rts_sha_reqctx *reqctx = shash_desc_ctx(desc);
	struct rts_sha_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct rts_sha_data *sdata = ctx->sdata;

	dev_dbg(&sdata->pdev->dev, "rts shash sha256 init\n");

	memset(reqctx, 0, sizeof(struct rts_sha_reqctx));
	reqctx->init_iv = 1;

	return 0;
}

static int rts_shash_sha256_update(struct shash_desc *desc, const u8 *data,
				   unsigned int len)
{
	struct rts_sha_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct rts_sha_data *sdata = ctx->sdata;

	dev_dbg(&sdata->pdev->dev, "rts shash sha256 update, nbytes=%d\n", len);

	return rts_shash_sha_crypt(sdata, desc, data, len, NULL);
}

static int rts_shash_sha256_final(struct shash_desc *desc, u8 *out)
{
	struct rts_sha_reqctx *reqctx = shash_desc_ctx(desc);
	struct rts_sha_ctx *ctx = crypto_shash_ctx(desc->tfm);
	struct rts_sha_data *sdata = ctx->sdata;

	dev_dbg(&sdata->pdev->dev, "rts shash sha256 final\n");

	reqctx->final = 1;

	return rts_shash_sha_crypt(sdata, desc, NULL, 0, out);
}

static int rts_shash_sha256_cra_init(struct crypto_tfm *tfm)
{
	struct rts_sha_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->sdata = rts_sdata;
	ctx->mode = MODE_SHA256;

	return 0;
}

/* ahash alg */
static struct ahash_alg rts_ahash_algs[] = {
	{
		.init = rts_ahash_sha256_init,
		.update = rts_ahash_sha256_update,
		.final = rts_ahash_sha256_final,
		.finup = rts_ahash_sha256_finup,
		.digest = rts_ahash_sha256_digest,
		.export = rts_ahash_export,
		.import = rts_ahash_import,
		.halg = {
			.digestsize = SHA256_DIGEST_SIZE,
			.statesize = sizeof(struct rts_sha_reqctx),
			.base = {
				.cra_name = "sha256",
				.cra_driver_name = "sha256-rlx",
				.cra_priority = 99,
				.cra_flags = CRYPTO_ALG_ASYNC,
				.cra_blocksize = SHA256_BLOCK_SIZE,
				.cra_ctxsize = sizeof(struct rts_sha_ctx),
				.cra_init = rts_ahash_sha256_cra_init,
				.cra_exit = rts_ahash_sha256_cra_exit,
				.cra_alignmask = 0x3,
				.cra_module = THIS_MODULE,
			}
		}
	},
};

/* shash alg */
static struct shash_alg rts_shash_algs[] = {
	{
		.init = rts_shash_sha256_init,
		.update = rts_shash_sha256_update,
		.final = rts_shash_sha256_final,
		.digestsize = SHA256_DIGEST_SIZE,
		.descsize = sizeof(struct rts_sha_reqctx),
		.base = {
			.cra_name = "sha256",
			.cra_driver_name = "shash-sha256-rlx",
			.cra_priority = 300,
			.cra_blocksize = SHA256_BLOCK_SIZE,
			.cra_ctxsize = sizeof(struct rts_sha_ctx),
			.cra_init = rts_shash_sha256_cra_init,
			.cra_alignmask = 0x3,
			.cra_module = THIS_MODULE,
		}
	},
};

static inline void unregister_algs(void)
{
	int i = 0;

	/* unregister ahash algs */
	for (i = 0; i < sizeof(rts_ahash_algs) / sizeof(struct ahash_alg); i++)
		crypto_unregister_ahash(&rts_ahash_algs[i]);

	/* unregister shash algs */
	for (i = 0; i < sizeof(rts_shash_algs) / sizeof(struct shash_alg); i++)
		crypto_unregister_shash(&rts_shash_algs[i]);
}

static int register_algs(void)
{
	int i = 0, ret;

	/* register ahash algs */
	for (i = 0; i < sizeof(rts_ahash_algs) / sizeof(struct ahash_alg);
	     i++) {
		ret = crypto_register_ahash(&rts_ahash_algs[i]);
		if (ret)
			goto err;
	}

	/* register shash algs */
	for (i = 0; i < sizeof(rts_shash_algs) / sizeof(struct shash_alg);
	     i++) {
		ret = crypto_register_shash(&rts_shash_algs[i]);
		if (ret)
			goto err;
	}

	return 0;
err:
	unregister_algs();
	return ret;
}

static const struct of_device_id rts_sha_dt_ids[] = {
	{ .compatible = "realtek,rts493xa-sha" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_sha_dt_ids);

static irqreturn_t rts_sha_irq_handler(int irq, void *data)
{
	struct rts_sha_data *sdata;
	u32 val;

	sdata = (struct rts_sha_data *)data;
	dev_dbg(&sdata->pdev->dev, "IRQ %d handler\n", irq);

	val = rts_sha_read(sdata, RLX_REG_SHA_IRQ_FLAG);

	val &= RLX_SHA256_DONE_INT;
	if (!val) {
		dev_dbg(&sdata->pdev->dev, "IRQ not from this device\n");
		return IRQ_NONE;
	}

	rts_sha_write(sdata, RLX_REG_SHA_IRQ_FLAG, val);

	complete(&sdata->sha_complete);

	return IRQ_HANDLED;
}

static int rts_sha_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;
	struct rts_sha_data *sdata;

	sdata = devm_kzalloc(&pdev->dev, sizeof(*sdata), GFP_KERNEL);
	if (sdata == NULL)
		return -ENOMEM;

	sdata->pdev = pdev;

	platform_set_drvdata(pdev, sdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&sdata->pdev->dev, "unable to get sha address\n");
		ret = -ENXIO;
		goto mem_err;
	}

	sdata->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(sdata->addr)) {
		dev_err(&sdata->pdev->dev, "unable to ioremap\n");
		ret = -ENXIO;
		goto mem_err;
	}

	sdata->sha_clk = devm_clk_get(&pdev->dev, "sha_ck");
	if (IS_ERR(sdata->sha_clk)) {
		dev_err(&pdev->dev, "clock initialization failed.\n");
		goto mem_err;
	}

	ret = clk_prepare_enable(sdata->sha_clk);
	if (ret) {
		dev_err(&pdev->dev, "clock prepare failed.\n");
		goto mem_err;
	}

	sdata->rst = devm_reset_control_get(&pdev->dev, "rst");
	if (IS_ERR(sdata->rst)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		goto mem_err;
	}

	/* reset sha */
	reset_control_reset(sdata->rst);

	sdata->irq = platform_get_irq(pdev, 0);
	if (sdata->irq < 0) {
		dev_err(&pdev->dev, "can't get IRQ resource\n");
		goto mem_err;
	}

	ret = devm_request_irq(&pdev->dev, sdata->irq, rts_sha_irq_handler,
			       IRQF_SHARED, dev_name(&pdev->dev), sdata);

	dev_dbg(&pdev->dev, "using IRQ channel %d\n", sdata->irq);

	/* register algs */
	ret = register_algs();
	if (ret)
		goto reg_err;

	rts_sdata = sdata;

	mutex_init(&sdata->sha_mutex);
	spin_lock_init(&sdata->splock);
	init_completion(&sdata->sha_complete);

	mdelay(5);

	dev_info(&pdev->dev, "Realtek RLX sha driver initialized\n");

	return 0;

reg_err:
	unregister_algs();
mem_err:
	devm_kfree(&pdev->dev, sdata);
	sdata = NULL;

	return ret;
}

static int rts_sha_remove(struct platform_device *pdev)
{
	struct rts_sha_data *sdata;
	struct resource *res;

	sdata = platform_get_drvdata(pdev);

	/* unregister algs */
	unregister_algs();

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	reset_control_assert(sdata->rst);
	clk_disable_unprepare(sdata->sha_clk);

	devm_kfree(&pdev->dev, sdata);
	dev_set_drvdata(&pdev->dev, NULL);
	sdata = NULL;
	rts_sdata = NULL;

	return 0;
}

static struct platform_driver rts_sha_driver = {
	.probe = rts_sha_probe,
	.remove = rts_sha_remove,
	.driver = {
		.name = "rts-sha",
		.of_match_table = of_match_ptr(rts_sha_dt_ids),
	},
};
module_platform_driver(rts_sha_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek RLX sha driver");
