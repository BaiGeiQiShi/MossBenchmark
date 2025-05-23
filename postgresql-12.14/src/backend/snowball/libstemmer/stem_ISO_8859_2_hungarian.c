                                                                             
                              

#include "header.h"

#ifdef __cplusplus
extern "C"
{
#endif
  extern int
  hungarian_ISO_8859_2_stem(struct SN_env *z);
#ifdef __cplusplus
}
#endif
static int
r_double(struct SN_env *z);
static int
r_undouble(struct SN_env *z);
static int
r_factive(struct SN_env *z);
static int
r_instrum(struct SN_env *z);
static int
r_plur_owner(struct SN_env *z);
static int
r_sing_owner(struct SN_env *z);
static int
r_owned(struct SN_env *z);
static int
r_plural(struct SN_env *z);
static int
r_case_other(struct SN_env *z);
static int
r_case_special(struct SN_env *z);
static int
r_case(struct SN_env *z);
static int
r_v_ending(struct SN_env *z);
static int
r_R1(struct SN_env *z);
static int
r_mark_regions(struct SN_env *z);
#ifdef __cplusplus
extern "C"
{
#endif

  extern struct SN_env *
  hungarian_ISO_8859_2_create_env(void);
  extern void
  hungarian_ISO_8859_2_close_env(struct SN_env *z);

#ifdef __cplusplus
}
#endif
static const symbol s_0_0[2] = {'c', 's'};
static const symbol s_0_1[3] = {'d', 'z', 's'};
static const symbol s_0_2[2] = {'g', 'y'};
static const symbol s_0_3[2] = {'l', 'y'};
static const symbol s_0_4[2] = {'n', 'y'};
static const symbol s_0_5[2] = {'s', 'z'};
static const symbol s_0_6[2] = {'t', 'y'};
static const symbol s_0_7[2] = {'z', 's'};

static const struct among a_0[8] = {
             {2, s_0_0, -1, -1, 0},
             {3, s_0_1, -1, -1, 0},
             {2, s_0_2, -1, -1, 0},
             {2, s_0_3, -1, -1, 0},
             {2, s_0_4, -1, -1, 0},
             {2, s_0_5, -1, -1, 0},
             {2, s_0_6, -1, -1, 0},
             {2, s_0_7, -1, -1, 0}};

static const symbol s_1_0[1] = {0xE1};
static const symbol s_1_1[1] = {0xE9};

static const struct among a_1[2] = {
             {1, s_1_0, -1, 1, 0},
             {1, s_1_1, -1, 2, 0}};

static const symbol s_2_0[2] = {'b', 'b'};
static const symbol s_2_1[2] = {'c', 'c'};
static const symbol s_2_2[2] = {'d', 'd'};
static const symbol s_2_3[2] = {'f', 'f'};
static const symbol s_2_4[2] = {'g', 'g'};
static const symbol s_2_5[2] = {'j', 'j'};
static const symbol s_2_6[2] = {'k', 'k'};
static const symbol s_2_7[2] = {'l', 'l'};
static const symbol s_2_8[2] = {'m', 'm'};
static const symbol s_2_9[2] = {'n', 'n'};
static const symbol s_2_10[2] = {'p', 'p'};
static const symbol s_2_11[2] = {'r', 'r'};
static const symbol s_2_12[3] = {'c', 'c', 's'};
static const symbol s_2_13[2] = {'s', 's'};
static const symbol s_2_14[3] = {'z', 'z', 's'};
static const symbol s_2_15[2] = {'t', 't'};
static const symbol s_2_16[2] = {'v', 'v'};
static const symbol s_2_17[3] = {'g', 'g', 'y'};
static const symbol s_2_18[3] = {'l', 'l', 'y'};
static const symbol s_2_19[3] = {'n', 'n', 'y'};
static const symbol s_2_20[3] = {'t', 't', 'y'};
static const symbol s_2_21[3] = {'s', 's', 'z'};
static const symbol s_2_22[2] = {'z', 'z'};

static const struct among a_2[23] = {
             {2, s_2_0, -1, -1, 0},
             {2, s_2_1, -1, -1, 0},
             {2, s_2_2, -1, -1, 0},
             {2, s_2_3, -1, -1, 0},
             {2, s_2_4, -1, -1, 0},
             {2, s_2_5, -1, -1, 0},
             {2, s_2_6, -1, -1, 0},
             {2, s_2_7, -1, -1, 0},
             {2, s_2_8, -1, -1, 0},
             {2, s_2_9, -1, -1, 0},
             {2, s_2_10, -1, -1, 0},
             {2, s_2_11, -1, -1, 0},
             {3, s_2_12, -1, -1, 0},
             {2, s_2_13, -1, -1, 0},
             {3, s_2_14, -1, -1, 0},
             {2, s_2_15, -1, -1, 0},
             {2, s_2_16, -1, -1, 0},
             {3, s_2_17, -1, -1, 0},
             {3, s_2_18, -1, -1, 0},
             {3, s_2_19, -1, -1, 0},
             {3, s_2_20, -1, -1, 0},
             {3, s_2_21, -1, -1, 0},
             {2, s_2_22, -1, -1, 0}};

static const symbol s_3_0[2] = {'a', 'l'};
static const symbol s_3_1[2] = {'e', 'l'};

static const struct among a_3[2] = {
             {2, s_3_0, -1, 1, 0},
             {2, s_3_1, -1, 1, 0}};

static const symbol s_4_0[2] = {'b', 'a'};
static const symbol s_4_1[2] = {'r', 'a'};
static const symbol s_4_2[2] = {'b', 'e'};
static const symbol s_4_3[2] = {'r', 'e'};
static const symbol s_4_4[2] = {'i', 'g'};
static const symbol s_4_5[3] = {'n', 'a', 'k'};
static const symbol s_4_6[3] = {'n', 'e', 'k'};
static const symbol s_4_7[3] = {'v', 'a', 'l'};
static const symbol s_4_8[3] = {'v', 'e', 'l'};
static const symbol s_4_9[2] = {'u', 'l'};
static const symbol s_4_10[3] = {'n', 0xE1, 'l'};
static const symbol s_4_11[3] = {'n', 0xE9, 'l'};
static const symbol s_4_12[3] = {'b', 0xF3, 'l'};
static const symbol s_4_13[3] = {'r', 0xF3, 'l'};
static const symbol s_4_14[3] = {'t', 0xF3, 'l'};
static const symbol s_4_15[3] = {'b', 0xF5, 'l'};
static const symbol s_4_16[3] = {'r', 0xF5, 'l'};
static const symbol s_4_17[3] = {'t', 0xF5, 'l'};
static const symbol s_4_18[2] = {0xFC, 'l'};
static const symbol s_4_19[1] = {'n'};
static const symbol s_4_20[2] = {'a', 'n'};
static const symbol s_4_21[3] = {'b', 'a', 'n'};
static const symbol s_4_22[2] = {'e', 'n'};
static const symbol s_4_23[3] = {'b', 'e', 'n'};
static const symbol s_4_24[6] = {'k', 0xE9, 'p', 'p', 'e', 'n'};
static const symbol s_4_25[2] = {'o', 'n'};
static const symbol s_4_26[2] = {0xF6, 'n'};
static const symbol s_4_27[4] = {'k', 0xE9, 'p', 'p'};
static const symbol s_4_28[3] = {'k', 'o', 'r'};
static const symbol s_4_29[1] = {'t'};
static const symbol s_4_30[2] = {'a', 't'};
static const symbol s_4_31[2] = {'e', 't'};
static const symbol s_4_32[4] = {'k', 0xE9, 'n', 't'};
static const symbol s_4_33[6] = {'a', 'n', 'k', 0xE9, 'n', 't'};
static const symbol s_4_34[6] = {'e', 'n', 'k', 0xE9, 'n', 't'};
static const symbol s_4_35[6] = {'o', 'n', 'k', 0xE9, 'n', 't'};
static const symbol s_4_36[2] = {'o', 't'};
static const symbol s_4_37[3] = {0xE9, 'r', 't'};
static const symbol s_4_38[2] = {0xF6, 't'};
static const symbol s_4_39[3] = {'h', 'e', 'z'};
static const symbol s_4_40[3] = {'h', 'o', 'z'};
static const symbol s_4_41[3] = {'h', 0xF6, 'z'};
static const symbol s_4_42[2] = {'v', 0xE1};
static const symbol s_4_43[2] = {'v', 0xE9};

static const struct among a_4[44] = {
             {2, s_4_0, -1, -1, 0},
             {2, s_4_1, -1, -1, 0},
             {2, s_4_2, -1, -1, 0},
             {2, s_4_3, -1, -1, 0},
             {2, s_4_4, -1, -1, 0},
             {3, s_4_5, -1, -1, 0},
             {3, s_4_6, -1, -1, 0},
             {3, s_4_7, -1, -1, 0},
             {3, s_4_8, -1, -1, 0},
             {2, s_4_9, -1, -1, 0},
             {3, s_4_10, -1, -1, 0},
             {3, s_4_11, -1, -1, 0},
             {3, s_4_12, -1, -1, 0},
             {3, s_4_13, -1, -1, 0},
             {3, s_4_14, -1, -1, 0},
             {3, s_4_15, -1, -1, 0},
             {3, s_4_16, -1, -1, 0},
             {3, s_4_17, -1, -1, 0},
             {2, s_4_18, -1, -1, 0},
             {1, s_4_19, -1, -1, 0},
             {2, s_4_20, 19, -1, 0},
             {3, s_4_21, 20, -1, 0},
             {2, s_4_22, 19, -1, 0},
             {3, s_4_23, 22, -1, 0},
             {6, s_4_24, 22, -1, 0},
             {2, s_4_25, 19, -1, 0},
             {2, s_4_26, 19, -1, 0},
             {4, s_4_27, -1, -1, 0},
             {3, s_4_28, -1, -1, 0},
             {1, s_4_29, -1, -1, 0},
             {2, s_4_30, 29, -1, 0},
             {2, s_4_31, 29, -1, 0},
             {4, s_4_32, 29, -1, 0},
             {6, s_4_33, 32, -1, 0},
             {6, s_4_34, 32, -1, 0},
             {6, s_4_35, 32, -1, 0},
             {2, s_4_36, 29, -1, 0},
             {3, s_4_37, 29, -1, 0},
             {2, s_4_38, 29, -1, 0},
             {3, s_4_39, -1, -1, 0},
             {3, s_4_40, -1, -1, 0},
             {3, s_4_41, -1, -1, 0},
             {2, s_4_42, -1, -1, 0},
             {2, s_4_43, -1, -1, 0}};

static const symbol s_5_0[2] = {0xE1, 'n'};
static const symbol s_5_1[2] = {0xE9, 'n'};
static const symbol s_5_2[6] = {0xE1, 'n', 'k', 0xE9, 'n', 't'};

static const struct among a_5[3] = {
             {2, s_5_0, -1, 2, 0},
             {2, s_5_1, -1, 1, 0},
             {6, s_5_2, -1, 2, 0}};

static const symbol s_6_0[4] = {'s', 't', 'u', 'l'};
static const symbol s_6_1[5] = {'a', 's', 't', 'u', 'l'};
static const symbol s_6_2[5] = {0xE1, 's', 't', 'u', 'l'};
static const symbol s_6_3[4] = {'s', 't', 0xFC, 'l'};
static const symbol s_6_4[5] = {'e', 's', 't', 0xFC, 'l'};
static const symbol s_6_5[5] = {0xE9, 's', 't', 0xFC, 'l'};

static const struct among a_6[6] = {
             {4, s_6_0, -1, 1, 0},
             {5, s_6_1, 0, 1, 0},
             {5, s_6_2, 0, 2, 0},
             {4, s_6_3, -1, 1, 0},
             {5, s_6_4, 3, 1, 0},
             {5, s_6_5, 3, 3, 0}};

static const symbol s_7_0[1] = {0xE1};
static const symbol s_7_1[1] = {0xE9};

static const struct among a_7[2] = {
             {1, s_7_0, -1, 1, 0},
             {1, s_7_1, -1, 1, 0}};

static const symbol s_8_0[1] = {'k'};
static const symbol s_8_1[2] = {'a', 'k'};
static const symbol s_8_2[2] = {'e', 'k'};
static const symbol s_8_3[2] = {'o', 'k'};
static const symbol s_8_4[2] = {0xE1, 'k'};
static const symbol s_8_5[2] = {0xE9, 'k'};
static const symbol s_8_6[2] = {0xF6, 'k'};

static const struct among a_8[7] = {
             {1, s_8_0, -1, 3, 0},
             {2, s_8_1, 0, 3, 0},
             {2, s_8_2, 0, 3, 0},
             {2, s_8_3, 0, 3, 0},
             {2, s_8_4, 0, 1, 0},
             {2, s_8_5, 0, 2, 0},
             {2, s_8_6, 0, 3, 0}};

static const symbol s_9_0[2] = {0xE9, 'i'};
static const symbol s_9_1[3] = {0xE1, 0xE9, 'i'};
static const symbol s_9_2[3] = {0xE9, 0xE9, 'i'};
static const symbol s_9_3[1] = {0xE9};
static const symbol s_9_4[2] = {'k', 0xE9};
static const symbol s_9_5[3] = {'a', 'k', 0xE9};
static const symbol s_9_6[3] = {'e', 'k', 0xE9};
static const symbol s_9_7[3] = {'o', 'k', 0xE9};
static const symbol s_9_8[3] = {0xE1, 'k', 0xE9};
static const symbol s_9_9[3] = {0xE9, 'k', 0xE9};
static const symbol s_9_10[3] = {0xF6, 'k', 0xE9};
static const symbol s_9_11[2] = {0xE9, 0xE9};

static const struct among a_9[12] = {
             {2, s_9_0, -1, 1, 0},
             {3, s_9_1, 0, 3, 0},
             {3, s_9_2, 0, 2, 0},
             {1, s_9_3, -1, 1, 0},
             {2, s_9_4, 3, 1, 0},
             {3, s_9_5, 4, 1, 0},
             {3, s_9_6, 4, 1, 0},
             {3, s_9_7, 4, 1, 0},
             {3, s_9_8, 4, 3, 0},
             {3, s_9_9, 4, 2, 0},
             {3, s_9_10, 4, 1, 0},
             {2, s_9_11, 3, 2, 0}};

static const symbol s_10_0[1] = {'a'};
static const symbol s_10_1[2] = {'j', 'a'};
static const symbol s_10_2[1] = {'d'};
static const symbol s_10_3[2] = {'a', 'd'};
static const symbol s_10_4[2] = {'e', 'd'};
static const symbol s_10_5[2] = {'o', 'd'};
static const symbol s_10_6[2] = {0xE1, 'd'};
static const symbol s_10_7[2] = {0xE9, 'd'};
static const symbol s_10_8[2] = {0xF6, 'd'};
static const symbol s_10_9[1] = {'e'};
static const symbol s_10_10[2] = {'j', 'e'};
static const symbol s_10_11[2] = {'n', 'k'};
static const symbol s_10_12[3] = {'u', 'n', 'k'};
static const symbol s_10_13[3] = {0xE1, 'n', 'k'};
static const symbol s_10_14[3] = {0xE9, 'n', 'k'};
static const symbol s_10_15[3] = {0xFC, 'n', 'k'};
static const symbol s_10_16[2] = {'u', 'k'};
static const symbol s_10_17[3] = {'j', 'u', 'k'};
static const symbol s_10_18[4] = {0xE1, 'j', 'u', 'k'};
static const symbol s_10_19[2] = {0xFC, 'k'};
static const symbol s_10_20[3] = {'j', 0xFC, 'k'};
static const symbol s_10_21[4] = {0xE9, 'j', 0xFC, 'k'};
static const symbol s_10_22[1] = {'m'};
static const symbol s_10_23[2] = {'a', 'm'};
static const symbol s_10_24[2] = {'e', 'm'};
static const symbol s_10_25[2] = {'o', 'm'};
static const symbol s_10_26[2] = {0xE1, 'm'};
static const symbol s_10_27[2] = {0xE9, 'm'};
static const symbol s_10_28[1] = {'o'};
static const symbol s_10_29[1] = {0xE1};
static const symbol s_10_30[1] = {0xE9};

static const struct among a_10[31] = {
             {1, s_10_0, -1, 1, 0},
             {2, s_10_1, 0, 1, 0},
             {1, s_10_2, -1, 1, 0},
             {2, s_10_3, 2, 1, 0},
             {2, s_10_4, 2, 1, 0},
             {2, s_10_5, 2, 1, 0},
             {2, s_10_6, 2, 2, 0},
             {2, s_10_7, 2, 3, 0},
             {2, s_10_8, 2, 1, 0},
             {1, s_10_9, -1, 1, 0},
             {2, s_10_10, 9, 1, 0},
             {2, s_10_11, -1, 1, 0},
             {3, s_10_12, 11, 1, 0},
             {3, s_10_13, 11, 2, 0},
             {3, s_10_14, 11, 3, 0},
             {3, s_10_15, 11, 1, 0},
             {2, s_10_16, -1, 1, 0},
             {3, s_10_17, 16, 1, 0},
             {4, s_10_18, 17, 2, 0},
             {2, s_10_19, -1, 1, 0},
             {3, s_10_20, 19, 1, 0},
             {4, s_10_21, 20, 3, 0},
             {1, s_10_22, -1, 1, 0},
             {2, s_10_23, 22, 1, 0},
             {2, s_10_24, 22, 1, 0},
             {2, s_10_25, 22, 1, 0},
             {2, s_10_26, 22, 2, 0},
             {2, s_10_27, 22, 3, 0},
             {1, s_10_28, -1, 1, 0},
             {1, s_10_29, -1, 2, 0},
             {1, s_10_30, -1, 3, 0}};

static const symbol s_11_0[2] = {'i', 'd'};
static const symbol s_11_1[3] = {'a', 'i', 'd'};
static const symbol s_11_2[4] = {'j', 'a', 'i', 'd'};
static const symbol s_11_3[3] = {'e', 'i', 'd'};
static const symbol s_11_4[4] = {'j', 'e', 'i', 'd'};
static const symbol s_11_5[3] = {0xE1, 'i', 'd'};
static const symbol s_11_6[3] = {0xE9, 'i', 'd'};
static const symbol s_11_7[1] = {'i'};
static const symbol s_11_8[2] = {'a', 'i'};
static const symbol s_11_9[3] = {'j', 'a', 'i'};
static const symbol s_11_10[2] = {'e', 'i'};
static const symbol s_11_11[3] = {'j', 'e', 'i'};
static const symbol s_11_12[2] = {0xE1, 'i'};
static const symbol s_11_13[2] = {0xE9, 'i'};
static const symbol s_11_14[4] = {'i', 't', 'e', 'k'};
static const symbol s_11_15[5] = {'e', 'i', 't', 'e', 'k'};
static const symbol s_11_16[6] = {'j', 'e', 'i', 't', 'e', 'k'};
static const symbol s_11_17[5] = {0xE9, 'i', 't', 'e', 'k'};
static const symbol s_11_18[2] = {'i', 'k'};
static const symbol s_11_19[3] = {'a', 'i', 'k'};
static const symbol s_11_20[4] = {'j', 'a', 'i', 'k'};
static const symbol s_11_21[3] = {'e', 'i', 'k'};
static const symbol s_11_22[4] = {'j', 'e', 'i', 'k'};
static const symbol s_11_23[3] = {0xE1, 'i', 'k'};
static const symbol s_11_24[3] = {0xE9, 'i', 'k'};
static const symbol s_11_25[3] = {'i', 'n', 'k'};
static const symbol s_11_26[4] = {'a', 'i', 'n', 'k'};
static const symbol s_11_27[5] = {'j', 'a', 'i', 'n', 'k'};
static const symbol s_11_28[4] = {'e', 'i', 'n', 'k'};
static const symbol s_11_29[5] = {'j', 'e', 'i', 'n', 'k'};
static const symbol s_11_30[4] = {0xE1, 'i', 'n', 'k'};
static const symbol s_11_31[4] = {0xE9, 'i', 'n', 'k'};
static const symbol s_11_32[5] = {'a', 'i', 't', 'o', 'k'};
static const symbol s_11_33[6] = {'j', 'a', 'i', 't', 'o', 'k'};
static const symbol s_11_34[5] = {0xE1, 'i', 't', 'o', 'k'};
static const symbol s_11_35[2] = {'i', 'm'};
static const symbol s_11_36[3] = {'a', 'i', 'm'};
static const symbol s_11_37[4] = {'j', 'a', 'i', 'm'};
static const symbol s_11_38[3] = {'e', 'i', 'm'};
static const symbol s_11_39[4] = {'j', 'e', 'i', 'm'};
static const symbol s_11_40[3] = {0xE1, 'i', 'm'};
static const symbol s_11_41[3] = {0xE9, 'i', 'm'};

static const struct among a_11[42] = {
             {2, s_11_0, -1, 1, 0},
             {3, s_11_1, 0, 1, 0},
             {4, s_11_2, 1, 1, 0},
             {3, s_11_3, 0, 1, 0},
             {4, s_11_4, 3, 1, 0},
             {3, s_11_5, 0, 2, 0},
             {3, s_11_6, 0, 3, 0},
             {1, s_11_7, -1, 1, 0},
             {2, s_11_8, 7, 1, 0},
             {3, s_11_9, 8, 1, 0},
             {2, s_11_10, 7, 1, 0},
             {3, s_11_11, 10, 1, 0},
             {2, s_11_12, 7, 2, 0},
             {2, s_11_13, 7, 3, 0},
             {4, s_11_14, -1, 1, 0},
             {5, s_11_15, 14, 1, 0},
             {6, s_11_16, 15, 1, 0},
             {5, s_11_17, 14, 3, 0},
             {2, s_11_18, -1, 1, 0},
             {3, s_11_19, 18, 1, 0},
             {4, s_11_20, 19, 1, 0},
             {3, s_11_21, 18, 1, 0},
             {4, s_11_22, 21, 1, 0},
             {3, s_11_23, 18, 2, 0},
             {3, s_11_24, 18, 3, 0},
             {3, s_11_25, -1, 1, 0},
             {4, s_11_26, 25, 1, 0},
             {5, s_11_27, 26, 1, 0},
             {4, s_11_28, 25, 1, 0},
             {5, s_11_29, 28, 1, 0},
             {4, s_11_30, 25, 2, 0},
             {4, s_11_31, 25, 3, 0},
             {5, s_11_32, -1, 1, 0},
             {6, s_11_33, 32, 1, 0},
             {5, s_11_34, -1, 2, 0},
             {2, s_11_35, -1, 1, 0},
             {3, s_11_36, 35, 1, 0},
             {4, s_11_37, 36, 1, 0},
             {3, s_11_38, 35, 1, 0},
             {4, s_11_39, 38, 1, 0},
             {3, s_11_40, 35, 2, 0},
             {3, s_11_41, 35, 3, 0}};

static const unsigned char g_v[] = {17, 65, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 17, 52, 14};

static const symbol s_0[] = {'a'};
static const symbol s_1[] = {'e'};
static const symbol s_2[] = {'e'};
static const symbol s_3[] = {'a'};
static const symbol s_4[] = {'a'};
static const symbol s_5[] = {'e'};
static const symbol s_6[] = {'a'};
static const symbol s_7[] = {'e'};
static const symbol s_8[] = {'e'};
static const symbol s_9[] = {'a'};
static const symbol s_10[] = {'a'};
static const symbol s_11[] = {'e'};
static const symbol s_12[] = {'a'};
static const symbol s_13[] = {'e'};

static int
r_mark_regions(struct SN_env *z)
{                                  
  z->I[0] = z->l;                                          
  {
    int c1 = z->c;                  
    if (in_grouping(z, g_v, 97, 252, 0))
    {
      goto lab1;                          
    }
    if (in_grouping(z, g_v, 97, 252, 1) < 0)
    {
      goto lab1;            /* non v, line 48 */
    }
    {
      int c2 = z->c;                  
      if (z->c + 1 >= z->l || z->p[z->c + 1] >> 5 != 3 || !((101187584 >> (z->p[z->c + 1] & 0x1f)) & 1))
      {
        goto lab3;                     
      }
      if (!(find_among(z, a_0, 8)))
      {
        goto lab3;
      }
      goto lab2;
    lab3:
      z->c = c2;
      if (z->c >= z->l)
      {
        goto lab1;
      }
      z->c++;                    
    }
  lab2:
    z->I[0] = z->c;                          
    goto lab0;
  lab1:
    z->c = c1;
    if (out_grouping(z, g_v, 97, 252, 0))
    {
      return 0;                     
    }
    {              /* grouping v, line 53 */
      int ret = out_grouping(z, g_v, 97, 252, 1);
      if (ret < 0)
      {
        return 0;
      }
      z->c += ret;
    }
    z->I[0] = z->c;                          
  }
lab0:
  return 1;
}

static int
r_R1(struct SN_env *z)
{                   
  if (!(z->I[0] <= z->c))
  {
    return 0;                                                               
  }
  return 1;
}

static int
r_v_ending(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                 
  if (z->c <= z->lb || (z->p[z->c - 1] != 225 && z->p[z->c - 1] != 233))
  {
    return 0;                         
  }
  among_var = find_among_b(z, a_1, 2);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                 
  {
    int ret = r_R1(z);                       
    if (ret <= 0)
    {
      return ret;
    }
  }
  switch (among_var)
  {                     
  case 1:
  {
    int ret = slice_from_s(z, 1, s_0);                  
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 2:
  {
    int ret = slice_from_s(z, 1, s_1);                  
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
r_double(struct SN_env *z)
{                   
  {
    int m_test1 = z->l - z->c;                    
    if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((106790108 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      return 0;                     
    }
    if (!(find_among_b(z, a_2, 23)))
    {
      return 0;
    }
    z->c = z->l - m_test1;
  }
  return 1;
}

static int
r_undouble(struct SN_env *z)
{                   
  if (z->c <= z->lb)
  {
    return 0;
  }
  z->c--;                           
  z->ket = z->c;                 
  {
    int ret = z->c - 1;                   
    if (z->lb > ret || ret > z->l)
    {
      return 0;
    }
    z->c = ret;
  }
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
r_instrum(struct SN_env *z)
{                                  
  z->ket = z->c;                 
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 108)
  {
    return 0;                         
  }
  if (!(find_among_b(z, a_3, 2)))
  {
    return 0;
  }
  z->bra = z->c;                 
  {
    int ret = r_R1(z);                       
    if (ret <= 0)
    {
      return ret;
    }
  }
  {
    int ret = r_double(z);                           
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
    int ret = r_undouble(z);                             
    if (ret <= 0)
    {
      return ret;
    }
  }
  return 1;
}

static int
r_case(struct SN_env *z)
{                                  
  z->ket = z->c;                 
  if (!(find_among_b(z, a_4, 44)))
  {
    return 0;                         
  }
  z->bra = z->c;                 
  {
    int ret = r_R1(z);                       
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
    int ret = r_v_ending(z);                              
    if (ret <= 0)
    {
      return ret;
    }
  }
  return 1;
}

static int
r_case_special(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || (z->p[z->c - 1] != 110 && z->p[z->c - 1] != 116))
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_5, 3);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
  switch (among_var)
  {                      
  case 1:
  {
    int ret = slice_from_s(z, 1, s_2);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 2:
  {
    int ret = slice_from_s(z, 1, s_3);                   
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
r_case_other(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c - 3 <= z->lb || z->p[z->c - 1] != 108)
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_6, 6);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
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
  {
    int ret = slice_from_s(z, 1, s_4);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 3:
  {
    int ret = slice_from_s(z, 1, s_5);                   
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
r_factive(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c <= z->lb || (z->p[z->c - 1] != 225 && z->p[z->c - 1] != 233))
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_7, 2)))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
  {
    int ret = r_double(z);                            
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
    int ret = r_undouble(z);                              
    if (ret <= 0)
    {
      return ret;
    }
  }
  return 1;
}

static int
r_plural(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c <= z->lb || z->p[z->c - 1] != 107)
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_8, 7);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
  }
  switch (among_var)
  {                      
  case 1:
  {
    int ret = slice_from_s(z, 1, s_6);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 2:
  {
    int ret = slice_from_s(z, 1, s_7);                   
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
  }
  return 1;
}

static int
r_owned(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c <= z->lb || (z->p[z->c - 1] != 105 && z->p[z->c - 1] != 233))
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_9, 12);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
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
  {
    int ret = slice_from_s(z, 1, s_8);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 3:
  {
    int ret = slice_from_s(z, 1, s_9);                   
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
r_sing_owner(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                                          
  among_var = find_among_b(z, a_10, 31);                          
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
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
  {
    int ret = slice_from_s(z, 1, s_10);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 3:
  {
    int ret = slice_from_s(z, 1, s_11);                   
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
r_plur_owner(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c <= z->lb || z->p[z->c - 1] >> 5 != 3 || !((10768 >> (z->p[z->c - 1] & 0x1f)) & 1))
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_11, 42);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = r_R1(z);                        
    if (ret <= 0)
    {
      return ret;
    }
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
  {
    int ret = slice_from_s(z, 1, s_12);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  case 3:
  {
    int ret = slice_from_s(z, 1, s_13);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  break;
  }
  return 1;
}

extern int
hungarian_ISO_8859_2_stem(struct SN_env *z)
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
      int ret = r_instrum(z);                             
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
      int ret = r_case(z);                          
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
      int ret = r_case_special(z);                                  
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
  {
    int m5 = z->l - z->c;
    (void)m5;                   
    {
      int ret = r_case_other(z);                                
      if (ret == 0)
      {
        goto lab4;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab4:
    z->c = z->l - m5;
  }
  {
    int m6 = z->l - z->c;
    (void)m6;                   
    {
      int ret = r_factive(z);                             
      if (ret == 0)
      {
        goto lab5;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab5:
    z->c = z->l - m6;
  }
  {
    int m7 = z->l - z->c;
    (void)m7;                   
    {
      int ret = r_owned(z);                           
      if (ret == 0)
      {
        goto lab6;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab6:
    z->c = z->l - m7;
  }
  {
    int m8 = z->l - z->c;
    (void)m8;                   
    {
      int ret = r_sing_owner(z);                                
      if (ret == 0)
      {
        goto lab7;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab7:
    z->c = z->l - m8;
  }
  {
    int m9 = z->l - z->c;
    (void)m9;                   
    {
      int ret = r_plur_owner(z);                                
      if (ret == 0)
      {
        goto lab8;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab8:
    z->c = z->l - m9;
  }
  {
    int m10 = z->l - z->c;
    (void)m10;                   
    {
      int ret = r_plural(z);                            
      if (ret == 0)
      {
        goto lab9;
      }
      if (ret < 0)
      {
        return ret;
      }
    }
  lab9:
    z->c = z->l - m10;
  }
  z->c = z->lb;
  return 1;
}

extern struct SN_env *
hungarian_ISO_8859_2_create_env(void)
{
  return SN_create_env(0, 1, 0);
}

extern void
hungarian_ISO_8859_2_close_env(struct SN_env *z)
{
  SN_close_env(z, 0);
}
