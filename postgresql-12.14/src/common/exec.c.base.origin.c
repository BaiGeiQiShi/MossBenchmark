/*-------------------------------------------------------------------------
 *
 * exec.c
 *		Functions for finding and validating executable files
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/common/exec.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Inhibit mingw CRT's auto-globbing of command line arguments */
#if defined(WIN32) && !defined(_MSC_VER)
extern int _CRT_glob = 0; /* 0 turns off globbing; 1 turns it on */
#endif

/*
 * Hacky solution to allow expressing both frontend and backend error reports
 * in one macro call.  First argument of log_error is an errcode() call of
 * some sort (ignored if FRONTEND); the rest are errmsg_internal() arguments,* i.e. message string and any parameters for it.
 *
 * Caller must provide the gettext wrapper around the message string, if
 * appropriate, so that it gets translated in the FRONTEND case; this
 * motivates using errmsg_internal() not errmsg().  We handle appending a
 * newline, if needed, inside the macro, so that there's only one translatable
 * string per call not two.
 */
#ifndef FRONTEND
#define log_error(errcodefn, ...) ereport(LOG, (errcodefn, errmsg_internal(__VA_ARGS__)))
#else
#define log_error(errcodefn, ...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#endif

#ifdef _MSC_VER
#define getcwd(cwd, len) GetCurrentDirectory(len, cwd)
#endif

static int
validate_exec(const char *path);
static int
resolve_symlinks(char *path);
static char *
pipe_read_line(char *cmd, char *line, int maxsize);

#ifdef WIN32
static BOOL
GetTokenUser(HANDLE hToken, PTOKEN_USER *ppTokenUser);
#endif

/*
 * validate_exec -- validate "path" as an executable file
 *
 * returns 0 if the file is found and no error is encountered.
 *		  -1 if the regular file "path" does not exist or cannot be
 *executed. -2 if the file is otherwise valid but cannot be read.
 */
static int
validate_exec(const char *path)
{












































}

/*
 * find_my_exec -- find an absolute path to a valid executable
 *
 *	argv0 is the name passed on the command line
 *	retpath is the output area (must be of size MAXPGPATH)
 *	Returns 0 if OK, -1 if error.
 *
 * The reason we have to work so hard to find an absolute path is that
 * on some platforms we can't do dynamic loading unless we know the
 * executable's location.  Also, we need a full path not a relative
 * path because we will later change working directory.  Finally, we want
 * a true path not a symlink location, so that we can locate other files
 * that are part of our installation relative to the executable.
 */
int
find_my_exec(const char *argv0, char *retpath)
{































































































}

/*
 * resolve_symlinks - resolve symlinks to the underlying file
 *
 * Replace "path" by the absolute path to the referenced file.
 *
 * Returns 0 if OK, -1 if error.
 *
 * Note: we are not particularly tense about producing nice error messages
 * because we are not really expecting error here; we just determined that
 * the symlink does point to a valid executable.
 */
static int
resolve_symlinks(char *path)
{















































































}

/*
 * Find another program in our binary's directory,* then make sure it is the proper version.
 */
int
find_other_exec(const char *argv0, const char *target, const char *versionstr, char *retpath)
{

































}

/*
 * The runtime library's popen() on win32 does not work when being
 * called from a service when running on windows <= 2000, because
 * there is no stdin/stdout/stderr.
 *
 * Executing a command in a pipe and reading the first line from it
 * is all we need.
 */
static char *
pipe_read_line(char *cmd, char *line, int maxsize)
{



























































































































































}

/*
 * pclose() plus useful error reporting
 */
int
pclose_check(FILE *stream)
{






















}

/*
 *	set_pglocale_pgservice
 *
 *	Set application-specific locale and service directory
 *
 *	This function takes the value of argv[0] rather than a full path.
 *
 * (You may be wondering why this is in exec.c.  It requires this module's
 * services and doesn't introduce any new dependencies, so this seems as
 * good as anyplace.)
 */
void
set_pglocale_pgservice(const char *argv0, const char *app)
{
























































}

#ifdef WIN32

/*
 * AddUserToTokenDacl(HANDLE hToken)
 *
 * This function adds the current user account to the restricted
 * token used when we create a restricted process.
 *
 * This is required because of some security changes in Windows
 * that appeared in patches to XP/2K3 and in Vista/2008.
 *
 * On these machines, the Administrator account is not included in
 * the default DACL - you just get Administrators + System. For
 * regular users you get User + System. Because we strip Administrators
 * when we create the restricted token, we are left with only System
 * in the DACL which leads to access denied errors for later CreatePipe()
 * and CreateProcess() calls when running as Administrator.
 *
 * This function fixes this problem by modifying the DACL of the
 * token the process will use, and explicitly re-adding the current
 * user account.  This is still secure because the Administrator account
 * inherits its privileges from the Administrators group - it doesn't
 * have any of its own.
 */
BOOL
AddUserToTokenDacl(HANDLE hToken)
{
  int i;
  ACL_SIZE_INFORMATION asi;
  ACCESS_ALLOWED_ACE *pace;
  DWORD dwNewAclSize;
  DWORD dwSize = 0;
  DWORD dwTokenInfoLength = 0;
  PACL pacl = NULL;
  PTOKEN_USER pTokenUser = NULL;
  TOKEN_DEFAULT_DACL tddNew;
  TOKEN_DEFAULT_DACL *ptdd = NULL;
  TOKEN_INFORMATION_CLASS tic = TokenDefaultDacl;
  BOOL ret = FALSE;

  /* Figure out the buffer size for the DACL info */
  if (!GetTokenInformation(hToken, tic, (LPVOID)NULL, dwTokenInfoLength, &dwSize))
  {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      ptdd = (TOKEN_DEFAULT_DACL *)LocalAlloc(LPTR, dwSize);
      if (ptdd == NULL)
      {
        log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
        goto cleanup;
      }

      if (!GetTokenInformation(hToken, tic, (LPVOID)ptdd, dwSize, &dwSize))
      {
        log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information: error code %lu", GetLastError());
        goto cleanup;
      }
    }
    else
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information buffer size: error code %lu", GetLastError());
      goto cleanup;
    }
  }

  /* Get the ACL info */
  if (!GetAclInformation(ptdd->DefaultDacl, (LPVOID)&asi, (DWORD)sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get ACL information: error code %lu", GetLastError());
    goto cleanup;
  }

  /* Get the current user SID */
  if (!GetTokenUser(hToken, &pTokenUser))
  {
    goto cleanup; /* callee printed a message */
  }

  /* Figure out the size of the new ACL */
  dwNewAclSize = asi.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE) + GetLengthSid(pTokenUser->User.Sid) - sizeof(DWORD);

  /* Allocate the ACL buffer & initialize it */
  pacl = (PACL)LocalAlloc(LPTR, dwNewAclSize);
  if (pacl == NULL)
  {
    log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
    goto cleanup;
  }

  if (!InitializeAcl(pacl, dwNewAclSize, ACL_REVISION))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not initialize ACL: error code %lu", GetLastError());
    goto cleanup;
  }

  /* Loop through the existing ACEs, and build the new ACL */
  for (i = 0; i < (int)asi.AceCount; i++)
  {
    if (!GetAce(ptdd->DefaultDacl, i, (LPVOID *)&pace))
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get ACE: error code %lu", GetLastError());
      goto cleanup;
    }

    if (!AddAce(pacl, ACL_REVISION, MAXDWORD, pace, ((PACE_HEADER)pace)->AceSize))
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not add ACE: error code %lu", GetLastError());
      goto cleanup;
    }
  }

  /* Add the new ACE for the current user */
  if (!AddAccessAllowedAceEx(pacl, ACL_REVISION, OBJECT_INHERIT_ACE, GENERIC_ALL, pTokenUser->User.Sid))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not add access allowed ACE: error code %lu", GetLastError());
    goto cleanup;
  }

  /* Set the new DACL in the token */
  tddNew.DefaultDacl = pacl;

  if (!SetTokenInformation(hToken, tic, (LPVOID)&tddNew, dwNewAclSize))
  {
    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not set token information: error code %lu", GetLastError());
    goto cleanup;
  }

  ret = TRUE;

cleanup:
  if (pTokenUser)
  {
    LocalFree((HLOCAL)pTokenUser);
  }

  if (pacl)
  {
    LocalFree((HLOCAL)pacl);
  }

  if (ptdd)
  {
    LocalFree((HLOCAL)ptdd);
  }

  return ret;
}

/*
 * GetTokenUser(HANDLE hToken, PTOKEN_USER *ppTokenUser)
 *
 * Get the users token information from a process token.
 *
 * The caller of this function is responsible for calling LocalFree() on the
 * returned TOKEN_USER memory.
 */
static BOOL
GetTokenUser(HANDLE hToken, PTOKEN_USER *ppTokenUser)
{
  DWORD dwLength;

  *ppTokenUser = NULL;

  if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwLength))
  {
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      *ppTokenUser = (PTOKEN_USER)LocalAlloc(LPTR, dwLength);

      if (*ppTokenUser == NULL)
      {
        log_error(errcode(ERRCODE_OUT_OF_MEMORY), _("out of memory"));
        return FALSE;
      }
    }
    else
    {
      log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information buffer size: error code %lu", GetLastError());
      return FALSE;
    }
  }

  if (!GetTokenInformation(hToken, TokenUser, *ppTokenUser, dwLength, &dwLength))
  {
    LocalFree(*ppTokenUser);
    *ppTokenUser = NULL;

    log_error(errcode(ERRCODE_SYSTEM_ERROR), "could not get token information: error code %lu", GetLastError());
    return FALSE;
  }

  /* Memory in *ppTokenUser is LocalFree():d by the caller */
  return TRUE;
}

#endif