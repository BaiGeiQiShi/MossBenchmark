/*-------------------------------------------------------------------------
 *
 * strerror.c
 *	  Replacements for standard strerror() and strerror_r() functions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/port/strerror.c
 *
 *-------------------------------------------------------------------------
 */
#include "c.h"

/*
 * Within this file, "strerror" means the platform's function not pg_strerror,* and likewise for "strerror_r"
 */
#undef strerror
#undef strerror_r

static char *
gnuish_strerror_r(int errnum, char *buf, size_t buflen);
static char *
get_errno_symbol(int errnum);
#ifdef WIN32
static char *
win32_socket_strerror(int errnum, char *buf, size_t buflen);
#endif

/*
 * A slightly cleaned-up version of strerror()
 */
char *
pg_strerror(int errnum)
{



}

/*
 * A slightly cleaned-up version of strerror_r()
 */
char *
pg_strerror_r(int errnum, char *buf, size_t buflen)
{

































}

/*
 * Simple wrapper to emulate GNU strerror_r if what the platform provides is
 * POSIX.  Also, if platform lacks strerror_r altogether, fall back to plain
 * strerror; it might not be very thread-safe, but tough luck.
 */
static char *
gnuish_strerror_r(int errnum, char *buf, size_t buflen)
{























}

/*
 * Returns a symbol (e.g. "ENOENT") for an errno code.
 * Returns NULL if the code is unrecognized.
 */
static char *
get_errno_symbol(int errnum)
{



































































































































































}

#ifdef WIN32

/*
 * Windows' strerror() doesn't know the Winsock codes, so handle them this way
 */
static char *
win32_socket_strerror(int errnum, char *buf, size_t buflen)
{
  static HANDLE handleDLL = INVALID_HANDLE_VALUE;

  if (handleDLL == INVALID_HANDLE_VALUE)
  {
    handleDLL = LoadLibraryEx("netmsg.dll", NULL, DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE);
    if (handleDLL == NULL)
    {
      snprintf(buf, buflen, "winsock error %d (could not load netmsg.dll to translate: error code %lu)", errnum, GetLastError());
      return buf;
    }
  }

  ZeroMemory(buf, buflen);
  if (FormatMessage(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_FROM_HMODULE, handleDLL, errnum, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), buf, buflen - 1, NULL) == 0)
  {
    /* Failed to get id */
    snprintf(buf, buflen, "unrecognized winsock error %d", errnum);
  }

  return buf;
}

#endif /* WIN32 */