/*-------------------------------------------------------------------------
 *
 * genfile.c
 *		Functions for direct access to files
 *
 *
 * Copyright (c) 2004-2019, PostgreSQL Global Development Group
 *
 * Author: Andreas Pflug <pgadmin@pse-consulting.de>
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/genfile.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "access/htup_details.h"
#include "access/xlog_internal.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace_d.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "postmaster/syslogger.h"
#include "storage/fd.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

/*
 * Convert a "text" filename argument to C string, and check it's allowable.
 *
 * Filename may be absolute or relative to the DataDir, but we only allow
 * absolute paths that match DataDir or Log_directory.
 *
 * This does a privilege check against the 'pg_read_server_files' role, so
 * this function is really only appropriate for callers who are only checking
 * 'read' access.  Do not use this function if you are looking for a check
 * for 'write' or 'program' access without updating it to access the type
 * of check as an argument and checking the appropriate role membership.
 */
static char *
convert_and_check_filename(text *arg)
{







































}

/*
 * Read a section of a file, returning it as bytea
 *
 * Caller is responsible for all permissions checking.
 *
 * We read the whole of the file when bytes_to_read is negative.
 */
static bytea *
read_binary_file(const char *filename, int64 seek_offset, int64 bytes_to_read, bool missing_ok)
{




































































































}

/*
 * Similar to read_binary_file, but we verify that the contents are valid
 * in the database encoding.
 */
static text *
read_text_file(const char *filename, int64 seek_offset, int64 bytes_to_read, bool missing_ok)
{
















}

/*
 * Read a section of a file, returning it as text
 *
 * This function is kept to support adminpack 1.0.
 */
Datum
pg_read_file(PG_FUNCTION_ARGS)
{







































}

/*
 * Read a section of a file, returning it as text
 *
 * No superuser check done here- instead privileges are handled by the
 * GRANT system.
 */
Datum
pg_read_file_v2(PG_FUNCTION_ARGS)
{


































}

/*
 * Read a section of a file, returning it as bytea
 */
Datum
pg_read_binary_file(PG_FUNCTION_ARGS)
{


































}

/*
 * Wrapper functions for the 1 and 3 argument variants of pg_read_file_v2()
 * and pg_binary_read_file().
 *
 * These are necessary to pass the sanity check in opr_sanity, which checks
 * that all built-in functions that share the implementing C function take
 * the same number of arguments.
 */
Datum
pg_read_file_off_len(PG_FUNCTION_ARGS)
{

}

Datum
pg_read_file_all(PG_FUNCTION_ARGS)
{

}

Datum
pg_read_binary_file_off_len(PG_FUNCTION_ARGS)
{

}

Datum
pg_read_binary_file_all(PG_FUNCTION_ARGS)
{

}

/*
 * stat a file
 */
Datum
pg_stat_file(PG_FUNCTION_ARGS)
{






























































}

/*
 * stat a file (1 argument version)
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments
 */
Datum
pg_stat_file_1arg(PG_FUNCTION_ARGS)
{

}

/*
 * List a directory (returns the filenames only)
 */
Datum
pg_ls_dir(PG_FUNCTION_ARGS)
{















































































}

/*
 * List a directory (1 argument version)
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments.
 */
Datum
pg_ls_dir_1arg(PG_FUNCTION_ARGS)
{

}

/*
 * Generic function to return a directory listing of files.
 *
 * If the directory isn't there, silently return an empty set if missing_ok.
 * Other unreadable-directory cases throw an error.
 */
static Datum
pg_ls_dir_files(FunctionCallInfo fcinfo, const char *dir, bool missing_ok)
{



























































































}

/* Function to return the list of files in the log directory */
Datum
pg_ls_logdir(PG_FUNCTION_ARGS)
{

}

/* Function to return the list of files in the WAL directory */
Datum
pg_ls_waldir(PG_FUNCTION_ARGS)
{

}

/*
 * Generic function to return the list of files in pgsql_tmp
 */
static Datum
pg_ls_tmpdir(FunctionCallInfo fcinfo, Oid tblspc)
{









}

/*
 * Function to return the list of temporary files in the pg_default tablespace's
 * pgsql_tmp directory
 */
Datum
pg_ls_tmpdir_noargs(PG_FUNCTION_ARGS)
{

}

/*
 * Function to return the list of temporary files in the specified tablespace's
 * pgsql_tmp directory
 */
Datum
pg_ls_tmpdir_1arg(PG_FUNCTION_ARGS)
{

}

/*
 * Function to return the list of files in the WAL archive status directory.
 */
Datum
pg_ls_archive_statusdir(PG_FUNCTION_ARGS)
{

}