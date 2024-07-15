                                                                            
   
                  
                                                            
   
                                                                         
   
                  
                                   
   
                                                                            
   
#include "postgres_fe.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pgtz.h"

                                                                               
extern const char *
select_default_timezone(const char *share_path);

#ifndef SYSTEMTZDIR
static char tzdirpath[MAXPGPATH];
#endif

   
                                                   
   
                                                                               
   
static const char *
pg_TZDIR(void)
{
#ifndef SYSTEMTZDIR
                                                          
  return tzdirpath;
#else
                                                          
  return SYSTEMTZDIR;
#endif
}

   
                                                                     
                                             
   
                                                                      
                                                                          
                                                                            
                                                                             
                                                                         
                                                             
   
                                                                             
                                                                           
                                                                       
   
int
pg_open_tzfile(const char *name, char *canonname)
{
  char fullname[MAXPGPATH];

  if (canonname)
  {
    strlcpy(canonname, name, TZ_STRLEN_MAX + 1);
  }

  strlcpy(fullname, pg_TZDIR(), sizeof(fullname));
  if (strlen(fullname) + 1 + strlen(name) >= MAXPGPATH)
  {
    return -1;                    
  }
  strcat(fullname, "/");
  strcat(fullname, name);

  return open(fullname, O_RDONLY | PG_BINARY, 0);
}

   
                               
                                                    
   
                                                                             
                                  
   
static pg_tz *
pg_load_tz(const char *name)
{
  static pg_tz tz;

  if (strlen(name) > TZ_STRLEN_MAX)
  {
    return NULL;                       
  }

     
                                                                     
     
  if (strcmp(name, "GMT") == 0)
  {
    if (!tzparse(name, &tz.state, true))
    {
                                                     
      return NULL;
    }
  }
  else if (tzload(name, NULL, &tz.state, true) != 0)
  {
    if (name[0] == ':' || !tzparse(name, &tz.state, false))
    {
      return NULL;                       
    }
  }

  strcpy(tz.TZname, name);

  return &tz;
}

   
                                                                           
                                                                       
   
                                                                          
                                                                         
                                                                          
                                                                           
                                                                          
                                                                           
                                                                        
                                                                      
              
   
                                                                         
                                                                             
                                                                             
                                                                              
                                                                            
                                          
   
                                                                         
                                                                            
                                                                     
                                                                  
   

#ifndef WIN32

#define T_DAY ((time_t)(60 * 60 * 24))
#define T_WEEK ((time_t)(60 * 60 * 24 * 7))
#define T_MONTH ((time_t)(60 * 60 * 24 * 31))

#define MAX_TEST_TIMES (52 * 100)                

struct tztry
{
  int n_test_times;
  time_t test_times[MAX_TEST_TIMES];
};

static bool
check_system_link_file(const char *linkname, struct tztry *tt, char *bestzonename);
static void
scan_available_timezones(char *tzdir, char *tzdirsub, struct tztry *tt, int *bestscore, char *bestzonename);

   
                                          
   
static int
get_timezone_offset(struct tm *tm)
{
#if defined(HAVE_STRUCT_TM_TM_ZONE)
  return tm->tm_gmtoff;
#elif defined(HAVE_INT_TIMEZONE)
  return -TIMEZONE_GLOBAL;
#else
#error No way to determine TZ? Can this happen?
#endif
}

   
                                                                     
   
static time_t
build_time_t(int year, int month, int day)
{
  struct tm tm;

  memset(&tm, 0, sizeof(tm));
  tm.tm_mday = day;
  tm.tm_mon = month - 1;
  tm.tm_year = year - 1900;
  tm.tm_isdst = -1;

  return mktime(&tm);
}

   
                                                           
   
static bool
compare_tm(struct tm *s, struct pg_tm *p)
{
  if (s->tm_sec != p->tm_sec || s->tm_min != p->tm_min || s->tm_hour != p->tm_hour || s->tm_mday != p->tm_mday || s->tm_mon != p->tm_mon || s->tm_year != p->tm_year || s->tm_wday != p->tm_wday || s->tm_yday != p->tm_yday || s->tm_isdst != p->tm_isdst)
  {
    return false;
  }
  return true;
}

   
                                                                        
   
                                                                        
                                                                            
                                                                         
   
                                                                          
                                                                         
              
   
static int
score_timezone(const char *tzname, struct tztry *tt)
{
  int i;
  pg_time_t pgtt;
  struct tm *systm;
  struct pg_tm *pgtm;
  char cbuf[TZ_STRLEN_MAX + 1];
  pg_tz *tz;

                                
  tz = pg_load_tz(tzname);
  if (!tz)
  {
    return -1;                             
  }

                                       
  if (!pg_tz_acceptable(tz))
  {
#ifdef DEBUG_IDENTIFY_TIMEZONE
    fprintf(stderr, "Reject TZ \"%s\": uses leap seconds\n", tzname);
#endif
    return -1;
  }

                                             
  for (i = 0; i < tt->n_test_times; i++)
  {
    pgtt = (pg_time_t)(tt->test_times[i]);
    pgtm = pg_localtime(&pgtt, tz);
    if (!pgtm)
    {
      return -1;                                
    }
    systm = localtime(&(tt->test_times[i]));
    if (!systm)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "TZ \"%s\" scores %d: at %ld %04d-%02d-%02d %02d:%02d:%02d %s, system had no data\n", tzname, i, (long)pgtt, pgtm->tm_year + 1900, pgtm->tm_mon + 1, pgtm->tm_mday, pgtm->tm_hour, pgtm->tm_min, pgtm->tm_sec, pgtm->tm_isdst ? "dst" : "std");
#endif
      return i;
    }
    if (!compare_tm(systm, pgtm))
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "TZ \"%s\" scores %d: at %ld %04d-%02d-%02d %02d:%02d:%02d %s versus %04d-%02d-%02d %02d:%02d:%02d %s\n", tzname, i, (long)pgtt, pgtm->tm_year + 1900, pgtm->tm_mon + 1, pgtm->tm_mday, pgtm->tm_hour, pgtm->tm_min, pgtm->tm_sec, pgtm->tm_isdst ? "dst" : "std", systm->tm_year + 1900, systm->tm_mon + 1, systm->tm_mday, systm->tm_hour, systm->tm_min, systm->tm_sec, systm->tm_isdst ? "dst" : "std");
#endif
      return i;
    }
    if (systm->tm_isdst >= 0)
    {
                                          
      if (pgtm->tm_zone == NULL)
      {
        return -1;                                
      }
      memset(cbuf, 0, sizeof(cbuf));
      strftime(cbuf, sizeof(cbuf) - 1, "%Z", systm);                
      if (strcmp(cbuf, pgtm->tm_zone) != 0)
      {
#ifdef DEBUG_IDENTIFY_TIMEZONE
        fprintf(stderr, "TZ \"%s\" scores %d: at %ld \"%s\" versus \"%s\"\n", tzname, i, (long)pgtt, pgtm->tm_zone, cbuf);
#endif
        return i;
      }
    }
  }

#ifdef DEBUG_IDENTIFY_TIMEZONE
  fprintf(stderr, "TZ \"%s\" gets max score %d\n", tzname, i);
#endif

  return i;
}

   
                                                                           
   
static bool
perfect_timezone_match(const char *tzname, struct tztry *tt)
{
  return (score_timezone(tzname, tt) == tt->n_test_times);
}

   
                                                                              
                                                         
   
static const char *
identify_system_timezone(void)
{
  static char resultbuf[TZ_STRLEN_MAX + 1];
  time_t tnow;
  time_t t;
  struct tztry tt;
  struct tm *tm;
  int thisyear;
  int bestscore;
  char tmptzdir[MAXPGPATH];
  int std_ofs;
  char std_zone_name[TZ_STRLEN_MAX + 1], dst_zone_name[TZ_STRLEN_MAX + 1];
  char cbuf[TZ_STRLEN_MAX + 1];

                                      
  tzset();

     
                                                                        
                                                                      
                                                                             
                                                                             
                                                                        
                                                                          
                                                                           
                                                                          
                                                                           
                                                                          
                                                                        
                                                                            
                                                                           
                                                                           
                                                                            
                                                                       
              
     
  tnow = time(NULL);
  tm = localtime(&tnow);
  if (!tm)
  {
    return NULL;                                        
  }
  thisyear = tm->tm_year + 1900;

  t = build_time_t(thisyear, 1, 15);

     
                                                                         
                                                                          
                                                                        
                                                         
     
  t -= (t % T_WEEK);

  tt.n_test_times = 0;
  tt.test_times[tt.n_test_times++] = t;

  t = build_time_t(thisyear, 7, 15);
  t -= (t % T_WEEK);

  tt.test_times[tt.n_test_times++] = t;

  while (tt.n_test_times < MAX_TEST_TIMES)
  {
    t -= T_WEEK;
    tt.test_times[tt.n_test_times++] = t;
  }

     
                                                                           
                                         
     
                                                                             
                                                                            
                           
     
  if (check_system_link_file("/etc/localtime", &tt, resultbuf))
  {
    return resultbuf;
  }

                                                              
  strlcpy(tmptzdir, pg_TZDIR(), sizeof(tmptzdir));
  bestscore = -1;
  resultbuf[0] = '\0';
  scan_available_timezones(tmptzdir, tmptzdir + strlen(tmptzdir) + 1, &tt, &bestscore, resultbuf);
  if (bestscore > 0)
  {
                                                                    
    if (strcmp(resultbuf, "Factory") == 0)
    {
      return NULL;
    }
    return resultbuf;
  }

     
                                                                            
                             
     
                                                                             
                                                                            
                                     
     
  memset(std_zone_name, 0, sizeof(std_zone_name));
  memset(dst_zone_name, 0, sizeof(dst_zone_name));
  std_ofs = 0;

  tnow = time(NULL);

     
                                                                           
         
     
  tnow -= (tnow % T_DAY);

     
                                                                            
                                                                           
                                                          
     
  for (t = tnow; t <= tnow + T_MONTH * 14; t += T_MONTH)
  {
    tm = localtime(&t);
    if (!tm)
    {
      continue;
    }
    if (tm->tm_isdst < 0)
    {
      continue;
    }
    if (tm->tm_isdst == 0 && std_zone_name[0] == '\0')
    {
                          
      memset(cbuf, 0, sizeof(cbuf));
      strftime(cbuf, sizeof(cbuf) - 1, "%Z", tm);                
      strcpy(std_zone_name, cbuf);
      std_ofs = get_timezone_offset(tm);
    }
    if (tm->tm_isdst > 0 && dst_zone_name[0] == '\0')
    {
                          
      memset(cbuf, 0, sizeof(cbuf));
      strftime(cbuf, sizeof(cbuf) - 1, "%Z", tm);                
      strcpy(dst_zone_name, cbuf);
    }
                            
    if (std_zone_name[0] && dst_zone_name[0])
    {
      break;
    }
  }

                                                      
  if (std_zone_name[0] == '\0')
  {
#ifdef DEBUG_IDENTIFY_TIMEZONE
    fprintf(stderr, "could not determine system time zone\n");
#endif
    return NULL;                
  }

                                            
  if (dst_zone_name[0] != '\0')
  {
    snprintf(resultbuf, sizeof(resultbuf), "%s%d%s", std_zone_name, -std_ofs / 3600, dst_zone_name);
    if (score_timezone(resultbuf, &tt) > 0)
    {
      return resultbuf;
    }
  }

                                                          
  strcpy(resultbuf, std_zone_name);
  if (score_timezone(resultbuf, &tt) > 0)
  {
    return resultbuf;
  }

                    
  snprintf(resultbuf, sizeof(resultbuf), "%s%d", std_zone_name, -std_ofs / 3600);
  if (score_timezone(resultbuf, &tt) > 0)
  {
    return resultbuf;
  }

     
                                                                            
                                                                            
                                                                          
                                                                         
     
  snprintf(resultbuf, sizeof(resultbuf), "Etc/GMT%s%d", (-std_ofs > 0) ? "+" : "", -std_ofs / 3600);

#ifdef DEBUG_IDENTIFY_TIMEZONE
  fprintf(stderr, "could not recognize system time zone, using \"%s\"\n", resultbuf);
#endif
  return resultbuf;
}

   
                                                                              
   
                                                                            
                                                                      
                                                                       
                                                                             
                                                                              
                                                                       
   
                                                                          
                                                                             
                                                         
   
                                                   
   
                                                                 
   
                                                                          
                                                                            
                      
   
static bool
check_system_link_file(const char *linkname, struct tztry *tt, char *bestzonename)
{
#ifdef HAVE_READLINK
  char link_target[MAXPGPATH];
  int len;
  const char *cur_name;

     
                                                                          
                                                          
     
  len = readlink(linkname, link_target, sizeof(link_target));
  if (len < 0 || len >= sizeof(link_target))
  {
    return false;
  }
  link_target[len] = '\0';

#ifdef DEBUG_IDENTIFY_TIMEZONE
  fprintf(stderr, "symbolic link \"%s\" contains \"%s\"\n", linkname, link_target);
#endif

     
                                                                        
                                                                            
                                                                            
                                                                             
                    
     
  cur_name = link_target;
  while (*cur_name)
  {
                                         
    cur_name = strchr(cur_name + 1, '/');
    if (cur_name == NULL)
    {
      break;
    }
                                                                         
    do
    {
      cur_name++;
    } while (*cur_name == '/');

       
                                                                    
                                                                           
                                                                       
       
    if (*cur_name && *cur_name != '.' && strlen(cur_name) <= TZ_STRLEN_MAX && perfect_timezone_match(cur_name, tt))
    {
                    
      strcpy(bestzonename, cur_name);
      return true;
    }
  }

                                             
  return false;
#else
                               
  return false;
#endif
}

   
                                                                              
                                                                             
                                                                             
   
static int
zone_name_pref(const char *zonename)
{
     
                                                                         
                                               
     
  if (strcmp(zonename, "UTC") == 0)
  {
    return 50;
  }
  if (strcmp(zonename, "Etc/UTC") == 0)
  {
    return 40;
  }

     
                                                                           
                                                                           
     
  if (strcmp(zonename, "localtime") == 0 || strcmp(zonename, "posixrules") == 0)
  {
    return -50;
  }

  return 0;
}

   
                                                                        
                                 
   
                                                                       
                                                                         
                                                                       
                                         
   
                                                                         
                                                                     
                     
   
                                                                 
   
                                                                          
                                                                         
                                                                      
   
static void
scan_available_timezones(char *tzdir, char *tzdirsub, struct tztry *tt, int *bestscore, char *bestzonename)
{
  int tzdir_orig_len = strlen(tzdir);
  char **names;
  char **namep;

  names = pgfnames(tzdir);
  if (!names)
  {
    return;
  }

  for (namep = names; *namep; namep++)
  {
    char *name = *namep;
    struct stat statbuf;

                                                        
    if (name[0] == '.')
    {
      continue;
    }

    snprintf(tzdir + tzdir_orig_len, MAXPGPATH - tzdir_orig_len, "/%s", name);

    if (stat(tzdir, &statbuf) != 0)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "could not stat \"%s\": %s\n", tzdir, strerror(errno));
#endif
      tzdir[tzdir_orig_len] = '\0';
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
    {
                                     
      scan_available_timezones(tzdir, tzdirsub, tt, bestscore, bestzonename);
    }
    else
    {
                                   
      int score = score_timezone(tzdirsub, tt);

      if (score > *bestscore)
      {
        *bestscore = score;
        strlcpy(bestzonename, tzdirsub, TZ_STRLEN_MAX + 1);
      }
      else if (score == *bestscore)
      {
                                         
        int namepref = (zone_name_pref(tzdirsub) - zone_name_pref(bestzonename));

        if (namepref > 0 || (namepref == 0 && (strlen(tzdirsub) < strlen(bestzonename) || (strlen(tzdirsub) == strlen(bestzonename) && strcmp(tzdirsub, bestzonename) < 0))))
        {
          strlcpy(bestzonename, tzdirsub, TZ_STRLEN_MAX + 1);
        }
      }
    }

                       
    tzdir[tzdir_orig_len] = '\0';
  }

  pgfnames_cleanup(names);
}
#else            

static const struct
{
  const char *stdname;                                         
  const char *dstname;                                         
  const char *pgtzname;                                       
} win32_tzmap[] =

    {
           
                                                                    
                                                                                
                                                                                 
                                                                       
                                  
           
                                                                                 
                                
           
        {                       
            "Afghanistan Standard Time", "Afghanistan Daylight Time", "Asia/Kabul"},
        {                        
            "Alaskan Standard Time", "Alaskan Daylight Time", "America/Anchorage"},
        {                                  
            "Aleutian Standard Time", "Aleutian Daylight Time", "America/Adak"},
        {                                        
            "Altai Standard Time", "Altai Daylight Time", "Asia/Barnaul"},
        {                                
            "Arab Standard Time", "Arab Daylight Time", "Asia/Riyadh"},
        {                                   
            "Arabian Standard Time", "Arabian Daylight Time", "Asia/Dubai"},
        {                         
            "Arabic Standard Time", "Arabic Daylight Time", "Asia/Baghdad"},
        {                                      
            "Argentina Standard Time", "Argentina Daylight Time", "America/Buenos_Aires"},
        {                                        
            "Armenian Standard Time", "Armenian Daylight Time", "Asia/Yerevan"},
        {                                      
            "Astrakhan Standard Time", "Astrakhan Daylight Time", "Europe/Astrakhan"},
        {                                        
            "Atlantic Standard Time", "Atlantic Daylight Time", "America/Halifax"},
        {                        
            "AUS Central Standard Time", "AUS Central Daylight Time", "Australia/Darwin"},
        {                       
            "Aus Central W. Standard Time", "Aus Central W. Daylight Time", "Australia/Eucla"},
        {                                             
            "AUS Eastern Standard Time", "AUS Eastern Daylight Time", "Australia/Sydney"},
        {                      
            "Azerbaijan Standard Time", "Azerbaijan Daylight Time", "Asia/Baku"},
        {                        
            "Azores Standard Time", "Azores Daylight Time", "Atlantic/Azores"},
        {                          
            "Bahia Standard Time", "Bahia Daylight Time", "America/Bahia"},
        {                       
            "Bangladesh Standard Time", "Bangladesh Daylight Time", "Asia/Dhaka"},
        {                       
            "Belarus Standard Time", "Belarus Daylight Time", "Europe/Minsk"},
        {                                     
            "Bougainville Standard Time", "Bougainville Daylight Time", "Pacific/Bougainville"},
        {                                
            "Cabo Verde Standard Time", "Cabo Verde Daylight Time", "Atlantic/Cape_Verde"},
        {                              
            "Canada Central Standard Time", "Canada Central Daylight Time", "America/Regina"},
        {                                
            "Cape Verde Standard Time", "Cape Verde Daylight Time", "Atlantic/Cape_Verde"},
        {                         
            "Caucasus Standard Time", "Caucasus Daylight Time", "Asia/Yerevan"},
        {                          
            "Cen. Australia Standard Time", "Cen. Australia Daylight Time", "Australia/Adelaide"},
        {                                 
            "Central America Standard Time", "Central America Daylight Time", "America/Guatemala"},
        {                        
            "Central Asia Standard Time", "Central Asia Daylight Time", "Asia/Almaty"},
        {                        
            "Central Brazilian Standard Time", "Central Brazilian Daylight Time", "America/Cuiaba"},
        {                                                                   
            "Central Europe Standard Time", "Central Europe Daylight Time", "Europe/Budapest"},
        {                                                  
            "Central European Standard Time", "Central European Daylight Time", "Europe/Warsaw"},
        {                                            
            "Central Pacific Standard Time", "Central Pacific Daylight Time", "Pacific/Guadalcanal"},
        {                                            
            "Central Standard Time", "Central Daylight Time", "America/Chicago"},
        {                                                     
            "Central Standard Time (Mexico)", "Central Daylight Time (Mexico)", "America/Mexico_City"},
        {                                 
            "Chatham Islands Standard Time", "Chatham Islands Daylight Time", "Pacific/Chatham"},
        {                                                       
            "China Standard Time", "China Daylight Time", "Asia/Shanghai"},
        {                                      
            "Coordinated Universal Time", "Coordinated Universal Time", "UTC"},
        {                        
            "Cuba Standard Time", "Cuba Daylight Time", "America/Havana"},
        {                                              
            "Dateline Standard Time", "Dateline Daylight Time", "Etc/GMT+12"},
        {                         
            "E. Africa Standard Time", "E. Africa Daylight Time", "Africa/Nairobi"},
        {                          
            "E. Australia Standard Time", "E. Australia Daylight Time", "Australia/Brisbane"},
        {                          
            "E. Europe Standard Time", "E. Europe Daylight Time", "Europe/Chisinau"},
        {                          
            "E. South America Standard Time", "E. South America Daylight Time", "America/Sao_Paulo"},
        {                               
            "Easter Island Standard Time", "Easter Island Daylight Time", "Pacific/Easter"},
        {                                            
            "Eastern Standard Time", "Eastern Daylight Time", "America/New_York"},
        {                          
            "Eastern Standard Time (Mexico)", "Eastern Daylight Time (Mexico)", "America/Cancun"},
        {                       
            "Egypt Standard Time", "Egypt Daylight Time", "Africa/Cairo"},
        {                              
            "Ekaterinburg Standard Time", "Ekaterinburg Daylight Time", "Asia/Yekaterinburg"},
        {                      
            "Fiji Standard Time", "Fiji Daylight Time", "Pacific/Fiji"},
        {                                                               
            "FLE Standard Time", "FLE Daylight Time", "Europe/Kiev"},
        {                         
            "Georgian Standard Time", "Georgian Daylight Time", "Asia/Tbilisi"},
        {                                                   
            "GMT Standard Time", "GMT Daylight Time", "Europe/London"},
        {                           
            "Greenland Standard Time", "Greenland Daylight Time", "America/Godthab"},
        {   
                                                                            
                                                                         
                                                                               
                                                                            
                                                                           
            
                                                 
            "Greenwich Standard Time", "Greenwich Daylight Time", "Atlantic/Reykjavik"},
        {                                   
            "GTB Standard Time", "GTB Daylight Time", "Europe/Bucharest"},
        {                       
            "Haiti Standard Time", "Haiti Daylight Time", "America/Port-au-Prince"},
        {                        
            "Hawaiian Standard Time", "Hawaiian Daylight Time", "Pacific/Honolulu"},
        {                                                     
            "India Standard Time", "India Daylight Time", "Asia/Calcutta"},
        {                        
            "Iran Standard Time", "Iran Daylight Time", "Asia/Tehran"},
        {                           
            "Israel Standard Time", "Israel Daylight Time", "Asia/Jerusalem"},
        {                                                       
            "Jerusalem Standard Time", "Jerusalem Daylight Time", "Asia/Jerusalem"},
        {                       
            "Jordan Standard Time", "Jordan Daylight Time", "Asia/Amman"},
        {                             
            "Kaliningrad Standard Time", "Kaliningrad Daylight Time", "Europe/Kaliningrad"},
        {                                                
            "Kamchatka Standard Time", "Kamchatka Daylight Time", "Asia/Kamchatka"},
        {                       
            "Korea Standard Time", "Korea Daylight Time", "Asia/Seoul"},
        {                         
            "Libya Standard Time", "Libya Daylight Time", "Africa/Tripoli"},
        {                                   
            "Line Islands Standard Time", "Line Islands Daylight Time", "Pacific/Kiritimati"},
        {                                  
            "Lord Howe Standard Time", "Lord Howe Daylight Time", "Australia/Lord_Howe"},
        {                         
            "Magadan Standard Time", "Magadan Daylight Time", "Asia/Magadan"},
        {                              
            "Magallanes Standard Time", "Magallanes Daylight Time", "America/Punta_Arenas"},
        {                                         
            "Malay Peninsula Standard Time", "Malay Peninsula Daylight Time", "Asia/Kuala_Lumpur"},
        {                                   
            "Marquesas Standard Time", "Marquesas Daylight Time", "Pacific/Marquesas"},
        {                            
            "Mauritius Standard Time", "Mauritius Daylight Time", "Indian/Mauritius"},
        {                                                     
            "Mexico Standard Time", "Mexico Daylight Time", "America/Mexico_City"},
        {                                             
            "Mexico Standard Time 2", "Mexico Daylight Time 2", "America/Chihuahua"},
        {                                    
            "Mid-Atlantic Standard Time", "Mid-Atlantic Daylight Time", "Atlantic/South_Georgia"},
        {                        
            "Middle East Standard Time", "Middle East Daylight Time", "Asia/Beirut"},
        {                            
            "Montevideo Standard Time", "Montevideo Daylight Time", "America/Montevideo"},
        {                            
            "Morocco Standard Time", "Morocco Daylight Time", "Africa/Casablanca"},
        {                                             
            "Mountain Standard Time", "Mountain Daylight Time", "America/Denver"},
        {                                             
            "Mountain Standard Time (Mexico)", "Mountain Daylight Time (Mexico)", "America/Chihuahua"},
        {                                  
            "Myanmar Standard Time", "Myanmar Daylight Time", "Asia/Rangoon"},
        {                             
            "N. Central Asia Standard Time", "N. Central Asia Daylight Time", "Asia/Novosibirsk"},
        {                          
            "Namibia Standard Time", "Namibia Daylight Time", "Africa/Windhoek"},
        {                           
            "Nepal Standard Time", "Nepal Daylight Time", "Asia/Katmandu"},
        {                                      
            "New Zealand Standard Time", "New Zealand Daylight Time", "Pacific/Auckland"},
        {                              
            "Newfoundland Standard Time", "Newfoundland Daylight Time", "America/St_Johns"},
        {                                
            "Norfolk Standard Time", "Norfolk Daylight Time", "Pacific/Norfolk"},
        {                         
            "North Asia East Standard Time", "North Asia East Daylight Time", "Asia/Irkutsk"},
        {                             
            "North Asia Standard Time", "North Asia Daylight Time", "Asia/Krasnoyarsk"},
        {                           
            "North Korea Standard Time", "North Korea Daylight Time", "Asia/Pyongyang"},
        {                             
            "Novosibirsk Standard Time", "Novosibirsk Daylight Time", "Asia/Novosibirsk"},
        {                      
            "Omsk Standard Time", "Omsk Daylight Time", "Asia/Omsk"},
        {                          
            "Pacific SA Standard Time", "Pacific SA Daylight Time", "America/Santiago"},
        {                                            
            "Pacific Standard Time", "Pacific Daylight Time", "America/Los_Angeles"},
        {                                 
            "Pacific Standard Time (Mexico)", "Pacific Daylight Time (Mexico)", "America/Tijuana"},
        {                                    
            "Pakistan Standard Time", "Pakistan Daylight Time", "Asia/Karachi"},
        {                          
            "Paraguay Standard Time", "Paraguay Daylight Time", "America/Asuncion"},
        {                           
            "Qyzylorda Standard Time", "Qyzylorda Daylight Time", "Asia/Qyzylorda"},
        {                                                     
            "Romance Standard Time", "Romance Daylight Time", "Europe/Paris"},
        {                                 
            "Russia Time Zone 3", "Russia Time Zone 3", "Europe/Samara"},
        {                            
            "Russia Time Zone 10", "Russia Time Zone 10", "Asia/Srednekolymsk"},
        {                                                  
            "Russia Time Zone 11", "Russia Time Zone 11", "Asia/Kamchatka"},
        {                             
            "Russia TZ 1 Standard Time", "Russia TZ 1 Daylight Time", "Europe/Kaliningrad"},
        {                                        
            "Russia TZ 2 Standard Time", "Russia TZ 2 Daylight Time", "Europe/Moscow"},
        {                                 
            "Russia TZ 3 Standard Time", "Russia TZ 3 Daylight Time", "Europe/Samara"},
        {                              
            "Russia TZ 4 Standard Time", "Russia TZ 4 Daylight Time", "Asia/Yekaterinburg"},
        {                                     
            "Russia TZ 5 Standard Time", "Russia TZ 5 Daylight Time", "Asia/Novosibirsk"},
        {                             
            "Russia TZ 6 Standard Time", "Russia TZ 6 Daylight Time", "Asia/Krasnoyarsk"},
        {                         
            "Russia TZ 7 Standard Time", "Russia TZ 7 Daylight Time", "Asia/Irkutsk"},
        {                         
            "Russia TZ 8 Standard Time", "Russia TZ 8 Daylight Time", "Asia/Yakutsk"},
        {                             
            "Russia TZ 9 Standard Time", "Russia TZ 9 Daylight Time", "Asia/Vladivostok"},
        {                            
            "Russia TZ 10 Standard Time", "Russia TZ 10 Daylight Time", "Asia/Magadan"},
        {                                                  
            "Russia TZ 11 Standard Time", "Russia TZ 11 Daylight Time", "Asia/Anadyr"},
        {                                        
            "Russian Standard Time", "Russian Daylight Time", "Europe/Moscow"},
        {                                    
            "SA Eastern Standard Time", "SA Eastern Daylight Time", "America/Cayenne"},
        {                                                 
            "SA Pacific Standard Time", "SA Pacific Daylight Time", "America/Bogota"},
        {                                                      
            "SA Western Standard Time", "SA Western Daylight Time", "America/La_Paz"},
        {                                           
            "Saint Pierre Standard Time", "Saint Pierre Daylight Time", "America/Miquelon"},
        {                          
            "Sakhalin Standard Time", "Sakhalin Daylight Time", "Asia/Sakhalin"},
        {                       
            "Samoa Standard Time", "Samoa Daylight Time", "Pacific/Apia"},
        {                          
            "Sao Tome Standard Time", "Sao Tome Daylight Time", "Africa/Sao_Tome"},
        {                         
            "Saratov Standard Time", "Saratov Daylight Time", "Europe/Saratov"},
        {                                         
            "SE Asia Standard Time", "SE Asia Daylight Time", "Asia/Bangkok"},
        {                                         
            "Singapore Standard Time", "Singapore Daylight Time", "Asia/Singapore"},
        {                                  
            "South Africa Standard Time", "South Africa Daylight Time", "Africa/Johannesburg"},
        {                      
            "South Sudan Standard Time", "South Sudan Daylight Time", "Africa/Juba"},
        {                                     
            "Sri Lanka Standard Time", "Sri Lanka Daylight Time", "Asia/Colombo"},
        {                          
            "Sudan Standard Time", "Sudan Daylight Time", "Africa/Khartoum"},
        {                          
            "Syria Standard Time", "Syria Daylight Time", "Asia/Damascus"},
        {                        
            "Taipei Standard Time", "Taipei Daylight Time", "Asia/Taipei"},
        {                        
            "Tasmania Standard Time", "Tasmania Daylight Time", "Australia/Hobart"},
        {                           
            "Tocantins Standard Time", "Tocantins Daylight Time", "America/Araguaina"},
        {                                       
            "Tokyo Standard Time", "Tokyo Daylight Time", "Asia/Tokyo"},
        {                       
            "Tomsk Standard Time", "Tomsk Daylight Time", "Asia/Tomsk"},
        {                            
            "Tonga Standard Time", "Tonga Daylight Time", "Pacific/Tongatapu"},
        {                       
            "Transbaikal Standard Time", "Transbaikal Daylight Time", "Asia/Chita"},
        {                          
            "Turkey Standard Time", "Turkey Daylight Time", "Europe/Istanbul"},
        {                                  
            "Turks And Caicos Standard Time", "Turks And Caicos Daylight Time", "America/Grand_Turk"},
        {                             
            "Ulaanbaatar Standard Time", "Ulaanbaatar Daylight Time", "Asia/Ulaanbaatar"},
        {                                
            "US Eastern Standard Time", "US Eastern Daylight Time", "America/Indianapolis"},
        {                         
            "US Mountain Standard Time", "US Mountain Daylight Time", "America/Phoenix"},
        {                                      
            "UTC", "UTC", "UTC"},
        {                                               
            "UTC+12", "UTC+12", "Etc/GMT-12"},
        {                                               
            "UTC+13", "UTC+13", "Etc/GMT-13"},
        {                                               
            "UTC-02", "UTC-02", "Etc/GMT+2"},
        {                                               
            "UTC-08", "UTC-08", "Etc/GMT+8"},
        {                                               
            "UTC-09", "UTC-09", "Etc/GMT+9"},
        {                                               
            "UTC-11", "UTC-11", "Etc/GMT+11"},
        {                         
            "Venezuela Standard Time", "Venezuela Daylight Time", "America/Caracas"},
        {                             
            "Vladivostok Standard Time", "Vladivostok Daylight Time", "Asia/Vladivostok"},
        {                           
            "Volgograd Standard Time", "Volgograd Daylight Time", "Europe/Volgograd"},
        {                       
            "W. Australia Standard Time", "W. Australia Daylight Time", "Australia/Perth"},
        {                                     
            "W. Central Africa Standard Time", "W. Central Africa Daylight Time", "Africa/Lagos"},
        {                                                                  
            "W. Europe Standard Time", "W. Europe Daylight Time", "Europe/Berlin"},
        {                      
            "W. Mongolia Standard Time", "W. Mongolia Daylight Time", "Asia/Hovd"},
        {                                    
            "West Asia Standard Time", "West Asia Daylight Time", "Asia/Tashkent"},
        {                              
            "West Bank Gaza Standard Time", "West Bank Gaza Daylight Time", "Asia/Gaza"},
        {                              
            "West Bank Standard Time", "West Bank Daylight Time", "Asia/Hebron"},
        {                                    
            "West Pacific Standard Time", "West Pacific Daylight Time", "Pacific/Port_Moresby"},
        {                         
            "Yakutsk Standard Time", "Yakutsk Daylight Time", "Asia/Yakutsk"},
        {                       
            "Yukon Standard Time", "Yukon Daylight Time", "America/Whitehorse"},
        {NULL, NULL, NULL}};

static const char *
identify_system_timezone(void)
{
  int i;
  char tzname[128];
  char localtzname[256];
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  HKEY rootKey;
  int idx;

  if (!tm)
  {
#ifdef DEBUG_IDENTIFY_TIMEZONE
    fprintf(stderr, "could not identify system time zone: localtime() failed\n");
#endif
    return NULL;                
  }

  memset(tzname, 0, sizeof(tzname));
  strftime(tzname, sizeof(tzname) - 1, "%Z", tm);

  for (i = 0; win32_tzmap[i].stdname != NULL; i++)
  {
    if (strcmp(tzname, win32_tzmap[i].stdname) == 0 || strcmp(tzname, win32_tzmap[i].dstname) == 0)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "TZ \"%s\" matches system time zone \"%s\"\n", win32_tzmap[i].pgtzname, tzname);
#endif
      return win32_tzmap[i].pgtzname;
    }
  }

     
                                                                         
                                                                       
                              
     
  memset(localtzname, 0, sizeof(localtzname));
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones", 0, KEY_READ, &rootKey) != ERROR_SUCCESS)
  {
#ifdef DEBUG_IDENTIFY_TIMEZONE
    fprintf(stderr, "could not open registry key to identify system time zone: error code %lu\n", GetLastError());
#endif
    return NULL;                
  }

  for (idx = 0;; idx++)
  {
    char keyname[256];
    char zonename[256];
    DWORD namesize;
    FILETIME lastwrite;
    HKEY key;
    LONG r;

    memset(keyname, 0, sizeof(keyname));
    namesize = sizeof(keyname);
    if ((r = RegEnumKeyEx(rootKey, idx, keyname, &namesize, NULL, NULL, NULL, &lastwrite)) != ERROR_SUCCESS)
    {
      if (r == ERROR_NO_MORE_ITEMS)
      {
        break;
      }
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "could not enumerate registry subkeys to identify system time zone: %d\n", (int)r);
#endif
      break;
    }

    if ((r = RegOpenKeyEx(rootKey, keyname, 0, KEY_READ, &key)) != ERROR_SUCCESS)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "could not open registry subkey to identify system time zone: %d\n", (int)r);
#endif
      break;
    }

    memset(zonename, 0, sizeof(zonename));
    namesize = sizeof(zonename);
    if ((r = RegQueryValueEx(key, "Std", NULL, NULL, (unsigned char *)zonename, &namesize)) != ERROR_SUCCESS)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "could not query value for key \"std\" to identify system time zone \"%s\": %d\n", keyname, (int)r);
#endif
      RegCloseKey(key);
      continue;                                           
    }
    if (strcmp(tzname, zonename) == 0)
    {
                        
      strcpy(localtzname, keyname);
      RegCloseKey(key);
      break;
    }
    memset(zonename, 0, sizeof(zonename));
    namesize = sizeof(zonename);
    if ((r = RegQueryValueEx(key, "Dlt", NULL, NULL, (unsigned char *)zonename, &namesize)) != ERROR_SUCCESS)
    {
#ifdef DEBUG_IDENTIFY_TIMEZONE
      fprintf(stderr, "could not query value for key \"dlt\" to identify system time zone \"%s\": %d\n", keyname, (int)r);
#endif
      RegCloseKey(key);
      continue;                                           
    }
    if (strcmp(tzname, zonename) == 0)
    {
                            
      strcpy(localtzname, keyname);
      RegCloseKey(key);
      break;
    }

    RegCloseKey(key);
  }

  RegCloseKey(rootKey);

  if (localtzname[0])
  {
                                                          
    for (i = 0; win32_tzmap[i].stdname != NULL; i++)
    {
      if (strcmp(localtzname, win32_tzmap[i].stdname) == 0 || strcmp(localtzname, win32_tzmap[i].dstname) == 0)
      {
#ifdef DEBUG_IDENTIFY_TIMEZONE
        fprintf(stderr, "TZ \"%s\" matches localized system time zone \"%s\" (\"%s\")\n", win32_tzmap[i].pgtzname, tzname, localtzname);
#endif
        return win32_tzmap[i].pgtzname;
      }
    }
  }

#ifdef DEBUG_IDENTIFY_TIMEZONE
  fprintf(stderr, "could not find a match for system time zone \"%s\"\n", tzname);
#endif
  return NULL;                
}
#endif            

   
                                                                            
   
static bool
validate_zone(const char *tzname)
{
  pg_tz *tz;

  if (!tzname || !tzname[0])
  {
    return false;
  }

  tz = pg_load_tz(tzname);
  if (!tz)
  {
    return false;
  }

  if (!pg_tz_acceptable(tz))
  {
    return false;
  }

  return true;
}

   
                                                                          
   
                                                                         
                                                 
   
                                                                      
                                                                      
                                                                           
                                                          
   
const char *
select_default_timezone(const char *share_path)
{
  const char *tzname;

                                                     
#ifndef SYSTEMTZDIR
  snprintf(tzdirpath, sizeof(tzdirpath), "%s/timezone", share_path);
#endif

                                     
  tzname = getenv("TZ");
  if (validate_zone(tzname))
  {
    return tzname;
  }

                                                    
  tzname = identify_system_timezone();
  if (validate_zone(tzname))
  {
    return tzname;
  }

  return NULL;
}
