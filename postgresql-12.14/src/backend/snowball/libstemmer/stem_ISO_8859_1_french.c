                                                                             
                              

#include "header.h"

#ifdef __cplusplus
extern "C"
{
#endif
  extern int
  french_ISO_8859_1_stem(struct SN_env *z);
#ifdef __cplusplus
}
#endif
static int
r_un_accent(struct SN_env *z);
static int
r_un_double(struct SN_env *z);
static int
r_residual_suffix(struct SN_env *z);
static int
r_verb_suffix(struct SN_env *z);
static int
r_i_verb_suffix(struct SN_env *z);
static int
r_standard_suffix(struct SN_env *z);
static int
r_R2(struct SN_env *z);
static int
r_R1(struct SN_env *z);
static int
r_RV(struct SN_env *z);
static int
r_mark_regions(struct SN_env *z);
static int
r_postlude(struct SN_env *z);
static int
r_prelude(struct SN_env *z);
#ifdef __cplusplus
extern "C"
{
#endif

  extern struct SN_env *
  french_ISO_8859_1_create_env(void);
  extern void
  french_ISO_8859_1_close_env(struct SN_env *z);

#ifdef __cplusplus
}
#endif
static const symbol s_0_0[3] = {'c', 'o', 'l'};
static const symbol s_0_1[3] = {'p', 'a', 'r'};
static const symbol s_0_2[3] = {'t', 'a', 'p'};

static const struct among a_0[3] = {
             {3, s_0_0, -1, -1, 0},
             {3, s_0_1, -1, -1, 0},
             {3, s_0_2, -1, -1, 0}};

static const symbol s_1_1[1] = {'I'};
static const symbol s_1_2[1] = {'U'};
static const symbol s_1_3[1] = {'Y'};

static const struct among a_1[4] = {
             {0, 0, -1, 4, 0},
             {1, s_1_1, 0, 1, 0},
             {1, s_1_2, 0, 2, 0},
             {1, s_1_3, 0, 3, 0}};

static const symbol s_2_0[3] = {'i', 'q', 'U'};
static const symbol s_2_1[3] = {'a', 'b', 'l'};
static const symbol s_2_2[3] = {'I', 0xE8, 'r'};
static const symbol s_2_3[3] = {'i', 0xE8, 'r'};
static const symbol s_2_4[3] = {'e', 'u', 's'};
static const symbol s_2_5[2] = {'i', 'v'};

static const struct among a_2[6] = {
             {3, s_2_0, -1, 3, 0},
             {3, s_2_1, -1, 3, 0},
             {3, s_2_2, -1, 4, 0},
             {3, s_2_3, -1, 4, 0},
             {3, s_2_4, -1, 2, 0},
             {2, s_2_5, -1, 1, 0}};

static const symbol s_3_0[2] = {'i', 'c'};
static const symbol s_3_1[4] = {'a', 'b', 'i', 'l'};
static const symbol s_3_2[2] = {'i', 'v'};

static const struct among a_3[3] = {
             {2, s_3_0, -1, 2, 0},
             {4, s_3_1, -1, 1, 0},
             {2, s_3_2, -1, 3, 0}};

static const symbol s_4_0[4] = {'i', 'q', 'U', 'e'};
static const symbol s_4_1[6] = {'a', 't', 'r', 'i', 'c', 'e'};
static const symbol s_4_2[4] = {'a', 'n', 'c', 'e'};
static const symbol s_4_3[4] = {'e', 'n', 'c', 'e'};
static const symbol s_4_4[5] = {'l', 'o', 'g', 'i', 'e'};
static const symbol s_4_5[4] = {'a', 'b', 'l', 'e'};
static const symbol s_4_6[4] = {'i', 's', 'm', 'e'};
static const symbol s_4_7[4] = {'e', 'u', 's', 'e'};
static const symbol s_4_8[4] = {'i', 's', 't', 'e'};
static const symbol s_4_9[3] = {'i', 'v', 'e'};
static const symbol s_4_10[2] = {'i', 'f'};
static const symbol s_4_11[5] = {'u', 's', 'i', 'o', 'n'};
static const symbol s_4_12[5] = {'a', 't', 'i', 'o', 'n'};
static const symbol s_4_13[5] = {'u', 't', 'i', 'o', 'n'};
static const symbol s_4_14[5] = {'a', 't', 'e', 'u', 'r'};
static const symbol s_4_15[5] = {'i', 'q', 'U', 'e', 's'};
static const symbol s_4_16[7] = {'a', 't', 'r', 'i', 'c', 'e', 's'};
static const symbol s_4_17[5] = {'a', 'n', 'c', 'e', 's'};
static const symbol s_4_18[5] = {'e', 'n', 'c', 'e', 's'};
static const symbol s_4_19[6] = {'l', 'o', 'g', 'i', 'e', 's'};
static const symbol s_4_20[5] = {'a', 'b', 'l', 'e', 's'};
static const symbol s_4_21[5] = {'i', 's', 'm', 'e', 's'};
static const symbol s_4_22[5] = {'e', 'u', 's', 'e', 's'};
static const symbol s_4_23[5] = {'i', 's', 't', 'e', 's'};
static const symbol s_4_24[4] = {'i', 'v', 'e', 's'};
static const symbol s_4_25[3] = {'i', 'f', 's'};
static const symbol s_4_26[6] = {'u', 's', 'i', 'o', 'n', 's'};
static const symbol s_4_27[6] = {'a', 't', 'i', 'o', 'n', 's'};
static const symbol s_4_28[6] = {'u', 't', 'i', 'o', 'n', 's'};
static const symbol s_4_29[6] = {'a', 't', 'e', 'u', 'r', 's'};
static const symbol s_4_30[5] = {'m', 'e', 'n', 't', 's'};
static const symbol s_4_31[6] = {'e', 'm', 'e', 'n', 't', 's'};
static const symbol s_4_32[9] = {'i', 's', 's', 'e', 'm', 'e', 'n', 't', 's'};
static const symbol s_4_33[4] = {'i', 't', 0xE9, 's'};
static const symbol s_4_34[4] = {'m', 'e', 'n', 't'};
static const symbol s_4_35[5] = {'e', 'm', 'e', 'n', 't'};
static const symbol s_4_36[8] = {'i', 's', 's', 'e', 'm', 'e', 'n', 't'};
static const symbol s_4_37[6] = {'a', 'm', 'm', 'e', 'n', 't'};
static const symbol s_4_38[6] = {'e', 'm', 'm', 'e', 'n', 't'};
static const symbol s_4_39[3] = {'a', 'u', 'x'};
static const symbol s_4_40[4] = {'e', 'a', 'u', 'x'};
static const symbol s_4_41[3] = {'e', 'u', 'x'};
static const symbol s_4_42[3] = {'i', 't', 0xE9};

static const struct among a_4[43] = {
             {4, s_4_0, -1, 1, 0},
             {6, s_4_1, -1, 2, 0},
             {4, s_4_2, -1, 1, 0},
             {4, s_4_3, -1, 5, 0},
             {5, s_4_4, -1, 3, 0},
             {4, s_4_5, -1, 1, 0},
             {4, s_4_6, -1, 1, 0},
             {4, s_4_7, -1, 11, 0},
             {4, s_4_8, -1, 1, 0},
             {3, s_4_9, -1, 8, 0},
             {2, s_4_10, -1, 8, 0},
             {5, s_4_11, -1, 4, 0},
             {5, s_4_12, -1, 2, 0},
             {5, s_4_13, -1, 4, 0},
             {5, s_4_14, -1, 2, 0},
             {5, s_4_15, -1, 1, 0},
             {7, s_4_16, -1, 2, 0},
             {5, s_4_17, -1, 1, 0},
             {5, s_4_18, -1, 5, 0},
             {6, s_4_19, -1, 3, 0},
             {5, s_4_20, -1, 1, 0},
             {5, s_4_21, -1, 1, 0},
             {5, s_4_22, -1, 11, 0},
             {5, s_4_23, -1, 1, 0},
             {4, s_4_24, -1, 8, 0},
             {3, s_4_25, -1, 8, 0},
             {6, s_4_26, -1, 4, 0},
             {6, s_4_27, -1, 2, 0},
             {6, s_4_28, -1, 4, 0},
             {6, s_4_29, -1, 2, 0},
             {5, s_4_30, -1, 15, 0},
             {6, s_4_31, 30, 6, 0},
             {9, s_4_32, 31, 12, 0},
             {4, s_4_33, -1, 7, 0},
             {4, s_4_34, -1, 15, 0},
             {5, s_4_35, 34, 6, 0},
             {8, s_4_36, 35, 12, 0},
             {6, s_4_37, 34, 13, 0},
             {6, s_4_38, 34, 14, 0},
             {3, s_4_39, -1, 10, 0},
             {4, s_4_40, 39, 9, 0},
             {3, s_4_41, -1, 1, 0},
             {3, s_4_42, -1, 7, 0}};

static const symbol s_5_0[3] = {'i', 'r', 'a'};
static const symbol s_5_1[2] = {'i', 'e'};
static const symbol s_5_2[4] = {'i', 's', 's', 'e'};
static const symbol s_5_3[7] = {'i', 's', 's', 'a', 'n', 't', 'e'};
static const symbol s_5_4[1] = {'i'};
static const symbol s_5_5[4] = {'i', 'r', 'a', 'i'};
static const symbol s_5_6[2] = {'i', 'r'};
static const symbol s_5_7[4] = {'i', 'r', 'a', 's'};
static const symbol s_5_8[3] = {'i', 'e', 's'};
static const symbol s_5_9[4] = {0xEE, 'm', 'e', 's'};
static const symbol s_5_10[5] = {'i', 's', 's', 'e', 's'};
static const symbol s_5_11[8] = {'i', 's', 's', 'a', 'n', 't', 'e', 's'};
static const symbol s_5_12[4] = {0xEE, 't', 'e', 's'};
static const symbol s_5_13[2] = {'i', 's'};
static const symbol s_5_14[5] = {'i', 'r', 'a', 'i', 's'};
static const symbol s_5_15[6] = {'i', 's', 's', 'a', 'i', 's'};
static const symbol s_5_16[6] = {'i', 'r', 'i', 'o', 'n', 's'};
static const symbol s_5_17[7] = {'i', 's', 's', 'i', 'o', 'n', 's'};
static const symbol s_5_18[5] = {'i', 'r', 'o', 'n', 's'};
static const symbol s_5_19[6] = {'i', 's', 's', 'o', 'n', 's'};
static const symbol s_5_20[7] = {'i', 's', 's', 'a', 'n', 't', 's'};
static const symbol s_5_21[2] = {'i', 't'};
static const symbol s_5_22[5] = {'i', 'r', 'a', 'i', 't'};
static const symbol s_5_23[6] = {'i', 's', 's', 'a', 'i', 't'};
static const symbol s_5_24[6] = {'i', 's', 's', 'a', 'n', 't'};
static const symbol s_5_25[7] = {'i', 'r', 'a', 'I', 'e', 'n', 't'};
static const symbol s_5_26[8] = {'i', 's', 's', 'a', 'I', 'e', 'n', 't'};
static const symbol s_5_27[5] = {'i', 'r', 'e', 'n', 't'};
static const symbol s_5_28[6] = {'i', 's', 's', 'e', 'n', 't'};
static const symbol s_5_29[5] = {'i', 'r', 'o', 'n', 't'};
static const symbol s_5_30[2] = {0xEE, 't'};
static const symbol s_5_31[5] = {'i', 'r', 'i', 'e', 'z'};
static const symbol s_5_32[6] = {'i', 's', 's', 'i', 'e', 'z'};
static const symbol s_5_33[4] = {'i', 'r', 'e', 'z'};
static const symbol s_5_34[5] = {'i', 's', 's', 'e', 'z'};

static const struct among a_5[35] = {
             {3, s_5_0, -1, 1, 0},
             {2, s_5_1, -1, 1, 0},
             {4, s_5_2, -1, 1, 0},
             {7, s_5_3, -1, 1, 0},
             {1, s_5_4, -1, 1, 0},
             {4, s_5_5, 4, 1, 0},
             {2, s_5_6, -1, 1, 0},
             {4, s_5_7, -1, 1, 0},
             {3, s_5_8, -1, 1, 0},
             {4, s_5_9, -1, 1, 0},
             {5, s_5_10, -1, 1, 0},
             {8, s_5_11, -1, 1, 0},
             {4, s_5_12, -1, 1, 0},
             {2, s_5_13, -1, 1, 0},
             {5, s_5_14, 13, 1, 0},
             {6, s_5_15, 13, 1, 0},
             {6, s_5_16, -1, 1, 0},
             {7, s_5_17, -1, 1, 0},
             {5, s_5_18, -1, 1, 0},
             {6, s_5_19, -1, 1, 0},
             {7, s_5_20, -1, 1, 0},
             {2, s_5_21, -1, 1, 0},
             {5, s_5_22, 21, 1, 0},
             {6, s_5_23, 21, 1, 0},
             {6, s_5_24, -1, 1, 0},
             {7, s_5_25, -1, 1, 0},
             {8, s_5_26, -1, 1, 0},
             {5, s_5_27, -1, 1, 0},
             {6, s_5_28, -1, 1, 0},
             {5, s_5_29, -1, 1, 0},
             {2, s_5_30, -1, 1, 0},
             {5, s_5_31, -1, 1, 0},
             {6, s_5_32, -1, 1, 0},
             {4, s_5_33, -1, 1, 0},
             {5, s_5_34, -1, 1, 0}};

static const symbol s_6_0[1] = {'a'};
static const symbol s_6_1[3] = {'e', 'r', 'a'};
static const symbol s_6_2[4] = {'a', 's', 's', 'e'};
static const symbol s_6_3[4] = {'a', 'n', 't', 'e'};
static const symbol s_6_4[2] = {0xE9, 'e'};
static const symbol s_6_5[2] = {'a', 'i'};
static const symbol s_6_6[4] = {'e', 'r', 'a', 'i'};
static const symbol s_6_7[2] = {'e', 'r'};
static const symbol s_6_8[2] = {'a', 's'};
static const symbol s_6_9[4] = {'e', 'r', 'a', 's'};
static const symbol s_6_10[4] = {0xE2, 'm', 'e', 's'};
static const symbol s_6_11[5] = {'a', 's', 's', 'e', 's'};
static const symbol s_6_12[5] = {'a', 'n', 't', 'e', 's'};
static const symbol s_6_13[4] = {0xE2, 't', 'e', 's'};
static const symbol s_6_14[3] = {0xE9, 'e', 's'};
static const symbol s_6_15[3] = {'a', 'i', 's'};
static const symbol s_6_16[5] = {'e', 'r', 'a', 'i', 's'};
static const symbol s_6_17[4] = {'i', 'o', 'n', 's'};
static const symbol s_6_18[6] = {'e', 'r', 'i', 'o', 'n', 's'};
static const symbol s_6_19[7] = {'a', 's', 's', 'i', 'o', 'n', 's'};
static const symbol s_6_20[5] = {'e', 'r', 'o', 'n', 's'};
static const symbol s_6_21[4] = {'a', 'n', 't', 's'};
static const symbol s_6_22[2] = {0xE9, 's'};
static const symbol s_6_23[3] = {'a', 'i', 't'};
static const symbol s_6_24[5] = {'e', 'r', 'a', 'i', 't'};
static const symbol s_6_25[3] = {'a', 'n', 't'};
static const symbol s_6_26[5] = {'a', 'I', 'e', 'n', 't'};
static const symbol s_6_27[7] = {'e', 'r', 'a', 'I', 'e', 'n', 't'};
static const symbol s_6_28[5] = {0xE8, 'r', 'e', 'n', 't'};
static const symbol s_6_29[6] = {'a', 's', 's', 'e', 'n', 't'};
static const symbol s_6_30[5] = {'e', 'r', 'o', 'n', 't'};
static const symbol s_6_31[2] = {0xE2, 't'};
static const symbol s_6_32[2] = {'e', 'z'};
static const symbol s_6_33[3] = {'i', 'e', 'z'};
static const symbol s_6_34[5] = {'e', 'r', 'i', 'e', 'z'};
static const symbol s_6_35[6] = {'a', 's', 's', 'i', 'e', 'z'};
static const symbol s_6_36[4] = {'e', 'r', 'e', 'z'};
static const symbol s_6_37[1] = {0xE9};

static const struct among a_6[38] = {
             {1, s_6_0, -1, 3, 0},
             {3, s_6_1, 0, 2, 0},
             {4, s_6_2, -1, 3, 0},
             {4, s_6_3, -1, 3, 0},
             {2, s_6_4, -1, 2, 0},
             {2, s_6_5, -1, 3, 0},
             {4, s_6_6, 5, 2, 0},
             {2, s_6_7, -1, 2, 0},
             {2, s_6_8, -1, 3, 0},
             {4, s_6_9, 8, 2, 0},
             {4, s_6_10, -1, 3, 0},
             {5, s_6_11, -1, 3, 0},
             {5, s_6_12, -1, 3, 0},
             {4, s_6_13, -1, 3, 0},
             {3, s_6_14, -1, 2, 0},
             {3, s_6_15, -1, 3, 0},
             {5, s_6_16, 15, 2, 0},
             {4, s_6_17, -1, 1, 0},
             {6, s_6_18, 17, 2, 0},
             {7, s_6_19, 17, 3, 0},
             {5, s_6_20, -1, 2, 0},
             {4, s_6_21, -1, 3, 0},
             {2, s_6_22, -1, 2, 0},
             {3, s_6_23, -1, 3, 0},
             {5, s_6_24, 23, 2, 0},
             {3, s_6_25, -1, 3, 0},
             {5, s_6_26, -1, 3, 0},
             {7, s_6_27, 26, 2, 0},
             {5, s_6_28, -1, 2, 0},
             {6, s_6_29, -1, 3, 0},
             {5, s_6_30, -1, 2, 0},
             {2, s_6_31, -1, 3, 0},
             {2, s_6_32, -1, 2, 0},
             {3, s_6_33, 32, 2, 0},
             {5, s_6_34, 33, 2, 0},
             {6, s_6_35, 33, 3, 0},
             {4, s_6_36, 32, 2, 0},
             {1, s_6_37, -1, 2, 0}};

static const symbol s_7_0[1] = {'e'};
static const symbol s_7_1[4] = {'I', 0xE8, 'r', 'e'};
static const symbol s_7_2[4] = {'i', 0xE8, 'r', 'e'};
static const symbol s_7_3[3] = {'i', 'o', 'n'};
static const symbol s_7_4[3] = {'I', 'e', 'r'};
static const symbol s_7_5[3] = {'i', 'e', 'r'};
static const symbol s_7_6[1] = {0xEB};

static const struct among a_7[7] = {
             {1, s_7_0, -1, 3, 0},
             {4, s_7_1, 0, 2, 0},
             {4, s_7_2, 0, 2, 0},
             {3, s_7_3, -1, 1, 0},
             {3, s_7_4, -1, 2, 0},
             {3, s_7_5, -1, 2, 0},
             {1, s_7_6, -1, 4, 0}};

static const symbol s_8_0[3] = {'e', 'l', 'l'};
static const symbol s_8_1[4] = {'e', 'i', 'l', 'l'};
static const symbol s_8_2[3] = {'e', 'n', 'n'};
static const symbol s_8_3[3] = {'o', 'n', 'n'};
static const symbol s_8_4[3] = {'e', 't', 't'};

static const struct among a_8[5] = {
             {3, s_8_0, -1, -1, 0},
             {4, s_8_1, -1, -1, 0},
             {3, s_8_2, -1, -1, 0},
             {3, s_8_3, -1, -1, 0},
             {3, s_8_4, -1, -1, 0}};

static const unsigned char g_v[] = {17, 65, 16, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 130, 103, 8, 5};

static const unsigned char g_keep_with_s[] = {1, 65, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128};

static const symbol s_0[] = {'U'};
static const symbol s_1[] = {'I'};
static const symbol s_2[] = {'Y'};
static const symbol s_3[] = {'Y'};
static const symbol s_4[] = {'U'};
static const symbol s_5[] = {'i'};
static const symbol s_6[] = {'u'};
static const symbol s_7[] = {'y'};
static const symbol s_8[] = {'i', 'c'};
static const symbol s_9[] = {'i', 'q', 'U'};
static const symbol s_10[] = {'l', 'o', 'g'};
static const symbol s_11[] = {'u'};
static const symbol s_12[] = {'e', 'n', 't'};
static const symbol s_13[] = {'a', 't'};
static const symbol s_14[] = {'e', 'u', 'x'};
static const symbol s_15[] = {'i'};
static const symbol s_16[] = {'a', 'b', 'l'};
static const symbol s_17[] = {'i', 'q', 'U'};
static const symbol s_18[] = {'a', 't'};
static const symbol s_19[] = {'i', 'c'};
static const symbol s_20[] = {'i', 'q', 'U'};
static const symbol s_21[] = {'e', 'a', 'u'};
static const symbol s_22[] = {'a', 'l'};
static const symbol s_23[] = {'e', 'u', 'x'};
static const symbol s_24[] = {'a', 'n', 't'};
static const symbol s_25[] = {'e', 'n', 't'};
static const symbol s_26[] = {'i'};
static const symbol s_27[] = {'g', 'u'};
static const symbol s_28[] = {'e'};
static const symbol s_29[] = {'i'};
static const symbol s_30[] = {'c'};

static int
r_prelude(struct SN_env *z)
{                  
  while (1)
  {                      
    int c1 = z->c;
    while (1)
    {                    
      int c2 = z->c;
      {
        int c3 = z->c;                  
        if (in_grouping(z, g_v, 97, 251, 0))
        {
          goto lab3;                          
        }
        z->bra = z->c;                 
        {
          int c4 = z->c;                  
          if (z->c == z->l || z->p[z->c] != 'u')
          {
            goto lab5;                       
          }
          z->c++;
          z->ket = z->c;                 
          if (in_grouping(z, g_v, 97, 251, 0))
          {
            goto lab5;                          
          }
          {
            int ret = slice_from_s(z, 1, s_0);                  
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab4;
        lab5:
          z->c = c4;
          if (z->c == z->l || z->p[z->c] != 'i')
          {
            goto lab6;                       
          }
          z->c++;
          z->ket = z->c;                 
          if (in_grouping(z, g_v, 97, 251, 0))
          {
            goto lab6;                          
          }
          {
            int ret = slice_from_s(z, 1, s_1);                  
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab4;
        lab6:
          z->c = c4;
          if (z->c == z->l || z->p[z->c] != 'y')
          {
            goto lab3;                       
          }
          z->c++;
          z->ket = z->c;                 
          {
            int ret = slice_from_s(z, 1, s_2);                  
            if (ret < 0)
            {
              return ret;
            }
          }
        }
      lab4:
        goto lab2;
      lab3:
        z->c = c3;
        z->bra = z->c;                 
        if (z->c == z->l || z->p[z->c] != 'y')
        {
          goto lab7;                       
        }
        z->c++;
        z->ket = z->c;                 
        if (in_grouping(z, g_v, 97, 251, 0))
        {
          goto lab7;                          
        }
        {
          int ret = slice_from_s(z, 1, s_3);                  
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab2;
      lab7:
        z->c = c3;
        if (z->c == z->l || z->p[z->c] != 'q')
        {
          goto lab1;                       
        }
        z->c++;
        z->bra = z->c;                 
        if (z->c == z->l || z->p[z->c] != 'u')
        {
          goto lab1;                       
        }
        z->c++;
        z->ket = z->c;                 
        {
          int ret = slice_from_s(z, 1, s_4);                  
          if (ret < 0)
          {
            return ret;
          }
        }
      }
    lab2:
      z->c = c2;
      break;
    lab1:
      z->c = c2;
      if (z->c >= z->l)
      {
        goto lab0;
      }
      z->c++;                    
    }
    continue;
  lab0:
    z->c = c1;
    break;
  }
  return 1;
}

static int
r_mark_regions(struct SN_env *z)
{                                  
  z->I[0] = z->l;                                          
  z->I[1] = z->l;                                          
  z->I[2] = z->l;                                          
  {
    int c1 = z->c;                  
    {
      int c2 = z->c;                  
      if (in_grouping(z, g_v, 97, 251, 0))
      {
        goto lab2;                          
      }
      if (in_grouping(z, g_v, 97, 251, 0))
      {
        goto lab2;                          
      }
      if (z->c >= z->l)
      {
        goto lab2;
      }
      z->c++;                    
      goto lab1;
    lab2:
      z->c = c2;
      if (z->c + 2 >= z->l || z->p[z->c + 2] >> 5 != 3 || !((331776 >> (z->p[z->c + 2] & 0x1f)) & 1))
      {
        goto lab3;                     
      }
      if (!(find_among(z, a_0, 3)))
      {
        goto lab3;
      }
      goto lab1;
    lab3:
      z->c = c2;
      if (z->c >= z->l)
      {
        goto lab0;
      }
      z->c++;                           
      {              /* grouping v, line 66 */
        int ret = out_grouping(z, g_v, 97, 251, 1);
        if (ret < 0)
        {
          goto lab0;
        }
        z->c += ret;
      }
    }
  lab1:
    z->I[0] = z->c;                          
  lab0:
    z->c = c1;
  }
  {
    int c3 = z->c;                  
    {              /* grouping v, line 70 */
      int ret = out_grouping(z, g_v, 97, 251, 1);
      if (ret < 0)
      {
        goto lab4;
      }
      z->c += ret;
    }
    {              /* non v, line 70 */
      int ret = in_grouping(z, g_v, 97, 251, 1);
      if (ret < 0)
      {
        goto lab4;
      }
      z->c += ret;
    }
    z->I[1] = z->c;                          
    {               /* grouping v, line 71 */
      int ret = out_grouping(z, g_v, 97, 251, 1);
      if (ret < 0)
      {
        goto lab4;
      }
      z->c += ret;
    }
    {              /* non v, line 71 */
      int ret = in_grouping(z, g_v, 97, 251, 1);
      if (ret < 0)
      {
        goto lab4;
      }
      z->c += ret;
    }
    z->I[2] = z->c;                          
  lab4:
    z->c = c3;
  }
  return 1;
}

static int
r_postlude(struct SN_env *z)
{                  
  int among_var;
  while (1)
  {                      
    int c1 = z->c;
    z->bra = z->c;                 
    if (z->c >= z->l || z->p[z->c + 0] >> 5 != 2 || !((35652096 >> (z->p[z->c + 0] & 0x1f)) & 1))
    {
      among_var = 4;
    }
    else                         
    {
      among_var = find_among(z, a_1, 4);
    }
    if (!(among_var))
    {
      goto lab0;
    }
    z->ket = z->c;                 
    switch (among_var)
    {                     
    case 1:
    {
      int ret = slice_from_s(z, 1, s_5);                  
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 2:
    {
      int ret = slice_from_s(z, 1, s_6);                  
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 3:
    {
      int ret = slice_from_s(z, 1, s_7);                  
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 4:
      if (z->c >= z->l)
      {
        goto lab0;
      }
      z->c++;                    
      break;
    }
    continue;
  lab0:
    z->c = c1;
    break;
  }
  return 1;
}

static int
r_RV(struct SN_env *z)
{                   
  if (!(z->I[0] <= z->c))
  {
    return 0;                                                               
  }
  return 1;
}

static int
r_R1(struct SN_env *z)
{                   
  if (!(z->I[1] <= z->c))
  {
    return 0;                                                               
  }
  return 1;
}

static int
r_R2(struct SN_env *z)
{                   
  if (!(z->I[2] <= z->c))
  {
    return 0;                                                               
  }
  return 1;
}

static int
r_standard_suffix(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                                        
  among_var = find_among_b(z, a_4, 43);                         
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                 
  switch (among_var)
  {                     
  case 1:
  {
    int ret = r_R2(z);                       
    if (ret <= 0)
    {
      return ret;
    }
  }
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
    int ret = r_R2(z);                       
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_del(z);                      
      if (ret < 0)
      {
        return ret;
      }
    }
    {
      int m1 = z->l - z->c;
      (void)m1;                         
      z->ket = z->c;                  
      if (!(eq_s_b(z, 2, s_8)))
      {
        z->c = z->l - m1;
        goto lab0;
      }                                     
      z->bra = z->c;                  
      {
        int m2 = z->l - z->c;
        (void)m2;                   
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            goto lab2;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab1;
      lab2:
        z->c = z->l - m2;
        {
          int ret = slice_from_s(z, 3, s_9);                   
          if (ret < 0)
          {
            return ret;
          }
        }
      }
    lab1:
    lab0:;
    }
    break;
  case 3:
  {
    int ret = r_R2(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 3, s_10);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 4:
  {
    int ret = r_R2(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 1, s_11);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 5:
  {
    int ret = r_R2(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 3, s_12);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 6:
  {
    int ret = r_RV(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    {
      int m3 = z->l - z->c;
      (void)m3;                                               
      z->ket = z->c;                                        
      among_var = find_among_b(z, a_2, 6);                          
      if (!(among_var))
      {
        z->c = z->l - m3;
        goto lab3;
      }
      z->bra = z->c;                  
      switch (among_var)
      {                      
      case 1:
      {
        int ret = r_R2(z);                        
        if (ret == 0)
        {
          z->c = z->l - m3;
          goto lab3;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        z->ket = z->c;                  
        if (!(eq_s_b(z, 2, s_13)))
        {
          z->c = z->l - m3;
          goto lab3;
        }                                     
        z->bra = z->c;                  
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            z->c = z->l - m3;
            goto lab3;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
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
        int m4 = z->l - z->c;
        (void)m4;                   
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            goto lab5;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab4;
      lab5:
        z->c = z->l - m4;
        {
          int ret = r_R1(z);                        
          if (ret == 0)
          {
            z->c = z->l - m3;
            goto lab3;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_from_s(z, 3, s_14);                   
          if (ret < 0)
          {
            return ret;
          }
        }
      }
      lab4:
        break;
      case 3:
      {
        int ret = r_R2(z);                        
        if (ret == 0)
        {
          z->c = z->l - m3;
          goto lab3;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
      case 4:
      {
        int ret = r_RV(z);                        
        if (ret == 0)
        {
          z->c = z->l - m3;
          goto lab3;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
        {
          int ret = slice_from_s(z, 1, s_15);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
      }
    lab3:;
    }
    break;
  case 7:
  {
    int ret = r_R2(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    {
      int m5 = z->l - z->c;
      (void)m5;                         
      z->ket = z->c;                  
      if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((4198408 >> (z->p[z->c - 1] & 0x1f)) & 1))
      {
        z->c = z->l - m5;
        goto lab6;
      }                          
      among_var = find_among_b(z, a_3, 3);
      if (!(among_var))
      {
        z->c = z->l - m5;
        goto lab6;
      }
      z->bra = z->c;                  
      switch (among_var)
      {                      
      case 1:
      {
        int m6 = z->l - z->c;
        (void)m6;                   
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            goto lab8;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab7;
      lab8:
        z->c = z->l - m6;
        {
          int ret = slice_from_s(z, 3, s_16);                   
          if (ret < 0)
          {
            return ret;
          }
        }
      }
      lab7:
        break;
      case 2:
      {
        int m7 = z->l - z->c;
        (void)m7;                   
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            goto lab10;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab9;
      lab10:
        z->c = z->l - m7;
        {
          int ret = slice_from_s(z, 3, s_17);                   
          if (ret < 0)
          {
            return ret;
          }
        }
      }
      lab9:
        break;
      case 3:
      {
        int ret = r_R2(z);                        
        if (ret == 0)
        {
          z->c = z->l - m5;
          goto lab6;
        }
        if (ret < 0)
        {
          return ret;
        }
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
    lab6:;
    }
    break;
  case 8:
  {
    int ret = r_R2(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    {
      int m8 = z->l - z->c;
      (void)m8;                         
      z->ket = z->c;                  
      if (!(eq_s_b(z, 2, s_18)))
      {
        z->c = z->l - m8;
        goto lab11;
      }                                     
      z->bra = z->c;                  
      {
        int ret = r_R2(z);                        
        if (ret == 0)
        {
          z->c = z->l - m8;
          goto lab11;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
      {
        int ret = slice_del(z);                       
        if (ret < 0)
        {
          return ret;
        }
      }
      z->ket = z->c;                  
      if (!(eq_s_b(z, 2, s_19)))
      {
        z->c = z->l - m8;
        goto lab11;
      }                                     
      z->bra = z->c;                  
      {
        int m9 = z->l - z->c;
        (void)m9;                   
        {
          int ret = r_R2(z);                        
          if (ret == 0)
          {
            goto lab13;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        {
          int ret = slice_del(z);                       
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab12;
      lab13:
        z->c = z->l - m9;
        {
          int ret = slice_from_s(z, 3, s_20);                   
          if (ret < 0)
          {
            return ret;
          }
        }
      }
    lab12:
    lab11:;
    }
    break;
  case 9:
  {
    int ret = slice_from_s(z, 3, s_21);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 10:
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 2, s_22);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 11:
  {
    int m10 = z->l - z->c;
    (void)m10;                   
    {
      int ret = r_R2(z);                        
      if (ret == 0)
      {
        goto lab15;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    goto lab14;
  lab15:
    z->c = z->l - m10;
    {
      int ret = r_R1(z);                        
      if (ret <= 0)
      {
        return ret;
      }
    }
    {
      int ret = slice_from_s(z, 3, s_23);                   
      if (ret < 0)
      {
        return ret;
      }
    }
  }
  lab14:
    break;
  case 12:
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    if (out_grouping_b(z, g_v, 97, 251, 0))
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
  case 13:
  {
    int ret = r_RV(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 3, s_24);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    return 0;                     
    break;
  case 14:
  {
    int ret = r_RV(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
    {
      int ret = slice_from_s(z, 3, s_25);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    return 0;                     
    break;
  case 15:
  {
    int m_test11 = z->l - z->c;                     
    if (in_grouping_b(z, g_v, 97, 251, 0))
    {
      return 0;                           
    }
    {
      int ret = r_RV(z);                        
      if (ret <= 0)
      {
        return ret;
      }
    }
    z->c = z->l - m_test11;
  }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    return 0;                     
    break;
  }
  return 1;
}

static int
r_i_verb_suffix(struct SN_env *z)
{                   

  {
    int mlimit1;                         
    if (z->c < z->I[0])
    {
      return 0;
    }
    mlimit1 = z->lb;
    z->lb = z->I[0];
    z->ket = z->c;                  
    if (z->c <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((68944418 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      z->lb = mlimit1;
      return 0;
    }                          
    if (!(find_among_b(z, a_5, 35)))
    {
      z->lb = mlimit1;
      return 0;
    }
    z->bra = z->c;                  
    if (out_grouping_b(z, g_v, 97, 251, 0))
    {
      z->lb = mlimit1;
      return 0;
    }                      
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    z->lb = mlimit1;
  }
  return 1;
}

static int
r_verb_suffix(struct SN_env *z)
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
    among_var = find_among_b(z, a_6, 38);                          
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
      int ret = r_R2(z);                        
      if (ret == 0)
      {
        z->lb = mlimit1;
        return 0;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
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
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 3:
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
      {
        int m2 = z->l - z->c;
        (void)m2;                         
        z->ket = z->c;                  
        if (z->c <= z->lb || z->p[z->c - 1] != 'e')
        {
          z->c = z->l - m2;
          goto lab0;
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
      lab0:;
      }
      break;
    }
    z->lb = mlimit1;
  }
  return 1;
}

static int
r_residual_suffix(struct SN_env *z)
{                   
  int among_var;
  {
    int m1 = z->l - z->c;
    (void)m1;                         
    z->ket = z->c;                  
    if (z->c <= z->lb || z->p[z->c - 1] != 's')
    {
      z->c = z->l - m1;
      goto lab0;
    }                        
    z->c--;
    z->bra = z->c;                  
    {
      int m_test2 = z->l - z->c;                     
      if (out_grouping_b(z, g_keep_with_s, 97, 232, 0))
      {
        z->c = z->l - m1;
        goto lab0;
      }                                
      z->c = z->l - m_test2;
    }
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
  lab0:;
  }

  {
    int mlimit3;                         
    if (z->c < z->I[0])
    {
      return 0;
    }
    mlimit3 = z->lb;
    z->lb = z->I[0];
    z->ket = z->c;                                        
    among_var = find_among_b(z, a_7, 7);                          
    if (!(among_var))
    {
      z->lb = mlimit3;
      return 0;
    }
    z->bra = z->c;                  
    switch (among_var)
    {                      
    case 1:
    {
      int ret = r_R2(z);                        
      if (ret == 0)
      {
        z->lb = mlimit3;
        return 0;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
      {
        int m4 = z->l - z->c;
        (void)m4;                   
        if (z->c <= z->lb || z->p[z->c - 1] != 's')
        {
          goto lab2;                        
        }
        z->c--;
        goto lab1;
      lab2:
        z->c = z->l - m4;
        if (z->c <= z->lb || z->p[z->c - 1] != 't')
        {
          z->lb = mlimit3;
          return 0;
        }                        
        z->c--;
      }
    lab1:
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
      int ret = slice_from_s(z, 1, s_26);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 3:
    {
      int ret = slice_del(z);                       
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
    case 4:
      if (!(eq_s_b(z, 2, s_27)))
      {
        z->lb = mlimit3;
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
    z->lb = mlimit3;
  }
  return 1;
}

static int
r_un_double(struct SN_env *z)
{                   
  {
    int m_test1 = z->l - z->c;                     
    if (z->c - 2 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((1069056 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      return 0;                      
    }
    if (!(find_among_b(z, a_8, 5)))
    {
      return 0;
    }
    z->c = z->l - m_test1;
  }
  z->ket = z->c;                  
  if (z->c <= z->lb)
  {
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
  return 1;
}

static int
r_un_accent(struct SN_env *z)
{                   
  {
    int i = 1;
    while (1)
    {                        
      if (out_grouping_b(z, g_v, 97, 251, 0))
      {
        goto lab0;                      
      }
      i--;
      continue;
    lab0:
      break;
    }
    if (i > 0)
    {
      return 0;
    }
  }
  z->ket = z->c;                  
  {
    int m1 = z->l - z->c;
    (void)m1;                   
    if (z->c <= z->lb || z->p[z->c - 1] != 0xE9)
    {
      goto lab2;                        
    }
    z->c--;
    goto lab1;
  lab2:
    z->c = z->l - m1;
    if (z->c <= z->lb || z->p[z->c - 1] != 0xE8)
    {
      return 0;                        
    }
    z->c--;
  }
lab1:
  z->bra = z->c;                  
  {
    int ret = slice_from_s(z, 1, s_28);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  return 1;
}

extern int
french_ISO_8859_1_stem(struct SN_env *z)
{                  
  {
    int c1 = z->c;                   
    {
      int ret = r_prelude(z);                             
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
                    
  {
    int ret = r_mark_regions(z);                                  
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
  z->lb = z->c;
  z->c = z->l;                          

  {
    int m2 = z->l - z->c;
    (void)m2;                   
    {
      int m3 = z->l - z->c;
      (void)m3;                   
      {
        int m4 = z->l - z->c;
        (void)m4;                    
        {
          int m5 = z->l - z->c;
          (void)m5;                   
          {
            int ret = r_standard_suffix(z);                                     
            if (ret == 0)
            {
              goto lab6;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab5;
        lab6:
          z->c = z->l - m5;
          {
            int ret = r_i_verb_suffix(z);                                   
            if (ret == 0)
            {
              goto lab7;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab5;
        lab7:
          z->c = z->l - m5;
          {
            int ret = r_verb_suffix(z);                                 
            if (ret == 0)
            {
              goto lab4;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
        }
      lab5:
        z->c = z->l - m4;
        {
          int m6 = z->l - z->c;
          (void)m6;                         
          z->ket = z->c;                  
          {
            int m7 = z->l - z->c;
            (void)m7;                   
            if (z->c <= z->lb || z->p[z->c - 1] != 'Y')
            {
              goto lab10;                        
            }
            z->c--;
            z->bra = z->c;                  
            {
              int ret = slice_from_s(z, 1, s_29);                   
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab9;
          lab10:
            z->c = z->l - m7;
            if (z->c <= z->lb || z->p[z->c - 1] != 0xE7)
            {
              z->c = z->l - m6;
              goto lab8;
            }                        
            z->c--;
            z->bra = z->c;                  
            {
              int ret = slice_from_s(z, 1, s_30);                   
              if (ret < 0)
              {
                return ret;
              }
            }
          }
        lab9:
        lab8:;
        }
      }
      goto lab3;
    lab4:
      z->c = z->l - m3;
      {
        int ret = r_residual_suffix(z);                                     
        if (ret == 0)
        {
          goto lab2;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
    }
  lab3:
  lab2:
    z->c = z->l - m2;
  }
  {
    int m8 = z->l - z->c;
    (void)m8;                   
    {
      int ret = r_un_double(z);                               
      if (ret == 0)
      {
        goto lab11;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab11:
    z->c = z->l - m8;
  }
  {
    int m9 = z->l - z->c;
    (void)m9;                   
    {
      int ret = r_un_accent(z);                               
      if (ret == 0)
      {
        goto lab12;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab12:
    z->c = z->l - m9;
  }
  z->c = z->lb;
  {
    int c10 = z->c;                   
    {
      int ret = r_postlude(z);                              
      if (ret == 0)
      {
        goto lab13;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab13:
    z->c = c10;
  }
  return 1;
}

extern struct SN_env *
french_ISO_8859_1_create_env(void)
{
  return SN_create_env(0, 3, 0);
}

extern void
french_ISO_8859_1_close_env(struct SN_env *z)
{
  SN_close_env(z, 0);
}
