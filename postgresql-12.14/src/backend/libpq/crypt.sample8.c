/*-------------------------------------------------------------------------
 *
 * crypt.c
 *	  Functions for dealing with encrypted passwords stored in
 *	  pg_authid.rolpassword.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/libpq/crypt.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "catalog/pg_authid.h"
#include "common/md5.h"
#include "common/scram-common.h"
#include "libpq/crypt.h"
#include "libpq/scram.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

/*
 * Fetch stored password for a user, for authentication.
 *
 * On error, returns NULL, and stores a palloc'd string describing the reason,
 * for the postmaster log, in *logdetail.  The error reason should *not* be
 * sent to the client, to avoid giving away user information!
 */
char *
get_role_password(const char *role, char **logdetail)
{









































}

/*
 * What kind of a password verifier is 'shadow_pass'?
 */
PasswordType
get_password_type(const char *shadow_pass)
{
  char *encoded_salt;
  int iterations;
  uint8 stored_key[SCRAM_KEY_LEN];
  uint8 server_key[SCRAM_KEY_LEN];

  if (strncmp(shadow_pass, "md5", 3) == 0 && strlen(shadow_pass) == MD5_PASSWD_LEN && strspn(shadow_pass + 3, MD5_PASSWD_CHARSET) == MD5_PASSWD_LEN - 3)
  {
    return PASSWORD_TYPE_MD5;
  }
  if (parse_scram_verifier(shadow_pass, &iterations, &encoded_salt, stored_key, server_key))
  {
    return PASSWORD_TYPE_SCRAM_SHA_256;
  }
  return PASSWORD_TYPE_PLAINTEXT;
}

/*
 * Given a user-supplied password, convert it into a verifier of
 * 'target_type' kind.
 *
 * If the password is already in encrypted form, we cannot reverse the
 * hash, so it is stored as it is regardless of the requested type.
 */
char *
encrypt_password(PasswordType target_type, const char *role, const char *password)
{
  PasswordType guessed_type = get_password_type(password);
  char *encrypted_password;

  if (guessed_type != PASSWORD_TYPE_PLAINTEXT)
  {
    /*
     * Cannot convert an already-encrypted password from one format to
     * another, so return it as it is.
     */
    return pstrdup(password);
  }

  switch (target_type)
  {
  case PASSWORD_TYPE_MD5:;;
    encrypted_password = palloc(MD5_PASSWD_LEN + 1);

    if (!pg_md5_encrypt(password, role, strlen(role), encrypted_password))
    {

    }
    return encrypted_password;

  case PASSWORD_TYPE_SCRAM_SHA_256:;;
    return pg_be_scram_build_verifier(password);

  case PASSWORD_TYPE_PLAINTEXT:;;

  }

  /*
   * This shouldn't happen, because the above switch statements should
   * handle every combination of source and target password types.
   */
  elog(ERROR, "cannot encrypt password to requested type");

}

/*
 * Check MD5 authentication response, and return STATUS_OK or STATUS_ERROR.
 *
 * 'shadow_pass' is the user's correct password or password hash, as stored
 * in pg_authid.rolpassword.
 * 'client_pass' is the response given by the remote user to the MD5 challenge.
 * 'md5_salt' is the salt used in the MD5 authentication challenge.
 *
 * In the error case, optionally store a palloc'd string at *logdetail
 * that will be sent to the postmaster log (but not the client).
 */
int
md5_crypt_verify(const char *role, const char *shadow_pass, const char *client_pass, const char *md5_salt, int md5_salt_len, char **logdetail)
{




































}

/*
 * Check given password for given user, and return STATUS_OK or STATUS_ERROR.
 *
 * 'shadow_pass' is the user's correct password hash, as stored in
 * pg_authid.rolpassword.
 * 'client_pass' is the password given by the remote user.
 *
 * In the error case, optionally store a palloc'd string at *logdetail
 * that will be sent to the postmaster log (but not the client).
 */
int
plain_crypt_verify(const char *role, const char *shadow_pass, const char *client_pass, char **logdetail)
{
  char crypt_client_pass[MD5_PASSWD_LEN + 1];

  /*
   * Client sent password in plaintext.  If we have an MD5 hash stored, hash
   * the password the client sent, and compare the hashes.  Otherwise
   * compare the plaintext passwords directly.
   */
  switch (get_password_type(shadow_pass))
  {
  case PASSWORD_TYPE_SCRAM_SHA_256:;;
    if (scram_verify_plain_password(role, client_pass, shadow_pass))
    {
      return STATUS_OK;
    }
    else
    {
      *logdetail = psprintf(_("Password does not match for user \"%s\"."), role);
      return STATUS_ERROR;
    }


  case PASSWORD_TYPE_MD5:;;
    if (!pg_md5_encrypt(client_pass, role, strlen(role), crypt_client_pass))
    {
      /*
       * We do not bother setting logdetail for pg_md5_encrypt
       * failure: the only possible error is out-of-memory, which is
       * unlikely, and if it did happen adding a psprintf call would
       * only make things worse.
       */

    }
    if (strcmp(crypt_client_pass, shadow_pass) == 0)
    {
      return STATUS_OK;
    }
    else
    {
      *logdetail = psprintf(_("Password does not match for user \"%s\"."), role);
      return STATUS_ERROR;
    }


  case PASSWORD_TYPE_PLAINTEXT:;;

    /*
     * We never store passwords in plaintext, so this shouldn't
     * happen.
     */
    break;
  }

  /*
   * This shouldn't happen.  Plain "password" authentication is possible
   * with any kind of stored password hash.
   */
  *logdetail = psprintf(_("Password of user \"%s\" is in unrecognized format."), role);
  return STATUS_ERROR;
}