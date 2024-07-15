   
                                                                  
                         
                                                                      
                                                                        
                    
   
                          
   
                                                                
   

                                               
#include "postgres_fe.h"

#include "mb/pg_wchar.h"

typedef struct
{
  unsigned short code, peer;
} codes_t;

                                                
static const codes_t big5Level1ToCnsPlane1[25] = {           
    {0xA140, 0x2121}, {0xA1F6, 0x2258}, {0xA1F7, 0x2257}, {0xA1F8, 0x2259}, {0xA2AF, 0x2421}, {0xA3C0, 0x4221}, {0xa3e1, 0x0000}, {0xA440, 0x4421}, {0xACFE, 0x5753}, {0xacff, 0x0000}, {0xAD40, 0x5323}, {0xAFD0, 0x5754}, {0xBBC8, 0x6B51}, {0xBE52, 0x6B50}, {0xBE53, 0x6F5C}, {0xC1AB, 0x7536}, {0xC2CB, 0x7535}, {0xC2CC, 0x7737}, {0xC361, 0x782E}, {0xC3B9, 0x7865}, {0xC3BA, 0x7864}, {0xC3BB, 0x7866}, {0xC456, 0x782D}, {0xC457, 0x7962}, {0xc67f, 0x0000}};

                                                
static const codes_t cnsPlane1ToBig5Level1[26] = {           
    {0x2121, 0xA140}, {0x2257, 0xA1F7}, {0x2258, 0xA1F6}, {0x2259, 0xA1F8}, {0x234f, 0x0000}, {0x2421, 0xA2AF}, {0x2571, 0x0000}, {0x4221, 0xA3C0}, {0x4242, 0x0000}, {0x4421, 0xA440}, {0x5323, 0xAD40}, {0x5753, 0xACFE}, {0x5754, 0xAFD0}, {0x6B50, 0xBE52}, {0x6B51, 0xBBC8}, {0x6F5C, 0xBE53}, {0x7535, 0xC2CB}, {0x7536, 0xC1AB}, {0x7737, 0xC2CC}, {0x782D, 0xC456}, {0x782E, 0xC361}, {0x7864, 0xC3BA}, {0x7865, 0xC3B9}, {0x7866, 0xC3BB}, {0x7962, 0xC457}, {0x7d4c, 0x0000}};

                                                
static const codes_t big5Level2ToCnsPlane2[48] = {           
    {0xC940, 0x2121}, {0xc94a, 0x0000}, {0xC94B, 0x212B}, {0xC96C, 0x214D}, {0xC9BE, 0x214C}, {0xC9BF, 0x217D}, {0xC9ED, 0x224E}, {0xCAF7, 0x224D}, {0xCAF8, 0x2439}, {0xD77A, 0x3F6A}, {0xD77B, 0x387E}, {0xDBA7, 0x3F6B}, {0xDDFC, 0x4176}, {0xDDFD, 0x4424}, {0xE8A3, 0x554C}, {0xE976, 0x5723}, {0xEB5B, 0x5A29}, {0xEBF1, 0x554B}, {0xEBF2, 0x5B3F}, {0xECDE, 0x5722}, {0xECDF, 0x5C6A}, {0xEDAA, 0x5D75}, {0xEEEB, 0x642F}, {0xEEEC, 0x6039}, {0xF056, 0x5D74}, {0xF057, 0x6243}, {0xF0CB, 0x5A28}, {0xF0CC, 0x6337}, {0xF163, 0x6430}, {0xF16B, 0x6761}, {0xF16C, 0x6438}, {0xF268, 0x6934}, {0xF269, 0x6573}, {0xF2C3, 0x664E}, {0xF375, 0x6762}, {0xF466, 0x6935}, {0xF4B5, 0x664D}, {0xF4B6, 0x6962}, {0xF4FD, 0x6A4C}, {0xF663, 0x6A4B}, {0xF664, 0x6C52}, {0xF977, 0x7167}, {0xF9C4, 0x7166}, {0xF9C5, 0x7234}, {0xF9C6, 0x7240}, {0xF9C7, 0x7235}, {0xF9D2, 0x7241}, {0xf9d6, 0x0000}};

                                                
static const codes_t cnsPlane2ToBig5Level2[49] = {           
    {0x2121, 0xC940}, {0x212B, 0xC94B}, {0x214C, 0xC9BE}, {0x214D, 0xC96C}, {0x217D, 0xC9BF}, {0x224D, 0xCAF7}, {0x224E, 0xC9ED}, {0x2439, 0xCAF8}, {0x387E, 0xD77B}, {0x3F6A, 0xD77A}, {0x3F6B, 0xDBA7}, {0x4424, 0x0000}, {0x4176, 0xDDFC}, {0x4177, 0x0000}, {0x4424, 0xDDFD}, {0x554B, 0xEBF1}, {0x554C, 0xE8A3}, {0x5722, 0xECDE}, {0x5723, 0xE976}, {0x5A28, 0xF0CB}, {0x5A29, 0xEB5B}, {0x5B3F, 0xEBF2}, {0x5C6A, 0xECDF}, {0x5D74, 0xF056}, {0x5D75, 0xEDAA}, {0x6039, 0xEEEC}, {0x6243, 0xF057}, {0x6337, 0xF0CC}, {0x642F, 0xEEEB}, {0x6430, 0xF163}, {0x6438, 0xF16C}, {0x6573, 0xF269}, {0x664D, 0xF4B5}, {0x664E, 0xF2C3}, {0x6761, 0xF16B}, {0x6762, 0xF375}, {0x6934, 0xF268}, {0x6935, 0xF466}, {0x6962, 0xF4B6}, {0x6A4B, 0xF663}, {0x6A4C, 0xF4FD}, {0x6C52, 0xF664}, {0x7166, 0xF9C4}, {0x7167, 0xF977}, {0x7234, 0xF9C5}, {0x7235, 0xF9C7}, {0x7240, 0xF9C6}, {0x7241, 0xF9D2}, {0x7245, 0x0000}};

                                                               
static const unsigned short b1c4[][2] = {{0xC879, 0x2123}, {0xC87B, 0x2124}, {0xC87D, 0x212A}, {0xC8A2, 0x2152}};

                                                               
static const unsigned short b2c3[][2] = {{0xF9D6, 0x4337}, {0xF9D7, 0x4F50}, {0xF9D8, 0x444E}, {0xF9D9, 0x504A}, {0xF9DA, 0x2C5D}, {0xF9DB, 0x3D7E}, {0xF9DC, 0x4B5C}};

static unsigned short
BinarySearchRange(const codes_t *array, int high, unsigned short code)
{
  int low, mid, distance, tmp;

  low = 0;
  mid = high >> 1;

  for (; low <= high; mid = (low + high) >> 1)
  {
    if ((array[mid].code <= code) && (array[mid + 1].code > code))
    {
      if (0 == array[mid].peer)
      {
        return 0;
      }
      if (code >= 0xa140U)
      {
                         
        tmp = ((code & 0xff00) - (array[mid].code & 0xff00)) >> 8;
        high = code & 0x00ff;
        low = array[mid].code & 0x00ff;

           
                                                                 
                                                                    
                                                                  
                                                                   
                                                                   
                                                                     
                              
           
        distance = tmp * 0x9d + high - low + (high >= 0xa1 ? (low >= 0xa1 ? 0 : -0x22) : (low >= 0xa1 ? +0x22 : 0));

           
                                                                    
                                                                   
                                                                      
                                                                       
                                                                    
                        
           
        tmp = (array[mid].peer & 0x00ff) + distance - 0x21;
        tmp = (array[mid].peer & 0xff00) + ((tmp / 0x5e) << 8) + 0x21 + tmp % 0x5e;
        return tmp;
      }
      else
      {
                         
        tmp = ((code & 0xff00) - (array[mid].code & 0xff00)) >> 8;

           
                                                                    
                                                                       
           
        distance = tmp * 0x5e + ((int)(code & 0x00ff) - (int)(array[mid].code & 0x00ff));

           
                                                               
                                                            
           
        low = array[mid].peer & 0x00ff;
        tmp = low + distance - (low >= 0xa1 ? 0x62 : 0x40);
        low = tmp % 0x9d;
        tmp = (array[mid].peer & 0xff00) + ((tmp / 0x9d) << 8) + (low > 0x3e ? 0x62 : 0x40) + low;
        return tmp;
      }
    }
    else if (array[mid].code > code)
    {
      high = mid - 1;
    }
    else
    {
      low = mid + 1;
    }
  }

  return 0;
}

unsigned short
BIG5toCNS(unsigned short big5, unsigned char *lc)
{
  unsigned short cns = 0;
  int i;

  if (big5 < 0xc940U)
  {
                 

    for (i = 0; i < sizeof(b1c4) / (sizeof(unsigned short) * 2); i++)
    {
      if (b1c4[i][0] == big5)
      {
        *lc = LC_CNS11643_4;
        return (b1c4[i][1] | 0x8080U);
      }
    }

    if (0 < (cns = BinarySearchRange(big5Level1ToCnsPlane1, 23, big5)))
    {
      *lc = LC_CNS11643_1;
    }
  }
  else if (big5 == 0xc94aU)
  {
                 
    *lc = LC_CNS11643_1;
    cns = 0x4442;
  }
  else
  {
                 
    for (i = 0; i < sizeof(b2c3) / (sizeof(unsigned short) * 2); i++)
    {
      if (b2c3[i][0] == big5)
      {
        *lc = LC_CNS11643_3;
        return (b2c3[i][1] | 0x8080U);
      }
    }

    if (0 < (cns = BinarySearchRange(big5Level2ToCnsPlane2, 46, big5)))
    {
      *lc = LC_CNS11643_2;
    }
  }

  if (0 == cns)
  {                                        
    *lc = 0;
    return (unsigned short)'?';
  }

  return cns | 0x8080;
}

unsigned short
CNStoBIG5(unsigned short cns, unsigned char lc)
{
  int i;
  unsigned int big5 = 0;

  cns &= 0x7f7f;

  switch (lc)
  {
  case LC_CNS11643_1:
    big5 = BinarySearchRange(cnsPlane1ToBig5Level1, 24, cns);
    break;
  case LC_CNS11643_2:
    big5 = BinarySearchRange(cnsPlane2ToBig5Level2, 47, cns);
    break;
  case LC_CNS11643_3:
    for (i = 0; i < sizeof(b2c3) / (sizeof(unsigned short) * 2); i++)
    {
      if (b2c3[i][1] == cns)
      {
        return b2c3[i][0];
      }
    }
    break;
  case LC_CNS11643_4:
    for (i = 0; i < sizeof(b1c4) / (sizeof(unsigned short) * 2); i++)
    {
      if (b1c4[i][1] == cns)
      {
        return b1c4[i][0];
      }
    }
  default:
    break;
  }
  return big5;
}
