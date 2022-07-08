/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary timestamps API
 *
 * Auxiliary functions to work with timestamps.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#ifndef __ONLOAD_LIB_TS_TIMESTAMPS_H__
#define __ONLOAD_LIB_TS_TIMESTAMPS_H__

#include "te_errno.h"
#include "lib-ts.h"

/**
 * Copy sfptpd to IUT agent and prepare configuration to start it.
 */
extern void libts_timestamps_configure_sfptpd(void);

/**
 * Stop ntpd and start sfptpd on IUT TA (unless ST_RUN_TS_NO_SFPTPD
 * environment variable is set to @c 1).
 *
 * @param pco_iut         RPC server on IUT.
 */
extern void libts_timestamps_enable_sfptpd(rcf_rpc_server *pco_iut);

/**
 * Stop sfptpd on IUT if it was started previously by
 * libts_timestamps_enable_sfptpd().
 *
 * @param pco_iut   RPC server on IUT.
 */
extern void libts_timestamps_disable_sfptpd(rcf_rpc_server *pco_iut);

#endif /* !__ONLOAD_LIB_TS_TIMESTAMPS_H__ */
