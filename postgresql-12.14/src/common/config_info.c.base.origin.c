/*-------------------------------------------------------------------------
 *
 * config_info.c
 *		Common code for pg_config output
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/config_info.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/config_info.h"

/*
 * get_configdata(const char *my_exec_path, size_t *configdata_len)
 *
 * Get configure-time constants. The caller is responsible
 * for pfreeing the result.
 */
ConfigData *
get_configdata(const char *my_exec_path, size_t *configdata_len)
{












































































































































































}