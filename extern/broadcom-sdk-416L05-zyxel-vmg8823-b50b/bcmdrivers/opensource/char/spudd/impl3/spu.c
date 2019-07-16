/*
<:copyright-BRCM:2015:GPL/GPL:spu

   Copyright (c) 2015 Broadcom 
   All Rights Reserved

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/types.h>
#include <crypto/algapi.h>
#include <linux/rtnetlink.h>
#include <crypto/aead.h>
#include <crypto/authenc.h>
#include <crypto/aes.h>
#include <crypto/des.h>
#include <crypto/sha.h>
#include <crypto/md5.h>
#include <crypto/scatterwalk.h>
#include <linux/skbuff.h>
#include <linux/nbuff.h>
#include <linux/ip.h>

#include <bcmspudrv.h>
#include <bcm_mm.h>
#include "spu.h"
#include "spu_runner.h"
#include "spu_blog.h"

struct spu_info *spuinfo = NULL;

typedef void (*IPSec_SPU_FN_IOCTL) (unsigned long arg);
static int  spu_open (struct inode *inode, struct file *filp);
static long spu_ioctl_unlocked(struct file *filep, unsigned int cmd, unsigned long arg);
static int  spu_release (struct inode *inode, struct file *filp);

static void spu_dump_data( struct aead_request *areq, char *giv, char *pId, int ingress)
{
#if defined(SPU_PACKET_TRACE)
    struct crypto_aead *aead;
    struct scatterlist *sg;
    int                 i;
    unsigned int        totalSize;
    unsigned int        dispCnt;
    char               *ptr;

    aead = crypto_aead_reqtfm(areq);
    sg   = areq->assoc;
    totalSize = 0;
    while ( sg )
    {
        totalSize += sg->length;
        sg = scatterwalk_sg_next(sg);
    }

    totalSize += crypto_aead_ivsize(aead);

    if ( 1 == ingress )
    {
        sg = areq->src;
    }
    else
    {
        sg = areq->dst;
    }
    while ( sg )
    {
        totalSize += sg->length;
        sg = scatterwalk_sg_next(sg);
    }

    printk("%s: data length %d (display max is %d)\n", pId, totalSize, (64 + areq->assoclen + crypto_aead_ivsize(aead)));
    dispCnt = 0;
    sg = areq->assoc;
    while ( sg )
    {
        ptr = (char *)page_address(sg_page(sg)) + sg->offset;
        for ( i = 0; i < sg->length; i++)
        {
           printk("%02x ", ptr[i]);
           if ( 0 == ((dispCnt+1) & 0x1F))
           {
              printk("\n");
           }
           dispCnt++;
        }
        sg = scatterwalk_sg_next(sg);
    }

    if ( giv )
    {
        for ( i = 0; i < crypto_aead_ivsize(aead); i++)
        {
           printk("%02x ", giv[i]);
           if ( 0 == ((dispCnt+1) & 0x1F))
           {
              printk("\n");
           }
           dispCnt++;
        }
    }
    else
    {
        for ( i = 0; i < crypto_aead_ivsize(aead); i++)
        {
           printk("xx ");
           if ( 0 == ((dispCnt+1) & 0x1F))
           {
              printk("\n");
           }
           dispCnt++;
        }
    }

    if ( 1 == ingress )
    {
        sg = areq->src;
    }
    else
    {
        sg = areq->dst;
    }
    while ( sg )
    {
        ptr = (char *)page_address(sg_page(sg)) + sg->offset;
        for ( i = 0; i < sg->length; i++)
        {
           printk("%02x ", ptr[i]);
           if ( 0 == ((dispCnt+1) & 0x1F))
           {
              printk("\n");
           }
           dispCnt++;
           /* print out max of 64 bytes of data after the assoc data */
           if (dispCnt >= (64 + areq->assoclen + crypto_aead_ivsize(aead))) break;
        }
        sg = scatterwalk_sg_next(sg);
    }
    printk("\n");
#endif
}

/* allocate a transfer request and initialize to 0 */
struct spu_trans_req *spu_alloc_trans_req(unsigned int cryptoflags)
{
    struct spu_trans_req *pTransReq = NULL;
    unsigned long         flags;

    spin_lock_irqsave(&spuinfo->spuListLock, flags);

    if ( list_empty(&spuinfo->transReqList) ) {
       SPU_TRACE(("exhausted transfer requests\n"));
       spin_unlock_irqrestore(&spuinfo->spuListLock, flags);
       return ERR_PTR(-EBUSY);
    }

    pTransReq = list_first_entry(&spuinfo->transReqList, struct spu_trans_req, entry);
    list_del(&pTransReq->entry);

    spin_unlock_irqrestore(&spuinfo->spuListLock, flags);
    /* do not initialize index or list */
    memset(&pTransReq->context, 0, sizeof(*pTransReq) - offsetof(struct spu_trans_req, context));
    return pTransReq;
} /* spu_alloc_trans_req */

void spu_free_trans_req(struct spu_trans_req *pTransReq)
{
   unsigned long         flags;
   struct spu_trans_req *pReqFree;

   spin_lock_irqsave(&spuinfo->spuListLock, flags);
   list_for_each_entry(pReqFree, &spuinfo->transReqList, entry)
   {
      if ( pReqFree->index == pTransReq->index )
      {
         /* buffer was freed twice */
         SPU_TRACE(("Transfer request already in free list\n"));
         spin_unlock_irqrestore(&spuinfo->spuListLock, flags);
         return;
      }
   }
   list_add_tail(&pTransReq->entry, &spuinfo->transReqList);
   spin_unlock_irqrestore(&spuinfo->spuListLock, flags);
}

static int spu_aead_need_fallback_skb(struct aead_request *areq, int *pSrcLen)
{
    uint8_t             num_frags;
    struct scatterlist *sg;
    int                 srcLen = 0;
    int                 retVal = 0;
    struct sk_buff     *pSkb;

    do 
    {
        num_frags = 0;
        sg = areq->src;
        while ( sg )
        {
            srcLen += sg->length;
            num_frags++;
            sg = scatterwalk_sg_next(sg);
        }
        if ( num_frags > 1 )
        {
            retVal = 1;
            break;
        }
    
        num_frags = 0;
        sg = areq->dst;
        while ( sg )
        {
            num_frags++;
            sg = scatterwalk_sg_next(sg);
        }
        if ( num_frags > 1 )
        {
            retVal = 1;
            break;
        }

        num_frags = 0;
        sg = areq->assoc;
        while ( sg )
        {
            num_frags++;
            sg = scatterwalk_sg_next(sg);
        }
        if ( num_frags > 1 )
        {
            retVal = 1;
            break;
        }
    } while ( 0 );

    pSkb = areq->base.data;
    if (atomic_read(&pSkb->users) != 1)
    {
        SPU_TRACE(("SKB has multiple users\n"));
        retVal = 1;
    }

    if ( pSrcLen != NULL ) *pSrcLen = srcLen;
    return retVal;
}

static struct sk_buff *spu_skb_alloc_and_copy(struct aead_request *areq, unsigned char *giv, int srcLen)
{
    struct crypto_aead *aead;
    struct sk_buff     *pSkb;
    struct scatterlist *sg;
    char               *pData;
    void               *ptr;
    int                 len;
 
    /* alocate data for assoc data and IV, IV may not need to be copied */
    aead   = crypto_aead_reqtfm(areq);
    len    = srcLen + areq->assoclen + crypto_aead_ivsize(aead);
    pSkb   = alloc_skb(len, GFP_ATOMIC);
    if ( NULL == pSkb ) {
       return ERR_PTR(-ENOMEM);
    }

    /* call to skb_reserve not requried - do not need any headroom */
    skb_put(pSkb, len);

    pData = pSkb->data;
    sg    = areq->assoc;
    len   = areq->assoclen;
    while((len > 0) && (sg))
    {
        ptr = page_address(sg_page(sg)) + sg->offset;
        memcpy(pData, ptr, sg->length);
        len   -= sg->length;
        pData += sg->length;
        sg = scatterwalk_sg_next(sg);
    }

    if ( (len != 0) || (sg != NULL))
    {
        kfree_skb(pSkb);
        return ERR_PTR(-EINVAL);
    }

    /* copy iv if pointer is set, otherwise just leave room for it */
    if ( giv )
    {
        /* copy the IV to the buffer */
        memcpy(pData, giv, crypto_aead_ivsize(aead));
    }
    pData += crypto_aead_ivsize(aead);

    sg  = areq->src;
    len = srcLen;
    while((len > 0) && (sg))
    {
        ptr = page_address(sg_page(sg)) + sg->offset;
        memcpy(pData, ptr, sg->length);
        len   -= sg->length;
        pData += sg->length;
        sg = scatterwalk_sg_next(sg);
    }

    if ( (len != 0) || (sg != NULL))
    {
        kfree_skb(pSkb);
        return ERR_PTR(-EINVAL);
    }
    return pSkb;
}

static void spu_skb_copy_and_free(struct spu_trans_req *pTransReq)
{
    struct aead_request *areq = pTransReq->context;
    struct crypto_aead  *aead;
    struct scatterlist  *sg;
    char                *pData;
    void                *ptr;
    int                  srcLen;

    aead   = crypto_aead_reqtfm(areq);
    pData  = pTransReq->pAllocSkb->data + areq->assoclen;
    if ( pTransReq->giv )
    {
        memcpy(pTransReq->giv, pData, crypto_aead_ivsize(aead));
    }
    
    pData  += crypto_aead_ivsize(aead);
    srcLen  = pTransReq->pAllocSkb->len - areq->assoclen - crypto_aead_ivsize(aead);
    sg      = areq->dst;
    while((srcLen > 0) && (sg))
    {
        ptr = page_address(sg_page(sg)) + sg->offset;
        memcpy(ptr, pData, sg->length);
        srcLen -= sg->length;
        pData  += sg->length;
        sg = scatterwalk_sg_next(sg);
    }

    kfree_skb(pTransReq->pAllocSkb);
    pTransReq->pAllocSkb = NULL;

    return;
}


static void spu_esp_crypt_done (struct spu_trans_req *transReq)
{
    struct aead_request *areq = transReq->context;

    if ( transReq->pAllocSkb )
    {
        /* copy data and free alloacted skb even if there 
           was an error */
        spu_skb_copy_and_free(transReq);
    }

    if ( 0 == transReq->err )
    {
       if ( SPU_DIRECTION_US == transReq->pSpuCtx->direction )
       {
           spuinfo->stats.encSpuEgress++;
       }
       else
       {
           spuinfo->stats.decSpuEgress++;
       }
    }

    spu_dump_data(areq, transReq->giv ? transReq->giv : areq->iv, "spu_esp_crypt_done", 0);

    aead_request_complete (areq, transReq->err);

    return;
} /* spu_esp_crypt_done */

static void spu_compute_aes_dec_key(struct spu_ctx *ctx, unsigned char *dest)
{
    struct crypto_aes_ctx gen_aes_key;
    int                   key_pos;
    int                   key_len;

    memset(&gen_aes_key, 0, sizeof(struct crypto_aes_ctx));

    key_len = (ctx->descAlg >> BCM_DESC_ENCR_KEYLEN_SHIFT) & BCM_DESC_ENCR_KEYLEN_MASK;
    crypto_aes_expand_key(&gen_aes_key, &ctx->encrypt_key[0], key_len);
    key_pos = key_len + 24;
    memcpy(dest, &gen_aes_key.key_enc[key_pos], 16);
    switch (key_len) {
        case AES_KEYSIZE_256:
            key_pos -= 2;
            /* fall */
        case AES_KEYSIZE_192:
            key_pos -= 2;
            memcpy(&dest[16], &gen_aes_key.key_enc[key_pos], 16);
            break;
    }
    
#if defined(SPU_KEY_TRACE)
{
    int i;
    printk("compute_aes_dec_key: len %d, ", key_len);
    for(i = 0; i < key_len; i++)
    {
      printk("%02x", dest[i]);
    }
    printk("\n");
}
#endif
}

static int spu_aead_setauthsize (struct crypto_aead *aead, 
                                 unsigned int authsize)
{
    if (authsize != SPU_MAX_ICV_LENGTH)
    {
        return -EINVAL;
    }

    return 0;
} /* spu_aead_setauthsize */

static int spu_aead_setkey (struct crypto_aead *aead, 
                            const u8           *key, 
                            unsigned int        keylen)
{
    struct spu_ctx *ctx = crypto_aead_ctx (aead);
    struct rtattr *rta;
    struct crypto_authenc_key_param *param;
    unsigned int authkeylen;
    unsigned int enckeylen;
    const uint8_t *pKey = key;
    unsigned int length = keylen;
    
    rta = (struct rtattr *)pKey;
    if (!RTA_OK (rta, length))
        goto badkey;

    if (rta->rta_type != CRYPTO_AUTHENC_KEYA_PARAM)
        goto badkey;

    if (RTA_PAYLOAD (rta) < sizeof (*param))
        goto badkey;

    param = RTA_DATA (rta);
    enckeylen = be32_to_cpu (param->enckeylen);

    pKey += RTA_ALIGN (rta->rta_len);
    length -= RTA_ALIGN (rta->rta_len);
    if (length < enckeylen)
    {
        goto badkey;
    }
    authkeylen = length - enckeylen;

    if ( spu_runner_descr_key_validate(enckeylen, authkeylen) < 0 )
    {
        goto badkey;
    }

    ctx->update = 1;
    memcpy (ctx->auth_key, pKey, authkeylen);
    memcpy (ctx->encrypt_key, (pKey + authkeylen), enckeylen);
    ctx->descAlg |= (authkeylen & BCM_DESC_AUTH_KEYLEN_MASK) << BCM_DESC_AUTH_KEYLEN_SHIFT;
    ctx->descAlg |= (enckeylen & BCM_DESC_ENCR_KEYLEN_MASK) << BCM_DESC_ENCR_KEYLEN_SHIFT;
    if (ctx->descAlg & BCM_DESC_ENCR_ALG_AES)
    {
        spu_compute_aes_dec_key(ctx, &ctx->decrypt_key[0]);
    }

    /* this is unlikely but check anyway */
    if (ctx->direction != SPU_DIRECTION_INVALID)
    {
       spu_runner_descr_config(ctx);
    }

#if defined(SPU_KEY_TRACE)
{
    int i;

    printk("spu_aead_setkey - akey len %d, ", authkeylen);
    for(i = 0; i < authkeylen; i++)
    {
        printk("%02x", ctx->auth_key[i]);
    }
    printk("\n");

    printk("spu_aead_setkey - ekey len %d, ", enckeylen);
    for(i = 0; i < enckeylen; i++)
    {
        printk("%02x", ctx->encrypt_key[i]);
    }
    printk("\n");
}
#endif
    return 0;

badkey:
    SPU_TRACE(("spu_aead_setkey: Invlaid key\n"));
    crypto_aead_set_flags (aead, CRYPTO_TFM_RES_BAD_KEY_LEN);

    return -EINVAL;
} /* spu_aead_setkey */

static int spu_aead_givencrypt(struct aead_givcrypt_request *req)
{
    struct aead_request      *areq = &req->areq;
    struct crypto_aead       *aead;
    struct spu_trans_req     *pTransReq;
    int                       ret;
    struct spu_ctx           *pCtx;
    int                       srcLen = 0;
    struct sk_buff           *pSkb = NULL;
    unsigned int              offset;

    spu_dump_data(areq, NULL, "spu_aead_givencrypt", 1);

    spuinfo->stats.encIngress++;
    if (spu_aead_need_fallback_skb(areq, &srcLen))
    {
        pSkb = spu_skb_alloc_and_copy(areq, NULL, srcLen);
        if ( IS_ERR(pSkb) )
        {
           spuinfo->stats.encDrops++;
           return PTR_ERR(pSkb);
        }
        spuinfo->stats.encFallback++;
    }

    pTransReq = spu_alloc_trans_req(areq->base.flags);
    if (IS_ERR(pTransReq))
    {
        spuinfo->stats.encDrops++;
        return PTR_ERR(pTransReq);
    }

    aead = crypto_aead_reqtfm(areq);
    pCtx = crypto_aead_ctx(aead);
    if ( pCtx->direction == SPU_DIRECTION_INVALID )
    {
       pCtx->next_hdr  = areq->next_hdr;
       pCtx->direction = SPU_DIRECTION_US;
       spu_runner_descr_config(pCtx);
    }
    pTransReq->context  = areq;
    pTransReq->callback = spu_esp_crypt_done;
    pTransReq->pSpuCtx  = pCtx;
    if ( pSkb != NULL )
    {
       pTransReq->pAllocSkb = pSkb;
       offset = 0;
    }
    else
    {
       pTransReq->pAllocSkb = NULL;
       pSkb   = areq->base.data;
       offset = areq->data_offset;
    }
    pTransReq->giv    = req->giv;
    pTransReq->pNBuf  = SKBUFF_2_PNBUFF(pSkb);
    pTransReq->ivsize = crypto_aead_ivsize(aead);

#if defined(CONFIG_BLOG)
    spu_blog_emit(pTransReq);
#endif

    /* flush all data */
    cache_flush_len(pSkb->data, pSkb->len);
    ret = spu_runner_process_ipsec(pTransReq, SKBUFF_2_PNBUFF(pSkb), offset);
    if ( 0 == ret )
    {
        ret = -EINPROGRESS;
        pCtx->update = 0;
    }
    else
    {
        spu_free_trans_req(pTransReq);
        SPU_TRACE(("aead_giv_encrypt: spu_runner_process_ipsec returned error %d\n", ret));
        ret = -EBUSY;
        spuinfo->stats.encDrops++;
    }
    return ret;
} /* spu_aead_givencrypt */

static int spu_aead_decrypt (struct aead_request *areq)
{
    struct crypto_aead       *aead;
    struct spu_trans_req     *pTransReq;
    int                       ret;
    struct spu_ctx           *pCtx;
    int                       srcLen = 0;
    struct sk_buff           *pSkb = NULL;
    unsigned int              offset;

    spu_dump_data(areq, areq->iv, "spu_aead_decrypt - org", 1);

    spuinfo->stats.decIngress++;
    if (spu_aead_need_fallback_skb(areq, &srcLen))
    {
        pSkb = spu_skb_alloc_and_copy(areq, areq->iv, srcLen);
        if ( IS_ERR(pSkb) )
        {
           spuinfo->stats.decDrops++;
           return PTR_ERR(pSkb);
        }
        spuinfo->stats.decFallback++;
    }

    pTransReq = spu_alloc_trans_req(areq->base.flags);
    if (IS_ERR(pTransReq))
    {
        spuinfo->stats.decDrops++;
        return PTR_ERR(pTransReq);
    }

    aead = crypto_aead_reqtfm(areq);
    pCtx = crypto_aead_ctx(aead);
    if ( pCtx->direction == SPU_DIRECTION_INVALID )
    {
       pCtx->direction = SPU_DIRECTION_DS;
       pCtx->next_hdr  = 0;
       spu_runner_descr_config(pCtx);
    }
    pTransReq->context  = areq;
    pTransReq->callback = spu_esp_crypt_done;
    pTransReq->err      = 0;
    pTransReq->pSpuCtx  = pCtx;
    if ( pSkb != NULL )
    {
       pTransReq->pAllocSkb = pSkb;
       offset = 0;
    }
    else
    {
       pSkb = areq->base.data;
       pTransReq->pAllocSkb = NULL;
       offset = areq->data_offset;
    }
    pTransReq->giv    = NULL;
    pTransReq->pNBuf  = SKBUFF_2_PNBUFF(pSkb);
    pTransReq->ivsize = crypto_aead_ivsize(aead);

#if defined(CONFIG_BLOG)
    spu_blog_emit(pTransReq);
#endif

    if ( pTransReq->pAllocSkb )
    {
       cache_flush_len(pSkb->data, pSkb->len);
    }
    /* else no data written so cache flush is not required
       cache is invalidated in return path */
    ret = spu_runner_process_ipsec(pTransReq, SKBUFF_2_PNBUFF(pSkb), offset);
    if ( 0 == ret )
    {
        ret = -EINPROGRESS;
        pCtx->update = 0;
    }
    else
    {
        SPU_TRACE(("spu_aead_decrypt spu_runner_process_ipsec returned error %d\n", ret));
        spu_free_trans_req(pTransReq);
        ret = -EBUSY;
        spuinfo->stats.decDrops++;
    }        

    return ret;
} /* spu_aead_decrypt */

static void spu_cra_exit (struct crypto_tfm *tfm)
{
    struct spu_ctx *ctx = crypto_tfm_ctx (tfm);
    spu_runner_descr_put(ctx->pDescr);
#if defined(CONFIG_BLOG)
    spu_blog_ctx_del(ctx);
#endif
} /* spu_cra_exit */

static int spu_cra_init (struct crypto_tfm *tfm)
{
    struct crypto_alg *alg = tfm->__crt_alg;
    struct spu_crypto_alg *spuAlg;
    struct spu_ctx *ctx = crypto_tfm_ctx (tfm);

    spuAlg = container_of(alg, struct spu_crypto_alg, cryptoAlg);
    ctx->direction = SPU_DIRECTION_INVALID;
    ctx->update    = 1;
    ctx->descAlg   = spuAlg->descAlg;
    ctx->pDescr    = spu_runner_descr_get();
    if ( ctx->pDescr == NULL )
    {
        return -ENOBUFS;
    }
#if defined(CONFIG_BLOG)
    spu_blog_ctx_add(ctx);
#endif
    return 0;
} /* spu_cra_init */

static struct spu_crypto_alg *spu_alg_alloc( struct spu_alg_template *algTemplate )
{
    struct spu_crypto_alg *spuAlg;
    struct crypto_alg     *alg;

    spuAlg = kzalloc(sizeof(struct spu_crypto_alg), GFP_KERNEL);
    if (NULL == spuAlg)
    {
        return ERR_PTR(-ENOMEM);
    }

    INIT_LIST_HEAD(&spuAlg->entry);
    spuAlg->descAlg = algTemplate->descAlg;
    alg = &spuAlg->cryptoAlg;
    memcpy(alg, &algTemplate->alg, sizeof(struct crypto_alg));
    alg->cra_module = THIS_MODULE;
    alg->cra_init = spu_cra_init;
    alg->cra_exit = spu_cra_exit;
    alg->cra_priority = SPU_CRA_PRIORITY;
    alg->cra_alignmask = 0;
    alg->cra_ctxsize = sizeof (struct spu_ctx);
    alg->cra_flags = CRYPTO_ALG_TYPE_AEAD | CRYPTO_ALG_ASYNC;
    alg->cra_type = &crypto_aead_type;

    return spuAlg;
} /* spu_alg_alloc */

static struct spu_alg_template spu_algs[] = {
  {
   .alg = {
       .cra_name = "authenc(hmac(sha1),cbc(aes))",
       .cra_driver_name = "authenc-hmac-sha1-cbc-aes-spu",
       .cra_blocksize = AES_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = AES_BLOCK_SIZE,
            .maxauthsize = SHA1_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_AES |
              BCM_DESC_AUTH_ALG_SHA1,
  },
  {
   .alg = {
       .cra_name = "authenc(hmac(sha1),cbc(des3_ede))",
       .cra_driver_name = "authenc-hmac-sha1-cbc-3des-spu",
       .cra_blocksize = DES3_EDE_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = DES3_EDE_BLOCK_SIZE,
            .maxauthsize = SHA1_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_3DES | 
              BCM_DESC_AUTH_ALG_SHA1,
  },
  {
   .alg = {
       .cra_name = "authenc(hmac(sha1),cbc(des))",
       .cra_driver_name = "authenc-hmac-sha1-cbc-des-spu",
       .cra_blocksize = DES_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = DES_BLOCK_SIZE,
            .maxauthsize = SHA1_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_DES | 
              BCM_DESC_AUTH_ALG_SHA1,
  },
  {
   .alg = {
       .cra_name = "authenc(hmac(md5),cbc(aes))",
       .cra_driver_name = "authenc-hmac-md5-cbc-aes-spu",
       .cra_blocksize = AES_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = AES_BLOCK_SIZE,
            .maxauthsize = MD5_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_AES | 
              BCM_DESC_AUTH_ALG_MD5,
  },
  {
   .alg = {
       .cra_name = "authenc(hmac(md5),cbc(des3_ede))",
       .cra_driver_name = "authenc-hmac-md5-cbc-3des-spu",
       .cra_blocksize = DES3_EDE_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = DES3_EDE_BLOCK_SIZE,
            .maxauthsize = MD5_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_3DES | 
              BCM_DESC_AUTH_ALG_MD5,
  },
  {
   .alg = {
       .cra_name = "authenc(hmac(md5),cbc(des))",
       .cra_driver_name = "authenc-hmac-md5-cbc-des-spu",
       .cra_blocksize = DES_BLOCK_SIZE,
       .cra_aead = {
            .setkey = spu_aead_setkey,
            .setauthsize = spu_aead_setauthsize,
            .decrypt = spu_aead_decrypt,
            .givencrypt = spu_aead_givencrypt,
            .geniv = "<built-in>",
            .ivsize = DES_BLOCK_SIZE,
            .maxauthsize = MD5_DIGEST_SIZE,
            }
       },
   .descAlg = BCM_DESC_ENCR_ALG_DES | 
              BCM_DESC_AUTH_ALG_MD5,
  },
};


static void spu_unregister(unsigned long arg)
{
    struct spu_crypto_alg *spuAlg;
    struct spu_crypto_alg *temp;
    SPUDDDRV_INITIALIZE    KArg;
    int                    err;
    int                    loopCnt;
    int                    reqCnt;
    int                    incomplete;

    do
    {
        KArg.bvStatus = SPUSTS_SUCCESS;
        if ( NULL == spuinfo->transReqBase )
        {
            /* already unregistered - return success */
            break;
        }

        /* release algorithms that are not referenced */
        list_for_each_entry_safe(spuAlg, temp, &spuinfo->algList, entry)
        {
            if ( atomic_read(&spuAlg->cryptoAlg.cra_refcnt) != 1 )
            {
               printk("reference to alg %s still held - spu unregister incomplete\n", spuAlg->cryptoAlg.cra_name);
               incomplete = 1;
            }
            else
            {
               err = crypto_unregister_alg(&spuAlg->cryptoAlg);
               if ( err )
               {
                   printk("unregister alg failed %s, err %d\n", spuAlg->cryptoAlg.cra_name, err);
               }
               list_del(&spuAlg->entry);
            }
        }
        /* some algorithms are still referenced - unable to shutdown so return error */
        if ( 1 == incomplete )
        {
            KArg.bvStatus = SPUSTS_ERROR;
            break;
        }

        /* wait for all pending requests to complete */
        loopCnt = 0;
        while ( loopCnt < 10 )
        {
             struct spu_trans_req *req;

             reqCnt = 0;
             spin_lock_bh(&spuinfo->spuListLock);
             list_for_each_entry(req, &spuinfo->transReqList, entry) 
             {
                 reqCnt++;
             }
             spin_unlock_bh(&spuinfo->spuListLock);
             if ( reqCnt == SPU_NUM_TRANS_REQ )
             {
                 break;
             }
             loopCnt++;
             msleep(100);
        }
        if (10 == loopCnt)
        {
           SPU_TRACE(("spu_unregister: %d requests still pending\n", (SPU_NUM_TRANS_REQ - reqCnt)));
        }

#if defined(CONFIG_BLOG)
        spu_blog_unregister();
#endif
        spu_runner_unregister();

        if (spuinfo->transReqBase) kfree(spuinfo->transReqBase);
        spuinfo->transReqBase = NULL;
    } while (0);

    if ( arg != 0 )
    {
        put_user(KArg.bvStatus, &((SPUDDDRV_INITIALIZE *)arg)->bvStatus);
    }
    return;
} /* spu_unregister */

static void spu_register (unsigned long arg)
{
    struct spu_crypto_alg   *spuAlg;
    struct spu_alg_template *algTemplate;
    int                      i;
    int                      err;
    struct spu_trans_req    *pTransReq;
    int                      size;
    SPUDDDRV_INITIALIZE      KArg;

    KArg.bvStatus = SPUSTS_SUCCESS;
    do
    {
        if ( spuinfo->transReqBase != NULL )
        {
            /* already registered - return success */
            break;
        }

        memset(&spuinfo->stats, 0, sizeof(SPU_STAT_PARMS));
        INIT_LIST_HEAD(&spuinfo->algList);
        INIT_LIST_HEAD(&spuinfo->transReqList);

        /* allocate transfer requests */
        size = (SPU_NUM_TRANS_REQ * sizeof(struct spu_trans_req));
        spuinfo->transReqBase = kzalloc(size, GFP_KERNEL);
        if (NULL == spuinfo->transReqBase)
        {
            printk("spu_register: Insufficient memory for spu transfer requests\n");
            KArg.bvStatus = SPUSTS_MEMERR;
            break;
        }

        pTransReq = (struct spu_trans_req *)spuinfo->transReqBase;
        for (i = 0; i < SPU_NUM_TRANS_REQ; i++)
        {
            list_add_tail(&pTransReq->entry, &spuinfo->transReqList);
            pTransReq->index = i + SPU_TRANS_REQ_BASE_IDX;
            pTransReq++;
        }

        err = spu_runner_register();
        if ( err != 0 )
        {
            printk("spu_runner_register failed with error %d\n", err);
            KArg.bvStatus = err;
            break;
        }

#if defined(CONFIG_BLOG)
        err = spu_blog_register();
        if ( err != 0 )
        {
            printk("spu_runner_blog failed with error %d\n", err);
            KArg.bvStatus = err;
            break;
        }
#endif
        /* register crypto algorithms */
        for (i = 0; i < ARRAY_SIZE(spu_algs); i++)
        {
            algTemplate = &spu_algs[i];
            spuAlg = spu_alg_alloc(algTemplate);
            if ( IS_ERR(spuAlg) )
            {
               printk("unable to allocate algorithm\n");
               KArg.bvStatus = SPUSTS_ERROR;
               continue;
            }

            err = crypto_register_alg (&spuAlg->cryptoAlg);
            if (err)
            {
                printk("spu_register: alg %s registration failed, err %d\n", spuAlg->cryptoAlg.cra_driver_name, err);
            }
            else
            {
                list_add_tail(&spuAlg->entry, &spuinfo->algList);
                SPU_TRACE(("spu_register: alg %s registered\n", spuAlg->cryptoAlg.cra_driver_name));
            }
        }
     } while ( 0 );

    if ( SPUSTS_SUCCESS != KArg.bvStatus )
    {
        spu_unregister(0);
    }

    put_user (KArg.bvStatus, &((SPUDDDRV_INITIALIZE *)arg)->bvStatus);

    return;
} /* spu_register */

/***************************************************************************
 * Function Name: do_spu_test
 * Description  : 
 *                
 * Returns      : N/A
 ***************************************************************************/
static void spu_test(unsigned long arg)
{
    SPU_TRACE (("SPU: do_spu_test\n"));

    return;
} /* do_spu_test */

/***************************************************************************
 * Function Name: do_spu_show
 * Description  : show statistics
 *                
 * Returns      : N/A
 ***************************************************************************/
static void spu_show (unsigned long arg)
{
    SPUDDDRV_SPU_SHOW  KArg;

    SPU_TRACE(("IPSEC SPU: do_spu_show\n"));

    if(!spuinfo)
    {
        return;
    }

    KArg.bvStatus = SPUSTS_SUCCESS;
    memcpy(&KArg.stats, &spuinfo->stats, sizeof(SPU_STAT_PARMS));
    if ( 0 != copy_to_user((void *)arg, &KArg, sizeof(SPUDDDRV_SPU_SHOW)) )
    {
        printk("spu_show - error copying data to user\n");
    }
    return;
} /* do_spu_show */

/***************************************************************************
 * Function Name: spu_ioctl
 * Description  : Entry point for application commands.
 * Returns      : 0 - success.
 ***************************************************************************/
static long spu_ioctl_unlocked(struct file *filep, unsigned int cmd, unsigned long arg)
{
    long                rt = 0;
    unsigned int        cmdnr = _IOC_NR(cmd);
    IPSec_SPU_FN_IOCTL  IoctlFuncs[] = { spu_register,
                                         spu_unregister,
                                         spu_show,
                                         spu_test };

    mutex_lock(&spuinfo->spuIoctlMutex);
    if (cmdnr < MAX_SPUDDDRV_IOCTL_COMMANDS)
    {
        SPU_TRACE (("IPSEC SPU: Ioctl cmd %u\n", cmdnr));
        (*IoctlFuncs[cmdnr])(arg);
    }
    else
    {
        rt = -EINVAL;
    }
    mutex_unlock(&spuinfo->spuIoctlMutex);

    return rt;
}

/***************************************************************************
 * Function Name: spu_release
 * Description  : Called when an application closes this device.
 * Returns      : 0 - success.
 ***************************************************************************/
static int spu_release (struct inode *inode, struct file *filp)
{
  return (0);
}


/***************************************************************************
 * Function Name: spu_open
 * Description  : Called when an application opens this device.
 * Returns      : 0 - success
 ***************************************************************************/
static int spu_open (struct inode *inode, struct file *filp)
{
    return (0);
}

__exit void bcm_spu_deinit(void)
{
    spu_runner_deinit( );

    kfree(spuinfo);
    spuinfo = NULL;

    unregister_chrdev(IPSECSPUDRV_MAJOR, "spu");
    return;
}

__init int bcm_spu_init (void)
{
    int err;

    spuinfo = kzalloc(sizeof(struct spu_info), GFP_KERNEL);
    if (NULL == spuinfo) {
        printk("bcm_spu_init: Insufficient memory for context data\n");
        return -1;
    }

    spuinfo->spu_file_ops.unlocked_ioctl = spu_ioctl_unlocked;
    spuinfo->spu_file_ops.open = spu_open;
    spuinfo->spu_file_ops.release = spu_release;
    if (register_chrdev(IPSECSPUDRV_MAJOR, "spu", &spuinfo->spu_file_ops))
    {
        printk("SPU: failed to register char device\n");
        bcm_spu_deinit();
        return -1;
    }

    spin_lock_init(&spuinfo->spuListLock);
    mutex_init(&spuinfo->spuIoctlMutex);

    err = spu_runner_init( );
    if ( err != 0 )
    {
        bcm_spu_deinit();
        return -1;
    }
    return 0;
}

module_init(bcm_spu_init);
module_exit(bcm_spu_deinit);
MODULE_LICENSE("GPL");
