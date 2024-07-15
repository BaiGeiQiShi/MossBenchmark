                                                                            
                  
                                                          
   
                                                                            
                                                                       
                                           
   
                                                                         
   
                  
                               
   
                                                                            
   
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/base64.h"
#include "common/scram-common.h"
#include "port/pg_bswap.h"

#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5C

   
                               
   
                                      
   
void
scram_HMAC_init(scram_HMAC_ctx *ctx, const uint8 *key, int keylen)
{
  uint8 k_ipad[SHA256_HMAC_B];
  int i;
  uint8 keybuf[SCRAM_KEY_LEN];

     
                                                                           
                                                
     
  if (keylen > SHA256_HMAC_B)
  {
    pg_sha256_ctx sha256_ctx;

    pg_sha256_init(&sha256_ctx);
    pg_sha256_update(&sha256_ctx, key, keylen);
    pg_sha256_final(&sha256_ctx, keybuf);
    key = keybuf;
    keylen = SCRAM_KEY_LEN;
  }

  memset(k_ipad, HMAC_IPAD, SHA256_HMAC_B);
  memset(ctx->k_opad, HMAC_OPAD, SHA256_HMAC_B);

  for (i = 0; i < keylen; i++)
  {
    k_ipad[i] ^= key[i];
    ctx->k_opad[i] ^= key[i];
  }

                                 
  pg_sha256_init(&ctx->sha256ctx);
  pg_sha256_update(&ctx->sha256ctx, k_ipad, SHA256_HMAC_B);
}

   
                           
                                      
   
void
scram_HMAC_update(scram_HMAC_ctx *ctx, const char *str, int slen)
{
  pg_sha256_update(&ctx->sha256ctx, (const uint8 *)str, slen);
}

   
                              
                                      
   
void
scram_HMAC_final(uint8 *result, scram_HMAC_ctx *ctx)
{
  uint8 h[SCRAM_KEY_LEN];

  pg_sha256_final(&ctx->sha256ctx, h);

                          
  pg_sha256_init(&ctx->sha256ctx);
  pg_sha256_update(&ctx->sha256ctx, ctx->k_opad, SHA256_HMAC_B);
  pg_sha256_update(&ctx->sha256ctx, h, SCRAM_KEY_LEN);
  pg_sha256_final(&ctx->sha256ctx, result);
}

   
                             
   
                                                          
   
void
scram_SaltedPassword(const char *password, const char *salt, int saltlen, int iterations, uint8 *result)
{
  int password_len = strlen(password);
  uint32 one = pg_hton32(1);
  int i, j;
  uint8 Ui[SCRAM_KEY_LEN];
  uint8 Ui_prev[SCRAM_KEY_LEN];
  scram_HMAC_ctx hmac_ctx;

     
                                                                       
                                                                      
               
     

                       
  scram_HMAC_init(&hmac_ctx, (uint8 *)password, password_len);
  scram_HMAC_update(&hmac_ctx, salt, saltlen);
  scram_HMAC_update(&hmac_ctx, (char *)&one, sizeof(uint32));
  scram_HMAC_final(Ui_prev, &hmac_ctx);
  memcpy(result, Ui_prev, SCRAM_KEY_LEN);

                             
  for (i = 2; i <= iterations; i++)
  {
    scram_HMAC_init(&hmac_ctx, (uint8 *)password, password_len);
    scram_HMAC_update(&hmac_ctx, (const char *)Ui_prev, SCRAM_KEY_LEN);
    scram_HMAC_final(Ui, &hmac_ctx);
    for (j = 0; j < SCRAM_KEY_LEN; j++)
    {
      result[j] ^= Ui[j];
    }
    memcpy(Ui_prev, Ui, SCRAM_KEY_LEN);
  }
}

   
                                                                                
                              
   
void
scram_H(const uint8 *input, int len, uint8 *result)
{
  pg_sha256_ctx ctx;

  pg_sha256_init(&ctx);
  pg_sha256_update(&ctx, input, len);
  pg_sha256_final(&ctx, result);
}

   
                        
   
void
scram_ClientKey(const uint8 *salted_password, uint8 *result)
{
  scram_HMAC_ctx ctx;

  scram_HMAC_init(&ctx, salted_password, SCRAM_KEY_LEN);
  scram_HMAC_update(&ctx, "Client Key", strlen("Client Key"));
  scram_HMAC_final(result, &ctx);
}

   
                        
   
void
scram_ServerKey(const uint8 *salted_password, uint8 *result)
{
  scram_HMAC_ctx ctx;

  scram_HMAC_init(&ctx, salted_password, SCRAM_KEY_LEN);
  scram_HMAC_update(&ctx, "Server Key", strlen("Server Key"));
  scram_HMAC_final(result, &ctx);
}

   
                                                                           
   
                                                                                
   
                                                                            
                                                                  
   
char *
scram_build_verifier(const char *salt, int saltlen, int iterations, const char *password)
{
  uint8 salted_password[SCRAM_KEY_LEN];
  uint8 stored_key[SCRAM_KEY_LEN];
  uint8 server_key[SCRAM_KEY_LEN];
  char *result;
  char *p;
  int maxlen;

  if (iterations <= 0)
  {
    iterations = SCRAM_DEFAULT_ITERATIONS;
  }

                                         
  scram_SaltedPassword(password, salt, saltlen, iterations, salted_password);
  scram_ClientKey(salted_password, stored_key);
  scram_H(stored_key, SCRAM_KEY_LEN, stored_key);

  scram_ServerKey(salted_password, server_key);

               
                    
                                                                    
               
     
  maxlen = strlen("SCRAM-SHA-256") + 1 + 10 + 1                      
           + pg_b64_enc_len(saltlen) + 1                                 
           + pg_b64_enc_len(SCRAM_KEY_LEN) + 1                                
           + pg_b64_enc_len(SCRAM_KEY_LEN) + 1;                               

#ifdef FRONTEND
  result = malloc(maxlen);
  if (!result)
  {
    return NULL;
  }
#else
  result = palloc(maxlen);
#endif

  p = result + sprintf(result, "SCRAM-SHA-256$%d:", iterations);

  p += pg_b64_encode(salt, saltlen, p);
  *(p++) = '$';
  p += pg_b64_encode((char *)stored_key, SCRAM_KEY_LEN, p);
  *(p++) = ':';
  p += pg_b64_encode((char *)server_key, SCRAM_KEY_LEN, p);
  *(p++) = '\0';

  Assert(p - result <= maxlen);

  return result;
}
