/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Auxiliary net namespaces API
 *
 * Implementation of auxilliary functions to work with network namespaces.
 *
 * @author Andrey Dmitrov <Andrey.Dmitrov@oktetlabs.ru>
 */

#define TE_LGR_USER     "Libts Netns"

#include "lib-ts.h"
#include "lib-ts_netns.h"
#include "tapi_cfg.h"
#include "tapi_namespaces.h"
#include "tapi_host_ns.h"

#define NETNS_GETENV(_var, _env)  \
    do {                                                                \
        _var = getenv(_env);                                            \
        if (_var == NULL)                                               \
        {                                                               \
            ERROR("Environment variable %s is not specified", _env);    \
            return TE_RC(TE_TAPI, TE_ENOENT);                           \
        }                                                               \
    } while(0)

te_errno
libts_netns_get_sfc_ta(char **ta)
{
    te_errno rc;
    char     *if_name;
    char     *agent;
    int32_t  status;

    agent = getenv("TE_IUT_TA_NAME");
    if (agent == NULL)
    {
        ERROR("Cannot get IUT agent name");
        return TE_RC(TE_TAPI, te_rc_os2te(errno));
    }

    if_name = getenv("TE_ORIG_IUT_TST1");
    if (if_name == NULL)
    {
        ERROR("Cannot get IUT interface name");
        return TE_RC(TE_TAPI, te_rc_os2te(errno));
    }

    rc = cfg_get_int32(&status, "/agent:%s/interface:%s/status:",
                       agent, if_name);
    if (rc == TE_RC(TE_CS, TE_ENOENT))
    {
        agent = getenv("TE_IUT_TA_NAME_NS");
        if (agent == NULL)
        {
            ERROR("Cannot get namespaced IUT agent name");
            return TE_RC(TE_TAPI, te_rc_os2te(errno));
        }
    }
    else if (rc != 0)
    {
        ERROR("Failed to get /agent:%s/interface:%s/status:", agent, if_name);
        return rc;
    }

    *ta = strdup(agent);
    if (*ta == NULL)
        return TE_RC(TE_TAPI, TE_ENOMEM);

    return 0;
}

/**
 * Move testing interfaces to the namespace @p ns_name.
 *
 * @param ta        Test agent
 * @param ns_name   The namespace name
 * @param ns_ta     Agent name in the net namespace
 * @param orig      Move original (the lowest) interfaces to the namespace
 *                  if @c TRUE
 *
 * @return Status code
 */
static te_errno
move_interfaces_to_ns(const char *ta, const char *ns_name, const char *ns_ta,
                      te_bool orig)
{
    const char *iut_ifs[] = { "TE_IUT_TST1",
                              "TE_IUT_TST1_IUT",
                              "TE_IUT_TST1_IUT2",
                              "TE_IUT_TST1_IUT3",
                              "TE_IUT_TST2_TST"
                            };
    const char *iut_ifs_orig[] = { "TE_ORIG_IUT_TST1",
                                   "TE_ORIG_IUT_TST1_IUT",
                                   "TE_ORIG_IUT_TST1_IUT2",
                                   "TE_ORIG_IUT_TST1_IUT3",
                                   "TE_ORIG_IUT_TST2_TST"
                                 };
    char       *ifname;
    te_errno    rc;
    size_t      i;

    for (i = 0; i < TE_ARRAY_LEN(iut_ifs); i++)
    {
        if (orig)
            ifname = getenv(iut_ifs_orig[i]);
        else
            ifname = getenv(iut_ifs[i]);
        if (ifname != NULL && strlen(ifname) > 0)
        {
            rc = tapi_host_ns_if_change_ns(ta, ifname, ns_name, ns_ta);
            if (rc != 0)
                return rc;
        }
    }

    return 0;
}

/**
 * Add route for the local network in the secondary namespace.
 * It can be required if a test modifies default route on IUT agent.
 *
 * @param ta        Name of the agent in the main namespace.
 * @param ta_iut    Name of the agent in the secondary namespace.
 * @param veth1     Virtual interface in the main namespace.
 * @param veth2     Virtual interface in the secondary namespace.
 *
 * @return Status code
 */
static te_errno
add_local_network_route(const char *ta, const char *ta_iut, const char *veth1,
                        const char *veth2)
{
    struct sockaddr_storage addr;
    struct sockaddr_storage gw_addr;

    char           *local_net_env;
    char            local_net[RCF_MAX_ID];
    char           *ptr;
    int             prefix;
    te_errno        rc;
    cfg_handle     *addresses;
    char           *inst_name;
    unsigned int    addr_num;
    unsigned int    i;

    local_net_env = getenv("SOCKAPI_TS_LOCAL_NETWORK");
    if (local_net_env == NULL)
        return 0;

    strncpy(local_net, local_net_env, sizeof(local_net) - 1);

    ptr = strchr(local_net, '/');
    if (ptr == NULL)
    {
        ERROR("Unexpected local network address format: %s", local_net);
        return TE_RC(TE_TAPI, TE_EINVAL);
    }
    *ptr = '\0';
    ptr++;
    prefix = atoi(ptr);

    rc = te_sockaddr_str2h(local_net, SA(&addr));
    if (rc != 0)
        return rc;

    rc = cfg_find_pattern_fmt(&addr_num, &addresses,
                              "/agent:%s/interface:%s/net_addr:*", ta, veth1);
    if (rc != 0)
        return rc;

    for (i = 0; i < addr_num; i++)
    {
        rc = cfg_get_inst_name(addresses[i], &inst_name);
        if (rc != 0)
        {
            free(addresses);
            return rc;
        }

        /* Skip IPv6 */
        if (strchr(inst_name, ':') == NULL)
            break;
        free(inst_name);
    }
    free(addresses);
    if (i == addr_num)
    {
        ERROR("Failed to get IP address of %s", veth1);
        return TE_RC(TE_TAPI, TE_EFAULT);
    }

    rc = te_sockaddr_str2h(inst_name, SA(&gw_addr));
    free(inst_name);
    if (rc != 0)
        return rc;

    rc = tapi_cfg_add_route(ta_iut, AF_INET,
                            te_sockaddr_get_netaddr(SA(&addr)), prefix,
                            te_sockaddr_get_netaddr(SA(&gw_addr)),
                            veth2, NULL, 0, 0, 0, 0, 0, 0, NULL);

    return rc;
}

/**
 * Get control interface name using default route or environment
 * @b SOCKAPI_TS_NETNS_CTL_IF if it is specified.
 *
 * @param ta            Test agent name
 * @param ctl_if        The control interface name
 * @param ctl_if_len    Length of the buffer @p ctl_if
 *
 * @return Status code
 */
static te_errno
get_ctl_if(const char *ta, char *ctl_if, size_t ctl_if_len)
{
    te_errno rc;
    size_t   len;
    char    *ifname;
    te_bool  release = FALSE;

    ifname = getenv("SOCKAPI_TS_NETNS_CTL_IF");
    if (ifname == NULL)
    {
        rc = cfg_get_instance_fmt(NULL, &ifname,
                                  "/agent:%s/ip4_rt_default_if:", ta);
        if (rc != 0)
            return rc;
        release = TRUE;
    }

    len = strlen(ifname) + 1;
    if (len > ctl_if_len)
    {
        ERROR("Too long interface name: %s", ifname);
        rc = TE_RC(TE_TA, TE_ESMALLBUF);
    }
    else
        memcpy(ctl_if, ifname, len);

    if (release)
        free(ifname);

    return rc;
}

te_errno
libts_setup_namespace(libts_netns_conn_mode mode)
{
    const char *set_netns = getenv("SOCKAPI_TS_NETNS");
    const char *ta;
    const char *ta_type;
    const char *host;
    const char *ta_iut;
    const char *ns_name;
    const char *cfg;
    const char *cfg_ifs;
    const char *veth1;
    const char *veth2;
    const char *macvlan;
    const char *rcfport_str;
    const char *ld_preload;
    const char *ta_rpcprovider;
    int         rcfport;
    char        addr[RCF_MAX_NAME] = {};
    char        ctl_if[IFNAMSIZ];

    te_errno rc;

    if (set_netns == NULL || strcmp(set_netns, "true") != 0)
        return 0;

    NETNS_GETENV(ta, "TE_IUT_TA_NAME");
    NETNS_GETENV(ta_rpcprovider, "SF_TS_IUT_RPCPROVIDER");
    NETNS_GETENV(ta_type, "TE_IUT_TA_TYPE");
    NETNS_GETENV(host, "TE_IUT");
    NETNS_GETENV(ta_iut, "TE_IUT_TA_NAME_NS");
    NETNS_GETENV(ns_name, "SOCKAPI_TS_NETNS_NAME");
    NETNS_GETENV(cfg, "SOCKAPI_TS_CFG_DUT");
    if (mode == LIBTS_NETNS_CONN_MACVLAN)
    {
        NETNS_GETENV(macvlan, "SOCKAPI_TS_NETNS_MACVLAN");
    }
    else
    {
        NETNS_GETENV(veth1, "SOCKAPI_TS_NETNS_VETH1");
        NETNS_GETENV(veth2, "SOCKAPI_TS_NETNS_VETH2");
    }
    NETNS_GETENV(rcfport_str, "SOCKAPI_TS_NETNS_PORT");
    rcfport = atoi(rcfport_str);
    cfg_ifs = getenv("SOCKAPI_TS_CFG_IFS");

    rc = get_ctl_if(ta, ctl_if, sizeof(ctl_if));
    if (rc != 0)
        return rc;

    if (mode == LIBTS_NETNS_CONN_MACVLAN)
        rc = tapi_netns_create_ns_with_macvlan(ta, ns_name, ctl_if, macvlan,
                                               addr, sizeof(addr));
    else
        rc = tapi_netns_create_ns_with_net_channel(ta, ns_name, veth1, veth2,
                                                   ctl_if, rcfport);
    if (rc != 0)
        return rc;

    ld_preload = getenv("TE_IUT_LD_PRELOAD");
    rc = tapi_netns_add_ta(host, ns_name, ta_iut, ta_type, rcfport, addr,
                           ld_preload, FALSE);
    if (rc != 0)
        return rc;

    /* Synchronize configurator DB after new test agent added */
    CHECK_RC(rc = cfg_synchronize("/:", TRUE));

    CHECK_RC(cfg_set_instance_fmt(CVT_STRING, ta_rpcprovider,
                                  "/agent:%s/rpcprovider:", ta_iut));

    rc = libts_fix_ta_path_env(ta_iut);
    if (rc != 0)
        return rc;

    rc = move_interfaces_to_ns(ta, ns_name, ta_iut, cfg_ifs != NULL);
    if (rc != 0)
        return rc;

    if (mode == LIBTS_NETNS_CONN_VETH)
    {
        rc = add_local_network_route(ta, ta_iut, veth1, veth2);
        if (rc != 0)
            return rc;
    }

    if (cfg_ifs != NULL)
    {
        rc = cfg_process_history(cfg_ifs, NULL);
        if (rc != 0)
            return rc;
    }

    return cfg_process_history(cfg, NULL);
}

te_errno
libts_cleanup_netns(void)
{
    const char *set_netns = getenv("SOCKAPI_TS_NETNS");
    const char *ta_iut;
    const char *macvlan;
    te_errno    rc = 0;

    if (set_netns == NULL || strcmp(set_netns, "true") != 0)
        return 0;

    ta_iut = getenv("TE_IUT_TA_NAME_NS");
    /* Remove test agent added in prologue and run in auxiliary network
     * namespace. */
    if (ta_iut != NULL)
    {
        rc = rcf_del_ta(ta_iut);
        if (rc == 0)
            rc = tapi_host_ns_agent_del(ta_iut);
    }
    else
    {
        ERROR("Failed to get ns agent name from env TE_IUT_TA_NAME_NS");
        rc = TE_RC(TE_TA, TE_ENOENT);
    }

    macvlan = getenv("SOCKAPI_TS_NETNS_MACVLAN");
    if (macvlan != NULL)
    {
        const char *ta;
        const char *ns;
        char        ctl_if[IFNAMSIZ];
        te_errno    rc2;

        NETNS_GETENV(ta, "TE_IUT_TA_NAME");
        NETNS_GETENV(ns, "SOCKAPI_TS_NETNS_NAME");
        rc2 = get_ctl_if(ta, ctl_if, sizeof(ctl_if));
        if (rc2 != 0)
            return rc2;

        rc2 = tapi_netns_destroy_ns_with_macvlan(ta, ns, ctl_if, macvlan);
        if (rc == 0)
            rc = rc2;
    }

    if (rc != 0)
        return rc;

    rc = cfg_synchronize_fmt(TRUE, "/agent:%s", ta_iut);
    return rc;
}
