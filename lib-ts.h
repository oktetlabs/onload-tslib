/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Test Suites common library
 *
 * Common includes and definitions.
 *
 * @author Denis Pryazhennikov <Denis.Pryazhennikov@oktetlabs.ru>
 */

#ifndef __ONLOAD_LIB_TS_H__
#define __ONLOAD_LIB_TS_H__

#include "te_config.h"

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef STDC_HEADERS
#include <stdlib.h>
#include <stdarg.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "te_defs.h"
#include "te_errno.h"
#include "te_string.h"
#include "logger_api.h"
#include "tapi_test.h"
#include "tapi_sockaddr.h"
#include "tapi_cfg.h"
#include "tapi_sh_env.h"
#include "tapi_serial.h"
#include "tapi_file.h"

/**
 * Update PATH variable for Test Agent.
 *
 * @note    Some OSes can place system commands in sbin or usr/sbin directories,
 *          so we should add these paths to the PATH environment variable.
 *
 * @param   ta_name   Test Agent's name.
 *
 * @return Status code.
 */
extern te_errno libts_fix_ta_path_env(const char *ta_name);

/**
 * Make sure PATH includes system commands locations on all TAs.
 */
extern void libts_fix_tas_path_env(void);

/**
 * Set host to use it for implicit ZF binds.
 *
 * @note It should be called in the main prologue after
 *       @ref tapi_network_setup() but before @ref tapi_cfg_env_local_to_agent()
 */
extern void libts_set_zf_host_addr(void);

/**
 * Set console loglevel in accordance with ST_CONSOLE_LOGLEVEL
 * which originates from --script=ool.console_loglevel:<N>
 */
extern void libts_init_console_loglevel(void);

/**
 * Set to RW mode conserver, send enter and return back to RO mode.
 *
 * @param ta              Test Agent name
 * @param rpc_server_name RPC server name
 * @param console_name    Console name
 */
extern void libts_send_enter2serial_console(const char *ta,
                                            const char *rpc_server_name,
                                            const char *console_name);

/**
 * Copy Socket API libraries specified in /local/socklib instances
 * to corresponding test agent.
 *
 * @return Status code.
 */
extern te_errno libts_copy_socklibs(void);

/**
 * Copy file from engine to agent or copy it on agent accorting to
 * SFC_ONLOAD_LOCAL environment variable.
 *
 * @param ta            Test Agent name
 * @param src           Source file name with optional 'iut:' prefix which
 *                      indicates that file should be taken from TA
 * @param dst           Destination file name
 * @param non_exist_f   Whether to fail or not in case of non existance of
 *                      src file
 */
extern int libts_file_copy_ta(const char *ta, const char *src,
                              const char *dst, te_bool non_exist_f);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__ONLOAD_LIB_TS_H__ */
