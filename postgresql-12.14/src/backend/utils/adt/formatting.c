                                                                           
                
   
                                      
   
   
                                                                          
   
   
                                                       
   
                                                                          
                                                                         
   
   
                    
                                                             
   
                                                                             
                                                                            
           
   
                             
                                                                
                                                                  
                                                                    
               
   
                                 
                                                                       
                                                              
                                                           
                                      
   
                                  
   
                                                   
   
                                           
   
                               
                      
                           
   
   
             
   
        
                                                                 
                 
                  
                                                                
                                     
                                                                    
                      
                                                 
   
                                                                           
   

#ifdef DEBUG_TO_FROM_CHAR
#define DEBUG_elog_output DEBUG3
#endif

#include "postgres.h"

#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <float.h>
#include <limits.h>

   
                                                                            
                              
   
#ifdef HAVE_WCHAR_H
#include <wchar.h>
#endif
#ifdef HAVE_WCTYPE_H
#include <wctype.h>
#endif

#ifdef USE_ICU
#include <unicode/ustring.h>
#endif

#include "catalog/pg_collation.h"
#include "mb/pg_wchar.h"
#include "parser/scansup.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datetime.h"
#include "utils/float.h"
#include "utils/formatting.h"
#include "utils/int8.h"
#include "utils/memutils.h"
#include "utils/numeric.h"
#include "utils/pg_locale.h"

              
                 
              
   
#define DCH_TYPE 1                        
#define NUM_TYPE 2                     

              
                                                           
              
   
#define KeyWord_INDEX_SIZE ('~' - ' ')
#define KeyWord_INDEX_FILTER(_c) ((_c) <= ' ' || (_c) >= '~' ? 0 : 1)

              
                              
              
   
#define DCH_MAX_ITEM_SIZ 12                              
#define NUM_MAX_ITEM_SIZ 8                                      

              
                         
              
   
typedef struct
{
  const char *name;                     
  int len,                              
      id,                                     
      type;                                
} KeySuffix;

              
                    
              
   
                                                                        
                                                             
   
typedef enum
{
  FROM_CHAR_DATE_NONE = 0,                                        
  FROM_CHAR_DATE_GREGORIAN,                                              
  FROM_CHAR_DATE_ISOWEEK                            
} FromCharDateMode;

typedef struct
{
  const char *name;
  int len;
  int id;
  bool is_digit;
  FromCharDateMode date_mode;
} KeyWord;

typedef struct
{
  uint8 type;                                                               
  char character[MAX_MULTIBYTE_CHAR_LEN + 1];                      
  uint8 suffix;                                                                       
  const KeyWord *key;                                                
} FormatNode;

#define NODE_TYPE_END 1
#define NODE_TYPE_ACTION 2
#define NODE_TYPE_CHAR 3
#define NODE_TYPE_SEPARATOR 4
#define NODE_TYPE_SPACE 5

#define SUFFTYPE_PREFIX 1
#define SUFFTYPE_POSTFIX 2

#define CLOCK_24_HOUR 0
#define CLOCK_12_HOUR 1

              
               
              
   
static const char *const months_full[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December", NULL};

static const char *const days_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

              
           
              
                                                                
                                                                    
                                                               
   
#define ADJUST_YEAR(year, is_interval) ((is_interval) ? (year) : ((year) <= 0 ? -((year)-1) : (year)))

#define A_D_STR "A.D."
#define a_d_STR "a.d."
#define AD_STR "AD"
#define ad_STR "ad"

#define B_C_STR "B.C."
#define b_c_STR "b.c."
#define BC_STR "BC"
#define bc_STR "bc"

   
                                   
   
                                                                            
                 
   
                                                                          
                                                                              
                                                  
   
static const char *const adbc_strings[] = {ad_STR, bc_STR, AD_STR, BC_STR, NULL};
static const char *const adbc_strings_long[] = {a_d_STR, b_c_STR, A_D_STR, B_C_STR, NULL};

              
           
              
   
#define A_M_STR "A.M."
#define a_m_STR "a.m."
#define AM_STR "AM"
#define am_STR "am"

#define P_M_STR "P.M."
#define p_m_STR "p.m."
#define PM_STR "PM"
#define pm_STR "pm"

   
                                   
   
                                                                            
                 
   
                                                                          
                                                                              
                                                  
   
static const char *const ampm_strings[] = {am_STR, pm_STR, AM_STR, PM_STR, NULL};
static const char *const ampm_strings_long[] = {a_m_STR, p_m_STR, A_M_STR, P_M_STR, NULL};

              
                           
                                                                    
                                                
              
   
static const char *const rm_months_upper[] = {"XII", "XI", "X", "IX", "VIII", "VII", "VI", "V", "IV", "III", "II", "I", NULL};

static const char *const rm_months_lower[] = {"xii", "xi", "x", "ix", "viii", "vii", "vi", "v", "iv", "iii", "ii", "i", NULL};

              
                 
              
   
static const char *const rm1[] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", NULL};
static const char *const rm10[] = {"X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC", NULL};
static const char *const rm100[] = {"C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM", NULL};

              
                     
              
   
static const char *const numTH[] = {"ST", "ND", "RD", "TH", NULL};
static const char *const numth[] = {"st", "nd", "rd", "th", NULL};

              
                    
              
   
#define TH_UPPER 1
#define TH_LOWER 2

              
                             
              
   
typedef struct
{
  int pre,                                               
      post,                                              
      lsign,                                   
      flag,                                    
      pre_lsign_num,                             
      multi,                                    
      zero_start,                                  
      zero_end,                                   
      need_locale;                           
} NUMDesc;

              
                            
              
   
#define NUM_F_DECIMAL (1 << 1)
#define NUM_F_LDECIMAL (1 << 2)
#define NUM_F_ZERO (1 << 3)
#define NUM_F_BLANK (1 << 4)
#define NUM_F_FILLMODE (1 << 5)
#define NUM_F_LSIGN (1 << 6)
#define NUM_F_BRACKET (1 << 7)
#define NUM_F_MINUS (1 << 8)
#define NUM_F_PLUS (1 << 9)
#define NUM_F_ROMAN (1 << 10)
#define NUM_F_MULTI (1 << 11)
#define NUM_F_PLUS_POST (1 << 12)
#define NUM_F_MINUS_POST (1 << 13)
#define NUM_F_EEEE (1 << 14)

#define NUM_LSIGN_PRE (-1)
#define NUM_LSIGN_POST 1
#define NUM_LSIGN_NONE 0

              
         
              
   
#define IS_DECIMAL(_f) ((_f)->flag & NUM_F_DECIMAL)
#define IS_LDECIMAL(_f) ((_f)->flag & NUM_F_LDECIMAL)
#define IS_ZERO(_f) ((_f)->flag & NUM_F_ZERO)
#define IS_BLANK(_f) ((_f)->flag & NUM_F_BLANK)
#define IS_FILLMODE(_f) ((_f)->flag & NUM_F_FILLMODE)
#define IS_BRACKET(_f) ((_f)->flag & NUM_F_BRACKET)
#define IS_MINUS(_f) ((_f)->flag & NUM_F_MINUS)
#define IS_LSIGN(_f) ((_f)->flag & NUM_F_LSIGN)
#define IS_PLUS(_f) ((_f)->flag & NUM_F_PLUS)
#define IS_ROMAN(_f) ((_f)->flag & NUM_F_ROMAN)
#define IS_MULTI(_f) ((_f)->flag & NUM_F_MULTI)
#define IS_EEEE(_f) ((_f)->flag & NUM_F_EEEE)

              
                        
   
                                                                           
                                                                    
   
                                                                           
                                                                   
   
                                                                           
                                                                         
                                                                           
                                                              
   
                                                                
                            
              
   
#define DCH_CACHE_OVERHEAD MAXALIGN(sizeof(bool) + sizeof(int))
#define NUM_CACHE_OVERHEAD MAXALIGN(sizeof(bool) + sizeof(int) + sizeof(NUMDesc))

#define DCH_CACHE_SIZE ((2048 - DCH_CACHE_OVERHEAD) / (sizeof(FormatNode) + sizeof(char)) - 1)
#define NUM_CACHE_SIZE ((1024 - NUM_CACHE_OVERHEAD) / (sizeof(FormatNode) + sizeof(char)) - 1)

#define DCH_CACHE_ENTRIES 20
#define NUM_CACHE_ENTRIES 20

typedef struct
{
  FormatNode format[DCH_CACHE_SIZE + 1];
  char str[DCH_CACHE_SIZE + 1];
  bool valid;
  int age;
} DCHCacheEntry;

typedef struct
{
  FormatNode format[NUM_CACHE_SIZE + 1];
  char str[NUM_CACHE_SIZE + 1];
  bool valid;
  int age;
  NUMDesc Num;
} NUMCacheEntry;

                                                
static DCHCacheEntry *DCHCache[DCH_CACHE_ENTRIES];
static int n_DCHCache = 0;                                
static int DCHCounter = 0;                          

                                             
static NUMCacheEntry *NUMCache[NUM_CACHE_ENTRIES];
static int n_NUMCache = 0;                                
static int NUMCounter = 0;                          

              
                                  
              
   
typedef struct
{
  FromCharDateMode mode;
  int hh, pm, mi, ss, ssss, d,                                                                           
      dd, ddd, mm, ms, year, bc, ww, w, cc, j, us, yysz,                         
      clock,                                                                       
      tzsign,                                                                                        
      tzh, tzm;
} TmFromChar;

#define ZERO_tmfc(_X) memset(_X, 0, sizeof(TmFromChar))

              
         
              
   
#ifdef DEBUG_TO_FROM_CHAR
#define DEBUG_TMFC(_X) elog(DEBUG_elog_output, "TMFC:\nmode %d\nhh %d\npm %d\nmi %d\nss %d\nssss %d\nd %d\ndd %d\nddd %d\nmm %d\nms: %d\nyear %d\nbc %d\nww %d\nw %d\ncc %d\nj %d\nus: %d\nyysz: %d\nclock: %d", (_X)->mode, (_X)->hh, (_X)->pm, (_X)->mi, (_X)->ss, (_X)->ssss, (_X)->d, (_X)->dd, (_X)->ddd, (_X)->mm, (_X)->ms, (_X)->year, (_X)->bc, (_X)->ww, (_X)->w, (_X)->cc, (_X)->j, (_X)->us, (_X)->yysz, (_X)->clock)
#define DEBUG_TM(_X) elog(DEBUG_elog_output, "TM:\nsec %d\nyear %d\nmin %d\nwday %d\nhour %d\nyday %d\nmday %d\nnisdst %d\nmon %d\n", (_X)->tm_sec, (_X)->tm_year, (_X)->tm_min, (_X)->tm_wday, (_X)->tm_hour, (_X)->tm_yday, (_X)->tm_mday, (_X)->tm_isdst, (_X)->tm_mon)
#else
#define DEBUG_TMFC(_X)
#define DEBUG_TM(_X)
#endif

              
                               
              
   
typedef struct TmToChar
{
  struct pg_tm tm;                          
  fsec_t fsec;                             
  const char *tzn;               
} TmToChar;

#define tmtcTm(_X) (&(_X)->tm)
#define tmtcTzn(_X) ((_X)->tzn)
#define tmtcFsec(_X) ((_X)->fsec)

#define ZERO_tm(_X)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (_X)->tm_sec = (_X)->tm_year = (_X)->tm_min = (_X)->tm_wday = (_X)->tm_hour = (_X)->tm_yday = (_X)->tm_isdst = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    (_X)->tm_mday = (_X)->tm_mon = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    (_X)->tm_zone = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  } while (0)

#define ZERO_tmtc(_X)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    ZERO_tm(tmtcTm(_X));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
    tmtcFsec(_X) = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    tmtcTzn(_X) = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  } while (0)

   
                                                                    
                                               
   
#define INVALID_FOR_INTERVAL                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (is_interval)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid format specification for an interval value"), errhint("Intervals are not tied to specific calendar dates.")));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

                                                                               
                         
                                                                               

              
                                                        
              
   
#define DCH_S_FM 0x01
#define DCH_S_TH 0x02
#define DCH_S_th 0x04
#define DCH_S_SP 0x08
#define DCH_S_TM 0x10

              
                
              
   
#define S_THth(_s) ((((_s) & DCH_S_TH) || ((_s) & DCH_S_th)) ? 1 : 0)
#define S_TH(_s) (((_s) & DCH_S_TH) ? 1 : 0)
#define S_th(_s) (((_s) & DCH_S_th) ? 1 : 0)
#define S_TH_TYPE(_s) (((_s) & DCH_S_TH) ? TH_UPPER : TH_LOWER)

                                                     
#define S_FM(_s) (((_s) & DCH_S_FM) ? 1 : 0)
#define S_SP(_s) (((_s) & DCH_S_SP) ? 1 : 0)
#define S_TM(_s) (((_s) & DCH_S_TM) ? 1 : 0)

              
                                                  
              
   
#define TM_SUFFIX_LEN 2

static const KeySuffix DCH_suff[] = {{"FM", 2, DCH_S_FM, SUFFTYPE_PREFIX}, {"fm", 2, DCH_S_FM, SUFFTYPE_PREFIX}, {"TM", TM_SUFFIX_LEN, DCH_S_TM, SUFFTYPE_PREFIX}, {"tm", 2, DCH_S_TM, SUFFTYPE_PREFIX}, {"TH", 2, DCH_S_TH, SUFFTYPE_POSTFIX}, {"th", 2, DCH_S_th, SUFFTYPE_POSTFIX}, {"SP", 2, DCH_S_SP, SUFFTYPE_POSTFIX},
              
    {NULL, 0, 0, 0}};

              
                              
   
                                                                       
                              
   
                                    
   
                                                                              
                                                                       
                                                                        
                   
   
       
                                                                               
       
   
                                                                                 
                                                                          
                                                                         
                
                                       
                                                      
                                                             
   
              
   

typedef enum
{
  DCH_A_D,
  DCH_A_M,
  DCH_AD,
  DCH_AM,
  DCH_B_C,
  DCH_BC,
  DCH_CC,
  DCH_DAY,
  DCH_DDD,
  DCH_DD,
  DCH_DY,
  DCH_Day,
  DCH_Dy,
  DCH_D,
  DCH_FX,                    
  DCH_HH24,
  DCH_HH12,
  DCH_HH,
  DCH_IDDD,
  DCH_ID,
  DCH_IW,
  DCH_IYYY,
  DCH_IYY,
  DCH_IY,
  DCH_I,
  DCH_J,
  DCH_MI,
  DCH_MM,
  DCH_MONTH,
  DCH_MON,
  DCH_MS,
  DCH_Month,
  DCH_Mon,
  DCH_OF,
  DCH_P_M,
  DCH_PM,
  DCH_Q,
  DCH_RM,
  DCH_SSSS,
  DCH_SS,
  DCH_TZH,
  DCH_TZM,
  DCH_TZ,
  DCH_US,
  DCH_WW,
  DCH_W,
  DCH_Y_YYY,
  DCH_YYYY,
  DCH_YYY,
  DCH_YY,
  DCH_Y,
  DCH_a_d,
  DCH_a_m,
  DCH_ad,
  DCH_am,
  DCH_b_c,
  DCH_bc,
  DCH_cc,
  DCH_day,
  DCH_ddd,
  DCH_dd,
  DCH_dy,
  DCH_d,
  DCH_fx,
  DCH_hh24,
  DCH_hh12,
  DCH_hh,
  DCH_iddd,
  DCH_id,
  DCH_iw,
  DCH_iyyy,
  DCH_iyy,
  DCH_iy,
  DCH_i,
  DCH_j,
  DCH_mi,
  DCH_mm,
  DCH_month,
  DCH_mon,
  DCH_ms,
  DCH_p_m,
  DCH_pm,
  DCH_q,
  DCH_rm,
  DCH_ssss,
  DCH_ss,
  DCH_tz,
  DCH_us,
  DCH_ww,
  DCH_w,
  DCH_y_yyy,
  DCH_yyyy,
  DCH_yyy,
  DCH_yy,
  DCH_y,

            
  _DCH_last_
} DCH_poz;

typedef enum
{
  NUM_COMMA,
  NUM_DEC,
  NUM_0,
  NUM_9,
  NUM_B,
  NUM_C,
  NUM_D,
  NUM_E,
  NUM_FM,
  NUM_G,
  NUM_L,
  NUM_MI,
  NUM_PL,
  NUM_PR,
  NUM_RN,
  NUM_SG,
  NUM_SP,
  NUM_S,
  NUM_TH,
  NUM_V,
  NUM_b,
  NUM_c,
  NUM_d,
  NUM_e,
  NUM_fm,
  NUM_g,
  NUM_l,
  NUM_mi,
  NUM_pl,
  NUM_pr,
  NUM_rn,
  NUM_sg,
  NUM_sp,
  NUM_s,
  NUM_th,
  NUM_v,

            
  _NUM_last_
} NUM_poz;

              
                                  
              
   
static const KeyWord DCH_keywords[] = {
                                            
    {"A.D.", 4, DCH_A_D, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"A.M.", 4, DCH_A_M, false, FROM_CHAR_DATE_NONE}, {"AD", 2, DCH_AD, false, FROM_CHAR_DATE_NONE}, {"AM", 2, DCH_AM, false, FROM_CHAR_DATE_NONE}, {"B.C.", 4, DCH_B_C, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                      
    {"BC", 2, DCH_BC, false, FROM_CHAR_DATE_NONE}, {"CC", 2, DCH_CC, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                           
    {"DAY", 3, DCH_DAY, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                       
    {"DDD", 3, DCH_DDD, true, FROM_CHAR_DATE_GREGORIAN}, {"DD", 2, DCH_DD, true, FROM_CHAR_DATE_GREGORIAN}, {"DY", 2, DCH_DY, false, FROM_CHAR_DATE_NONE}, {"Day", 3, DCH_Day, false, FROM_CHAR_DATE_NONE}, {"Dy", 2, DCH_Dy, false, FROM_CHAR_DATE_NONE}, {"D", 1, DCH_D, true, FROM_CHAR_DATE_GREGORIAN}, {"FX", 2, DCH_FX, false, FROM_CHAR_DATE_NONE},                                 
    {"HH24", 4, DCH_HH24, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"HH12", 4, DCH_HH12, true, FROM_CHAR_DATE_NONE}, {"HH", 2, DCH_HH, true, FROM_CHAR_DATE_NONE}, {"IDDD", 4, DCH_IDDD, true, FROM_CHAR_DATE_ISOWEEK},                                                                                                                                                                                                                                   
    {"ID", 2, DCH_ID, true, FROM_CHAR_DATE_ISOWEEK}, {"IW", 2, DCH_IW, true, FROM_CHAR_DATE_ISOWEEK}, {"IYYY", 4, DCH_IYYY, true, FROM_CHAR_DATE_ISOWEEK}, {"IYY", 3, DCH_IYY, true, FROM_CHAR_DATE_ISOWEEK}, {"IY", 2, DCH_IY, true, FROM_CHAR_DATE_ISOWEEK}, {"I", 1, DCH_I, true, FROM_CHAR_DATE_ISOWEEK}, {"J", 1, DCH_J, true, FROM_CHAR_DATE_NONE},                                  
    {"MI", 2, DCH_MI, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                          
    {"MM", 2, DCH_MM, true, FROM_CHAR_DATE_GREGORIAN}, {"MONTH", 5, DCH_MONTH, false, FROM_CHAR_DATE_GREGORIAN}, {"MON", 3, DCH_MON, false, FROM_CHAR_DATE_GREGORIAN}, {"MS", 2, DCH_MS, true, FROM_CHAR_DATE_NONE}, {"Month", 5, DCH_Month, false, FROM_CHAR_DATE_GREGORIAN}, {"Mon", 3, DCH_Mon, false, FROM_CHAR_DATE_GREGORIAN}, {"OF", 2, DCH_OF, false, FROM_CHAR_DATE_NONE},        
    {"P.M.", 4, DCH_P_M, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"PM", 2, DCH_PM, false, FROM_CHAR_DATE_NONE}, {"Q", 1, DCH_Q, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                             
    {"RM", 2, DCH_RM, false, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                                                                                    
    {"SSSS", 4, DCH_SSSS, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"SS", 2, DCH_SS, true, FROM_CHAR_DATE_NONE}, {"TZH", 3, DCH_TZH, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                         
    {"TZM", 3, DCH_TZM, true, FROM_CHAR_DATE_NONE}, {"TZ", 2, DCH_TZ, false, FROM_CHAR_DATE_NONE}, {"US", 2, DCH_US, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                           
    {"WW", 2, DCH_WW, true, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                                                                                     
    {"W", 1, DCH_W, true, FROM_CHAR_DATE_GREGORIAN}, {"Y,YYY", 5, DCH_Y_YYY, true, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                              
    {"YYYY", 4, DCH_YYYY, true, FROM_CHAR_DATE_GREGORIAN}, {"YYY", 3, DCH_YYY, true, FROM_CHAR_DATE_GREGORIAN}, {"YY", 2, DCH_YY, true, FROM_CHAR_DATE_GREGORIAN}, {"Y", 1, DCH_Y, true, FROM_CHAR_DATE_GREGORIAN}, {"a.d.", 4, DCH_a_d, false, FROM_CHAR_DATE_NONE},                                                                                                                      
    {"a.m.", 4, DCH_a_m, false, FROM_CHAR_DATE_NONE}, {"ad", 2, DCH_ad, false, FROM_CHAR_DATE_NONE}, {"am", 2, DCH_am, false, FROM_CHAR_DATE_NONE}, {"b.c.", 4, DCH_b_c, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                      
    {"bc", 2, DCH_bc, false, FROM_CHAR_DATE_NONE}, {"cc", 2, DCH_CC, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                           
    {"day", 3, DCH_day, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                       
    {"ddd", 3, DCH_DDD, true, FROM_CHAR_DATE_GREGORIAN}, {"dd", 2, DCH_DD, true, FROM_CHAR_DATE_GREGORIAN}, {"dy", 2, DCH_dy, false, FROM_CHAR_DATE_NONE}, {"d", 1, DCH_D, true, FROM_CHAR_DATE_GREGORIAN}, {"fx", 2, DCH_FX, false, FROM_CHAR_DATE_NONE},                                                                                                                                 
    {"hh24", 4, DCH_HH24, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"hh12", 4, DCH_HH12, true, FROM_CHAR_DATE_NONE}, {"hh", 2, DCH_HH, true, FROM_CHAR_DATE_NONE}, {"iddd", 4, DCH_IDDD, true, FROM_CHAR_DATE_ISOWEEK},                                                                                                                                                                                                                                   
    {"id", 2, DCH_ID, true, FROM_CHAR_DATE_ISOWEEK}, {"iw", 2, DCH_IW, true, FROM_CHAR_DATE_ISOWEEK}, {"iyyy", 4, DCH_IYYY, true, FROM_CHAR_DATE_ISOWEEK}, {"iyy", 3, DCH_IYY, true, FROM_CHAR_DATE_ISOWEEK}, {"iy", 2, DCH_IY, true, FROM_CHAR_DATE_ISOWEEK}, {"i", 1, DCH_I, true, FROM_CHAR_DATE_ISOWEEK}, {"j", 1, DCH_J, true, FROM_CHAR_DATE_NONE},                                  
    {"mi", 2, DCH_MI, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                          
    {"mm", 2, DCH_MM, true, FROM_CHAR_DATE_GREGORIAN}, {"month", 5, DCH_month, false, FROM_CHAR_DATE_GREGORIAN}, {"mon", 3, DCH_mon, false, FROM_CHAR_DATE_GREGORIAN}, {"ms", 2, DCH_MS, true, FROM_CHAR_DATE_NONE}, {"p.m.", 4, DCH_p_m, false, FROM_CHAR_DATE_NONE},                                                                                                                     
    {"pm", 2, DCH_pm, false, FROM_CHAR_DATE_NONE}, {"q", 1, DCH_Q, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                             
    {"rm", 2, DCH_rm, false, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                                                                                    
    {"ssss", 4, DCH_SSSS, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                      
    {"ss", 2, DCH_SS, true, FROM_CHAR_DATE_NONE}, {"tz", 2, DCH_tz, false, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                           
    {"us", 2, DCH_US, true, FROM_CHAR_DATE_NONE},                                                                                                                                                                                                                                                                                                                                          
    {"ww", 2, DCH_WW, true, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                                                                                     
    {"w", 1, DCH_W, true, FROM_CHAR_DATE_GREGORIAN}, {"y,yyy", 5, DCH_Y_YYY, true, FROM_CHAR_DATE_GREGORIAN},                                                                                                                                                                                                                                                                              
    {"yyyy", 4, DCH_YYYY, true, FROM_CHAR_DATE_GREGORIAN}, {"yyy", 3, DCH_YYY, true, FROM_CHAR_DATE_GREGORIAN}, {"yy", 2, DCH_YY, true, FROM_CHAR_DATE_GREGORIAN}, {"y", 1, DCH_Y, true, FROM_CHAR_DATE_GREGORIAN},

              
    {NULL, 0, 0, 0, 0}};

              
                               
   
                                                            
              
   
static const KeyWord NUM_keywords[] = {
                                     
    {",", 1, NUM_COMMA},                                          
    {".", 1, NUM_DEC},                                            
    {"0", 1, NUM_0},                                              
    {"9", 1, NUM_9},                                              
    {"B", 1, NUM_B},                                              
    {"C", 1, NUM_C},                                              
    {"D", 1, NUM_D},                                              
    {"EEEE", 4, NUM_E},                                           
    {"FM", 2, NUM_FM},                                            
    {"G", 1, NUM_G},                                              
    {"L", 1, NUM_L},                                              
    {"MI", 2, NUM_MI},                                            
    {"PL", 2, NUM_PL},                                            
    {"PR", 2, NUM_PR}, {"RN", 2, NUM_RN},                         
    {"SG", 2, NUM_SG},                                            
    {"SP", 2, NUM_SP}, {"S", 1, NUM_S}, {"TH", 2, NUM_TH},        
    {"V", 1, NUM_V},                                              
    {"b", 1, NUM_B},                                              
    {"c", 1, NUM_C},                                              
    {"d", 1, NUM_D},                                              
    {"eeee", 4, NUM_E},                                           
    {"fm", 2, NUM_FM},                                            
    {"g", 1, NUM_G},                                              
    {"l", 1, NUM_L},                                              
    {"mi", 2, NUM_MI},                                            
    {"pl", 2, NUM_PL},                                            
    {"pr", 2, NUM_PR}, {"rn", 2, NUM_rn},                         
    {"sg", 2, NUM_SG},                                            
    {"sp", 2, NUM_SP}, {"s", 1, NUM_S}, {"th", 2, NUM_th},        
    {"v", 1, NUM_V},                                              

              
    {NULL, 0, 0}};

              
                                        
              
   
static const int DCH_index[KeyWord_INDEX_SIZE] = {
       
                        
      
                                               

    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, DCH_A_D, DCH_B_C, DCH_CC, DCH_DAY, -1, DCH_FX, -1, DCH_HH24, DCH_IDDD, DCH_J, -1, -1, DCH_MI, -1, DCH_OF, DCH_P_M, DCH_Q, DCH_RM, DCH_SSSS, DCH_TZH, DCH_US, -1, DCH_WW, -1, DCH_Y_YYY, -1, -1, -1, -1, -1, -1, -1, DCH_a_d, DCH_b_c, DCH_cc, DCH_day, -1, DCH_fx, -1, DCH_hh24, DCH_iddd, DCH_j, -1, -1, DCH_mi, -1, -1, DCH_p_m, DCH_q, DCH_rm, DCH_ssss, DCH_tz, DCH_us, -1, DCH_ww, -1, DCH_y_yyy, -1, -1, -1, -1

                                            
};

              
                                     
              
   
static const int NUM_index[KeyWord_INDEX_SIZE] = {
       
                        
      
                                               

    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, NUM_COMMA, -1, NUM_DEC, -1, NUM_0, -1, -1, -1, -1, -1, -1, -1, -1, NUM_9, -1, -1, -1, -1, -1, -1, -1, -1, NUM_B, NUM_C, NUM_D, NUM_E, NUM_FM, NUM_G, -1, -1, -1, -1, NUM_L, NUM_MI, -1, -1, NUM_PL, -1, NUM_RN, NUM_SG, NUM_TH, -1, NUM_V, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, NUM_b, NUM_c, NUM_d, NUM_e, NUM_fm, NUM_g, -1, -1, -1, -1, NUM_l, NUM_mi, -1, -1, NUM_pl, -1, NUM_rn, NUM_sg, NUM_th, -1, NUM_v, -1, -1, -1, -1, -1, -1, -1

                                            
};

              
                           
              
   
typedef struct NUMProc
{
  bool is_to_char;
  NUMDesc *Num;                          

  int sign,                             
      sign_wrote,                          
      num_count,                                  
      num_in,                                
      num_curr,                                       
      out_pre_spaces,                                

      read_dec,                                       
      read_post,                                       
      read_pre;                                         

  char *number,                               
      *number_p,                                              
      *inout,                              
      *inout_p,                                              
      *last_relevant,                                               

      *L_negative_sign,             
      *L_positive_sign, *decimal, *L_thousands_sep, *L_currency_symbol;
} NUMProc;

              
             
              
   
static const KeyWord *
index_seq_search(const char *str, const KeyWord *kw, const int *index);
static const KeySuffix *
suff_search(const char *str, const KeySuffix *suf, int type);
static bool
is_separator_char(const char *str);
static void
NUMDesc_prepare(NUMDesc *num, FormatNode *n);
static void
parse_format(FormatNode *node, const char *str, const KeyWord *kw, const KeySuffix *suf, const int *index, int ver, NUMDesc *Num);

static void
DCH_to_char(FormatNode *node, bool is_interval, TmToChar *in, char *out, Oid collid);
static void
DCH_from_char(FormatNode *node, const char *in, TmFromChar *out);

#ifdef DEBUG_TO_FROM_CHAR
static void
dump_index(const KeyWord *k, const int *index);
static void
dump_node(FormatNode *node, int max);
#endif

static const char *
get_th(char *num, int type);
static char *
str_numth(char *dest, char *num, int type);
static int
adjust_partial_year_to_2020(int year);
static int
strspace_len(const char *str);
static void
from_char_set_mode(TmFromChar *tmfc, const FromCharDateMode mode);
static void
from_char_set_int(int *dest, const int value, const FormatNode *node);
static int
from_char_parse_int_len(int *dest, const char **src, const int len, FormatNode *node);
static int
from_char_parse_int(int *dest, const char **src, FormatNode *node);
static int
seq_search(const char *name, const char *const *array, int *len);
static int
from_char_seq_search(int *dest, const char **src, const char *const *array, FormatNode *node);
static void
do_to_timestamp(text *date_txt, text *fmt, struct pg_tm *tm, fsec_t *fsec);
static char *
fill_str(char *str, int c, int max);
static FormatNode *
NUM_cache(int len, NUMDesc *Num, text *pars_str, bool *shouldFree);
static char *
int_to_roman(int number);
static void
NUM_prepare_locale(NUMProc *Np);
static char *
get_last_relevant_decnum(char *num);
static void
NUM_numpart_from_char(NUMProc *Np, int id, int input_len);
static void
NUM_numpart_to_char(NUMProc *Np, int id);
static char *
NUM_processor(FormatNode *node, NUMDesc *Num, char *inout, char *number, int input_len, int to_char_out_pre_spaces, int sign, bool is_to_char, Oid collid);
static DCHCacheEntry *
DCH_cache_getnew(const char *str);
static DCHCacheEntry *
DCH_cache_search(const char *str);
static DCHCacheEntry *
DCH_cache_fetch(const char *str);
static NUMCacheEntry *
NUM_cache_getnew(const char *str);
static NUMCacheEntry *
NUM_cache_search(const char *str);
static NUMCacheEntry *
NUM_cache_fetch(const char *str);

              
                                                              
                                                           
                                                   
              
   
static const KeyWord *
index_seq_search(const char *str, const KeyWord *kw, const int *index)
{
  int poz;

  if (!KeyWord_INDEX_FILTER(*str))
  {
    return NULL;
  }

  if ((poz = *(index + (*str - ' '))) > -1)
  {
    const KeyWord *k = kw + poz;

    do
    {
      if (strncmp(str, k->name, k->len) == 0)
      {
        return k;
      }
      k++;
      if (!k->name)
      {
        return NULL;
      }
    } while (*str == *k->name);
  }
  return NULL;
}

static const KeySuffix *
suff_search(const char *str, const KeySuffix *suf, int type)
{
  const KeySuffix *s;

  for (s = suf; s->name != NULL; s++)
  {
    if (s->type != type)
    {
      continue;
    }

    if (strncmp(str, s->name, s->len) == 0)
    {
      return s;
    }
  }
  return NULL;
}

static bool
is_separator_char(const char *str)
{
                                                          
  return (*str > 0x20 && *str < 0x7F && !(*str >= 'A' && *str <= 'Z') && !(*str >= 'a' && *str <= 'z') && !(*str >= '0' && *str <= '9'));
}

              
                                                                     
              
   
static void
NUMDesc_prepare(NUMDesc *num, FormatNode *n)
{
  if (n->type != NODE_TYPE_ACTION)
  {
    return;
  }

  if (IS_EEEE(num) && n->key->id != NUM_E)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"EEEE\" must be the last pattern used")));
  }

  switch (n->key->id)
  {
  case NUM_9:
    if (IS_BRACKET(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"9\" must be ahead of \"PR\"")));
    }
    if (IS_MULTI(num))
    {
      ++num->multi;
      break;
    }
    if (IS_DECIMAL(num))
    {
      ++num->post;
    }
    else
    {
      ++num->pre;
    }
    break;

  case NUM_0:
    if (IS_BRACKET(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"0\" must be ahead of \"PR\"")));
    }
    if (!IS_ZERO(num) && !IS_DECIMAL(num))
    {
      num->flag |= NUM_F_ZERO;
      num->zero_start = num->pre + 1;
    }
    if (!IS_DECIMAL(num))
    {
      ++num->pre;
    }
    else
    {
      ++num->post;
    }

    num->zero_end = num->pre + num->post;
    break;

  case NUM_B:
    if (num->pre == 0 && num->post == 0 && (!IS_ZERO(num)))
    {
      num->flag |= NUM_F_BLANK;
    }
    break;

  case NUM_D:
    num->flag |= NUM_F_LDECIMAL;
    num->need_locale = true;
                     
  case NUM_DEC:
    if (IS_DECIMAL(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("multiple decimal points")));
    }
    if (IS_MULTI(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"V\" and decimal point together")));
    }
    num->flag |= NUM_F_DECIMAL;
    break;

  case NUM_FM:
    num->flag |= NUM_F_FILLMODE;
    break;

  case NUM_S:
    if (IS_LSIGN(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"S\" twice")));
    }
    if (IS_PLUS(num) || IS_MINUS(num) || IS_BRACKET(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"S\" and \"PL\"/\"MI\"/\"SG\"/\"PR\" together")));
    }
    if (!IS_DECIMAL(num))
    {
      num->lsign = NUM_LSIGN_PRE;
      num->pre_lsign_num = num->pre;
      num->need_locale = true;
      num->flag |= NUM_F_LSIGN;
    }
    else if (num->lsign == NUM_LSIGN_NONE)
    {
      num->lsign = NUM_LSIGN_POST;
      num->need_locale = true;
      num->flag |= NUM_F_LSIGN;
    }
    break;

  case NUM_MI:
    if (IS_LSIGN(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"S\" and \"MI\" together")));
    }
    num->flag |= NUM_F_MINUS;
    if (IS_DECIMAL(num))
    {
      num->flag |= NUM_F_MINUS_POST;
    }
    break;

  case NUM_PL:
    if (IS_LSIGN(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"S\" and \"PL\" together")));
    }
    num->flag |= NUM_F_PLUS;
    if (IS_DECIMAL(num))
    {
      num->flag |= NUM_F_PLUS_POST;
    }
    break;

  case NUM_SG:
    if (IS_LSIGN(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"S\" and \"SG\" together")));
    }
    num->flag |= NUM_F_MINUS;
    num->flag |= NUM_F_PLUS;
    break;

  case NUM_PR:
    if (IS_LSIGN(num) || IS_PLUS(num) || IS_MINUS(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"PR\" and \"S\"/\"PL\"/\"MI\"/\"SG\" together")));
    }
    num->flag |= NUM_F_BRACKET;
    break;

  case NUM_rn:
  case NUM_RN:
    num->flag |= NUM_F_ROMAN;
    break;

  case NUM_L:
  case NUM_G:
    num->need_locale = true;
    break;

  case NUM_V:
    if (IS_DECIMAL(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"V\" and decimal point together")));
    }
    num->flag |= NUM_F_MULTI;
    break;

  case NUM_E:
    if (IS_EEEE(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot use \"EEEE\" twice")));
    }
    if (IS_BLANK(num) || IS_FILLMODE(num) || IS_LSIGN(num) || IS_BRACKET(num) || IS_MINUS(num) || IS_PLUS(num) || IS_ROMAN(num) || IS_MULTI(num))
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("\"EEEE\" is incompatible with other formats"), errdetail("\"EEEE\" may only be used together with digit and decimal point patterns.")));
    }
    num->flag |= NUM_F_EEEE;
    break;
  }
}

              
                                                                         
                     
   
                                  
              
   
static void
parse_format(FormatNode *node, const char *str, const KeyWord *kw, const KeySuffix *suf, const int *index, int ver, NUMDesc *Num)
{
  FormatNode *n;

#ifdef DEBUG_TO_FROM_CHAR
  elog(DEBUG_elog_output, "to_char/number(): run parser");
#endif

  n = node;

  while (*str)
  {
    int suffix = 0;
    const KeySuffix *s;

       
              
       
    if (ver == DCH_TYPE && (s = suff_search(str, suf, SUFFTYPE_PREFIX)) != NULL)
    {
      suffix |= s->id;
      if (s->len)
      {
        str += s->len;
      }
    }

       
               
       
    if (*str && (n->key = index_seq_search(str, kw, index)) != NULL)
    {
      n->type = NODE_TYPE_ACTION;
      n->suffix = suffix;
      if (n->key->len)
      {
        str += n->key->len;
      }

         
                                                    
         
      if (ver == NUM_TYPE)
      {
        NUMDesc_prepare(Num, n);
      }

         
                 
         
      if (ver == DCH_TYPE && *str && (s = suff_search(str, suf, SUFFTYPE_POSTFIX)) != NULL)
      {
        n->suffix |= s->id;
        if (s->len)
        {
          str += s->len;
        }
      }

      n++;
    }
    else if (*str)
    {
      int chlen;

         
                                                      
         
      if (*str == '"')
      {
        str++;
        while (*str)
        {
          if (*str == '"')
          {
            str++;
            break;
          }
                                                           
          if (*str == '\\' && *(str + 1))
          {
            str++;
          }
          chlen = pg_mblen(str);
          n->type = NODE_TYPE_CHAR;
          memcpy(n->character, str, chlen);
          n->character[chlen] = '\0';
          n->key = NULL;
          n->suffix = 0;
          n++;
          str += chlen;
        }
      }
      else
      {
           
                                                                       
                                                   
           
        if (*str == '\\' && *(str + 1) == '"')
        {
          str++;
        }
        chlen = pg_mblen(str);

        if (ver == DCH_TYPE && is_separator_char(str))
        {
          n->type = NODE_TYPE_SEPARATOR;
        }
        else if (isspace((unsigned char)*str))
        {
          n->type = NODE_TYPE_SPACE;
        }
        else
        {
          n->type = NODE_TYPE_CHAR;
        }

        memcpy(n->character, str, chlen);
        n->character[chlen] = '\0';
        n->key = NULL;
        n->suffix = 0;
        n++;
        str += chlen;
      }
    }
  }

  n->type = NODE_TYPE_END;
  n->suffix = 0;
}

              
                                           
              
   
#ifdef DEBUG_TO_FROM_CHAR

#define DUMP_THth(_suf) (S_TH(_suf) ? "TH" : (S_th(_suf) ? "th" : " "))
#define DUMP_FM(_suf) (S_FM(_suf) ? "FM" : " ")

static void
dump_node(FormatNode *node, int max)
{
  FormatNode *n;
  int a;

  elog(DEBUG_elog_output, "to_from-char(): DUMP FORMAT");

  for (a = 0, n = node; a <= max; n++, a++)
  {
    if (n->type == NODE_TYPE_ACTION)
    {
      elog(DEBUG_elog_output, "%d:\t NODE_TYPE_ACTION '%s'\t(%s,%s)", a, n->key->name, DUMP_THth(n->suffix), DUMP_FM(n->suffix));
    }
    else if (n->type == NODE_TYPE_CHAR)
    {
      elog(DEBUG_elog_output, "%d:\t NODE_TYPE_CHAR '%s'", a, n->character);
    }
    else if (n->type == NODE_TYPE_END)
    {
      elog(DEBUG_elog_output, "%d:\t NODE_TYPE_END", a);
      return;
    }
    else
    {
      elog(DEBUG_elog_output, "%d:\t unknown NODE!", a);
    }
  }
}
#endif            

                                                                               
                   
                                                                               

              
                                                
                             
              
   
static const char *
get_th(char *num, int type)
{
  int len = strlen(num), last, seclast;

  last = *(num + (len - 1));
  if (!isdigit((unsigned char)last))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("\"%s\" is not a number", num)));
  }

     
                                                                         
                                             
     
  if ((len > 1) && ((seclast = num[len - 2]) == '1'))
  {
    last = 0;
  }

  switch (last)
  {
  case '1':
    if (type == TH_UPPER)
    {
      return numTH[0];
    }
    return numth[0];
  case '2':
    if (type == TH_UPPER)
    {
      return numTH[1];
    }
    return numth[1];
  case '3':
    if (type == TH_UPPER)
    {
      return numTH[2];
    }
    return numth[2];
  default:
    if (type == TH_UPPER)
    {
      return numTH[3];
    }
    return numth[3];
  }
}

              
                                                  
                             
              
   
static char *
str_numth(char *dest, char *num, int type)
{
  if (dest != num)
  {
    strcpy(dest, num);
  }
  strcat(dest, get_th(num, type));
  return dest;
}

                                                                               
                                   
                                                                               

#ifdef USE_ICU

typedef int32_t (*ICU_Convert_Func)(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *pErrorCode);

static int32_t
icu_convert_case(ICU_Convert_Func func, pg_locale_t mylocale, UChar **buff_dest, UChar *buff_source, int32_t len_source)
{
  UErrorCode status;
  int32_t len_dest;

  len_dest = len_source;                                 
  *buff_dest = palloc(len_dest * sizeof(**buff_dest));
  status = U_ZERO_ERROR;
  len_dest = func(*buff_dest, len_dest, buff_source, len_source, mylocale->info.icu.locale, &status);
  if (status == U_BUFFER_OVERFLOW_ERROR)
  {
                                        
    pfree(*buff_dest);
    *buff_dest = palloc(len_dest * sizeof(**buff_dest));
    status = U_ZERO_ERROR;
    len_dest = func(*buff_dest, len_dest, buff_source, len_source, mylocale->info.icu.locale, &status);
  }
  if (U_FAILURE(status))
  {
    ereport(ERROR, (errmsg("case conversion failed: %s", u_errorName(status))));
  }
  return len_dest;
}

static int32_t
u_strToTitle_default_BI(UChar *dest, int32_t destCapacity, const UChar *src, int32_t srcLength, const char *locale, UErrorCode *pErrorCode)
{
  return u_strToTitle(dest, destCapacity, src, srcLength, NULL, locale, pErrorCode);
}

#endif              

   
                                                                               
                                                                              
                                                                       
                                                                             
                                                                             
                                                                              
                
   
                                                                       
                                                                         
                                                                           
                                                                          
                                                        
   

   
                                                        
   
                                                                
                                                                        
   
char *
str_tolower(const char *buff, size_t nbytes, Oid collid)
{
  char *result;

  if (!buff)
  {
    return NULL;
  }

                                                                        
  if (lc_ctype_is_c(collid))
  {
    result = asc_tolower(buff, nbytes);
  }
  else
  {
    pg_locale_t mylocale = 0;

    if (collid != DEFAULT_COLLATION_OID)
    {
      if (!OidIsValid(collid))
      {
           
                                                                    
                                                                   
           
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for %s function", "lower()"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
      mylocale = pg_newlocale_from_collation(collid);
    }

#ifdef USE_ICU
    if (mylocale && mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t len_uchar;
      int32_t len_conv;
      UChar *buff_uchar;
      UChar *buff_conv;

      len_uchar = icu_to_uchar(&buff_uchar, buff, nbytes);
      len_conv = icu_convert_case(u_strToLower, mylocale, &buff_conv, buff_uchar, len_uchar);
      icu_from_uchar(&result, buff_conv, len_conv);
      pfree(buff_uchar);
      pfree(buff_conv);
    }
    else
#endif
    {
      if (pg_database_encoding_max_length() > 1)
      {
        wchar_t *workspace;
        size_t curr_char;
        size_t result_size;

                               
        if ((nbytes + 1) > (INT_MAX / sizeof(wchar_t)))
        {
          ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
        }

                                                                      
        workspace = (wchar_t *)palloc((nbytes + 1) * sizeof(wchar_t));

        char2wchar(workspace, nbytes + 1, buff, nbytes, mylocale);

        for (curr_char = 0; workspace[curr_char] != 0; curr_char++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            workspace[curr_char] = towlower_l(workspace[curr_char], mylocale->info.lt);
          }
          else
#endif
            workspace[curr_char] = towlower(workspace[curr_char]);
        }

           
                                                                     
                    
           
        result_size = curr_char * pg_database_encoding_max_length() + 1;
        result = palloc(result_size);

        wchar2char(result, workspace, result_size, mylocale);
        pfree(workspace);
      }
      else
      {
        char *p;

        result = pnstrdup(buff, nbytes);

           
                                                                     
                                                                      
                                                                      
                                                                   
                                                               
           
        for (p = result; *p; p++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            *p = tolower_l((unsigned char)*p, mylocale->info.lt);
          }
          else
#endif
            *p = pg_tolower((unsigned char)*p);
        }
      }
    }
  }

  return result;
}

   
                                                        
   
                                                                
                                                                        
   
char *
str_toupper(const char *buff, size_t nbytes, Oid collid)
{
  char *result;

  if (!buff)
  {
    return NULL;
  }

                                                                        
  if (lc_ctype_is_c(collid))
  {
    result = asc_toupper(buff, nbytes);
  }
  else
  {
    pg_locale_t mylocale = 0;

    if (collid != DEFAULT_COLLATION_OID)
    {
      if (!OidIsValid(collid))
      {
           
                                                                    
                                                                   
           
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for %s function", "upper()"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
      mylocale = pg_newlocale_from_collation(collid);
    }

#ifdef USE_ICU
    if (mylocale && mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t len_uchar, len_conv;
      UChar *buff_uchar;
      UChar *buff_conv;

      len_uchar = icu_to_uchar(&buff_uchar, buff, nbytes);
      len_conv = icu_convert_case(u_strToUpper, mylocale, &buff_conv, buff_uchar, len_uchar);
      icu_from_uchar(&result, buff_conv, len_conv);
      pfree(buff_uchar);
      pfree(buff_conv);
    }
    else
#endif
    {
      if (pg_database_encoding_max_length() > 1)
      {
        wchar_t *workspace;
        size_t curr_char;
        size_t result_size;

                               
        if ((nbytes + 1) > (INT_MAX / sizeof(wchar_t)))
        {
          ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
        }

                                                                      
        workspace = (wchar_t *)palloc((nbytes + 1) * sizeof(wchar_t));

        char2wchar(workspace, nbytes + 1, buff, nbytes, mylocale);

        for (curr_char = 0; workspace[curr_char] != 0; curr_char++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            workspace[curr_char] = towupper_l(workspace[curr_char], mylocale->info.lt);
          }
          else
#endif
            workspace[curr_char] = towupper(workspace[curr_char]);
        }

           
                                                                     
                    
           
        result_size = curr_char * pg_database_encoding_max_length() + 1;
        result = palloc(result_size);

        wchar2char(result, workspace, result_size, mylocale);
        pfree(workspace);
      }
      else
      {
        char *p;

        result = pnstrdup(buff, nbytes);

           
                                                                     
                                                                      
                                                                      
                                                                   
                                                               
           
        for (p = result; *p; p++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            *p = toupper_l((unsigned char)*p, mylocale->info.lt);
          }
          else
#endif
            *p = pg_toupper((unsigned char)*p);
        }
      }
    }
  }

  return result;
}

   
                                                          
   
                                                                
                                                                        
   
char *
str_initcap(const char *buff, size_t nbytes, Oid collid)
{
  char *result;
  int wasalnum = false;

  if (!buff)
  {
    return NULL;
  }

                                                                        
  if (lc_ctype_is_c(collid))
  {
    result = asc_initcap(buff, nbytes);
  }
  else
  {
    pg_locale_t mylocale = 0;

    if (collid != DEFAULT_COLLATION_OID)
    {
      if (!OidIsValid(collid))
      {
           
                                                                    
                                                                   
           
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for %s function", "initcap()"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
      mylocale = pg_newlocale_from_collation(collid);
    }

#ifdef USE_ICU
    if (mylocale && mylocale->provider == COLLPROVIDER_ICU)
    {
      int32_t len_uchar, len_conv;
      UChar *buff_uchar;
      UChar *buff_conv;

      len_uchar = icu_to_uchar(&buff_uchar, buff, nbytes);
      len_conv = icu_convert_case(u_strToTitle_default_BI, mylocale, &buff_conv, buff_uchar, len_uchar);
      icu_from_uchar(&result, buff_conv, len_conv);
      pfree(buff_uchar);
      pfree(buff_conv);
    }
    else
#endif
    {
      if (pg_database_encoding_max_length() > 1)
      {
        wchar_t *workspace;
        size_t curr_char;
        size_t result_size;

                               
        if ((nbytes + 1) > (INT_MAX / sizeof(wchar_t)))
        {
          ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY), errmsg("out of memory")));
        }

                                                                      
        workspace = (wchar_t *)palloc((nbytes + 1) * sizeof(wchar_t));

        char2wchar(workspace, nbytes + 1, buff, nbytes, mylocale);

        for (curr_char = 0; workspace[curr_char] != 0; curr_char++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            if (wasalnum)
            {
              workspace[curr_char] = towlower_l(workspace[curr_char], mylocale->info.lt);
            }
            else
            {
              workspace[curr_char] = towupper_l(workspace[curr_char], mylocale->info.lt);
            }
            wasalnum = iswalnum_l(workspace[curr_char], mylocale->info.lt);
          }
          else
#endif
          {
            if (wasalnum)
            {
              workspace[curr_char] = towlower(workspace[curr_char]);
            }
            else
            {
              workspace[curr_char] = towupper(workspace[curr_char]);
            }
            wasalnum = iswalnum(workspace[curr_char]);
          }
        }

           
                                                                     
                    
           
        result_size = curr_char * pg_database_encoding_max_length() + 1;
        result = palloc(result_size);

        wchar2char(result, workspace, result_size, mylocale);
        pfree(workspace);
      }
      else
      {
        char *p;

        result = pnstrdup(buff, nbytes);

           
                                                                       
                                                                  
                                                                      
                                                                   
                                                               
           
        for (p = result; *p; p++)
        {
#ifdef HAVE_LOCALE_T
          if (mylocale)
          {
            if (wasalnum)
            {
              *p = tolower_l((unsigned char)*p, mylocale->info.lt);
            }
            else
            {
              *p = toupper_l((unsigned char)*p, mylocale->info.lt);
            }
            wasalnum = isalnum_l((unsigned char)*p, mylocale->info.lt);
          }
          else
#endif
          {
            if (wasalnum)
            {
              *p = pg_tolower((unsigned char)*p);
            }
            else
            {
              *p = pg_toupper((unsigned char)*p);
            }
            wasalnum = isalnum((unsigned char)*p);
          }
        }
      }
    }
  }

  return result;
}

   
                             
   
                                                                
                                                                        
   
char *
asc_tolower(const char *buff, size_t nbytes)
{
  char *result;
  char *p;

  if (!buff)
  {
    return NULL;
  }

  result = pnstrdup(buff, nbytes);

  for (p = result; *p; p++)
  {
    *p = pg_ascii_tolower((unsigned char)*p);
  }

  return result;
}

   
                             
   
                                                                
                                                                        
   
char *
asc_toupper(const char *buff, size_t nbytes)
{
  char *result;
  char *p;

  if (!buff)
  {
    return NULL;
  }

  result = pnstrdup(buff, nbytes);

  for (p = result; *p; p++)
  {
    *p = pg_ascii_toupper((unsigned char)*p);
  }

  return result;
}

   
                               
   
                                                                
                                                                        
   
char *
asc_initcap(const char *buff, size_t nbytes)
{
  char *result;
  char *p;
  int wasalnum = false;

  if (!buff)
  {
    return NULL;
  }

  result = pnstrdup(buff, nbytes);

  for (p = result; *p; p++)
  {
    char c;

    if (wasalnum)
    {
      *p = c = pg_ascii_tolower((unsigned char)*p);
    }
    else
    {
      *p = c = pg_ascii_toupper((unsigned char)*p);
    }
                                       
    wasalnum = ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'));
  }

  return result;
}

                                                                

static char *
str_tolower_z(const char *buff, Oid collid)
{
  return str_tolower(buff, strlen(buff), collid);
}

static char *
str_toupper_z(const char *buff, Oid collid)
{
  return str_toupper(buff, strlen(buff), collid);
}

static char *
str_initcap_z(const char *buff, Oid collid)
{
  return str_initcap(buff, strlen(buff), collid);
}

static char *
asc_tolower_z(const char *buff)
{
  return asc_tolower(buff, strlen(buff));
}

static char *
asc_toupper_z(const char *buff)
{
  return asc_toupper(buff, strlen(buff));
}

                                           

              
                             
   
                                                                     
              
   
#define SKIP_THth(ptr, _suf)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (S_THth(_suf))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      if (*(ptr))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
        (ptr) += pg_mblen(ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      if (*(ptr))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
        (ptr) += pg_mblen(ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

#ifdef DEBUG_TO_FROM_CHAR
               
                                                                  
                                              
              
   
static void
dump_index(const KeyWord *k, const int *index)
{
  int i, count = 0, free_i = 0;

  elog(DEBUG_elog_output, "TO-FROM_CHAR: Dump KeyWord Index:");

  for (i = 0; i < KeyWord_INDEX_SIZE; i++)
  {
    if (index[i] != -1)
    {
      elog(DEBUG_elog_output, "\t%c: %s, ", i + 32, k[index[i]].name);
      count++;
    }
    else
    {
      free_i++;
      elog(DEBUG_elog_output, "\t(%d) %c %d", i, i + 32, index[i]);
    }
  }
  elog(DEBUG_elog_output, "\n\t\tUsed positions: %d,\n\t\tFree positions: %d", count, free_i);
}
#endif            

              
                                                         
              
   
static bool
is_next_separator(FormatNode *n)
{
  if (n->type == NODE_TYPE_END)
  {
    return false;
  }

  if (n->type == NODE_TYPE_ACTION && S_THth(n->suffix))
  {
    return true;
  }

     
               
     
  n++;

                                                                  
  if (n->type == NODE_TYPE_END)
  {
    return true;
  }

  if (n->type == NODE_TYPE_ACTION)
  {
    if (n->key->is_digit)
    {
      return false;
    }

    return true;
  }
  else if (n->character[1] == '\0' && isdigit((unsigned char)n->character[0]))
  {
    return false;
  }

  return true;                                       
}

static int
adjust_partial_year_to_2020(int year)
{
     
                                                                            
                                           
     
                                  
  if (year < 70)
  {
    return year + 2000;
  }
                                   
  else if (year < 100)
  {
    return year + 1900;
  }
                                     
  else if (year < 520)
  {
    return year + 2000;
  }
                                     
  else if (year < 1000)
  {
    return year + 1000;
  }
  else
  {
    return year;
  }
}

static int
strspace_len(const char *str)
{
  int len = 0;

  while (*str && isspace((unsigned char)*str))
  {
    str++;
    len++;
  }
  return len;
}

   
                                                
   
                                                                              
                             
   
static void
from_char_set_mode(TmFromChar *tmfc, const FromCharDateMode mode)
{
  if (mode != FROM_CHAR_DATE_NONE)
  {
    if (tmfc->mode == FROM_CHAR_DATE_NONE)
    {
      tmfc->mode = mode;
    }
    else if (tmfc->mode != mode)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid combination of date conventions"),
                         errhint("Do not mix Gregorian and ISO week date "
                                 "conventions in a formatting template.")));
    }
  }
}

   
                                                            
   
                                                                         
                   
   
static void
from_char_set_int(int *dest, const int value, const FormatNode *node)
{
  if (*dest != 0 && *dest != value)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("conflicting values for \"%s\" field in formatting string", node->key->name),
                       errdetail("This value contradicts a previous setting for "
                                 "the same field type.")));
  }
  *dest = value;
}

   
                                                                            
                                                       
   
                                                                               
                                                                              
   
                                                                           
                                         
   
                                                                            
                                                                               
               
   
                                             
   
                                                                            
                                                                               
                    
   
static int
from_char_parse_int_len(int *dest, const char **src, const int len, FormatNode *node)
{
  long result;
  char copy[DCH_MAX_ITEM_SIZ + 1];
  const char *init = *src;
  int used;

     
                                                     
     
  *src += strspace_len(*src);

  Assert(len <= DCH_MAX_ITEM_SIZ);
  used = (int)strlcpy(copy, *src, len + 1);

  if (S_FM(node->suffix) || is_next_separator(node))
  {
       
                                                                    
                                                                           
       
    char *endptr;

    errno = 0;
    result = strtol(init, &endptr, 10);
    *src = endptr;
  }
  else
  {
       
                                                                           
                                         
       
    char *last;

    if (used < len)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("source string too short for \"%s\" formatting field", node->key->name),
                         errdetail("Field requires %d characters, but only %d "
                                   "remain.",
                             len, used),
                         errhint("If your source string is not fixed-width, try "
                                 "using the \"FM\" modifier.")));
    }

    errno = 0;
    result = strtol(copy, &last, 10);
    used = last - copy;

    if (used > 0 && used < len)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid value \"%s\" for \"%s\"", copy, node->key->name),
                         errdetail("Field requires %d characters, but only %d "
                                   "could be parsed.",
                             len, used),
                         errhint("If your source string is not fixed-width, try "
                                 "using the \"FM\" modifier.")));
    }

    *src += used;
  }

  if (*src == init)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid value \"%s\" for \"%s\"", copy, node->key->name), errdetail("Value must be an integer.")));
  }

  if (errno == ERANGE || result < INT_MIN || result > INT_MAX)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("value for \"%s\" in source string is out of range", node->key->name), errdetail("Value must be in the range %d to %d.", INT_MIN, INT_MAX)));
  }

  if (dest != NULL)
  {
    from_char_set_int(dest, (int)result, node);
  }
  return *src - init;
}

   
                                                                             
                                     
   
                                                                           
                                                                               
                                                                        
                               
   
static int
from_char_parse_int(int *dest, const char **src, FormatNode *node)
{
  return from_char_parse_int_len(dest, src, node->key->len, node);
}

   
                                                                            
                                          
   
                                                     
   
                                                              
   
                                                                 
                                              
   
static int
seq_search(const char *name, const char *const *array, int *len)
{
  unsigned char firstc;
  const char *const *a;

  *len = 0;

                                         
  if (!*name)
  {
    return -1;
  }

                                                         
  firstc = pg_tolower((unsigned char)*name);

  for (a = array; *a != NULL; a++)
  {
    const char *p;
    const char *n;

                             
    if (pg_tolower((unsigned char)**a) != firstc)
    {
      continue;
    }

                                
    for (p = *a + 1, n = name + 1;; p++, n++)
    {
                                                          
      if (*p == '\0')
      {
        *len = n - name;
        return a - array;
      }
                                                           
      if (*n == '\0')
      {
        break;
      }
                                 
      if (pg_tolower((unsigned char)*p) != pg_tolower((unsigned char)*n))
      {
        break;
      }
    }
  }

  return -1;
}

   
                                                                          
                                                        
   
                                                                           
                                                                            
                                                                
   
                                                
   
                                                                         
                                     
   
static int
from_char_seq_search(int *dest, const char **src, const char *const *array, FormatNode *node)
{
  int len;

  *dest = seq_search(*src, array, &len);

  if (len <= 0)
  {
       
                                                                           
                                                
       
    char *copy = pstrdup(*src);
    char *c;

    for (c = copy; *c; c++)
    {
      if (scanner_isspace(*c))
      {
        *c = '\0';
        break;
      }
    }

    ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid value \"%s\" for \"%s\"", copy, node->key->name),
                       errdetail("The given value did not match any of the allowed "
                                 "values for this field.")));
  }
  *src += len;
  return len;
}

              
                                                                  
                                                                    
              
   
static void
DCH_to_char(FormatNode *node, bool is_interval, TmToChar *in, char *out, Oid collid)
{
  FormatNode *n;
  char *s;
  struct pg_tm *tm = &in->tm;
  int i;

                                       
  cache_locale_time();

  s = out;
  for (n = node; n->type != NODE_TYPE_END; n++)
  {
    if (n->type != NODE_TYPE_ACTION)
    {
      strcpy(s, n->character);
      s += strlen(s);
      continue;
    }

    switch (n->key->id)
    {
    case DCH_A_M:
    case DCH_P_M:
      strcpy(s, (tm->tm_hour % HOURS_PER_DAY >= HOURS_PER_DAY / 2) ? P_M_STR : A_M_STR);
      s += strlen(s);
      break;
    case DCH_AM:
    case DCH_PM:
      strcpy(s, (tm->tm_hour % HOURS_PER_DAY >= HOURS_PER_DAY / 2) ? PM_STR : AM_STR);
      s += strlen(s);
      break;
    case DCH_a_m:
    case DCH_p_m:
      strcpy(s, (tm->tm_hour % HOURS_PER_DAY >= HOURS_PER_DAY / 2) ? p_m_STR : a_m_STR);
      s += strlen(s);
      break;
    case DCH_am:
    case DCH_pm:
      strcpy(s, (tm->tm_hour % HOURS_PER_DAY >= HOURS_PER_DAY / 2) ? pm_STR : am_STR);
      s += strlen(s);
      break;
    case DCH_HH:
    case DCH_HH12:

         
                                                            
                   
         
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (tm->tm_hour >= 0) ? 2 : 3, tm->tm_hour % (HOURS_PER_DAY / 2) == 0 ? HOURS_PER_DAY / 2 : tm->tm_hour % (HOURS_PER_DAY / 2));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_HH24:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (tm->tm_hour >= 0) ? 2 : 3, tm->tm_hour);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_MI:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (tm->tm_min >= 0) ? 2 : 3, tm->tm_min);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_SS:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (tm->tm_sec >= 0) ? 2 : 3, tm->tm_sec);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_MS:                  
      sprintf(s, "%03d", (int)(in->fsec / INT64CONST(1000)));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_US:                  
      sprintf(s, "%06d", (int)in->fsec);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_SSSS:
      sprintf(s, "%d", tm->tm_hour * SECS_PER_HOUR + tm->tm_min * SECS_PER_MINUTE + tm->tm_sec);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_tz:
      INVALID_FOR_INTERVAL;
      if (tmtcTzn(in))
      {
                                                                 
        char *p = asc_tolower_z(tmtcTzn(in));

        strcpy(s, p);
        pfree(p);
        s += strlen(s);
      }
      break;
    case DCH_TZ:
      INVALID_FOR_INTERVAL;
      if (tmtcTzn(in))
      {
        strcpy(s, tmtcTzn(in));
        s += strlen(s);
      }
      break;
    case DCH_TZH:
      INVALID_FOR_INTERVAL;
      sprintf(s, "%c%02d", (tm->tm_gmtoff >= 0) ? '+' : '-', abs((int)tm->tm_gmtoff) / SECS_PER_HOUR);
      s += strlen(s);
      break;
    case DCH_TZM:
      INVALID_FOR_INTERVAL;
      sprintf(s, "%02d", (abs((int)tm->tm_gmtoff) % SECS_PER_HOUR) / SECS_PER_MINUTE);
      s += strlen(s);
      break;
    case DCH_OF:
      INVALID_FOR_INTERVAL;
      sprintf(s, "%c%0*d", (tm->tm_gmtoff >= 0) ? '+' : '-', S_FM(n->suffix) ? 0 : 2, abs((int)tm->tm_gmtoff) / SECS_PER_HOUR);
      s += strlen(s);
      if (abs((int)tm->tm_gmtoff) % SECS_PER_HOUR != 0)
      {
        sprintf(s, ":%02d", (abs((int)tm->tm_gmtoff) % SECS_PER_HOUR) / SECS_PER_MINUTE);
        s += strlen(s);
      }
      break;
    case DCH_A_D:
    case DCH_B_C:
      INVALID_FOR_INTERVAL;
      strcpy(s, (tm->tm_year <= 0 ? B_C_STR : A_D_STR));
      s += strlen(s);
      break;
    case DCH_AD:
    case DCH_BC:
      INVALID_FOR_INTERVAL;
      strcpy(s, (tm->tm_year <= 0 ? BC_STR : AD_STR));
      s += strlen(s);
      break;
    case DCH_a_d:
    case DCH_b_c:
      INVALID_FOR_INTERVAL;
      strcpy(s, (tm->tm_year <= 0 ? b_c_STR : a_d_STR));
      s += strlen(s);
      break;
    case DCH_ad:
    case DCH_bc:
      INVALID_FOR_INTERVAL;
      strcpy(s, (tm->tm_year <= 0 ? bc_STR : ad_STR));
      s += strlen(s);
      break;
    case DCH_MONTH:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_toupper_z(localized_full_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, asc_toupper_z(months_full[tm->tm_mon - 1]));
      }
      s += strlen(s);
      break;
    case DCH_Month:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_initcap_z(localized_full_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, months_full[tm->tm_mon - 1]);
      }
      s += strlen(s);
      break;
    case DCH_month:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_tolower_z(localized_full_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, asc_tolower_z(months_full[tm->tm_mon - 1]));
      }
      s += strlen(s);
      break;
    case DCH_MON:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_toupper_z(localized_abbrev_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, asc_toupper_z(months[tm->tm_mon - 1]));
      }
      s += strlen(s);
      break;
    case DCH_Mon:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_initcap_z(localized_abbrev_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, months[tm->tm_mon - 1]);
      }
      s += strlen(s);
      break;
    case DCH_mon:
      INVALID_FOR_INTERVAL;
      if (!tm->tm_mon)
      {
        break;
      }
      if (S_TM(n->suffix))
      {
        char *str = str_tolower_z(localized_abbrev_months[tm->tm_mon - 1], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, asc_tolower_z(months[tm->tm_mon - 1]));
      }
      s += strlen(s);
      break;
    case DCH_MM:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (tm->tm_mon >= 0) ? 2 : 3, tm->tm_mon);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_DAY:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_toupper_z(localized_full_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, asc_toupper_z(days[tm->tm_wday]));
      }
      s += strlen(s);
      break;
    case DCH_Day:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_initcap_z(localized_full_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, days[tm->tm_wday]);
      }
      s += strlen(s);
      break;
    case DCH_day:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_tolower_z(localized_full_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -9, asc_tolower_z(days[tm->tm_wday]));
      }
      s += strlen(s);
      break;
    case DCH_DY:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_toupper_z(localized_abbrev_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, asc_toupper_z(days_short[tm->tm_wday]));
      }
      s += strlen(s);
      break;
    case DCH_Dy:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_initcap_z(localized_abbrev_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, days_short[tm->tm_wday]);
      }
      s += strlen(s);
      break;
    case DCH_dy:
      INVALID_FOR_INTERVAL;
      if (S_TM(n->suffix))
      {
        char *str = str_tolower_z(localized_abbrev_days[tm->tm_wday], collid);

        if (strlen(str) <= (n->key->len + TM_SUFFIX_LEN) * DCH_MAX_ITEM_SIZ)
        {
          strcpy(s, str);
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("localized string format value too long")));
        }
      }
      else
      {
        strcpy(s, asc_tolower_z(days_short[tm->tm_wday]));
      }
      s += strlen(s);
      break;
    case DCH_DDD:
    case DCH_IDDD:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : 3, (n->key->id == DCH_DDD) ? tm->tm_yday : date2isoyearday(tm->tm_year, tm->tm_mon, tm->tm_mday));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_DD:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : 2, tm->tm_mday);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_D:
      INVALID_FOR_INTERVAL;
      sprintf(s, "%d", tm->tm_wday + 1);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_ID:
      INVALID_FOR_INTERVAL;
      sprintf(s, "%d", (tm->tm_wday == 0) ? 7 : tm->tm_wday);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_WW:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : 2, (tm->tm_yday - 1) / 7 + 1);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_IW:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : 2, date2isoweek(tm->tm_year, tm->tm_mon, tm->tm_mday));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_Q:
      if (!tm->tm_mon)
      {
        break;
      }
      sprintf(s, "%d", (tm->tm_mon - 1) / 3 + 1);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_CC:
      if (is_interval)                           
      {
        i = tm->tm_year / 100;
      }
      else
      {
        if (tm->tm_year > 0)
        {
                                         
          i = (tm->tm_year - 1) / 100 + 1;
        }
        else
        {
                                            
          i = tm->tm_year / 100 - 1;
        }
      }
      if (i <= 99 && i >= -99)
      {
        sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (i >= 0) ? 2 : 3, i);
      }
      else
      {
        sprintf(s, "%d", i);
      }
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_Y_YYY:
      i = ADJUST_YEAR(tm->tm_year, is_interval) / 1000;
      sprintf(s, "%d,%03d", i, ADJUST_YEAR(tm->tm_year, is_interval) - (i * 1000));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_YYYY:
    case DCH_IYYY:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (ADJUST_YEAR(tm->tm_year, is_interval) >= 0) ? 4 : 5, (n->key->id == DCH_YYYY ? ADJUST_YEAR(tm->tm_year, is_interval) : ADJUST_YEAR(date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday), is_interval)));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_YYY:
    case DCH_IYY:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (ADJUST_YEAR(tm->tm_year, is_interval) >= 0) ? 3 : 4, (n->key->id == DCH_YYY ? ADJUST_YEAR(tm->tm_year, is_interval) : ADJUST_YEAR(date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday), is_interval)) % 1000);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_YY:
    case DCH_IY:
      sprintf(s, "%0*d", S_FM(n->suffix) ? 0 : (ADJUST_YEAR(tm->tm_year, is_interval) >= 0) ? 2 : 3, (n->key->id == DCH_YY ? ADJUST_YEAR(tm->tm_year, is_interval) : ADJUST_YEAR(date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday), is_interval)) % 100);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_Y:
    case DCH_I:
      sprintf(s, "%1d", (n->key->id == DCH_Y ? ADJUST_YEAR(tm->tm_year, is_interval) : ADJUST_YEAR(date2isoyear(tm->tm_year, tm->tm_mon, tm->tm_mday), is_interval)) % 10);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_RM:
                       
    case DCH_rm:

         
                                                                    
                                                           
         
      if (!tm->tm_mon && !tm->tm_year)
      {
        break;
      }
      else
      {
        int mon = 0;
        const char *const *months;

        if (n->key->id == DCH_RM)
        {
          months = rm_months_upper;
        }
        else
        {
          months = rm_months_lower;
        }

           
                                                                  
                                                                 
                                         
           
        if (tm->tm_mon == 0)
        {
             
                                                               
                             
             
          mon = tm->tm_year >= 0 ? 0 : MONTHS_PER_YEAR - 1;
        }
        else if (tm->tm_mon < 0)
        {
             
                                                              
                                                             
                  
             
          mon = -1 * (tm->tm_mon + 1);
        }
        else
        {
             
                                                               
                                                             
                     
             
          mon = MONTHS_PER_YEAR - tm->tm_mon;
        }

        sprintf(s, "%*s", S_FM(n->suffix) ? 0 : -4, months[mon]);
        s += strlen(s);
      }
      break;
    case DCH_W:
      sprintf(s, "%d", (tm->tm_mday - 1) / 7 + 1);
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    case DCH_J:
      sprintf(s, "%d", date2j(tm->tm_year, tm->tm_mon, tm->tm_mday));
      if (S_THth(n->suffix))
      {
        str_numth(s, s, S_TH_TYPE(n->suffix));
      }
      s += strlen(s);
      break;
    }
  }

  *s = '\0';
}

              
                                                         
                                                                            
   
                                                                      
                                                    
              
   
static void
DCH_from_char(FormatNode *node, const char *in, TmFromChar *out)
{
  FormatNode *n;
  const char *s;
  int len, value;
  bool fx_mode = false;

                                                                             
  int extra_skip = 0;

  for (n = node, s = in; n->type != NODE_TYPE_END && *s != '\0'; n++)
  {
       
                                                                           
                                     
       
    if (!fx_mode && (n->type != NODE_TYPE_ACTION || n->key->id != DCH_FX) && (n->type == NODE_TYPE_ACTION || n == node))
    {
      while (*s != '\0' && isspace((unsigned char)*s))
      {
        s++;
        extra_skip++;
      }
    }

    if (n->type == NODE_TYPE_SPACE || n->type == NODE_TYPE_SEPARATOR)
    {
      if (!fx_mode)
      {
           
                                                                    
                                                                      
                                                                     
                                             
           
        extra_skip--;
        if (isspace((unsigned char)*s) || is_separator_char(s))
        {
          s++;
          extra_skip++;
        }
      }
      else
      {
           
                                                                      
                                                                     
                                                                 
                      
           
        s += pg_mblen(s);
      }
      continue;
    }
    else if (n->type != NODE_TYPE_ACTION)
    {
         
                                                                     
                                                                      
                             
         
      if (!fx_mode)
      {
           
                                                                      
                                                                   
                                                                       
                             
           
        if (extra_skip > 0)
        {
          extra_skip--;
        }
        else
        {
          s += pg_mblen(s);
        }
      }
      else
      {
        s += pg_mblen(s);
      }
      continue;
    }

    from_char_set_mode(out, n->key->date_mode);

    switch (n->key->id)
    {
    case DCH_FX:
      fx_mode = true;
      break;
    case DCH_A_M:
    case DCH_P_M:
    case DCH_a_m:
    case DCH_p_m:
      from_char_seq_search(&value, &s, ampm_strings_long, n);
      from_char_set_int(&out->pm, value % 2, n);
      out->clock = CLOCK_12_HOUR;
      break;
    case DCH_AM:
    case DCH_PM:
    case DCH_am:
    case DCH_pm:
      from_char_seq_search(&value, &s, ampm_strings, n);
      from_char_set_int(&out->pm, value % 2, n);
      out->clock = CLOCK_12_HOUR;
      break;
    case DCH_HH:
    case DCH_HH12:
      from_char_parse_int_len(&out->hh, &s, 2, n);
      out->clock = CLOCK_12_HOUR;
      SKIP_THth(s, n->suffix);
      break;
    case DCH_HH24:
      from_char_parse_int_len(&out->hh, &s, 2, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_MI:
      from_char_parse_int(&out->mi, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_SS:
      from_char_parse_int(&out->ss, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_MS:                  
      len = from_char_parse_int_len(&out->ms, &s, 3, n);

         
                                                                   
         
      out->ms *= len == 1 ? 100 : len == 2 ? 10 : 1;

      SKIP_THth(s, n->suffix);
      break;
    case DCH_US:                  
      len = from_char_parse_int_len(&out->us, &s, 6, n);

      out->us *= len == 1 ? 100000 : len == 2 ? 10000 : len == 3 ? 1000 : len == 4 ? 100 : len == 5 ? 10 : 1;

      SKIP_THth(s, n->suffix);
      break;
    case DCH_SSSS:
      from_char_parse_int(&out->ssss, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_tz:
    case DCH_TZ:
    case DCH_OF:
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("formatting field \"%s\" is only supported in to_char", n->key->name)));
      break;
    case DCH_TZH:

         
                                                                   
                                                                    
                                                              
                                                                   
              
         
      if (*s == '+' || *s == '-' || *s == ' ')
      {
        out->tzsign = *s == '-' ? -1 : +1;
        s++;
      }
      else
      {
        if (extra_skip > 0 && *(s - 1) == '-')
        {
          out->tzsign = -1;
        }
        else
        {
          out->tzsign = +1;
        }
      }

      from_char_parse_int_len(&out->tzh, &s, 2, n);
      break;
    case DCH_TZM:
                                                                    
      if (!out->tzsign)
      {
        out->tzsign = +1;
      }
      from_char_parse_int_len(&out->tzm, &s, 2, n);
      break;
    case DCH_A_D:
    case DCH_B_C:
    case DCH_a_d:
    case DCH_b_c:
      from_char_seq_search(&value, &s, adbc_strings_long, n);
      from_char_set_int(&out->bc, value % 2, n);
      break;
    case DCH_AD:
    case DCH_BC:
    case DCH_ad:
    case DCH_bc:
      from_char_seq_search(&value, &s, adbc_strings, n);
      from_char_set_int(&out->bc, value % 2, n);
      break;
    case DCH_MONTH:
    case DCH_Month:
    case DCH_month:
      from_char_seq_search(&value, &s, months_full, n);
      from_char_set_int(&out->mm, value + 1, n);
      break;
    case DCH_MON:
    case DCH_Mon:
    case DCH_mon:
      from_char_seq_search(&value, &s, months, n);
      from_char_set_int(&out->mm, value + 1, n);
      break;
    case DCH_MM:
      from_char_parse_int(&out->mm, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_DAY:
    case DCH_Day:
    case DCH_day:
      from_char_seq_search(&value, &s, days, n);
      from_char_set_int(&out->d, value, n);
      out->d++;
      break;
    case DCH_DY:
    case DCH_Dy:
    case DCH_dy:
      from_char_seq_search(&value, &s, days_short, n);
      from_char_set_int(&out->d, value, n);
      out->d++;
      break;
    case DCH_DDD:
      from_char_parse_int(&out->ddd, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_IDDD:
      from_char_parse_int_len(&out->ddd, &s, 3, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_DD:
      from_char_parse_int(&out->dd, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_D:
      from_char_parse_int(&out->d, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_ID:
      from_char_parse_int_len(&out->d, &s, 1, n);
                                                               
      if (++out->d > 7)
      {
        out->d = 1;
      }
      SKIP_THth(s, n->suffix);
      break;
    case DCH_WW:
    case DCH_IW:
      from_char_parse_int(&out->ww, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_Q:

         
                                                                     
                                                                   
                                                               
                                                                     
                         
         
                                                                 
                                         
         
      from_char_parse_int((int *)NULL, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_CC:
      from_char_parse_int(&out->cc, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_Y_YYY:
    {
      int matched, years, millennia, nch;

      matched = sscanf(s, "%d,%03d%n", &millennia, &years, &nch);
      if (matched < 2)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("invalid input string for \"Y,YYY\"")));
      }
      years += (millennia * 1000);
      from_char_set_int(&out->year, years, n);
      out->yysz = 4;
      s += nch;
      SKIP_THth(s, n->suffix);
    }
    break;
    case DCH_YYYY:
    case DCH_IYYY:
      from_char_parse_int(&out->year, &s, n);
      out->yysz = 4;
      SKIP_THth(s, n->suffix);
      break;
    case DCH_YYY:
    case DCH_IYY:
      if (from_char_parse_int(&out->year, &s, n) < 4)
      {
        out->year = adjust_partial_year_to_2020(out->year);
      }
      out->yysz = 3;
      SKIP_THth(s, n->suffix);
      break;
    case DCH_YY:
    case DCH_IY:
      if (from_char_parse_int(&out->year, &s, n) < 4)
      {
        out->year = adjust_partial_year_to_2020(out->year);
      }
      out->yysz = 2;
      SKIP_THth(s, n->suffix);
      break;
    case DCH_Y:
    case DCH_I:
      if (from_char_parse_int(&out->year, &s, n) < 4)
      {
        out->year = adjust_partial_year_to_2020(out->year);
      }
      out->yysz = 1;
      SKIP_THth(s, n->suffix);
      break;
    case DCH_RM:
    case DCH_rm:
      from_char_seq_search(&value, &s, rm_months_lower, n);
      from_char_set_int(&out->mm, MONTHS_PER_YEAR - value, n);
      break;
    case DCH_W:
      from_char_parse_int(&out->w, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    case DCH_J:
      from_char_parse_int(&out->j, &s, n);
      SKIP_THth(s, n->suffix);
      break;
    }

                                        
    if (!fx_mode)
    {
      extra_skip = 0;
      while (*s != '\0' && isspace((unsigned char)*s))
      {
        s++;
        extra_skip++;
      }
    }
  }
}

   
                                                                            
                                                                            
                                                                          
                                                                           
                             
   
static inline void
DCH_prevent_counter_overflow(void)
{
  if (DCHCounter >= (INT_MAX - 1))
  {
    for (int i = 0; i < n_DCHCache; i++)
    {
      DCHCache[i]->age >>= 1;
    }
    DCHCounter >>= 1;
  }
}

                                                             
static DCHCacheEntry *
DCH_cache_getnew(const char *str)
{
  DCHCacheEntry *ent;

                                              
  DCH_prevent_counter_overflow();

     
                                                                            
     
  if (n_DCHCache >= DCH_CACHE_ENTRIES)
  {
    DCHCacheEntry *old = DCHCache[0];

#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "cache is full (%d)", n_DCHCache);
#endif
    if (old->valid)
    {
      for (int i = 1; i < DCH_CACHE_ENTRIES; i++)
      {
        ent = DCHCache[i];
        if (!ent->valid)
        {
          old = ent;
          break;
        }
        if (ent->age < old->age)
        {
          old = ent;
        }
      }
    }
#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "OLD: '%s' AGE: %d", old->str, old->age);
#endif
    old->valid = false;
    StrNCpy(old->str, str, DCH_CACHE_SIZE + 1);
    old->age = (++DCHCounter);
                                                           
    return old;
  }
  else
  {
#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "NEW (%d)", n_DCHCache);
#endif
    Assert(DCHCache[n_DCHCache] == NULL);
    DCHCache[n_DCHCache] = ent = (DCHCacheEntry *)MemoryContextAllocZero(TopMemoryContext, sizeof(DCHCacheEntry));
    ent->valid = false;
    StrNCpy(ent->str, str, DCH_CACHE_SIZE + 1);
    ent->age = (++DCHCounter);
                                                           
    ++n_DCHCache;
    return ent;
  }
}

                                                                          
static DCHCacheEntry *
DCH_cache_search(const char *str)
{
                                              
  DCH_prevent_counter_overflow();

  for (int i = 0; i < n_DCHCache; i++)
  {
    DCHCacheEntry *ent = DCHCache[i];

    if (ent->valid && strcmp(ent->str, str) == 0)
    {
      ent->age = (++DCHCounter);
      return ent;
    }
  }

  return NULL;
}

                                                                 
static DCHCacheEntry *
DCH_cache_fetch(const char *str)
{
  DCHCacheEntry *ent;

  if ((ent = DCH_cache_search(str)) == NULL)
  {
       
                                                                          
                                                                   
                 
       
    ent = DCH_cache_getnew(str);

    parse_format(ent->format, str, DCH_keywords, DCH_suff, DCH_index, DCH_TYPE, NULL);

    ent->valid = true;
  }
  return ent;
}

   
                                                                  
                                                                                
                   
   
static text *
datetime_to_char_body(TmToChar *tmtc, text *fmt, bool is_interval, Oid collid)
{
  FormatNode *format;
  char *fmt_str, *result;
  bool incache;
  int fmt_len;
  text *res;

     
                             
     
  fmt_str = text_to_cstring(fmt);
  fmt_len = strlen(fmt_str);

     
                                               
     
  result = palloc((fmt_len * DCH_MAX_ITEM_SIZ) + 1);
  *result = '\0';

  if (fmt_len > DCH_CACHE_SIZE)
  {
       
                                                                         
                                                 
       
    incache = false;

    format = (FormatNode *)palloc((fmt_len + 1) * sizeof(FormatNode));

    parse_format(format, fmt_str, DCH_keywords, DCH_suff, DCH_index, DCH_TYPE, NULL);
  }
  else
  {
       
                         
       
    DCHCacheEntry *ent = DCH_cache_fetch(fmt_str);

    incache = true;
    format = ent->format;
  }

                             
  DCH_to_char(format, is_interval, tmtc, result, collid);

  if (!incache)
  {
    pfree(format);
  }

  pfree(fmt_str);

                                              
  res = cstring_to_text(result);

  pfree(result);
  return res;
}

                                                                              
                      
                                                                             

                       
                       
                       
   
Datum
timestamp_to_char(PG_FUNCTION_ARGS)
{
  Timestamp dt = PG_GETARG_TIMESTAMP(0);
  text *fmt = PG_GETARG_TEXT_PP(1), *res;
  TmToChar tmtc;
  struct pg_tm *tm;
  int thisdate;

  if (VARSIZE_ANY_EXHDR(fmt) <= 0 || TIMESTAMP_NOT_FINITE(dt))
  {
    PG_RETURN_NULL();
  }

  ZERO_tmtc(&tmtc);
  tm = tmtcTm(&tmtc);

  if (timestamp2tm(dt, NULL, tm, &tmtcFsec(&tmtc), NULL, NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  thisdate = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
  tm->tm_wday = (thisdate + 1) % 7;
  tm->tm_yday = thisdate - date2j(tm->tm_year, 1, 1) + 1;

  if (!(res = datetime_to_char_body(&tmtc, fmt, false, PG_GET_COLLATION())))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(res);
}

Datum
timestamptz_to_char(PG_FUNCTION_ARGS)
{
  TimestampTz dt = PG_GETARG_TIMESTAMP(0);
  text *fmt = PG_GETARG_TEXT_PP(1), *res;
  TmToChar tmtc;
  int tz;
  struct pg_tm *tm;
  int thisdate;

  if (VARSIZE_ANY_EXHDR(fmt) <= 0 || TIMESTAMP_NOT_FINITE(dt))
  {
    PG_RETURN_NULL();
  }

  ZERO_tmtc(&tmtc);
  tm = tmtcTm(&tmtc);

  if (timestamp2tm(dt, &tz, tm, &tmtcFsec(&tmtc), &tmtcTzn(&tmtc), NULL) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  thisdate = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
  tm->tm_wday = (thisdate + 1) % 7;
  tm->tm_yday = thisdate - date2j(tm->tm_year, 1, 1) + 1;

  if (!(res = datetime_to_char_body(&tmtc, fmt, false, PG_GET_COLLATION())))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(res);
}

                       
                      
                       
   
Datum
interval_to_char(PG_FUNCTION_ARGS)
{
  Interval *it = PG_GETARG_INTERVAL_P(0);
  text *fmt = PG_GETARG_TEXT_PP(1), *res;
  TmToChar tmtc;
  struct pg_tm *tm;

  if (VARSIZE_ANY_EXHDR(fmt) <= 0)
  {
    PG_RETURN_NULL();
  }

  ZERO_tmtc(&tmtc);
  tm = tmtcTm(&tmtc);

  if (interval2tm(*it, tm, &tmtcFsec(&tmtc)) != 0)
  {
    PG_RETURN_NULL();
  }

                                                                     
  tm->tm_yday = (tm->tm_year * MONTHS_PER_YEAR + tm->tm_mon) * DAYS_PER_MONTH + tm->tm_mday;

  if (!(res = datetime_to_char_body(&tmtc, fmt, true, PG_GET_COLLATION())))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_TEXT_P(res);
}

                         
                  
   
                                                                     
                                         
                         
   
Datum
to_timestamp(PG_FUNCTION_ARGS)
{
  text *date_txt = PG_GETARG_TEXT_PP(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  Timestamp result;
  int tz;
  struct pg_tm tm;
  fsec_t fsec;

  do_to_timestamp(date_txt, fmt, &tm, &fsec);

                                            
  if (tm.tm_zone)
  {
    int dterr = DecodeTimezone(unconstify(char *, tm.tm_zone), &tz);

    if (dterr)
    {
      DateTimeParseError(dterr, text_to_cstring(date_txt), "timestamptz");
    }
  }
  else
  {
    tz = DetermineTimeZoneOffset(&tm, session_timezone);
  }

  if (tm2timestamp(&tm, fsec, &tz, &result) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("timestamp out of range")));
  }

  PG_RETURN_TIMESTAMP(result);
}

              
           
                                                                
              
   
Datum
to_date(PG_FUNCTION_ARGS)
{
  text *date_txt = PG_GETARG_TEXT_PP(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  DateADT result;
  struct pg_tm tm;
  fsec_t fsec;

  do_to_timestamp(date_txt, fmt, &tm, &fsec);

                                               
  if (!IS_VALID_JULIAN(tm.tm_year, tm.tm_mon, tm.tm_mday))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: \"%s\"", text_to_cstring(date_txt))));
  }

  result = date2j(tm.tm_year, tm.tm_mon, tm.tm_mday) - POSTGRES_EPOCH_JDATE;

                                             
  if (!IS_VALID_DATE(result))
  {
    ereport(ERROR, (errcode(ERRCODE_DATETIME_VALUE_OUT_OF_RANGE), errmsg("date out of range: \"%s\"", text_to_cstring(date_txt))));
  }

  PG_RETURN_DATEADT(result);
}

   
                                                             
   
                                                                             
                           
   
                                                                      
                                                                      
               
   
                                                                           
                           
   
static void
do_to_timestamp(text *date_txt, text *fmt, struct pg_tm *tm, fsec_t *fsec)
{
  FormatNode *format;
  TmFromChar tmfc;
  int fmt_len;
  char *date_str;
  int fmask;

  date_str = text_to_cstring(date_txt);

  ZERO_tmfc(&tmfc);
  ZERO_tm(tm);
  *fsec = 0;
  fmask = 0;                                  

  fmt_len = VARSIZE_ANY_EXHDR(fmt);

  if (fmt_len)
  {
    char *fmt_str;
    bool incache;

    fmt_str = text_to_cstring(fmt);

    if (fmt_len > DCH_CACHE_SIZE)
    {
         
                                                                     
                                                         
         
      incache = false;

      format = (FormatNode *)palloc((fmt_len + 1) * sizeof(FormatNode));

      parse_format(format, fmt_str, DCH_keywords, DCH_suff, DCH_index, DCH_TYPE, NULL);
    }
    else
    {
         
                           
         
      DCHCacheEntry *ent = DCH_cache_fetch(fmt_str);

      incache = true;
      format = ent->format;
    }

#ifdef DEBUG_TO_FROM_CHAR
                                     
                                              
#endif

    DCH_from_char(format, date_str, &tmfc);

    pfree(fmt_str);
    if (!incache)
    {
      pfree(format);
    }
  }

  DEBUG_TMFC(&tmfc);

     
                                                                
     
  if (tmfc.ssss)
  {
    int x = tmfc.ssss;

    tm->tm_hour = x / SECS_PER_HOUR;
    x %= SECS_PER_HOUR;
    tm->tm_min = x / SECS_PER_MINUTE;
    x %= SECS_PER_MINUTE;
    tm->tm_sec = x;
  }

  if (tmfc.ss)
  {
    tm->tm_sec = tmfc.ss;
  }
  if (tmfc.mi)
  {
    tm->tm_min = tmfc.mi;
  }
  if (tmfc.hh)
  {
    tm->tm_hour = tmfc.hh;
  }

  if (tmfc.clock == CLOCK_12_HOUR)
  {
    if (tm->tm_hour < 1 || tm->tm_hour > HOURS_PER_DAY / 2)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("hour \"%d\" is invalid for the 12-hour clock", tm->tm_hour), errhint("Use the 24-hour clock, or give an hour between 1 and 12.")));
    }

    if (tmfc.pm && tm->tm_hour < HOURS_PER_DAY / 2)
    {
      tm->tm_hour += HOURS_PER_DAY / 2;
    }
    else if (!tmfc.pm && tm->tm_hour == HOURS_PER_DAY / 2)
    {
      tm->tm_hour = 0;
    }
  }

  if (tmfc.year)
  {
       
                                                                          
                                                                          
                                                                       
                       
       
    if (tmfc.cc && tmfc.yysz <= 2)
    {
      if (tmfc.bc)
      {
        tmfc.cc = -tmfc.cc;
      }
      tm->tm_year = tmfc.year % 100;
      if (tm->tm_year)
      {
        if (tmfc.cc >= 0)
        {
          tm->tm_year += (tmfc.cc - 1) * 100;
        }
        else
        {
          tm->tm_year = (tmfc.cc + 1) * 100 - tm->tm_year + 1;
        }
      }
      else
      {
                                                        
        tm->tm_year = tmfc.cc * 100 + ((tmfc.cc >= 0) ? 0 : 1);
      }
    }
    else
    {
                                                                     
      tm->tm_year = tmfc.year;
      if (tmfc.bc)
      {
        tm->tm_year = -tm->tm_year;
      }
                                                      
      if (tm->tm_year < 0)
      {
        tm->tm_year++;
      }
    }
    fmask |= DTK_M(YEAR);
  }
  else if (tmfc.cc)
  {
                                   
    if (tmfc.bc)
    {
      tmfc.cc = -tmfc.cc;
    }
    if (tmfc.cc >= 0)
    {
                                                   
      tm->tm_year = (tmfc.cc - 1) * 100 + 1;
    }
    else
    {
                                            
      tm->tm_year = tmfc.cc * 100 + 1;
    }
    fmask |= DTK_M(YEAR);
  }

  if (tmfc.j)
  {
    j2date(tmfc.j, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
    fmask |= DTK_DATE_M;
  }

  if (tmfc.ww)
  {
    if (tmfc.mode == FROM_CHAR_DATE_ISOWEEK)
    {
         
                                                                         
                                
         
      if (tmfc.d)
      {
        isoweekdate2date(tmfc.ww, tmfc.d, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
      }
      else
      {
        isoweek2date(tmfc.ww, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
      }
      fmask |= DTK_DATE_M;
    }
    else
    {
      tmfc.ddd = (tmfc.ww - 1) * 7 + 1;
    }
  }

  if (tmfc.w)
  {
    tmfc.dd = (tmfc.w - 1) * 7 + 1;
  }
  if (tmfc.dd)
  {
    tm->tm_mday = tmfc.dd;
    fmask |= DTK_M(DAY);
  }
  if (tmfc.mm)
  {
    tm->tm_mon = tmfc.mm;
    fmask |= DTK_M(MONTH);
  }

  if (tmfc.ddd && (tm->tm_mon <= 1 || tm->tm_mday <= 1))
  {
       
                                                                
                                                                        
                                                                           
                              
       

    if (!tm->tm_year && !tmfc.bc)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_DATETIME_FORMAT), errmsg("cannot calculate day of year without year information")));
    }

    if (tmfc.mode == FROM_CHAR_DATE_ISOWEEK)
    {
      int j0;                                            

      j0 = isoweek2j(tm->tm_year, 1) - 1;

      j2date(j0 + tmfc.ddd, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
      fmask |= DTK_DATE_M;
    }
    else
    {
      const int *y;
      int i;

      static const int ysum[2][13] = {{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365}, {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}};

      y = ysum[isleap(tm->tm_year)];

      for (i = 1; i <= MONTHS_PER_YEAR; i++)
      {
        if (tmfc.ddd <= y[i])
        {
          break;
        }
      }
      if (tm->tm_mon <= 1)
      {
        tm->tm_mon = i;
      }

      if (tm->tm_mday <= 1)
      {
        tm->tm_mday = tmfc.ddd - y[i - 1];
      }

      fmask |= DTK_M(MONTH) | DTK_M(DAY);
    }
  }

  if (tmfc.ms)
  {
    *fsec += tmfc.ms * 1000;
  }
  if (tmfc.us)
  {
    *fsec += tmfc.us;
  }

                                                                    
  if (fmask != 0)
  {
                                                              
    int dterr = ValidateDate(fmask, true, false, false, tm);

    if (dterr != 0)
    {
         
                                                                         
                                                                         
                                          
         
      DateTimeParseError(DTERR_FIELD_OVERFLOW, date_str, "timestamp");
    }
  }

                                   
  if (tm->tm_hour < 0 || tm->tm_hour >= HOURS_PER_DAY || tm->tm_min < 0 || tm->tm_min >= MINS_PER_HOUR || tm->tm_sec < 0 || tm->tm_sec >= SECS_PER_MINUTE || *fsec < INT64CONST(0) || *fsec >= USECS_PER_SEC)
  {
    DateTimeParseError(DTERR_FIELD_OVERFLOW, date_str, "timestamp");
  }

                                                                  
  if (tmfc.tzsign)
  {
    char *tz;

    if (tmfc.tzh < 0 || tmfc.tzh > MAX_TZDISP_HOUR || tmfc.tzm < 0 || tmfc.tzm >= MINS_PER_HOUR)
    {
      DateTimeParseError(DTERR_TZDISP_OVERFLOW, date_str, "timestamp");
    }

    tz = psprintf("%c%02d:%02d", tmfc.tzsign > 0 ? '+' : '-', tmfc.tzh, tmfc.tzm);

    tm->tm_zone = tz;
  }

  DEBUG_TM(tm);

  pfree(date_str);
}

                                                                        
                           
                                                                       

static char *
fill_str(char *str, int c, int max)
{
  memset(str, c, max);
  *(str + max) = '\0';
  return str;
}

#define zeroize_NUM(_n)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (_n)->flag = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (_n)->lsign = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (_n)->pre = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    (_n)->post = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    (_n)->pre_lsign_num = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    (_n)->need_locale = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    (_n)->multi = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (_n)->zero_start = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    (_n)->zero_end = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  } while (0)

                                                         
static inline void
NUM_prevent_counter_overflow(void)
{
  if (NUMCounter >= (INT_MAX - 1))
  {
    for (int i = 0; i < n_NUMCache; i++)
    {
      NUMCache[i]->age >>= 1;
    }
    NUMCounter >>= 1;
  }
}

                                                             
static NUMCacheEntry *
NUM_cache_getnew(const char *str)
{
  NUMCacheEntry *ent;

                                              
  NUM_prevent_counter_overflow();

     
                                                                            
     
  if (n_NUMCache >= NUM_CACHE_ENTRIES)
  {
    NUMCacheEntry *old = NUMCache[0];

#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "Cache is full (%d)", n_NUMCache);
#endif
    if (old->valid)
    {
      for (int i = 1; i < NUM_CACHE_ENTRIES; i++)
      {
        ent = NUMCache[i];
        if (!ent->valid)
        {
          old = ent;
          break;
        }
        if (ent->age < old->age)
        {
          old = ent;
        }
      }
    }
#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "OLD: \"%s\" AGE: %d", old->str, old->age);
#endif
    old->valid = false;
    StrNCpy(old->str, str, NUM_CACHE_SIZE + 1);
    old->age = (++NUMCounter);
                                                                   
    return old;
  }
  else
  {
#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "NEW (%d)", n_NUMCache);
#endif
    Assert(NUMCache[n_NUMCache] == NULL);
    NUMCache[n_NUMCache] = ent = (NUMCacheEntry *)MemoryContextAllocZero(TopMemoryContext, sizeof(NUMCacheEntry));
    ent->valid = false;
    StrNCpy(ent->str, str, NUM_CACHE_SIZE + 1);
    ent->age = (++NUMCounter);
                                                                   
    ++n_NUMCache;
    return ent;
  }
}

                                                                          
static NUMCacheEntry *
NUM_cache_search(const char *str)
{
                                              
  NUM_prevent_counter_overflow();

  for (int i = 0; i < n_NUMCache; i++)
  {
    NUMCacheEntry *ent = NUMCache[i];

    if (ent->valid && strcmp(ent->str, str) == 0)
    {
      ent->age = (++NUMCounter);
      return ent;
    }
  }

  return NULL;
}

                                                                 
static NUMCacheEntry *
NUM_cache_fetch(const char *str)
{
  NUMCacheEntry *ent;

  if ((ent = NUM_cache_search(str)) == NULL)
  {
       
                                                                          
                                                                   
                 
       
    ent = NUM_cache_getnew(str);

    zeroize_NUM(&ent->Num);

    parse_format(ent->format, str, NUM_keywords, NULL, NUM_index, NUM_TYPE, &ent->Num);

    ent->valid = true;
  }
  return ent;
}

              
                                         
              
   
static FormatNode *
NUM_cache(int len, NUMDesc *Num, text *pars_str, bool *shouldFree)
{
  FormatNode *format = NULL;
  char *str;

  str = text_to_cstring(pars_str);

  if (len > NUM_CACHE_SIZE)
  {
       
                                                                         
                                                 
       
    format = (FormatNode *)palloc((len + 1) * sizeof(FormatNode));

    *shouldFree = true;

    zeroize_NUM(Num);

    parse_format(format, str, NUM_keywords, NULL, NUM_index, NUM_TYPE, Num);
  }
  else
  {
       
                         
       
    NUMCacheEntry *ent = NUM_cache_fetch(str);

    *shouldFree = false;

    format = ent->format;

       
                                 
       
    Num->flag = ent->Num.flag;
    Num->lsign = ent->Num.lsign;
    Num->pre = ent->Num.pre;
    Num->post = ent->Num.post;
    Num->pre_lsign_num = ent->Num.pre_lsign_num;
    Num->need_locale = ent->Num.need_locale;
    Num->multi = ent->Num.multi;
    Num->zero_start = ent->Num.zero_start;
    Num->zero_end = ent->Num.zero_end;
  }

#ifdef DEBUG_TO_FROM_CHAR
                               
  dump_index(NUM_keywords, NUM_index);
#endif

  pfree(str);
  return format;
}

static char *
int_to_roman(int number)
{
  int len = 0, num = 0;
  char *p = NULL, *result, numstr[12];

  result = (char *)palloc(16);
  *result = '\0';

  if (number > 3999 || number < 1)
  {
    fill_str(result, '#', 15);
    return result;
  }
  len = snprintf(numstr, sizeof(numstr), "%d", number);

  for (p = numstr; *p != '\0'; p++, --len)
  {
    num = *p - 49;                   
    if (num < 0)
    {
      continue;
    }

    if (len > 3)
    {
      while (num-- != -1)
      {
        strcat(result, "M");
      }
    }
    else
    {
      if (len == 3)
      {
        strcat(result, rm100[num]);
      }
      else if (len == 2)
      {
        strcat(result, rm10[num]);
      }
      else if (len == 1)
      {
        strcat(result, rm1[num]);
      }
    }
  }
  return result;
}

              
          
              
   
static void
NUM_prepare_locale(NUMProc *Np)
{
  if (Np->Num->need_locale)
  {
    struct lconv *lconv;

       
                   
       
    lconv = PGLC_localeconv();

       
                                       
       
    if (lconv->negative_sign && *lconv->negative_sign)
    {
      Np->L_negative_sign = lconv->negative_sign;
    }
    else
    {
      Np->L_negative_sign = "-";
    }

    if (lconv->positive_sign && *lconv->positive_sign)
    {
      Np->L_positive_sign = lconv->positive_sign;
    }
    else
    {
      Np->L_positive_sign = "+";
    }

       
                            
       
    if (lconv->decimal_point && *lconv->decimal_point)
    {
      Np->decimal = lconv->decimal_point;
    }

    else
    {
      Np->decimal = ".";
    }

    if (!IS_LDECIMAL(Np->Num))
    {
      Np->decimal = ".";
    }

       
                                  
       
                                                                         
                                                                  
                                                                         
       
    if (lconv->thousands_sep && *lconv->thousands_sep)
    {
      Np->L_thousands_sep = lconv->thousands_sep;
    }
                                                                           
    else if (strcmp(Np->decimal, ",") != 0)
    {
      Np->L_thousands_sep = ",";
    }
    else
    {
      Np->L_thousands_sep = ".";
    }

       
                       
       
    if (lconv->currency_symbol && *lconv->currency_symbol)
    {
      Np->L_currency_symbol = lconv->currency_symbol;
    }
    else
    {
      Np->L_currency_symbol = " ";
    }
  }
  else
  {
       
                      
       
    Np->L_negative_sign = "-";
    Np->L_positive_sign = "+";
    Np->decimal = ".";

    Np->L_thousands_sep = ",";
    Np->L_currency_symbol = " ";
  }
}

              
                                                              
                                    
                                    
                                                                        
                                             
              
   
static char *
get_last_relevant_decnum(char *num)
{
  char *result, *p = strchr(num, '.');

#ifdef DEBUG_TO_FROM_CHAR
  elog(DEBUG_elog_output, "get_last_relevant_decnum()");
#endif

  if (!p)
  {
    return NULL;
  }

  result = p;

  while (*(++p))
  {
    if (*p != '0')
    {
      result = p;
    }
  }

  return result;
}

   
                                                                         
                                                            
                                                             
   
#define OVERLOAD_TEST (Np->inout_p >= Np->inout + input_len)
#define AMOUNT_TEST(s) (Np->inout_p <= Np->inout + (input_len - (s)))

              
                                     
              
   
static void
NUM_numpart_from_char(NUMProc *Np, int id, int input_len)
{
  bool isread = false;

#ifdef DEBUG_TO_FROM_CHAR
  elog(DEBUG_elog_output, " --- scan start --- id=%s", (id == NUM_0 || id == NUM_9) ? "NUM_0/9" : id == NUM_DEC ? "NUM_DEC" : "???");
#endif

  if (OVERLOAD_TEST)
  {
    return;
  }

  if (*Np->inout_p == ' ')
  {
    Np->inout_p++;
  }

  if (OVERLOAD_TEST)
  {
    return;
  }

     
                             
     
  if (*Np->number == ' ' && (id == NUM_0 || id == NUM_9) && (Np->read_pre + Np->read_post) == 0)
  {
#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "Try read sign (%c), locale positive: %s, negative: %s", *Np->inout_p, Np->L_positive_sign, Np->L_negative_sign);
#endif

       
                   
       
    if (IS_LSIGN(Np->Num) && Np->Num->lsign == NUM_LSIGN_PRE)
    {
      int x = 0;

#ifdef DEBUG_TO_FROM_CHAR
      elog(DEBUG_elog_output, "Try read locale pre-sign (%c)", *Np->inout_p);
#endif
      if ((x = strlen(Np->L_negative_sign)) && AMOUNT_TEST(x) && strncmp(Np->inout_p, Np->L_negative_sign, x) == 0)
      {
        Np->inout_p += x;
        *Np->number = '-';
      }
      else if ((x = strlen(Np->L_positive_sign)) && AMOUNT_TEST(x) && strncmp(Np->inout_p, Np->L_positive_sign, x) == 0)
      {
        Np->inout_p += x;
        *Np->number = '+';
      }
    }
    else
    {
#ifdef DEBUG_TO_FROM_CHAR
      elog(DEBUG_elog_output, "Try read simple sign (%c)", *Np->inout_p);
#endif

         
                        
         
      if (*Np->inout_p == '-' || (IS_BRACKET(Np->Num) && *Np->inout_p == '<'))
      {
        *Np->number = '-';            
        Np->inout_p++;
      }
      else if (*Np->inout_p == '+')
      {
        *Np->number = '+';            
        Np->inout_p++;
      }
    }
  }

  if (OVERLOAD_TEST)
  {
    return;
  }

#ifdef DEBUG_TO_FROM_CHAR
  elog(DEBUG_elog_output, "Scan for numbers (%c), current number: '%s'", *Np->inout_p, Np->number);
#endif

     
                                 
     
  if (isdigit((unsigned char)*Np->inout_p))
  {
    if (Np->read_dec && Np->read_post == Np->Num->post)
    {
      return;
    }

    *Np->number_p = *Np->inout_p;
    Np->number_p++;

    if (Np->read_dec)
    {
      Np->read_post++;
    }
    else
    {
      Np->read_pre++;
    }

    isread = true;

#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "Read digit (%c)", *Np->inout_p);
#endif
  }
  else if (IS_DECIMAL(Np->Num) && Np->read_dec == false)
  {
       
                                                                      
                                                                         
                                                        
       
    int x = strlen(Np->decimal);

#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "Try read decimal point (%c)", *Np->inout_p);
#endif
    if (x && AMOUNT_TEST(x) && strncmp(Np->inout_p, Np->decimal, x) == 0)
    {
      Np->inout_p += x - 1;
      *Np->number_p = '.';
      Np->number_p++;
      Np->read_dec = true;
      isread = true;
    }
  }

  if (OVERLOAD_TEST)
  {
    return;
  }

     
                                    
     
                                                                             
                
     
                                                                     
           
     
  if (*Np->number == ' ' && Np->read_pre + Np->read_post > 0)
  {
       
                                                                          
                                                                          
                              
       
    if (IS_LSIGN(Np->Num) && isread && (Np->inout_p + 1) < Np->inout + input_len && !isdigit((unsigned char)*(Np->inout_p + 1)))
    {
      int x;
      char *tmp = Np->inout_p++;

#ifdef DEBUG_TO_FROM_CHAR
      elog(DEBUG_elog_output, "Try read locale post-sign (%c)", *Np->inout_p);
#endif
      if ((x = strlen(Np->L_negative_sign)) && AMOUNT_TEST(x) && strncmp(Np->inout_p, Np->L_negative_sign, x) == 0)
      {
        Np->inout_p += x - 1;                                         
        *Np->number = '-';
      }
      else if ((x = strlen(Np->L_positive_sign)) && AMOUNT_TEST(x) && strncmp(Np->inout_p, Np->L_positive_sign, x) == 0)
      {
        Np->inout_p += x - 1;                                         
        *Np->number = '+';
      }
      if (*Np->number == ' ')
      {
                          
        Np->inout_p = tmp;
      }
    }

       
                                                                         
                                                                      
       
                                  
       
                                                                         
                                                                      
               
       
    else if (isread == false && IS_LSIGN(Np->Num) == false && (IS_PLUS(Np->Num) || IS_MINUS(Np->Num)))
    {
#ifdef DEBUG_TO_FROM_CHAR
      elog(DEBUG_elog_output, "Try read simple post-sign (%c)", *Np->inout_p);
#endif

         
                    
         
      if (*Np->inout_p == '-' || *Np->inout_p == '+')
      {
                                          
        *Np->number = *Np->inout_p;
      }
    }
  }
}

#define IS_PREDEC_SPACE(_n) (IS_ZERO((_n)->Num) == false && (_n)->number == (_n)->number_p && *(_n)->number == '0' && (_n)->Num->post != 0)

              
                                      
              
   
static void
NUM_numpart_to_char(NUMProc *Np, int id)
{
  int end;

  if (IS_ROMAN(Np->Num))
  {
    return;
  }

                                                           

#ifdef DEBUG_TO_FROM_CHAR

     
                                                                         
                                
     
  elog(DEBUG_elog_output, "SIGN_WROTE: %d, CURRENT: %d, NUMBER_P: \"%s\", INOUT: \"%s\"", Np->sign_wrote, Np->num_curr, Np->number_p, Np->inout);
#endif
  Np->num_in = false;

     
                                                                            
                            
     
  if (Np->sign_wrote == false && (Np->num_curr >= Np->out_pre_spaces || (IS_ZERO(Np->Num) && Np->Num->zero_start == Np->num_curr)) && (IS_PREDEC_SPACE(Np) == false || (Np->last_relevant && *Np->last_relevant == '.')))
  {
    if (IS_LSIGN(Np->Num))
    {
      if (Np->Num->lsign == NUM_LSIGN_PRE)
      {
        if (Np->sign == '-')
        {
          strcpy(Np->inout_p, Np->L_negative_sign);
        }
        else
        {
          strcpy(Np->inout_p, Np->L_positive_sign);
        }
        Np->inout_p += strlen(Np->inout_p);
        Np->sign_wrote = true;
      }
    }
    else if (IS_BRACKET(Np->Num))
    {
      *Np->inout_p = Np->sign == '+' ? ' ' : '<';
      ++Np->inout_p;
      Np->sign_wrote = true;
    }
    else if (Np->sign == '+')
    {
      if (!IS_FILLMODE(Np->Num))
      {
        *Np->inout_p = ' ';              
        ++Np->inout_p;
      }
      Np->sign_wrote = true;
    }
    else if (Np->sign == '-')
    {              
      *Np->inout_p = '-';
      ++Np->inout_p;
      Np->sign_wrote = true;
    }
  }

     
                                     
     
  if (id == NUM_9 || id == NUM_0 || id == NUM_D || id == NUM_DEC)
  {
    if (Np->num_curr < Np->out_pre_spaces && (Np->Num->zero_start > Np->num_curr || !IS_ZERO(Np->Num)))
    {
         
                           
         
      if (!IS_FILLMODE(Np->Num))
      {
        *Np->inout_p = ' ';                
        ++Np->inout_p;
      }
    }
    else if (IS_ZERO(Np->Num) && Np->num_curr < Np->out_pre_spaces && Np->Num->zero_start <= Np->num_curr)
    {
         
                    
         
      *Np->inout_p = '0';                
      ++Np->inout_p;
      Np->num_in = true;
    }
    else
    {
         
                             
         
      if (*Np->number_p == '.')
      {
        if (!Np->last_relevant || *Np->last_relevant != '.')
        {
          strcpy(Np->inout_p, Np->decimal);                  
          Np->inout_p += strlen(Np->inout_p);
        }

           
                                     
           
        else if (IS_FILLMODE(Np->Num) && Np->last_relevant && *Np->last_relevant == '.')
        {
          strcpy(Np->inout_p, Np->decimal);                  
          Np->inout_p += strlen(Np->inout_p);
        }
      }
      else
      {
           
                        
           
        if (Np->last_relevant && Np->number_p > Np->last_relevant && id != NUM_0)
          ;

           
                                   
           
        else if (IS_PREDEC_SPACE(Np))
        {
          if (!IS_FILLMODE(Np->Num))
          {
            *Np->inout_p = ' ';
            ++Np->inout_p;
          }

             
                                   
             
          else if (Np->last_relevant && *Np->last_relevant == '.')
          {
            *Np->inout_p = '0';
            ++Np->inout_p;
          }
        }
        else
        {
          *Np->inout_p = *Np->number_p;                  
          ++Np->inout_p;
          Np->num_in = true;
        }
      }
                                      
      if (*Np->number_p)
      {
        ++Np->number_p;
      }
    }

    end = Np->num_count + (Np->out_pre_spaces ? 1 : 0) + (IS_DECIMAL(Np->Num) ? 1 : 0);

    if (Np->last_relevant && Np->last_relevant == Np->number_p)
    {
      end = Np->num_curr;
    }

    if (Np->num_curr + 1 == end)
    {
      if (Np->sign_wrote == true && IS_BRACKET(Np->Num))
      {
        *Np->inout_p = Np->sign == '+' ? ' ' : '>';
        ++Np->inout_p;
      }
      else if (IS_LSIGN(Np->Num) && Np->Num->lsign == NUM_LSIGN_POST)
      {
        if (Np->sign == '-')
        {
          strcpy(Np->inout_p, Np->L_negative_sign);
        }
        else
        {
          strcpy(Np->inout_p, Np->L_positive_sign);
        }
        Np->inout_p += strlen(Np->inout_p);
      }
    }
  }

  ++Np->num_curr;
}

   
                                                                        
   
static void
NUM_eat_non_data_chars(NUMProc *Np, int n, int input_len)
{
  while (n-- > 0)
  {
    if (OVERLOAD_TEST)
    {
      break;                   
    }
    if (strchr("0123456789.,+-", *Np->inout_p) != NULL)
    {
      break;                            
    }
    Np->inout_p += pg_mblen(Np->inout_p);
  }
}

static char *
NUM_processor(FormatNode *node, NUMDesc *Num, char *inout, char *number, int input_len, int to_char_out_pre_spaces, int sign, bool is_to_char, Oid collid)
{
  FormatNode *n;
  NUMProc _Np, *Np = &_Np;
  const char *pattern;
  int pattern_len;

  MemSet(Np, 0, sizeof(NUMProc));

  Np->Num = Num;
  Np->is_to_char = is_to_char;
  Np->number = number;
  Np->inout = inout;
  Np->last_relevant = NULL;
  Np->read_post = 0;
  Np->read_pre = 0;
  Np->read_dec = false;

  if (Np->Num->zero_start)
  {
    --Np->Num->zero_start;
  }

  if (IS_EEEE(Np->Num))
  {
    if (!Np->is_to_char)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("\"EEEE\" not supported for input")));
    }
    return strcpy(inout, number);
  }

     
                      
     
  if (IS_ROMAN(Np->Num))
  {
    if (!Np->is_to_char)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("\"RN\" not supported for input")));
    }

    Np->Num->lsign = Np->Num->pre_lsign_num = Np->Num->post = Np->Num->pre = Np->out_pre_spaces = Np->sign = 0;

    if (IS_FILLMODE(Np->Num))
    {
      Np->Num->flag = 0;
      Np->Num->flag |= NUM_F_FILLMODE;
    }
    else
    {
      Np->Num->flag = 0;
    }
    Np->Num->flag |= NUM_F_ROMAN;
  }

     
          
     
  if (is_to_char)
  {
    Np->sign = sign;

                                                        
    if (IS_PLUS(Np->Num) || IS_MINUS(Np->Num))
    {
      if (IS_PLUS(Np->Num) && IS_MINUS(Np->Num) == false)
      {
        Np->sign_wrote = false;                
      }
      else
      {
        Np->sign_wrote = true;                   
      }
    }
    else
    {
      if (Np->sign != '-')
      {
        if (IS_BRACKET(Np->Num) && IS_FILLMODE(Np->Num))
        {
          Np->Num->flag &= ~NUM_F_BRACKET;
        }
        if (IS_MINUS(Np->Num))
        {
          Np->Num->flag &= ~NUM_F_MINUS;
        }
      }
      else if (Np->sign != '+' && IS_PLUS(Np->Num))
      {
        Np->Num->flag &= ~NUM_F_PLUS;
      }

      if (Np->sign == '+' && IS_FILLMODE(Np->Num) && IS_LSIGN(Np->Num) == false)
      {
        Np->sign_wrote = true;                   
      }
      else
      {
        Np->sign_wrote = false;                
      }

      if (Np->Num->lsign == NUM_LSIGN_PRE && Np->Num->pre == Np->Num->pre_lsign_num)
      {
        Np->Num->lsign = NUM_LSIGN_POST;
      }
    }
  }
  else
  {
    Np->sign = false;
  }

     
           
     
  Np->num_count = Np->Num->post + Np->Num->pre - 1;

  if (is_to_char)
  {
    Np->out_pre_spaces = to_char_out_pre_spaces;

    if (IS_FILLMODE(Np->Num) && IS_DECIMAL(Np->Num))
    {
      Np->last_relevant = get_last_relevant_decnum(Np->number);

         
                                                                     
                       
         
      if (Np->last_relevant && Np->Num->zero_end > Np->out_pre_spaces)
      {
        char *last_zero;

        last_zero = Np->number + (Np->Num->zero_end - Np->out_pre_spaces);
        if (Np->last_relevant < last_zero)
        {
          Np->last_relevant = last_zero;
        }
      }
    }

    if (Np->sign_wrote == false && Np->out_pre_spaces == 0)
    {
      ++Np->num_count;
    }
  }
  else
  {
    Np->out_pre_spaces = 0;
    *Np->number = ' ';                 
    *(Np->number + 1) = '\0';
  }

  Np->num_in = 0;
  Np->num_curr = 0;

#ifdef DEBUG_TO_FROM_CHAR
  elog(DEBUG_elog_output, "\n\tSIGN: '%c'\n\tNUM: '%s'\n\tPRE: %d\n\tPOST: %d\n\tNUM_COUNT: %d\n\tNUM_PRE: %d\n\tSIGN_WROTE: %s\n\tZERO: %s\n\tZERO_START: %d\n\tZERO_END: %d\n\tLAST_RELEVANT: %s\n\tBRACKET: %s\n\tPLUS: %s\n\tMINUS: %s\n\tFILLMODE: %s\n\tROMAN: %s\n\tEEEE: %s", Np->sign, Np->number, Np->Num->pre, Np->Num->post, Np->num_count, Np->out_pre_spaces, Np->sign_wrote ? "Yes" : "No", IS_ZERO(Np->Num) ? "Yes" : "No", Np->Num->zero_start, Np->Num->zero_end, Np->last_relevant ? Np->last_relevant : "<not set>", IS_BRACKET(Np->Num) ? "Yes" : "No", IS_PLUS(Np->Num) ? "Yes" : "No", IS_MINUS(Np->Num) ? "Yes" : "No", IS_FILLMODE(Np->Num) ? "Yes" : "No", IS_ROMAN(Np->Num) ? "Yes" : "No", IS_EEEE(Np->Num) ? "Yes" : "No");
#endif

     
            
     
  NUM_prepare_locale(Np);

     
                            
     
  if (Np->is_to_char)
  {
    Np->number_p = Np->number;
  }
  else
  {
    Np->number_p = Np->number + 1;                                   
  }

  for (n = node, Np->inout_p = Np->inout; n->type != NODE_TYPE_END; n++)
  {
    if (!Np->is_to_char)
    {
         
                                                                     
                                                                        
                
         
      if (OVERLOAD_TEST)
      {
        break;
      }
    }

       
                               
       
    if (n->type == NODE_TYPE_ACTION)
    {
         
                                                        
         
                                                                    
                                                              
                                                                   
         
                                                                        
                                                                         
                                                                         
                                                         
         
      switch (n->key->id)
      {
      case NUM_9:
      case NUM_0:
      case NUM_DEC:
      case NUM_D:
        if (Np->is_to_char)
        {
          NUM_numpart_to_char(Np, n->key->id);
          continue;            
        }
        else
        {
          NUM_numpart_from_char(Np, n->key->id, input_len);
          break;                     
        }

      case NUM_COMMA:
        if (Np->is_to_char)
        {
          if (!Np->num_in)
          {
            if (IS_FILLMODE(Np->Num))
            {
              continue;
            }
            else
            {
              *Np->inout_p = ' ';
            }
          }
          else
          {
            *Np->inout_p = ',';
          }
        }
        else
        {
          if (!Np->num_in)
          {
            if (IS_FILLMODE(Np->Num))
            {
              continue;
            }
          }
          if (*Np->inout_p != ',')
          {
            continue;
          }
        }
        break;

      case NUM_G:
        pattern = Np->L_thousands_sep;
        pattern_len = strlen(pattern);
        if (Np->is_to_char)
        {
          if (!Np->num_in)
          {
            if (IS_FILLMODE(Np->Num))
            {
              continue;
            }
            else
            {
                                                   
              pattern_len = pg_mbstrlen(pattern);
              memset(Np->inout_p, ' ', pattern_len);
              Np->inout_p += pattern_len - 1;
            }
          }
          else
          {
            strcpy(Np->inout_p, pattern);
            Np->inout_p += pattern_len - 1;
          }
        }
        else
        {
          if (!Np->num_in)
          {
            if (IS_FILLMODE(Np->Num))
            {
              continue;
            }
          }

             
                                                             
                                                          
                                                                
                                                
             
          if (AMOUNT_TEST(pattern_len) && strncmp(Np->inout_p, pattern, pattern_len) == 0)
          {
            Np->inout_p += pattern_len - 1;
          }
          else
          {
            continue;
          }
        }
        break;

      case NUM_L:
        pattern = Np->L_currency_symbol;
        if (Np->is_to_char)
        {
          strcpy(Np->inout_p, pattern);
          Np->inout_p += strlen(pattern) - 1;
        }
        else
        {
          NUM_eat_non_data_chars(Np, pg_mbstrlen(pattern), input_len);
          continue;
        }
        break;

      case NUM_RN:
        if (IS_FILLMODE(Np->Num))
        {
          strcpy(Np->inout_p, Np->number_p);
          Np->inout_p += strlen(Np->inout_p) - 1;
        }
        else
        {
          sprintf(Np->inout_p, "%15s", Np->number_p);
          Np->inout_p += strlen(Np->inout_p) - 1;
        }
        break;

      case NUM_rn:
        if (IS_FILLMODE(Np->Num))
        {
          strcpy(Np->inout_p, asc_tolower_z(Np->number_p));
          Np->inout_p += strlen(Np->inout_p) - 1;
        }
        else
        {
          sprintf(Np->inout_p, "%15s", asc_tolower_z(Np->number_p));
          Np->inout_p += strlen(Np->inout_p) - 1;
        }
        break;

      case NUM_th:
        if (IS_ROMAN(Np->Num) || *Np->number == '#' || Np->sign == '-' || IS_DECIMAL(Np->Num))
        {
          continue;
        }

        if (Np->is_to_char)
        {
          strcpy(Np->inout_p, get_th(Np->number, TH_LOWER));
          Np->inout_p += 1;
        }
        else
        {
                                                        
          NUM_eat_non_data_chars(Np, 2, input_len);
          continue;
        }
        break;

      case NUM_TH:
        if (IS_ROMAN(Np->Num) || *Np->number == '#' || Np->sign == '-' || IS_DECIMAL(Np->Num))
        {
          continue;
        }

        if (Np->is_to_char)
        {
          strcpy(Np->inout_p, get_th(Np->number, TH_UPPER));
          Np->inout_p += 1;
        }
        else
        {
                                                        
          NUM_eat_non_data_chars(Np, 2, input_len);
          continue;
        }
        break;

      case NUM_MI:
        if (Np->is_to_char)
        {
          if (Np->sign == '-')
          {
            *Np->inout_p = '-';
          }
          else if (IS_FILLMODE(Np->Num))
          {
            continue;
          }
          else
          {
            *Np->inout_p = ' ';
          }
        }
        else
        {
          if (*Np->inout_p == '-')
          {
            *Np->number = '-';
          }
          else
          {
            NUM_eat_non_data_chars(Np, 1, input_len);
            continue;
          }
        }
        break;

      case NUM_PL:
        if (Np->is_to_char)
        {
          if (Np->sign == '+')
          {
            *Np->inout_p = '+';
          }
          else if (IS_FILLMODE(Np->Num))
          {
            continue;
          }
          else
          {
            *Np->inout_p = ' ';
          }
        }
        else
        {
          if (*Np->inout_p == '+')
          {
            *Np->number = '+';
          }
          else
          {
            NUM_eat_non_data_chars(Np, 1, input_len);
            continue;
          }
        }
        break;

      case NUM_SG:
        if (Np->is_to_char)
        {
          *Np->inout_p = Np->sign;
        }
        else
        {
          if (*Np->inout_p == '-')
          {
            *Np->number = '-';
          }
          else if (*Np->inout_p == '+')
          {
            *Np->number = '+';
          }
          else
          {
            NUM_eat_non_data_chars(Np, 1, input_len);
            continue;
          }
        }
        break;

      default:
        continue;
        break;
      }
    }
    else
    {
         
                                                                        
                                                                         
                                                                     
                           
         
      if (Np->is_to_char)
      {
        strcpy(Np->inout_p, n->character);
        Np->inout_p += strlen(Np->inout_p);
      }
      else
      {
        Np->inout_p += pg_mblen(Np->inout_p);
      }
      continue;
    }
    Np->inout_p++;
  }

  if (Np->is_to_char)
  {
    *Np->inout_p = '\0';
    return Np->inout;
  }
  else
  {
    if (*(Np->number_p - 1) == '.')
    {
      *(Np->number_p - 1) = '\0';
    }
    else
    {
      *Np->number_p = '\0';
    }

       
                                             
       
    Np->Num->post = Np->read_post;

#ifdef DEBUG_TO_FROM_CHAR
    elog(DEBUG_elog_output, "TO_NUMBER (number): '%s'", Np->number);
#endif
    return Np->number;
  }
}

              
                                                             
                                                          
              
   
#define NUM_TOCHAR_prepare                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int len = VARSIZE_ANY_EXHDR(fmt);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    if (len <= 0 || len >= (INT_MAX - VARHDRSZ) / NUM_MAX_ITEM_SIZ)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      PG_RETURN_TEXT_P(cstring_to_text(""));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    result = (text *)palloc0((len * NUM_MAX_ITEM_SIZ) + 1 + VARHDRSZ);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    format = NUM_cache(len, &Num, fmt, &shouldFree);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

              
                             
              
   
#define NUM_TOCHAR_finish                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int len;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    NUM_processor(format, &Num, VARDATA(result), numstr, 0, out_pre_spaces, sign, true, PG_GET_COLLATION());                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (shouldFree)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
      pfree(format);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    len = strlen(VARDATA(result));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    SET_VARSIZE(result, len + VARHDRSZ);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)

                       
                                                   
                       
   
Datum
numeric_to_number(PG_FUNCTION_ARGS)
{
  text *value = PG_GETARG_TEXT_PP(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  Datum result;
  FormatNode *format;
  char *numstr;
  bool shouldFree;
  int len = 0;
  int scale, precision;

  len = VARSIZE_ANY_EXHDR(fmt);

  if (len <= 0 || len >= INT_MAX / NUM_MAX_ITEM_SIZ)
  {
    PG_RETURN_NULL();
  }

  format = NUM_cache(len, &Num, fmt, &shouldFree);

  numstr = (char *)palloc((len * NUM_MAX_ITEM_SIZ) + 1);

  NUM_processor(format, &Num, VARDATA_ANY(value), numstr, VARSIZE_ANY_EXHDR(value), 0, 0, false, PG_GET_COLLATION());

  scale = Num.post;
  precision = Num.pre + Num.multi + scale;

  if (shouldFree)
  {
    pfree(format);
  }

  result = DirectFunctionCall3(numeric_in, CStringGetDatum(numstr), ObjectIdGetDatum(InvalidOid), Int32GetDatum(((precision << 16) | scale) + VARHDRSZ));

  if (IS_MULTI(&Num))
  {
    Numeric x;
    Numeric a = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(10)));
    Numeric b = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(-Num.multi)));

    x = DatumGetNumeric(DirectFunctionCall2(numeric_power, NumericGetDatum(a), NumericGetDatum(b)));
    result = DirectFunctionCall2(numeric_mul, result, NumericGetDatum(x));
  }

  pfree(numstr);
  return result;
}

                      
                     
                      
   
Datum
numeric_to_char(PG_FUNCTION_ARGS)
{
  Numeric value = PG_GETARG_NUMERIC(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  FormatNode *format;
  text *result;
  bool shouldFree;
  int out_pre_spaces = 0, sign = 0;
  char *numstr, *orgnum, *p;
  Numeric x;

  NUM_TOCHAR_prepare;

     
                                       
     
  if (IS_ROMAN(&Num))
  {
    x = DatumGetNumeric(DirectFunctionCall2(numeric_round, NumericGetDatum(value), Int32GetDatum(0)));
    numstr = orgnum = int_to_roman(DatumGetInt32(DirectFunctionCall1(numeric_int4, NumericGetDatum(x))));
  }
  else if (IS_EEEE(&Num))
  {
    orgnum = numeric_out_sci(value, Num.post);

       
                                                                        
                                                                      
                                                                         
       
    if (strcmp(orgnum, "NaN") == 0)
    {
         
                                                                     
                                                           
         
      numstr = (char *)palloc(Num.pre + Num.post + 7);
      fill_str(numstr, '#', Num.pre + Num.post + 6);
      *numstr = ' ';
      *(numstr + Num.pre + 1) = '.';
    }
    else if (*orgnum != '-')
    {
      numstr = (char *)palloc(strlen(orgnum) + 2);
      *numstr = ' ';
      strcpy(numstr + 1, orgnum);
    }
    else
    {
      numstr = orgnum;
    }
  }
  else
  {
    int numstr_pre_len;
    Numeric val = value;

    if (IS_MULTI(&Num))
    {
      Numeric a = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(10)));
      Numeric b = DatumGetNumeric(DirectFunctionCall1(int4_numeric, Int32GetDatum(Num.multi)));

      x = DatumGetNumeric(DirectFunctionCall2(numeric_power, NumericGetDatum(a), NumericGetDatum(b)));
      val = DatumGetNumeric(DirectFunctionCall2(numeric_mul, NumericGetDatum(value), NumericGetDatum(x)));
      Num.pre += Num.multi;
    }

    x = DatumGetNumeric(DirectFunctionCall2(numeric_round, NumericGetDatum(val), Int32GetDatum(Num.post)));
    orgnum = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(x)));

    if (*orgnum == '-')
    {
      sign = '-';
      numstr = orgnum + 1;
    }
    else
    {
      sign = '+';
      numstr = orgnum;
    }

    if ((p = strchr(numstr, '.')))
    {
      numstr_pre_len = p - numstr;
    }
    else
    {
      numstr_pre_len = strlen(numstr);
    }

                        
    if (numstr_pre_len < Num.pre)
    {
      out_pre_spaces = Num.pre - numstr_pre_len;
    }
                                         
    else if (numstr_pre_len > Num.pre)
    {
      numstr = (char *)palloc(Num.pre + Num.post + 2);
      fill_str(numstr, '#', Num.pre + Num.post + 1);
      *(numstr + Num.pre) = '.';
    }
  }

  NUM_TOCHAR_finish;
  PG_RETURN_TEXT_P(result);
}

                   
                  
                   
   
Datum
int4_to_char(PG_FUNCTION_ARGS)
{
  int32 value = PG_GETARG_INT32(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  FormatNode *format;
  text *result;
  bool shouldFree;
  int out_pre_spaces = 0, sign = 0;
  char *numstr, *orgnum;

  NUM_TOCHAR_prepare;

     
                                     
     
  if (IS_ROMAN(&Num))
  {
    numstr = orgnum = int_to_roman(value);
  }
  else if (IS_EEEE(&Num))
  {
                                                                     
    float8 val = (float8)value;

    orgnum = (char *)psprintf("%+.*e", Num.post, val);

       
                                                 
       
    if (*orgnum == '+')
    {
      *orgnum = ' ';
    }

    numstr = orgnum;
  }
  else
  {
    int numstr_pre_len;

    if (IS_MULTI(&Num))
    {
      orgnum = DatumGetCString(DirectFunctionCall1(int4out, Int32GetDatum(value * ((int32)pow((double)10, (double)Num.multi)))));
      Num.pre += Num.multi;
    }
    else
    {
      orgnum = DatumGetCString(DirectFunctionCall1(int4out, Int32GetDatum(value)));
    }

    if (*orgnum == '-')
    {
      sign = '-';
      orgnum++;
    }
    else
    {
      sign = '+';
    }

    numstr_pre_len = strlen(orgnum);

                                                   
    if (Num.post)
    {
      numstr = (char *)palloc(numstr_pre_len + Num.post + 2);
      strcpy(numstr, orgnum);
      *(numstr + numstr_pre_len) = '.';
      memset(numstr + numstr_pre_len + 1, '0', Num.post);
      *(numstr + numstr_pre_len + Num.post + 1) = '\0';
    }
    else
    {
      numstr = orgnum;
    }

                        
    if (numstr_pre_len < Num.pre)
    {
      out_pre_spaces = Num.pre - numstr_pre_len;
    }
                                         
    else if (numstr_pre_len > Num.pre)
    {
      numstr = (char *)palloc(Num.pre + Num.post + 2);
      fill_str(numstr, '#', Num.pre + Num.post + 1);
      *(numstr + Num.pre) = '.';
    }
  }

  NUM_TOCHAR_finish;
  PG_RETURN_TEXT_P(result);
}

                   
                  
                   
   
Datum
int8_to_char(PG_FUNCTION_ARGS)
{
  int64 value = PG_GETARG_INT64(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  FormatNode *format;
  text *result;
  bool shouldFree;
  int out_pre_spaces = 0, sign = 0;
  char *numstr, *orgnum;

  NUM_TOCHAR_prepare;

     
                                     
     
  if (IS_ROMAN(&Num))
  {
                                                             
    numstr = orgnum = int_to_roman(DatumGetInt32(DirectFunctionCall1(int84, Int64GetDatum(value))));
  }
  else if (IS_EEEE(&Num))
  {
                                                                    
    Numeric val;

    val = DatumGetNumeric(DirectFunctionCall1(int8_numeric, Int64GetDatum(value)));
    orgnum = numeric_out_sci(val, Num.post);

       
                                                                        
                                                                      
                                                                    
       
    if (*orgnum != '-')
    {
      numstr = (char *)palloc(strlen(orgnum) + 2);
      *numstr = ' ';
      strcpy(numstr + 1, orgnum);
    }
    else
    {
      numstr = orgnum;
    }
  }
  else
  {
    int numstr_pre_len;

    if (IS_MULTI(&Num))
    {
      double multi = pow((double)10, (double)Num.multi);

      value = DatumGetInt64(DirectFunctionCall2(int8mul, Int64GetDatum(value), DirectFunctionCall1(dtoi8, Float8GetDatum(multi))));
      Num.pre += Num.multi;
    }

    orgnum = DatumGetCString(DirectFunctionCall1(int8out, Int64GetDatum(value)));

    if (*orgnum == '-')
    {
      sign = '-';
      orgnum++;
    }
    else
    {
      sign = '+';
    }

    numstr_pre_len = strlen(orgnum);

                                                   
    if (Num.post)
    {
      numstr = (char *)palloc(numstr_pre_len + Num.post + 2);
      strcpy(numstr, orgnum);
      *(numstr + numstr_pre_len) = '.';
      memset(numstr + numstr_pre_len + 1, '0', Num.post);
      *(numstr + numstr_pre_len + Num.post + 1) = '\0';
    }
    else
    {
      numstr = orgnum;
    }

                        
    if (numstr_pre_len < Num.pre)
    {
      out_pre_spaces = Num.pre - numstr_pre_len;
    }
                                         
    else if (numstr_pre_len > Num.pre)
    {
      numstr = (char *)palloc(Num.pre + Num.post + 2);
      fill_str(numstr, '#', Num.pre + Num.post + 1);
      *(numstr + Num.pre) = '.';
    }
  }

  NUM_TOCHAR_finish;
  PG_RETURN_TEXT_P(result);
}

                     
                    
                     
   
Datum
float4_to_char(PG_FUNCTION_ARGS)
{
  float4 value = PG_GETARG_FLOAT4(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  FormatNode *format;
  text *result;
  bool shouldFree;
  int out_pre_spaces = 0, sign = 0;
  char *numstr, *orgnum, *p;

  NUM_TOCHAR_prepare;

  if (IS_ROMAN(&Num))
  {
    numstr = orgnum = int_to_roman((int)rint(value));
  }
  else if (IS_EEEE(&Num))
  {
    if (isnan(value) || isinf(value))
    {
         
                                                                     
                                                           
         
      numstr = (char *)palloc(Num.pre + Num.post + 7);
      fill_str(numstr, '#', Num.pre + Num.post + 6);
      *numstr = ' ';
      *(numstr + Num.pre + 1) = '.';
    }
    else
    {
      numstr = orgnum = psprintf("%+.*e", Num.post, value);

         
                                                   
         
      if (*orgnum == '+')
      {
        *orgnum = ' ';
      }

      numstr = orgnum;
    }
  }
  else
  {
    float4 val = value;
    int numstr_pre_len;

    if (IS_MULTI(&Num))
    {
      float multi = pow((double)10, (double)Num.multi);

      val = value * multi;
      Num.pre += Num.multi;
    }

    orgnum = (char *)psprintf("%.0f", fabs(val));
    numstr_pre_len = strlen(orgnum);

                                                    
    if (numstr_pre_len >= FLT_DIG)
    {
      Num.post = 0;
    }
    else if (numstr_pre_len + Num.post > FLT_DIG)
    {
      Num.post = FLT_DIG - numstr_pre_len;
    }
    orgnum = psprintf("%.*f", Num.post, val);

    if (*orgnum == '-')
    {          
      sign = '-';
      numstr = orgnum + 1;
    }
    else
    {
      sign = '+';
      numstr = orgnum;
    }

    if ((p = strchr(numstr, '.')))
    {
      numstr_pre_len = p - numstr;
    }
    else
    {
      numstr_pre_len = strlen(numstr);
    }

                        
    if (numstr_pre_len < Num.pre)
    {
      out_pre_spaces = Num.pre - numstr_pre_len;
    }
                                         
    else if (numstr_pre_len > Num.pre)
    {
      numstr = (char *)palloc(Num.pre + Num.post + 2);
      fill_str(numstr, '#', Num.pre + Num.post + 1);
      *(numstr + Num.pre) = '.';
    }
  }

  NUM_TOCHAR_finish;
  PG_RETURN_TEXT_P(result);
}

                     
                    
                     
   
Datum
float8_to_char(PG_FUNCTION_ARGS)
{
  float8 value = PG_GETARG_FLOAT8(0);
  text *fmt = PG_GETARG_TEXT_PP(1);
  NUMDesc Num;
  FormatNode *format;
  text *result;
  bool shouldFree;
  int out_pre_spaces = 0, sign = 0;
  char *numstr, *orgnum, *p;

  NUM_TOCHAR_prepare;

  if (IS_ROMAN(&Num))
  {
    numstr = orgnum = int_to_roman((int)rint(value));
  }
  else if (IS_EEEE(&Num))
  {
    if (isnan(value) || isinf(value))
    {
         
                                                                     
                                                           
         
      numstr = (char *)palloc(Num.pre + Num.post + 7);
      fill_str(numstr, '#', Num.pre + Num.post + 6);
      *numstr = ' ';
      *(numstr + Num.pre + 1) = '.';
    }
    else
    {
      numstr = orgnum = (char *)psprintf("%+.*e", Num.post, value);

         
                                                   
         
      if (*orgnum == '+')
      {
        *orgnum = ' ';
      }

      numstr = orgnum;
    }
  }
  else
  {
    float8 val = value;
    int numstr_pre_len;

    if (IS_MULTI(&Num))
    {
      double multi = pow((double)10, (double)Num.multi);

      val = value * multi;
      Num.pre += Num.multi;
    }
    orgnum = psprintf("%.0f", fabs(val));
    numstr_pre_len = strlen(orgnum);

                                                     
    if (numstr_pre_len >= DBL_DIG)
    {
      Num.post = 0;
    }
    else if (numstr_pre_len + Num.post > DBL_DIG)
    {
      Num.post = DBL_DIG - numstr_pre_len;
    }
    orgnum = psprintf("%.*f", Num.post, val);

    if (*orgnum == '-')
    {          
      sign = '-';
      numstr = orgnum + 1;
    }
    else
    {
      sign = '+';
      numstr = orgnum;
    }

    if ((p = strchr(numstr, '.')))
    {
      numstr_pre_len = p - numstr;
    }
    else
    {
      numstr_pre_len = strlen(numstr);
    }

                        
    if (numstr_pre_len < Num.pre)
    {
      out_pre_spaces = Num.pre - numstr_pre_len;
    }
                                         
    else if (numstr_pre_len > Num.pre)
    {
      numstr = (char *)palloc(Num.pre + Num.post + 2);
      fill_str(numstr, '#', Num.pre + Num.post + 1);
      *(numstr + Num.pre) = '.';
    }
  }

  NUM_TOCHAR_finish;
  PG_RETURN_TEXT_P(result);
}
