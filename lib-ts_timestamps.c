/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary timestamps API
 *
 * Implementation of auxiliary functions to work with timestamps.
 *
 * @author Dmitry Izbitsky <Dmitry.Izbitsky@oktetlabs.ru>
 */

#define TE_LGR_USER     "Libts Timestamps"

#include "lib-ts.h"
#include "lib-ts_netns.h"
#include "lib-ts_timestamps.h"
#include "tapi_cfg.h"
#include "tapi_rpc_socket.h"
#include "tapi_rpc_unistd.h"
#include "tapi_host_ns.h"
#include "tapi_ntpd.h"
#include "tapi_sfptpd.h"
#include "tapi_rpc_client_server.h"
#include "tapi_rpcsock_macros.h"
#include "rcf_api.h"

/* See description in lib-ts_timestamps.h */
void
libts_timestamps_configure_sfptpd(void)
{
    const char  *daemon = getenv("SFC_ONLOAD_SFPTPD");
    const char  *cfg = getenv("SFC_ONLOAD_SFPTPD_CFG");
    const char  *ifname = getenv("TE_ORIG_IUT_TST1");
    char         dest[PATH_MAX] = {0,};
    char        *agt_dir = NULL;
    char        *ta;
    cfg_val_type val_type;
    int          rc;

    if (daemon == NULL || strcmp(daemon, "") == 0)
        return;

    if (cfg == NULL)
        TEST_FAIL("sftpd configuration file was not set, please set it "
                  "to env SFC_ONLOAD_SFPTPD_CFG");

    if (ifname == NULL)
        TEST_FAIL("environment value TE_ORIG_IUT_TST1 was not set");

    CHECK_RC(libts_netns_get_sfc_ta(&ta));

    val_type = CVT_STRING;
    CHECK_RC(cfg_get_instance_fmt(&val_type, &agt_dir, "/agent:%s/dir:", ta));

    if ((rc = snprintf(dest, sizeof(dest), "%s/sfptpd", agt_dir)) < 0 ||
        rc > (int)sizeof(dest))
        TEST_FAIL("Failed to create string with sftpd destination");
    CHECK_RC(rcf_ta_put_file(ta, 0, daemon, dest));
    RING("Successful file transmission %s -> %s:%s", daemon, ta, dest);

    CHECK_RC(cfg_set_instance_fmt(CVT_STRING, dest,
                                  "/agent:%s/sfptpd:/path:", ta));

    memset(dest, 0, sizeof(dest));
    if ((rc = snprintf(dest, sizeof(dest), "%s/sfptpd.cfg", agt_dir)) < 0 ||
        rc > (int)sizeof(dest))
        TEST_FAIL("Failed to create string with sftpd config destination");
    free(agt_dir);

    CHECK_RC(rcf_ta_put_file(ta, 0, cfg, dest));
    RING("Successful file transmission %s -> %s:%s", cfg, ta, dest);

    CHECK_RC(cfg_set_instance_fmt(CVT_STRING, dest,
                                  "/agent:%s/sfptpd:/config:", ta));

    CHECK_RC(cfg_set_instance_fmt(CVT_STRING, ifname,
                                  "/agent:%s/sfptpd:/ifname:", ta));
}

/* See description in lib-ts_timestamps.h */
void
libts_timestamps_enable_sfptpd(rcf_rpc_server *pco_iut)
{
    char *ta_sfc = NULL;

    /* Don't try to start SF PTP daemon if env ST_RUN_TS_NO_SFPTPD=1. */
    if (tapi_getenv_bool("ST_RUN_TS_NO_SFPTPD") == FALSE)
    {
        tapi_ntpd_disable(pco_iut);
        VSLEEP(1, "waiting for ntpd stop");

        CHECK_RC(libts_netns_get_sfc_ta(&ta_sfc));
        tapi_sfptpd_enable(ta_sfc);
    }
}

/* See description in lib-ts_timestamps.h */
void
libts_timestamps_disable_sfptpd(rcf_rpc_server *pco_iut)
{
    char *ta_sfc = NULL;

    if (tapi_getenv_bool("ST_RUN_TS_NO_SFPTPD") == FALSE)
    {
        CHECK_RC(libts_netns_get_sfc_ta(&ta_sfc));
        if (!tapi_sfptpd_status(ta_sfc))
            TEST_VERDICT("sfptpd is not running");

        tapi_sfptpd_disable(pco_iut->ta);
        VSLEEP(1, "waiting for sfptpd stop");

        tapi_ntpd_enable(pco_iut);
    }
}
