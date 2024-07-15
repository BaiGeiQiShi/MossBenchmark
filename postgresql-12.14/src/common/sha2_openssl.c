                                                                            
   
                  
                                                                  
                                             
   
                                                                      
   
                                                                         
   
                  
                                
   
                                                                            
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <openssl/sha.h>

#include "common/sha2.h"

                                    
void
pg_sha256_init(pg_sha256_ctx *ctx)
{
  SHA256_Init((SHA256_CTX *)ctx);
}

void
pg_sha256_update(pg_sha256_ctx *ctx, const uint8 *data, size_t len)
{
  SHA256_Update((SHA256_CTX *)ctx, data, len);
}

void
pg_sha256_final(pg_sha256_ctx *ctx, uint8 *dest)
{
  SHA256_Final(dest, (SHA256_CTX *)ctx);
}

                                    
void
pg_sha512_init(pg_sha512_ctx *ctx)
{
  SHA512_Init((SHA512_CTX *)ctx);
}

void
pg_sha512_update(pg_sha512_ctx *ctx, const uint8 *data, size_t len)
{
  SHA512_Update((SHA512_CTX *)ctx, data, len);
}

void
pg_sha512_final(pg_sha512_ctx *ctx, uint8 *dest)
{
  SHA512_Final(dest, (SHA512_CTX *)ctx);
}

                                    
void
pg_sha384_init(pg_sha384_ctx *ctx)
{
  SHA384_Init((SHA512_CTX *)ctx);
}

void
pg_sha384_update(pg_sha384_ctx *ctx, const uint8 *data, size_t len)
{
  SHA384_Update((SHA512_CTX *)ctx, data, len);
}

void
pg_sha384_final(pg_sha384_ctx *ctx, uint8 *dest)
{
  SHA384_Final(dest, (SHA512_CTX *)ctx);
}

                                    
void
pg_sha224_init(pg_sha224_ctx *ctx)
{
  SHA224_Init((SHA256_CTX *)ctx);
}

void
pg_sha224_update(pg_sha224_ctx *ctx, const uint8 *data, size_t len)
{
  SHA224_Update((SHA256_CTX *)ctx, data, len);
}

void
pg_sha224_final(pg_sha224_ctx *ctx, uint8 *dest)
{
  SHA224_Final(dest, (SHA256_CTX *)ctx);
}
