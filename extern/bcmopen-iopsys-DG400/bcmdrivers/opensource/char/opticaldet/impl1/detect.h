/*
<:copyright-BRCM:2015:DUAL/GPL:standard

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

#ifndef DETECT_H_INCLUDED
#define DETECT_H_INCLUDED

typedef enum {
    TRX_ACTIVE_LOW,
    TRX_ACTIVE_HIGH
} TRX_SIG_ACTIVE_POLARITY;

#define EPON2G (1 << 31)
#define RDPA_WAN_MASK 0xF

#define DEV_MAJOR 3020
#define DEV_CLASS "opticaldetect"

#define OPTICALDET_IOCTL_DETECT 1
#define OPTICALDET_IOCTL_SD 2

int opticaldetect(void);
int signalDetect(void);
int trx_get_lbe_polarity(TRX_SIG_ACTIVE_POLARITY *lbe_polarity_p);
int trx_get_tx_sd_polarity(TRX_SIG_ACTIVE_POLARITY *tx_sd_polarity_p);
int trx_get_vendor_name_part_num(char *vendor_name_p, int vendor_name_len,
                                 char *part_num_p, int part_num_len);

#endif /* DETECT_H_INCLUDED */
