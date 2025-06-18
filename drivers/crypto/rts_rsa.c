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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/mpi.h>
#include <linux/of.h>

#include <crypto/if_alg.h>
#include <crypto/internal/rsa.h>
#include <crypto/internal/akcipher.h>
#include <crypto/akcipher.h>
#include <crypto/algapi.h>

#include "rts_rsa.h"

//#define RSA_DEBUG

#ifdef RSA_DEBUG
#define sg_printf(sg)                                                \
	do {                                                         \
		int i;                                               \
		pr_info("sg printf:\n");                             \
		for (i = 0; i < (sg)->length; i++)                   \
			pr_info("%02x", *((u8 *)sg_virt((sg)) + i)); \
	} while (0)

#define dump_regs(rdata, s, e)                              \
	do {                                                \
		int i;                                      \
		if (rdata)                                  \
			rts_rsa_set_mode_readmsg(rdata);    \
		pr_info("dump regs:\n");                    \
		for (i = s; i <= e; i += 4)                 \
			pr_info(" 0xb8f0%04x= 0x%08x\n", i, \
				readl(0xb8f00000 + i));     \
	} while (0)
#else
#define sg_printf(sg)
#define dump_regs(rdata, s, n)
#endif

#define safe_free(f, m)           \
	do {                      \
		if (m) {          \
			f(m);     \
			m = NULL; \
		}                 \
	} while (0)

#if 0
void reverse(unsigned char *str, unsigned int len)
{
	unsigned int i;
	unsigned char temp;

	for (i = 0; i < len / 2; i++) {
		temp = str[len - 1 - i];
		str[len - 1 - i] = str[i];
		str[i] = temp;
	}
}
#endif

struct rts_rsa_data {
	struct platform_device *pdev;
	void __iomem *addr;
	int irq;
	struct reset_control *rst;
	struct reset_control *sd;
	struct mutex rsa_mutex;
	struct completion rsa_complete;
	struct clk *rsa_clk;
};

struct rts_rsa_key {
	struct rts_rsa_data *rdata;
	u8 *n;
	u8 *e;
	u8 *d;
	size_t n_sz;
	size_t e_sz;
	size_t d_sz;
};

static struct rts_rsa_data *rts_rdata;

static u32 rsa_npinv_calculate(u32 ulA)
{
	u32 ulM = 0xFFFFFFFF; /* 2^32 - 1 */
	u32 ulE = ulM - ulA + 1;
	u32 ulX = 0;
	u32 ulY = 1;
	u32 ulx = 1;
	u32 uly = 1;
	u32 dwtmp[2] = { 0 };

	while (ulE != 0) {
		dwtmp[0] = ulM / ulE;
		dwtmp[1] = ulM % ulE;

		if (ulM == 0xFFFFFFFF)
			dwtmp[1]++;

		ulM = ulE;
		ulE = dwtmp[1];

		dwtmp[1] = ulY;

		ulY *= dwtmp[0];

		if (ulx == uly) {
			if (ulX >= ulY) {
				ulY = ulX - ulY;
			} else {
				ulY -= ulX;
				uly = 0;
			}
		} else {
			ulY += ulX;
			ulx = 1 - ulx;
			uly = 1 - uly;
		}

		ulX = dwtmp[1];
	}

	if (ulx == 0)
		ulX = 0xFFFFFFFF - ulX + 1;

	return ulX;
}

static inline u8 rts_rsa_readb(struct rts_rsa_data *rdata, u32 reg)
{
	return readb(rdata->addr + reg);
}

static inline void rts_rsa_writeb(struct rts_rsa_data *rdata, u32 reg, u8 val)
{
	writeb(val, rdata->addr + reg);
}

static inline u32 rts_rsa_read(struct rts_rsa_data *rdata, u32 reg)
{
	return readl(rdata->addr + reg);
}

static inline void rts_rsa_write(struct rts_rsa_data *rdata, u32 reg, u32 val)
{
	writel(val, rdata->addr + reg);
}

static inline void rts_rsa_write_mask(struct rts_rsa_data *rdata, u32 reg,
				      u32 val, u32 mask)
{
	u32 value = rts_rsa_read(rdata, reg);

	rts_rsa_write(rdata, reg, (value & (~mask)) | val);
}

static inline void rts_rsa_init(struct rts_rsa_data *rdata)
{
	reset_control_reset(rdata->rst);
	clk_prepare_enable(rdata->rsa_clk);

	/* enable interrupt */
	rts_rsa_write(rdata, RLX_REG_RSA_INT_STATUS, RLX_RSA_INT_STATUS_DONE);
	rts_rsa_write_mask(rdata, RLX_REG_RSA_INT_CTRL, RLX_RSA_INT_CTL_EN,
			   RLX_RSA_INT_CTL_MASK);
}

static inline void rts_rsa_unint(struct rts_rsa_data *rdata)
{
	/* disable interrupt */
	rts_rsa_write_mask(rdata, RLX_REG_RSA_INT_CTRL, 0x0,
			   RLX_RSA_INT_CTL_MASK);
	clk_disable(rdata->rsa_clk);
}

static inline void rts_rsa_set_modn_mode_rr(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE,
			   RLX_RSA_MODE_MODN_CALC_RRMODN,
			   RLX_RSA_MODE_MODN_MASK);
}

static inline void rts_rsa_set_modn_mode_orig_fun(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, RLX_RSA_MODE_MODN_ORIG_FUNC,
			   RLX_RSA_MODE_MODN_MASK);
}

static inline void rts_rsa_set_mode_idle(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, RLX_RSA_MODE_SEL_IDLE,
			   RLX_RSA_MODE_SEL_MASK);
}

static inline void rts_rsa_set_mode_writekey(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, RLX_RSA_MODE_SEL_WRITE_KEY,
			   RLX_RSA_MODE_SEL_MASK);
}

static inline void rts_rsa_set_mode_readmsg(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, RLX_RSA_MODE_SEL_READ_MSG,
			   RLX_RSA_MODE_SEL_MASK);
}

static inline void rts_rsa_set_mode_calc(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, RLX_RSA_MODE_SEL_CALC,
			   RLX_RSA_MODE_SEL_MASK);
}

static inline void rts_rsa_set_mode_shift(struct rts_rsa_data *rdata, u32 val)
{
	rts_rsa_write(rdata, RLX_REG_RSA_MODE_SHIFT, val);
}

static inline void rts_rsa_set_npinv(struct rts_rsa_data *rdata, u32 val)
{
	rts_rsa_write(rdata, RLX_REG_RSA_NP_INV, val);
}

static inline void rts_rsa_start_calc(struct rts_rsa_data *rdata)
{
	rts_rsa_write_mask(rdata, RLX_REG_RSA_CTRL, RLX_RSA_CTRL_START,
			   RLX_RSA_CTRL_START);
}

static int rts_rsa_set_nbits_sel(struct rts_rsa_data *rdata, unsigned int nbits)
{
	switch (nbits) {
	case 512:
		nbits = RLX_RSA_MODE_NBITS_512;
		break;
	case 1024:
		nbits = RLX_RSA_MODE_NBITS_1024;
		break;
	case 2048:
		nbits = RLX_RSA_MODE_NBITS_2048;
		break;
	case 3072:
		nbits = RLX_RSA_MODE_NBITS_3072;
		break;
	default:
		return -EINVAL;
	}

	rts_rsa_write_mask(rdata, RLX_REG_RSA_MODE, nbits,
			   RLX_RSA_MODE_NBITS_MASK);

	return 0;
}

static void rts_rsa_set_ekey(struct rts_rsa_data *rdata, unsigned int keylen)
{
	u32 data_buf_tmp = 0x0C40;

	switch (keylen) {
	case 384:
		data_buf_tmp = 0x0C40;
		break;
	case 256:
		data_buf_tmp = 0x0840;
		break;
	case 128:
		data_buf_tmp = 0x0440;
		break;
	default:
		keylen = 0x180;
		break;
	}

	rts_rsa_set_mode_writekey(rdata);
	memset(rdata->addr + RLX_RSA_RAM_DEKEY_BASE_ADDR, 0, keylen);
	rts_rsa_write(rdata, RLX_RSA_RAM_DEKEY_BASE_ADDR, data_buf_tmp);
}

static void rts_rsa_set_rrmodn(struct rts_rsa_data *rdata,
			       unsigned int rrmodnlen)
{
	u32 data_buf_tmp = 0x80000000;

	rts_rsa_set_mode_writekey(rdata);
	memset(rdata->addr + RLX_RSA_RAM_RRMODN_BASE_ADDR, 0, rrmodnlen);
	rts_rsa_write(rdata, RLX_RSA_RAM_RRMODN_BASE_ADDR + rrmodnlen - 4,
		      data_buf_tmp);
}

static int rts_rsa_rrmodn_proc(struct rts_rsa_data *rdata, u32 npinv,
			       unsigned int keylen)
{
	int ret = 0;

	//Init eKey
	rts_rsa_set_ekey(rdata, keylen);

	//Init RR Mod N
	rts_rsa_set_rrmodn(rdata, keylen);

	//RSA Mode Shift
	rts_rsa_set_mode_shift(rdata, 0x0042);

	//RSA NP INV
	rts_rsa_set_npinv(rdata, npinv);

	dump_regs(NULL, 0x0, 0x14);
	dump_regs(rdata, 0x1000, 0x1800);

	//Start Calculation
	rts_rsa_set_mode_calc(rdata);
	rts_rsa_start_calc(rdata);

	dev_dbg(&rdata->pdev->dev, "wait rrmodn proc complete\n");

	ret = wait_for_completion_timeout(&rdata->rsa_complete,
					  msecs_to_jiffies(10000));
	if (ret == 0) {
		dev_err(&rdata->pdev->dev, "timed out\n");
		return -ETIMEDOUT;
	}

	dev_dbg(&rdata->pdev->dev, "rts rsa rrmodn proc complete\n");
	return 0;
}

static int rts_rsa_load_nkey_proc(struct rts_rsa_data *rdata, u8 *n,
				  unsigned int keylen)
{
	int i;
	u32 npinv = 0;

	rts_rsa_set_mode_writekey(rdata);

	for (i = 0; i < keylen; i++)
		rts_rsa_writeb(
			rdata,
			/*			RLX_RSA_RAM_NKEY_BASE_ADDR + i, *(n + i)); */
			RLX_RSA_RAM_NKEY_BASE_ADDR + keylen - 1 - i, *(n + i));

	/* npinv calculate*/
	/*	npinv = *(u32 *)n; */
	npinv = cpu_to_be32(*(u32 *)(n + keylen - 4));
	dev_dbg(&rdata->pdev->dev, "npinv= 0x%08x\n", npinv);
	npinv = rsa_npinv_calculate(npinv);

	return rts_rsa_rrmodn_proc(rdata, npinv, keylen);
}

static int rts_rsa_load_dekey_proc(struct rts_rsa_data *rdata, u8 *d_e,
				   unsigned int d_e_sz)
{
	int i;

	rts_rsa_set_mode_writekey(rdata);

	for (i = 0; i < d_e_sz; i++)
		rts_rsa_writeb(
			rdata,
			/*			RLX_RSA_RAM_DEKEY_BASE_ADDR + i, *(d_e + i)); */
			RLX_RSA_RAM_DEKEY_BASE_ADDR + d_e_sz - 1 - i,
			*(d_e + i));

	return 0;
}

static int rts_rsa_load_data_proc(struct rts_rsa_data *rdata, u8 *in,
				  unsigned int datalen)
{
	int i;

	rts_rsa_set_mode_writekey(rdata);

	for (i = 0; i < datalen; i++)
		rts_rsa_writeb(rdata,
			       RLX_RSA_RAM_PCTEXT_BASE_ADDR + datalen - 1 - i,
			       *(in + i));

	return 0;
}

static int rts_rsa_do_crypt(struct rts_rsa_data *rdata)
{
	int ret = 0;

	dump_regs(NULL, 0x0, 0x14);
	dump_regs(rdata, 0x1000, 0x1800);

	//Change RSA MODE to ORIGINAL FUNC
	rts_rsa_set_modn_mode_orig_fun(rdata);

	//Start Calculation
	rts_rsa_set_mode_calc(rdata);
	rts_rsa_start_calc(rdata);

	dev_dbg(&rdata->pdev->dev, "wait do crypt complete\n");

	ret = wait_for_completion_timeout(&rdata->rsa_complete,
					  msecs_to_jiffies(10000));
	if (ret == 0) {
		dev_err(&rdata->pdev->dev, "timed out\n");
		return -ETIMEDOUT;
	}

	dev_dbg(&rdata->pdev->dev, "rts rsa do crypt complete\n");
	return 0;
}

static int rts_rsa_get_result(struct rts_rsa_data *rdata, u8 *out,
			      unsigned int datalen)
{
	int i;
	u32 addr;

	rts_rsa_set_mode_readmsg(rdata);

	if ((rts_rsa_read(rdata, RLX_REG_RSA_MODE) &
	     RLX_RSA_MODE_MODN_CALC_RRMODN) ||
	    (rts_rsa_read(rdata, RLX_REG_RSA_CTRL) & RLX_RSA_CTRL_READ_RRMODN))
		addr = RLX_RSA_RAM_RRMODN_BASE_ADDR;
	else
		addr = RLX_RSA_RAM_PCTEXT_BASE_ADDR;

	for (i = 0; i < datalen; i++)
		*(out + i) = rts_rsa_readb(rdata, addr + datalen - 1 - i);

	rts_rsa_set_mode_idle(rdata);

	return 0;
}

static int _rts_rsa_crypt(struct rts_rsa_data *rdata, u8 *in, u8 *out, u8 *n,
			  size_t n_sz, u8 *d_e, size_t d_e_sz)
{
	int ret = 0;
	unsigned int datalen = n_sz;
	unsigned int keylen = n_sz;
	unsigned int nbits = keylen << 3;

	dev_dbg(&rdata->pdev->dev, "_rts rsa crypt\n");
	dev_dbg(&rdata->pdev->dev, "datalen:%d, keylen:%d, nbits:%d\n", datalen,
		keylen, nbits);

	mutex_lock(&rdata->rsa_mutex);
	rts_rsa_init(rdata);

	/* Change Mode to RR Mod N */
	rts_rsa_set_modn_mode_rr(rdata);

	/* set nbits sel */
	ret = rts_rsa_set_nbits_sel(rdata, nbits);
	if (ret) {
		dev_err(&rdata->pdev->dev, "rts_rsa_set_nbits_sel failed\n");
		return ret;
	}

	//Set nKey
	ret = rts_rsa_load_nkey_proc(rdata, n, keylen);
	if (ret) {
		dev_err(&rdata->pdev->dev, "rts_rsa_load_nkey_proc failed\n");
		return ret;
	}

	//Set dKey
	rts_rsa_load_dekey_proc(rdata, d_e, d_e_sz);

	//Set In Data
	rts_rsa_load_data_proc(rdata, in, datalen);

	//Start RSA Proc
	ret = rts_rsa_do_crypt(rdata);
	if (ret) {
		dev_err(&rdata->pdev->dev, "rts_rsa_do_crypt failed\n");
		return ret;
	}

	//Get result
	rts_rsa_get_result(rdata, out, datalen);

	rts_rsa_unint(rdata);
	mutex_unlock(&rdata->rsa_mutex);

	dev_dbg(&rdata->pdev->dev, "_rts rsa crypt complete\n");
	return ret;
}

static int rts_rsa_crypt(struct rts_rsa_data *rdata,
			 struct akcipher_request *req, u8 *n, size_t n_sz,
			 u8 *d_e, size_t d_e_sz)
{
	u8 *in = NULL, *out = NULL;
	int ret = 0, nents, s_of;

	dev_dbg(&rdata->pdev->dev, "rts rsa crypt\n");
	dev_dbg(&rdata->pdev->dev, "src_len:%d, dst_len:%d\n", req->src_len,
		req->dst_len);

	if (unlikely(!n || !d_e)) {
		dev_err(&rdata->pdev->dev, "key is null\n");
		return -EINVAL;
	}

	/* modify for rsa-pkcs1pad.c (missing the first '00') */
	s_of = n_sz - req->src_len;
	if (s_of && s_of != 1) {
		dev_err(&rdata->pdev->dev, "invalid src_len\n");
		return -EINVAL;
	}

	if (req->dst_len < n_sz) {
		req->dst_len = n_sz;
		dev_err(&rdata->pdev->dev,
			"Out buf len less than n key size\n");
		return -EOVERFLOW;
	}

	in = kmalloc(n_sz, GFP_KERNEL);
	if (!in) {
		ret = -ENOMEM;
		goto err;
	}

	//	sg_printf(req->src);

	in[0] = '\0';
	nents = sg_nents(req->src);
	sg_copy_to_buffer(req->src, nents, in + s_of, req->src_len);

	out = kmalloc(n_sz, GFP_KERNEL);
	if (!out) {
		ret = -ENOMEM;
		goto err;
	}

	ret = _rts_rsa_crypt(rdata, in, out, n, n_sz, d_e, d_e_sz);
	if (ret) {
		dev_err(&rdata->pdev->dev, "_rts_rsa_crypt failed, ret = %d\n",
			ret);
		goto err;
	}

	nents = sg_nents(req->dst);
	sg_copy_from_buffer(req->dst, nents, out, n_sz);

	//	sg_printf(req->dst);

	dev_dbg(&rdata->pdev->dev, "rts rsa crypt complete\n");
err:
	safe_free(kfree, in);
	safe_free(kfree, out);
	return ret;
}

static int rts_rsa_enc(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	const struct rts_rsa_key *key = akcipher_tfm_ctx(tfm);
	struct rts_rsa_data *rdata = key->rdata;

	dev_dbg(&rdata->pdev->dev, "rts rsa enc\n");

	return rts_rsa_crypt(rdata, req, key->n, key->n_sz, key->e, key->e_sz);
}

static int rts_rsa_dec(struct akcipher_request *req)
{
	struct crypto_akcipher *tfm = crypto_akcipher_reqtfm(req);
	const struct rts_rsa_key *key = akcipher_tfm_ctx(tfm);
	struct rts_rsa_data *rdata = key->rdata;

	dev_dbg(&rdata->pdev->dev, "rts rsa dec\n");

	return rts_rsa_crypt(rdata, req, key->n, key->n_sz, key->d, key->d_sz);
}

static void rsa_free_key(struct rts_rsa_key *key)
{
	safe_free(kfree_sensitive, key->d);
	safe_free(kfree, key->e);
	safe_free(kfree, key->n);
	key->d_sz = 0;
	key->e_sz = 0;
	key->n_sz = 0;
}

static u8 *rsa_read_raw_data(const u8 *buf, size_t *nbytes)
{
	u8 *val;

	/* skip leading zeros */
	while (!*buf && *nbytes) {
		buf++;
		(*nbytes)--;
	}

	val = kzalloc(*nbytes, GFP_KERNEL);
	if (!val)
		return NULL;

	memcpy(val, buf, *nbytes);

	return val;
}

static int rsa_check_key_length(unsigned int len)
{
	switch (len) {
		/*	case 512: */
	case 1024:
	case 2048:
	case 3072:
		return 0;
	}

	return -EINVAL;
}

static int rts_rsa_set_pub_key(struct crypto_akcipher *tfm, const void *key,
			       unsigned int keylen)
{
	struct rts_rsa_key *pkey = akcipher_tfm_ctx(tfm);
	struct rsa_key raw_key = { 0 };
	int ret;

	dev_dbg(&rts_rdata->pdev->dev, "rts rsa set pub key\n");

	/* Free the old key if any */
	rsa_free_key(pkey);

	ret = rsa_parse_pub_key(&raw_key, key, keylen);
	if (ret)
		return ret;

	pkey->e = kzalloc(raw_key.e_sz, GFP_KERNEL);
	if (!pkey->e)
		goto err;

	/* n key skip leading zeros from ASN.1 */
	pkey->n = rsa_read_raw_data(raw_key.n, &raw_key.n_sz);
	if (!pkey->n)
		goto err;

	if (rsa_check_key_length(raw_key.n_sz << 3)) {
		rsa_free_key(pkey);
		return -EINVAL;
	}

	pkey->e_sz = raw_key.e_sz;
	pkey->n_sz = raw_key.n_sz;

	memcpy(pkey->e, raw_key.e, raw_key.e_sz);

	dev_dbg(&rts_rdata->pdev->dev, "n_sz:%d, e_sz:%d\n", pkey->n_sz,
		pkey->e_sz);

	return 0;
err:
	rsa_free_key(pkey);
	return -ENOMEM;
}

static int rts_rsa_set_priv_key(struct crypto_akcipher *tfm, const void *key,
				unsigned int keylen)
{
	struct rts_rsa_key *pkey = akcipher_tfm_ctx(tfm);
	struct rsa_key raw_key = { 0 };
	int ret;

	dev_dbg(&rts_rdata->pdev->dev, "rts rsa set priv key\n");

	/* Free the old key if any */
	rsa_free_key(pkey);

	ret = rsa_parse_priv_key(&raw_key, key, keylen);
	if (ret)
		return ret;

	dev_dbg(&rts_rdata->pdev->dev, "n_sz:%d, e_sz:%d, d_sz:%d\n",
		raw_key.n_sz, raw_key.e_sz, raw_key.d_sz);

	/* skip leading zeros from ASN.1 */
	pkey->d = rsa_read_raw_data(raw_key.d, &raw_key.d_sz);
	if (!pkey->d)
		goto err;

	/* 65537: 010001 */
	pkey->e = rsa_read_raw_data(raw_key.e, &raw_key.e_sz);
	if (!pkey->e)
		goto err;

	/* skip leading zeros from ASN.1 */
	pkey->n = rsa_read_raw_data(raw_key.n, &raw_key.n_sz);
	if (!pkey->n)
		goto err;

	if (rsa_check_key_length(raw_key.n_sz << 3)) {
		rsa_free_key(pkey);
		return -EINVAL;
	}

	pkey->d_sz = raw_key.d_sz;
	pkey->e_sz = raw_key.e_sz;
	pkey->n_sz = raw_key.n_sz;

	dev_dbg(&rts_rdata->pdev->dev, "n_sz:%d, e_sz:%d, d_sz:%d\n",
		pkey->n_sz, pkey->e_sz, pkey->d_sz);

	return 0;
err:
	rsa_free_key(pkey);
	return -ENOMEM;
}

static unsigned int rts_rsa_max_size(struct crypto_akcipher *tfm)
{
	struct rts_rsa_key *key = akcipher_tfm_ctx(tfm);

	return key->n ? key->n_sz : -EINVAL;
}

static int rts_rsa_init_tfm(struct crypto_akcipher *tfm)
{
	struct rts_rsa_key *key = akcipher_tfm_ctx(tfm);

	key->rdata = rts_rdata;

	return 0;
}

static void rts_rsa_exit_tfm(struct crypto_akcipher *tfm)
{
	struct rts_rsa_key *key = akcipher_tfm_ctx(tfm);

	rsa_free_key(key);
}

static struct akcipher_alg rts_rsa_alg = {
	.encrypt = rts_rsa_enc,
	.decrypt = rts_rsa_dec,
	.set_priv_key = rts_rsa_set_priv_key,
	.set_pub_key = rts_rsa_set_pub_key,
	.max_size = rts_rsa_max_size,
	.init = rts_rsa_init_tfm,
	.exit = rts_rsa_exit_tfm,
	.base = {
		.cra_name = "rsa",
		.cra_driver_name = "rsa-rlx",
		.cra_priority = 300,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct rts_rsa_key),
	},
};

static inline void unregister_algs(void)
{
	crypto_unregister_akcipher(&rts_rsa_alg);
}

static int register_algs(void)
{
	int ret = 0;

	ret = crypto_register_akcipher(&rts_rsa_alg);
	if (ret)
		unregister_algs();

	return ret;
}

static const struct of_device_id rts_rsa_dt_ids[] = {
	{ .compatible = "realtek,rts493xa-rsa" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, rts_rsa_dt_ids);

static irqreturn_t rts_rsa_irq_handler(int irq, void *data)
{
	struct rts_rsa_data *rdata = (struct rts_rsa_data *)data;
	u32 val;

	dev_dbg(&rdata->pdev->dev, "IRQ %d handler\n", irq);
	val = rts_rsa_read(rdata, RLX_REG_RSA_INT_STATUS);

	val &= RLX_RSA_INT_STATUS_DONE;
	if (!val) {
		dev_dbg(&rdata->pdev->dev, "IRQ not from this device\n");
		return IRQ_NONE;
	}

	rts_rsa_write(rdata, RLX_REG_RSA_INT_STATUS, val);

	complete(&rdata->rsa_complete);

	return IRQ_HANDLED;
}

static int rts_rsa_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	struct rts_rsa_data *rdata;

	rdata = devm_kzalloc(&pdev->dev, sizeof(*rdata), GFP_KERNEL);
	if (rdata == NULL)
		return -ENOMEM;

	rdata->pdev = pdev;

	platform_set_drvdata(pdev, rdata);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&rdata->pdev->dev, "unable to get rsa address\n");
		ret = -ENXIO;
		goto mem_err;
	}

	rdata->addr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rdata->addr)) {
		dev_err(&rdata->pdev->dev, "unable to ioremap\n");
		ret = -ENXIO;
		goto mem_err;
	}

	rdata->rsa_clk = devm_clk_get(&pdev->dev, "rsa_ck");
	if (IS_ERR(rdata->rsa_clk)) {
		dev_err(&pdev->dev, "clock initialization failed.\n");
		goto mem_err;
	}

	ret = clk_prepare_enable(rdata->rsa_clk);
	if (ret) {
		dev_err(&pdev->dev, "clock prepare failed.\n");
		goto mem_err;
	}

	/* mem up */
	rdata->sd = devm_reset_control_get(&pdev->dev, "sd");
	if (IS_ERR(rdata->sd)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		goto mem_err;
	}

	reset_control_deassert(rdata->sd);

	/* reset rsa */
	rdata->rst = devm_reset_control_get(&pdev->dev, "rst");
	if (IS_ERR(rdata->rst)) {
		dev_err(&pdev->dev, "no top level reset found.\n");
		goto mem_err;
	}

	reset_control_reset(rdata->rst);

	rdata->irq = platform_get_irq(pdev, 0);
	if (rdata->irq < 0) {
		dev_err(&pdev->dev, "can't get IRQ resource\n");
		goto mem_err;
	}

	ret = devm_request_irq(&pdev->dev, rdata->irq, rts_rsa_irq_handler,
			       IRQF_SHARED, dev_name(&pdev->dev), rdata);

	dev_dbg(&pdev->dev, "using IRQ channel %d\n", rdata->irq);

	/* register algs */
	ret = register_algs();
	if (ret)
		goto reg_err;

	rts_rdata = rdata;
	mutex_init(&rdata->rsa_mutex);
	init_completion(&rdata->rsa_complete);

	dev_info(&pdev->dev, "Realtek RLX rsa driver initialized\n");

	return 0;
reg_err:
	unregister_algs();
mem_err:
	devm_kfree(&pdev->dev, rdata);
	rdata = NULL;

	return ret;
}

static int rts_rsa_remove(struct platform_device *pdev)
{
	struct rts_rsa_data *rdata;
	struct resource *res;

	rdata = platform_get_drvdata(pdev);

	reset_control_assert(rdata->sd);

	/* unregister algs */
	unregister_algs();

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res)
		release_mem_region(res->start, resource_size(res));

	reset_control_assert(rdata->rst);
	clk_disable_unprepare(rdata->rsa_clk);

	devm_kfree(&pdev->dev, rdata);
	dev_set_drvdata(&pdev->dev, NULL);
	rdata = NULL;
	rts_rdata = NULL;

	return 0;
}

static struct platform_driver rts_rsa_driver = {
	.probe = rts_rsa_probe,
	.remove = rts_rsa_remove,
	.driver = {
		.name = "rts-rsa",
		.of_match_table = of_match_ptr(rts_rsa_dt_ids),
	},
};
module_platform_driver(rts_rsa_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Realtek RLX rsa driver");
