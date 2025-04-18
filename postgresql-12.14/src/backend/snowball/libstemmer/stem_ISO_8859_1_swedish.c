                                                                             
                              

#include "header.h"

#ifdef __cplusplus
extern "C"
{
#endif
  extern int
  swedish_ISO_8859_1_stem(struct SN_env *z);
#ifdef __cplusplus
}
#endif
static int
r_other_suffix(struct SN_env *z);
static int
r_consonant_pair(struct SN_env *z);
static int
r_main_suffix(struct SN_env *z);
static int
r_mark_regions(struct SN_env *z);
#ifdef __cplusplus
extern "C"
{
#endif

  extern struct SN_env *
  swedish_ISO_8859_1_create_env(void);
  extern void
  swedish_ISO_8859_1_close_env(struct SN_env *z);

#ifdef __cplusplus
}
#endif
static const symbol s_0_0[1] = {'a'};
static const symbol s_0_1[4] = {'a', 'r', 'n', 'a'};
static const symbol s_0_2[4] = {'e', 'r', 'n', 'a'};
static const symbol s_0_3[7] = {'h', 'e', 't', 'e', 'r', 'n', 'a'};
static const symbol s_0_4[4] = {'o', 'r', 'n', 'a'};
static const symbol s_0_5[2] = {'a', 'd'};
static const symbol s_0_6[1] = {'e'};
static const symbol s_0_7[3] = {'a', 'd', 'e'};
static const symbol s_0_8[4] = {'a', 'n', 'd', 'e'};
static const symbol s_0_9[4] = {'a', 'r', 'n', 'e'};
static const symbol s_0_10[3] = {'a', 'r', 'e'};
static const symbol s_0_11[4] = {'a', 's', 't', 'e'};
static const symbol s_0_12[2] = {'e', 'n'};
static const symbol s_0_13[5] = {'a', 'n', 'd', 'e', 'n'};
static const symbol s_0_14[4] = {'a', 'r', 'e', 'n'};
static const symbol s_0_15[5] = {'h', 'e', 't', 'e', 'n'};
static const symbol s_0_16[3] = {'e', 'r', 'n'};
static const symbol s_0_17[2] = {'a', 'r'};
static const symbol s_0_18[2] = {'e', 'r'};
static const symbol s_0_19[5] = {'h', 'e', 't', 'e', 'r'};
static const symbol s_0_20[2] = {'o', 'r'};
static const symbol s_0_21[1] = {'s'};
static const symbol s_0_22[2] = {'a', 's'};
static const symbol s_0_23[5] = {'a', 'r', 'n', 'a', 's'};
static const symbol s_0_24[5] = {'e', 'r', 'n', 'a', 's'};
static const symbol s_0_25[5] = {'o', 'r', 'n', 'a', 's'};
static const symbol s_0_26[2] = {'e', 's'};
static const symbol s_0_27[4] = {'a', 'd', 'e', 's'};
static const symbol s_0_28[5] = {'a', 'n', 'd', 'e', 's'};
static const symbol s_0_29[3] = {'e', 'n', 's'};
static const symbol s_0_30[5] = {'a', 'r', 'e', 'n', 's'};
static const symbol s_0_31[6] = {'h', 'e', 't', 'e', 'n', 's'};
static const symbol s_0_32[4] = {'e', 'r', 'n', 's'};
static const symbol s_0_33[2] = {'a', 't'};
static const symbol s_0_34[5] = {'a', 'n', 'd', 'e', 't'};
static const symbol s_0_35[3] = {'h', 'e', 't'};
static const symbol s_0_36[3] = {'a', 's', 't'};

static const struct among a_0[37] = {
             {1, s_0_0, -1, 1, 0},
             {4, s_0_1, 0, 1, 0},
             {4, s_0_2, 0, 1, 0},
             {7, s_0_3, 2, 1, 0},
             {4, s_0_4, 0, 1, 0},
             {2, s_0_5, -1, 1, 0},
             {1, s_0_6, -1, 1, 0},
             {3, s_0_7, 6, 1, 0},
             {4, s_0_8, 6, 1, 0},
             {4, s_0_9, 6, 1, 0},
             {3, s_0_10, 6, 1, 0},
             {4, s_0_11, 6, 1, 0},
             {2, s_0_12, -1, 1, 0},
             {5, s_0_13, 12, 1, 0},
             {4, s_0_14, 12, 1, 0},
             {5, s_0_15, 12, 1, 0},
             {3, s_0_16, -1, 1, 0},
             {2, s_0_17, -1, 1, 0},
             {2, s_0_18, -1, 1, 0},
             {5, s_0_19, 18, 1, 0},
             {2, s_0_20, -1, 1, 0},
             {1, s_0_21, -1, 2, 0},
             {2, s_0_22, 21, 1, 0},
             {5, s_0_23, 22, 1, 0},
             {5, s_0_24, 22, 1, 0},
             {5, s_0_25, 22, 1, 0},
             {2, s_0_26, 21, 1, 0},
             {4, s_0_27, 26, 1, 0},
             {5, s_0_28, 26, 1, 0},
             {3, s_0_29, 21, 1, 0},
             {5, s_0_30, 29, 1, 0},
             {6, s_0_31, 29, 1, 0},
             {4, s_0_32, 21, 1, 0},
             {2, s_0_33, -1, 1, 0},
             {5, s_0_34, -1, 1, 0},
             {3, s_0_35, -1, 1, 0},
             {3, s_0_36, -1, 1, 0}};

static const symbol s_1_0[2] = {'d', 'd'};
static const symbol s_1_1[2] = {'g', 'd'};
static const symbol s_1_2[2] = {'n', 'n'};
static const symbol s_1_3[2] = {'d', 't'};
static const symbol s_1_4[2] = {'g', 't'};
static const symbol s_1_5[2] = {'k', 't'};
static const symbol s_1_6[2] = {'t', 't'};

static const struct among a_1[7] = {
             {2, s_1_0, -1, -1, 0},
             {2, s_1_1, -1, -1, 0},
             {2, s_1_2, -1, -1, 0},
             {2, s_1_3, -1, -1, 0},
             {2, s_1_4, -1, -1, 0},
             {2, s_1_5, -1, -1, 0},
             {2, s_1_6, -1, -1, 0}};

static const symbol s_2_0[2] = {'i', 'g'};
static const symbol s_2_1[3] = {'l', 'i', 'g'};
static const symbol s_2_2[3] = {'e', 'l', 's'};
static const symbol s_2_3[5] = {'f', 'u', 'l', 'l', 't'};
static const symbol s_2_4[4] = {'l', 0xF6, 's', 't'};

static const struct among a_2[5] = {
             {2, s_2_0, -1, 1, 0},
             {3, s_2_1, 0, 1, 0},
             {3, s_2_2, -1, 1, 0},
             {5, s_2_3, -1, 3, 0},
             {4, s_2_4, -1, 2, 0}};

static const unsigned char g_v[] = {17, 65, 16, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 0, 32};

static const unsigned char g_s_ending[] = {119, 127, 149};

static const symbol s_0[] = {'l', 0xF6, 's'};
static const symbol s_1[] = {'f', 'u', 'l', 'l'};

static int
r_mark_regions(struct SN_env *z)
{                                  
  z->I[0] = z->l;                                          
  {
    int c_test1 = z->c;                    
    {
      int ret = z->c + 3;                   
      if (0 > ret || ret > z->l)
      {
        return 0;
      }
      z->c = ret;
    }
    z->I[1] = z->c;                         
    z->c = c_test1;
  }
  if (out_grouping(z, g_v, 97, 246, 1) < 0)
  {
    return 0;            /* grouping v, line 30 */
  }
  {              /* non v, line 30 */
    int ret = in_grouping(z, g_v, 97, 246, 1);
    if (ret < 0)
    {
      return 0;
    }
    z->c += ret;
  }
  z->I[0] = z->c;                          
                    
  if (!(z->I[0] < z->I[1]))
  {
    goto lab0;                                                              
  }
  z->I[0] = z->I[1];                                          
lab0:
  return 1;
}

static int
r_main_suffix(struct SN_env *z)
{                   
  int among_var;

  {
    int mlimit1;                        
    if (z->c < z->I[0])
    {
      return 0;
    }
    mlimit1 = z->lb;
    z->lb = z->I[0];
    z->ket = z->c;                 
    if (z->c <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((1851442 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      z->lb = mlimit1;
      return 0;
    }                         
    among_var = find_among_b(z, a_0, 37);
    if (!(among_var))
    {
      z->lb = mlimit1;
      return 0;
    }
    z->bra = z->c;                 
    z->lb = mlimit1;
  }
  switch (among_var)
  {                     
  case 1:
  {
    int ret = slice_del(z);                      
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 2:
    if (in_grouping_b(z, g_s_ending, 98, 121, 0))
    {
      return 0;                                 
    }
    {
      int ret = slice_del(z);                      
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  }
  return 1;
}

static int
r_consonant_pair(struct SN_env *z)
{                   

  {
    int mlimit1;                        
    if (z->c < z->I[0])
    {
      return 0;
    }
    mlimit1 = z->lb;
    z->lb = z->I[0];
    {
      int m2 = z->l - z->c;
      (void)m2;                   
      if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((1064976 >> (z->p[z->c - 1] & 0x1f)) & 1))
      {
        z->lb = mlimit1;
        return 0;
      }                     
      if (!(find_among_b(z, a_1, 7)))
      {
        z->lb = mlimit1;
        return 0;
      }
      z->c = z->l - m2;
      z->ket = z->c;                 
      if (z->c <= z->lb)
      {
        z->lb = mlimit1;
        return 0;
      }
      z->c--;                           
      z->bra = z->c;                 
      {
        int ret = slice_del(z);                      
        if (ret < 0)
        {
          return ret;
        }
      }
    }
    z->lb = mlimit1;
  }
  return 1;
}

static int
r_other_suffix(struct SN_env *z)
{                   
  int among_var;

  {
    int mlimit1;                        
    if (z->c < z->I[0])
    {
      return 0;
    }
    mlimit1 = z->lb;
    z->lb = z->I[0];
    z->ket = z->c;                 
    if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((1572992 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      z->lb = mlimit1;
      return 0;
    }                         
    among_var = find_among_b(z, a_2, 5);
    if (!(among_var))
    {
      z->lb = mlimit1;
      return 0;
    }
    z->bra = z->c;                 
    switch (among_var)
    {                     
    case 1:
    {
      int ret = slice_del(z);                      
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 2:
    {
      int ret = slice_from_s(z, 3, s_0);                  
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 3:
    {
      int ret = slice_from_s(z, 4, s_1);                  
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    }
    z->lb = mlimit1;
  }
  return 1;
}

extern int
swedish_ISO_8859_1_stem(struct SN_env *z)
{                  
  {
    int c1 = z->c;                  
    {
      int ret = r_mark_regions(z);                                 
      if (ret == 0)
      {
        goto lab0;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab0:
    z->c = c1;
  }
  z->lb = z->c;
  z->c = z->l;                         

  {
    int m2 = z->l - z->c;
    (void)m2;                  
    {
      int ret = r_main_suffix(z);                                
      if (ret == 0)
      {
        goto lab1;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab1:
    z->c = z->l - m2;
  }
  {
    int m3 = z->l - z->c;
    (void)m3;                  
    {
      int ret = r_consonant_pair(z);                                   
      if (ret == 0)
      {
        goto lab2;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab2:
    z->c = z->l - m3;
  }
  {
    int m4 = z->l - z->c;
    (void)m4;                  
    {
      int ret = r_other_suffix(z);                                 
      if (ret == 0)
      {
        goto lab3;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab3:
    z->c = z->l - m4;
  }
  z->c = z->lb;
  return 1;
}

extern struct SN_env *
swedish_ISO_8859_1_create_env(void)
{
  return SN_create_env(0, 2, 0);
}

extern void
swedish_ISO_8859_1_close_env(struct SN_env *z)
{
  SN_close_env(z, 0);
}
