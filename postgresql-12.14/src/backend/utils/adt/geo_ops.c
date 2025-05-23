                                                                            
   
             
                             
   
                                                                      
                                                          
   
           
          
                  
         
            
             
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <math.h>
#include <limits.h>
#include <float.h>
#include <ctype.h>

#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/float.h"
#include "utils/fmgrprotos.h"
#include "utils/geo_decls.h"

   
                                       
                                             
   
                                                
                                                                        
   
                         
                         
                                                                        
                                                                      
                                         
   
                  
                                                         
                                       
                                                                          
                                                                     
   
                                                      
                                                                          
                                                                          
                                                                
   
                                                                               
                                                                           
                                 
   

   
                     
   

enum path_delim
{
  PATH_NONE,
  PATH_OPEN,
  PATH_CLOSED
};

                         
static inline void
point_construct(Point *result, float8 x, float8 y);
static inline void
point_add_point(Point *result, Point *pt1, Point *pt2);
static inline void
point_sub_point(Point *result, Point *pt1, Point *pt2);
static inline void
point_mul_point(Point *result, Point *pt1, Point *pt2);
static inline void
point_div_point(Point *result, Point *pt1, Point *pt2);
static inline bool
point_eq_point(Point *pt1, Point *pt2);
static inline float8
point_dt(Point *pt1, Point *pt2);
static inline float8
point_sl(Point *pt1, Point *pt2);
static int
point_inside(Point *p, int npts, Point *plist);

                        
static inline void
line_construct(LINE *result, Point *pt, float8 m);
static inline float8
line_sl(LINE *line);
static inline float8
line_invsl(LINE *line);
static bool
line_interpt_line(Point *result, LINE *l1, LINE *l2);
static bool
line_contain_point(LINE *line, Point *point);
static float8
line_closept_point(Point *result, LINE *line, Point *pt);

                                
static inline void
statlseg_construct(LSEG *lseg, Point *pt1, Point *pt2);
static inline float8
lseg_sl(LSEG *lseg);
static inline float8
lseg_invsl(LSEG *lseg);
static bool
lseg_interpt_line(Point *result, LSEG *lseg, LINE *line);
static bool
lseg_interpt_lseg(Point *result, LSEG *l1, LSEG *l2);
static int
lseg_crossing(float8 x, float8 y, float8 px, float8 py);
static bool
lseg_contain_point(LSEG *lseg, Point *point);
static float8
lseg_closept_point(Point *result, LSEG *lseg, Point *pt);
static float8
lseg_closept_line(Point *result, LSEG *lseg, LINE *line);
static float8
lseg_closept_lseg(Point *result, LSEG *on_lseg, LSEG *to_lseg);

                        
static inline void
box_construct(BOX *result, Point *pt1, Point *pt2);
static void
box_cn(Point *center, BOX *box);
static bool
box_ov(BOX *box1, BOX *box2);
static float8
box_ar(BOX *box);
static float8
box_ht(BOX *box);
static float8
box_wd(BOX *box);
static bool
box_contain_point(BOX *box, Point *point);
static bool
box_contain_box(BOX *contains_box, BOX *contained_box);
static bool
box_contain_lseg(BOX *box, LSEG *lseg);
static bool
box_interpt_lseg(Point *result, BOX *box, LSEG *lseg);
static float8
box_closept_point(Point *result, BOX *box, Point *point);
static float8
box_closept_lseg(Point *result, BOX *box, LSEG *lseg);

                          
static float8
circle_ar(CIRCLE *circle);

                           
static void
make_bound_box(POLYGON *poly);
static void
poly_to_circle(CIRCLE *result, POLYGON *poly);
static bool
lseg_inside_poly(Point *a, Point *b, POLYGON *poly, int start);
static bool
poly_contain_poly(POLYGON *contains_poly, POLYGON *contained_poly);
static bool
plist_same(int npts, Point *p1, Point *p2);
static float8
dist_ppoly_internal(Point *pt, POLYGON *poly);

                                        
static float8
single_decode(char *num, char **endptr_p, const char *type_name, const char *orig_string);
static void
single_encode(float8 x, StringInfo str);
static void
pair_decode(char *str, float8 *x, float8 *y, char **endptr_p, const char *type_name, const char *orig_string);
static void
pair_encode(float8 x, float8 y, StringInfo str);
static int
pair_count(char *s, char delim);
static void
path_decode(char *str, bool opentype, int npts, Point *p, bool *isopen, char **endptr_p, const char *type_name, const char *orig_string);
static char *
path_encode(enum path_delim path_delim, int npts, Point *pt);

   
                                            
                                                                                      
                                                                                
   

#define LDELIM '('
#define RDELIM ')'
#define DELIM ','
#define LDELIM_EP '['
#define RDELIM_EP ']'
#define LDELIM_C '<'
#define RDELIM_C '>'
#define LDELIM_L '{'
#define RDELIM_L '}'

   
                                                
                                                                         
                                                                 
                                                                     
                                                                             
                                   
   
                                      
                   
                                    
                           
                                      
                                       
                                    
   
                                                                                     
                                                                          
                                          
   
                                                                       
                                                                             
                                                         
   

static float8
single_decode(char *num, char **endptr_p, const char *type_name, const char *orig_string)
{
  return float8in_internal(num, endptr_p, type_name, orig_string);
}                      

static void
single_encode(float8 x, StringInfo str)
{
  char *xstr = float8out_internal(x);

  appendStringInfoString(str, xstr);
  pfree(xstr);
}                      

static void
pair_decode(char *str, float8 *x, float8 *y, char **endptr_p, const char *type_name, const char *orig_string)
{
  bool has_delim;

  while (isspace((unsigned char)*str))
  {
    str++;
  }
  if ((has_delim = (*str == LDELIM)))
  {
    str++;
  }

  *x = float8in_internal(str, &str, type_name, orig_string);

  if (*str++ != DELIM)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
  }

  *y = float8in_internal(str, &str, type_name, orig_string);

  if (has_delim)
  {
    if (*str++ != RDELIM)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
    }
    while (isspace((unsigned char)*str))
    {
      str++;
    }
  }

                                                                           
  if (endptr_p)
  {
    *endptr_p = str;
  }
  else if (*str != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
  }
}

static void
pair_encode(float8 x, float8 y, StringInfo str)
{
  char *xstr = float8out_internal(x);
  char *ystr = float8out_internal(y);

  appendStringInfo(str, "%s,%s", xstr, ystr);
  pfree(xstr);
  pfree(ystr);
}

static void
path_decode(char *str, bool opentype, int npts, Point *p, bool *isopen, char **endptr_p, const char *type_name, const char *orig_string)
{
  int depth = 0;
  char *cp;
  int i;

  while (isspace((unsigned char)*str))
  {
    str++;
  }
  if ((*isopen = (*str == LDELIM_EP)))
  {
                                    
    if (!opentype)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
    }
    depth++;
    str++;
  }
  else if (*str == LDELIM)
  {
    cp = (str + 1);
    while (isspace((unsigned char)*cp))
    {
      cp++;
    }
    if (*cp == LDELIM)
    {
      depth++;
      str = cp;
    }
    else if (strrchr(str, LDELIM) == str)
    {
      depth++;
      str = cp;
    }
  }

  for (i = 0; i < npts; i++)
  {
    pair_decode(str, &(p->x), &(p->y), &str, type_name, orig_string);
    if (*str == DELIM)
    {
      str++;
    }
    p++;
  }

  while (depth > 0)
  {
    if (*str == RDELIM || (*str == RDELIM_EP && *isopen && depth == 1))
    {
      depth--;
      str++;
      while (isspace((unsigned char)*str))
      {
        str++;
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
    }
  }

                                                                           
  if (endptr_p)
  {
    *endptr_p = str;
  }
  else if (*str != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", type_name, orig_string)));
  }
}                    

static char *
path_encode(enum path_delim path_delim, int npts, Point *pt)
{
  StringInfoData str;
  int i;

  initStringInfo(&str);

  switch (path_delim)
  {
  case PATH_CLOSED:
    appendStringInfoChar(&str, LDELIM);
    break;
  case PATH_OPEN:
    appendStringInfoChar(&str, LDELIM_EP);
    break;
  case PATH_NONE:
    break;
  }

  for (i = 0; i < npts; i++)
  {
    if (i > 0)
    {
      appendStringInfoChar(&str, DELIM);
    }
    appendStringInfoChar(&str, LDELIM);
    pair_encode(pt->x, pt->y, &str);
    appendStringInfoChar(&str, RDELIM);
    pt++;
  }

  switch (path_delim)
  {
  case PATH_CLOSED:
    appendStringInfoChar(&str, RDELIM);
    break;
  case PATH_OPEN:
    appendStringInfoChar(&str, RDELIM_EP);
    break;
  case PATH_NONE:
    break;
  }

  return str.data;
}                    

                                                                
                                           
                                 
                   
               
                                                           
                                                                 
static int
pair_count(char *s, char delim)
{
  int ndelim = 0;

  while ((s = strchr(s, delim)) != NULL)
  {
    ndelim++;
    s++;
  }
  return (ndelim % 2) ? ((ndelim + 1) / 2) : -1;
}

                                                                         
    
                                         
    
                                                                         

                                                             
                                       
                                                             

                                                 
   
                                          
                           
                                                       
   
Datum
box_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  BOX *box = (BOX *)palloc(sizeof(BOX));
  bool isopen;
  float8 x, y;

  path_decode(str, false, 2, &(box->high), &isopen, NULL, "box", str);

                                       
  if (float8_lt(box->high.x, box->low.x))
  {
    x = box->high.x;
    box->high.x = box->low.x;
    box->low.x = x;
  }
  if (float8_lt(box->high.y, box->low.y))
  {
    y = box->high.y;
    box->high.y = box->low.y;
    box->low.y = y;
  }

  PG_RETURN_BOX_P(box);
}

                                               
   
Datum
box_out(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);

  PG_RETURN_CSTRING(path_encode(PATH_NONE, 2, &(box->high)));
}

   
                                                        
   
Datum
box_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  BOX *box;
  float8 x, y;

  box = (BOX *)palloc(sizeof(BOX));

  box->high.x = pq_getmsgfloat8(buf);
  box->high.y = pq_getmsgfloat8(buf);
  box->low.x = pq_getmsgfloat8(buf);
  box->low.y = pq_getmsgfloat8(buf);

                                       
  if (float8_lt(box->high.x, box->low.x))
  {
    x = box->high.x;
    box->high.x = box->low.x;
    box->low.x = x;
  }
  if (float8_lt(box->high.y, box->low.y))
  {
    y = box->high.y;
    box->high.y = box->low.y;
    box->low.y = y;
  }

  PG_RETURN_BOX_P(box);
}

   
                                               
   
Datum
box_send(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, box->high.x);
  pq_sendfloat8(&buf, box->high.y);
  pq_sendfloat8(&buf, box->low.x);
  pq_sendfloat8(&buf, box->low.y);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                        
   
static inline void
box_construct(BOX *result, Point *pt1, Point *pt2)
{
  if (float8_gt(pt1->x, pt2->x))
  {
    result->high.x = pt1->x;
    result->low.x = pt2->x;
  }
  else
  {
    result->high.x = pt2->x;
    result->low.x = pt1->x;
  }
  if (float8_gt(pt1->y, pt2->y))
  {
    result->high.y = pt1->y;
    result->low.y = pt2->y;
  }
  else
  {
    result->high.y = pt2->y;
    result->low.y = pt1->y;
  }
}

                                                             
                                   
                                                
                                                             

                                          
   
Datum
box_same(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(point_eq_point(&box1->high, &box2->high) && point_eq_point(&box1->low, &box2->low));
}

                                            
   
Datum
box_overlap(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_ov(box1, box2));
}

static bool
box_ov(BOX *box1, BOX *box2)
{
  return (FPle(box1->low.x, box2->high.x) && FPle(box2->low.x, box1->high.x) && FPle(box1->low.y, box2->high.y) && FPle(box2->low.y, box1->high.y));
}

                                                
   
Datum
box_left(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPlt(box1->high.x, box2->low.x));
}

                                                            
                                  
   
                                                              
                                               
   
Datum
box_overleft(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPle(box1->high.x, box2->high.x));
}

                                                  
   
Datum
box_right(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPgt(box1->low.x, box2->high.x));
}

                                                             
                                 
   
                                                                      
                              
   
Datum
box_overright(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPge(box1->low.x, box2->low.x));
}

                                               
   
Datum
box_below(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPlt(box1->high.y, box2->low.y));
}

                                                           
                                  
   
Datum
box_overbelow(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPle(box1->high.y, box2->high.y));
}

                                               
   
Datum
box_above(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPgt(box1->low.y, box2->high.y));
}

                                                           
                                  
   
Datum
box_overabove(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPge(box1->low.y, box2->low.y));
}

                                                
   
Datum
box_contained(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_contain_box(box2, box1));
}

                                            
   
Datum
box_contain(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_contain_box(box1, box2));
}

   
                                                                     
   
static bool
box_contain_box(BOX *contains_box, BOX *contained_box)
{
  return FPge(contains_box->high.x, contained_box->high.x) && FPle(contains_box->low.x, contained_box->low.x) && FPge(contains_box->high.y, contained_box->high.y) && FPle(contains_box->low.y, contained_box->low.y);
}

                     
                                           
   
                                                                      
                                                                       
                                                                         
                                                               
   
Datum
box_below_eq(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPle(box1->high.y, box2->low.y));
}

Datum
box_above_eq(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPge(box1->low.y, box2->high.y));
}

                                                         
                                   
   
Datum
box_lt(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPlt(box_ar(box1), box_ar(box2)));
}

Datum
box_gt(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPgt(box_ar(box1), box_ar(box2)));
}

Datum
box_eq(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPeq(box_ar(box1), box_ar(box2)));
}

Datum
box_le(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPle(box_ar(box1), box_ar(box2)));
}

Datum
box_ge(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(FPge(box_ar(box1), box_ar(box2)));
}

                                                             
                                    
                                                             

                                              
   
Datum
box_area(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);

  PG_RETURN_FLOAT8(box_ar(box));
}

                                               
                                    
   
Datum
box_width(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);

  PG_RETURN_FLOAT8(box_wd(box));
}

                                                 
                                  
   
Datum
box_height(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);

  PG_RETURN_FLOAT8(box_ht(box));
}

                                                     
                                        
   
Datum
box_distance(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);
  Point a, b;

  box_cn(&a, box1);
  box_cn(&b, box2);

  PG_RETURN_FLOAT8(point_dt(&a, &b));
}

                                                        
   
Datum
box_center(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *result = (Point *)palloc(sizeof(Point));

  box_cn(result, box);

  PG_RETURN_POINT_P(result);
}

                                           
   
static float8
box_ar(BOX *box)
{
  return float8_mul(box_wd(box), box_ht(box));
}

                                                              
   
static void
box_cn(Point *center, BOX *box)
{
  center->x = float8_div(float8_pl(box->high.x, box->low.x), 2.0);
  center->y = float8_div(float8_pl(box->high.y, box->low.y), 2.0);
}

                                                    
                                    
   
static float8
box_wd(BOX *box)
{
  return float8_mi(box->high.x, box->low.x);
}

                                            
                                  
   
static float8
box_ht(BOX *box)
{
  return float8_mi(box->high.y, box->low.y);
}

                                                             
                     
                                                             

                    
                                                    
                                          
   
Datum
box_intersect(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0);
  BOX *box2 = PG_GETARG_BOX_P(1);
  BOX *result;

  if (!box_ov(box1, box2))
  {
    PG_RETURN_NULL();
  }

  result = (BOX *)palloc(sizeof(BOX));

  result->high.x = float8_min(box1->high.x, box2->high.x);
  result->low.x = float8_max(box1->low.x, box2->low.x);
  result->high.y = float8_min(box1->high.y, box2->high.y);
  result->low.y = float8_max(box1->low.y, box2->low.y);

  PG_RETURN_BOX_P(result);
}

                   
                                                     
                                          
   
Datum
box_diagonal(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  LSEG *result = (LSEG *)palloc(sizeof(LSEG));

  statlseg_construct(result, &box->high, &box->low);

  PG_RETURN_LSEG_P(result);
}

                                                                         
    
                            
    
                                                                         

static bool
line_decode(char *s, const char *str, LINE *line)
{
                                               
  line->A = single_decode(s, &s, "line", str);
  if (*s++ != DELIM)
  {
    return false;
  }
  line->B = single_decode(s, &s, "line", str);
  if (*s++ != DELIM)
  {
    return false;
  }
  line->C = single_decode(s, &s, "line", str);
  if (*s++ != RDELIM_L)
  {
    return false;
  }
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  if (*s != '\0')
  {
    return false;
  }
  return true;
}

Datum
line_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  LINE *line = (LINE *)palloc(sizeof(LINE));
  LSEG lseg;
  bool isopen;
  char *s;

  s = str;
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  if (*s == LDELIM_L)
  {
    if (!line_decode(s + 1, str, line))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "line", str)));
    }
    if (FPzero(line->A) && FPzero(line->B))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid line specification: A and B cannot both be zero")));
    }
  }
  else
  {
    path_decode(s, true, 2, &lseg.p[0], &isopen, NULL, "line", str);
    if (point_eq_point(&lseg.p[0], &lseg.p[1]))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid line specification: must be two distinct points")));
    }
    line_construct(line, &lseg.p[0], lseg_sl(&lseg));
  }

  PG_RETURN_LINE_P(line);
}

Datum
line_out(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);
  char *astr = float8out_internal(line->A);
  char *bstr = float8out_internal(line->B);
  char *cstr = float8out_internal(line->C);

  PG_RETURN_CSTRING(psprintf("%c%s%c%s%c%s%c", LDELIM_L, astr, DELIM, bstr, DELIM, cstr, RDELIM_L));
}

   
                                                          
   
Datum
line_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  LINE *line;

  line = (LINE *)palloc(sizeof(LINE));

  line->A = pq_getmsgfloat8(buf);
  line->B = pq_getmsgfloat8(buf);
  line->C = pq_getmsgfloat8(buf);

  if (FPzero(line->A) && FPzero(line->B))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid line specification: A and B cannot both be zero")));
  }

  PG_RETURN_LINE_P(line);
}

   
                                                 
   
Datum
line_send(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, line->A);
  pq_sendfloat8(&buf, line->B);
  pq_sendfloat8(&buf, line->C);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                             
                                                          
                             
                                                             

   
                                                                   
   
static inline void
line_construct(LINE *result, Point *pt, float8 m)
{
  if (m == DBL_MAX)
  {
                                
    result->A = -1.0;
    result->B = 0.0;
    result->C = pt->x;
  }
  else
  {
                                   
    result->A = m;
    result->B = -1.0;
    result->C = float8_mi(pt->y, float8_mul(m, pt->x));
                                                                         
    if (result->C == 0.0)
    {
      result->C = 0.0;
    }
  }
}

                       
              
   
Datum
line_construct_pp(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);
  LINE *result = (LINE *)palloc(sizeof(LINE));

  if (point_eq_point(pt1, pt2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid line specification: must be two distinct points")));
  }

  line_construct(result, pt1, point_sl(pt1, pt2));

  PG_RETURN_LINE_P(result);
}

                                                             
                               
                                                             

Datum
line_intersect(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);

  PG_RETURN_BOOL(line_interpt_line(NULL, l1, l2));
}

Datum
line_parallel(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);

  PG_RETURN_BOOL(!line_interpt_line(NULL, l1, l2));
}

Datum
line_perp(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);

  if (FPzero(l1->A))
  {
    PG_RETURN_BOOL(FPzero(l2->B));
  }
  if (FPzero(l2->A))
  {
    PG_RETURN_BOOL(FPzero(l1->B));
  }
  if (FPzero(l1->B))
  {
    PG_RETURN_BOOL(FPzero(l2->A));
  }
  if (FPzero(l2->B))
  {
    PG_RETURN_BOOL(FPzero(l1->A));
  }

  PG_RETURN_BOOL(FPeq(float8_div(float8_mul(l1->A, l2->A), float8_mul(l1->B, l2->B)), -1.0));
}

Datum
line_vertical(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);

  PG_RETURN_BOOL(FPzero(line->B));
}

Datum
line_horizontal(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);

  PG_RETURN_BOOL(FPzero(line->A));
}

   
                                            
   
                                                                        
                
   
Datum
line_eq(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);
  float8 ratio;

  if (!FPzero(l2->A) && !isnan(l2->A))
  {
    ratio = float8_div(l1->A, l2->A);
  }
  else if (!FPzero(l2->B) && !isnan(l2->B))
  {
    ratio = float8_div(l1->B, l2->B);
  }
  else if (!FPzero(l2->C) && !isnan(l2->C))
  {
    ratio = float8_div(l1->C, l2->C);
  }
  else
  {
    ratio = 1.0;
  }

  PG_RETURN_BOOL((FPeq(l1->A, float8_mul(ratio, l2->A)) && FPeq(l1->B, float8_mul(ratio, l2->B)) && FPeq(l1->C, float8_mul(ratio, l2->C))) || (float8_eq(l1->A, l2->A) && float8_eq(l1->B, l2->B) && float8_eq(l1->C, l2->C)));
}

                                                             
                             
                                                             

   
                            
   
static inline float8
line_sl(LINE *line)
{
  if (FPzero(line->A))
  {
    return 0.0;
  }
  if (FPzero(line->B))
  {
    return DBL_MAX;
  }
  return float8_div(line->A, -line->B);
}

   
                                    
   
static inline float8
line_invsl(LINE *line)
{
  if (FPzero(line->A))
  {
    return DBL_MAX;
  }
  if (FPzero(line->B))
  {
    return 0.0;
  }
  return float8_div(line->B, line->A);
}

                   
                               
   
Datum
line_distance(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);
  float8 ratio;

  if (line_interpt_line(NULL, l1, l2))                    
  {
    PG_RETURN_FLOAT8(0.0);
  }

  if (!FPzero(l1->A) && !isnan(l1->A) && !FPzero(l2->A) && !isnan(l2->A))
  {
    ratio = float8_div(l1->A, l2->A);
  }
  else if (!FPzero(l1->B) && !isnan(l1->B) && !FPzero(l2->B) && !isnan(l2->B))
  {
    ratio = float8_div(l1->B, l2->B);
  }
  else
  {
    ratio = 1.0;
  }

  PG_RETURN_FLOAT8(float8_div(fabs(float8_mi(l1->C, float8_mul(ratio, l2->C))), HYPOT(l1->A, l1->B)));
}

                  
                                                   
   
Datum
line_interpt(PG_FUNCTION_ARGS)
{
  LINE *l1 = PG_GETARG_LINE_P(0);
  LINE *l2 = PG_GETARG_LINE_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (!line_interpt_line(result, l1, l2))
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_POINT_P(result);
}

   
                                    
   
                                                                            
                           
   
                                                                        
                                                                    
                                                                      
   
                                                                              
                                                                             
                                                   
   
static bool
line_interpt_line(Point *result, LINE *l1, LINE *l2)
{
  float8 x, y;

  if (!FPzero(l1->B))
  {
    if (FPeq(l2->A, float8_mul(l1->A, float8_div(l2->B, l1->B))))
    {
      return false;
    }

    x = float8_div(float8_mi(float8_mul(l1->B, l2->C), float8_mul(l2->B, l1->C)), float8_mi(float8_mul(l1->A, l2->B), float8_mul(l2->A, l1->B)));
    y = float8_div(-float8_pl(float8_mul(l1->A, x), l1->C), l1->B);
  }
  else if (!FPzero(l2->B))
  {
    if (FPeq(l1->A, float8_mul(l2->A, float8_div(l1->B, l2->B))))
    {
      return false;
    }

    x = float8_div(float8_mi(float8_mul(l2->B, l1->C), float8_mul(l1->B, l2->C)), float8_mi(float8_mul(l2->A, l1->B), float8_mul(l1->A, l2->B)));
    y = float8_div(-float8_pl(float8_mul(l2->A, x), l2->C), l2->B);
  }
  else
  {
    return false;
  }

                                                                        
  if (x == 0.0)
  {
    x = 0.0;
  }
  if (y == 0.0)
  {
    y = 0.0;
  }

  if (result != NULL)
  {
    point_construct(result, x, y);
  }

  return true;
}

                                                                         
    
                                                             
                            
    
                                                          
                                                           
                                                      
    
                                                                         

                                                             
                                               
                     
                                
                                
                              
                              
                               
                                            
                                                             

Datum
path_area(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);
  float8 area = 0.0;
  int i, j;

  if (!path->closed)
  {
    PG_RETURN_NULL();
  }

  for (i = 0; i < path->npts; i++)
  {
    j = (i + 1) % path->npts;
    area = float8_pl(area, float8_mul(path->p[i].x, path->p[j].y));
    area = float8_mi(area, float8_mul(path->p[i].y, path->p[j].x));
  }

  PG_RETURN_FLOAT8(float8_div(fabs(area), 2.0));
}

Datum
path_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  PATH *path;
  bool isopen;
  char *s;
  int npts;
  int size;
  int base_size;
  int depth = 0;

  if ((npts = pair_count(str, ',')) <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "path", str)));
  }

  s = str;
  while (isspace((unsigned char)*s))
  {
    s++;
  }

                                 
  if ((*s == LDELIM) && (strrchr(s, LDELIM) == s))
  {
    s++;
    depth++;
  }

  base_size = sizeof(path->p[0]) * npts;
  size = offsetof(PATH, p) + base_size;

                                  
  if (base_size / npts != sizeof(path->p[0]) || size <= base_size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many points requested")));
  }

  path = (PATH *)palloc(size);

  SET_VARSIZE(path, size);
  path->npts = npts;

  path_decode(s, true, npts, &(path->p[0]), &isopen, &s, "path", str);

  if (depth >= 1)
  {
    if (*s++ != RDELIM)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "path", str)));
    }
    while (isspace((unsigned char)*s))
    {
      s++;
    }
  }
  if (*s != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "path", str)));
  }

  path->closed = (!isopen);
                                               
  path->dummy = 0;

  PG_RETURN_PATH_P(path);
}

Datum
path_out(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);

  PG_RETURN_CSTRING(path_encode(path->closed ? PATH_CLOSED : PATH_OPEN, path->npts, path->p));
}

   
                                                          
   
                                                                         
                              
   
Datum
path_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  PATH *path;
  int closed;
  int32 npts;
  int32 i;
  int size;

  closed = pq_getmsgbyte(buf);
  npts = pq_getmsgint(buf, sizeof(int32));
  if (npts <= 0 || npts >= (int32)((INT_MAX - offsetof(PATH, p)) / sizeof(Point)))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid number of points in external \"path\" value")));
  }

  size = offsetof(PATH, p) + sizeof(path->p[0]) * npts;
  path = (PATH *)palloc(size);

  SET_VARSIZE(path, size);
  path->npts = npts;
  path->closed = (closed ? 1 : 0);
                                               
  path->dummy = 0;

  for (i = 0; i < npts; i++)
  {
    path->p[i].x = pq_getmsgfloat8(buf);
    path->p[i].y = pq_getmsgfloat8(buf);
  }

  PG_RETURN_PATH_P(path);
}

   
                                                 
   
Datum
path_send(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);
  StringInfoData buf;
  int32 i;

  pq_begintypsend(&buf);
  pq_sendbyte(&buf, path->closed ? 1 : 0);
  pq_sendint32(&buf, path->npts);
  for (i = 0; i < path->npts; i++)
  {
    pq_sendfloat8(&buf, path->p[i].x);
    pq_sendfloat8(&buf, path->p[i].y);
  }
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                             
                         
                                             
                              
   
                                                  
                                                             

Datum
path_n_lt(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);

  PG_RETURN_BOOL(p1->npts < p2->npts);
}

Datum
path_n_gt(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);

  PG_RETURN_BOOL(p1->npts > p2->npts);
}

Datum
path_n_eq(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);

  PG_RETURN_BOOL(p1->npts == p2->npts);
}

Datum
path_n_le(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);

  PG_RETURN_BOOL(p1->npts <= p2->npts);
}

Datum
path_n_ge(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);

  PG_RETURN_BOOL(p1->npts >= p2->npts);
}

                                                             
                         
                                                             

Datum
path_isclosed(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);

  PG_RETURN_BOOL(path->closed);
}

Datum
path_isopen(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);

  PG_RETURN_BOOL(!path->closed);
}

Datum
path_npoints(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);

  PG_RETURN_INT32(path->npts);
}

Datum
path_close(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);

  path->closed = true;

  PG_RETURN_PATH_P(path);
}

Datum
path_open(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);

  path->closed = false;

  PG_RETURN_PATH_P(path);
}

                
                                       
                                                           
                                 
   
Datum
path_inter(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);
  BOX b1, b2;
  int i, j;
  LSEG seg1, seg2;

  Assert(p1->npts > 0 && p2->npts > 0);

  b1.high.x = b1.low.x = p1->p[0].x;
  b1.high.y = b1.low.y = p1->p[0].y;
  for (i = 1; i < p1->npts; i++)
  {
    b1.high.x = float8_max(p1->p[i].x, b1.high.x);
    b1.high.y = float8_max(p1->p[i].y, b1.high.y);
    b1.low.x = float8_min(p1->p[i].x, b1.low.x);
    b1.low.y = float8_min(p1->p[i].y, b1.low.y);
  }
  b2.high.x = b2.low.x = p2->p[0].x;
  b2.high.y = b2.low.y = p2->p[0].y;
  for (i = 1; i < p2->npts; i++)
  {
    b2.high.x = float8_max(p2->p[i].x, b2.high.x);
    b2.high.y = float8_max(p2->p[i].y, b2.high.y);
    b2.low.x = float8_min(p2->p[i].x, b2.low.x);
    b2.low.y = float8_min(p2->p[i].y, b2.low.y);
  }
  if (!box_ov(&b1, &b2))
  {
    PG_RETURN_BOOL(false);
  }

                                         
  for (i = 0; i < p1->npts; i++)
  {
    int iprev;

    if (i > 0)
    {
      iprev = i - 1;
    }
    else
    {
      if (!p1->closed)
      {
        continue;
      }
      iprev = p1->npts - 1;                                  
    }

    for (j = 0; j < p2->npts; j++)
    {
      int jprev;

      if (j > 0)
      {
        jprev = j - 1;
      }
      else
      {
        if (!p2->closed)
        {
          continue;
        }
        jprev = p2->npts - 1;                                  
      }

      statlseg_construct(&seg1, &p1->p[iprev], &p1->p[i]);
      statlseg_construct(&seg2, &p2->p[jprev], &p2->p[j]);
      if (lseg_interpt_lseg(NULL, &seg1, &seg2))
      {
        PG_RETURN_BOOL(true);
      }
    }
  }

                                                      
  PG_RETURN_BOOL(false);
}

                   
                                                                 
                                                               
   
Datum
path_distance(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);
  float8 min = 0.0;                                        
  bool have_min = false;
  float8 tmp;
  int i, j;
  LSEG seg1, seg2;

  for (i = 0; i < p1->npts; i++)
  {
    int iprev;

    if (i > 0)
    {
      iprev = i - 1;
    }
    else
    {
      if (!p1->closed)
      {
        continue;
      }
      iprev = p1->npts - 1;                                  
    }

    for (j = 0; j < p2->npts; j++)
    {
      int jprev;

      if (j > 0)
      {
        jprev = j - 1;
      }
      else
      {
        if (!p2->closed)
        {
          continue;
        }
        jprev = p2->npts - 1;                                  
      }

      statlseg_construct(&seg1, &p1->p[iprev], &p1->p[i]);
      statlseg_construct(&seg2, &p2->p[jprev], &p2->p[j]);

      tmp = lseg_closept_lseg(NULL, &seg1, &seg2);
      if (!have_min || float8_lt(tmp, min))
      {
        min = tmp;
        have_min = true;
      }
    }
  }

  if (!have_min)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_FLOAT8(min);
}

                                                             
                            
                                                             

Datum
path_length(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);
  float8 result = 0.0;
  int i;

  for (i = 0; i < path->npts; i++)
  {
    int iprev;

    if (i > 0)
    {
      iprev = i - 1;
    }
    else
    {
      if (!path->closed)
      {
        continue;
      }
      iprev = path->npts - 1;                                  
    }

    result = float8_pl(result, point_dt(&path->p[iprev], &path->p[i]));
  }

  PG_RETURN_FLOAT8(result);
}

                                                                         
    
                             
    
                                                                         

                                                             
                                                
                     
              
            
                                                             

Datum
point_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  Point *point = (Point *)palloc(sizeof(Point));

  pair_decode(str, &point->x, &point->y, NULL, "point", str);
  PG_RETURN_POINT_P(point);
}

Datum
point_out(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);

  PG_RETURN_CSTRING(path_encode(PATH_NONE, 1, pt));
}

   
                                                            
   
Datum
point_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  Point *point;

  point = (Point *)palloc(sizeof(Point));
  point->x = pq_getmsgfloat8(buf);
  point->y = pq_getmsgfloat8(buf);
  PG_RETURN_POINT_P(point);
}

   
                                                   
   
Datum
point_send(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, pt->x);
  pq_sendfloat8(&buf, pt->y);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                      
   
static inline void
point_construct(Point *result, float8 x, float8 y)
{
  result->x = x;
  result->y = y;
}

                                                             
                                    
                                                  
                                                           
                                                        
                                                          
                    
                                                             

Datum
point_left(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPlt(pt1->x, pt2->x));
}

Datum
point_right(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPgt(pt1->x, pt2->x));
}

Datum
point_above(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPgt(pt1->y, pt2->y));
}

Datum
point_below(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPlt(pt1->y, pt2->y));
}

Datum
point_vert(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPeq(pt1->x, pt2->x));
}

Datum
point_horiz(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(FPeq(pt1->y, pt2->y));
}

Datum
point_eq(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(point_eq_point(pt1, pt2));
}

Datum
point_ne(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(!point_eq_point(pt1, pt2));
}

   
                                             
   
                                                                              
                
   
static inline bool
point_eq_point(Point *pt1, Point *pt2)
{
  return ((FPeq(pt1->x, pt2->x) && FPeq(pt1->y, pt2->y)) || (float8_eq(pt1->x, pt2->x) && float8_eq(pt1->y, pt2->y)));
}

                                                             
                                     
                                                             

Datum
point_distance(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_FLOAT8(point_dt(pt1, pt2));
}

static inline float8
point_dt(Point *pt1, Point *pt2)
{
  return HYPOT(float8_mi(pt1->x, pt2->x), float8_mi(pt1->y, pt2->y));
}

Datum
point_slope(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);

  PG_RETURN_FLOAT8(point_sl(pt1, pt2));
}

   
                              
   
                                                                         
   
static inline float8
point_sl(Point *pt1, Point *pt2)
{
  if (FPeq(pt1->x, pt2->x))
  {
    return DBL_MAX;
  }
  if (FPeq(pt1->y, pt2->y))
  {
    return 0.0;
  }
  return float8_div(float8_mi(pt1->y, pt2->y), float8_mi(pt1->x, pt2->x));
}

   
                                      
   
                                                                     
   
static inline float8
point_invsl(Point *pt1, Point *pt2)
{
  if (FPeq(pt1->x, pt2->x))
  {
    return 0.0;
  }
  if (FPeq(pt1->y, pt2->y))
  {
    return DBL_MAX;
  }
  return float8_div(float8_mi(pt1->x, pt2->x), float8_mi(pt2->y, pt1->y));
}

                                                                         
    
                                    
    
                                                                         

                                                             
                                              
                                           
                             
                         
                                          
                                   
                                                             

Datum
lseg_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  LSEG *lseg = (LSEG *)palloc(sizeof(LSEG));
  bool isopen;

  path_decode(str, true, 2, &lseg->p[0], &isopen, NULL, "lseg", str);
  PG_RETURN_LSEG_P(lseg);
}

Datum
lseg_out(PG_FUNCTION_ARGS)
{
  LSEG *ls = PG_GETARG_LSEG_P(0);

  PG_RETURN_CSTRING(path_encode(PATH_OPEN, 2, &ls->p[0]));
}

   
                                                          
   
Datum
lseg_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  LSEG *lseg;

  lseg = (LSEG *)palloc(sizeof(LSEG));

  lseg->p[0].x = pq_getmsgfloat8(buf);
  lseg->p[0].y = pq_getmsgfloat8(buf);
  lseg->p[1].x = pq_getmsgfloat8(buf);
  lseg->p[1].y = pq_getmsgfloat8(buf);

  PG_RETURN_LSEG_P(lseg);
}

   
                                                 
   
Datum
lseg_send(PG_FUNCTION_ARGS)
{
  LSEG *ls = PG_GETARG_LSEG_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, ls->p[0].x);
  pq_sendfloat8(&buf, ls->p[0].y);
  pq_sendfloat8(&buf, ls->p[1].x);
  pq_sendfloat8(&buf, ls->p[1].y);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                    
                                 
   
Datum
lseg_construct(PG_FUNCTION_ARGS)
{
  Point *pt1 = PG_GETARG_POINT_P(0);
  Point *pt2 = PG_GETARG_POINT_P(1);
  LSEG *result = (LSEG *)palloc(sizeof(LSEG));

  statlseg_construct(result, pt1, pt2);

  PG_RETURN_LSEG_P(result);
}

                                                             
static inline void
statlseg_construct(LSEG *lseg, Point *pt1, Point *pt2)
{
  lseg->p[0].x = pt1->x;
  lseg->p[0].y = pt1->y;
  lseg->p[1].x = pt2->x;
  lseg->p[1].y = pt2->y;
}

   
                                    
   
static inline float8
lseg_sl(LSEG *lseg)
{
  return point_sl(&lseg->p[0], &lseg->p[1]);
}

   
                                            
   
static inline float8
lseg_invsl(LSEG *lseg)
{
  return point_invsl(&lseg->p[0], &lseg->p[1]);
}

Datum
lseg_length(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);

  PG_RETURN_FLOAT8(point_dt(&lseg->p[0], &lseg->p[1]));
}

                                                             
                               
                                                             

   
                                                                
                    
   
Datum
lseg_intersect(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(lseg_interpt_lseg(NULL, l1, l2));
}

Datum
lseg_parallel(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPeq(lseg_sl(l1), lseg_sl(l2)));
}

   
                                                     
   
Datum
lseg_perp(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPeq(lseg_sl(l1), lseg_invsl(l2)));
}

Datum
lseg_vertical(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);

  PG_RETURN_BOOL(FPeq(lseg->p[0].x, lseg->p[1].x));
}

Datum
lseg_horizontal(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);

  PG_RETURN_BOOL(FPeq(lseg->p[0].y, lseg->p[1].y));
}

Datum
lseg_eq(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(point_eq_point(&l1->p[0], &l2->p[0]) && point_eq_point(&l1->p[1], &l2->p[1]));
}

Datum
lseg_ne(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(!point_eq_point(&l1->p[0], &l2->p[0]) || !point_eq_point(&l1->p[1], &l2->p[1]));
}

Datum
lseg_lt(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPlt(point_dt(&l1->p[0], &l1->p[1]), point_dt(&l2->p[0], &l2->p[1])));
}

Datum
lseg_le(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPle(point_dt(&l1->p[0], &l1->p[1]), point_dt(&l2->p[0], &l2->p[1])));
}

Datum
lseg_gt(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPgt(point_dt(&l1->p[0], &l1->p[1]), point_dt(&l2->p[0], &l2->p[1])));
}

Datum
lseg_ge(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(FPge(point_dt(&l1->p[0], &l1->p[1]), point_dt(&l2->p[0], &l2->p[1])));
}

                                                             
                             
                                                             

                   
                                                      
                                                         
             
   
Datum
lseg_distance(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);

  PG_RETURN_FLOAT8(lseg_closept_lseg(NULL, l1, l2));
}

Datum
lseg_center(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  result->x = float8_div(float8_pl(lseg->p[0].x, lseg->p[1].x), 2.0);
  result->y = float8_div(float8_pl(lseg->p[0].y, lseg->p[1].y), 2.0);

  PG_RETURN_POINT_P(result);
}

   
                                                                      
                                        
   
                                                                            
                                                               
   
static bool
lseg_interpt_lseg(Point *result, LSEG *l1, LSEG *l2)
{
  Point interpt;
  LINE tmp;

  line_construct(&tmp, &l2->p[0], lseg_sl(l2));
  if (!lseg_interpt_line(&interpt, l1, &tmp))
  {
    return false;
  }

     
                                                                       
                                        
     
  if (!lseg_contain_point(l2, &interpt))
  {
    return false;
  }

  if (result != NULL)
  {
    *result = interpt;
  }

  return true;
}

Datum
lseg_interpt(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (!lseg_interpt_lseg(result, l1, l2))
  {
    PG_RETURN_NULL();
  }
  PG_RETURN_POINT_P(result);
}

                                                                         
    
                                                            
                   
    
                                                                         

                                                                        
          
                                                   
                                                                       

   
                                   
   
Datum
dist_pl(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LINE *line = PG_GETARG_LINE_P(1);

  PG_RETURN_FLOAT8(line_closept_point(NULL, line, pt));
}

   
                                   
   
Datum
dist_ps(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LSEG *lseg = PG_GETARG_LSEG_P(1);

  PG_RETURN_FLOAT8(lseg_closept_point(NULL, lseg, pt));
}

   
                                   
   
Datum
dist_ppath(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  PATH *path = PG_GETARG_PATH_P(1);
  float8 result = 0.0;                          
  bool have_min = false;
  float8 tmp;
  int i;
  LSEG lseg;

  Assert(path->npts > 0);

     
                                                                           
                                               
     
  for (i = 0; i < path->npts; i++)
  {
    int iprev;

    if (i > 0)
    {
      iprev = i - 1;
    }
    else
    {
      if (!path->closed)
      {
        continue;
      }
      iprev = path->npts - 1;                                  
    }

    statlseg_construct(&lseg, &path->p[iprev], &path->p[i]);
    tmp = lseg_closept_point(NULL, &lseg, pt);
    if (!have_min || float8_lt(tmp, result))
    {
      result = tmp;
      have_min = true;
    }
  }

  PG_RETURN_FLOAT8(result);
}

   
                                  
   
Datum
dist_pb(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  BOX *box = PG_GETARG_BOX_P(1);

  PG_RETURN_FLOAT8(box_closept_point(NULL, box, pt));
}

   
                                  
   
Datum
dist_sl(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  LINE *line = PG_GETARG_LINE_P(1);

  PG_RETURN_FLOAT8(lseg_closept_line(NULL, lseg, line));
}

   
                                 
   
Datum
dist_sb(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  BOX *box = PG_GETARG_BOX_P(1);

  PG_RETURN_FLOAT8(box_closept_lseg(NULL, box, lseg));
}

   
                                 
   
Datum
dist_lb(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
  LINE *line = PG_GETARG_LINE_P(0);
  BOX *box = PG_GETARG_BOX_P(1);
#endif

                                                
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function \"dist_lb\" not implemented")));

  PG_RETURN_NULL();
}

   
                                       
   
Datum
dist_cpoly(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  POLYGON *poly = PG_GETARG_POLYGON_P(1);
  float8 result;

                                                         
  result = float8_mi(dist_ppoly_internal(&circle->center, poly), circle->radius);
  if (result < 0.0)
  {
    result = 0.0;
  }

  PG_RETURN_FLOAT8(result);
}

   
                                      
   
Datum
dist_ppoly(PG_FUNCTION_ARGS)
{
  Point *point = PG_GETARG_POINT_P(0);
  POLYGON *poly = PG_GETARG_POLYGON_P(1);

  PG_RETURN_FLOAT8(dist_ppoly_internal(point, poly));
}

Datum
dist_polyp(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  Point *point = PG_GETARG_POINT_P(1);

  PG_RETURN_FLOAT8(dist_ppoly_internal(point, poly));
}

static float8
dist_ppoly_internal(Point *pt, POLYGON *poly)
{
  float8 result;
  float8 d;
  int i;
  LSEG seg;

  if (point_inside(pt, poly->npts, poly->p) != 0)
  {
    return 0.0;
  }

                                                                      
  seg.p[0].x = poly->p[0].x;
  seg.p[0].y = poly->p[0].y;
  seg.p[1].x = poly->p[poly->npts - 1].x;
  seg.p[1].y = poly->p[poly->npts - 1].y;
  result = lseg_closept_point(NULL, &seg, pt);

                                          
  for (i = 0; i < poly->npts - 1; i++)
  {
    seg.p[0].x = poly->p[i].x;
    seg.p[0].y = poly->p[i].y;
    seg.p[1].x = poly->p[i + 1].x;
    seg.p[1].y = poly->p[i + 1].y;
    d = lseg_closept_point(NULL, &seg, pt);
    if (float8_lt(d, result))
    {
      result = d;
    }
  }

  return result;
}

                                                                        
             
                                     
                                                              
                                                        
                                                                       

   
                                                                              
                                              
   
static bool
lseg_interpt_line(Point *result, LSEG *lseg, LINE *line)
{
  Point interpt;
  LINE tmp;

     
                                                                          
                                                                      
                                      
     
  line_construct(&tmp, &lseg->p[0], lseg_sl(lseg));
  if (!line_interpt_line(&interpt, &tmp, line))
  {
    return false;
  }

     
                                                                           
              
     
  if (!lseg_contain_point(lseg, &interpt))
  {
    return false;
  }
  if (result != NULL)
  {
       
                                                                       
                                                                       
                
       
    if (point_eq_point(&lseg->p[0], &interpt))
    {
      *result = lseg->p[0];
    }
    else if (point_eq_point(&lseg->p[1], &interpt))
    {
      *result = lseg->p[1];
    }
    else
    {
      *result = interpt;
    }
  }

  return true;
}

                                                                        
           
                                                  
                                                                       

   
                                                                    
                                                                      
                        
   
static float8
line_closept_point(Point *result, LINE *line, Point *point)
{
  Point closept;
  LINE tmp;

     
                                                                            
                                                                     
                                                                
     
  line_construct(&tmp, point, line_invsl(line));
  if (!line_interpt_line(&closept, &tmp, line))
  {
    if (result != NULL)
    {
      *result = *point;
    }

    return get_float8_nan();
  }

  if (result != NULL)
  {
    *result = closept;
  }

  return point_dt(&closept, point);
}

Datum
close_pl(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LINE *line = PG_GETARG_LINE_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (isnan(line_closept_point(result, line, pt)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

   
                                                     
   
                                                                           
                                                          
   
static float8
lseg_closept_point(Point *result, LSEG *lseg, Point *pt)
{
  Point closept;
  LINE tmp;

     
                                                                            
                          
     
  line_construct(&tmp, pt, point_invsl(&lseg->p[0], &lseg->p[1]));
  lseg_closept_line(&closept, lseg, &tmp);

  if (result != NULL)
  {
    *result = closept;
  }

  return point_dt(&closept, pt);
}

Datum
close_ps(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LSEG *lseg = PG_GETARG_LSEG_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (isnan(lseg_closept_point(result, lseg, pt)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

   
                                                 
   
static float8
lseg_closept_lseg(Point *result, LSEG *on_lseg, LSEG *to_lseg)
{
  Point point;
  float8 dist, d;

                                                                          
  if (lseg_interpt_lseg(result, on_lseg, to_lseg))
  {
    return 0.0;
  }

     
                                                                            
                                        
     
  dist = lseg_closept_point(result, on_lseg, &to_lseg->p[0]);
  d = lseg_closept_point(&point, on_lseg, &to_lseg->p[1]);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = point;
    }
  }

                                                                             
  d = lseg_closept_point(NULL, to_lseg, &on_lseg->p[0]);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = on_lseg->p[0];
    }
  }
  d = lseg_closept_point(NULL, to_lseg, &on_lseg->p[1]);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = on_lseg->p[1];
    }
  }

  return dist;
}

Datum
close_lseg(PG_FUNCTION_ARGS)
{
  LSEG *l1 = PG_GETARG_LSEG_P(0);
  LSEG *l2 = PG_GETARG_LSEG_P(1);
  Point *result;

  if (lseg_sl(l1) == lseg_sl(l2))
  {
    PG_RETURN_NULL();
  }

  result = (Point *)palloc(sizeof(Point));

  if (isnan(lseg_closept_lseg(result, l2, l1)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

   
                                                  
   
                                                                         
                                                           
   
static float8
box_closept_point(Point *result, BOX *box, Point *pt)
{
  float8 dist, d;
  Point point, closept;
  LSEG lseg;

  if (box_contain_point(box, pt))
  {
    if (result != NULL)
    {
      *result = *pt;
    }

    return 0.0;
  }

                                     
  point.x = box->low.x;
  point.y = box->high.y;
  statlseg_construct(&lseg, &box->low, &point);
  dist = lseg_closept_point(result, &lseg, pt);

  statlseg_construct(&lseg, &box->high, &point);
  d = lseg_closept_point(&closept, &lseg, pt);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  point.x = box->high.x;
  point.y = box->low.y;
  statlseg_construct(&lseg, &box->low, &point);
  d = lseg_closept_point(&closept, &lseg, pt);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  statlseg_construct(&lseg, &box->high, &point);
  d = lseg_closept_point(&closept, &lseg, pt);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  return dist;
}

Datum
close_pb(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  BOX *box = PG_GETARG_BOX_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (isnan(box_closept_point(result, box, pt)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

              
                                          
   
                          
                                                                  
                                                          
                                                                         
                       
   
Datum
close_sl(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  LINE *line = PG_GETARG_LINE_P(1);
  Point *result;
  float8 d1, d2;

  result = (Point *)palloc(sizeof(Point));

  if (lseg_interpt_line(result, lseg, line))
  {
    PG_RETURN_POINT_P(result);
  }

  d1 = line_closept_point(NULL, line, &lseg->p[0]);
  d2 = line_closept_point(NULL, line, &lseg->p[1]);
  if (float8_lt(d1, d2))
  {
    *result = lseg->p[0];
  }
  else
  {
    *result = lseg->p[1];
  }

  PG_RETURN_POINT_P(result);
#endif

  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function \"close_sl\" not implemented")));

  PG_RETURN_NULL();
}

   
                                          
   
                                                                          
                                                                       
   
                                                                           
                                                                        
                                                                             
                                                                           
          
   
static float8
lseg_closept_line(Point *result, LSEG *lseg, LINE *line)
{
  float8 dist1, dist2;

  if (lseg_interpt_line(result, lseg, line))
  {
    return 0.0;
  }

  dist1 = line_closept_point(NULL, line, &lseg->p[0]);
  dist2 = line_closept_point(NULL, line, &lseg->p[1]);

  if (dist1 < dist2)
  {
    if (result != NULL)
    {
      *result = lseg->p[0];
    }

    return dist1;
  }
  else
  {
    if (result != NULL)
    {
      *result = lseg->p[1];
    }

    return dist2;
  }
}

Datum
close_ls(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);
  LSEG *lseg = PG_GETARG_LSEG_P(1);
  Point *result;

  if (lseg_sl(lseg) == line_sl(line))
  {
    PG_RETURN_NULL();
  }

  result = (Point *)palloc(sizeof(Point));

  if (isnan(lseg_closept_line(result, lseg, line)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

   
                                               
   
                                                                      
                                                                       
   
static float8
box_closept_lseg(Point *result, BOX *box, LSEG *lseg)
{
  float8 dist, d;
  Point point, closept;
  LSEG bseg;

  if (box_interpt_lseg(result, box, lseg))
  {
    return 0.0;
  }

                                     
  point.x = box->low.x;
  point.y = box->high.y;
  statlseg_construct(&bseg, &box->low, &point);
  dist = lseg_closept_lseg(result, &bseg, lseg);

  statlseg_construct(&bseg, &box->high, &point);
  d = lseg_closept_lseg(&closept, &bseg, lseg);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  point.x = box->high.x;
  point.y = box->low.y;
  statlseg_construct(&bseg, &box->low, &point);
  d = lseg_closept_lseg(&closept, &bseg, lseg);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  statlseg_construct(&bseg, &box->high, &point);
  d = lseg_closept_lseg(&closept, &bseg, lseg);
  if (float8_lt(d, dist))
  {
    dist = d;
    if (result != NULL)
    {
      *result = closept;
    }
  }

  return dist;
}

Datum
close_sb(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  BOX *box = PG_GETARG_BOX_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  if (isnan(box_closept_lseg(result, box, lseg)))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_POINT_P(result);
}

Datum
close_lb(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
  LINE *line = PG_GETARG_LINE_P(0);
  BOX *box = PG_GETARG_BOX_P(1);
#endif

                                        
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function \"close_lb\" not implemented")));

  PG_RETURN_NULL();
}

                                                                        
        
                                                         
                                                                       

   
                                         
   
static bool
line_contain_point(LINE *line, Point *point)
{
  return FPzero(float8_pl(float8_pl(float8_mul(line->A, point->x), float8_mul(line->B, point->y)), line->C));
}

Datum
on_pl(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LINE *line = PG_GETARG_LINE_P(1);

  PG_RETURN_BOOL(line_contain_point(line, pt));
}

   
                                                              
                                                                                 
   
static bool
lseg_contain_point(LSEG *lseg, Point *pt)
{
  return FPeq(point_dt(pt, &lseg->p[0]) + point_dt(pt, &lseg->p[1]), point_dt(&lseg->p[0], &lseg->p[1]));
}

Datum
on_ps(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  LSEG *lseg = PG_GETARG_LSEG_P(1);

  PG_RETURN_BOOL(lseg_contain_point(lseg, pt));
}

   
                                                          
   
static bool
box_contain_point(BOX *box, Point *point)
{
  return box->high.x >= point->x && box->low.x <= point->x && box->high.y >= point->y && box->low.y <= point->y;
}

Datum
on_pb(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  BOX *box = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_contain_point(box, pt));
}

Datum
box_contain_pt(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *pt = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(box_contain_point(box, pt));
}

              
                                                 
                                                    
                                                                            
                                                                    
                                                       
                                                     
                                                            
                     
                                                              
   
Datum
on_ppath(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  PATH *path = PG_GETARG_PATH_P(1);
  int i, n;
  float8 a, b;

                
  if (!path->closed)
  {
    n = path->npts - 1;
    a = point_dt(pt, &path->p[0]);
    for (i = 0; i < n; i++)
    {
      b = point_dt(pt, &path->p[i + 1]);
      if (FPeq(float8_pl(a, b), point_dt(&path->p[i], &path->p[i + 1])))
      {
        PG_RETURN_BOOL(true);
      }
      a = b;
    }
    PG_RETURN_BOOL(false);
  }

                  
  PG_RETURN_BOOL(point_inside(pt, path->npts, path->p) != 0);
}

   
                                                                 
   
                                                                 
   
Datum
on_sl(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  LINE *line = PG_GETARG_LINE_P(1);

  PG_RETURN_BOOL(line_contain_point(line, &lseg->p[0]) && line_contain_point(line, &lseg->p[1]));
}

   
                                                                 
   
                                                                 
   
static bool
box_contain_lseg(BOX *box, LSEG *lseg)
{
  return box_contain_point(box, &lseg->p[0]) && box_contain_point(box, &lseg->p[1]);
}

Datum
on_sb(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  BOX *box = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_contain_lseg(box, lseg));
}

                                                                        
           
                                             
                                                                       

Datum
inter_sl(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  LINE *line = PG_GETARG_LINE_P(1);

  PG_RETURN_BOOL(lseg_interpt_line(NULL, lseg, line));
}

   
                                      
   
                                                         
                                                      
                                     
   
                                                                        
                                                                        
                                                                        
                                                  
   
                                                                         
                       
   
static bool
box_interpt_lseg(Point *result, BOX *box, LSEG *lseg)
{
  BOX lbox;
  LSEG bseg;
  Point point;

  lbox.low.x = float8_min(lseg->p[0].x, lseg->p[1].x);
  lbox.low.y = float8_min(lseg->p[0].y, lseg->p[1].y);
  lbox.high.x = float8_max(lseg->p[0].x, lseg->p[1].x);
  lbox.high.y = float8_max(lseg->p[0].y, lseg->p[1].y);

                                                             
  if (!box_ov(&lbox, box))
  {
    return false;
  }

  if (result != NULL)
  {
    box_cn(&point, box);
    lseg_closept_point(result, lseg, &point);
  }

                                                                     
  if (box_contain_point(box, &lseg->p[0]) || box_contain_point(box, &lseg->p[1]))
  {
    return true;
  }

                                         
  point.x = box->low.x;
  point.y = box->high.y;
  statlseg_construct(&bseg, &box->low, &point);
  if (lseg_interpt_lseg(NULL, &bseg, lseg))
  {
    return true;
  }

  statlseg_construct(&bseg, &box->high, &point);
  if (lseg_interpt_lseg(NULL, &bseg, lseg))
  {
    return true;
  }

  point.x = box->high.x;
  point.y = box->low.y;
  statlseg_construct(&bseg, &box->low, &point);
  if (lseg_interpt_lseg(NULL, &bseg, lseg))
  {
    return true;
  }

  statlseg_construct(&bseg, &box->high, &point);
  if (lseg_interpt_lseg(NULL, &bseg, lseg))
  {
    return true;
  }

                                                      
  return false;
}

Datum
inter_sb(PG_FUNCTION_ARGS)
{
  LSEG *lseg = PG_GETARG_LSEG_P(0);
  BOX *box = PG_GETARG_BOX_P(1);

  PG_RETURN_BOOL(box_interpt_lseg(NULL, box, lseg));
}

              
                              
   
Datum
inter_lb(PG_FUNCTION_ARGS)
{
  LINE *line = PG_GETARG_LINE_P(0);
  BOX *box = PG_GETARG_BOX_P(1);
  LSEG bseg;
  Point p1, p2;

                                         
  p1.x = box->low.x;
  p1.y = box->low.y;
  p2.x = box->low.x;
  p2.y = box->high.y;
  statlseg_construct(&bseg, &p1, &p2);
  if (lseg_interpt_line(NULL, &bseg, line))
  {
    PG_RETURN_BOOL(true);
  }
  p1.x = box->high.x;
  p1.y = box->high.y;
  statlseg_construct(&bseg, &p1, &p2);
  if (lseg_interpt_line(NULL, &bseg, line))
  {
    PG_RETURN_BOOL(true);
  }
  p2.x = box->high.x;
  p2.y = box->low.y;
  statlseg_construct(&bseg, &p1, &p2);
  if (lseg_interpt_line(NULL, &bseg, line))
  {
    PG_RETURN_BOOL(true);
  }
  p1.x = box->low.x;
  p1.y = box->low.y;
  statlseg_construct(&bseg, &p1, &p2);
  if (lseg_interpt_line(NULL, &bseg, line))
  {
    PG_RETURN_BOOL(true);
  }

                                              
  PG_RETURN_BOOL(false);
}

                                                                     
                                                                    
                                                                        
                             
   
                                                                  
                                                                      

                                                                        
                                                         
                                                                         
static void
make_bound_box(POLYGON *poly)
{
  int i;
  float8 x1, y1, x2, y2;

  Assert(poly->npts > 0);

  x1 = x2 = poly->p[0].x;
  y2 = y1 = poly->p[0].y;
  for (i = 1; i < poly->npts; i++)
  {
    if (float8_lt(poly->p[i].x, x1))
    {
      x1 = poly->p[i].x;
    }
    if (float8_gt(poly->p[i].x, x2))
    {
      x2 = poly->p[i].x;
    }
    if (float8_lt(poly->p[i].y, y1))
    {
      y1 = poly->p[i].y;
    }
    if (float8_gt(poly->p[i].y, y2))
    {
      y2 = poly->p[i].y;
    }
  }

  poly->boundbox.low.x = x1;
  poly->boundbox.high.x = x2;
  poly->boundbox.low.y = y1;
  poly->boundbox.high.y = y2;
}

                                                                     
                                                             
   
                     
                              
                        
                                                           
                                                                      
Datum
poly_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  POLYGON *poly;
  int npts;
  int size;
  int base_size;
  bool isopen;

  if ((npts = pair_count(str, ',')) <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "polygon", str)));
  }

  base_size = sizeof(poly->p[0]) * npts;
  size = offsetof(POLYGON, p) + base_size;

                                  
  if (base_size / npts != sizeof(poly->p[0]) || size <= base_size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many points requested")));
  }

  poly = (POLYGON *)palloc0(size);                     

  SET_VARSIZE(poly, size);
  poly->npts = npts;

  path_decode(str, false, npts, &(poly->p[0]), &isopen, NULL, "polygon", str);

  make_bound_box(poly);

  PG_RETURN_POLYGON_P(poly);
}

                                                                  
                                                             
                                                       
                                                                   
Datum
poly_out(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);

  PG_RETURN_CSTRING(path_encode(PATH_CLOSED, poly->npts, poly->p));
}

   
                                                             
   
                                                                      
                                                                    
                                                                   
                                          
   
Datum
poly_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  POLYGON *poly;
  int32 npts;
  int32 i;
  int size;

  npts = pq_getmsgint(buf, sizeof(int32));
  if (npts <= 0 || npts >= (int32)((INT_MAX - offsetof(POLYGON, p)) / sizeof(Point)))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid number of points in external \"polygon\" value")));
  }

  size = offsetof(POLYGON, p) + sizeof(poly->p[0]) * npts;
  poly = (POLYGON *)palloc0(size);                     

  SET_VARSIZE(poly, size);
  poly->npts = npts;

  for (i = 0; i < npts; i++)
  {
    poly->p[i].x = pq_getmsgfloat8(buf);
    poly->p[i].y = pq_getmsgfloat8(buf);
  }

  make_bound_box(poly);

  PG_RETURN_POLYGON_P(poly);
}

   
                                                    
   
Datum
poly_send(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  StringInfoData buf;
  int32 i;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, poly->npts);
  for (i = 0; i < poly->npts; i++)
  {
    pq_sendfloat8(&buf, poly->p[i].x);
    pq_sendfloat8(&buf, poly->p[i].y);
  }
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                          
                                                    
                                                         
         
                                                           
Datum
poly_left(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.high.x < polyb->boundbox.low.x;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                          
                                                                
         
                                                           
Datum
poly_overleft(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.high.x <= polyb->boundbox.high.x;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                     
                                                          
         
                                                           
Datum
poly_right(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.low.x > polyb->boundbox.high.x;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                           
                                                               
         
                                                           
Datum
poly_overright(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.low.x >= polyb->boundbox.low.x;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                  
                                                        
         
                                                           
Datum
poly_below(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.high.y < polyb->boundbox.low.y;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                        
                                                              
         
                                                           
Datum
poly_overbelow(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.high.y <= polyb->boundbox.high.y;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                  
                                                        
         
                                                           
Datum
poly_above(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.low.y > polyb->boundbox.high.y;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                        
                                                              
         
                                                           
Datum
poly_overabove(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = polya->boundbox.low.y >= polyb->boundbox.low.y;

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                          
                                                        
                    
                                                            
                                                        
                  
                                                           
Datum
poly_same(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  if (polya->npts != polyb->npts)
  {
    result = false;
  }
  else
  {
    result = plist_same(polya->npts, polya->p, polyb->p);
  }

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                                    
                                             
                                                                     
Datum
poly_overlap(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  Assert(polya->npts > 0 && polyb->npts > 0);

                                   
  result = box_ov(&polya->boundbox, &polyb->boundbox);

     
                                                                       
                                                                           
                                      
     
  if (result)
  {
    int ia, ib;
    LSEG sa, sb;

                                                    
    sa.p[0] = polya->p[polya->npts - 1];
    result = false;

    for (ia = 0; ia < polya->npts && !result; ia++)
    {
                                                         
      sa.p[1] = polya->p[ia];

                                                      
      sb.p[0] = polyb->p[polyb->npts - 1];

      for (ib = 0; ib < polyb->npts && !result; ib++)
      {
        sb.p[1] = polyb->p[ib];
        result = lseg_interpt_lseg(NULL, &sa, &sb);
        sb.p[0] = sb.p[1];
      }

         
                                                               
         
      sa.p[0] = sa.p[1];
    }

    if (!result)
    {
      result = (point_inside(polya->p, polyb->npts, polyb->p) || point_inside(polyb->p, polya->npts, polya->p));
    }
  }

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

   
                                                        
                       
                                    
                                                
                    
                                                             
                                                            
                                      
   

static bool
touched_lseg_inside_poly(Point *a, Point *b, LSEG *s, POLYGON *poly, int start)
{
                                 
  LSEG t;

  t.p[0] = *a;
  t.p[1] = *b;

  if (point_eq_point(a, s->p))
  {
    if (lseg_contain_point(&t, s->p + 1))
    {
      return lseg_inside_poly(b, s->p + 1, poly, start);
    }
  }
  else if (point_eq_point(a, s->p + 1))
  {
    if (lseg_contain_point(&t, s->p))
    {
      return lseg_inside_poly(b, s->p, poly, start);
    }
  }
  else if (lseg_contain_point(&t, s->p))
  {
    return lseg_inside_poly(b, s->p, poly, start);
  }
  else if (lseg_contain_point(&t, s->p + 1))
  {
    return lseg_inside_poly(b, s->p + 1, poly, start);
  }

  return true;                                                 
}

   
                                                       
                                                    
                                       
   
static bool
lseg_inside_poly(Point *a, Point *b, POLYGON *poly, int start)
{
  LSEG s, t;
  int i;
  bool res = true, intersection = false;

                                                                          
  check_stack_depth();

  t.p[0] = *a;
  t.p[1] = *b;
  s.p[0] = poly->p[(start == 0) ? (poly->npts - 1) : (start - 1)];

  for (i = start; i < poly->npts && res; i++)
  {
    Point interpt;

    CHECK_FOR_INTERRUPTS();

    s.p[1] = poly->p[i];

    if (lseg_contain_point(&s, t.p))
    {
      if (lseg_contain_point(&s, t.p + 1))
      {
        return true;                          
      }

                   
      res = touched_lseg_inside_poly(t.p, t.p + 1, &s, poly, i + 1);
    }
    else if (lseg_contain_point(&s, t.p + 1))
    {
                   
      res = touched_lseg_inside_poly(t.p + 1, t.p, &s, poly, i + 1);
    }
    else if (lseg_interpt_lseg(&interpt, &t, &s))
    {
         
                                                              
         

      intersection = true;
      res = lseg_inside_poly(t.p, &interpt, poly, i + 1);
      if (res)
      {
        res = lseg_inside_poly(t.p + 1, &interpt, poly, i + 1);
      }
    }

    s.p[0] = s.p[1];
  }

  if (res && !intersection)
  {
    Point p;

       
                                                                          
                                                                  
       
    p.x = float8_div(float8_pl(t.p[0].x, t.p[1].x), 2.0);
    p.y = float8_div(float8_pl(t.p[0].y, t.p[1].y), 2.0);

    res = point_inside(&p, poly->npts, poly->p);
  }

  return res;
}

   
                                                       
   
static bool
poly_contain_poly(POLYGON *contains_poly, POLYGON *contained_poly)
{
  int i;
  LSEG s;

  Assert(contains_poly->npts > 0 && contained_poly->npts > 0);

     
                                                                    
                   
     
  if (!box_contain_box(&contains_poly->boundbox, &contained_poly->boundbox))
  {
    return false;
  }

  s.p[0] = contained_poly->p[contained_poly->npts - 1];

  for (i = 0; i < contained_poly->npts; i++)
  {
    s.p[1] = contained_poly->p[i];
    if (!lseg_inside_poly(s.p, s.p + 1, contains_poly, 0))
    {
      return false;
    }
    s.p[0] = s.p[1];
  }

  return true;
}

Datum
poly_contain(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

  result = poly_contain_poly(polya, polyb);

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

                                                                    
                                                    
                                                                     
Datum
poly_contained(PG_FUNCTION_ARGS)
{
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
  bool result;

                                                                 
  result = poly_contain_poly(polyb, polya);

     
                                                                          
     
  PG_FREE_IF_COPY(polya, 0);
  PG_FREE_IF_COPY(polyb, 1);

  PG_RETURN_BOOL(result);
}

Datum
poly_contain_pt(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  Point *p = PG_GETARG_POINT_P(1);

  PG_RETURN_BOOL(point_inside(p, poly->npts, poly->p) != 0);
}

Datum
pt_contained_poly(PG_FUNCTION_ARGS)
{
  Point *p = PG_GETARG_POINT_P(0);
  POLYGON *poly = PG_GETARG_POLYGON_P(1);

  PG_RETURN_BOOL(point_inside(p, poly->npts, poly->p) != 0);
}

Datum
poly_distance(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
  POLYGON *polya = PG_GETARG_POLYGON_P(0);
  POLYGON *polyb = PG_GETARG_POLYGON_P(1);
#endif

  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function \"poly_distance\" not implemented")));

  PG_RETURN_NULL();
}

                                                                         
    
                             
    
                                                                         

Datum
construct_point(PG_FUNCTION_ARGS)
{
  float8 x = PG_GETARG_FLOAT8(0);
  float8 y = PG_GETARG_FLOAT8(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  point_construct(result, x, y);

  PG_RETURN_POINT_P(result);
}

static inline void
point_add_point(Point *result, Point *pt1, Point *pt2)
{
  point_construct(result, float8_pl(pt1->x, pt2->x), float8_pl(pt1->y, pt2->y));
}

Datum
point_add(PG_FUNCTION_ARGS)
{
  Point *p1 = PG_GETARG_POINT_P(0);
  Point *p2 = PG_GETARG_POINT_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  point_add_point(result, p1, p2);

  PG_RETURN_POINT_P(result);
}

static inline void
point_sub_point(Point *result, Point *pt1, Point *pt2)
{
  point_construct(result, float8_mi(pt1->x, pt2->x), float8_mi(pt1->y, pt2->y));
}

Datum
point_sub(PG_FUNCTION_ARGS)
{
  Point *p1 = PG_GETARG_POINT_P(0);
  Point *p2 = PG_GETARG_POINT_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  point_sub_point(result, p1, p2);

  PG_RETURN_POINT_P(result);
}

static inline void
point_mul_point(Point *result, Point *pt1, Point *pt2)
{
  point_construct(result, float8_mi(float8_mul(pt1->x, pt2->x), float8_mul(pt1->y, pt2->y)), float8_pl(float8_mul(pt1->x, pt2->y), float8_mul(pt1->y, pt2->x)));
}

Datum
point_mul(PG_FUNCTION_ARGS)
{
  Point *p1 = PG_GETARG_POINT_P(0);
  Point *p2 = PG_GETARG_POINT_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  point_mul_point(result, p1, p2);

  PG_RETURN_POINT_P(result);
}

static inline void
point_div_point(Point *result, Point *pt1, Point *pt2)
{
  float8 div;

  div = float8_pl(float8_mul(pt2->x, pt2->x), float8_mul(pt2->y, pt2->y));

  point_construct(result, float8_div(float8_pl(float8_mul(pt1->x, pt2->x), float8_mul(pt1->y, pt2->y)), div), float8_div(float8_mi(float8_mul(pt1->y, pt2->x), float8_mul(pt1->x, pt2->y)), div));
}

Datum
point_div(PG_FUNCTION_ARGS)
{
  Point *p1 = PG_GETARG_POINT_P(0);
  Point *p2 = PG_GETARG_POINT_P(1);
  Point *result;

  result = (Point *)palloc(sizeof(Point));

  point_div_point(result, p1, p2);

  PG_RETURN_POINT_P(result);
}

                                                                         
    
                            
    
                                                                         

Datum
points_box(PG_FUNCTION_ARGS)
{
  Point *p1 = PG_GETARG_POINT_P(0);
  Point *p2 = PG_GETARG_POINT_P(1);
  BOX *result;

  result = (BOX *)palloc(sizeof(BOX));

  box_construct(result, p1, p2);

  PG_RETURN_BOX_P(result);
}

Datum
box_add(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *p = PG_GETARG_POINT_P(1);
  BOX *result;

  result = (BOX *)palloc(sizeof(BOX));

  point_add_point(&result->high, &box->high, p);
  point_add_point(&result->low, &box->low, p);

  PG_RETURN_BOX_P(result);
}

Datum
box_sub(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *p = PG_GETARG_POINT_P(1);
  BOX *result;

  result = (BOX *)palloc(sizeof(BOX));

  point_sub_point(&result->high, &box->high, p);
  point_sub_point(&result->low, &box->low, p);

  PG_RETURN_BOX_P(result);
}

Datum
box_mul(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *p = PG_GETARG_POINT_P(1);
  BOX *result;
  Point high, low;

  result = (BOX *)palloc(sizeof(BOX));

  point_mul_point(&high, &box->high, p);
  point_mul_point(&low, &box->low, p);

  box_construct(result, &high, &low);

  PG_RETURN_BOX_P(result);
}

Datum
box_div(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  Point *p = PG_GETARG_POINT_P(1);
  BOX *result;
  Point high, low;

  result = (BOX *)palloc(sizeof(BOX));

  point_div_point(&high, &box->high, p);
  point_div_point(&low, &box->low, p);

  box_construct(result, &high, &low);

  PG_RETURN_BOX_P(result);
}

   
                              
   
Datum
point_box(PG_FUNCTION_ARGS)
{
  Point *pt = PG_GETARG_POINT_P(0);
  BOX *box;

  box = (BOX *)palloc(sizeof(BOX));

  box->high.x = pt->x;
  box->low.x = pt->x;
  box->high.y = pt->y;
  box->low.y = pt->y;

  PG_RETURN_BOX_P(box);
}

   
                                                               
   
Datum
boxes_bound_box(PG_FUNCTION_ARGS)
{
  BOX *box1 = PG_GETARG_BOX_P(0), *box2 = PG_GETARG_BOX_P(1), *container;

  container = (BOX *)palloc(sizeof(BOX));

  container->high.x = float8_max(box1->high.x, box2->high.x);
  container->low.x = float8_min(box1->low.x, box2->low.x);
  container->high.y = float8_max(box1->high.y, box2->high.y);
  container->low.y = float8_min(box1->low.y, box2->low.y);

  PG_RETURN_BOX_P(container);
}

                                                                         
    
                            
    
                                                                         

              
                                                       
   
Datum
path_add(PG_FUNCTION_ARGS)
{
  PATH *p1 = PG_GETARG_PATH_P(0);
  PATH *p2 = PG_GETARG_PATH_P(1);
  PATH *result;
  int size, base_size;
  int i;

  if (p1->closed || p2->closed)
  {
    PG_RETURN_NULL();
  }

  base_size = sizeof(p1->p[0]) * (p1->npts + p2->npts);
  size = offsetof(PATH, p) + base_size;

                                  
  if (base_size / sizeof(p1->p[0]) != (p1->npts + p2->npts) || size <= base_size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many points requested")));
  }

  result = (PATH *)palloc(size);

  SET_VARSIZE(result, size);
  result->npts = (p1->npts + p2->npts);
  result->closed = p1->closed;
                                               
  result->dummy = 0;

  for (i = 0; i < p1->npts; i++)
  {
    result->p[i].x = p1->p[i].x;
    result->p[i].y = p1->p[i].y;
  }
  for (i = 0; i < p2->npts; i++)
  {
    result->p[i + p1->npts].x = p2->p[i].x;
    result->p[i + p1->npts].y = p2->p[i].y;
  }

  PG_RETURN_PATH_P(result);
}

                 
                          
   
Datum
path_add_pt(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);
  Point *point = PG_GETARG_POINT_P(1);
  int i;

  for (i = 0; i < path->npts; i++)
  {
    point_add_point(&path->p[i], &path->p[i], point);
  }

  PG_RETURN_PATH_P(path);
}

Datum
path_sub_pt(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);
  Point *point = PG_GETARG_POINT_P(1);
  int i;

  for (i = 0; i < path->npts; i++)
  {
    point_sub_point(&path->p[i], &path->p[i], point);
  }

  PG_RETURN_PATH_P(path);
}

                 
                                   
   
Datum
path_mul_pt(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);
  Point *point = PG_GETARG_POINT_P(1);
  int i;

  for (i = 0; i < path->npts; i++)
  {
    point_mul_point(&path->p[i], &path->p[i], point);
  }

  PG_RETURN_PATH_P(path);
}

Datum
path_div_pt(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P_COPY(0);
  Point *point = PG_GETARG_POINT_P(1);
  int i;

  for (i = 0; i < path->npts; i++)
  {
    point_div_point(&path->p[i], &path->p[i], point);
  }

  PG_RETURN_PATH_P(path);
}

Datum
path_center(PG_FUNCTION_ARGS)
{
#ifdef NOT_USED
  PATH *path = PG_GETARG_PATH_P(0);
#endif

  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("function \"path_center\" not implemented")));

  PG_RETURN_NULL();
}

Datum
path_poly(PG_FUNCTION_ARGS)
{
  PATH *path = PG_GETARG_PATH_P(0);
  POLYGON *poly;
  int size;
  int i;

                                                                           
  if (!path->closed)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("open path cannot be converted to polygon")));
  }

     
                                                                            
                                   
     
  size = offsetof(POLYGON, p) + sizeof(poly->p[0]) * path->npts;
  poly = (POLYGON *)palloc(size);

  SET_VARSIZE(poly, size);
  poly->npts = path->npts;

  for (i = 0; i < path->npts; i++)
  {
    poly->p[i].x = path->p[i].x;
    poly->p[i].y = path->p[i].y;
  }

  make_bound_box(poly);

  PG_RETURN_POLYGON_P(poly);
}

                                                                         
    
                               
    
                                                                         

Datum
poly_npoints(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);

  PG_RETURN_INT32(poly->npts);
}

Datum
poly_center(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  Point *result;
  CIRCLE circle;

  result = (Point *)palloc(sizeof(Point));

  poly_to_circle(&circle, poly);
  *result = circle.center;

  PG_RETURN_POINT_P(result);
}

Datum
poly_box(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  BOX *box;

  box = (BOX *)palloc(sizeof(BOX));
  *box = poly->boundbox;

  PG_RETURN_BOX_P(box);
}

              
                               
   
Datum
box_poly(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  POLYGON *poly;
  int size;

                                                
  size = offsetof(POLYGON, p) + sizeof(poly->p[0]) * 4;
  poly = (POLYGON *)palloc(size);

  SET_VARSIZE(poly, size);
  poly->npts = 4;

  poly->p[0].x = box->low.x;
  poly->p[0].y = box->low.y;
  poly->p[1].x = box->low.x;
  poly->p[1].y = box->high.y;
  poly->p[2].x = box->high.x;
  poly->p[2].y = box->high.y;
  poly->p[3].x = box->high.x;
  poly->p[3].y = box->low.y;

  box_construct(&poly->boundbox, &box->high, &box->low);

  PG_RETURN_POLYGON_P(poly);
}

Datum
poly_path(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  PATH *path;
  int size;
  int i;

     
                                                                            
                                  
     
  size = offsetof(PATH, p) + sizeof(path->p[0]) * poly->npts;
  path = (PATH *)palloc(size);

  SET_VARSIZE(path, size);
  path->npts = poly->npts;
  path->closed = true;
                                               
  path->dummy = 0;

  for (i = 0; i < poly->npts; i++)
  {
    path->p[i].x = poly->p[i].x;
    path->p[i].y = poly->p[i].y;
  }

  PG_RETURN_PATH_P(path);
}

                                                                         
    
                           
    
                                                                         

                                                             
                                       
                                                             

                                                     
   
                                                   
                     
                                                 
   
Datum
circle_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  CIRCLE *circle = (CIRCLE *)palloc(sizeof(CIRCLE));
  char *s, *cp;
  int depth = 0;

  s = str;
  while (isspace((unsigned char)*s))
  {
    s++;
  }
  if (*s == LDELIM_C)
  {
    depth++, s++;
  }
  else if (*s == LDELIM)
  {
                                                             
    cp = (s + 1);
    while (isspace((unsigned char)*cp))
    {
      cp++;
    }
    if (*cp == LDELIM)
    {
      depth++, s = cp;
    }
  }

                                                               
  pair_decode(s, &circle->center.x, &circle->center.y, &s, "circle", str);

  if (*s == DELIM)
  {
    s++;
  }

  circle->radius = single_decode(s, &s, "circle", str);
                              
  if (circle->radius < 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "circle", str)));
  }

  while (depth > 0)
  {
    if ((*s == RDELIM) || ((*s == RDELIM_C) && (depth == 1)))
    {
      depth--;
      s++;
      while (isspace((unsigned char)*s))
      {
        s++;
      }
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "circle", str)));
    }
  }

  if (*s != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "circle", str)));
  }

  PG_RETURN_CIRCLE_P(circle);
}

                                                      
   
Datum
circle_out(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  StringInfoData str;

  initStringInfo(&str);

  appendStringInfoChar(&str, LDELIM_C);
  appendStringInfoChar(&str, LDELIM);
  pair_encode(circle->center.x, circle->center.y, &str);
  appendStringInfoChar(&str, RDELIM);
  appendStringInfoChar(&str, DELIM);
  single_encode(circle->radius, &str);
  appendStringInfoChar(&str, RDELIM_C);

  PG_RETURN_CSTRING(str.data);
}

   
                                                              
   
Datum
circle_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  CIRCLE *circle;

  circle = (CIRCLE *)palloc(sizeof(CIRCLE));

  circle->center.x = pq_getmsgfloat8(buf);
  circle->center.y = pq_getmsgfloat8(buf);
  circle->radius = pq_getmsgfloat8(buf);

                              
  if (circle->radius < 0.0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid radius in external \"circle\" value")));
  }

  PG_RETURN_CIRCLE_P(circle);
}

   
                                                     
   
Datum
circle_send(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendfloat8(&buf, circle->center.x);
  pq_sendfloat8(&buf, circle->center.y);
  pq_sendfloat8(&buf, circle->radius);
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                             
                                     
                                                   
                                                             

                       
   
                                                                          
                
   
Datum
circle_same(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(((isnan(circle1->radius) && isnan(circle2->radius)) || FPeq(circle1->radius, circle2->radius)) && point_eq_point(&circle1->center, &circle2->center));
}

                                                    
   
Datum
circle_overlap(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(point_dt(&circle1->center, &circle2->center), float8_pl(circle1->radius, circle2->radius)));
}

                                                                  
                                     
   
Datum
circle_overleft(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(float8_pl(circle1->center.x, circle1->radius), float8_pl(circle2->center.x, circle2->radius)));
}

                                                         
   
Datum
circle_left(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPlt(float8_pl(circle1->center.x, circle1->radius), float8_mi(circle2->center.x, circle2->radius)));
}

                                                          
   
Datum
circle_right(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPgt(float8_mi(circle1->center.x, circle1->radius), float8_pl(circle2->center.x, circle2->radius)));
}

                                                                  
                                    
   
Datum
circle_overright(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPge(float8_mi(circle1->center.x, circle1->radius), float8_mi(circle2->center.x, circle2->radius)));
}

                                                          
   
Datum
circle_contained(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(point_dt(&circle1->center, &circle2->center), float8_mi(circle2->radius, circle1->radius)));
}

                                                    
   
Datum
circle_contain(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(point_dt(&circle1->center, &circle2->center), float8_mi(circle1->radius, circle2->radius)));
}

                                                        
   
Datum
circle_below(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPlt(float8_pl(circle1->center.y, circle1->radius), float8_mi(circle2->center.y, circle2->radius)));
}

                                                       
   
Datum
circle_above(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPgt(float8_mi(circle1->center.y, circle1->radius), float8_pl(circle2->center.y, circle2->radius)));
}

                                                                 
                                     
   
Datum
circle_overbelow(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(float8_pl(circle1->center.y, circle1->radius), float8_pl(circle2->center.y, circle2->radius)));
}

                                                                
                                     
   
Datum
circle_overabove(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPge(float8_mi(circle1->center.y, circle1->radius), float8_mi(circle2->center.y, circle2->radius)));
}

                                                                 
                                   
   
Datum
circle_eq(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPeq(circle_ar(circle1), circle_ar(circle2)));
}

Datum
circle_ne(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPne(circle_ar(circle1), circle_ar(circle2)));
}

Datum
circle_lt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPlt(circle_ar(circle1), circle_ar(circle2)));
}

Datum
circle_gt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPgt(circle_ar(circle1), circle_ar(circle2)));
}

Datum
circle_le(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPle(circle_ar(circle1), circle_ar(circle2)));
}

Datum
circle_ge(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);

  PG_RETURN_BOOL(FPge(circle_ar(circle1), circle_ar(circle2)));
}

                                                             
                                      
                                                             

                   
                         
   
Datum
circle_add_pt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  point_add_point(&result->center, &circle->center, point);
  result->radius = circle->radius;

  PG_RETURN_CIRCLE_P(result);
}

Datum
circle_sub_pt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  point_sub_point(&result->center, &circle->center, point);
  result->radius = circle->radius;

  PG_RETURN_CIRCLE_P(result);
}

                   
                                   
   
Datum
circle_mul_pt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  point_mul_point(&result->center, &circle->center, point);
  result->radius = float8_mul(circle->radius, HYPOT(point->x, point->y));

  PG_RETURN_CIRCLE_P(result);
}

Datum
circle_div_pt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  point_div_point(&result->center, &circle->center, point);
  result->radius = float8_div(circle->radius, HYPOT(point->x, point->y));

  PG_RETURN_CIRCLE_P(result);
}

                                                    
   
Datum
circle_area(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);

  PG_RETURN_FLOAT8(circle_ar(circle));
}

                                                           
   
Datum
circle_diameter(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);

  PG_RETURN_FLOAT8(float8_mul(circle->radius, 2.0));
}

                                                       
   
Datum
circle_radius(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);

  PG_RETURN_FLOAT8(circle->radius);
}

                                                    
                         
   
Datum
circle_distance(PG_FUNCTION_ARGS)
{
  CIRCLE *circle1 = PG_GETARG_CIRCLE_P(0);
  CIRCLE *circle2 = PG_GETARG_CIRCLE_P(1);
  float8 result;

  result = float8_mi(point_dt(&circle1->center, &circle2->center), float8_pl(circle1->radius, circle2->radius));
  if (result < 0.0)
  {
    result = 0.0;
  }

  PG_RETURN_FLOAT8(result);
}

Datum
circle_contain_pt(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  float8 d;

  d = point_dt(&circle->center, point);
  PG_RETURN_BOOL(d <= circle->radius);
}

Datum
pt_contained_circle(PG_FUNCTION_ARGS)
{
  Point *point = PG_GETARG_POINT_P(0);
  CIRCLE *circle = PG_GETARG_CIRCLE_P(1);
  float8 d;

  d = point_dt(&circle->center, point);
  PG_RETURN_BOOL(d <= circle->radius);
}

                                            
                                
   
Datum
dist_pc(PG_FUNCTION_ARGS)
{
  Point *point = PG_GETARG_POINT_P(0);
  CIRCLE *circle = PG_GETARG_CIRCLE_P(1);
  float8 result;

  result = float8_mi(point_dt(point, &circle->center), circle->radius);
  if (result < 0.0)
  {
    result = 0.0;
  }

  PG_RETURN_FLOAT8(result);
}

   
                                     
   
Datum
dist_cpoint(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *point = PG_GETARG_POINT_P(1);
  float8 result;

  result = float8_mi(point_dt(point, &circle->center), circle->radius);
  if (result < 0.0)
  {
    result = 0.0;
  }

  PG_RETURN_FLOAT8(result);
}

                                                             
   
Datum
circle_center(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  Point *result;

  result = (Point *)palloc(sizeof(Point));
  result->x = circle->center.x;
  result->y = circle->center.y;

  PG_RETURN_POINT_P(result);
}

                                                  
   
static float8
circle_ar(CIRCLE *circle)
{
  return float8_mul(float8_mul(circle->radius, circle->radius), M_PI);
}

                                                             
                         
                                                             

Datum
cr_circle(PG_FUNCTION_ARGS)
{
  Point *center = PG_GETARG_POINT_P(0);
  float8 radius = PG_GETARG_FLOAT8(1);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  result->center.x = center->x;
  result->center.y = center->y;
  result->radius = radius;

  PG_RETURN_CIRCLE_P(result);
}

Datum
circle_box(PG_FUNCTION_ARGS)
{
  CIRCLE *circle = PG_GETARG_CIRCLE_P(0);
  BOX *box;
  float8 delta;

  box = (BOX *)palloc(sizeof(BOX));

  delta = float8_div(circle->radius, sqrt(2.0));

  box->high.x = float8_pl(circle->center.x, delta);
  box->low.x = float8_mi(circle->center.x, delta);
  box->high.y = float8_pl(circle->center.y, delta);
  box->low.y = float8_mi(circle->center.y, delta);

  PG_RETURN_BOX_P(box);
}

                
                              
   
Datum
box_circle(PG_FUNCTION_ARGS)
{
  BOX *box = PG_GETARG_BOX_P(0);
  CIRCLE *circle;

  circle = (CIRCLE *)palloc(sizeof(CIRCLE));

  circle->center.x = float8_div(float8_pl(box->high.x, box->low.x), 2.0);
  circle->center.y = float8_div(float8_pl(box->high.y, box->low.y), 2.0);

  circle->radius = point_dt(&circle->center, &box->high);

  PG_RETURN_CIRCLE_P(circle);
}

Datum
circle_poly(PG_FUNCTION_ARGS)
{
  int32 npts = PG_GETARG_INT32(0);
  CIRCLE *circle = PG_GETARG_CIRCLE_P(1);
  POLYGON *poly;
  int base_size, size;
  int i;
  float8 angle;
  float8 anglestep;

  if (FPzero(circle->radius))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert circle with radius zero to polygon")));
  }

  if (npts < 2)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("must request at least 2 points")));
  }

  base_size = sizeof(poly->p[0]) * npts;
  size = offsetof(POLYGON, p) + base_size;

                                  
  if (base_size / npts != sizeof(poly->p[0]) || size <= base_size)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("too many points requested")));
  }

  poly = (POLYGON *)palloc0(size);                     
  SET_VARSIZE(poly, size);
  poly->npts = npts;

  anglestep = float8_div(2.0 * M_PI, npts);

  for (i = 0; i < npts; i++)
  {
    angle = float8_mul(anglestep, i);

    poly->p[i].x = float8_mi(circle->center.x, float8_mul(circle->radius, cos(angle)));
    poly->p[i].y = float8_pl(circle->center.y, float8_mul(circle->radius, sin(angle)));
  }

  make_bound_box(poly);

  PG_RETURN_POLYGON_P(poly);
}

   
                             
   
                                    
   
                                                                 
                                                                 
   
static void
poly_to_circle(CIRCLE *result, POLYGON *poly)
{
  int i;

  Assert(poly->npts > 0);

  result->center.x = 0;
  result->center.y = 0;
  result->radius = 0;

  for (i = 0; i < poly->npts; i++)
  {
    point_add_point(&result->center, &result->center, &poly->p[i]);
  }
  result->center.x = float8_div(result->center.x, poly->npts);
  result->center.y = float8_div(result->center.y, poly->npts);

  for (i = 0; i < poly->npts; i++)
  {
    result->radius = float8_pl(result->radius, point_dt(&poly->p[i], &result->center));
  }
  result->radius = float8_div(result->radius, poly->npts);
}

Datum
poly_circle(PG_FUNCTION_ARGS)
{
  POLYGON *poly = PG_GETARG_POLYGON_P(0);
  CIRCLE *result;

  result = (CIRCLE *)palloc(sizeof(CIRCLE));

  poly_to_circle(result, poly);

  PG_RETURN_CIRCLE_P(result);
}

                                                                         
    
                                          
    
                                                                         

   
                                                                        
                                
                                                                    
                       
                                   
                                                
                                                                       
                                                     
   

#define POINT_ON_POLYGON INT_MAX

static int
point_inside(Point *p, int npts, Point *plist)
{
  float8 x0, y0;
  float8 prev_x, prev_y;
  int i = 0;
  float8 x, y;
  int cross, total_cross = 0;

  Assert(npts > 0);

                                                            
  x0 = float8_mi(plist[0].x, p->x);
  y0 = float8_mi(plist[0].y, p->y);

  prev_x = x0;
  prev_y = y0;
                                                          
  for (i = 1; i < npts; i++)
  {
                                                             
    x = float8_mi(plist[i].x, p->x);
    y = float8_mi(plist[i].y, p->y);

                                                    
    if ((cross = lseg_crossing(x, y, prev_x, prev_y)) == POINT_ON_POLYGON)
    {
      return 2;
    }
    total_cross += cross;

    prev_x = x;
    prev_y = y;
  }

                              
  if ((cross = lseg_crossing(x0, y0, prev_x, prev_y)) == POINT_ON_POLYGON)
  {
    return 2;
  }
  total_cross += cross;

  if (total_cross != 0)
  {
    return 1;
  }
  return 0;
}

                   
                                                                                
                                                        
                                                                                 
                                                           
                                                                          
                                           
   

static int
lseg_crossing(float8 x, float8 y, float8 prev_x, float8 prev_y)
{
  float8 z;
  int y_sign;

  if (FPzero(y))
  {                                       
    if (FPzero(x))                      
    {
      return POINT_ON_POLYGON;
    }
    else if (FPgt(x, 0))
    {                                
      if (FPzero(prev_y))                            
      {
                         
        return FPgt(prev_x, 0.0) ? 0 : POINT_ON_POLYGON;
      }
      return FPlt(prev_y, 0.0) ? 1 : -1;
    }
    else
    {                                      
      if (FPzero(prev_y))
      {
                         
        return FPlt(prev_x, 0.0) ? 0 : POINT_ON_POLYGON;
      }
      return 0;
    }
  }
  else
  {             
                                                          
    y_sign = FPgt(y, 0.0) ? 1 : -1;

    if (FPzero(prev_y))
    {
                                                                          
      return FPlt(prev_x, 0.0) ? 0 : y_sign;
    }
    else if ((y_sign < 0 && FPlt(prev_y, 0.0)) || (y_sign > 0 && FPgt(prev_y, 0.0)))
    {
                                      
      return 0;                
    }
    else
    {                                
      if (FPge(x, 0.0) && FPgt(prev_x, 0.0))
      {
                                                        
        return 2 * y_sign;
      }
      if (FPlt(x, 0.0) && FPle(prev_x, 0.0))
      {
                                                               
        return 0;
      }

                                                            
      z = float8_mi(float8_mul(float8_mi(x, prev_x), y), float8_mul(float8_mi(y, prev_y), x));
      if (FPzero(z))
      {
        return POINT_ON_POLYGON;
      }
      if ((y_sign < 0 && FPlt(z, 0.0)) || (y_sign > 0 && FPgt(z, 0.0)))
      {
        return 0;
      }
      return 2 * y_sign;
    }
  }
}

static bool
plist_same(int npts, Point *p1, Point *p2)
{
  int i, ii, j;

                                  
  for (i = 0; i < npts; i++)
  {
    if (point_eq_point(&p2[i], &p1[0]))
    {

                                                                   
      for (ii = 1, j = i + 1; ii < npts; ii++, j++)
      {
        if (j >= npts)
        {
          j = 0;
        }
        if (!point_eq_point(&p2[j], &p1[ii]))
        {
          break;
        }
      }
      if (ii == npts)
      {
        return true;
      }

                                                         
      for (ii = 1, j = i - 1; ii < npts; ii++, j--)
      {
        if (j < 0)
        {
          j = (npts - 1);
        }
        if (!point_eq_point(&p2[j], &p1[ii]))
        {
          break;
        }
      }
      if (ii == npts)
      {
        return true;
      }
    }
  }

  return false;
}

                                                                            
                             
   
                                                                     
                                                                        
                                                                     
                                                                        
                                                                         
                          
   
                                                 
                                  
                                    
   
                                                                         
                         
   
                                                                          
                                                       
                                                                          
   
float8
pg_hypot(float8 x, float8 y)
{
  float8 yx, result;

                                   
  if (isinf(x) || isinf(y))
  {
    return get_float8_infinity();
  }

  if (isnan(x) || isnan(y))
  {
    return get_float8_nan();
  }

                                  
  x = fabs(x);
  y = fabs(y);

                                                       
  if (x < y)
  {
    float8 temp = x;

    x = y;
    y = temp;
  }

     
                                                                         
                                                               
                                              
     
  if (y == 0.0)
  {
    return x;
  }

                                
  yx = y / x;
  result = x * sqrt(1.0 + (yx * yx));

  if (unlikely(isinf(result)))
  {
    float_overflow_error();
  }
  if (unlikely(result == 0.0))
  {
    float_underflow_error();
  }

  return result;
}
