                      
                                                           

   
                            
                                                                      
   
                                                                 
                 
   
                                                                      
                                                                      
            
                                                                     
                                                                   
                                                                        
                                                                         
                                                                          
                                                                           
                                                                           
                                                
   
                                                                           
                                                                         
                                                                              
                                                                            
                                                                              
                                                                           
                                                                         
                                                                              
                                                                             
                                                                          
                
   

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)crypt.c	8.1.1.1 (Berkeley) 8/18/93";
#else
__RCSID("$NetBSD: crypt.c,v 1.18 2001/03/01 14:37:35 wiz Exp $");
#endif
#endif               

#include "c.h"

#include <limits.h>

#ifndef WIN32
#include <unistd.h>
#endif

static int
des_setkey(const char *key);
static int
des_cipher(const char *in, char *out, long salt, int num_iter);

   
                                       
                                     
                                                            
   
               
                                                                         
                                                 
   
                                                                    
                                                               
   
                                                                  
                                                 
   

                                               

   
                                                          
                                                           
                                                                  
   
                        

#ifdef CHAR_BITS
#if CHAR_BITS != 8
#error C_block structure assumes 8 bit characters
#endif
#endif

   
                                                            
                                                                     
   
                       

   
                                                                              
                                                                            
                             
   
                       

                                                 
#ifndef STATIC
#define STATIC static void
#endif

   
                                                                        
            
   
typedef int int32_t;

                                          

#define _PASSWORD_EFMT1 '_'                                 

   
                                              
   
                                                                  
                                                                               
                                                                               
                                                                               
                                                                            
                                                                              
                                                                              
                                                                               
                                                                               
               
   
                                                                              
                                                                            
                                                                     
                                                                         
                                                                           
                                                                         
                                                                          
                                                                           
                                                                          
                                                                             
                                                                           
                                                                           
                                                                              
                                                                            
                                                                              
                                                                         
                                                                            
                      
   
                                                                 
                                                                         
                                                                          
                                                                       
                                                                          
                                     
   
                                                                           
                                                                          
                                                                          
                                                                            
                                                                             
                                                                            
                                                                          
                                                                            
                                                                         
                                                                             
                                                                            
                                                                     
   
                                              
   
                                                                           
                                                                             
                                                                           
                                                                           
                                                                              
                                                                              
                                                                            
                                                                          
                                                                             
                                            
   
                                 
                                                                    
                                                                     
                                                                        
                                                                    
                                                                      
                    
                                                                    
                                                                 
                                                                   
                                                                 
                                                               
                                                               
                                                             
                                                               
                                                            
                
                                                                   
                                                                
                                                                   
                                                                    
                                                                
                                                                   
                                                                 
                                                                     
                                                               
                                                                   
                                                                   
                                                                   
                                                               
                                           
                                                         
                                                 
   
                                       
   
                                                                          
                                                                            
                                                                           
                                                                           
                                  
   
                                                                           
                                                                         
                                                                            
                           
   

typedef union
{
  unsigned char b[8];
  struct
  {
    int32_t i0;
    int32_t i1;
  } b32;
#if defined(B64)
  B64 b64;
#endif
} C_block;

   
                                              
                                                                       
   
#define TO_SIX_BIT(rslt, src)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    C_block cvt;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    cvt.b[0] = src;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    src >>= 6;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    cvt.b[1] = src;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    src >>= 6;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    cvt.b[2] = src;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    src >>= 6;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    cvt.b[3] = src;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    rslt = (cvt.b32.i0 & 0x3f3f3f3fL) << 2;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  }

   
                                                                     
   
#define ZERO(d, d0, d1) d0 = 0, d1 = 0
#define LOAD(d, d0, d1, bl) d0 = (bl).b32.i0, d1 = (bl).b32.i1
#define LOADREG(d, d0, d1, s, s0, s1) d0 = s0, d1 = s1
#define OR(d, d0, d1, bl) d0 |= (bl).b32.i0, d1 |= (bl).b32.i1
#define STORE(s, s0, s1, bl) (bl).b32.i0 = s0, (bl).b32.i1 = s1
#define DCL_BLOCK(d, d0, d1) int32_t d0, d1

#if defined(LARGEDATA)
                                                             
#define LGCHUNKBITS 3
#define CHUNKBITS (1 << LGCHUNKBITS)
#define PERM6464(d, d0, d1, cpp, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  LOAD(d, d0, d1, (p)[(0 << CHUNKBITS) + (cpp)[0]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  OR(d, d0, d1, (p)[(1 << CHUNKBITS) + (cpp)[1]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(2 << CHUNKBITS) + (cpp)[2]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(3 << CHUNKBITS) + (cpp)[3]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(4 << CHUNKBITS) + (cpp)[4]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(5 << CHUNKBITS) + (cpp)[5]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(6 << CHUNKBITS) + (cpp)[6]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(7 << CHUNKBITS) + (cpp)[7]]);
#define PERM3264(d, d0, d1, cpp, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  LOAD(d, d0, d1, (p)[(0 << CHUNKBITS) + (cpp)[0]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  OR(d, d0, d1, (p)[(1 << CHUNKBITS) + (cpp)[1]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(2 << CHUNKBITS) + (cpp)[2]]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
  OR(d, d0, d1, (p)[(3 << CHUNKBITS) + (cpp)[3]]);
#else
                  
#define LGCHUNKBITS 2
#define CHUNKBITS (1 << LGCHUNKBITS)
#define PERM6464(d, d0, d1, cpp, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    C_block tblk;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    permute(cpp, &tblk, p, 8);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    LOAD(d, d0, d1, tblk);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  }
#define PERM3264(d, d0, d1, cpp, p)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    C_block tblk;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    permute(cpp, &tblk, p, 4);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    LOAD(d, d0, d1, tblk);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  }
#endif                

STATIC
init_des(void);
STATIC init_perm(C_block[64 / CHUNKBITS][1 << CHUNKBITS], unsigned char[64], int, int);

#ifndef LARGEDATA
STATIC
permute(unsigned char *, C_block *, C_block *, int);
#endif
#ifdef DEBUG
STATIC
prtab(char *, unsigned char *, int);
#endif

#ifndef LARGEDATA
STATIC
permute(unsigned char *cp, C_block *out, C_block *p, int chars_in)
{
  DCL_BLOCK(D, D0, D1);
  C_block *tp;
  int t;

  ZERO(D, D0, D1);
  do
  {
    t = *cp++;
    tp = &p[t & 0xf];
    OR(D, D0, D1, *tp);
    p += (1 << CHUNKBITS);
    tp = &p[t >> 4];
    OR(D, D0, D1, *tp);
    p += (1 << CHUNKBITS);
  } while (--chars_in > 0);
  STORE(D, D0, D1, *out);
}
#endif                

                                                              

static const unsigned char IP[] = {
                             
    58,
    50,
    42,
    34,
    26,
    18,
    10,
    2,
    60,
    52,
    44,
    36,
    28,
    20,
    12,
    4,
    62,
    54,
    46,
    38,
    30,
    22,
    14,
    6,
    64,
    56,
    48,
    40,
    32,
    24,
    16,
    8,
    57,
    49,
    41,
    33,
    25,
    17,
    9,
    1,
    59,
    51,
    43,
    35,
    27,
    19,
    11,
    3,
    61,
    53,
    45,
    37,
    29,
    21,
    13,
    5,
    63,
    55,
    47,
    39,
    31,
    23,
    15,
    7,
};

                                                                        

static const unsigned char ExpandTr[] = {
                             
    32,
    1,
    2,
    3,
    4,
    5,
    4,
    5,
    6,
    7,
    8,
    9,
    8,
    9,
    10,
    11,
    12,
    13,
    12,
    13,
    14,
    15,
    16,
    17,
    16,
    17,
    18,
    19,
    20,
    21,
    20,
    21,
    22,
    23,
    24,
    25,
    24,
    25,
    26,
    27,
    28,
    29,
    28,
    29,
    30,
    31,
    32,
    1,
};

static const unsigned char PC1[] = {
                                 
    57,
    49,
    41,
    33,
    25,
    17,
    9,
    1,
    58,
    50,
    42,
    34,
    26,
    18,
    10,
    2,
    59,
    51,
    43,
    35,
    27,
    19,
    11,
    3,
    60,
    52,
    44,
    36,

    63,
    55,
    47,
    39,
    31,
    23,
    15,
    7,
    62,
    54,
    46,
    38,
    30,
    22,
    14,
    6,
    61,
    53,
    45,
    37,
    29,
    21,
    13,
    5,
    28,
    20,
    12,
    4,
};

static const unsigned char Rotates[] = {
                               
    1,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
    2,
    2,
    2,
    2,
    2,
    2,
    1,
};

                                                                              
static const unsigned char PC2[] = {
                                 
    9,
    18,
    14,
    17,
    11,
    24,
    1,
    5,
    22,
    25,
    3,
    28,
    15,
    6,
    21,
    10,
    35,
    38,
    23,
    19,
    12,
    4,
    26,
    8,
    43,
    54,
    16,
    7,
    27,
    20,
    13,
    2,

    0,
    0,
    41,
    52,
    31,
    37,
    47,
    55,
    0,
    0,
    30,
    40,
    51,
    45,
    33,
    48,
    0,
    0,
    44,
    49,
    39,
    56,
    34,
    53,
    0,
    0,
    46,
    42,
    50,
    36,
    29,
    32,
};

static const unsigned char S[8][64] = {                                    
                
    {14, 4, 13, 1, 2, 15, 11, 8, 3, 10, 6, 12, 5, 9, 0, 7, 0, 15, 7, 4, 14, 2, 13, 1, 10, 6, 12, 11, 9, 5, 3, 8, 4, 1, 14, 8, 13, 6, 2, 11, 15, 12, 9, 7, 3, 10, 5, 0, 15, 12, 8, 2, 4, 9, 1, 7, 5, 11, 3, 14, 10, 0, 6, 13},
                
    {15, 1, 8, 14, 6, 11, 3, 4, 9, 7, 2, 13, 12, 0, 5, 10, 3, 13, 4, 7, 15, 2, 8, 14, 12, 0, 1, 10, 6, 9, 11, 5, 0, 14, 7, 11, 10, 4, 13, 1, 5, 8, 12, 6, 9, 3, 2, 15, 13, 8, 10, 1, 3, 15, 4, 2, 11, 6, 7, 12, 0, 5, 14, 9},
                
    {10, 0, 9, 14, 6, 3, 15, 5, 1, 13, 12, 7, 11, 4, 2, 8, 13, 7, 0, 9, 3, 4, 6, 10, 2, 8, 5, 14, 12, 11, 15, 1, 13, 6, 4, 9, 8, 15, 3, 0, 11, 1, 2, 12, 5, 10, 14, 7, 1, 10, 13, 0, 6, 9, 8, 7, 4, 15, 14, 3, 11, 5, 2, 12},
                
    {7, 13, 14, 3, 0, 6, 9, 10, 1, 2, 8, 5, 11, 12, 4, 15, 13, 8, 11, 5, 6, 15, 0, 3, 4, 7, 2, 12, 1, 10, 14, 9, 10, 6, 9, 0, 12, 11, 7, 13, 15, 1, 3, 14, 5, 2, 8, 4, 3, 15, 0, 6, 10, 1, 13, 8, 9, 4, 5, 11, 12, 7, 2, 14},
                
    {2, 12, 4, 1, 7, 10, 11, 6, 8, 5, 3, 15, 13, 0, 14, 9, 14, 11, 2, 12, 4, 7, 13, 1, 5, 0, 15, 10, 3, 9, 8, 6, 4, 2, 1, 11, 10, 13, 7, 8, 15, 9, 12, 5, 6, 3, 0, 14, 11, 8, 12, 7, 1, 14, 2, 13, 6, 15, 0, 9, 10, 4, 5, 3},
                
    {12, 1, 10, 15, 9, 2, 6, 8, 0, 13, 3, 4, 14, 7, 5, 11, 10, 15, 4, 2, 7, 12, 9, 5, 6, 1, 13, 14, 0, 11, 3, 8, 9, 14, 15, 5, 2, 8, 12, 3, 7, 0, 4, 10, 1, 13, 11, 6, 4, 3, 2, 12, 9, 5, 15, 10, 11, 14, 1, 7, 6, 0, 8, 13},
                
    {4, 11, 2, 14, 15, 0, 8, 13, 3, 12, 9, 7, 5, 10, 6, 1, 13, 0, 11, 7, 4, 9, 1, 10, 14, 3, 5, 12, 2, 15, 8, 6, 1, 4, 11, 13, 12, 3, 7, 14, 10, 15, 6, 8, 0, 5, 9, 2, 6, 11, 13, 8, 1, 4, 10, 7, 9, 5, 0, 15, 14, 2, 3, 12},
                
    {13, 2, 8, 4, 6, 15, 11, 1, 10, 9, 3, 14, 5, 0, 12, 7, 1, 15, 13, 8, 10, 3, 7, 4, 12, 5, 6, 11, 0, 14, 9, 2, 7, 11, 4, 1, 9, 12, 14, 2, 0, 6, 10, 13, 15, 3, 5, 8, 2, 1, 14, 7, 4, 10, 8, 13, 15, 12, 9, 0, 3, 5, 6, 11}};

static const unsigned char P32Tr[] = {
                                     
    16,
    7,
    20,
    21,
    29,
    12,
    28,
    17,
    1,
    15,
    23,
    26,
    5,
    18,
    31,
    10,
    2,
    8,
    24,
    14,
    32,
    27,
    3,
    9,
    19,
    13,
    30,
    6,
    22,
    11,
    4,
    25,
};

static const unsigned char CIFP[] = {
                                            
    1,
    2,
    3,
    4,
    17,
    18,
    19,
    20,
    5,
    6,
    7,
    8,
    21,
    22,
    23,
    24,
    9,
    10,
    11,
    12,
    25,
    26,
    27,
    28,
    13,
    14,
    15,
    16,
    29,
    30,
    31,
    32,

    33,
    34,
    35,
    36,
    49,
    50,
    51,
    52,
    37,
    38,
    39,
    40,
    53,
    54,
    55,
    56,
    41,
    42,
    43,
    44,
    57,
    58,
    59,
    60,
    45,
    46,
    47,
    48,
    61,
    62,
    63,
    64,
};

static const unsigned char itoa64[] =                        
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

                                                                          

static unsigned char a64toi[128];                        

                                      
static C_block PC1ROT[64 / CHUNKBITS][1 << CHUNKBITS];

                                                   
static C_block PC2ROT[2][64 / CHUNKBITS][1 << CHUNKBITS];

                                         
static C_block IE3264[32 / CHUNKBITS][1 << CHUNKBITS];

                                                      
static int32_t SPE[2][8][64];

                                                       
static C_block CF6464[64 / CHUNKBITS][1 << CHUNKBITS];

                                          

static C_block constdatablock;                                        
static char cryptresult[1 + 4 + 4 + 11 + 1];                       

extern char *
__md5crypt(const char *, const char *);          
extern char *
__bcrypt(const char *, const char *);          

   
                                                               
                                                                  
   
char *
crypt(const char *key, const char *setting)
{
  char *encp;
  int32_t i;
  int t;
  int32_t salt;
  int num_iter, salt_size;
  C_block keyblock, rsltblock;

#if 0
	                                              
	if (setting[0] == _PASSWORD_NONDES)
	{
		switch (setting[1])
		{
			case '2':
				return (__bcrypt(key, setting));
			case '1':
			default:
				return (__md5crypt(key, setting));
		}
	}
#endif

  for (i = 0; i < 8; i++)
  {
    if ((t = 2 * (unsigned char)(*key)) != 0)
    {
      key++;
    }
    keyblock.b[i] = t;
  }
  if (des_setkey((char *)keyblock.b))                                
  {
    return (NULL);
  }

  encp = &cryptresult[0];
  switch (*setting)
  {
  case _PASSWORD_EFMT1:

       
                                                                
       
    while (*key)
    {
      if (des_cipher((char *)(void *)&keyblock, (char *)(void *)&keyblock, 0L, 1))
      {
        return (NULL);
      }
      for (i = 0; i < 8; i++)
      {
        if ((t = 2 * (unsigned char)(*key)) != 0)
        {
          key++;
        }
        keyblock.b[i] ^= t;
      }
      if (des_setkey((char *)keyblock.b))
      {
        return (NULL);
      }
    }

    *encp++ = *setting++;

                             
    num_iter = 0;
    for (i = 4; --i >= 0;)
    {
      if ((t = (unsigned char)setting[i]) == '\0')
      {
        t = '.';
      }
      encp[i] = t;
      num_iter = (num_iter << 6) | a64toi[t];
    }
    setting += 4;
    encp += 4;
    salt_size = 4;
    break;
  default:
    num_iter = 25;
    salt_size = 2;
  }

  salt = 0;
  for (i = salt_size; --i >= 0;)
  {
    if ((t = (unsigned char)setting[i]) == '\0')
    {
      t = '.';
    }
    encp[i] = t;
    salt = (salt << 6) | a64toi[t];
  }
  encp += salt_size;
  if (des_cipher((char *)(void *)&constdatablock, (char *)(void *)&rsltblock, salt, num_iter))
  {
    return (NULL);
  }

     
                                                       
     
  i = ((int32_t)((rsltblock.b[0] << 8) | rsltblock.b[1]) << 8) | rsltblock.b[2];
  encp[3] = itoa64[i & 0x3f];
  i >>= 6;
  encp[2] = itoa64[i & 0x3f];
  i >>= 6;
  encp[1] = itoa64[i & 0x3f];
  i >>= 6;
  encp[0] = itoa64[i];
  encp += 4;
  i = ((int32_t)((rsltblock.b[3] << 8) | rsltblock.b[4]) << 8) | rsltblock.b[5];
  encp[3] = itoa64[i & 0x3f];
  i >>= 6;
  encp[2] = itoa64[i & 0x3f];
  i >>= 6;
  encp[1] = itoa64[i & 0x3f];
  i >>= 6;
  encp[0] = itoa64[i];
  encp += 4;
  i = ((int32_t)((rsltblock.b[6]) << 8) | rsltblock.b[7]) << 2;
  encp[2] = itoa64[i & 0x3f];
  i >>= 6;
  encp[1] = itoa64[i & 0x3f];
  i >>= 6;
  encp[0] = itoa64[i];

  encp[3] = 0;

  return (cryptresult);
}

   
                                                            
   
#define KS_SIZE 16
static C_block KS[KS_SIZE];

static volatile int des_ready = 0;

   
                                         
   
static int
des_setkey(const char *key)
{
  DCL_BLOCK(K, K0, K1);
  C_block *ptabp;
  int i;

  if (!des_ready)
  {
    init_des();
  }

  PERM6464(K, K0, K1, (unsigned char *)key, (C_block *)PC1ROT);
  key = (char *)&KS[0];
  STORE(K & ~0x03030303L, K0 & ~0x03030303L, K1, *(C_block *)key);
  for (i = 1; i < 16; i++)
  {
    key += sizeof(C_block);
    STORE(K, K0, K1, *(C_block *)key);
    ptabp = (C_block *)PC2ROT[Rotates[i] - 1];
    PERM6464(K, K0, K1, (unsigned char *)key, ptabp);
    STORE(K & ~0x03030303L, K0 & ~0x03030303L, K1, *(C_block *)key);
  }
  return (0);
}

   
                                                                               
                                                                           
                                                                                
   
                                                                         
                                      
   
static int
des_cipher(const char *in, char *out, long salt, int num_iter)
{
                                                                 
#if defined(pdp11)
  int j;
#endif
  int32_t L0, L1, R0, R1, k;
  C_block *kp;
  int ks_inc, loop_count;
  C_block B;

  L0 = salt;
  TO_SIX_BIT(salt, L0);                                

#if defined(__vax__) || defined(pdp11)
  salt = ~salt;                                       
#define SALT (~salt)
#else
#define SALT salt
#endif

#if defined(MUST_ALIGN)
  B.b[0] = in[0];
  B.b[1] = in[1];
  B.b[2] = in[2];
  B.b[3] = in[3];
  B.b[4] = in[4];
  B.b[5] = in[5];
  B.b[6] = in[6];
  B.b[7] = in[7];
  LOAD(L, L0, L1, B);
#else
  LOAD(L, L0, L1, *(C_block *)in);
#endif
  LOADREG(R, R0, R1, L, L0, L1);
  L0 &= 0x55555555L;
  L1 &= 0x55555555L;
  L0 = (L0 << 1) | L1;                                         
  R0 &= 0xaaaaaaaaL;
  R1 = (R1 >> 1) & 0x55555555L;
  L1 = R0 | R1;                                        
  STORE(L, L0, L1, B);
  PERM3264(L, L0, L1, B.b, (C_block *)IE3264);                    
  PERM3264(R, R0, R1, B.b + 4, (C_block *)IE3264);               

  if (num_iter >= 0)
  {                 
    kp = &KS[0];
    ks_inc = sizeof(*kp);
  }
  else
  {                 
    num_iter = -num_iter;
    kp = &KS[KS_SIZE - 1];
    ks_inc = -(long)sizeof(*kp);
  }

  while (--num_iter >= 0)
  {
    loop_count = 8;
    do
    {

#define SPTAB(t, i) (*(int32_t *)((unsigned char *)(t) + (i) * (sizeof(int32_t) / 4)))
#if defined(gould)
                                                         
#define DOXOR(x, y, i)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  x ^= SPTAB(SPE[0][i], B.b[i]);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  y ^= SPTAB(SPE[1][i], B.b[i]);
#else
#if defined(pdp11)
                                                        
#define DOXOR(x, y, i)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  j = B.b[i];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  x ^= SPTAB(SPE[0][i], j);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  y ^= SPTAB(SPE[1][i], j);
#else
                                                          
#define DOXOR(x, y, i)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  k = B.b[i];                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  x ^= SPTAB(SPE[0][i], k);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  y ^= SPTAB(SPE[1][i], k);
#endif
#endif

#define CRUNCH(p0, p1, q0, q1)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  k = ((q0) ^ (q1)) & SALT;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  B.b32.i0 = k ^ (q0) ^ kp->b32.i0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  B.b32.i1 = k ^ (q1) ^ kp->b32.i1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  kp = (C_block *)((char *)kp + ks_inc);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  DOXOR(p0, p1, 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 1);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 2);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 3);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 4);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 5);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 6);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  DOXOR(p0, p1, 7);

      CRUNCH(L0, L1, R0, R1);
      CRUNCH(R0, R1, L0, L1);
    } while (--loop_count != 0);
    kp = (C_block *)((char *)kp - (ks_inc * KS_SIZE));

                      
    L0 ^= R0;
    L1 ^= R1;
    R0 ^= L0;
    R1 ^= L1;
    L0 ^= R0;
    L1 ^= R1;
  }

                                                 
  L0 = ((L0 >> 3) & 0x0f0f0f0fL) | ((L1 << 1) & 0xf0f0f0f0L);
  L1 = ((R0 >> 3) & 0x0f0f0f0fL) | ((R1 << 1) & 0xf0f0f0f0L);
  STORE(L, L0, L1, B);
  PERM6464(L, L0, L1, B.b, (C_block *)CF6464);
#if defined(MUST_ALIGN)
  STORE(L, L0, L1, B);
  out[0] = B.b[0];
  out[1] = B.b[1];
  out[2] = B.b[2];
  out[3] = B.b[3];
  out[4] = B.b[4];
  out[5] = B.b[5];
  out[6] = B.b[6];
  out[7] = B.b[7];
#else
  STORE(L, L0, L1, *(C_block *)out);
#endif
  return (0);
}

   
                                                                              
                                                                             
   
STATIC
init_des(void)
{
  int i, j;
  int32_t k;
  int tableno;
  static unsigned char perm[64], tmp32[32];                         

                                                     

     
                                                              
     
  for (i = 0; i < 64; i++)
  {
    a64toi[itoa64[i]] = i;
  }

     
                                                            
     
  for (i = 0; i < 64; i++)
  {
    perm[i] = 0;
  }
  for (i = 0; i < 64; i++)
  {
    if ((k = PC2[i]) == 0)
    {
      continue;
    }
    k += Rotates[0] - 1;
    if ((k % 28) < Rotates[0])
    {
      k -= 28;
    }
    k = PC1[k];
    if (k > 0)
    {
      k--;
      k = (k | 07) - (k & 07);
      k++;
    }
    perm[i] = k;
  }
#ifdef DEBUG
  prtab("pc1tab", perm, 8);
#endif
  init_perm(PC1ROT, perm, 8, 8);

     
                                                                  
     
  for (j = 0; j < 2; j++)
  {
    unsigned char pc2inv[64];

    for (i = 0; i < 64; i++)
    {
      perm[i] = pc2inv[i] = 0;
    }
    for (i = 0; i < 64; i++)
    {
      if ((k = PC2[i]) == 0)
      {
        continue;
      }
      pc2inv[k - 1] = i + 1;
    }
    for (i = 0; i < 64; i++)
    {
      if ((k = PC2[i]) == 0)
      {
        continue;
      }
      k += j;
      if ((k % 28) <= j)
      {
        k -= 28;
      }
      perm[i] = pc2inv[k];
    }
#ifdef DEBUG
    prtab("pc2tab", perm, 8);
#endif
    init_perm(PC2ROT[j], perm, 8, 8);
  }

     
                                                            
     
  for (i = 0; i < 8; i++)
  {
    for (j = 0; j < 8; j++)
    {
      k = (j < 2) ? 0 : IP[ExpandTr[i * 6 + j - 2] - 1];
      if (k > 32)
      {
        k -= 32;
      }
      else if (k > 0)
      {
        k--;
      }
      if (k > 0)
      {
        k--;
        k = (k | 07) - (k & 07);
        k++;
      }
      perm[i * 8 + j] = k;
    }
  }
#ifdef DEBUG
  prtab("ietab", perm, 8);
#endif
  init_perm(IE3264, perm, 4, 8);

     
                                                            
     
  for (i = 0; i < 64; i++)
  {
    k = IP[CIFP[i] - 1];
    if (k > 0)
    {
      k--;
      k = (k | 07) - (k & 07);
      k++;
    }
    perm[k - 1] = i + 1;
  }
#ifdef DEBUG
  prtab("cftab", perm, 8);
#endif
  init_perm(CF6464, perm, 8, 8);

     
               
     
  for (i = 0; i < 48; i++)
  {
    perm[i] = P32Tr[ExpandTr[i] - 1];
  }
  for (tableno = 0; tableno < 8; tableno++)
  {
    for (j = 0; j < 64; j++)
    {
      k = (((j >> 0) & 01) << 5) | (((j >> 1) & 01) << 3) | (((j >> 2) & 01) << 2) | (((j >> 3) & 01) << 1) | (((j >> 4) & 01) << 0) | (((j >> 5) & 01) << 4);
      k = S[tableno][k];
      k = (((k >> 3) & 01) << 0) | (((k >> 2) & 01) << 1) | (((k >> 1) & 01) << 2) | (((k >> 0) & 01) << 3);
      for (i = 0; i < 32; i++)
      {
        tmp32[i] = 0;
      }
      for (i = 0; i < 4; i++)
      {
        tmp32[4 * tableno + i] = (k >> i) & 01;
      }
      k = 0;
      for (i = 24; --i >= 0;)
      {
        k = (k << 1) | tmp32[perm[i] - 1];
      }
      TO_SIX_BIT(SPE[0][tableno][j], k);
      k = 0;
      for (i = 24; --i >= 0;)
      {
        k = (k << 1) | tmp32[perm[i + 24] - 1];
      }
      TO_SIX_BIT(SPE[1][tableno][j], k);
    }
  }

  des_ready = 1;
}

   
                                                                       
                                                                        
                                                                           
                
   
                                                       
   
STATIC
init_perm(C_block perm[64 / CHUNKBITS][1 << CHUNKBITS], unsigned char p[64], int chars_in, int chars_out)
{
  int i, j, k, l;

  for (k = 0; k < chars_out * 8; k++)
  {                                             
    l = p[k] - 1;                                
    if (l < 0)
    {
      continue;                             
    }
    i = l >> LGCHUNKBITS;                                                
    l = 1 << (l & (CHUNKBITS - 1));                        
    for (j = 0; j < (1 << CHUNKBITS); j++)
    {                       
      if ((j & l) != 0)
      {
        perm[i][j].b[k >> 3] |= 1 << (k & 07);
      }
    }
  }
}

   
                                                  
   
#ifdef NOT_USED
int
setkey(const char *key)
{
  int i, j, k;
  C_block keyblock;

  for (i = 0; i < 8; i++)
  {
    k = 0;
    for (j = 0; j < 8; j++)
    {
      k <<= 1;
      k |= (unsigned char)*key++;
    }
    keyblock.b[i] = k;
  }
  return (des_setkey((char *)keyblock.b));
}

   
                                                   
   
static int
encrypt(char *block, int flag)
{
  int i, j, k;
  C_block cblock;

  for (i = 0; i < 8; i++)
  {
    k = 0;
    for (j = 0; j < 8; j++)
    {
      k <<= 1;
      k |= (unsigned char)*block++;
    }
    cblock.b[i] = k;
  }
  if (des_cipher((char *)&cblock, (char *)&cblock, 0L, (flag ? -1 : 1)))
  {
    return (1);
  }
  for (i = 7; i >= 0; i--)
  {
    k = cblock.b[i];
    for (j = 7; j >= 0; j--)
    {
      *--block = k & 01;
      k >>= 1;
    }
  }
  return (0);
}
#endif

#ifdef DEBUG
STATIC
prtab(char *s, unsigned char *t, int num_rows)
{
  int i, j;

  (void)printf("%s:\n", s);
  for (i = 0; i < num_rows; i++)
  {
    for (j = 0; j < 8; j++)
    {
      (void)printf("%3d", t[i * 8 + j]);
    }
    (void)printf("\n");
  }
  (void)printf("\n");
}

#endif
