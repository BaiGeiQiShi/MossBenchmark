                                                                            
   
           
                                                              
                            
   
                                                                         
                                                                        
   
                             
   
                                                                            
   
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

   
                                                         
   
                                                                               
                                                                            
                                                              
   
char *
get_role_password(const char *role, char **logdetail)
{
  TimestampTz vuntil = 0;
  HeapTuple roleTup;
  Datum datum;
  bool isnull;
  char *shadow_pass;

                                    
  roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(role));
  if (!HeapTupleIsValid(roleTup))
  {
    *logdetail = psprintf(_("Role \"%s\" does not exist."), role);
    return NULL;                   
  }

  datum = SysCacheGetAttr(AUTHNAME, roleTup, Anum_pg_authid_rolpassword, &isnull);
  if (isnull)
  {
    ReleaseSysCache(roleTup);
    *logdetail = psprintf(_("User \"%s\" has no password assigned."), role);
    return NULL;                           
  }
  shadow_pass = TextDatumGetCString(datum);

  datum = SysCacheGetAttr(AUTHNAME, roleTup, Anum_pg_authid_rolvaliduntil, &isnull);
  if (!isnull)
  {
    vuntil = DatumGetTimestampTz(datum);
  }

  ReleaseSysCache(roleTup);

     
                                                                     
     
  if (!isnull && vuntil < GetCurrentTimestamp())
  {
    *logdetail = psprintf(_("User \"%s\" has an expired password."), role);
    return NULL;
  }

  return shadow_pass;
}

   
                                                      
   
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

   
                                                                 
                       
   
                                                                       
                                                                    
   
char *
encrypt_password(PasswordType target_type, const char *role, const char *password)
{
  PasswordType guessed_type = get_password_type(password);
  char *encrypted_password;

  if (guessed_type != PASSWORD_TYPE_PLAINTEXT)
  {
       
                                                                       
                                       
       
    return pstrdup(password);
  }

  switch (target_type)
  {
  case PASSWORD_TYPE_MD5:
    encrypted_password = palloc(MD5_PASSWD_LEN + 1);

    if (!pg_md5_encrypt(password, role, strlen(role), encrypted_password))
    {
      elog(ERROR, "password encryption failed");
    }
    return encrypted_password;

  case PASSWORD_TYPE_SCRAM_SHA_256:
    return pg_be_scram_build_verifier(password);

  case PASSWORD_TYPE_PLAINTEXT:
    elog(ERROR, "cannot encrypt password with 'plaintext'");
  }

     
                                                                       
                                                                   
     
  elog(ERROR, "cannot encrypt password to requested type");
  return NULL;                          
}

   
                                                                            
   
                                                                            
                             
                                                                                
                                                                    
   
                                                                       
                                                                 
   
int
md5_crypt_verify(const char *role, const char *shadow_pass, const char *client_pass, const char *md5_salt, int md5_salt_len, char **logdetail)
{
  int retval;
  char crypt_pwd[MD5_PASSWD_LEN + 1];

  Assert(md5_salt_len > 0);

  if (get_password_type(shadow_pass) != PASSWORD_TYPE_MD5)
  {
                                            
    *logdetail = psprintf(_("User \"%s\" has a password that cannot be used with MD5 authentication."), role);
    return STATUS_ERROR;
  }

     
                                                       
     
                                                                       
                                                                             
                                                                           
     
                                                       
  if (!pg_md5_encrypt(shadow_pass + strlen("md5"), md5_salt, md5_salt_len, crypt_pwd))
  {
    return STATUS_ERROR;
  }

  if (strcmp(client_pass, crypt_pwd) == 0)
  {
    retval = STATUS_OK;
  }
  else
  {
    *logdetail = psprintf(_("Password does not match for user \"%s\"."), role);
    retval = STATUS_ERROR;
  }

  return retval;
}

   
                                                                              
   
                                                                   
                          
                                                           
   
                                                                       
                                                                 
   
int
plain_crypt_verify(const char *role, const char *shadow_pass, const char *client_pass, char **logdetail)
{
  char crypt_client_pass[MD5_PASSWD_LEN + 1];

     
                                                                             
                                                                      
                                               
     
  switch (get_password_type(shadow_pass))
  {
  case PASSWORD_TYPE_SCRAM_SHA_256:
    if (scram_verify_plain_password(role, client_pass, shadow_pass))
    {
      return STATUS_OK;
    }
    else
    {
      *logdetail = psprintf(_("Password does not match for user \"%s\"."), role);
      return STATUS_ERROR;
    }
    break;

  case PASSWORD_TYPE_MD5:
    if (!pg_md5_encrypt(client_pass, role, strlen(role), crypt_client_pass))
    {
         
                                                               
                                                                     
                                                                     
                                 
         
      return STATUS_ERROR;
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
    break;

  case PASSWORD_TYPE_PLAINTEXT:

       
                                                                
               
       
    break;
  }

     
                                                                         
                                            
     
  *logdetail = psprintf(_("Password of user \"%s\" is in unrecognized format."), role);
  return STATUS_ERROR;
}
