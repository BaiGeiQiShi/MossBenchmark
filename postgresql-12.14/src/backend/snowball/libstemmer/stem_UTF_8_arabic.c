                                                                             
                              

#include "header.h"

#ifdef __cplusplus
extern "C"
{
#endif
  extern int
  arabic_UTF_8_stem(struct SN_env *z);
#ifdef __cplusplus
}
#endif
static int
r_Checks1(struct SN_env *z);
static int
r_Normalize_pre(struct SN_env *z);
static int
r_Normalize_post(struct SN_env *z);
static int
r_Suffix_Verb_Step2c(struct SN_env *z);
static int
r_Suffix_Verb_Step2b(struct SN_env *z);
static int
r_Suffix_Verb_Step2a(struct SN_env *z);
static int
r_Suffix_Verb_Step1(struct SN_env *z);
static int
r_Suffix_Noun_Step3(struct SN_env *z);
static int
r_Suffix_Noun_Step2c2(struct SN_env *z);
static int
r_Suffix_Noun_Step2c1(struct SN_env *z);
static int
r_Suffix_Noun_Step2b(struct SN_env *z);
static int
r_Suffix_Noun_Step2a(struct SN_env *z);
static int
r_Suffix_Noun_Step1b(struct SN_env *z);
static int
r_Suffix_Noun_Step1a(struct SN_env *z);
static int
r_Suffix_All_alef_maqsura(struct SN_env *z);
static int
r_Prefix_Step4_Verb(struct SN_env *z);
static int
r_Prefix_Step3_Verb(struct SN_env *z);
static int
r_Prefix_Step3b_Noun(struct SN_env *z);
static int
r_Prefix_Step3a_Noun(struct SN_env *z);
static int
r_Prefix_Step2(struct SN_env *z);
static int
r_Prefix_Step1(struct SN_env *z);
#ifdef __cplusplus
extern "C"
{
#endif

  extern struct SN_env *
  arabic_UTF_8_create_env(void);
  extern void
  arabic_UTF_8_close_env(struct SN_env *z);

#ifdef __cplusplus
}
#endif
static const symbol s_0_0[2] = {0xD9, 0x80};
static const symbol s_0_1[2] = {0xD9, 0x8B};
static const symbol s_0_2[2] = {0xD9, 0x8C};
static const symbol s_0_3[2] = {0xD9, 0x8D};
static const symbol s_0_4[2] = {0xD9, 0x8E};
static const symbol s_0_5[2] = {0xD9, 0x8F};
static const symbol s_0_6[2] = {0xD9, 0x90};
static const symbol s_0_7[2] = {0xD9, 0x91};
static const symbol s_0_8[2] = {0xD9, 0x92};
static const symbol s_0_9[2] = {0xD9, 0xA0};
static const symbol s_0_10[2] = {0xD9, 0xA1};
static const symbol s_0_11[2] = {0xD9, 0xA2};
static const symbol s_0_12[2] = {0xD9, 0xA3};
static const symbol s_0_13[2] = {0xD9, 0xA4};
static const symbol s_0_14[2] = {0xD9, 0xA5};
static const symbol s_0_15[2] = {0xD9, 0xA6};
static const symbol s_0_16[2] = {0xD9, 0xA7};
static const symbol s_0_17[2] = {0xD9, 0xA8};
static const symbol s_0_18[2] = {0xD9, 0xA9};
static const symbol s_0_19[3] = {0xEF, 0xBA, 0x80};
static const symbol s_0_20[3] = {0xEF, 0xBA, 0x81};
static const symbol s_0_21[3] = {0xEF, 0xBA, 0x82};
static const symbol s_0_22[3] = {0xEF, 0xBA, 0x83};
static const symbol s_0_23[3] = {0xEF, 0xBA, 0x84};
static const symbol s_0_24[3] = {0xEF, 0xBA, 0x85};
static const symbol s_0_25[3] = {0xEF, 0xBA, 0x86};
static const symbol s_0_26[3] = {0xEF, 0xBA, 0x87};
static const symbol s_0_27[3] = {0xEF, 0xBA, 0x88};
static const symbol s_0_28[3] = {0xEF, 0xBA, 0x89};
static const symbol s_0_29[3] = {0xEF, 0xBA, 0x8A};
static const symbol s_0_30[3] = {0xEF, 0xBA, 0x8B};
static const symbol s_0_31[3] = {0xEF, 0xBA, 0x8C};
static const symbol s_0_32[3] = {0xEF, 0xBA, 0x8D};
static const symbol s_0_33[3] = {0xEF, 0xBA, 0x8E};
static const symbol s_0_34[3] = {0xEF, 0xBA, 0x8F};
static const symbol s_0_35[3] = {0xEF, 0xBA, 0x90};
static const symbol s_0_36[3] = {0xEF, 0xBA, 0x91};
static const symbol s_0_37[3] = {0xEF, 0xBA, 0x92};
static const symbol s_0_38[3] = {0xEF, 0xBA, 0x93};
static const symbol s_0_39[3] = {0xEF, 0xBA, 0x94};
static const symbol s_0_40[3] = {0xEF, 0xBA, 0x95};
static const symbol s_0_41[3] = {0xEF, 0xBA, 0x96};
static const symbol s_0_42[3] = {0xEF, 0xBA, 0x97};
static const symbol s_0_43[3] = {0xEF, 0xBA, 0x98};
static const symbol s_0_44[3] = {0xEF, 0xBA, 0x99};
static const symbol s_0_45[3] = {0xEF, 0xBA, 0x9A};
static const symbol s_0_46[3] = {0xEF, 0xBA, 0x9B};
static const symbol s_0_47[3] = {0xEF, 0xBA, 0x9C};
static const symbol s_0_48[3] = {0xEF, 0xBA, 0x9D};
static const symbol s_0_49[3] = {0xEF, 0xBA, 0x9E};
static const symbol s_0_50[3] = {0xEF, 0xBA, 0x9F};
static const symbol s_0_51[3] = {0xEF, 0xBA, 0xA0};
static const symbol s_0_52[3] = {0xEF, 0xBA, 0xA1};
static const symbol s_0_53[3] = {0xEF, 0xBA, 0xA2};
static const symbol s_0_54[3] = {0xEF, 0xBA, 0xA3};
static const symbol s_0_55[3] = {0xEF, 0xBA, 0xA4};
static const symbol s_0_56[3] = {0xEF, 0xBA, 0xA5};
static const symbol s_0_57[3] = {0xEF, 0xBA, 0xA6};
static const symbol s_0_58[3] = {0xEF, 0xBA, 0xA7};
static const symbol s_0_59[3] = {0xEF, 0xBA, 0xA8};
static const symbol s_0_60[3] = {0xEF, 0xBA, 0xA9};
static const symbol s_0_61[3] = {0xEF, 0xBA, 0xAA};
static const symbol s_0_62[3] = {0xEF, 0xBA, 0xAB};
static const symbol s_0_63[3] = {0xEF, 0xBA, 0xAC};
static const symbol s_0_64[3] = {0xEF, 0xBA, 0xAD};
static const symbol s_0_65[3] = {0xEF, 0xBA, 0xAE};
static const symbol s_0_66[3] = {0xEF, 0xBA, 0xAF};
static const symbol s_0_67[3] = {0xEF, 0xBA, 0xB0};
static const symbol s_0_68[3] = {0xEF, 0xBA, 0xB1};
static const symbol s_0_69[3] = {0xEF, 0xBA, 0xB2};
static const symbol s_0_70[3] = {0xEF, 0xBA, 0xB3};
static const symbol s_0_71[3] = {0xEF, 0xBA, 0xB4};
static const symbol s_0_72[3] = {0xEF, 0xBA, 0xB5};
static const symbol s_0_73[3] = {0xEF, 0xBA, 0xB6};
static const symbol s_0_74[3] = {0xEF, 0xBA, 0xB7};
static const symbol s_0_75[3] = {0xEF, 0xBA, 0xB8};
static const symbol s_0_76[3] = {0xEF, 0xBA, 0xB9};
static const symbol s_0_77[3] = {0xEF, 0xBA, 0xBA};
static const symbol s_0_78[3] = {0xEF, 0xBA, 0xBB};
static const symbol s_0_79[3] = {0xEF, 0xBA, 0xBC};
static const symbol s_0_80[3] = {0xEF, 0xBA, 0xBD};
static const symbol s_0_81[3] = {0xEF, 0xBA, 0xBE};
static const symbol s_0_82[3] = {0xEF, 0xBA, 0xBF};
static const symbol s_0_83[3] = {0xEF, 0xBB, 0x80};
static const symbol s_0_84[3] = {0xEF, 0xBB, 0x81};
static const symbol s_0_85[3] = {0xEF, 0xBB, 0x82};
static const symbol s_0_86[3] = {0xEF, 0xBB, 0x83};
static const symbol s_0_87[3] = {0xEF, 0xBB, 0x84};
static const symbol s_0_88[3] = {0xEF, 0xBB, 0x85};
static const symbol s_0_89[3] = {0xEF, 0xBB, 0x86};
static const symbol s_0_90[3] = {0xEF, 0xBB, 0x87};
static const symbol s_0_91[3] = {0xEF, 0xBB, 0x88};
static const symbol s_0_92[3] = {0xEF, 0xBB, 0x89};
static const symbol s_0_93[3] = {0xEF, 0xBB, 0x8A};
static const symbol s_0_94[3] = {0xEF, 0xBB, 0x8B};
static const symbol s_0_95[3] = {0xEF, 0xBB, 0x8C};
static const symbol s_0_96[3] = {0xEF, 0xBB, 0x8D};
static const symbol s_0_97[3] = {0xEF, 0xBB, 0x8E};
static const symbol s_0_98[3] = {0xEF, 0xBB, 0x8F};
static const symbol s_0_99[3] = {0xEF, 0xBB, 0x90};
static const symbol s_0_100[3] = {0xEF, 0xBB, 0x91};
static const symbol s_0_101[3] = {0xEF, 0xBB, 0x92};
static const symbol s_0_102[3] = {0xEF, 0xBB, 0x93};
static const symbol s_0_103[3] = {0xEF, 0xBB, 0x94};
static const symbol s_0_104[3] = {0xEF, 0xBB, 0x95};
static const symbol s_0_105[3] = {0xEF, 0xBB, 0x96};
static const symbol s_0_106[3] = {0xEF, 0xBB, 0x97};
static const symbol s_0_107[3] = {0xEF, 0xBB, 0x98};
static const symbol s_0_108[3] = {0xEF, 0xBB, 0x99};
static const symbol s_0_109[3] = {0xEF, 0xBB, 0x9A};
static const symbol s_0_110[3] = {0xEF, 0xBB, 0x9B};
static const symbol s_0_111[3] = {0xEF, 0xBB, 0x9C};
static const symbol s_0_112[3] = {0xEF, 0xBB, 0x9D};
static const symbol s_0_113[3] = {0xEF, 0xBB, 0x9E};
static const symbol s_0_114[3] = {0xEF, 0xBB, 0x9F};
static const symbol s_0_115[3] = {0xEF, 0xBB, 0xA0};
static const symbol s_0_116[3] = {0xEF, 0xBB, 0xA1};
static const symbol s_0_117[3] = {0xEF, 0xBB, 0xA2};
static const symbol s_0_118[3] = {0xEF, 0xBB, 0xA3};
static const symbol s_0_119[3] = {0xEF, 0xBB, 0xA4};
static const symbol s_0_120[3] = {0xEF, 0xBB, 0xA5};
static const symbol s_0_121[3] = {0xEF, 0xBB, 0xA6};
static const symbol s_0_122[3] = {0xEF, 0xBB, 0xA7};
static const symbol s_0_123[3] = {0xEF, 0xBB, 0xA8};
static const symbol s_0_124[3] = {0xEF, 0xBB, 0xA9};
static const symbol s_0_125[3] = {0xEF, 0xBB, 0xAA};
static const symbol s_0_126[3] = {0xEF, 0xBB, 0xAB};
static const symbol s_0_127[3] = {0xEF, 0xBB, 0xAC};
static const symbol s_0_128[3] = {0xEF, 0xBB, 0xAD};
static const symbol s_0_129[3] = {0xEF, 0xBB, 0xAE};
static const symbol s_0_130[3] = {0xEF, 0xBB, 0xAF};
static const symbol s_0_131[3] = {0xEF, 0xBB, 0xB0};
static const symbol s_0_132[3] = {0xEF, 0xBB, 0xB1};
static const symbol s_0_133[3] = {0xEF, 0xBB, 0xB2};
static const symbol s_0_134[3] = {0xEF, 0xBB, 0xB3};
static const symbol s_0_135[3] = {0xEF, 0xBB, 0xB4};
static const symbol s_0_136[3] = {0xEF, 0xBB, 0xB5};
static const symbol s_0_137[3] = {0xEF, 0xBB, 0xB6};
static const symbol s_0_138[3] = {0xEF, 0xBB, 0xB7};
static const symbol s_0_139[3] = {0xEF, 0xBB, 0xB8};
static const symbol s_0_140[3] = {0xEF, 0xBB, 0xB9};
static const symbol s_0_141[3] = {0xEF, 0xBB, 0xBA};
static const symbol s_0_142[3] = {0xEF, 0xBB, 0xBB};
static const symbol s_0_143[3] = {0xEF, 0xBB, 0xBC};

static const struct among a_0[144] = {
             {2, s_0_0, -1, 1, 0},
             {2, s_0_1, -1, 1, 0},
             {2, s_0_2, -1, 1, 0},
             {2, s_0_3, -1, 1, 0},
             {2, s_0_4, -1, 1, 0},
             {2, s_0_5, -1, 1, 0},
             {2, s_0_6, -1, 1, 0},
             {2, s_0_7, -1, 1, 0},
             {2, s_0_8, -1, 1, 0},
             {2, s_0_9, -1, 2, 0},
             {2, s_0_10, -1, 3, 0},
             {2, s_0_11, -1, 4, 0},
             {2, s_0_12, -1, 5, 0},
             {2, s_0_13, -1, 6, 0},
             {2, s_0_14, -1, 7, 0},
             {2, s_0_15, -1, 8, 0},
             {2, s_0_16, -1, 9, 0},
             {2, s_0_17, -1, 10, 0},
             {2, s_0_18, -1, 11, 0},
             {3, s_0_19, -1, 12, 0},
             {3, s_0_20, -1, 16, 0},
             {3, s_0_21, -1, 16, 0},
             {3, s_0_22, -1, 13, 0},
             {3, s_0_23, -1, 13, 0},
             {3, s_0_24, -1, 17, 0},
             {3, s_0_25, -1, 17, 0},
             {3, s_0_26, -1, 14, 0},
             {3, s_0_27, -1, 14, 0},
             {3, s_0_28, -1, 15, 0},
             {3, s_0_29, -1, 15, 0},
             {3, s_0_30, -1, 15, 0},
             {3, s_0_31, -1, 15, 0},
             {3, s_0_32, -1, 18, 0},
             {3, s_0_33, -1, 18, 0},
             {3, s_0_34, -1, 19, 0},
             {3, s_0_35, -1, 19, 0},
             {3, s_0_36, -1, 19, 0},
             {3, s_0_37, -1, 19, 0},
             {3, s_0_38, -1, 20, 0},
             {3, s_0_39, -1, 20, 0},
             {3, s_0_40, -1, 21, 0},
             {3, s_0_41, -1, 21, 0},
             {3, s_0_42, -1, 21, 0},
             {3, s_0_43, -1, 21, 0},
             {3, s_0_44, -1, 22, 0},
             {3, s_0_45, -1, 22, 0},
             {3, s_0_46, -1, 22, 0},
             {3, s_0_47, -1, 22, 0},
             {3, s_0_48, -1, 23, 0},
             {3, s_0_49, -1, 23, 0},
             {3, s_0_50, -1, 23, 0},
             {3, s_0_51, -1, 23, 0},
             {3, s_0_52, -1, 24, 0},
             {3, s_0_53, -1, 24, 0},
             {3, s_0_54, -1, 24, 0},
             {3, s_0_55, -1, 24, 0},
             {3, s_0_56, -1, 25, 0},
             {3, s_0_57, -1, 25, 0},
             {3, s_0_58, -1, 25, 0},
             {3, s_0_59, -1, 25, 0},
             {3, s_0_60, -1, 26, 0},
             {3, s_0_61, -1, 26, 0},
             {3, s_0_62, -1, 27, 0},
             {3, s_0_63, -1, 27, 0},
             {3, s_0_64, -1, 28, 0},
             {3, s_0_65, -1, 28, 0},
             {3, s_0_66, -1, 29, 0},
             {3, s_0_67, -1, 29, 0},
             {3, s_0_68, -1, 30, 0},
             {3, s_0_69, -1, 30, 0},
             {3, s_0_70, -1, 30, 0},
             {3, s_0_71, -1, 30, 0},
             {3, s_0_72, -1, 31, 0},
             {3, s_0_73, -1, 31, 0},
             {3, s_0_74, -1, 31, 0},
             {3, s_0_75, -1, 31, 0},
             {3, s_0_76, -1, 32, 0},
             {3, s_0_77, -1, 32, 0},
             {3, s_0_78, -1, 32, 0},
             {3, s_0_79, -1, 32, 0},
             {3, s_0_80, -1, 33, 0},
             {3, s_0_81, -1, 33, 0},
             {3, s_0_82, -1, 33, 0},
             {3, s_0_83, -1, 33, 0},
             {3, s_0_84, -1, 34, 0},
             {3, s_0_85, -1, 34, 0},
             {3, s_0_86, -1, 34, 0},
             {3, s_0_87, -1, 34, 0},
             {3, s_0_88, -1, 35, 0},
             {3, s_0_89, -1, 35, 0},
             {3, s_0_90, -1, 35, 0},
             {3, s_0_91, -1, 35, 0},
             {3, s_0_92, -1, 36, 0},
             {3, s_0_93, -1, 36, 0},
             {3, s_0_94, -1, 36, 0},
             {3, s_0_95, -1, 36, 0},
             {3, s_0_96, -1, 37, 0},
             {3, s_0_97, -1, 37, 0},
             {3, s_0_98, -1, 37, 0},
             {3, s_0_99, -1, 37, 0},
             {3, s_0_100, -1, 38, 0},
             {3, s_0_101, -1, 38, 0},
             {3, s_0_102, -1, 38, 0},
             {3, s_0_103, -1, 38, 0},
             {3, s_0_104, -1, 39, 0},
             {3, s_0_105, -1, 39, 0},
             {3, s_0_106, -1, 39, 0},
             {3, s_0_107, -1, 39, 0},
             {3, s_0_108, -1, 40, 0},
             {3, s_0_109, -1, 40, 0},
             {3, s_0_110, -1, 40, 0},
             {3, s_0_111, -1, 40, 0},
             {3, s_0_112, -1, 41, 0},
             {3, s_0_113, -1, 41, 0},
             {3, s_0_114, -1, 41, 0},
             {3, s_0_115, -1, 41, 0},
             {3, s_0_116, -1, 42, 0},
             {3, s_0_117, -1, 42, 0},
             {3, s_0_118, -1, 42, 0},
             {3, s_0_119, -1, 42, 0},
             {3, s_0_120, -1, 43, 0},
             {3, s_0_121, -1, 43, 0},
             {3, s_0_122, -1, 43, 0},
             {3, s_0_123, -1, 43, 0},
             {3, s_0_124, -1, 44, 0},
             {3, s_0_125, -1, 44, 0},
             {3, s_0_126, -1, 44, 0},
             {3, s_0_127, -1, 44, 0},
             {3, s_0_128, -1, 45, 0},
             {3, s_0_129, -1, 45, 0},
             {3, s_0_130, -1, 46, 0},
             {3, s_0_131, -1, 46, 0},
             {3, s_0_132, -1, 47, 0},
             {3, s_0_133, -1, 47, 0},
             {3, s_0_134, -1, 47, 0},
             {3, s_0_135, -1, 47, 0},
             {3, s_0_136, -1, 51, 0},
             {3, s_0_137, -1, 51, 0},
             {3, s_0_138, -1, 49, 0},
             {3, s_0_139, -1, 49, 0},
             {3, s_0_140, -1, 50, 0},
             {3, s_0_141, -1, 50, 0},
             {3, s_0_142, -1, 48, 0},
             {3, s_0_143, -1, 48, 0}};

static const symbol s_1_0[2] = {0xD8, 0xA2};
static const symbol s_1_1[2] = {0xD8, 0xA3};
static const symbol s_1_2[2] = {0xD8, 0xA4};
static const symbol s_1_3[2] = {0xD8, 0xA5};
static const symbol s_1_4[2] = {0xD8, 0xA6};

static const struct among a_1[5] = {
             {2, s_1_0, -1, 1, 0},
             {2, s_1_1, -1, 1, 0},
             {2, s_1_2, -1, 1, 0},
             {2, s_1_3, -1, 1, 0},
             {2, s_1_4, -1, 1, 0}};

static const symbol s_2_0[2] = {0xD8, 0xA2};
static const symbol s_2_1[2] = {0xD8, 0xA3};
static const symbol s_2_2[2] = {0xD8, 0xA4};
static const symbol s_2_3[2] = {0xD8, 0xA5};
static const symbol s_2_4[2] = {0xD8, 0xA6};

static const struct among a_2[5] = {
             {2, s_2_0, -1, 1, 0},
             {2, s_2_1, -1, 1, 0},
             {2, s_2_2, -1, 2, 0},
             {2, s_2_3, -1, 1, 0},
             {2, s_2_4, -1, 3, 0}};

static const symbol s_3_0[4] = {0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_3_1[6] = {0xD8, 0xA8, 0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_3_2[6] = {0xD9, 0x83, 0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_3_3[4] = {0xD9, 0x84, 0xD9, 0x84};

static const struct among a_3[4] = {
             {4, s_3_0, -1, 2, 0},
             {6, s_3_1, -1, 1, 0},
             {6, s_3_2, -1, 1, 0},
             {4, s_3_3, -1, 2, 0}};

static const symbol s_4_0[4] = {0xD8, 0xA3, 0xD8, 0xA2};
static const symbol s_4_1[4] = {0xD8, 0xA3, 0xD8, 0xA3};
static const symbol s_4_2[4] = {0xD8, 0xA3, 0xD8, 0xA4};
static const symbol s_4_3[4] = {0xD8, 0xA3, 0xD8, 0xA5};
static const symbol s_4_4[4] = {0xD8, 0xA3, 0xD8, 0xA7};

static const struct among a_4[5] = {
             {4, s_4_0, -1, 2, 0},
             {4, s_4_1, -1, 1, 0},
             {4, s_4_2, -1, 1, 0},
             {4, s_4_3, -1, 4, 0},
             {4, s_4_4, -1, 3, 0}};

static const symbol s_5_0[2] = {0xD9, 0x81};
static const symbol s_5_1[2] = {0xD9, 0x88};

static const struct among a_5[2] = {
             {2, s_5_0, -1, 1, 0},
             {2, s_5_1, -1, 1, 0}};

static const symbol s_6_0[4] = {0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_6_1[6] = {0xD8, 0xA8, 0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_6_2[6] = {0xD9, 0x83, 0xD8, 0xA7, 0xD9, 0x84};
static const symbol s_6_3[4] = {0xD9, 0x84, 0xD9, 0x84};

static const struct among a_6[4] = {
             {4, s_6_0, -1, 2, 0},
             {6, s_6_1, -1, 1, 0},
             {6, s_6_2, -1, 1, 0},
             {4, s_6_3, -1, 2, 0}};

static const symbol s_7_0[2] = {0xD8, 0xA8};
static const symbol s_7_1[4] = {0xD8, 0xA8, 0xD8, 0xA8};
static const symbol s_7_2[4] = {0xD9, 0x83, 0xD9, 0x83};

static const struct among a_7[3] = {
             {2, s_7_0, -1, 1, 0},
             {4, s_7_1, 0, 2, 0},
             {4, s_7_2, -1, 3, 0}};

static const symbol s_8_0[4] = {0xD8, 0xB3, 0xD8, 0xA3};
static const symbol s_8_1[4] = {0xD8, 0xB3, 0xD8, 0xAA};
static const symbol s_8_2[4] = {0xD8, 0xB3, 0xD9, 0x86};
static const symbol s_8_3[4] = {0xD8, 0xB3, 0xD9, 0x8A};

static const struct among a_8[4] = {
             {4, s_8_0, -1, 4, 0},
             {4, s_8_1, -1, 2, 0},
             {4, s_8_2, -1, 3, 0},
             {4, s_8_3, -1, 1, 0}};

static const symbol s_9_0[6] = {0xD8, 0xAA, 0xD8, 0xB3, 0xD8, 0xAA};
static const symbol s_9_1[6] = {0xD9, 0x86, 0xD8, 0xB3, 0xD8, 0xAA};
static const symbol s_9_2[6] = {0xD9, 0x8A, 0xD8, 0xB3, 0xD8, 0xAA};

static const struct among a_9[3] = {
             {6, s_9_0, -1, 1, 0},
             {6, s_9_1, -1, 1, 0},
             {6, s_9_2, -1, 1, 0}};

static const symbol s_10_0[2] = {0xD9, 0x83};
static const symbol s_10_1[4] = {0xD9, 0x83, 0xD9, 0x85};
static const symbol s_10_2[4] = {0xD9, 0x87, 0xD9, 0x85};
static const symbol s_10_3[4] = {0xD9, 0x87, 0xD9, 0x86};
static const symbol s_10_4[2] = {0xD9, 0x87};
static const symbol s_10_5[2] = {0xD9, 0x8A};
static const symbol s_10_6[6] = {0xD9, 0x83, 0xD9, 0x85, 0xD8, 0xA7};
static const symbol s_10_7[6] = {0xD9, 0x87, 0xD9, 0x85, 0xD8, 0xA7};
static const symbol s_10_8[4] = {0xD9, 0x86, 0xD8, 0xA7};
static const symbol s_10_9[4] = {0xD9, 0x87, 0xD8, 0xA7};

static const struct among a_10[10] = {
             {2, s_10_0, -1, 1, 0},
             {4, s_10_1, -1, 2, 0},
             {4, s_10_2, -1, 2, 0},
             {4, s_10_3, -1, 2, 0},
             {2, s_10_4, -1, 1, 0},
             {2, s_10_5, -1, 1, 0},
             {6, s_10_6, -1, 3, 0},
             {6, s_10_7, -1, 3, 0},
             {4, s_10_8, -1, 2, 0},
             {4, s_10_9, -1, 2, 0}};

static const symbol s_11_0[2] = {0xD9, 0x86};

static const struct among a_11[1] = {
             {2, s_11_0, -1, 1, 0}};

static const symbol s_12_0[2] = {0xD9, 0x88};
static const symbol s_12_1[2] = {0xD9, 0x8A};
static const symbol s_12_2[2] = {0xD8, 0xA7};

static const struct among a_12[3] = {
             {2, s_12_0, -1, 1, 0},
             {2, s_12_1, -1, 1, 0},
             {2, s_12_2, -1, 1, 0}};

static const symbol s_13_0[4] = {0xD8, 0xA7, 0xD8, 0xAA};

static const struct among a_13[1] = {
             {4, s_13_0, -1, 1, 0}};

static const symbol s_14_0[2] = {0xD8, 0xAA};

static const struct among a_14[1] = {
             {2, s_14_0, -1, 1, 0}};

static const symbol s_15_0[2] = {0xD8, 0xA9};

static const struct among a_15[1] = {
             {2, s_15_0, -1, 1, 0}};

static const symbol s_16_0[2] = {0xD9, 0x8A};

static const struct among a_16[1] = {
             {2, s_16_0, -1, 1, 0}};

static const symbol s_17_0[2] = {0xD9, 0x83};
static const symbol s_17_1[4] = {0xD9, 0x83, 0xD9, 0x85};
static const symbol s_17_2[4] = {0xD9, 0x87, 0xD9, 0x85};
static const symbol s_17_3[4] = {0xD9, 0x83, 0xD9, 0x86};
static const symbol s_17_4[4] = {0xD9, 0x87, 0xD9, 0x86};
static const symbol s_17_5[2] = {0xD9, 0x87};
static const symbol s_17_6[6] = {0xD9, 0x83, 0xD9, 0x85, 0xD9, 0x88};
static const symbol s_17_7[4] = {0xD9, 0x86, 0xD9, 0x8A};
static const symbol s_17_8[6] = {0xD9, 0x83, 0xD9, 0x85, 0xD8, 0xA7};
static const symbol s_17_9[6] = {0xD9, 0x87, 0xD9, 0x85, 0xD8, 0xA7};
static const symbol s_17_10[4] = {0xD9, 0x86, 0xD8, 0xA7};
static const symbol s_17_11[4] = {0xD9, 0x87, 0xD8, 0xA7};

static const struct among a_17[12] = {
             {2, s_17_0, -1, 1, 0},
             {4, s_17_1, -1, 2, 0},
             {4, s_17_2, -1, 2, 0},
             {4, s_17_3, -1, 2, 0},
             {4, s_17_4, -1, 2, 0},
             {2, s_17_5, -1, 1, 0},
             {6, s_17_6, -1, 3, 0},
             {4, s_17_7, -1, 2, 0},
             {6, s_17_8, -1, 3, 0},
             {6, s_17_9, -1, 3, 0},
             {4, s_17_10, -1, 2, 0},
             {4, s_17_11, -1, 2, 0}};

static const symbol s_18_0[2] = {0xD9, 0x86};
static const symbol s_18_1[4] = {0xD9, 0x88, 0xD9, 0x86};
static const symbol s_18_2[4] = {0xD9, 0x8A, 0xD9, 0x86};
static const symbol s_18_3[4] = {0xD8, 0xA7, 0xD9, 0x86};
static const symbol s_18_4[4] = {0xD8, 0xAA, 0xD9, 0x86};
static const symbol s_18_5[2] = {0xD9, 0x8A};
static const symbol s_18_6[2] = {0xD8, 0xA7};
static const symbol s_18_7[6] = {0xD8, 0xAA, 0xD9, 0x85, 0xD8, 0xA7};
static const symbol s_18_8[4] = {0xD9, 0x86, 0xD8, 0xA7};
static const symbol s_18_9[4] = {0xD8, 0xAA, 0xD8, 0xA7};
static const symbol s_18_10[2] = {0xD8, 0xAA};

static const struct among a_18[11] = {
             {2, s_18_0, -1, 1, 0},
             {4, s_18_1, 0, 3, 0},
             {4, s_18_2, 0, 3, 0},
             {4, s_18_3, 0, 3, 0},
             {4, s_18_4, 0, 2, 0},
             {2, s_18_5, -1, 1, 0},
             {2, s_18_6, -1, 1, 0},
             {6, s_18_7, 6, 4, 0},
             {4, s_18_8, 6, 2, 0},
             {4, s_18_9, 6, 2, 0},
             {2, s_18_10, -1, 1, 0}};

static const symbol s_19_0[4] = {0xD8, 0xAA, 0xD9, 0x85};
static const symbol s_19_1[4] = {0xD9, 0x88, 0xD8, 0xA7};

static const struct among a_19[2] = {
             {4, s_19_0, -1, 1, 0},
             {4, s_19_1, -1, 1, 0}};

static const symbol s_20_0[2] = {0xD9, 0x88};
static const symbol s_20_1[6] = {0xD8, 0xAA, 0xD9, 0x85, 0xD9, 0x88};

static const struct among a_20[2] = {
             {2, s_20_0, -1, 1, 0},
             {6, s_20_1, 0, 2, 0}};

static const symbol s_21_0[2] = {0xD9, 0x89};

static const struct among a_21[1] = {
             {2, s_21_0, -1, 1, 0}};

static const symbol s_0[] = {'0'};
static const symbol s_1[] = {'1'};
static const symbol s_2[] = {'2'};
static const symbol s_3[] = {'3'};
static const symbol s_4[] = {'4'};
static const symbol s_5[] = {'5'};
static const symbol s_6[] = {'6'};
static const symbol s_7[] = {'7'};
static const symbol s_8[] = {'8'};
static const symbol s_9[] = {'9'};
static const symbol s_10[] = {0xD8, 0xA1};
static const symbol s_11[] = {0xD8, 0xA3};
static const symbol s_12[] = {0xD8, 0xA5};
static const symbol s_13[] = {0xD8, 0xA6};
static const symbol s_14[] = {0xD8, 0xA2};
static const symbol s_15[] = {0xD8, 0xA4};
static const symbol s_16[] = {0xD8, 0xA7};
static const symbol s_17[] = {0xD8, 0xA8};
static const symbol s_18[] = {0xD8, 0xA9};
static const symbol s_19[] = {0xD8, 0xAA};
static const symbol s_20[] = {0xD8, 0xAB};
static const symbol s_21[] = {0xD8, 0xAC};
static const symbol s_22[] = {0xD8, 0xAD};
static const symbol s_23[] = {0xD8, 0xAE};
static const symbol s_24[] = {0xD8, 0xAF};
static const symbol s_25[] = {0xD8, 0xB0};
static const symbol s_26[] = {0xD8, 0xB1};
static const symbol s_27[] = {0xD8, 0xB2};
static const symbol s_28[] = {0xD8, 0xB3};
static const symbol s_29[] = {0xD8, 0xB4};
static const symbol s_30[] = {0xD8, 0xB5};
static const symbol s_31[] = {0xD8, 0xB6};
static const symbol s_32[] = {0xD8, 0xB7};
static const symbol s_33[] = {0xD8, 0xB8};
static const symbol s_34[] = {0xD8, 0xB9};
static const symbol s_35[] = {0xD8, 0xBA};
static const symbol s_36[] = {0xD9, 0x81};
static const symbol s_37[] = {0xD9, 0x82};
static const symbol s_38[] = {0xD9, 0x83};
static const symbol s_39[] = {0xD9, 0x84};
static const symbol s_40[] = {0xD9, 0x85};
static const symbol s_41[] = {0xD9, 0x86};
static const symbol s_42[] = {0xD9, 0x87};
static const symbol s_43[] = {0xD9, 0x88};
static const symbol s_44[] = {0xD9, 0x89};
static const symbol s_45[] = {0xD9, 0x8A};
static const symbol s_46[] = {0xD9, 0x84, 0xD8, 0xA7};
static const symbol s_47[] = {0xD9, 0x84, 0xD8, 0xA3};
static const symbol s_48[] = {0xD9, 0x84, 0xD8, 0xA5};
static const symbol s_49[] = {0xD9, 0x84, 0xD8, 0xA2};
static const symbol s_50[] = {0xD8, 0xA1};
static const symbol s_51[] = {0xD8, 0xA7};
static const symbol s_52[] = {0xD9, 0x88};
static const symbol s_53[] = {0xD9, 0x8A};
static const symbol s_54[] = {0xD8, 0xA3};
static const symbol s_55[] = {0xD8, 0xA2};
static const symbol s_56[] = {0xD8, 0xA7};
static const symbol s_57[] = {0xD8, 0xA5};
static const symbol s_58[] = {0xD9, 0x81, 0xD8, 0xA7};
static const symbol s_59[] = {0xD9, 0x88, 0xD8, 0xA7};
static const symbol s_60[] = {0xD8, 0xA8, 0xD8, 0xA7};
static const symbol s_61[] = {0xD8, 0xA8};
static const symbol s_62[] = {0xD9, 0x83};
static const symbol s_63[] = {0xD9, 0x8A};
static const symbol s_64[] = {0xD8, 0xAA};
static const symbol s_65[] = {0xD9, 0x86};
static const symbol s_66[] = {0xD8, 0xA3};
static const symbol s_67[] = {0xD8, 0xA7, 0xD8, 0xB3, 0xD8, 0xAA};
static const symbol s_68[] = {0xD9, 0x8A};

static int
r_Normalize_pre(struct SN_env *z)
{                  
  int among_var;
  {
    int c1 = z->c;                   
    while (1)
    {                       
      int c2 = z->c;
      {
        int c3 = z->c;                                         
        z->bra = z->c;                                        
        among_var = find_among(z, a_0, 144);                          
        if (!(among_var))
        {
          goto lab3;
        }
        z->ket = z->c;                  
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
          int ret = slice_from_s(z, 1, s_0);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 3:
        {
          int ret = slice_from_s(z, 1, s_1);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 4:
        {
          int ret = slice_from_s(z, 1, s_2);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 5:
        {
          int ret = slice_from_s(z, 1, s_3);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 6:
        {
          int ret = slice_from_s(z, 1, s_4);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 7:
        {
          int ret = slice_from_s(z, 1, s_5);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 8:
        {
          int ret = slice_from_s(z, 1, s_6);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 9:
        {
          int ret = slice_from_s(z, 1, s_7);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 10:
        {
          int ret = slice_from_s(z, 1, s_8);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 11:
        {
          int ret = slice_from_s(z, 1, s_9);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 12:
        {
          int ret = slice_from_s(z, 2, s_10);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 13:
        {
          int ret = slice_from_s(z, 2, s_11);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 14:
        {
          int ret = slice_from_s(z, 2, s_12);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 15:
        {
          int ret = slice_from_s(z, 2, s_13);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 16:
        {
          int ret = slice_from_s(z, 2, s_14);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 17:
        {
          int ret = slice_from_s(z, 2, s_15);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 18:
        {
          int ret = slice_from_s(z, 2, s_16);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 19:
        {
          int ret = slice_from_s(z, 2, s_17);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 20:
        {
          int ret = slice_from_s(z, 2, s_18);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 21:
        {
          int ret = slice_from_s(z, 2, s_19);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 22:
        {
          int ret = slice_from_s(z, 2, s_20);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 23:
        {
          int ret = slice_from_s(z, 2, s_21);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 24:
        {
          int ret = slice_from_s(z, 2, s_22);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 25:
        {
          int ret = slice_from_s(z, 2, s_23);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 26:
        {
          int ret = slice_from_s(z, 2, s_24);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 27:
        {
          int ret = slice_from_s(z, 2, s_25);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 28:
        {
          int ret = slice_from_s(z, 2, s_26);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 29:
        {
          int ret = slice_from_s(z, 2, s_27);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 30:
        {
          int ret = slice_from_s(z, 2, s_28);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 31:
        {
          int ret = slice_from_s(z, 2, s_29);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 32:
        {
          int ret = slice_from_s(z, 2, s_30);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 33:
        {
          int ret = slice_from_s(z, 2, s_31);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 34:
        {
          int ret = slice_from_s(z, 2, s_32);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 35:
        {
          int ret = slice_from_s(z, 2, s_33);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 36:
        {
          int ret = slice_from_s(z, 2, s_34);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 37:
        {
          int ret = slice_from_s(z, 2, s_35);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 38:
        {
          int ret = slice_from_s(z, 2, s_36);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 39:
        {
          int ret = slice_from_s(z, 2, s_37);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 40:
        {
          int ret = slice_from_s(z, 2, s_38);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 41:
        {
          int ret = slice_from_s(z, 2, s_39);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 42:
        {
          int ret = slice_from_s(z, 2, s_40);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 43:
        {
          int ret = slice_from_s(z, 2, s_41);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 44:
        {
          int ret = slice_from_s(z, 2, s_42);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 45:
        {
          int ret = slice_from_s(z, 2, s_43);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 46:
        {
          int ret = slice_from_s(z, 2, s_44);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 47:
        {
          int ret = slice_from_s(z, 2, s_45);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 48:
        {
          int ret = slice_from_s(z, 4, s_46);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 49:
        {
          int ret = slice_from_s(z, 4, s_47);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 50:
        {
          int ret = slice_from_s(z, 4, s_48);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 51:
        {
          int ret = slice_from_s(z, 4, s_49);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        }
        goto lab2;
      lab3:
        z->c = c3;
        {
          int ret = skip_utf8(z->p, z->c, 0, z->l, 1);
          if (ret < 0)
          {
            goto lab1;
          }
          z->c = ret;                     
        }
      }
    lab2:
      continue;
    lab1:
      z->c = c2;
      break;
    }
    z->c = c1;
  }
  return 1;
}

static int
r_Normalize_post(struct SN_env *z)
{                  
  int among_var;
  {
    int c1 = z->c;                   
    z->lb = z->c;
    z->c = z->l;                          

    z->ket = z->c;                  
    if (z->c - 1 <= z->lb || z->p[z->c - 1] >> 5 != 5 || !((124 >> (z->p[z->c - 1] & 0x1f)) & 1))
    {
      goto lab0;                          
    }
    if (!(find_among_b(z, a_1, 5)))
    {
      goto lab0;
    }
    z->bra = z->c;                  
    {
      int ret = slice_from_s(z, 2, s_50);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    z->c = z->lb;
  lab0:
    z->c = c1;
  }
  {
    int c2 = z->c;                   
    while (1)
    {                       
      int c3 = z->c;
      {
        int c4 = z->c;                   
        z->bra = z->c;                  
        if (z->c + 1 >= z->l || z->p[z->c + 1] >> 5 != 5 || !((124 >> (z->p[z->c + 1] & 0x1f)) & 1))
        {
          goto lab4;                          
        }
        among_var = find_among(z, a_2, 5);
        if (!(among_var))
        {
          goto lab4;
        }
        z->ket = z->c;                  
        switch (among_var)
        {                      
        case 1:
        {
          int ret = slice_from_s(z, 2, s_51);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 2:
        {
          int ret = slice_from_s(z, 2, s_52);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        case 3:
        {
          int ret = slice_from_s(z, 2, s_53);                   
          if (ret < 0)
          {
            return ret;
          }
        }
        break;
        }
        goto lab3;
      lab4:
        z->c = c4;
        {
          int ret = skip_utf8(z->p, z->c, 0, z->l, 1);
          if (ret < 0)
          {
            goto lab2;
          }
          z->c = ret;                     
        }
      }
    lab3:
      continue;
    lab2:
      z->c = c3;
      break;
    }
    z->c = c2;
  }
  return 1;
}

static int
r_Checks1(struct SN_env *z)
{                  
  int among_var;
  z->bra = z->c;                  
  if (z->c + 3 >= z->l || (z->p[z->c + 3] != 132 && z->p[z->c + 3] != 167))
  {
    return 0;                          
  }
  among_var = find_among(z, a_3, 4);
  if (!(among_var))
  {
    return 0;
  }
  z->ket = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) > 4))
    {
      return 0;                                                               
    }
    z->B[0] = 1;                            
    z->B[1] = 0;                              
    z->B[2] = 1;                               
    break;
  case 2:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    z->B[0] = 1;                            
    z->B[1] = 0;                              
    z->B[2] = 1;                               
    break;
  }
  return 1;
}

static int
r_Prefix_Step1(struct SN_env *z)
{                  
  int among_var;
  z->bra = z->c;                  
  if (z->c + 3 >= z->l || z->p[z->c + 3] >> 5 != 5 || !((188 >> (z->p[z->c + 3] & 0x1f)) & 1))
  {
    return 0;                          
  }
  among_var = find_among(z, a_4, 5);
  if (!(among_var))
  {
    return 0;
  }
  z->ket = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_54);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 2:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_55);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 3:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_56);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 4:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_57);                   
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
r_Prefix_Step2(struct SN_env *z)
{                  
  {
    int c1 = z->c;                    
    if (!(eq_s(z, 4, s_58)))
    {
      goto lab0;                        
    }
    return 0;
  lab0:
    z->c = c1;
  }
  {
    int c2 = z->c;                    
    if (!(eq_s(z, 4, s_59)))
    {
      goto lab1;                        
    }
    return 0;
  lab1:
    z->c = c2;
  }
  z->bra = z->c;                  
  if (z->c + 1 >= z->l || (z->p[z->c + 1] != 129 && z->p[z->c + 1] != 136))
  {
    return 0;                          
  }
  if (!(find_among(z, a_5, 2)))
  {
    return 0;
  }
  z->ket = z->c;                  
  if (!(len_utf8(z->p) > 3))
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
  return 1;
}

static int
r_Prefix_Step3a_Noun(struct SN_env *z)
{                  
  int among_var;
  z->bra = z->c;                  
  if (z->c + 3 >= z->l || (z->p[z->c + 3] != 132 && z->p[z->c + 3] != 167))
  {
    return 0;                          
  }
  among_var = find_among(z, a_6, 4);
  if (!(among_var))
  {
    return 0;
  }
  z->ket = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) > 5))
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
  case 2:
    if (!(len_utf8(z->p) > 4))
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
r_Prefix_Step3b_Noun(struct SN_env *z)
{                  
  int among_var;
  {
    int c1 = z->c;                    
    if (!(eq_s(z, 4, s_60)))
    {
      goto lab0;                        
    }
    return 0;
  lab0:
    z->c = c1;
  }
  z->bra = z->c;                  
  if (z->c + 1 >= z->l || (z->p[z->c + 1] != 168 && z->p[z->c + 1] != 131))
  {
    return 0;                          
  }
  among_var = find_among(z, a_7, 3);
  if (!(among_var))
  {
    return 0;
  }
  z->ket = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) > 3))
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
  case 2:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_61);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 3:
    if (!(len_utf8(z->p) > 3))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_62);                   
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
r_Prefix_Step3_Verb(struct SN_env *z)
{                  
  int among_var;
  z->bra = z->c;                                      
  among_var = find_among(z, a_8, 4);                          
  if (!(among_var))
  {
    return 0;
  }
  z->ket = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) > 4))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_63);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 2:
    if (!(len_utf8(z->p) > 4))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_64);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 3:
    if (!(len_utf8(z->p) > 4))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_65);                   
      if (ret < 0)
      {
        return ret;
      }
    }
    break;
  case 4:
    if (!(len_utf8(z->p) > 4))
    {
      return 0;                                                               
    }
    {
      int ret = slice_from_s(z, 2, s_66);                   
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
r_Prefix_Step4_Verb(struct SN_env *z)
{                                 
  z->bra = z->c;                  
  if (z->c + 5 >= z->l || z->p[z->c + 5] != 170)
  {
    return 0;                          
  }
  if (!(find_among(z, a_9, 3)))
  {
    return 0;
  }
  z->ket = z->c;                  
  if (!(len_utf8(z->p) > 4))
  {
    return 0;                                                               
  }
  z->B[1] = 1;                            
  z->B[0] = 0;                              
  {
    int ret = slice_from_s(z, 6, s_67);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  return 1;
}

static int
r_Suffix_Noun_Step1a(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                                          
  among_var = find_among_b(z, a_10, 10);                          
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) >= 4))
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
  case 2:
    if (!(len_utf8(z->p) >= 5))
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
  case 3:
    if (!(len_utf8(z->p) >= 6))
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
r_Suffix_Noun_Step1b(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 134)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_11, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) > 5))
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
  return 1;
}

static int
r_Suffix_Noun_Step2a(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (!(find_among_b(z, a_12, 3)))
  {
    return 0;                          
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) > 4))
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
  return 1;
}

static int
r_Suffix_Noun_Step2b(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 3 <= z->lb || z->p[z->c - 1] != 170)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_13, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) >= 5))
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
  return 1;
}

static int
r_Suffix_Noun_Step2c1(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 170)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_14, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) >= 4))
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
  return 1;
}

static int
r_Suffix_Noun_Step2c2(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 169)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_15, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) >= 4))
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
  return 1;
}

static int
r_Suffix_Noun_Step3(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 138)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_16, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) >= 3))
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
  return 1;
}

static int
r_Suffix_Verb_Step1(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                                          
  among_var = find_among_b(z, a_17, 12);                          
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) >= 4))
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
  case 2:
    if (!(len_utf8(z->p) >= 5))
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
  case 3:
    if (!(len_utf8(z->p) >= 6))
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
r_Suffix_Verb_Step2a(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                                          
  among_var = find_among_b(z, a_18, 11);                          
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) >= 4))
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
  case 2:
    if (!(len_utf8(z->p) >= 5))
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
  case 3:
    if (!(len_utf8(z->p) > 5))
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
  case 4:
    if (!(len_utf8(z->p) >= 6))
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
r_Suffix_Verb_Step2b(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 3 <= z->lb || (z->p[z->c - 1] != 133 && z->p[z->c - 1] != 167))
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_19, 2)))
  {
    return 0;
  }
  z->bra = z->c;                  
  if (!(len_utf8(z->p) >= 5))
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
  return 1;
}

static int
r_Suffix_Verb_Step2c(struct SN_env *z)
{                   
  int among_var;
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 136)
  {
    return 0;                          
  }
  among_var = find_among_b(z, a_20, 2);
  if (!(among_var))
  {
    return 0;
  }
  z->bra = z->c;                  
  switch (among_var)
  {                      
  case 1:
    if (!(len_utf8(z->p) >= 4))
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
  case 2:
    if (!(len_utf8(z->p) >= 6))
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
r_Suffix_All_alef_maqsura(struct SN_env *z)
{                                  
  z->ket = z->c;                  
  if (z->c - 1 <= z->lb || z->p[z->c - 1] != 137)
  {
    return 0;                          
  }
  if (!(find_among_b(z, a_21, 1)))
  {
    return 0;
  }
  z->bra = z->c;                  
  {
    int ret = slice_from_s(z, 2, s_68);                   
    if (ret < 0)
    {
      return ret;
    }
  }
  return 1;
}

extern int
arabic_UTF_8_stem(struct SN_env *z)
{                               
  z->B[0] = 1;                            
  z->B[1] = 1;                            
  z->B[2] = 0;                                 
  {
    int c1 = z->c;                   
    {
      int ret = r_Checks1(z);                             
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
    int ret = r_Normalize_pre(z);                                   
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
      if (!(z->B[1]))
      {
        goto lab4;                                     
      }
      {
        int m4 = z->l - z->c;
        (void)m4;                   
        {
          int i = 1;
          while (1)
          {                        
            int m5 = z->l - z->c;
            (void)m5;
            {
              int ret = r_Suffix_Verb_Step1(z);                                       
              if (ret == 0)
              {
                goto lab7;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            i--;
            continue;
          lab7:
            z->c = z->l - m5;
            break;
          }
          if (i > 0)
          {
            goto lab6;
          }
        }
        {
          int m6 = z->l - z->c;
          (void)m6;                   
          {
            int ret = r_Suffix_Verb_Step2a(z);                                        
            if (ret == 0)
            {
              goto lab9;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab8;
        lab9:
          z->c = z->l - m6;
          {
            int ret = r_Suffix_Verb_Step2c(z);                                        
            if (ret == 0)
            {
              goto lab10;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab8;
        lab10:
          z->c = z->l - m6;
          {
            int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
            if (ret < 0)
            {
              goto lab6;
            }
            z->c = ret;                     
          }
        }
      lab8:
        goto lab5;
      lab6:
        z->c = z->l - m4;
        {
          int ret = r_Suffix_Verb_Step2b(z);                                        
          if (ret == 0)
          {
            goto lab11;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
        goto lab5;
      lab11:
        z->c = z->l - m4;
        {
          int ret = r_Suffix_Verb_Step2a(z);                                        
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
      goto lab3;
    lab4:
      z->c = z->l - m3;
      if (!(z->B[0]))
      {
        goto lab12;                                     
      }
      {
        int m7 = z->l - z->c;
        (void)m7;                    
        {
          int m8 = z->l - z->c;
          (void)m8;                   
          {
            int ret = r_Suffix_Noun_Step2c2(z);                                         
            if (ret == 0)
            {
              goto lab15;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          goto lab14;
        lab15:
          z->c = z->l - m8;
                             
          if (!(z->B[2]))
          {
            goto lab17;                                        
          }
          goto lab16;
        lab17:
        {
          int ret = r_Suffix_Noun_Step1a(z);                                        
          if (ret == 0)
          {
            goto lab16;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
          {
            int m9 = z->l - z->c;
            (void)m9;                   
            {
              int ret = r_Suffix_Noun_Step2a(z);                                        
              if (ret == 0)
              {
                goto lab19;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab18;
          lab19:
            z->c = z->l - m9;
            {
              int ret = r_Suffix_Noun_Step2b(z);                                        
              if (ret == 0)
              {
                goto lab20;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab18;
          lab20:
            z->c = z->l - m9;
            {
              int ret = r_Suffix_Noun_Step2c1(z);                                         
              if (ret == 0)
              {
                goto lab21;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab18;
          lab21:
            z->c = z->l - m9;
            {
              int ret = skip_utf8(z->p, z->c, z->lb, 0, -1);
              if (ret < 0)
              {
                goto lab16;
              }
              z->c = ret;                     
            }
          }
        lab18:
          goto lab14;
        lab16:
          z->c = z->l - m8;
          {
            int ret = r_Suffix_Noun_Step1b(z);                                        
            if (ret == 0)
            {
              goto lab22;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
          {
            int m10 = z->l - z->c;
            (void)m10;                   
            {
              int ret = r_Suffix_Noun_Step2a(z);                                        
              if (ret == 0)
              {
                goto lab24;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab23;
          lab24:
            z->c = z->l - m10;
            {
              int ret = r_Suffix_Noun_Step2b(z);                                        
              if (ret == 0)
              {
                goto lab25;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
            goto lab23;
          lab25:
            z->c = z->l - m10;
            {
              int ret = r_Suffix_Noun_Step2c1(z);                                         
              if (ret == 0)
              {
                goto lab22;
              }
              if (ret < 0)
              {
                return ret;
              }
            }
          }
        lab23:
          goto lab14;
        lab22:
          z->c = z->l - m8;
                             
          if (!(z->B[2]))
          {
            goto lab27;                                        
          }
          goto lab26;
        lab27:
        {
          int ret = r_Suffix_Noun_Step2a(z);                                        
          if (ret == 0)
          {
            goto lab26;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
          goto lab14;
        lab26:
          z->c = z->l - m8;
          {
            int ret = r_Suffix_Noun_Step2b(z);                                        
            if (ret == 0)
            {
              z->c = z->l - m7;
              goto lab13;
            }
            if (ret < 0)
            {
              return ret;
            }
          }
        }
      lab14:
      lab13:;
      }
      {
        int ret = r_Suffix_Noun_Step3(z);                                       
        if (ret == 0)
        {
          goto lab12;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
      goto lab3;
    lab12:
      z->c = z->l - m3;
      {
        int ret = r_Suffix_All_alef_maqsura(z);                                             
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
  z->c = z->lb;
  {
    int c11 = z->c;                   
    {
      int c12 = z->c;                    
      {
        int ret = r_Prefix_Step1(z);                                  
        if (ret == 0)
        {
          z->c = c12;
          goto lab29;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
    lab29:;
    }
    {
      int c13 = z->c;                    
      {
        int ret = r_Prefix_Step2(z);                                  
        if (ret == 0)
        {
          z->c = c13;
          goto lab30;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
    lab30:;
    }
    {
      int c14 = z->c;                   
      {
        int ret = r_Prefix_Step3a_Noun(z);                                        
        if (ret == 0)
        {
          goto lab32;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
      goto lab31;
    lab32:
      z->c = c14;
      if (!(z->B[0]))
      {
        goto lab33;                                     
      }
      {
        int ret = r_Prefix_Step3b_Noun(z);                                        
        if (ret == 0)
        {
          goto lab33;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
      goto lab31;
    lab33:
      z->c = c14;
      if (!(z->B[1]))
      {
        goto lab28;                                     
      }
      {
        int c15 = z->c;                    
        {
          int ret = r_Prefix_Step3_Verb(z);                                       
          if (ret == 0)
          {
            z->c = c15;
            goto lab34;
          }
          if (ret < 0)
          {
            return ret;
          }
        }
      lab34:;
      }
      {
        int ret = r_Prefix_Step4_Verb(z);                                       
        if (ret == 0)
        {
          goto lab28;
        }
        if (ret < 0)
        {
          return ret;
        }
      }
    }
  lab31:
  lab28:
    z->c = c11;
  }
                    
  {
    int ret = r_Normalize_post(z);                                    
    if (ret == 0)
    {
      goto lab35;
    }
    if (ret < 0)
    {
      return ret;
    }
  }
lab35:
  return 1;
}

extern struct SN_env *
arabic_UTF_8_create_env(void)
{
  return SN_create_env(0, 0, 3);
}

extern void
arabic_UTF_8_close_env(struct SN_env *z)
{
  SN_close_env(z, 0);
}
