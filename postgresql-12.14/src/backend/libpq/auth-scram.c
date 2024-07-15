                                                                            
   
                
                                                                     
   
                                            
                                                   
                                                   
                                                   
   
                              
   
                                                                       
                                                 
   
                                                                          
                                                                         
                                           
   
                                                                    
                                                                       
                                                                         
                                                                          
                                                                           
                                                                        
                                                                          
                                                                   
                                                                         
                                                                      
                                                                     
                                  
   
   
                                                                           
                            
   
                  
                  
   
                                                                       
                                                                       
                                                                           
                                                                           
                                                                        
                                                                           
                                                                            
                                                                          
                                                                           
                                     
   
                                                                       
                                                                         
                                                                         
                                                                    
                                                                      
                                                                        
                                                                         
                                                                  
                       
   
                  
                  
   
                                                                         
                                                                         
                                                                         
                                                                           
                                                                          
                                                                     
                                                                    
                                                                           
                                                                          
                                                                   
                       
   
                                                                         
                                                                         
                                                                           
                                                                          
                                                             
   
   
                                                                         
                                                                        
   
                                  
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "access/xlog.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_control.h"
#include "common/base64.h"
#include "common/saslprep.h"
#include "common/scram-common.h"
#include "common/sha2.h"
#include "libpq/auth.h"
#include "libpq/crypt.h"
#include "libpq/scram.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/timestamp.h"

   
                                                                         
                          
   
typedef enum
{
  SCRAM_AUTH_INIT,
  SCRAM_AUTH_SALT_SENT,
  SCRAM_AUTH_FINISHED
} scram_state_enum;

typedef struct
{
  scram_state_enum state;

  const char *username;                                   

  Port *port;
  bool channel_binding_in_use;

  int iterations;
  char *salt;                     
  uint8 StoredKey[SCRAM_KEY_LEN];
  uint8 ServerKey[SCRAM_KEY_LEN];

                                               
  char cbind_flag;
  char *client_first_message_bare;
  char *client_username;
  char *client_nonce;

                                                
  char *client_final_message_without_proof;
  char *client_final_nonce;
  char ClientProof[SCRAM_KEY_LEN];

                                      
  char *server_first_message;
  char *server_nonce;

     
                                                                             
                                                                         
                                                                           
                  
     
  bool doomed;
  char *logdetail;
} scram_state;

static void
read_client_first_message(scram_state *state, const char *input);
static void
read_client_final_message(scram_state *state, const char *input);
static char *
build_server_first_message(scram_state *state);
static char *
build_server_final_message(scram_state *state);
static bool
verify_client_proof(scram_state *state);
static bool
verify_final_nonce(scram_state *state);
static void
mock_scram_verifier(const char *username, int *iterations, char **salt, uint8 *stored_key, uint8 *server_key);
static bool
is_scram_printable(char *p);
static char *
sanitize_char(char c);
static char *
sanitize_str(const char *s);
static char *
scram_mock_salt(const char *username);

   
                              
   
                                                            
   
                                                                   
                                                                      
                            
   
void
pg_be_scram_get_mechanisms(Port *port, StringInfo buf)
{
     
                                                                         
                                                                        
                                                                            
                                                   
     
#ifdef HAVE_BE_TLS_GET_CERTIFICATE_HASH
  if (port->ssl_in_use)
  {
    appendStringInfoString(buf, SCRAM_SHA_256_PLUS_NAME);
    appendStringInfoChar(buf, '\0');
  }
#endif
  appendStringInfoString(buf, SCRAM_SHA_256_NAME);
  appendStringInfoChar(buf, '\0');
}

   
                    
   
                                                                        
                                                                          
                                                           
   
                                                                           
                                                                      
                                 
   
                                                                              
                                                                          
                                                                             
                                                                             
              
   
void *
pg_be_scram_init(Port *port, const char *selected_mech, const char *shadow_pass)
{
  scram_state *state;
  bool got_verifier;

  state = (scram_state *)palloc0(sizeof(scram_state));
  state->port = port;
  state->state = SCRAM_AUTH_INIT;

     
                                   
     
                                                                           
                                                                         
                                                                            
                                                                            
                                                          
     
#ifdef HAVE_BE_TLS_GET_CERTIFICATE_HASH
  if (strcmp(selected_mech, SCRAM_SHA_256_PLUS_NAME) == 0 && port->ssl_in_use)
  {
    state->channel_binding_in_use = true;
  }
  else
#endif
      if (strcmp(selected_mech, SCRAM_SHA_256_NAME) == 0)
  {
    state->channel_binding_in_use = false;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("client selected an invalid SASL authentication mechanism")));
  }

     
                                         
     
  if (shadow_pass)
  {
    int password_type = get_password_type(shadow_pass);

    if (password_type == PASSWORD_TYPE_SCRAM_SHA_256)
    {
      if (parse_scram_verifier(shadow_pass, &state->iterations, &state->salt, state->StoredKey, state->ServerKey))
      {
        got_verifier = true;
      }
      else
      {
           
                                                                       
                   
           
        ereport(LOG, (errmsg("invalid SCRAM verifier for user \"%s\"", state->port->user_name)));
        got_verifier = false;
      }
    }
    else
    {
         
                                                                    
                                           
         
      state->logdetail = psprintf(_("User \"%s\" does not have a valid SCRAM verifier."), state->port->user_name);
      got_verifier = false;
    }
  }
  else
  {
       
                                                                           
                                                                          
               
       
    got_verifier = false;
  }

     
                                                                          
                                                                        
                                                                       
               
     
  if (!got_verifier)
  {
    mock_scram_verifier(state->port->user_name, &state->iterations, &state->salt, state->StoredKey, state->ServerKey);
    state->doomed = true;
  }

  return state;
}

   
                                             
   
                                                                        
                                                                          
                                                                         
                                                                         
                                                                       
                                                                      
                                                              
   
                                                                         
                                                                         
                                                                         
                
   
int
pg_be_scram_exchange(void *opaq, const char *input, int inputlen, char **output, int *outputlen, char **logdetail)
{
  scram_state *state = (scram_state *)opaq;
  int result;

  *output = NULL;

     
                                                                      
                                                                        
                                                                      
                              
     
  if (input == NULL)
  {
    Assert(state->state == SCRAM_AUTH_INIT);

    *output = pstrdup("");
    *outputlen = 0;
    return SASL_EXCHANGE_CONTINUE;
  }

     
                                                                             
                                        
     
  if (inputlen == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("The message is empty.")));
  }
  if (inputlen != strlen(input))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Message length does not match input length.")));
  }

  switch (state->state)
  {
  case SCRAM_AUTH_INIT:

       
                                                                    
                                                                      
                      
       
    read_client_first_message(state, input);

                                           
    *output = build_server_first_message(state);

    state->state = SCRAM_AUTH_SALT_SENT;
    result = SASL_EXCHANGE_CONTINUE;
    break;

  case SCRAM_AUTH_SALT_SENT:

       
                                                                
                                                                       
                                      
       
    read_client_final_message(state, input);

    if (!verify_final_nonce(state))
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("invalid SCRAM response"), errdetail("Nonce does not match.")));
    }

       
                                                       
       
                                                                       
                                               
       
                                                       
                                                                       
                                                                     
                                                                     
                                                                      
                                                                       
       
                                                                       
                                                                    
                                                                      
                                          
       
    if (!verify_client_proof(state) || state->doomed)
    {
      result = SASL_EXCHANGE_FAILURE;
      break;
    }

                                        
    *output = build_server_final_message(state);

                  
    result = SASL_EXCHANGE_SUCCESS;
    state->state = SCRAM_AUTH_FINISHED;
    break;

  default:
    elog(ERROR, "invalid SCRAM exchange state");
    result = SASL_EXCHANGE_FAILURE;
  }

  if (result == SASL_EXCHANGE_FAILURE && state->logdetail && logdetail)
  {
    *logdetail = state->logdetail;
  }

  if (*output)
  {
    *outputlen = strlen(*output);
  }

  return result;
}

   
                                                                           
   
                                                                    
   
char *
pg_be_scram_build_verifier(const char *password)
{
  char *prep_password;
  pg_saslprep_rc rc;
  char saltbuf[SCRAM_DEFAULT_SALT_LEN];
  char *result;

     
                                                                          
                                                                            
                                                                         
     
  rc = pg_saslprep(password, &prep_password);
  if (rc == SASLPREP_SUCCESS)
  {
    password = (const char *)prep_password;
  }

                            
  if (!pg_strong_random(saltbuf, SCRAM_DEFAULT_SALT_LEN))
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate random salt")));
  }

  result = scram_build_verifier(saltbuf, SCRAM_DEFAULT_SALT_LEN, SCRAM_DEFAULT_ITERATIONS, password);

  if (prep_password)
  {
    pfree(prep_password);
  }

  return result;
}

   
                                                                            
                                                                            
                                 
   
bool
scram_verify_plain_password(const char *username, const char *password, const char *verifier)
{
  char *encoded_salt;
  char *salt;
  int saltlen;
  int iterations;
  uint8 salted_password[SCRAM_KEY_LEN];
  uint8 stored_key[SCRAM_KEY_LEN];
  uint8 server_key[SCRAM_KEY_LEN];
  uint8 computed_key[SCRAM_KEY_LEN];
  char *prep_password;
  pg_saslprep_rc rc;

  if (!parse_scram_verifier(verifier, &iterations, &encoded_salt, stored_key, server_key))
  {
       
                                                                           
       
    ereport(LOG, (errmsg("invalid SCRAM verifier for user \"%s\"", username)));
    return false;
  }

  salt = palloc(pg_b64_dec_len(strlen(encoded_salt)));
  saltlen = pg_b64_decode(encoded_salt, strlen(encoded_salt), salt);
  if (saltlen == -1)
  {
    ereport(LOG, (errmsg("invalid SCRAM verifier for user \"%s\"", username)));
    return false;
  }

                              
  rc = pg_saslprep(password, &prep_password);
  if (rc == SASLPREP_SUCCESS)
  {
    password = prep_password;
  }

                                                                        
  scram_SaltedPassword(password, salt, saltlen, iterations, salted_password);
  scram_ServerKey(salted_password, computed_key);

  if (prep_password)
  {
    pfree(prep_password);
  }

     
                                                                      
                             
     
  return memcmp(computed_key, server_key, SCRAM_KEY_LEN) == 0;
}

   
                                                      
   
                                                                         
                                                                              
                                                                        
                                                                         
                                                                  
   
                                                                            
   
bool
parse_scram_verifier(const char *verifier, int *iterations, char **salt, uint8 *stored_key, uint8 *server_key)
{
  char *v;
  char *p;
  char *scheme_str;
  char *salt_str;
  char *iterations_str;
  char *storedkey_str;
  char *serverkey_str;
  int decoded_len;
  char *decoded_salt_buf;
  char *decoded_stored_buf;
  char *decoded_server_buf;

     
                              
     
                                                               
     
  v = pstrdup(verifier);
  if ((scheme_str = strtok(v, "$")) == NULL)
  {
    goto invalid_verifier;
  }
  if ((iterations_str = strtok(NULL, ":")) == NULL)
  {
    goto invalid_verifier;
  }
  if ((salt_str = strtok(NULL, "$")) == NULL)
  {
    goto invalid_verifier;
  }
  if ((storedkey_str = strtok(NULL, ":")) == NULL)
  {
    goto invalid_verifier;
  }
  if ((serverkey_str = strtok(NULL, "")) == NULL)
  {
    goto invalid_verifier;
  }

                        
  if (strcmp(scheme_str, "SCRAM-SHA-256") != 0)
  {
    goto invalid_verifier;
  }

  errno = 0;
  *iterations = strtol(iterations_str, &p, 10);
  if (*p || errno != 0)
  {
    goto invalid_verifier;
  }

     
                                                                       
                                                           
     
  decoded_salt_buf = palloc(pg_b64_dec_len(strlen(salt_str)));
  decoded_len = pg_b64_decode(salt_str, strlen(salt_str), decoded_salt_buf);
  if (decoded_len < 0)
  {
    goto invalid_verifier;
  }
  *salt = pstrdup(salt_str);

     
                                     
     
  decoded_stored_buf = palloc(pg_b64_dec_len(strlen(storedkey_str)));
  decoded_len = pg_b64_decode(storedkey_str, strlen(storedkey_str), decoded_stored_buf);
  if (decoded_len != SCRAM_KEY_LEN)
  {
    goto invalid_verifier;
  }
  memcpy(stored_key, decoded_stored_buf, SCRAM_KEY_LEN);

  decoded_server_buf = palloc(pg_b64_dec_len(strlen(serverkey_str)));
  decoded_len = pg_b64_decode(serverkey_str, strlen(serverkey_str), decoded_server_buf);
  if (decoded_len != SCRAM_KEY_LEN)
  {
    goto invalid_verifier;
  }
  memcpy(server_key, decoded_server_buf, SCRAM_KEY_LEN);

  return true;

invalid_verifier:
  *salt = NULL;
  return false;
}

   
                                                                         
   
                                                                     
                                                                   
                                                    
   
                                                                          
                                                                     
                                                         
   
static void
mock_scram_verifier(const char *username, int *iterations, char **salt, uint8 *stored_key, uint8 *server_key)
{
  char *raw_salt;
  char *encoded_salt;
  int encoded_len;

                                   
  raw_salt = scram_mock_salt(username);

  encoded_salt = (char *)palloc(pg_b64_enc_len(SCRAM_DEFAULT_SALT_LEN) + 1);
  encoded_len = pg_b64_encode(raw_salt, SCRAM_DEFAULT_SALT_LEN, encoded_salt);
  encoded_salt[encoded_len] = '\0';

  *salt = encoded_salt;
  *iterations = SCRAM_DEFAULT_ITERATIONS;

                                                                       
  memset(stored_key, 0, SCRAM_KEY_LEN);
  memset(server_key, 0, SCRAM_KEY_LEN);
}

   
                                                                         
   
static char *
read_attr_value(char **input, char attr)
{
  char *begin = *input;
  char *end;

  if (*begin != attr)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Expected attribute \"%c\" but found \"%s\".", attr, sanitize_char(*begin))));
  }
  begin++;

  if (*begin != '=')
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Expected character \"=\" for attribute \"%c\".", attr)));
  }
  begin++;

  end = begin;
  while (*end && *end != ',')
  {
    end++;
  }

  if (*end)
  {
    *end = '\0';
    *input = end + 1;
  }
  else
  {
    *input = end;
  }

  return begin;
}

static bool
is_scram_printable(char *p)
{
           
                                                                
     
                                    
                                          
                                                
                               
           
     
  for (; *p; p++)
  {
    if (*p < 0x21 || *p > 0x7E || *p == 0x2C            )
    {
      return false;
    }
  }
  return true;
}

   
                                                                     
   
                                                                        
                               
   
                                                   
   
static char *
sanitize_char(char c)
{
  static char buf[5];

  if (c >= 0x21 && c <= 0x7E)
  {
    snprintf(buf, sizeof(buf), "'%c'", c);
  }
  else
  {
    snprintf(buf, sizeof(buf), "0x%02x", (unsigned char)c);
  }
  return buf;
}

   
                                                                      
   
                                                                    
                                                      
   
                                                   
   
static char *
sanitize_str(const char *s)
{
  static char buf[30 + 1];
  int i;

  for (i = 0; i < sizeof(buf) - 1; i++)
  {
    char c = s[i];

    if (c == '\0')
    {
      break;
    }

    if (c >= 0x21 && c <= 0x7E)
    {
      buf[i] = c;
    }
    else
    {
      buf[i] = '?';
    }
  }
  buf[i] = '\0';
  return buf;
}

   
                                                                  
   
                                       
   
static char *
read_any_attr(char **input, char *attr_p)
{
  char *begin = *input;
  char *end;
  char attr = *begin;

           
                                    
                                                  
                                 
           
     
  if (!((attr >= 'A' && attr <= 'Z') || (attr >= 'a' && attr <= 'z')))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Attribute expected, but found invalid character \"%s\".", sanitize_char(attr))));
  }
  if (attr_p)
  {
    *attr_p = attr;
  }
  begin++;

  if (*begin != '=')
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Expected character \"=\" for attribute \"%c\".", attr)));
  }
  begin++;

  end = begin;
  while (*end && *end != ',')
  {
    end++;
  }

  if (*end)
  {
    *end = '\0';
    *input = end + 1;
  }
  else
  {
    *input = end;
  }

  return begin;
}

   
                                                                          
                                    
   
                                                                            
   
static void
read_client_first_message(scram_state *state, const char *input)
{
  char *p = pstrdup(input);
  char *channel_binding_type;

           
                                                            
     
                                                        
                                  
     
                                 
                                
     
                                                 
                                       
                                              
                            
     
                                                  
                                                            
                                                        
                                                 
                                                     
                                                        
     
                                                        
                                  
                                                         
                                                            
                                                           
                                       
     
                                  
                                                    
     
                                          
                                                          
                                                 
                         
     
                                        
                                             
     
                             
     
                                 
                              
                                              
     
                            
                                               
     
                  
                                          
     
                                                                      
                                                                          
                                                                            
                                                                           
                                                                         
                       
           
     

     
                                                                             
                
     
  state->cbind_flag = *p;
  switch (*p)
  {
  case 'n':

       
                                                                 
                                                            
       
    if (state->channel_binding_in_use)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("The client selected SCRAM-SHA-256-PLUS, but the SCRAM message does not include channel binding data.")));
    }

    p++;
    if (*p != ',')
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Comma expected, but found character \"%s\".", sanitize_char(*p))));
    }
    p++;
    break;
  case 'y':

       
                                                                      
                                                                       
                                    
       
    if (state->channel_binding_in_use)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("The client selected SCRAM-SHA-256-PLUS, but the SCRAM message does not include channel binding data.")));
    }

#ifdef HAVE_BE_TLS_GET_CERTIFICATE_HASH
    if (state->port->ssl_in_use)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), errmsg("SCRAM channel binding negotiation error"),
                         errdetail("The client supports SCRAM channel binding but thinks the server does not.  "
                                   "However, this server does support channel binding.")));
    }
#endif
    p++;
    if (*p != ',')
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Comma expected, but found character \"%s\".", sanitize_char(*p))));
    }
    p++;
    break;
  case 'p':

       
                                                                  
                                                
       
    if (!state->channel_binding_in_use)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("The client selected SCRAM-SHA-256 without channel binding, but the SCRAM message includes channel binding data.")));
    }

    channel_binding_type = read_attr_value(&p, 'p');

       
                                                   
                             
       
    if (strcmp(channel_binding_type, "tls-server-end-point") != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), (errmsg("unsupported SCRAM channel-binding type \"%s\"", sanitize_str(channel_binding_type)))));
    }
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Unexpected channel-binding flag \"%s\".", sanitize_char(*p))));
  }

     
                                                                             
     
  if (*p == 'a')
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("client uses authorization identity, but it is not supported")));
  }
  if (*p != ',')
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Unexpected attribute \"%s\" in client-first-message.", sanitize_char(*p))));
  }
  p++;

  state->client_first_message_bare = pstrdup(p);

     
                                                                    
     
                                                                          
                                                                             
                                              
     
  if (*p == 'm')
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("client requires an unsupported SCRAM extension")));
  }

     
                                                                          
                                                                        
                                                 
     
  state->client_username = read_attr_value(&p, 'n');

                                                                         
  state->client_nonce = read_attr_value(&p, 'r');
  if (!is_scram_printable(state->client_nonce))
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("non-printable characters in SCRAM nonce")));
  }

     
                                                                          
                                             
     
  while (*p != '\0')
  {
    read_any_attr(&p, NULL);
  }

                
}

   
                                                                      
                          
   
static bool
verify_final_nonce(scram_state *state)
{
  int client_nonce_len = strlen(state->client_nonce);
  int server_nonce_len = strlen(state->server_nonce);
  int final_nonce_len = strlen(state->client_final_nonce);

  if (final_nonce_len != client_nonce_len + server_nonce_len)
  {
    return false;
  }
  if (memcmp(state->client_final_nonce, state->client_nonce, client_nonce_len) != 0)
  {
    return false;
  }
  if (memcmp(state->client_final_nonce + client_nonce_len, state->server_nonce, server_nonce_len) != 0)
  {
    return false;
  }

  return true;
}

   
                                                                       
                          
   
static bool
verify_client_proof(scram_state *state)
{
  uint8 ClientSignature[SCRAM_KEY_LEN];
  uint8 ClientKey[SCRAM_KEY_LEN];
  uint8 client_StoredKey[SCRAM_KEY_LEN];
  scram_HMAC_ctx ctx;
  int i;

                                 
  scram_HMAC_init(&ctx, state->StoredKey, SCRAM_KEY_LEN);
  scram_HMAC_update(&ctx, state->client_first_message_bare, strlen(state->client_first_message_bare));
  scram_HMAC_update(&ctx, ",", 1);
  scram_HMAC_update(&ctx, state->server_first_message, strlen(state->server_first_message));
  scram_HMAC_update(&ctx, ",", 1);
  scram_HMAC_update(&ctx, state->client_final_message_without_proof, strlen(state->client_final_message_without_proof));
  scram_HMAC_final(ClientSignature, &ctx);

                                                                       
  for (i = 0; i < SCRAM_KEY_LEN; i++)
  {
    ClientKey[i] = state->ClientProof[i] ^ ClientSignature[i];
  }

                                                         
  scram_H(ClientKey, SCRAM_KEY_LEN, client_StoredKey);

  if (memcmp(client_StoredKey, state->StoredKey, SCRAM_KEY_LEN) != 0)
  {
    return false;
  }

  return true;
}

   
                                                                     
                           
   
static char *
build_server_first_message(scram_state *state)
{
           
                                                            
     
                            
                                                 
                                           
     
                                        
                                             
     
                             
     
                             
     
                             
     
                                         
                                
     
              
     
                                                                            
           
     

     
                                                                            
                                                                       
                                                                    
     
  char raw_nonce[SCRAM_RAW_NONCE_LEN];
  int encoded_len;

  if (!pg_strong_random(raw_nonce, SCRAM_RAW_NONCE_LEN))
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("could not generate random nonce")));
  }

  state->server_nonce = palloc(pg_b64_enc_len(SCRAM_RAW_NONCE_LEN) + 1);
  encoded_len = pg_b64_encode(raw_nonce, SCRAM_RAW_NONCE_LEN, state->server_nonce);
  state->server_nonce[encoded_len] = '\0';

  state->server_first_message = psprintf("r=%s%s,s=%s,i=%u", state->client_nonce, state->server_nonce, state->salt, state->iterations);

  return pstrdup(state->server_first_message);
}

   
                                                          
   
static void
read_client_final_message(scram_state *state, const char *input)
{
  char attr;
  char *channel_binding;
  char *value;
  char *begin, *proof;
  char *p;
  char *client_proof;

  begin = p = pstrdup(input);

           
                                                            
     
                                                        
                                  
                                                         
                                                            
                                                           
                                       
     
                                              
                                            
                                                      
                             
     
                                   
                                             
     
                             
     
                                          
                                         
                      
     
                            
                                                       
           
     

     
                                                                          
                                                                     
     
  channel_binding = read_attr_value(&p, 'c');
  if (state->channel_binding_in_use)
  {
#ifdef HAVE_BE_TLS_GET_CERTIFICATE_HASH
    const char *cbind_data = NULL;
    size_t cbind_data_len = 0;
    size_t cbind_header_len;
    char *cbind_input;
    size_t cbind_input_len;
    char *b64_message;
    int b64_message_len;

    Assert(state->cbind_flag == 'p');

                                                     
    cbind_data = be_tls_get_certificate_hash(state->port, &cbind_data_len);

                           
    if (cbind_data == NULL || cbind_data_len == 0)
    {
      elog(ERROR, "could not get server certificate hash");
    }

    cbind_header_len = strlen("p=tls-server-end-point,,");               
    cbind_input_len = cbind_header_len + cbind_data_len;
    cbind_input = palloc(cbind_input_len);
    snprintf(cbind_input, cbind_input_len, "p=tls-server-end-point,,");
    memcpy(cbind_input + cbind_header_len, cbind_data, cbind_data_len);

    b64_message = palloc(pg_b64_enc_len(cbind_input_len) + 1);
    b64_message_len = pg_b64_encode(cbind_input, cbind_input_len, b64_message);
    b64_message[b64_message_len] = '\0';

       
                                                                           
               
       
    if (strcmp(channel_binding, b64_message) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION), (errmsg("SCRAM channel binding check failed"))));
    }
#else
                                                                   
    elog(ERROR, "channel binding not supported by this build");
#endif
  }
  else
  {
       
                                                                         
                                                                      
                                                                           
                                            
       
    if (!(strcmp(channel_binding, "biws") == 0 && state->cbind_flag == 'n') && !(strcmp(channel_binding, "eSws") == 0 && state->cbind_flag == 'y'))
    {
      ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), (errmsg("unexpected SCRAM channel-binding attribute in client-final-message"))));
    }
  }

  state->client_final_nonce = read_attr_value(&p, 'r');

                                  
  do
  {
    proof = p - 1;
    value = read_any_attr(&p, &attr);
  } while (attr != 'p');

  client_proof = palloc(pg_b64_dec_len(strlen(value)));
  if (pg_b64_decode(value, strlen(value), client_proof) != SCRAM_KEY_LEN)
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Malformed proof in client-final-message.")));
  }
  memcpy(state->ClientProof, client_proof, SCRAM_KEY_LEN);
  pfree(client_proof);

  if (*p != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_PROTOCOL_VIOLATION), errmsg("malformed SCRAM message"), errdetail("Garbage found at the end of client-final-message.")));
  }

  state->client_final_message_without_proof = palloc(proof - begin + 1);
  memcpy(state->client_final_message_without_proof, input, proof - begin);
  state->client_final_message_without_proof[proof - begin] = '\0';
}

   
                                                       
   
static char *
build_server_final_message(scram_state *state)
{
  uint8 ServerSignature[SCRAM_KEY_LEN];
  char *server_signature_base64;
  int siglen;
  scram_HMAC_ctx ctx;

                                 
  scram_HMAC_init(&ctx, state->ServerKey, SCRAM_KEY_LEN);
  scram_HMAC_update(&ctx, state->client_first_message_bare, strlen(state->client_first_message_bare));
  scram_HMAC_update(&ctx, ",", 1);
  scram_HMAC_update(&ctx, state->server_first_message, strlen(state->server_first_message));
  scram_HMAC_update(&ctx, ",", 1);
  scram_HMAC_update(&ctx, state->client_final_message_without_proof, strlen(state->client_final_message_without_proof));
  scram_HMAC_final(ServerSignature, &ctx);

  server_signature_base64 = palloc(pg_b64_enc_len(SCRAM_KEY_LEN) + 1);
  siglen = pg_b64_encode((const char *)ServerSignature, SCRAM_KEY_LEN, server_signature_base64);
  server_signature_base64[siglen] = '\0';

           
                                                            
     
                                
                                              
     
                                                      
                           
     
           
     
  return psprintf("v=%s", server_signature_base64);
}

   
                                                                           
                                                                         
                                                              
   
static char *
scram_mock_salt(const char *username)
{
  pg_sha256_ctx ctx;
  static uint8 sha_digest[PG_SHA256_DIGEST_LENGTH];
  char *mock_auth_nonce = GetMockAuthenticationNonce();

     
                                                                         
                                                                           
                                                                             
                                       
     
  StaticAssertStmt(PG_SHA256_DIGEST_LENGTH >= SCRAM_DEFAULT_SALT_LEN, "salt length greater than SHA256 digest length");

  pg_sha256_init(&ctx);
  pg_sha256_update(&ctx, (uint8 *)username, strlen(username));
  pg_sha256_update(&ctx, (uint8 *)mock_auth_nonce, MOCK_AUTH_NONCE_LEN);
  pg_sha256_final(&ctx, sha_digest);

  return (char *)sha_digest;
}
