                                                

#include "postgres_fe.h"

#include <time.h>
#include <ctype.h>
#include <math.h>

#include "pgtypeslib_extern.h"
#include "dt.h"
#include "pgtypes_timestamp.h"

const int day_tab[2][13] = {{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}, {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0}};

typedef long AbsoluteTime;

static const datetkn datetktbl[] = {
                             
    {EARLY, RESERV, DTK_EARLY},                                                        
    {"acsst", DTZ, 37800},                                       
    {"acst", DTZ, -14400},                                           
    {"act", TZ, -18000},                                             
    {DA_D, ADBC, AD},                                                
    {"adt", DTZ, -10800},                                               
    {"aesst", DTZ, 39600},                                    
    {"aest", TZ, 36000},                                                    
    {"aft", TZ, 16200},                                
    {"ahst", TZ, -36000},                                               
    {"akdt", DTZ, -28800},                                            
    {"akst", DTZ, -32400},                                            
    {"allballs", RESERV, DTK_ZULU},                       
    {"almst", TZ, 25200},                                            
    {"almt", TZ, 21600},                                     
    {"am", AMPM, AM}, {"amst", DTZ, 18000},                                    
#if 0
	{"amst", DTZ, -10800},		                 
#endif
    {"amt", TZ, 14400},                                
    {"anast", DTZ, 46800},                                  
    {"anat", TZ, 43200},                             
    {"apr", MONTH, 4}, {"april", MONTH, 4},
#if 0
	aqtst
	aqtt
	arst
#endif
    {"art", TZ, -10800},                     
#if 0
	ashst
	ast							                                           
                                      
#endif
    {"ast", TZ, -14400},                                                                            
    {"at", IGNORE_DTF, 0},                                                                
    {"aug", MONTH, 8}, {"august", MONTH, 8}, {"awsst", DTZ, 32400},                   
    {"awst", TZ, 28800},                                                              
    {"awt", DTZ, -10800}, {"azost", DTZ, 0},                                                
    {"azot", TZ, -3600},                                                             
    {"azst", DTZ, 18000},                                                                       
    {"azt", TZ, 14400},                                                                  
    {DB_C, ADBC, BC},                                                                       
    {"bdst", TZ, 7200},                                                                             
    {"bdt", TZ, 21600},                                                        
    {"bnt", TZ, 28800},                                                                         
    {"bort", TZ, 28800},                                                                         
#if 0
	bortst
	bost
#endif
    {"bot", TZ, -14400},                   
    {"bra", TZ, -10800},                  
#if 0
	brst
	brt
#endif
    {"bst", DTZ, 3600},                          
#if 0
	{"bst", TZ, -10800},		                          
	{"bst", DTZ, -39600},		                        
#endif
    {"bt", TZ, 10800},                      
    {"btt", TZ, 21600},                    
    {"cadt", DTZ, 37800},                             
    {"cast", TZ, 34200},                             
    {"cat", TZ, -36000},                           
    {"cct", TZ, 28800},                         
#if 0
	{"cct", TZ, 23400},			                                
#endif
    {"cdt", DTZ, -18000},                             
    {"cest", DTZ, 7200},                                   
    {"cet", TZ, 3600},                                
    {"cetdst", DTZ, 7200},                                 
    {"chadt", DTZ, 49500},                                           
    {"chast", TZ, 45900},                                   
#if 0
	ckhst
#endif
    {"ckt", TZ, 43200},                           
    {"clst", DTZ, -10800},                        
    {"clt", TZ, -14400},                   
#if 0
	cost
#endif
    {"cot", TZ, -18000},                    
    {"cst", TZ, -21600},                            
#if 0
	cvst
#endif
    {"cvt", TZ, 25200},                                                                                       
    {"cxt", TZ, 25200},                                                                                       
    {"d", UNITS, DTK_DAY},                                                                            
    {"davt", TZ, 25200},                                                                         
    {"ddut", TZ, 36000},                                                                                    
    {"dec", MONTH, 12}, {"december", MONTH, 12}, {"dnt", TZ, 3600},                       
    {"dow", UNITS, DTK_DOW},                                                         
    {"doy", UNITS, DTK_DOY},                                                         
    {"dst", DTZMOD, SECS_PER_HOUR},
#if 0
	{"dusst", DTZ, 21600},		                          
#endif
    {"easst", DTZ, -18000},                                
    {"east", TZ, -21600},                           
    {"eat", TZ, 10800},                           
#if 0
	{"east", DTZ, 14400},		                                      
	{"eat", TZ, 10800},			                              
	{"ect", TZ, -14400},		                            
	{"ect", TZ, -18000},		                  
#endif
    {"edt", DTZ, -14400},                              
    {"eest", DTZ, 10800},                                   
    {"eet", TZ, 7200},                                     
    {"eetdst", DTZ, 10800},                                   
    {"egst", DTZ, 0},                                       
    {"egt", TZ, -3600},                              
#if 0
	ehdt
#endif
    {EPOCH, RESERV, DTK_EPOCH},                                                                                   
    {"est", TZ, -18000},                                                                         
    {"feb", MONTH, 2}, {"february", MONTH, 2}, {"fjst", DTZ, -46800},                                         
    {"fjt", TZ, -43200},                                                             
    {"fkst", DTZ, -10800},                                                                              
    {"fkt", TZ, -7200},                                                                          
#if 0
	fnst
	fnt
#endif
    {"fri", DOW, 5}, {"friday", DOW, 5}, {"fst", TZ, 3600},                         
    {"fwt", DTZ, 7200},                                                              
    {"galt", TZ, -21600},                                                       
    {"gamt", TZ, -32400},                                                     
    {"gest", DTZ, 18000},                                                            
    {"get", TZ, 14400},                                                       
    {"gft", TZ, -10800},                                                            
#if 0
	ghst
#endif
    {"gilt", TZ, 43200},                              
    {"gmt", TZ, 0},                                  
    {"gst", TZ, 36000},                                     
    {"gyt", TZ, -14400},                     
    {"h", UNITS, DTK_HOUR},             
#if 0
	hadt
	hast
#endif
    {"hdt", DTZ, -32400},                                  
#if 0
	hkst
#endif
    {"hkt", TZ, 28800},                     
#if 0
	{"hmt", TZ, 10800},			                
	hovst
	hovt
#endif
    {"hst", TZ, -36000},                      
#if 0
	hwt
#endif
    {"ict", TZ, 25200},                       
    {"idle", TZ, 43200},                             
    {"idlw", TZ, -43200},                            
#if 0
	idt							                                         
#endif
    {LATE, RESERV, DTK_LATE},                                               
    {"iot", TZ, 18000},                                    
    {"irkst", DTZ, 32400},                                  
    {"irkt", TZ, 28800},                             
    {"irt", TZ, 12600},                           
    {"isodow", UNITS, DTK_ISODOW},                                   
#if 0
	isst
#endif
    {"ist", TZ, 7200},                                                                                                                            
    {"it", TZ, 12600},                                                                                                                               
    {"j", UNITS, DTK_JULIAN}, {"jan", MONTH, 1}, {"january", MONTH, 1}, {"javt", TZ, 25200},                                                                         
    {"jayt", TZ, 32400},                                                                                                                                             
    {"jd", UNITS, DTK_JULIAN}, {"jst", TZ, 32400},                                                                                                                    
    {"jt", TZ, 27000},                                                                                                                                                 
    {"jul", MONTH, 7}, {"julian", UNITS, DTK_JULIAN}, {"july", MONTH, 7}, {"jun", MONTH, 6}, {"june", MONTH, 6}, {"kdt", DTZ, 36000},                          
    {"kgst", DTZ, 21600},                                                                                                                                         
    {"kgt", TZ, 18000},                                                                                                                                    
    {"kost", TZ, 43200},                                                                                                                               
    {"krast", DTZ, 25200},                                                                                                                                         
    {"krat", TZ, 28800},                                                                                                                                             
    {"kst", TZ, 32400},                                                                                                                                        
    {"lhdt", DTZ, 39600},                                                                                                                                                     
    {"lhst", TZ, 37800},                                                                                                                                                      
    {"ligt", TZ, 36000},                                                                                                                                             
    {"lint", TZ, 50400},                                                                                                                                                            
    {"lkt", TZ, 21600},                                                                                                                               
    {"m", UNITS, DTK_MONTH},                                                                                                                                     
    {"magst", DTZ, 43200},                                                                                                                                     
    {"magt", TZ, 39600},                                                                                                                                
    {"mar", MONTH, 3}, {"march", MONTH, 3}, {"mart", TZ, -34200},                                                                                         
    {"mawt", TZ, 21600},                                                                                                                                      
    {"may", MONTH, 5}, {"mdt", DTZ, -21600},                                                                                                                      
    {"mest", DTZ, 7200},                                                                                                                                             
    {"met", TZ, 3600},                                                                                                                                        
    {"metdst", DTZ, 7200},                                                                                                                                             
    {"mewt", TZ, 3600},                                                                                                                                              
    {"mez", TZ, 3600},                                                                                                                                        
    {"mht", TZ, 43200},                                                                                                                              
    {"mm", UNITS, DTK_MINUTE},                                                                                                                                    
    {"mmt", TZ, 23400},                                                                                                                                 
    {"mon", DOW, 1}, {"monday", DOW, 1},
#if 0
	most
#endif
    {"mpt", TZ, 36000},                                  
    {"msd", DTZ, 14400},                         
    {"msk", TZ, 10800},                   
    {"mst", TZ, -25200},                             
    {"mt", TZ, 30600},                      
    {"mut", TZ, 14400},                             
    {"mvt", TZ, 18000},                            
    {"myt", TZ, 28800},                     
#if 0
	ncst
#endif
    {"nct", TZ, 39600},                                                                         
    {"ndt", DTZ, -9000},                                                                         
    {"nft", TZ, -12600},                                                                                
    {"nor", TZ, 3600},                                                                            
    {"nov", MONTH, 11}, {"november", MONTH, 11}, {"novst", DTZ, 25200},                              
    {"novt", TZ, 21600},                                                                               
    {NOW, RESERV, DTK_NOW},                                                                           
    {"npt", TZ, 20700},                                                                                     
    {"nst", TZ, -12600},                                                                         
    {"nt", TZ, -39600},                                                                
    {"nut", TZ, -39600},                                                               
    {"nzdt", DTZ, 46800},                                                                              
    {"nzst", TZ, 43200},                                                                               
    {"nzt", TZ, 43200},                                                                       
    {"oct", MONTH, 10}, {"october", MONTH, 10}, {"omsst", DTZ, 25200},                        
    {"omst", TZ, 21600},                                                               
    {"on", IGNORE_DTF, 0},                                                                    
    {"pdt", DTZ, -25200},                                                                          
#if 0
	pest
#endif
    {"pet", TZ, -18000},                  
    {"petst", DTZ, 46800},                                           
    {"pett", TZ, 43200},                                      
    {"pgt", TZ, 36000},                               
    {"phot", TZ, 46800},                                        
#if 0
	phst
#endif
    {"pht", TZ, 28800},                                          
    {"pkt", TZ, 18000},                                        
    {"pm", AMPM, PM}, {"pmdt", DTZ, -7200},                                      
#if 0
	pmst
#endif
    {"pont", TZ, 39600},                                    
    {"pst", TZ, -28800},                                 
    {"pwt", TZ, 32400},                       
    {"pyst", DTZ, -10800},                              
    {"pyt", TZ, -14400},                         
    {"ret", DTZ, 14400},                               
    {"s", UNITS, DTK_SECOND},                              
    {"sadt", DTZ, 37800},                                   
#if 0
	samst
	samt
#endif
    {"sast", TZ, 34200},                                
    {"sat", DOW, 6}, {"saturday", DOW, 6},
#if 0
	sbt
#endif
    {"sct", DTZ, 14400},                                                                                      
    {"sep", MONTH, 9}, {"sept", MONTH, 9}, {"september", MONTH, 9}, {"set", TZ, -3600},                         
#if 0
	sgt
#endif
    {"sst", DTZ, 7200},                                                              
    {"sun", DOW, 0}, {"sunday", DOW, 0}, {"swt", TZ, 3600},                          
#if 0
	syot
#endif
    {"t", ISOTIME, DTK_TIME},                                                                                                       
    {"tft", TZ, 18000},                                                                                                 
    {"that", TZ, -36000},                                                                                            
    {"thu", DOW, 4}, {"thur", DOW, 4}, {"thurs", DOW, 4}, {"thursday", DOW, 4}, {"tjt", TZ, 18000},                      
    {"tkt", TZ, -36000},                                                                                              
    {"tmt", TZ, 18000},                                                                                                    
    {TODAY, RESERV, DTK_TODAY},                                                                                   
    {TOMORROW, RESERV, DTK_TOMORROW},                                                                                      
#if 0
	tost
#endif
    {"tot", TZ, 46800},                 
#if 0
	tpt
#endif
    {"truk", TZ, 36000},                                                                       
    {"tue", DOW, 2}, {"tues", DOW, 2}, {"tuesday", DOW, 2}, {"tvt", TZ, 43200},                  
#if 0
	uct
#endif
    {"ulast", DTZ, 32400},                                                           
    {"ulat", TZ, 28800},                                                      
    {"ut", TZ, 0}, {"utc", TZ, 0}, {"uyst", DTZ, -7200},                          
    {"uyt", TZ, -10800},                                                   
    {"uzst", DTZ, 21600},                                                            
    {"uzt", TZ, 18000},                                                       
    {"vet", TZ, -14400},                                                     
    {"vlast", DTZ, 39600},                                                            
    {"vlat", TZ, 36000},                                                       
#if 0
	vust
#endif
    {"vut", TZ, 39600},                     
    {"wadt", DTZ, 28800},                          
    {"wakt", TZ, 43200},                 
#if 0
	warst
#endif
    {"wast", TZ, 25200},                                                                                         
    {"wat", TZ, -3600},                                                                                  
    {"wdt", DTZ, 32400},                                                                                    
    {"wed", DOW, 3}, {"wednesday", DOW, 3}, {"weds", DOW, 3}, {"west", DTZ, 3600},                                 
    {"wet", TZ, 0},                                                                                    
    {"wetdst", DTZ, 3600},                                                                                                   
    {"wft", TZ, 43200},                                                                                        
    {"wgst", DTZ, -7200},                                                                                          
    {"wgt", TZ, -10800},                                                                                    
    {"wst", TZ, 28800},                                                                                               
    {"y", UNITS, DTK_YEAR},                                                                                  
    {"yakst", DTZ, 36000},                                                                                  
    {"yakt", TZ, 32400},                                                                             
    {"yapt", TZ, 36000},                                                                                      
    {"ydt", DTZ, -28800},                                                                                   
    {"yekst", DTZ, 21600},                                                                                        
    {"yekt", TZ, 18000},                                                                                   
    {YESTERDAY, RESERV, DTK_YESTERDAY},                                                                    
    {"yst", TZ, -32400},                                                                                    
    {"z", TZ, 0},                                                                                                  
    {"zp4", TZ, -14400},                                                                               
    {"zp5", TZ, -18000},                                                                               
    {"zp6", TZ, -21600},                                                                               
    {ZULU, TZ, 0},                                                                          
};

static const datetkn deltatktbl[] = {
                             
    {"@", IGNORE_DTF, 0},                                                                                                                                                                                                                                     
    {DAGO, AGO, 0},                                                                                                                                                                                                                                                       
    {"c", UNITS, DTK_CENTURY},                                                                                                                                                                                                                          
    {"cent", UNITS, DTK_CENTURY},                                                                                                                                                                                                                       
    {"centuries", UNITS, DTK_CENTURY},                                                                                                                                                                                                                    
    {DCENTURY, UNITS, DTK_CENTURY},                                                                                                                                                                                                                     
    {"d", UNITS, DTK_DAY},                                                                                                                                                                                                                          
    {DDAY, UNITS, DTK_DAY},                                                                                                                                                                                                                         
    {"days", UNITS, DTK_DAY},                                                                                                                                                                                                                        
    {"dec", UNITS, DTK_DECADE},                                                                                                                                                                                                                        
    {DDECADE, UNITS, DTK_DECADE},                                                                                                                                                                                                                      
    {"decades", UNITS, DTK_DECADE},                                                                                                                                                                                                                     
    {"decs", UNITS, DTK_DECADE},                                                                                                                                                                                                                        
    {"h", UNITS, DTK_HOUR},                                                                                                                                                                                                                          
    {DHOUR, UNITS, DTK_HOUR},                                                                                                                                                                                                                        
    {"hours", UNITS, DTK_HOUR},                                                                                                                                                                                                                       
    {"hr", UNITS, DTK_HOUR},                                                                                                                                                                                                                         
    {"hrs", UNITS, DTK_HOUR},                                                                                                                                                                                                                         
    {"m", UNITS, DTK_MINUTE},                                                                                                                                                                                                                          
    {"microsecon", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                    
    {"mil", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                        
    {"millennia", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                 
    {DMILLENNIUM, UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                  
    {"millisecon", UNITS, DTK_MILLISEC},                                                                                                                                                                                                      
    {"mils", UNITS, DTK_MILLENNIUM},                                                                                                                                                                                                                      
    {"min", UNITS, DTK_MINUTE},                                                                                                                                                                                                                        
    {"mins", UNITS, DTK_MINUTE},                                                                                                                                                                                                                        
    {DMINUTE, UNITS, DTK_MINUTE},                                                                                                                                                                                                                      
    {"minutes", UNITS, DTK_MINUTE},                                                                                                                                                                                                                     
    {"mon", UNITS, DTK_MONTH},                                                                                                                                                                                                                         
    {"mons", UNITS, DTK_MONTH},                                                                                                                                                                                                                        
    {DMONTH, UNITS, DTK_MONTH},                                                                                                                                                                                                                       
    {"months", UNITS, DTK_MONTH}, {"ms", UNITS, DTK_MILLISEC}, {"msec", UNITS, DTK_MILLISEC}, {DMILLISEC, UNITS, DTK_MILLISEC}, {"mseconds", UNITS, DTK_MILLISEC}, {"msecs", UNITS, DTK_MILLISEC}, {"qtr", UNITS, DTK_QUARTER},                         
    {DQUARTER, UNITS, DTK_QUARTER},                                                                                                                                                                                                                     
    {"s", UNITS, DTK_SECOND}, {"sec", UNITS, DTK_SECOND}, {DSECOND, UNITS, DTK_SECOND}, {"seconds", UNITS, DTK_SECOND}, {"secs", UNITS, DTK_SECOND}, {DTIMEZONE, UNITS, DTK_TZ},                                                                            
    {"timezone_h", UNITS, DTK_TZ_HOUR},                                                                                                                                                                                                                  
    {"timezone_m", UNITS, DTK_TZ_MINUTE},                                                                                                                                                                                                                   
    {"us", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                            
    {"usec", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                          
    {DMICROSEC, UNITS, DTK_MICROSEC},                                                                                                                                                                                                                       
    {"useconds", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                       
    {"usecs", UNITS, DTK_MICROSEC},                                                                                                                                                                                                                          
    {"w", UNITS, DTK_WEEK},                                                                                                                                                                                                                          
    {DWEEK, UNITS, DTK_WEEK},                                                                                                                                                                                                                        
    {"weeks", UNITS, DTK_WEEK},                                                                                                                                                                                                                       
    {"y", UNITS, DTK_YEAR},                                                                                                                                                                                                                          
    {DYEAR, UNITS, DTK_YEAR},                                                                                                                                                                                                                        
    {"years", UNITS, DTK_YEAR},                                                                                                                                                                                                                       
    {"yr", UNITS, DTK_YEAR},                                                                                                                                                                                                                         
    {"yrs", UNITS, DTK_YEAR},                                                                                                                                                                                                                         
};

static const unsigned int szdatetktbl = lengthof(datetktbl);
static const unsigned int szdeltatktbl = lengthof(deltatktbl);

static const datetkn *datecache[MAXDATEFIELDS] = {NULL};

static const datetkn *deltacache[MAXDATEFIELDS] = {NULL};

char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};

char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", NULL};

char *pgtypes_date_weekdays_short[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *pgtypes_date_months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December", NULL};

static const datetkn *
datebsearch(const char *key, const datetkn *base, unsigned int nel)
{
  if (nel > 0)
  {
    const datetkn *last = base + nel - 1, *position;
    int result;

    while (last >= base)
    {
      position = base + ((last - base) >> 1);
                                                                 
      result = (int)key[0] - (int)position->token[0];
      if (result == 0)
      {
                                                           
        result = strncmp(key, position->token, TOKMAXLEN);
        if (result == 0)
        {
          return position;
        }
      }
      if (result < 0)
      {
        last = position - 1;
      }
      else
      {
        base = position + 1;
      }
    }
  }
  return NULL;
}

                 
                                          
                                                 
   
int
DecodeUnits(int field, char *lowtoken, int *val)
{
  int type;
  const datetkn *tp;

                                                     
  if (deltacache[field] != NULL && strncmp(lowtoken, deltacache[field]->token, TOKMAXLEN) == 0)
  {
    tp = deltacache[field];
  }
  else
  {
    tp = datebsearch(lowtoken, deltatktbl, szdeltatktbl);
  }
  deltacache[field] = tp;
  if (tp == NULL)
  {
    type = UNKNOWN_FIELD;
    *val = 0;
  }
  else
  {
    type = tp->type;
    *val = tp->value;
  }

  return type;
}                    

   
                                             
                                                              
                                                                
                                                                  
                                                      
                                 
   
                                                           
                     
   
                                                                 
                                                             
                                                              
                                                                 
                       
   

int
date2j(int y, int m, int d)
{
  int julian;
  int century;

  if (m > 2)
  {
    m += 1;
    y += 4800;
  }
  else
  {
    m += 13;
    y += 4799;
  }

  century = y / 100;
  julian = y * 365 - 32167;
  julian += y / 4 - century + century / 4;
  julian += 7834 * m / 256 + d;

  return julian;
}               

void
j2date(int jd, int *year, int *month, int *day)
{
  unsigned int julian;
  unsigned int quad;
  unsigned int extra;
  int y;

  julian = jd;
  julian += 32044;
  quad = julian / 146097;
  extra = (julian - quad * 146097) * 4 + 3;
  julian += 60 + quad * 3 + extra / 146097;
  quad = julian / 1461;
  julian -= quad * 1461;
  y = julian * 4 / 1461;
  julian = ((y != 0) ? (julian + 305) % 365 : (julian + 306) % 366) + 123;
  y += quad * 4;
  *year = y - 4800;
  quad = julian * 2141 / 65536;
  *day = julian - 7834 * quad / 256;
  *month = (quad + 10) % 12 + 1;

  return;
}               

                   
                                          
                                                          
                              
   
static int
DecodeSpecial(int field, char *lowtoken, int *val)
{
  int type;
  const datetkn *tp;

                                                     
  if (datecache[field] != NULL && strncmp(lowtoken, datecache[field]->token, TOKMAXLEN) == 0)
  {
    tp = datecache[field];
  }
  else
  {
    tp = NULL;
    if (!tp)
    {
      tp = datebsearch(lowtoken, datetktbl, szdatetktbl);
    }
  }
  datecache[field] = tp;
  if (tp == NULL)
  {
    type = UNKNOWN_FIELD;
    *val = 0;
  }
  else
  {
    type = tp->type;
    *val = tp->value;
  }

  return type;
}                      

                    
                              
   
void
EncodeDateOnly(struct tm *tm, int style, char *str, bool EuroDates)
{
  Assert(tm->tm_mon >= 1 && tm->tm_mon <= MONTHS_PER_YEAR);

  switch (style)
  {
  case USE_ISO_DATES:
                                          
    if (tm->tm_year > 0)
    {
      sprintf(str, "%04d-%02d-%02d", tm->tm_year, tm->tm_mon, tm->tm_mday);
    }
    else
    {
      sprintf(str, "%04d-%02d-%02d %s", -(tm->tm_year - 1), tm->tm_mon, tm->tm_mday, "BC");
    }
    break;

  case USE_SQL_DATES:
                                                    
    if (EuroDates)
    {
      sprintf(str, "%02d/%02d", tm->tm_mday, tm->tm_mon);
    }
    else
    {
      sprintf(str, "%02d/%02d", tm->tm_mon, tm->tm_mday);
    }
    if (tm->tm_year > 0)
    {
      sprintf(str + 5, "/%04d", tm->tm_year);
    }
    else
    {
      sprintf(str + 5, "/%04d %s", -(tm->tm_year - 1), "BC");
    }
    break;

  case USE_GERMAN_DATES:
                                  
    sprintf(str, "%02d.%02d", tm->tm_mday, tm->tm_mon);
    if (tm->tm_year > 0)
    {
      sprintf(str + 5, ".%04d", tm->tm_year);
    }
    else
    {
      sprintf(str + 5, ".%04d %s", -(tm->tm_year - 1), "BC");
    }
    break;

  case USE_POSTGRES_DATES:
  default:
                                                  
    if (EuroDates)
    {
      sprintf(str, "%02d-%02d", tm->tm_mday, tm->tm_mon);
    }
    else
    {
      sprintf(str, "%02d-%02d", tm->tm_mon, tm->tm_mday);
    }
    if (tm->tm_year > 0)
    {
      sprintf(str + 5, "-%04d", tm->tm_year);
    }
    else
    {
      sprintf(str + 5, "-%04d %s", -(tm->tm_year - 1), "BC");
    }
    break;
  }
}

void
TrimTrailingZeros(char *str)
{
  int len = strlen(str);

                                                                         
  while (*(str + len - 1) == '0' && *(str + len - 3) != '.')
  {
    len--;
    *(str + len) = '\0';
  }
}

                    
                                                   
   
                                                                               
                                                                               
                                                                        
                                                                          
                                            
   
                          
                                       
                                   
                                  
                                   
                                                                          
                   
                         
   
void
EncodeDateTime(struct tm *tm, fsec_t fsec, bool print_tz, int tz, const char *tzn, int style, char *str, bool EuroDates)
{
  int day, hour, min;

     
                                                                     
     
  if (tm->tm_isdst < 0)
  {
    print_tz = false;
  }

  switch (style)
  {
  case USE_ISO_DATES:
                                               

    sprintf(str, "%04d-%02d-%02d %02d:%02d", (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min);

       
                                                                      
                                                     
       
    if (fsec != 0)
    {
      sprintf(str + strlen(str), ":%02d.%06d", tm->tm_sec, fsec);
      TrimTrailingZeros(str);
    }
    else
    {
      sprintf(str + strlen(str), ":%02d", tm->tm_sec);
    }

    if (tm->tm_year <= 0)
    {
      sprintf(str + strlen(str), " BC");
    }

    if (print_tz)
    {
      hour = -(tz / SECS_PER_HOUR);
      min = (abs(tz) / MINS_PER_HOUR) % MINS_PER_HOUR;
      if (min != 0)
      {
        sprintf(str + strlen(str), "%+03d:%02d", hour, min);
      }
      else
      {
        sprintf(str + strlen(str), "%+03d", hour);
      }
    }
    break;

  case USE_SQL_DATES:
                                                    

    if (EuroDates)
    {
      sprintf(str, "%02d/%02d", tm->tm_mday, tm->tm_mon);
    }
    else
    {
      sprintf(str, "%02d/%02d", tm->tm_mon, tm->tm_mday);
    }

    sprintf(str + 5, "/%04d %02d:%02d", (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), tm->tm_hour, tm->tm_min);

       
                                                                      
                                                     
       
    if (fsec != 0)
    {
      sprintf(str + strlen(str), ":%02d.%06d", tm->tm_sec, fsec);
      TrimTrailingZeros(str);
    }
    else
    {
      sprintf(str + strlen(str), ":%02d", tm->tm_sec);
    }

    if (tm->tm_year <= 0)
    {
      sprintf(str + strlen(str), " BC");
    }

       
                                                                     
                                                                       
                                                              
       

    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str + strlen(str), " %.*s", MAXTZLEN, tzn);
      }
      else
      {
        hour = -(tz / SECS_PER_HOUR);
        min = (abs(tz) / MINS_PER_HOUR) % MINS_PER_HOUR;
        if (min != 0)
        {
          sprintf(str + strlen(str), "%+03d:%02d", hour, min);
        }
        else
        {
          sprintf(str + strlen(str), "%+03d", hour);
        }
      }
    }
    break;

  case USE_GERMAN_DATES:
                                          

    sprintf(str, "%02d.%02d", tm->tm_mday, tm->tm_mon);

    sprintf(str + 5, ".%04d %02d:%02d", (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1), tm->tm_hour, tm->tm_min);

       
                                                                      
                                                     
       
    if (fsec != 0)
    {
      sprintf(str + strlen(str), ":%02d.%06d", tm->tm_sec, fsec);
      TrimTrailingZeros(str);
    }
    else
    {
      sprintf(str + strlen(str), ":%02d", tm->tm_sec);
    }

    if (tm->tm_year <= 0)
    {
      sprintf(str + strlen(str), " BC");
    }

    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str + strlen(str), " %.*s", MAXTZLEN, tzn);
      }
      else
      {
        hour = -(tz / SECS_PER_HOUR);
        min = (abs(tz) / MINS_PER_HOUR) % MINS_PER_HOUR;
        if (min != 0)
        {
          sprintf(str + strlen(str), "%+03d:%02d", hour, min);
        }
        else
        {
          sprintf(str + strlen(str), "%+03d", hour);
        }
      }
    }
    break;

  case USE_POSTGRES_DATES:
  default:
                                                                     

    day = date2j(tm->tm_year, tm->tm_mon, tm->tm_mday);
    tm->tm_wday = (int)((day + date2j(2000, 1, 1) + 1) % 7);

    memcpy(str, days[tm->tm_wday], 3);
    strcpy(str + 3, " ");

    if (EuroDates)
    {
      sprintf(str + 4, "%02d %3s", tm->tm_mday, months[tm->tm_mon - 1]);
    }
    else
    {
      sprintf(str + 4, "%3s %02d", months[tm->tm_mon - 1], tm->tm_mday);
    }

    sprintf(str + 10, " %02d:%02d", tm->tm_hour, tm->tm_min);

       
                                                                      
                                                     
       
    if (fsec != 0)
    {
      sprintf(str + strlen(str), ":%02d.%06d", tm->tm_sec, fsec);
      TrimTrailingZeros(str);
    }
    else
    {
      sprintf(str + strlen(str), ":%02d", tm->tm_sec);
    }

    sprintf(str + strlen(str), " %04d", (tm->tm_year > 0) ? tm->tm_year : -(tm->tm_year - 1));
    if (tm->tm_year <= 0)
    {
      sprintf(str + strlen(str), " BC");
    }

    if (print_tz)
    {
      if (tzn)
      {
        sprintf(str + strlen(str), " %.*s", MAXTZLEN, tzn);
      }
      else
      {
           
                                                               
                                                                   
                                                                 
                                                           
           
        hour = -(tz / SECS_PER_HOUR);
        min = (abs(tz) / MINS_PER_HOUR) % MINS_PER_HOUR;
        if (min != 0)
        {
          sprintf(str + strlen(str), " %+03d:%02d", hour, min);
        }
        else
        {
          sprintf(str + strlen(str), " %+03d", hour);
        }
      }
    }
    break;
  }
}

int
GetEpochTime(struct tm *tm)
{
  struct tm *t0;
  time_t epoch = 0;

  t0 = gmtime(&epoch);

  if (t0)
  {
    tm->tm_year = t0->tm_year + 1900;
    tm->tm_mon = t0->tm_mon + 1;
    tm->tm_mday = t0->tm_mday;
    tm->tm_hour = t0->tm_hour;
    tm->tm_min = t0->tm_min;
    tm->tm_sec = t0->tm_sec;

    return 0;
  }

  return -1;
}                     

static void
abstime2tm(AbsoluteTime _time, int *tzp, struct tm *tm, char **tzn)
{
  time_t time = (time_t)_time;
  struct tm *tx;

  errno = 0;
  if (tzp != NULL)
  {
    tx = localtime((time_t *)&time);
  }
  else
  {
    tx = gmtime((time_t *)&time);
  }

  if (!tx)
  {
    errno = PGTYPES_TS_BAD_TIMESTAMP;
    return;
  }

  tm->tm_year = tx->tm_year + 1900;
  tm->tm_mon = tx->tm_mon + 1;
  tm->tm_mday = tx->tm_mday;
  tm->tm_hour = tx->tm_hour;
  tm->tm_min = tx->tm_min;
  tm->tm_sec = tx->tm_sec;
  tm->tm_isdst = tx->tm_isdst;

#if defined(HAVE_TM_ZONE)
  tm->tm_gmtoff = tx->tm_gmtoff;
  tm->tm_zone = tx->tm_zone;

  if (tzp != NULL)
  {
       
                                                                      
                                                              
       
    *tzp = -tm->tm_gmtoff;                               

       
                                                                       
       
    if (tzn != NULL)
    {
         
                                                                         
                                                                    
         
      StrNCpy(*tzn, tm->tm_zone, MAXTZLEN + 1);
      if (strlen(tm->tm_zone) > MAXTZLEN)
      {
        tm->tm_isdst = -1;
      }
    }
  }
  else
  {
    tm->tm_isdst = -1;
  }
#elif defined(HAVE_INT_TIMEZONE)
  if (tzp != NULL)
  {
    *tzp = (tm->tm_isdst > 0) ? TIMEZONE_GLOBAL - SECS_PER_HOUR : TIMEZONE_GLOBAL;

    if (tzn != NULL)
    {
         
                                                                         
                                                                    
         
      StrNCpy(*tzn, TZNAME_GLOBAL[tm->tm_isdst], MAXTZLEN + 1);
      if (strlen(TZNAME_GLOBAL[tm->tm_isdst]) > MAXTZLEN)
      {
        tm->tm_isdst = -1;
      }
    }
  }
  else
  {
    tm->tm_isdst = -1;
  }
#else                                              
  if (tzp != NULL)
  {
                        
    *tzp = 0;
    if (tzn != NULL)
    {
      *tzn = NULL;
    }
  }
  else
  {
    tm->tm_isdst = -1;
  }
#endif
}

void
GetCurrentDateTime(struct tm *tm)
{
  int tz;

  abstime2tm(time(NULL), &tz, tm, NULL);
}

void
dt2time(double jd, int *hour, int *min, int *sec, fsec_t *fsec)
{
  int64 time;

  time = jd;
  *hour = time / USECS_PER_HOUR;
  time -= (*hour) * USECS_PER_HOUR;
  *min = time / USECS_PER_MINUTE;
  time -= (*min) * USECS_PER_MINUTE;
  *sec = time / USECS_PER_SEC;
  *fsec = time - (*sec * USECS_PER_SEC);
}                

                       
                                                                  
                                                             
                       
   
static int
DecodeNumberField(int len, char *str, int fmask, int *tmask, struct tm *tm, fsec_t *fsec, bool *is2digits)
{
  char *cp;

     
                                                                           
              
     
  if ((cp = strchr(str, '.')) != NULL)
  {
    char fstr[7];
    int i;

    cp++;

       
                                                                       
                                                                           
                                 
       
                                                                       
                     
       
    for (i = 0; i < 6; i++)
    {
      fstr[i] = *cp != '\0' ? *cp++ : '0';
    }
    fstr[i] = '\0';
    *fsec = strtol(fstr, NULL, 10);
    *cp = '\0';
    len = strlen(str);
  }
                                                  
  else if ((fmask & DTK_DATE_M) != DTK_DATE_M)
  {
                   
    if (len == 8)
    {
      *tmask = DTK_DATE_M;

      tm->tm_mday = atoi(str + 6);
      *(str + 6) = '\0';
      tm->tm_mon = atoi(str + 4);
      *(str + 4) = '\0';
      tm->tm_year = atoi(str + 0);

      return DTK_DATE;
    }
                 
    else if (len == 6)
    {
      *tmask = DTK_DATE_M;
      tm->tm_mday = atoi(str + 4);
      *(str + 4) = '\0';
      tm->tm_mon = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_year = atoi(str + 0);
      *is2digits = true;

      return DTK_DATE;
    }
                
    else if (len == 5)
    {
      *tmask = DTK_DATE_M;
      tm->tm_mday = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_mon = 1;
      tm->tm_year = atoi(str + 0);
      *is2digits = true;

      return DTK_DATE;
    }
  }

                                          
  if ((fmask & DTK_TIME_M) != DTK_TIME_M)
  {
                
    if (len == 6)
    {
      *tmask = DTK_TIME_M;
      tm->tm_sec = atoi(str + 4);
      *(str + 4) = '\0';
      tm->tm_min = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_hour = atoi(str + 0);

      return DTK_TIME;
    }
               
    else if (len == 4)
    {
      *tmask = DTK_TIME_M;
      tm->tm_sec = 0;
      tm->tm_min = atoi(str + 2);
      *(str + 2) = '\0';
      tm->tm_hour = atoi(str + 0);

      return DTK_TIME;
    }
  }

  return -1;
}                          

                  
                                                             
   
static int
DecodeNumber(int flen, char *str, int fmask, int *tmask, struct tm *tm, fsec_t *fsec, bool *is2digits, bool EuroDates)
{
  int val;
  char *cp;

  *tmask = 0;

  val = strtol(str, &cp, 10);
  if (cp == str)
  {
    return -1;
  }

  if (*cp == '.')
  {
       
                                                                          
                                    
       
    if (cp - str > 2)
    {
      return DecodeNumberField(flen, str, (fmask | DTK_DATE_M), tmask, tm, fsec, is2digits);
    }

    *fsec = strtod(cp, &cp);
    if (*cp != '\0')
    {
      return -1;
    }
  }
  else if (*cp != '\0')
  {
    return -1;
  }

                                 
  if (flen == 3 && (fmask & DTK_M(YEAR)) && val >= 1 && val <= 366)
  {
    *tmask = (DTK_M(DOY) | DTK_M(MONTH) | DTK_M(DAY));
    tm->tm_yday = val;
    j2date(date2j(tm->tm_year, 1, 1) + tm->tm_yday - 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
  }

       
                                                                        
                                                                   
                                                            
                         
                                                             
                                            
       
  else if (flen >= 4)
  {
    *tmask = DTK_M(YEAR);

                                                               
    if ((fmask & DTK_M(YEAR)) && !(fmask & DTK_M(DAY)) && tm->tm_year >= 1 && tm->tm_year <= 31)
    {
      tm->tm_mday = tm->tm_year;
      *tmask = DTK_M(DAY);
    }

    tm->tm_year = val;
  }

                                              
  else if ((fmask & DTK_M(YEAR)) && !(fmask & DTK_M(MONTH)) && val >= 1 && val <= MONTHS_PER_YEAR)
  {
    *tmask = DTK_M(MONTH);
    tm->tm_mon = val;
  }
                                                        
  else if ((EuroDates || (fmask & DTK_M(MONTH))) && !(fmask & DTK_M(YEAR)) && !(fmask & DTK_M(DAY)) && val >= 1 && val <= 31)
  {
    *tmask = DTK_M(DAY);
    tm->tm_mday = val;
  }
  else if (!(fmask & DTK_M(MONTH)) && val >= 1 && val <= MONTHS_PER_YEAR)
  {
    *tmask = DTK_M(MONTH);
    tm->tm_mon = val;
  }
  else if (!(fmask & DTK_M(DAY)) && val >= 1 && val <= 31)
  {
    *tmask = DTK_M(DAY);
    tm->tm_mday = val;
  }

     
                                                                          
                                     
     
  else if (!(fmask & DTK_M(YEAR)) && (flen >= 4 || flen == 2))
  {
    *tmask = DTK_M(YEAR);
    tm->tm_year = val;

                                              
    *is2digits = (flen == 2);
  }
  else
  {
    return -1;
  }

  return 0;
}                     

                
                                                 
                                       
   
static int
DecodeDate(char *str, int fmask, int *tmask, struct tm *tm, bool EuroDates)
{
  fsec_t fsec;

  int nf = 0;
  int i, len;
  bool bc = false;
  bool is2digits = false;
  int type, val, dmask = 0;
  char *field[MAXDATEFIELDS];

                            
  while (*str != '\0' && nf < MAXDATEFIELDS)
  {
                               
    while (!isalnum((unsigned char)*str))
    {
      str++;
    }

    field[nf] = str;
    if (isdigit((unsigned char)*str))
    {
      while (isdigit((unsigned char)*str))
      {
        str++;
      }
    }
    else if (isalpha((unsigned char)*str))
    {
      while (isalpha((unsigned char)*str))
      {
        str++;
      }
    }

                                                                
    if (*str != '\0')
    {
      *str++ = '\0';
    }
    nf++;
  }

#if 0
	                                 
	if (nf > 3)
		return -1;
#endif

  *tmask = 0;

                                                                        
  for (i = 0; i < nf; i++)
  {
    if (isalpha((unsigned char)*field[i]))
    {
      type = DecodeSpecial(i, field[i], &val);
      if (type == IGNORE_DTF)
      {
        continue;
      }

      dmask = DTK_M(type);
      switch (type)
      {
      case MONTH:
        tm->tm_mon = val;
        break;

      case ADBC:
        bc = (val == BC);
        break;

      default:
        return -1;
      }
      if (fmask & dmask)
      {
        return -1;
      }

      fmask |= dmask;
      *tmask |= dmask;

                                              
      field[i] = NULL;
    }
  }

                                            
  for (i = 0; i < nf; i++)
  {
    if (field[i] == NULL)
    {
      continue;
    }

    if ((len = strlen(field[i])) <= 0)
    {
      return -1;
    }

    if (DecodeNumber(len, field[i], fmask, &dmask, tm, &fsec, &is2digits, EuroDates) != 0)
    {
      return -1;
    }

    if (fmask & dmask)
    {
      return -1;
    }

    fmask |= dmask;
    *tmask |= dmask;
  }

  if ((fmask & ~(DTK_M(DOY) | DTK_M(TZ))) != DTK_DATE_M)
  {
    return -1;
  }

                                                                      
  if (bc)
  {
    if (tm->tm_year > 0)
    {
      tm->tm_year = -(tm->tm_year - 1);
    }
    else
    {
      return -1;
    }
  }
  else if (is2digits)
  {
    if (tm->tm_year < 70)
    {
      tm->tm_year += 2000;
    }
    else if (tm->tm_year < 100)
    {
      tm->tm_year += 1900;
    }
  }

  return 0;
}                   

                
                                                 
                                                             
                                        
   
int
DecodeTime(char *str, int *tmask, struct tm *tm, fsec_t *fsec)
{
  char *cp;

  *tmask = DTK_TIME_M;

  tm->tm_hour = strtol(str, &cp, 10);
  if (*cp != ':')
  {
    return -1;
  }
  str = cp + 1;
  tm->tm_min = strtol(str, &cp, 10);
  if (*cp == '\0')
  {
    tm->tm_sec = 0;
    *fsec = 0;
  }
  else if (*cp != ':')
  {
    return -1;
  }
  else
  {
    str = cp + 1;
    tm->tm_sec = strtol(str, &cp, 10);
    if (*cp == '\0')
    {
      *fsec = 0;
    }
    else if (*cp == '.')
    {
      char fstr[7];
      int i;

      cp++;

         
                                                                         
                                                                         
                                       
         
                                                                         
                       
         
      for (i = 0; i < 6; i++)
      {
        fstr[i] = *cp != '\0' ? *cp++ : '0';
      }
      fstr[i] = '\0';
      *fsec = strtol(fstr, &cp, 10);
      if (*cp != '\0')
      {
        return -1;
      }
    }
    else
    {
      return -1;
    }
  }

                         
  if (tm->tm_hour < 0 || tm->tm_min < 0 || tm->tm_min > 59 || tm->tm_sec < 0 || tm->tm_sec > 59 || *fsec >= USECS_PER_SEC)
  {
    return -1;
  }

  return 0;
}                   

                    
                                           
   
                                                                       
                          
   
static int
DecodeTimezone(char *str, int *tzp)
{
  int tz;
  int hr, min;
  char *cp;
  int len;

                                              
  hr = strtol(str + 1, &cp, 10);

                           
  if (*cp == ':')
  {
    min = strtol(cp + 1, &cp, 10);
  }
                                                    
  else if (*cp == '\0' && (len = strlen(str)) > 3)
  {
    min = strtol(str + len - 2, &cp, 10);
    if (min < 0 || min >= 60)
    {
      return -1;
    }

    *(str + len - 2) = '\0';
    hr = strtol(str + 1, &cp, 10);
    if (hr < 0 || hr > 13)
    {
      return -1;
    }
  }
  else
  {
    min = 0;
  }

  tz = (hr * MINS_PER_HOUR + min) * SECS_PER_MINUTE;
  if (*str == '-')
  {
    tz = -tz;
  }

  *tzp = -tz;
  return *cp != '\0';
}                       

                         
                                                    
             
         
                       
   
static int
DecodePosixTimezone(char *str, int *tzp)
{
  int val, tz;
  int type;
  char *cp;
  char delim;

  cp = str;
  while (*cp != '\0' && isalpha((unsigned char)*cp))
  {
    cp++;
  }

  if (DecodeTimezone(cp, &tz) != 0)
  {
    return -1;
  }

  delim = *cp;
  *cp = '\0';
  type = DecodeSpecial(MAXDATEFIELDS - 1, str, &val);
  *cp = delim;

  switch (type)
  {
  case DTZ:
  case TZ:
    *tzp = -(val + tz);
    break;

  default:
    return -1;
  }

  return 0;
}                            

                   
                                                          
                                     
                                                      
                                                            
                                                                     
                                 
                                                     
                                                  
                                                         
                                            
                                                             
                                              
   
                                                                               
                                                                   
                                                                            
   
int
ParseDateTime(char *timestr, char *lowstr, char **field, int *ftype, int *numfields, char **endstr)
{
  int nf = 0;
  char *lp = lowstr;

  *endstr = timestr;
                                 
  while (*(*endstr) != '\0')
  {
                                       
    if (nf >= MAXDATEFIELDS)
    {
      return -1;
    }
    field[nf] = lp;

                                          
    if (isdigit((unsigned char)*(*endstr)))
    {
      *lp++ = *(*endstr)++;
      while (isdigit((unsigned char)*(*endstr)))
      {
        *lp++ = *(*endstr)++;
      }

                       
      if (*(*endstr) == ':')
      {
        ftype[nf] = DTK_TIME;
        *lp++ = *(*endstr)++;
        while (isdigit((unsigned char)*(*endstr)) || (*(*endstr) == ':') || (*(*endstr) == '.'))
        {
          *lp++ = *(*endstr)++;
        }
      }
                                                 
      else if (*(*endstr) == '-' || *(*endstr) == '/' || *(*endstr) == '.')
      {
                                                    
        char *dp = (*endstr);

        *lp++ = *(*endstr)++;
                                                                     
        if (isdigit((unsigned char)*(*endstr)))
        {
          ftype[nf] = (*dp == '.') ? DTK_NUMBER : DTK_DATE;
          while (isdigit((unsigned char)*(*endstr)))
          {
            *lp++ = *(*endstr)++;
          }

             
                                                                   
                   
             
          if (*(*endstr) == *dp)
          {
            ftype[nf] = DTK_DATE;
            *lp++ = *(*endstr)++;
            while (isdigit((unsigned char)*(*endstr)) || (*(*endstr) == *dp))
            {
              *lp++ = *(*endstr)++;
            }
          }
        }
        else
        {
          ftype[nf] = DTK_DATE;
          while (isalnum((unsigned char)*(*endstr)) || (*(*endstr) == *dp))
          {
            *lp++ = pg_tolower((unsigned char)*(*endstr)++);
          }
        }
      }

         
                                                                        
                                      
         
      else
      {
        ftype[nf] = DTK_NUMBER;
      }
    }
                                                           
    else if (*(*endstr) == '.')
    {
      *lp++ = *(*endstr)++;
      while (isdigit((unsigned char)*(*endstr)))
      {
        *lp++ = *(*endstr)++;
      }

      ftype[nf] = DTK_NUMBER;
    }

       
                                                                        
       
    else if (isalpha((unsigned char)*(*endstr)))
    {
      ftype[nf] = DTK_STRING;
      *lp++ = pg_tolower((unsigned char)*(*endstr)++);
      while (isalpha((unsigned char)*(*endstr)))
      {
        *lp++ = pg_tolower((unsigned char)*(*endstr)++);
      }

         
                                                                         
                      
         
      if (*(*endstr) == '-' || *(*endstr) == '/' || *(*endstr) == '.')
      {
        char *dp = (*endstr);

        ftype[nf] = DTK_DATE;
        *lp++ = *(*endstr)++;
        while (isdigit((unsigned char)*(*endstr)) || *(*endstr) == *dp)
        {
          *lp++ = *(*endstr)++;
        }
      }
    }
                             
    else if (isspace((unsigned char)*(*endstr)))
    {
      (*endstr)++;
      continue;
    }
                                                
    else if (*(*endstr) == '+' || *(*endstr) == '-')
    {
      *lp++ = *(*endstr)++;
                                      
      while (isspace((unsigned char)*(*endstr)))
      {
        (*endstr)++;
      }
                             
      if (isdigit((unsigned char)*(*endstr)))
      {
        ftype[nf] = DTK_TZ;
        *lp++ = *(*endstr)++;
        while (isdigit((unsigned char)*(*endstr)) || (*(*endstr) == ':') || (*(*endstr) == '.'))
        {
          *lp++ = *(*endstr)++;
        }
      }
                    
      else if (isalpha((unsigned char)*(*endstr)))
      {
        ftype[nf] = DTK_SPECIAL;
        *lp++ = pg_tolower((unsigned char)*(*endstr)++);
        while (isalpha((unsigned char)*(*endstr)))
        {
          *lp++ = pg_tolower((unsigned char)*(*endstr)++);
        }
      }
                                        
      else
      {
        return -1;
      }
    }
                                                 
    else if (ispunct((unsigned char)*(*endstr)))
    {
      (*endstr)++;
      continue;
    }
                                              
    else
    {
      return -1;
    }

                                               
    *lp++ = '\0';
    nf++;
  }

  *numfields = nf;

  return 0;
}                      

                    
                                                                 
                                                              
                        
                                                                
                                
                            
                          
                          
                                               
                                         
                      
                     
                               
   
                                                                  
                                         
                                                                  
                                                  
   
int
DecodeDateTime(char **field, int *ftype, int nf, int *dtype, struct tm *tm, fsec_t *fsec, bool EuroDates)
{
  int fmask = 0, tmask, type;
  int ptype = 0;                                               
  int i;
  int val;
  int mer = HR24;
  bool haveTextMonth = false;
  bool is2digits = false;
  bool bc = false;
  int t = 0;
  int *tzp = &t;

       
                                                                         
                                                        
       
  *dtype = DTK_DATE;
  tm->tm_hour = 0;
  tm->tm_min = 0;
  tm->tm_sec = 0;
  *fsec = 0;
                                                       
  tm->tm_isdst = -1;
  if (tzp != NULL)
  {
    *tzp = 0;
  }

  for (i = 0; i < nf; i++)
  {
    switch (ftype[i])
    {
    case DTK_DATE:
           
                                                      
                                                        
                                                            
           
      if (ptype == DTK_JULIAN)
      {
        char *cp;
        int val;

        if (tzp == NULL)
        {
          return -1;
        }

        val = strtol(field[i], &cp, 10);
        if (*cp != '-')
        {
          return -1;
        }

        j2date(val, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
                                                          
        if (DecodeTimezone(cp, tzp) != 0)
        {
          return -1;
        }

        tmask = DTK_DATE_M | DTK_TIME_M | DTK_M(TZ);
        ptype = 0;
        break;
      }
           
                                                              
                                                               
                                                                       
                             
           
      else if (((fmask & DTK_DATE_M) == DTK_DATE_M) || (ptype != 0))
      {
                                                 
        if (tzp == NULL)
        {
          return -1;
        }

        if (isdigit((unsigned char)*field[i]) || ptype != 0)
        {
          char *cp;

          if (ptype != 0)
          {
                                                         
            if (ptype != DTK_TIME)
            {
              return -1;
            }
            ptype = 0;
          }

             
                                                            
                                                                
                        
             
          if ((fmask & DTK_TIME_M) == DTK_TIME_M)
          {
            return -1;
          }

          if ((cp = strchr(field[i], '-')) == NULL)
          {
            return -1;
          }

                                                            
          if (DecodeTimezone(cp, tzp) != 0)
          {
            return -1;
          }
          *cp = '\0';

             
                                                               
                  
             
          if ((ftype[i] = DecodeNumberField(strlen(field[i]), field[i], fmask, &tmask, tm, fsec, &is2digits)) < 0)
          {
            return -1;
          }

             
                                               
                                 
             
          tmask |= DTK_M(TZ);
        }
        else
        {
          if (DecodePosixTimezone(field[i], tzp) != 0)
          {
            return -1;
          }

          ftype[i] = DTK_TZ;
          tmask = DTK_M(TZ);
        }
      }
      else if (DecodeDate(field[i], fmask, &tmask, tm, EuroDates) != 0)
      {
        return -1;
      }
      break;

    case DTK_TIME:
      if (DecodeTime(field[i], &tmask, tm, fsec) != 0)
      {
        return -1;
      }

         
                                                             
                      
         
                               
      if (tm->tm_hour > 24 || (tm->tm_hour == 24 && (tm->tm_min > 0 || tm->tm_sec > 0)))
      {
        return -1;
      }
      break;

    case DTK_TZ:
    {
      int tz;

      if (tzp == NULL)
      {
        return -1;
      }

      if (DecodeTimezone(field[i], &tz) != 0)
      {
        return -1;
      }

         
                                                                 
                                                          
         
      if (i > 0 && (fmask & DTK_M(TZ)) != 0 && ftype[i - 1] == DTK_TZ && isalpha((unsigned char)*field[i - 1]))
      {
        *tzp -= tz;
        tmask = 0;
      }
      else
      {
        *tzp = tz;
        tmask = DTK_M(TZ);
      }
    }
    break;

    case DTK_NUMBER:

         
                                                               
                                                      
         
      if (ptype != 0)
      {
        char *cp;
        int val;

        val = strtol(field[i], &cp, 10);

           
                                                            
                   
           
        if (*cp == '.')
        {
          switch (ptype)
          {
          case DTK_JULIAN:
          case DTK_TIME:
          case DTK_SECOND:
            break;
          default:
            return 1;
            break;
          }
        }
        else if (*cp != '\0')
        {
          return -1;
        }

        switch (ptype)
        {
        case DTK_YEAR:
          tm->tm_year = val;
          tmask = DTK_M(YEAR);
          break;

        case DTK_MONTH:

             
                                                        
                     
             
          if ((fmask & DTK_M(MONTH)) != 0 && (fmask & DTK_M(HOUR)) != 0)
          {
            tm->tm_min = val;
            tmask = DTK_M(MINUTE);
          }
          else
          {
            tm->tm_mon = val;
            tmask = DTK_M(MONTH);
          }
          break;

        case DTK_DAY:
          tm->tm_mday = val;
          tmask = DTK_M(DAY);
          break;

        case DTK_HOUR:
          tm->tm_hour = val;
          tmask = DTK_M(HOUR);
          break;

        case DTK_MINUTE:
          tm->tm_min = val;
          tmask = DTK_M(MINUTE);
          break;

        case DTK_SECOND:
          tm->tm_sec = val;
          tmask = DTK_M(SECOND);
          if (*cp == '.')
          {
            double frac;

            frac = strtod(cp, &cp);
            if (*cp != '\0')
            {
              return -1;
            }
            *fsec = frac * 1000000;
          }
          break;

        case DTK_TZ:
          tmask = DTK_M(TZ);
          if (DecodeTimezone(field[i], tzp) != 0)
          {
            return -1;
          }
          break;

        case DTK_JULIAN:
               
                                                           
               
          tmask = DTK_DATE_M;
          j2date(val, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
                                      
          if (*cp == '.')
          {
            double time;

            time = strtod(cp, &cp);
            if (*cp != '\0')
            {
              return -1;
            }

            tmask |= DTK_TIME_M;
            dt2time((time * USECS_PER_DAY), &tm->tm_hour, &tm->tm_min, &tm->tm_sec, fsec);
          }
          break;

        case DTK_TIME:
                                                   
          if ((ftype[i] = DecodeNumberField(strlen(field[i]), field[i], (fmask | DTK_DATE_M), &tmask, tm, fsec, &is2digits)) < 0)
          {
            return -1;
          }

          if (tmask != DTK_TIME_M)
          {
            return -1;
          }
          break;

        default:
          return -1;
          break;
        }

        ptype = 0;
        *dtype = DTK_DATE;
      }
      else
      {
        char *cp;
        int flen;

        flen = strlen(field[i]);
        cp = strchr(field[i], '.');

                                               
        if (cp != NULL && !(fmask & DTK_DATE_M))
        {
          if (DecodeDate(field[i], fmask, &tmask, tm, EuroDates) != 0)
          {
            return -1;
          }
        }
                                                         
        else if (cp != NULL && flen - strlen(cp) > 2)
        {
             
                                                              
                                                              
                                         
             
          if ((ftype[i] = DecodeNumberField(flen, field[i], fmask, &tmask, tm, fsec, &is2digits)) < 0)
          {
            return -1;
          }
        }
        else if (flen > 4)
        {
          if ((ftype[i] = DecodeNumberField(flen, field[i], fmask, &tmask, tm, fsec, &is2digits)) < 0)
          {
            return -1;
          }
        }
                                                         
        else if (DecodeNumber(flen, field[i], fmask, &tmask, tm, fsec, &is2digits, EuroDates) != 0)
        {
          return -1;
        }
      }
      break;

    case DTK_STRING:
    case DTK_SPECIAL:
      type = DecodeSpecial(i, field[i], &val);
      if (type == IGNORE_DTF)
      {
        continue;
      }

      tmask = DTK_M(type);
      switch (type)
      {
      case RESERV:
        switch (val)
        {
        case DTK_NOW:
          tmask = (DTK_DATE_M | DTK_TIME_M | DTK_M(TZ));
          *dtype = DTK_DATE;
          GetCurrentDateTime(tm);
          break;

        case DTK_YESTERDAY:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(tm);
          j2date(date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) - 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          break;

        case DTK_TODAY:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(tm);
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          break;

        case DTK_TOMORROW:
          tmask = DTK_DATE_M;
          *dtype = DTK_DATE;
          GetCurrentDateTime(tm);
          j2date(date2j(tm->tm_year, tm->tm_mon, tm->tm_mday) + 1, &tm->tm_year, &tm->tm_mon, &tm->tm_mday);
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          break;

        case DTK_ZULU:
          tmask = (DTK_TIME_M | DTK_M(TZ));
          *dtype = DTK_DATE;
          tm->tm_hour = 0;
          tm->tm_min = 0;
          tm->tm_sec = 0;
          if (tzp != NULL)
          {
            *tzp = 0;
          }
          break;

        default:
          *dtype = val;
        }

        break;

      case MONTH:

           
                                                              
                         
           
        if ((fmask & DTK_M(MONTH)) && !haveTextMonth && !(fmask & DTK_M(DAY)) && tm->tm_mon >= 1 && tm->tm_mon <= 31)
        {
          tm->tm_mday = tm->tm_mon;
          tmask = DTK_M(DAY);
        }
        haveTextMonth = true;
        tm->tm_mon = val;
        break;

      case DTZMOD:

           
                                                            
                   
           
        tmask |= DTK_M(DTZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return -1;
        }
        *tzp -= val;
        break;

      case DTZ:

           
                                                              
                                    
           
        tmask |= DTK_M(TZ);
        tm->tm_isdst = 1;
        if (tzp == NULL)
        {
          return -1;
        }
        *tzp = -val;
        ftype[i] = DTK_TZ;
        break;

      case TZ:
        tm->tm_isdst = 0;
        if (tzp == NULL)
        {
          return -1;
        }
        *tzp = -val;
        ftype[i] = DTK_TZ;
        break;

      case IGNORE_DTF:
        break;

      case AMPM:
        mer = val;
        break;

      case ADBC:
        bc = (val == BC);
        break;

      case DOW:
        tm->tm_wday = val;
        break;

      case UNITS:
        tmask = 0;
        ptype = val;
        break;

      case ISOTIME:

           
                                                               
                                                               
           
        tmask = 0;

                                             
        if ((fmask & DTK_DATE_M) != DTK_DATE_M)
        {
          return -1;
        }

             
                                                     
                                           
                                           
                                        
             
        if (i >= nf - 1 || (ftype[i + 1] != DTK_NUMBER && ftype[i + 1] != DTK_TIME && ftype[i + 1] != DTK_DATE))
        {
          return -1;
        }

        ptype = val;
        break;

      default:
        return -1;
      }
      break;

    default:
      return -1;
    }

    if (tmask & fmask)
    {
      return -1;
    }
    fmask |= tmask;
  }

                                                                      
  if (bc)
  {
    if (tm->tm_year > 0)
    {
      tm->tm_year = -(tm->tm_year - 1);
    }
    else
    {
      return -1;
    }
  }
  else if (is2digits)
  {
    if (tm->tm_year < 70)
    {
      tm->tm_year += 2000;
    }
    else if (tm->tm_year < 100)
    {
      tm->tm_year += 1900;
    }
  }

  if (mer != HR24 && tm->tm_hour > 12)
  {
    return -1;
  }
  if (mer == AM && tm->tm_hour == 12)
  {
    tm->tm_hour = 0;
  }
  else if (mer == PM && tm->tm_hour != 12)
  {
    tm->tm_hour += 12;
  }

                                                     
  if (*dtype == DTK_DATE)
  {
    if ((fmask & DTK_DATE_M) != DTK_DATE_M)
    {
      return ((fmask & DTK_TIME_M) == DTK_TIME_M) ? 1 : -1;
    }

       
                                                                         
                   
       
    if (tm->tm_mday < 1 || tm->tm_mday > day_tab[isleap(tm->tm_year)][tm->tm_mon - 1])
    {
      return -1;
    }

       
                                                                      
                                                                          
                                                       
       
    if ((fmask & DTK_DATE_M) == DTK_DATE_M && tzp != NULL && !(fmask & DTK_M(TZ)) && (fmask & DTK_M(DTZMOD)))
    {
      return -1;
    }
  }

  return 0;
}                       

                              
   
   
     

static char *
find_end_token(char *str, char *fmt)
{
     
                                                                     
     
                                                                            
                                                                         
                    
     
                                                                            
                                                                          
                                                                          
                                                                        
     
                                                                   
                
     
  char *end_position = NULL;
  char *next_percent, *subst_location = NULL;
  int scan_offset = 0;
  char last_char;

                          
  if (!*fmt)
  {
    end_position = fmt;
    return end_position;
  }

                      
  while (fmt[scan_offset] == '%' && fmt[scan_offset + 1])
  {
       
                                                                          
                                                                           
                                                                       
                    
       
    scan_offset += 2;
  }
  next_percent = strchr(fmt + scan_offset, '%');
  if (next_percent)
  {
       
                                                                         
                                                                          
                                                                           
                                        
       

    subst_location = next_percent;
    while (*(subst_location - 1) == ' ' && subst_location - 1 > fmt + scan_offset)
    {
      subst_location--;
    }
    last_char = *subst_location;
    *subst_location = '\0';

       
                                                                         
                                                                 
       

       
                                                                        
                                                                     
                                                                          
                                                                       
            
       
    while (*str == ' ')
    {
      str++;
    }
    end_position = strstr(str, fmt + scan_offset);
    *subst_location = last_char;
  }
  else
  {
       
                                                                          
              
       
    end_position = str + strlen(str);
  }
  if (!end_position)
  {
       
                                         
       
                                       
       
                                   
       
                                
       
                                                                        
       
                                                                        
                                                     
       
    if ((fmt + scan_offset)[0] == ' ' && fmt + scan_offset + 1 == subst_location)
    {
      end_position = str + strlen(str);
    }
  }
  return end_position;
}

static int
pgtypes_defmt_scan(union un_fmt_comb *scan_val, int scan_type, char **pstr, char *pfmt)
{
     
                                                                          
                                                               
     

  char last_char;
  int err = 0;
  char *pstr_end;
  char *strtol_end = NULL;

  while (**pstr == ' ')
  {
    pstr++;
  }
  pstr_end = find_end_token(*pstr, pfmt);
  if (!pstr_end)
  {
                                      
    return 1;
  }
  last_char = *pstr_end;
  *pstr_end = '\0';

  switch (scan_type)
  {
  case PGTYPES_TYPE_UINT:

       
                                                                    
                                
       
    while (**pstr == ' ')
    {
      (*pstr)++;
    }
    errno = 0;
    scan_val->uint_val = (unsigned int)strtol(*pstr, &strtol_end, 10);
    if (errno)
    {
      err = 1;
    }
    break;
  case PGTYPES_TYPE_UINT_LONG:
    while (**pstr == ' ')
    {
      (*pstr)++;
    }
    errno = 0;
    scan_val->luint_val = (unsigned long int)strtol(*pstr, &strtol_end, 10);
    if (errno)
    {
      err = 1;
    }
    break;
  case PGTYPES_TYPE_STRING_MALLOCED:
    scan_val->str_val = pgtypes_strdup(*pstr);
    if (scan_val->str_val == NULL)
    {
      err = 1;
    }
    break;
  }
  if (strtol_end && *strtol_end)
  {
    *pstr = strtol_end;
  }
  else
  {
    *pstr = pstr_end;
  }
  *pstr_end = last_char;
  return err;
}

                        
int
PGTYPEStimestamp_defmt_scan(char **str, char *fmt, timestamp *d, int *year, int *month, int *day, int *hour, int *minute, int *second, int *tz)
{
  union un_fmt_comb scan_val;
  int scan_type;

  char *pstr, *pfmt, *tmp;
  int err = 1;
  unsigned int j;
  struct tm tm;

  pfmt = fmt;
  pstr = *str;

  while (*pfmt)
  {
    err = 0;
    while (*pfmt == ' ')
    {
      pfmt++;
    }
    while (*pstr == ' ')
    {
      pstr++;
    }
    if (*pfmt != '%')
    {
      if (*pfmt == *pstr)
      {
        pfmt++;
        pstr++;
      }
      else
      {
                             
        err = 1;
        return err;
      }
      continue;
    }
                               
    pfmt++;
    switch (*pfmt)
    {
    case 'a':
      pfmt++;

         
                                                                    
                                                       
         
      err = 1;
      j = 0;
      while (pgtypes_date_weekdays_short[j])
      {
        if (strncmp(pgtypes_date_weekdays_short[j], pstr, strlen(pgtypes_date_weekdays_short[j])) == 0)
        {
                        
          err = 0;
          pstr += strlen(pgtypes_date_weekdays_short[j]);
          break;
        }
        j++;
      }
      break;
    case 'A':
                          
      pfmt++;
      err = 1;
      j = 0;
      while (days[j])
      {
        if (strncmp(days[j], pstr, strlen(days[j])) == 0)
        {
                        
          err = 0;
          pstr += strlen(days[j]);
          break;
        }
        j++;
      }
      break;
    case 'b':
    case 'h':
      pfmt++;
      err = 1;
      j = 0;
      while (months[j])
      {
        if (strncmp(months[j], pstr, strlen(months[j])) == 0)
        {
                        
          err = 0;
          pstr += strlen(months[j]);
          *month = j + 1;
          break;
        }
        j++;
      }
      break;
    case 'B':
                          
      pfmt++;
      err = 1;
      j = 0;
      while (pgtypes_date_months[j])
      {
        if (strncmp(pgtypes_date_months[j], pstr, strlen(pgtypes_date_months[j])) == 0)
        {
                        
          err = 0;
          pstr += strlen(pgtypes_date_months[j]);
          *month = j + 1;
          break;
        }
        j++;
      }
      break;
    case 'c':
               
      break;
    case 'C':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *year = scan_val.uint_val * 100;
      break;
    case 'd':
    case 'e':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *day = scan_val.uint_val;
      break;
    case 'D':

         
                                                                   
                                          
         
      pfmt++;
      tmp = pgtypes_alloc(strlen("%m/%d/%y") + strlen(pstr) + 1);
      strcpy(tmp, "%m/%d/%y");
      strcat(tmp, pfmt);
      err = PGTYPEStimestamp_defmt_scan(&pstr, tmp, d, year, month, day, hour, minute, second, tz);
      free(tmp);
      return err;
    case 'm':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *month = scan_val.uint_val;
      break;
    case 'y':
    case 'g':                                
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (*year < 0)
      {
                         
        *year = scan_val.uint_val;
      }
      else
      {
        *year += scan_val.uint_val;
      }
      if (*year < 100)
      {
        *year += 1900;
      }
      break;
    case 'G':
                                      
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *year = scan_val.uint_val;
      break;
    case 'H':
    case 'I':
    case 'k':
    case 'l':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *hour += scan_val.uint_val;
      break;
    case 'j':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);

         
                                                                 
                                                                    
                                         
         
      break;
    case 'M':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *minute = scan_val.uint_val;
      break;
    case 'n':
      pfmt++;
      if (*pstr == '\n')
      {
        pstr++;
      }
      else
      {
        err = 1;
      }
      break;
    case 'p':
      err = 1;
      pfmt++;
      if (strncmp(pstr, "am", 2) == 0)
      {
        *hour += 0;
        err = 0;
        pstr += 2;
      }
      if (strncmp(pstr, "a.m.", 4) == 0)
      {
        *hour += 0;
        err = 0;
        pstr += 4;
      }
      if (strncmp(pstr, "pm", 2) == 0)
      {
        *hour += 12;
        err = 0;
        pstr += 2;
      }
      if (strncmp(pstr, "p.m.", 4) == 0)
      {
        *hour += 12;
        err = 0;
        pstr += 4;
      }
      break;
    case 'P':
      err = 1;
      pfmt++;
      if (strncmp(pstr, "AM", 2) == 0)
      {
        *hour += 0;
        err = 0;
        pstr += 2;
      }
      if (strncmp(pstr, "A.M.", 4) == 0)
      {
        *hour += 0;
        err = 0;
        pstr += 4;
      }
      if (strncmp(pstr, "PM", 2) == 0)
      {
        *hour += 12;
        err = 0;
        pstr += 2;
      }
      if (strncmp(pstr, "P.M.", 4) == 0)
      {
        *hour += 12;
        err = 0;
        pstr += 4;
      }
      break;
    case 'r':
      pfmt++;
      tmp = pgtypes_alloc(strlen("%I:%M:%S %p") + strlen(pstr) + 1);
      strcpy(tmp, "%I:%M:%S %p");
      strcat(tmp, pfmt);
      err = PGTYPEStimestamp_defmt_scan(&pstr, tmp, d, year, month, day, hour, minute, second, tz);
      free(tmp);
      return err;
    case 'R':
      pfmt++;
      tmp = pgtypes_alloc(strlen("%H:%M") + strlen(pstr) + 1);
      strcpy(tmp, "%H:%M");
      strcat(tmp, pfmt);
      err = PGTYPEStimestamp_defmt_scan(&pstr, tmp, d, year, month, day, hour, minute, second, tz);
      free(tmp);
      return err;
    case 's':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT_LONG;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
                                                   
      {
        struct tm *tms;
        time_t et = (time_t)scan_val.luint_val;

        tms = gmtime(&et);

        if (tms)
        {
          *year = tms->tm_year + 1900;
          *month = tms->tm_mon + 1;
          *day = tms->tm_mday;
          *hour = tms->tm_hour;
          *minute = tms->tm_min;
          *second = tms->tm_sec;
        }
        else
        {
          err = 1;
        }
      }
      break;
    case 'S':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *second = scan_val.uint_val;
      break;
    case 't':
      pfmt++;
      if (*pstr == '\t')
      {
        pstr++;
      }
      else
      {
        err = 1;
      }
      break;
    case 'T':
      pfmt++;
      tmp = pgtypes_alloc(strlen("%H:%M:%S") + strlen(pstr) + 1);
      strcpy(tmp, "%H:%M:%S");
      strcat(tmp, pfmt);
      err = PGTYPEStimestamp_defmt_scan(&pstr, tmp, d, year, month, day, hour, minute, second, tz);
      free(tmp);
      return err;
    case 'u':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (scan_val.uint_val < 1 || scan_val.uint_val > 7)
      {
        err = 1;
      }
      break;
    case 'U':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (scan_val.uint_val > 53)
      {
        err = 1;
      }
      break;
    case 'V':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (scan_val.uint_val < 1 || scan_val.uint_val > 53)
      {
        err = 1;
      }
      break;
    case 'w':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (scan_val.uint_val > 6)
      {
        err = 1;
      }
      break;
    case 'W':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (scan_val.uint_val > 53)
      {
        err = 1;
      }
      break;
    case 'x':
    case 'X':
               
      break;
    case 'Y':
      pfmt++;
      scan_type = PGTYPES_TYPE_UINT;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      *year = scan_val.uint_val;
      break;
    case 'z':
      pfmt++;
      scan_type = PGTYPES_TYPE_STRING_MALLOCED;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (!err)
      {
        err = DecodeTimezone(scan_val.str_val, tz);
        free(scan_val.str_val);
      }
      break;
    case 'Z':
      pfmt++;
      scan_type = PGTYPES_TYPE_STRING_MALLOCED;
      err = pgtypes_defmt_scan(&scan_val, scan_type, &pstr, pfmt);
      if (!err)
      {
           
                                                                 
                 
           
        err = 1;
        for (j = 0; j < szdatetktbl; j++)
        {
          if ((datetktbl[j].type == TZ || datetktbl[j].type == DTZ) && pg_strcasecmp(datetktbl[j].token, scan_val.str_val) == 0)
          {
            *tz = -datetktbl[j].value;
            err = 0;
            break;
          }
        }
        free(scan_val.str_val);
      }
      break;
    case '+':
               
      break;
    case '%':
      pfmt++;
      if (*pstr == '%')
      {
        pstr++;
      }
      else
      {
        err = 1;
      }
      break;
    default:
      err = 1;
    }
  }
  if (!err)
  {
    if (*second < 0)
    {
      *second = 0;
    }
    if (*minute < 0)
    {
      *minute = 0;
    }
    if (*hour < 0)
    {
      *hour = 0;
    }
    if (*day < 0)
    {
      err = 1;
      *day = 1;
    }
    if (*month < 0)
    {
      err = 1;
      *month = 1;
    }
    if (*year < 0)
    {
      err = 1;
      *year = 1970;
    }

    if (*second > 59)
    {
      err = 1;
      *second = 0;
    }
    if (*minute > 59)
    {
      err = 1;
      *minute = 0;
    }
    if (*hour > 24 ||                          
        (*hour == 24 && (*minute > 0 || *second > 0)))
    {
      err = 1;
      *hour = 0;
    }
    if (*month > MONTHS_PER_YEAR)
    {
      err = 1;
      *month = 1;
    }
    if (*day > day_tab[isleap(*year)][*month - 1])
    {
      *day = day_tab[isleap(*year)][*month - 1];
      err = 1;
    }

    tm.tm_sec = *second;
    tm.tm_min = *minute;
    tm.tm_hour = *hour;
    tm.tm_mday = *day;
    tm.tm_mon = *month;
    tm.tm_year = *year;

    tm2timestamp(&tm, 0, tz, d);
  }
  return err;
}

                                                    
