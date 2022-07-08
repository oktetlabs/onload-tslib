/* SPDX-License-Identifier: Apache-2.0 */
/* (c) Copyright 2019 - 2022 Xilinx, Inc. All rights reserved. */
/** @file
 * @brief Common lib for SF Tets Suites
 *
 * Implementation of common functions.
 *
 * @author Denis Pryazhennikov <Denis.Pryazhennikov@oktetlabs.ru>
 */

#define TE_LGR_USER     "Onload Library"

#include "lib-ts.h"

/* See description in lib-ts.h */
te_errno
libts_fix_ta_path_env(const char *ta_name)
{
    if (ta_name == NULL)
    {
        ERROR("%s(): wrong argument value", __FUNCTION__);
        return TE_EINVAL;
    }
    return tapi_sh_env_ta_path_append(ta_name, "/sbin:/usr/sbin");
}

/* See description in lib-ts.h */
void
libts_fix_tas_path_env(void)
{
    unsigned int    nb_ta_handles;
    cfg_handle     *ta_handles;
    char           *ta_name = NULL;
    unsigned int    i;

    CHECK_RC(cfg_find_pattern_fmt(&nb_ta_handles, &ta_handles, "/agent:*"));
    for (i = 0; i < nb_ta_handles; i++)
    {
        CHECK_RC(cfg_get_inst_name(ta_handles[i], &ta_name));
        CHECK_RC(libts_fix_ta_path_env(ta_name));
        free(ta_name);
    }
    free(ta_handles);
}

/* See description in lib-ts.h */
void
libts_set_zf_host_addr(void)
{
    te_string       zf_attr = TE_STRING_INIT_STATIC(RCF_MAX_PATH);
    cfg_handle     *addresses = NULL;
    char           *inst_name = NULL;
    const char     *if_name = NULL;
    const char     *ta = NULL;
    unsigned int    addr_num;
    cfg_handle      handle = CFG_HANDLE_INVALID;
    te_errno        rc;
    te_bool         ip4_found = FALSE;
    unsigned int    i;

    if ((ta = getenv("TE_IUT_TA_NAME_NS")) == NULL)
        return;

    if ((if_name = getenv("TE_IUT_TST1")) == NULL)
        return;

    rc = cfg_find_fmt(&handle, "/local:%s/env:ZF_ATTR", ta);
    if (TE_RC_GET_ERROR(rc) == TE_ENOENT)
        return;

    CHECK_RC(cfg_get_instance(handle, NULL, &inst_name));
    CHECK_RC(te_string_append(&zf_attr, "%s", inst_name));
    free(inst_name);

    CHECK_RC(cfg_find_pattern_fmt(&addr_num, &addresses,
                                  "/agent:%s/interface:%s/net_addr:*",
                                  ta, if_name));

    for (i = 0; i < addr_num; i++)
    {
        CHECK_RC(cfg_get_inst_name(addresses[i], &inst_name));
        /* Checking for version ip address. IPv4 is needed for ZF. */
        if (strchr(inst_name, ':') == NULL)
        {
            ip4_found = TRUE;
            break;
        }

        free(inst_name);
    }

    if (ip4_found == FALSE)
    {
        ERROR("Failed to find IPv4 address of %s", if_name);
        return;
    }

    CHECK_RC(te_string_append(&zf_attr, ";zfss_implicit_host=%s", inst_name));
    CHECK_RC(cfg_set_instance(handle, CVT_STRING, zf_attr.ptr));

    free(inst_name);
    free(addresses);
}

/* See description in lib-ts.h */
void
libts_init_console_loglevel(void)
{
    /* We reset IUT console loglevel to 7 by default.
     * See ST-1706: Test debug kernels with the Socket Tester
     * for explanation. */
#define DEFAULT_LOGLEVEL 7

    const char *level_str = getenv("ST_CONSOLE_LOGLEVEL");
    long level = DEFAULT_LOGLEVEL;

    /*
     * --script=ool.console_loglevel: without a number means
     *  "do not change the logvelel"
     * */
    if (level_str != NULL && level_str[0] == '\0')
        return;

    if (level_str != NULL && level_str[0] != '\0')
    {
        level = strtol(level_str, NULL, 0);
        if( level < 0 || level > 15 )
        {
            WARN("Unparseable value of ST_CONSOLE_LOGLEVEL: \"%s\", "
                 "using %d", level_str, DEFAULT_LOGLEVEL);
            level = DEFAULT_LOGLEVEL;
        }
    }

    tapi_cfg_set_loglevel_save(getenv("TE_IUT_TA_NAME"), level, NULL);
}

/* See description in lib-ts.h */
void
libts_send_enter2serial_console(const char *ta,
                                const char *rpc_server_name,
                                const char *console_name)
{
    rcf_rpc_server    *rpcs_serial;
    tapi_serial_handle p_handle = NULL;
    char *te_sc_write_disable = getenv("TE_SERIAL_CONSOLE_WRITE_DISABLE");

    if (te_sc_write_disable != NULL && te_sc_write_disable[0] != '\0')
    {
        RING("%s() is disabled by TE_SERIAL_CONSOLE_WRITE_DISABLE",
             __FUNCTION__);
        return;
    }

    /* It fails when Agt_D doesn't exist. */
    if (rcf_rpc_server_create(ta, rpc_server_name,
                              &rpcs_serial) != 0)
    {
        WARN("Failed to create RPC server on %s", ta);
        return;
    }
    RPC_AWAIT_IUT_ERROR(rpcs_serial);
    tapi_serial_open_rpcs(rpcs_serial, console_name,
                          &p_handle);
    if (p_handle != NULL && p_handle->sock > 0)
    {
        CHECK_RC(tapi_serial_force_rw(p_handle));
        CHECK_RC(tapi_serial_send_enter(p_handle));
        CHECK_RC(tapi_serial_spy(p_handle));
    }
    else
    {
        WARN("Failed to open console for %s", ta);
    }
    CHECK_RC(rcf_rpc_server_destroy(rpcs_serial));
}

/* See description in lib-ts.h */
int
libts_file_copy_ta(const char *ta, const char *src, const char *dst,
                   te_bool non_exist_f)
{
    int     rc;

    if (src != NULL && strncmp(src, "iut:", sizeof("iut:") - 1) == 0)
    {
        rc = tapi_file_copy_ta(ta, &src[sizeof("iut:") - 1], ta, dst);
        if (rc != 0)
        {
            ERROR("Failed to copy file '%s' to %s on agent %s",
                  src, dst, ta);
            if (!non_exist_f)
                return 0;
        }
        else
        {
            RING("File '%s' copied to %s on agent %s", src, dst, ta);
        }
    }
    else
    {
        if ((rc = access(src, F_OK)) < 0)
        {
            ERROR("There is no such tool or library in gnu build: %s",
                  src);
            if (!non_exist_f)
                return 0;
            else
                return rc;
        }
        rc = rcf_ta_put_file(ta, 0, src, dst);
        if (rc != 0)
        {
            ERROR("Failed to put file '%s' to %s:%s", src, ta, dst);
        }
        else
        {
            RING("File '%s' put to %s:%s", src, ta, dst);
        }
    }
    return rc;
}

/* See description in lib-ts.h */
te_errno
libts_copy_socklibs(void)
{
    te_errno        rc;
    unsigned int    n_socklibs;
    cfg_handle     *socklibs = NULL;
    unsigned int    i;
    cfg_val_type    val_type;
    cfg_oid        *oid = NULL;

    rc = cfg_find_pattern("/local:*/socklib:", &n_socklibs, &socklibs);
    if (rc != 0)
    {
        TEST_FAIL("cfg_find_pattern(/local:*/socklib:) failed: %r", rc);
    }
    for (i = 0; i < n_socklibs; ++i)
    {
        const char * const libdir_def = "/usr/lib";
        const char * const remote_libname = "libte-iut.so";

        char   *socklib;
        char   *libdir;
        char   *remote_file;

        val_type = CVT_STRING;
        rc = cfg_get_instance(socklibs[i], &val_type, &socklib);
        if (rc != 0)
        {
            free(socklibs);
            TEST_FAIL("cfg_get_instance() failed: %r", rc);
        }
        if (strlen(socklib) == 0)
        {
            /* Just ignore empty values */
            free(socklib);
            continue;
        }

        rc = cfg_get_oid(socklibs[i], &oid);
        if (rc != 0)
        {
            free(socklib);
            free(socklibs);
            TEST_FAIL("cfg_get_oid() failed: %r", rc);
        }

        val_type = CVT_STRING;
        rc = cfg_get_instance_fmt(&val_type, &libdir,
                                  "/local:%s/libdir:",
                                  CFG_OID_GET_INST_NAME(oid, 1));
        if (rc == TE_RC(TE_CS, TE_ENOENT))
        {
            libdir = (char *)libdir_def;
        }
        else if (rc != 0)
        {
            cfg_free_oid(oid);
            free(socklib);
            free(socklibs);
            TEST_FAIL("%u: cfg_get_instance_fmt() failed", __LINE__);
        }

        /* Prepare name of the remote file to put */
        remote_file = malloc(strlen(libdir) + 1 /* '/' */ +
                             strlen(remote_libname) + 1 /* '\0' */);
        if (remote_file == NULL)
        {
            if (libdir != libdir_def)
                free(libdir);
            cfg_free_oid(oid);
            free(socklib);
            free(socklibs);
            TEST_FAIL("Memory allocation failure");
        }

        strcpy(remote_file, libdir);
        if (libdir != libdir_def)
            free(libdir);
        strcat(remote_file, "/");
        strcat(remote_file, remote_libname);

        rc = libts_file_copy_ta(CFG_OID_GET_INST_NAME(oid, 1), socklib,
                                remote_file, TRUE);
        if (rc != 0)
        {
            free(remote_file);
            cfg_free_oid(oid);
            free(socklib);
            free(socklibs);
            goto cleanup;
        }

        free(socklib);
        rc = cfg_set_instance(socklibs[i], val_type, remote_file);
        if (rc != 0)
        {
            free(remote_file);
            cfg_free_oid(oid);
            free(socklibs);
            TEST_FAIL("cfg_set_instance() failed: %r", rc);
        }

        {
            int rc2;

            rc = rcf_ta_call(CFG_OID_GET_INST_NAME(oid, 1), 0, "shell",
                             &rc2, 3, TRUE, "chmod", "+s,a+rx", remote_file);
            if (rc != 0)
            {
                ERROR("Failed to call 'shell' on %s: %r",
                      CFG_OID_GET_INST_NAME(oid, 1), rc);
                free(remote_file);
                cfg_free_oid(oid);
                free(socklibs);
                goto cleanup;
            }
            if ((rc = rc2) != 0)
            {
                ERROR("Failed to execute 'chmod' on %s: %r",
                      CFG_OID_GET_INST_NAME(oid, 1), rc2);
                free(remote_file);
                cfg_free_oid(oid);
                free(socklibs);
                goto cleanup;
            }
        }
        free(remote_file);
        cfg_free_oid(oid);
    }
    free(socklibs);

cleanup:
    return rc;
}
