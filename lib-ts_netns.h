/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary net namespaces API
 *
 * Auxilliary functions to work with network namespaces.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#ifndef __ONLOAD_LIB_TS_NETNS_H__
#define __ONLOAD_LIB_TS_NETNS_H__

#include "te_errno.h"

/**
 * Configuration options of TA-TEN communication channel.
 */
typedef enum {
    LIBTS_NETNS_CONN_VETH,      /**< IP-level switching using VETH intefaces
                                     couple. */
    LIBTS_NETNS_CONN_MACVLAN,   /**< MAC-level switching using MAC VLAN
                                     inteface. */
} libts_netns_conn_mode;

/**
 * Get name of the test agent which controls real SFC interfaces.
 *
 * @param ta    The test agent name (from the heap).
 *
 * @return Status code
 */
extern te_errno libts_netns_get_sfc_ta(char **ta);

/**
 * Setup network namespace and IUT ta.
 *
 * @param mode     Control communication channel mode.
 *
 * @return Status code
 */
extern te_errno libts_setup_namespace(libts_netns_conn_mode mode);

/**
 * Remove network namespace, auxiliary test agent and interfaces.
 *
 * @return Status code
 */
extern te_errno libts_cleanup_netns(void);

#endif /* !__ONLOAD_LIB_TS_NETNS_H__ */
