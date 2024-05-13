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
  char *filename;

  filename = text_to_cstring(arg);
  canonicalize_path(filename); /* filename can change length here */

  /*
   * Members of the 'pg_read_server_files' role are allowed to access any
   * files on the server as the PG user, so no need to do any further checks
   * here.
   */
  if (is_member_of_role(GetUserId(), DEFAULT_ROLE_READ_SERVER_FILES))
  {
    return filename;
  }

  /* User isn't a member of the default role, so check if it's allowable */























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
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  char *location;
  bool missing_ok = false;
  bool include_dot_dirs = false;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  DIR *dirdesc;
  struct dirent *de;
  MemoryContext oldcontext;

  location = convert_and_check_filename(PG_GETARG_TEXT_PP(0));

  /* check the optional arguments */
  if (PG_NARGS() == 3)
  {








  }

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {

  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {

  }

  /* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  tupdesc = CreateTemplateTupleDesc(1);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "pg_ls_dir", TEXTOID, -1, 0);

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  dirdesc = AllocateDir(location);
  if (!dirdesc)
  {
    /* Return empty tuplestore if appropriate */




    /* Otherwise, we can let ReadDir() throw the error */
  }

  while ((de = ReadDir(dirdesc, location)) != NULL)
  {
    Datum values[1];
    bool nulls[1];

    if (!include_dot_dirs && (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0))
    {
      continue;
    }

    values[0] = CStringGetTextDatum(de->d_name);
    nulls[0] = false;

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
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
  return pg_ls_dir(fcinfo);
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
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  bool randomAccess;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  DIR *dirdesc;
  struct dirent *de;
  MemoryContext oldcontext;

  /* check to see if caller supports us returning a tuplestore */
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {

  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {

  }

  /* The tupdesc and tuplestore must be created in ecxt_per_query_memory */
  oldcontext = MemoryContextSwitchTo(rsinfo->econtext->ecxt_per_query_memory);

  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {

  }

  randomAccess = (rsinfo->allowedModes & SFRM_Materialize_Random) != 0;
  tupstore = tuplestore_begin_heap(randomAccess, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  /*
   * Now walk the directory.  Note that we must do this within a single SRF
   * call, not leave the directory open across multiple calls, since we
   * can't count on the SRF being run to completion.
   */
  dirdesc = AllocateDir(dir);
  if (!dirdesc)
  {
    /* Return empty tuplestore if appropriate */




    /* Otherwise, we can let ReadDir() throw the error */
  }

  while ((de = ReadDir(dirdesc, dir)) != NULL)
  {
    Datum values[3];
    bool nulls[3];
    char path[MAXPGPATH * 2];
    struct stat attrib;

    /* Skip hidden files */
    if (de->d_name[0] == '.')
    {
      continue;
    }

    /* Get the file info */
    snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
    if (stat(path, &attrib) < 0)
    {
      /* Ignore concurrently-deleted files, else complain */





    }

    /* Ignore anything but regular files */
    if (!S_ISREG(attrib.st_mode))
    {
      continue;
    }

    values[0] = CStringGetTextDatum(de->d_name);
    values[1] = Int64GetDatum((int64)attrib.st_size);
    values[2] = TimestampTzGetDatum(time_t_to_timestamptz(attrib.st_mtime));
    memset(nulls, 0, sizeof(nulls));

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

  FreeDir(dirdesc);
  return (Datum)0;
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
  return pg_ls_dir_files(fcinfo, XLOGDIR, false);
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
  return pg_ls_dir_files(fcinfo, XLOGDIR "/archive_status", true);
}
