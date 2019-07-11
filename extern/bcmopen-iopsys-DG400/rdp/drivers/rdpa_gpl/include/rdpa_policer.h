/*
* <:copyright-BRCM:2013:DUAL/GPL:standard
* 
*    Copyright (c) 2013 Broadcom 
*    All Rights Reserved
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License, version 2, as published by
* the Free Software Foundation (the "GPL").
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* 
* 
* A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
* writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
* Boston, MA 02111-1307, USA.
* 
* :> 
*/


#ifndef _RDPA_POLICER_H_
#define _RDPA_POLICER_H_

/**
 * \defgroup policer Traffic Policer
 * API in this group controls Runner Traffic Policing capabilities. Traffic policing allows you to control the maximum
 * rate of traffic.\n
 * Policer can be assigned to any type of  \ref ingress_class_d "classification flow". 
 * \ingroup tm
 * @{
 */

#ifndef XRDP
#define RDPA_TM_MAX_US_POLICER 16 /**< Max number of US policers */
#define RDPA_TM_MAX_DS_POLICER 16 /**< Max number of DS policers */
#define RDPA_TM_MAX_POLICER    (RDPA_TM_MAX_US_POLICER+RDPA_TM_MAX_DS_POLICER)
#else
#define RDPA_TM_MAX_POLICER    80 /**< Max number of policers */
#endif

/** Traffic policer type */
typedef enum {
    rdpa_tm_policer_token_bucket,                                       /**< Single token bucket */
    rdpa_tm_policer_single_token_bucket = rdpa_tm_policer_token_bucket, /**< Single token bucket */
    rdpa_tm_policer_sr_overflow_dual_token_bucket,    /**< Dual token bucket single rate with overflow */
    rdpa_tm_policer_tr_dual_token_bucket,             /**< Dual token bucket two rate without overflow */  
    rdpa_tm_policer_tr_overflow_dual_token_bucket,    /**< Dual token bucket two rate with overflow */  
    rdpa_tm_policer_type__num_of,   /* Number of possible types */
} rdpa_tm_policer_type;

/** Policer factor to be added for every packet */
typedef enum {
    rdpa_policer_factor_bytes_0,       /**< don't add bytes to policing */
    rdpa_policer_factor_bytes_4,       /**< add 4B to policing */
    rdpa_policer_factor_bytes_8,       /**< add 8B to policing */
    rdpa_policer_factor_bytes_neg_4,   /**< subtract 4B to policing */
    rdpa_policer_factor_bytes_neg_8,   /**< subtract 8B to policing */
    rdpa_policer_factor_num_of,   /* Number of possible types */
} rdpa_policer_factor_bytes;


#if defined(XRDP)
#define policer_rate_size_t   uint64_t
#else
#define policer_rate_size_t   uint32_t
#endif

/** Policer configuration.
 * Underlying type for tm_policer_cfg aggregate type
 */
typedef struct {
    rdpa_tm_policer_type  type;             /**< Policer type */
    policer_rate_size_t commited_rate;      /**< Committed Information Rate (CIR) - bps */
    uint32_t committed_burst_size;          /**< Committed Burst Size (CBS) - bytes */
    policer_rate_size_t peak_rate;          /**< PEAK Information Rate (PIR) - bps */
    uint32_t peak_burst_size;               /**< PEAK Burst Size (PBS) - bytes */
    uint8_t dei_mode;                       /**< DEI remark  yes/no */
    rdpa_policer_factor_bytes factor_bytes; /**< Policer factor to be added for every packet */
} rdpa_tm_policer_cfg_t;

/** Policer statistics.
 * Underlying structure for tm_policer_stat aggregate type
 */
typedef struct {
    rdpa_stat_t green;          /**< green statistics */
    rdpa_stat_t yellow;         /**< yellow statistics */
    rdpa_stat_t red;            /**< Red statistics */
} rdpa_tm_policer_stat_t;


#define RDPA_POLICER_MIN_SR       64000   /**< Min sustain rate */
#ifndef XRDP
#define RDPA_POLICER_MAX_SR       1000000000  /**< Max sustain rate */
#else
/* Ariel: this number can't be hold in uint32 */
#define RDPA_POLICER_MAX_SR       10000000000 /**< Max sustain rate */
#endif

/* @} end of policer Doxygen group */

#endif /* _RDPA_POLICER_H_ */
