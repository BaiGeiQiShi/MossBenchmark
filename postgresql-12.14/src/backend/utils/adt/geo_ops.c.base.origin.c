/*-------------------------------------------------------------------------
 *
 * geo_ops.c
 *	  2D geometric operations
 *
 * This module implements the geometric functions and operators.  The
 * geometric types are (from simple to more complicated):
 *
 * - point
 * - line
 * - line segment
 * - box
 * - circle
 * - polygon
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/geo_ops.c
 *
 *-------------------------------------------------------------------------
 */
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

/*
 * * Type constructors have this form:
 *   void type_construct(Type *result, ...);
 *
 * * Operators commonly have signatures such as
 *   void type1_operator_type2(Type *result, Type1 *obj1, Type2 *obj2);
 *
 * Common operators are:
 * * Intersection point:
 *   bool type1_interpt_type2(Point *result, Type1 *obj1, Type2 *obj2);
 *		Return whether the two objects intersect. If *result is not
 *NULL, it is set to the intersection point.
 *
 * * Containment:
 *   bool type1_contain_type2(Type1 *obj1, Type2 *obj2);
 *		Return whether obj1 contains obj2.
 *   bool type1_contain_type2(Type1 *contains_obj, Type1 *contained_obj);
 *		Return whether obj1 contains obj2 (used when types are the same)
 *
 * * Distance of closest point in or on obj1 to obj2:
 *   float8 type1_closept_type2(Point *result, Type1 *obj1, Type2 *obj2);
 *		Returns the shortest distance between two objects.  If *result
 *is not NULL, it is set to the closest point in or on obj1 to obj2.
 *
 * These functions may be used to implement multiple SQL-level operators.  For
 * example, determining whether two lines are parallel is done by checking
 * whether they don't intersect.
 */

/*
 * Internal routines
 */

enum path_delim
{
  PATH_NONE,
  PATH_OPEN,
  PATH_CLOSED
};

/* Routines for points */
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

/* Routines for lines */
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

/* Routines for line segments */
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

/* Routines for boxes */
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

/* Routines for circles */
static float8
circle_ar(CIRCLE *circle);

/* Routines for polygons */
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

/* Routines for encoding and decoding */
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

/*
 * Delimiters for input and output strings.
 * LDELIM, RDELIM, and DELIM are left, right, and separator delimiters,
 * respectively. LDELIM_EP, RDELIM_EP are left and right delimiters for paths
 * with endpoints.
 */

#define LDELIM '('
#define RDELIM ')'
#define DELIM ','
#define LDELIM_EP '['
#define RDELIM_EP ']'
#define LDELIM_C '<'
#define RDELIM_C '>'
#define LDELIM_L '{'
#define RDELIM_L '}'

/*
 * Geometric data types are composed of points.
 * This code tries to support a common format throughout the data types,
 *	to allow for more predictable usage and data type conversion.
 * The fundamental unit is the point. Other units are line segments,
 *	open paths, boxes, closed paths, and polygons (which should be
 *considered non-intersecting closed paths).
 *
 * Data representation is as follows:
 *	point:				(x,y)
 *	line segment:		[(x1,y1),(x2,y2)]
 *	box:				(x1,y1),(x2,y2)
 *	open path:			[(x1,y1),...,(xn,yn)]
 *	closed path:		((x1,y1),...,(xn,yn))
 *	polygon:			((x1,y1),...,(xn,yn))
 *
 * For boxes, the points are opposite corners with the first point at the top
 *right. For closed paths and polygons, the points should be reordered to allow
 *	fast and correct equality comparisons.
 *
 * XXX perhaps points in complex shapes should be reordered internally
 *	to allow faster internal operations, but should keep track of input
 *order and restore that order for text output - tgl 97/01/16
 */

static float8
single_decode(char *num, char **endptr_p, const char *type_name, const char *orig_string)
{

} /* single_decode() */

static void
single_encode(float8 x, StringInfo str)
{




} /* single_encode() */

static void
pair_decode(char *str, float8 *x, float8 *y, char **endptr_p, const char *type_name, const char *orig_string)
{









































}

static void
pair_encode(float8 x, float8 y, StringInfo str)
{






}

static void
path_decode(char *str, bool opentype, int npts, Point *p, bool *isopen, char **endptr_p, const char *type_name, const char *orig_string)
{









































































} /* path_decode() */

static char *
path_encode(enum path_delim path_delim, int npts, Point *pt)
{










































} /* path_encode() */

/*-------------------------------------------------------------
 * pair_count - count the number of points
 * allow the following notation:
 * '((1,2),(3,4))'
 * '(1,3,2,4)'
 * require an odd number of delim characters in the string
 *-------------------------------------------------------------*/
static int
pair_count(char *s, char delim)
{








}

/***********************************************************************
 **
 **		Routines for two-dimensional boxes.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 * Formatting and conversion routines.
 *---------------------------------------------------------*/

/*		box_in	-		convert a string to internal form.
 *
 *		External format: (two corners of box)
 *				"(f8, f8), (f8, f8)"
 *				also supports the older style "(f8, f8, f8, f8)"
 */
Datum
box_in(PG_FUNCTION_ARGS)
{






















}

/*		box_out -		convert a box to external form.
 */
Datum
box_out(PG_FUNCTION_ARGS)
{



}

/*
 *		box_recv			- converts external binary
 *format to box
 */
Datum
box_recv(PG_FUNCTION_ARGS)
{


























}

/*
 *		box_send			- converts box to binary format
 */
Datum
box_send(PG_FUNCTION_ARGS)
{









}

/*		box_construct	-		fill in a new box.
 */
static inline void
box_construct(BOX *result, Point *pt1, Point *pt2)
{




















}

/*----------------------------------------------------------
 *	Relational operators for BOXes.
 *		<, >, <=, >=, and == are based on box area.
 *---------------------------------------------------------*/

/*		box_same		-		are two boxes identical?
 */
Datum
box_same(PG_FUNCTION_ARGS)
{




}

/*		box_overlap		-		does box1 overlap box2?
 */
Datum
box_overlap(PG_FUNCTION_ARGS)
{




}

static bool
box_ov(BOX *box1, BOX *box2)
{

}

/*		box_left		-		is box1 strictly left of
 * box2?
 */
Datum
box_left(PG_FUNCTION_ARGS)
{




}

/*		box_overleft	-		is the right edge of box1 at or
 *left of the right edge of box2?
 *
 *		This is "less than or equal" for the end of a time range,
 *		when time ranges are stored as rectangles.
 */
Datum
box_overleft(PG_FUNCTION_ARGS)
{




}

/*		box_right		-		is box1 strictly right
 * of box2?
 */
Datum
box_right(PG_FUNCTION_ARGS)
{




}

/*		box_overright	-		is the left edge of box1 at or
 *right of the left edge of box2?
 *
 *		This is "greater than or equal" for time ranges, when time
 *ranges are stored as rectangles.
 */
Datum
box_overright(PG_FUNCTION_ARGS)
{




}

/*		box_below		-		is box1 strictly below
 * box2?
 */
Datum
box_below(PG_FUNCTION_ARGS)
{




}

/*		box_overbelow	-		is the upper edge of box1 at or
 *below the upper edge of box2?
 */
Datum
box_overbelow(PG_FUNCTION_ARGS)
{




}

/*		box_above		-		is box1 strictly above
 * box2?
 */
Datum
box_above(PG_FUNCTION_ARGS)
{




}

/*		box_overabove	-		is the lower edge of box1 at or
 *above the lower edge of box2?
 */
Datum
box_overabove(PG_FUNCTION_ARGS)
{




}

/*		box_contained	-		is box1 contained by box2?
 */
Datum
box_contained(PG_FUNCTION_ARGS)
{




}

/*		box_contain		-		does box1 contain box2?
 */
Datum
box_contain(PG_FUNCTION_ARGS)
{




}

/*
 * Check whether the second box is in the first box or on its border
 */
static bool
box_contain_box(BOX *contains_box, BOX *contained_box)
{

}

/*		box_positionop	-
 *				is box1 entirely {above,below} box2?
 *
 * box_below_eq and box_above_eq are obsolete versions that (probably
 * erroneously) accept the equal-boundaries case.  Since these are not
 * in sync with the box_left and box_right code, they are deprecated and
 * not supported in the PG 8.1 rtree operator class extension.
 */
Datum
box_below_eq(PG_FUNCTION_ARGS)
{




}

Datum
box_above_eq(PG_FUNCTION_ARGS)
{




}

/*		box_relop		-		is area(box1) relop
 *area(box2), within our accuracy constraint?
 */
Datum
box_lt(PG_FUNCTION_ARGS)
{




}

Datum
box_gt(PG_FUNCTION_ARGS)
{




}

Datum
box_eq(PG_FUNCTION_ARGS)
{




}

Datum
box_le(PG_FUNCTION_ARGS)
{




}

Datum
box_ge(PG_FUNCTION_ARGS)
{




}

/*----------------------------------------------------------
 *	"Arithmetic" operators on boxes.
 *---------------------------------------------------------*/

/*		box_area		-		returns the area of the
 * box.
 */
Datum
box_area(PG_FUNCTION_ARGS)
{



}

/*		box_width		-		returns the width of the
 *box (horizontal magnitude).
 */
Datum
box_width(PG_FUNCTION_ARGS)
{



}

/*		box_height		-		returns the height of
 *the box (vertical magnitude).
 */
Datum
box_height(PG_FUNCTION_ARGS)
{



}

/*		box_distance	-		returns the distance between the
 *								  center points
 *of two boxes.
 */
Datum
box_distance(PG_FUNCTION_ARGS)
{








}

/*		box_center		-		returns the center point
 * of the box.
 */
Datum
box_center(PG_FUNCTION_ARGS)
{






}

/*		box_ar	-		returns the area of the box.
 */
static float8
box_ar(BOX *box)
{

}

/*		box_cn	-		stores the centerpoint of the box into
 * *center.
 */
static void
box_cn(Point *center, BOX *box)
{


}

/*		box_wd	-		returns the width (length) of the box
 *								  (horizontal
 *magnitude).
 */
static float8
box_wd(BOX *box)
{

}

/*		box_ht	-		returns the height of the box
 *								  (vertical
 *magnitude).
 */
static float8
box_ht(BOX *box)
{

}

/*----------------------------------------------------------
 *	Funky operations.
 *---------------------------------------------------------*/

/*		box_intersect	-
 *				returns the overlapping portion of two boxes,
 *				  or NULL if they do not intersect.
 */
Datum
box_intersect(PG_FUNCTION_ARGS)
{

















}

/*		box_diagonal	-
 *				returns a line segment which happens to be the
 *				  positive-slope diagonal of "box".
 */
Datum
box_diagonal(PG_FUNCTION_ARGS)
{






}

/***********************************************************************
 **
 **		Routines for 2D lines.
 **
 ***********************************************************************/

static bool
line_decode(char *s, const char *str, LINE *line)
{

























}

Datum
line_in(PG_FUNCTION_ARGS)
{

































}

Datum
line_out(PG_FUNCTION_ARGS)
{






}

/*
 *		line_recv			- converts external binary
 *format to line
 */
Datum
line_recv(PG_FUNCTION_ARGS)
{















}

/*
 *		line_send			- converts line to binary format
 */
Datum
line_send(PG_FUNCTION_ARGS)
{








}

/*----------------------------------------------------------
 *	Conversion routines from one line formula to internal.
 *		Internal form:	Ax+By+C=0
 *---------------------------------------------------------*/

/*
 * Fill already-allocated LINE struct from the point and the slope
 */
static inline void
line_construct(LINE *result, Point *pt, float8 m)
{



















}

/* line_construct_pp()
 * two points
 */
Datum
line_construct_pp(PG_FUNCTION_ARGS)
{












}

/*----------------------------------------------------------
 *	Relative position routines.
 *---------------------------------------------------------*/

Datum
line_intersect(PG_FUNCTION_ARGS)
{




}

Datum
line_parallel(PG_FUNCTION_ARGS)
{




}

Datum
line_perp(PG_FUNCTION_ARGS)
{





















}

Datum
line_vertical(PG_FUNCTION_ARGS)
{



}

Datum
line_horizontal(PG_FUNCTION_ARGS)
{



}

/*
 * Check whether the two lines are the same
 *
 * We consider NaNs values to be equal to each other to let those lines
 * to be found.
 */
Datum
line_eq(PG_FUNCTION_ARGS)
{






















}

/*----------------------------------------------------------
 *	Line arithmetic routines.
 *---------------------------------------------------------*/

/*
 * Return slope of the line
 */
static inline float8
line_sl(LINE *line)
{









}

/*
 * Return inverse slope of the line
 */
static inline float8
line_invsl(LINE *line)
{









}

/* line_distance()
 * Distance between two lines.
 */
Datum
line_distance(PG_FUNCTION_ARGS)
{























}

/* line_interpt()
 * Point where two lines l1, l2 intersect (if any)
 */
Datum
line_interpt(PG_FUNCTION_ARGS)
{











}

/*
 * Internal version of line_interpt
 *
 * Return whether two lines intersect. If *result is not NULL, it is set to
 * the intersection point.
 *
 * NOTE: If the lines are identical then we will find they are parallel
 * and report "no intersection".  This is a little weird, but since
 * there's no *unique* intersection, maybe it's appropriate behavior.
 *
 * If the lines have NaN constants, we will return true, and the intersection
 * point would have NaN coordinates.  We shouldn't return false in this case
 * because that would mean the lines are parallel.
 */
static bool
line_interpt_line(Point *result, LINE *l1, LINE *l2)
{











































}

/***********************************************************************
 **
 **		Routines for 2D paths (sequences of line segments, also
 **				called `polylines').
 **
 **				This is not a general package for geometric
 *paths, *				which of course include polygons; the
 *emphasis here *				is on (for example) usefulness
 *in wire layout.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 *	String to path / path to string conversion.
 *		External format:
 *				"((xcoord, ycoord),... )"
 *				"[(xcoord, ycoord),... ]"
 *				"(xcoord, ycoord),... "
 *				"[xcoord, ycoord,... ]"
 *		Also support older format:
 *				"(closed, npts, xcoord, ycoord,... )"
 *---------------------------------------------------------*/

Datum
path_area(PG_FUNCTION_ARGS)
{

















}

Datum
path_in(PG_FUNCTION_ARGS)
{
































































}

Datum
path_out(PG_FUNCTION_ARGS)
{



}

/*
 *		path_recv			- converts external binary
 *format to path
 *
 * External representation is closed flag (a boolean byte), int32 number
 * of points, and the points.
 */
Datum
path_recv(PG_FUNCTION_ARGS)
{






























}

/*
 *		path_send			- converts path to binary format
 */
Datum
path_send(PG_FUNCTION_ARGS)
{













}

/*----------------------------------------------------------
 *	Relational operators.
 *		These are based on the path cardinality,
 *		as stupid as that sounds.
 *
 *		Better relops and access methods coming soon.
 *---------------------------------------------------------*/

Datum
path_n_lt(PG_FUNCTION_ARGS)
{




}

Datum
path_n_gt(PG_FUNCTION_ARGS)
{




}

Datum
path_n_eq(PG_FUNCTION_ARGS)
{




}

Datum
path_n_le(PG_FUNCTION_ARGS)
{




}

Datum
path_n_ge(PG_FUNCTION_ARGS)
{




}

/*----------------------------------------------------------
 * Conversion operators.
 *---------------------------------------------------------*/

Datum
path_isclosed(PG_FUNCTION_ARGS)
{



}

Datum
path_isopen(PG_FUNCTION_ARGS)
{



}

Datum
path_npoints(PG_FUNCTION_ARGS)
{



}

Datum
path_close(PG_FUNCTION_ARGS)
{





}

Datum
path_open(PG_FUNCTION_ARGS)
{





}

/* path_inter -
 *		Does p1 intersect p2 at any point?
 *		Use bounding boxes for a quick (O(n)) check, then do a
 *		O(n^2) iterative edge check.
 */
Datum
path_inter(PG_FUNCTION_ARGS)
{













































































}

/* path_distance()
 * This essentially does a cartesian product of the lsegs in the
 *	two paths, and finds the min distance between any two lsegs
 */
Datum
path_distance(PG_FUNCTION_ARGS)
{




























































}

/*----------------------------------------------------------
 *	"Arithmetic" operations.
 *---------------------------------------------------------*/

Datum
path_length(PG_FUNCTION_ARGS)
{

























}

/***********************************************************************
 **
 **		Routines for 2D points.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 *	String to point, point to string conversion.
 *		External format:
 *				"(x,y)"
 *				"x,y"
 *---------------------------------------------------------*/

Datum
point_in(PG_FUNCTION_ARGS)
{





}

Datum
point_out(PG_FUNCTION_ARGS)
{



}

/*
 *		point_recv			- converts external binary
 *format to point
 */
Datum
point_recv(PG_FUNCTION_ARGS)
{







}

/*
 *		point_send			- converts point to binary
 *format
 */
Datum
point_send(PG_FUNCTION_ARGS)
{







}

/*
 * Initialize a point
 */
static inline void
point_construct(Point *result, float8 x, float8 y)
{


}

/*----------------------------------------------------------
 *	Relational operators for Points.
 *		Since we do have a sense of coordinates being
 *		"equal" to a given accuracy (point_vert, point_horiz),
 *		the other ops must preserve that sense.  This means
 *		that results may, strictly speaking, be a lie (unless
 *		EPSILON = 0.0).
 *---------------------------------------------------------*/

Datum
point_left(PG_FUNCTION_ARGS)
{




}

Datum
point_right(PG_FUNCTION_ARGS)
{




}

Datum
point_above(PG_FUNCTION_ARGS)
{




}

Datum
point_below(PG_FUNCTION_ARGS)
{




}

Datum
point_vert(PG_FUNCTION_ARGS)
{




}

Datum
point_horiz(PG_FUNCTION_ARGS)
{




}

Datum
point_eq(PG_FUNCTION_ARGS)
{




}

Datum
point_ne(PG_FUNCTION_ARGS)
{




}

/*
 * Check whether the two points are the same
 *
 * We consider NaNs coordinates to be equal to each other to let those points
 * to be found.
 */
static inline bool
point_eq_point(Point *pt1, Point *pt2)
{

}

/*----------------------------------------------------------
 *	"Arithmetic" operators on points.
 *---------------------------------------------------------*/

Datum
point_distance(PG_FUNCTION_ARGS)
{




}

static inline float8
point_dt(Point *pt1, Point *pt2)
{

}

Datum
point_slope(PG_FUNCTION_ARGS)
{




}

/*
 * Return slope of two points
 *
 * Note that this function returns DBL_MAX when the points are the same.
 */
static inline float8
point_sl(Point *pt1, Point *pt2)
{









}

/*
 * Return inverse slope of two points
 *
 * Note that this function returns 0.0 when the points are the same.
 */
static inline float8
point_invsl(Point *pt1, Point *pt2)
{









}

/***********************************************************************
 **
 **		Routines for 2D line segments.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 *	String to lseg, lseg to string conversion.
 *		External forms: "[(x1, y1), (x2, y2)]"
 *						"(x1, y1), (x2, y2)"
 *						"x1, y1, x2, y2"
 *		closed form ok	"((x1, y1), (x2, y2))"
 *		(old form)		"(x1, y1, x2, y2)"
 *---------------------------------------------------------*/

Datum
lseg_in(PG_FUNCTION_ARGS)
{






}

Datum
lseg_out(PG_FUNCTION_ARGS)
{



}

/*
 *		lseg_recv			- converts external binary
 *format to lseg
 */
Datum
lseg_recv(PG_FUNCTION_ARGS)
{











}

/*
 *		lseg_send			- converts lseg to binary format
 */
Datum
lseg_send(PG_FUNCTION_ARGS)
{









}

/* lseg_construct -
 *		form a LSEG from two Points.
 */
Datum
lseg_construct(PG_FUNCTION_ARGS)
{







}

/* like lseg_construct, but assume space already allocated */
static inline void
statlseg_construct(LSEG *lseg, Point *pt1, Point *pt2)
{




}

/*
 * Return slope of the line segment
 */
static inline float8
lseg_sl(LSEG *lseg)
{

}

/*
 * Return inverse slope of the line segment
 */
static inline float8
lseg_invsl(LSEG *lseg)
{

}

Datum
lseg_length(PG_FUNCTION_ARGS)
{



}

/*----------------------------------------------------------
 *	Relative position routines.
 *---------------------------------------------------------*/

/*
 **  find intersection of the two lines, and see if it falls on
 **  both segments.
 */
Datum
lseg_intersect(PG_FUNCTION_ARGS)
{




}

Datum
lseg_parallel(PG_FUNCTION_ARGS)
{




}

/*
 * Determine if two line segments are perpendicular.
 */
Datum
lseg_perp(PG_FUNCTION_ARGS)
{




}

Datum
lseg_vertical(PG_FUNCTION_ARGS)
{



}

Datum
lseg_horizontal(PG_FUNCTION_ARGS)
{



}

Datum
lseg_eq(PG_FUNCTION_ARGS)
{




}

Datum
lseg_ne(PG_FUNCTION_ARGS)
{




}

Datum
lseg_lt(PG_FUNCTION_ARGS)
{




}

Datum
lseg_le(PG_FUNCTION_ARGS)
{




}

Datum
lseg_gt(PG_FUNCTION_ARGS)
{




}

Datum
lseg_ge(PG_FUNCTION_ARGS)
{




}

/*----------------------------------------------------------
 *	Line arithmetic routines.
 *---------------------------------------------------------*/

/* lseg_distance -
 *		If two segments don't intersect, then the closest
 *		point will be from one of the endpoints to the other
 *		segment.
 */
Datum
lseg_distance(PG_FUNCTION_ARGS)
{




}

Datum
lseg_center(PG_FUNCTION_ARGS)
{









}

/*
 * Return whether the two segments intersect. If *result is not NULL,
 * it is set to the intersection point.
 *
 * This function is almost perfectly symmetric, even though it doesn't look
 * like it.  See lseg_interpt_line() for the other half of it.
 */
static bool
lseg_interpt_lseg(Point *result, LSEG *l1, LSEG *l2)
{
























}

Datum
lseg_interpt(PG_FUNCTION_ARGS)
{











}

/***********************************************************************
 **
 **		Routines for position comparisons of differently-typed
 **				2D objects.
 **
 ***********************************************************************/

/*---------------------------------------------------------------------
 *		dist_
 *				Minimum distance from one object to another.
 *-------------------------------------------------------------------*/

/*
 * Distance from a point to a line
 */
Datum
dist_pl(PG_FUNCTION_ARGS)
{




}

/*
 * Distance from a point to a lseg
 */
Datum
dist_ps(PG_FUNCTION_ARGS)
{




}

/*
 * Distance from a point to a path
 */
Datum
dist_ppath(PG_FUNCTION_ARGS)
{









































}

/*
 * Distance from a point to a box
 */
Datum
dist_pb(PG_FUNCTION_ARGS)
{




}

/*
 * Distance from a lseg to a line
 */
Datum
dist_sl(PG_FUNCTION_ARGS)
{




}

/*
 * Distance from a lseg to a box
 */
Datum
dist_sb(PG_FUNCTION_ARGS)
{




}

/*
 * Distance from a line to a box
 */
Datum
dist_lb(PG_FUNCTION_ARGS)
{









}

/*
 * Distance from a circle to a polygon
 */
Datum
dist_cpoly(PG_FUNCTION_ARGS)
{












}

/*
 * Distance from a point to a polygon
 */
Datum
dist_ppoly(PG_FUNCTION_ARGS)
{




}

Datum
dist_polyp(PG_FUNCTION_ARGS)
{




}

static float8
dist_ppoly_internal(Point *pt, POLYGON *poly)
{
































}

/*---------------------------------------------------------------------
 *		interpt_
 *				Intersection point of objects.
 *				We choose to ignore the "point" of intersection
 *between lines and boxes, since there are typically two.
 *-------------------------------------------------------------------*/

/*
 * Return whether the line segment intersect with the line. If *result is not
 * NULL, it is set to the intersection point.
 */
static bool
lseg_interpt_line(Point *result, LSEG *lseg, LINE *line)
{












































}

/*---------------------------------------------------------------------
 *		close_
 *				Point of closest proximity between objects.
 *-------------------------------------------------------------------*/

/*
 * If *result is not NULL, it is set to the intersection point of a
 * perpendicular of the line through the point.  Returns the distance
 * of those two points.
 */
static float8
line_closept_point(Point *result, LINE *line, Point *point)
{

























}

Datum
close_pl(PG_FUNCTION_ARGS)
{












}

/*
 * Closest point on line segment to specified point.
 *
 * If *result is not NULL, set it to the closest point on the line segment
 * to the point.  Returns the distance of the two points.
 */
static float8
lseg_closept_point(Point *result, LSEG *lseg, Point *pt)
{
















}

Datum
close_ps(PG_FUNCTION_ARGS)
{












}

/*
 * Closest point on line segment to line segment
 */
static float8
lseg_closept_lseg(Point *result, LSEG *on_lseg, LSEG *to_lseg)
{













































}

Datum
close_lseg(PG_FUNCTION_ARGS)
{

















}

/*
 * Closest point on or in box to specified point.
 *
 * If *result is not NULL, set it to the closest point on the box to the
 * given point, and return the distance of the two points.
 */
static float8
box_closept_point(Point *result, BOX *box, Point *pt)
{
























































}

Datum
close_pb(PG_FUNCTION_ARGS)
{












}

/* close_sl()
 * Closest point on line to line segment.
 *
 * XXX THIS CODE IS WRONG
 * The code is actually calculating the point on the line segment
 *	which is backwards from the routine naming convention.
 * Copied code to new routine close_ls() but haven't fixed this one yet.
 * - thomas 1998-01-31
 */
Datum
close_sl(PG_FUNCTION_ARGS)
{






























}

/*
 * Closest point on line segment to line.
 *
 * Return the distance between the line and the closest point of the line
 * segment to the line.  If *result is not NULL, set it to that point.
 *
 * NOTE: When the lines are parallel, endpoints of one of the line segment
 * are FPeq(), in presence of NaN or Infinite coordinates, or perhaps =
 * even because of simple roundoff issues, there may not be a single closest
 * point.  We are likely to set the result to the second endpoint in these
 * cases.
 */
static float8
lseg_closept_line(Point *result, LSEG *lseg, LINE *line)
{




























}

Datum
close_ls(PG_FUNCTION_ARGS)
{

















}

/*
 * Closest point on or in box to line segment.
 *
 * Returns the distance between the closest point on or in the box to
 * the line segment.  If *result is not NULL, it is set to that point.
 */
static float8
box_closept_lseg(Point *result, BOX *box, LSEG *lseg)
{



















































}

Datum
close_sb(PG_FUNCTION_ARGS)
{












}

Datum
close_lb(PG_FUNCTION_ARGS)
{









}

/*---------------------------------------------------------------------
 *		on_
 *				Whether one object lies completely within
 *another.
 *-------------------------------------------------------------------*/

/*
 *		Does the point satisfy the equation?
 */
static bool
line_contain_point(LINE *line, Point *point)
{

}

Datum
on_pl(PG_FUNCTION_ARGS)
{




}

/*
 *		Determine colinearity by detecting a triangle inequality.
 * This algorithm seems to behave nicely even with lsb residues - tgl 1997-07-09
 */
static bool
lseg_contain_point(LSEG *lseg, Point *pt)
{

}

Datum
on_ps(PG_FUNCTION_ARGS)
{




}

/*
 * Check whether the point is in the box or on its border
 */
static bool
box_contain_point(BOX *box, Point *point)
{

}

Datum
on_pb(PG_FUNCTION_ARGS)
{




}

Datum
box_contain_pt(PG_FUNCTION_ARGS)
{




}

/* on_ppath -
 *		Whether a point lies within (on) a polyline.
 *		If open, we have to (groan) check each segment.
 * (uses same algorithm as for point intersecting segment - tgl 1997-07-09)
 *		If closed, we use the old O(n) ray method for point-in-polygon.
 *				The ray is horizontal, from pt out to the right.
 *				Each segment that crosses the ray counts as an
 *				intersection; note that an endpoint or edge may
 *touch but not cross. (we can do p-in-p in lg(n), but it takes preprocessing)
 */
Datum
on_ppath(PG_FUNCTION_ARGS)
{
























}

/*
 * Check whether the line segment is on the line or close enough
 *
 * It is, if both of its points are on the line or close enough.
 */
Datum
on_sl(PG_FUNCTION_ARGS)
{




}

/*
 * Check whether the line segment is in the box or on its border
 *
 * It is, if both of its points are in the box or on its border.
 */
static bool
box_contain_lseg(BOX *box, LSEG *lseg)
{

}

Datum
on_sb(PG_FUNCTION_ARGS)
{




}

/*---------------------------------------------------------------------
 *		inter_
 *				Whether one object intersects another.
 *-------------------------------------------------------------------*/

Datum
inter_sl(PG_FUNCTION_ARGS)
{




}

/*
 * Do line segment and box intersect?
 *
 * Segment completely inside box counts as intersection.
 * If you want only segments crossing box boundaries,
 *	try converting box to path first.
 *
 * This function also sets the *result to the closest point on the line
 * segment to the center of the box when they overlap and the result is
 * not NULL.  It is somewhat arbitrary, but maybe the best we can do as
 * there are typically two points they intersect.
 *
 * Optimize for non-intersection by checking for box intersection first.
 * - thomas 1998-01-30
 */
static bool
box_interpt_lseg(Point *result, BOX *box, LSEG *lseg)
{


























































}

Datum
inter_sb(PG_FUNCTION_ARGS)
{




}

/* inter_lb()
 * Do line and box intersect?
 */
Datum
inter_lb(PG_FUNCTION_ARGS)
{







































}

/*------------------------------------------------------------------
 * The following routines define a data type and operator class for
 * POLYGONS .... Part of which (the polygon's bounding box) is built on
 * top of the BOX data type.
 *
 * make_bound_box - create the bounding box for the input polygon
 *------------------------------------------------------------------*/

/*---------------------------------------------------------------------
 * Make the smallest bounding box for the given polygon.
 *---------------------------------------------------------------------*/
static void
make_bound_box(POLYGON *poly)
{































}

/*------------------------------------------------------------------
 * poly_in - read in the polygon from a string specification
 *
 *		External format:
 *				"((x0,y0),...,(xn,yn))"
 *				"x0,y0,...,xn,yn"
 *				also supports the older style
 *"(x1,...,xn,y1,...yn)"
 *------------------------------------------------------------------*/
Datum
poly_in(PG_FUNCTION_ARGS)
{































}

/*---------------------------------------------------------------
 * poly_out - convert internal POLYGON representation to the
 *			  character string format "((f8,f8),...,(f8,f8))"
 *---------------------------------------------------------------*/
Datum
poly_out(PG_FUNCTION_ARGS)
{



}

/*
 *		poly_recv			- converts external binary
 *format to polygon
 *
 * External representation is int32 number of points, and the points.
 * We recompute the bounding box on read, instead of trusting it to
 * be valid.  (Checking it would take just as long, so may as well
 * omit it from external representation.)
 */
Datum
poly_recv(PG_FUNCTION_ARGS)
{



























}

/*
 *		poly_send			- converts polygon to binary
 *format
 */
Datum
poly_send(PG_FUNCTION_ARGS)
{












}

/*-------------------------------------------------------
 * Is polygon A strictly left of polygon B? i.e. is
 * the right most point of A left of the left most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_left(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A overlapping or left of polygon B? i.e. is
 * the right most point of A at or left of the right most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_overleft(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A strictly right of polygon B? i.e. is
 * the left most point of A right of the right most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_right(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A overlapping or right of polygon B? i.e. is
 * the left most point of A at or right of the left most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_overright(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A strictly below polygon B? i.e. is
 * the upper most point of A below the lower most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_below(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A overlapping or below polygon B? i.e. is
 * the upper most point of A at or below the upper most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_overbelow(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A strictly above polygon B? i.e. is
 * the lower most point of A above the upper most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_above(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A overlapping or above polygon B? i.e. is
 * the lower most point of A at or above the lower most point
 * of B?
 *-------------------------------------------------------*/
Datum
poly_overabove(PG_FUNCTION_ARGS)
{













}

/*-------------------------------------------------------
 * Is polygon A the same as polygon B? i.e. are all the
 * points the same?
 * Check all points for matches in both forward and reverse
 *	direction since polygons are non-directional and are
 *	closed shapes.
 *-------------------------------------------------------*/
Datum
poly_same(PG_FUNCTION_ARGS)
{




















}

/*-----------------------------------------------------------------
 * Determine if polygon A overlaps polygon B
 *-----------------------------------------------------------------*/
Datum
poly_overlap(PG_FUNCTION_ARGS)
{

























































}

/*
 * Tests special kind of segment for in/out of polygon.
 * Special kind means:
 *	- point a should be on segment s
 *	- segment (a,b) should not be contained by s
 * Returns true if:
 *	- segment (a,b) is collinear to s and (a,b) is in polygon
 *	- segment (a,b) s not collinear to s. Note: that doesn't
 *	  mean that segment is in polygon!
 */

static bool
touched_lseg_inside_poly(Point *a, Point *b, LSEG *s, POLYGON *poly, int start)
{






























}

/*
 * Returns true if segment (a,b) is in polygon, option
 * start is used for optimization - function checks
 * polygon's edges starting from start
 */
static bool
lseg_inside_poly(Point *a, Point *b, POLYGON *poly, int start)
{


































































}

/*
 * Check whether the first polygon contains the second
 */
static bool
poly_contain_poly(POLYGON *contains_poly, POLYGON *contained_poly)
{



























}

Datum
poly_contain(PG_FUNCTION_ARGS)
{













}

/*-----------------------------------------------------------------
 * Determine if polygon A is contained by polygon B
 *-----------------------------------------------------------------*/
Datum
poly_contained(PG_FUNCTION_ARGS)
{














}

Datum
poly_contain_pt(PG_FUNCTION_ARGS)
{




}

Datum
pt_contained_poly(PG_FUNCTION_ARGS)
{




}

Datum
poly_distance(PG_FUNCTION_ARGS)
{








}

/***********************************************************************
 **
 **		Routines for 2D points.
 **
 ***********************************************************************/

Datum
construct_point(PG_FUNCTION_ARGS)
{









}

static inline void
point_add_point(Point *result, Point *pt1, Point *pt2)
{

}

Datum
point_add(PG_FUNCTION_ARGS)
{









}

static inline void
point_sub_point(Point *result, Point *pt1, Point *pt2)
{

}

Datum
point_sub(PG_FUNCTION_ARGS)
{









}

static inline void
point_mul_point(Point *result, Point *pt1, Point *pt2)
{

}

Datum
point_mul(PG_FUNCTION_ARGS)
{









}

static inline void
point_div_point(Point *result, Point *pt1, Point *pt2)
{





}

Datum
point_div(PG_FUNCTION_ARGS)
{









}

/***********************************************************************
 **
 **		Routines for 2D boxes.
 **
 ***********************************************************************/

Datum
points_box(PG_FUNCTION_ARGS)
{









}

Datum
box_add(PG_FUNCTION_ARGS)
{










}

Datum
box_sub(PG_FUNCTION_ARGS)
{










}

Datum
box_mul(PG_FUNCTION_ARGS)
{













}

Datum
box_div(PG_FUNCTION_ARGS)
{













}

/*
 * Convert point to empty box
 */
Datum
point_box(PG_FUNCTION_ARGS)
{











}

/*
 * Smallest bounding box that includes both of the given boxes
 */
Datum
boxes_bound_box(PG_FUNCTION_ARGS)
{










}

/***********************************************************************
 **
 **		Routines for 2D paths.
 **
 ***********************************************************************/

/* path_add()
 * Concatenate two paths (only if they are both open).
 */
Datum
path_add(PG_FUNCTION_ARGS)
{








































}

/* path_add_pt()
 * Translation operators.
 */
Datum
path_add_pt(PG_FUNCTION_ARGS)
{










}

Datum
path_sub_pt(PG_FUNCTION_ARGS)
{










}

/* path_mul_pt()
 * Rotation and scaling operators.
 */
Datum
path_mul_pt(PG_FUNCTION_ARGS)
{










}

Datum
path_div_pt(PG_FUNCTION_ARGS)
{










}

Datum
path_center(PG_FUNCTION_ARGS)
{







}

Datum
path_poly(PG_FUNCTION_ARGS)
{






























}

/***********************************************************************
 **
 **		Routines for 2D polygons.
 **
 ***********************************************************************/

Datum
poly_npoints(PG_FUNCTION_ARGS)
{



}

Datum
poly_center(PG_FUNCTION_ARGS)
{










}

Datum
poly_box(PG_FUNCTION_ARGS)
{







}

/* box_poly()
 * Convert a box to a polygon.
 */
Datum
box_poly(PG_FUNCTION_ARGS)
{























}

Datum
poly_path(PG_FUNCTION_ARGS)
{

























}

/***********************************************************************
 **
 **		Routines for circles.
 **
 ***********************************************************************/

/*----------------------------------------------------------
 * Formatting and conversion routines.
 *---------------------------------------------------------*/

/*		circle_in		-		convert a string to
 *internal form.
 *
 *		External format: (center and radius of circle)
 *				"<(f8,f8),f8>"
 *				also supports quick entry style "f8,f8,f8"
 */
Datum
circle_in(PG_FUNCTION_ARGS)
{


































































}

/*		circle_out		-		convert a circle to
 * external form.
 */
Datum
circle_out(PG_FUNCTION_ARGS)
{














}

/*
 *		circle_recv			- converts external binary
 *format to circle
 */
Datum
circle_recv(PG_FUNCTION_ARGS)
{
















}

/*
 *		circle_send			- converts circle to binary
 *format
 */
Datum
circle_send(PG_FUNCTION_ARGS)
{








}

/*----------------------------------------------------------
 *	Relational operators for CIRCLEs.
 *		<, >, <=, >=, and == are based on circle area.
 *---------------------------------------------------------*/

/*		circles identical?
 *
 * We consider NaNs values to be equal to each other to let those circles
 * to be found.
 */
Datum
circle_same(PG_FUNCTION_ARGS)
{




}

/*		circle_overlap	-		does circle1 overlap circle2?
 */
Datum
circle_overlap(PG_FUNCTION_ARGS)
{




}

/*		circle_overleft -		is the right edge of circle1 at
 *or left of the right edge of circle2?
 */
Datum
circle_overleft(PG_FUNCTION_ARGS)
{




}

/*		circle_left		-		is circle1 strictly left
 * of circle2?
 */
Datum
circle_left(PG_FUNCTION_ARGS)
{




}

/*		circle_right	-		is circle1 strictly right of
 * circle2?
 */
Datum
circle_right(PG_FUNCTION_ARGS)
{




}

/*		circle_overright	-	is the left edge of circle1 at
 *or right of the left edge of circle2?
 */
Datum
circle_overright(PG_FUNCTION_ARGS)
{




}

/*		circle_contained		-		is circle1
 * contained by circle2?
 */
Datum
circle_contained(PG_FUNCTION_ARGS)
{




}

/*		circle_contain	-		does circle1 contain circle2?
 */
Datum
circle_contain(PG_FUNCTION_ARGS)
{




}

/*		circle_below		-		is circle1 strictly
 * below circle2?
 */
Datum
circle_below(PG_FUNCTION_ARGS)
{




}

/*		circle_above	-		is circle1 strictly above
 * circle2?
 */
Datum
circle_above(PG_FUNCTION_ARGS)
{




}

/*		circle_overbelow -		is the upper edge of circle1 at
 *or below the upper edge of circle2?
 */
Datum
circle_overbelow(PG_FUNCTION_ARGS)
{




}

/*		circle_overabove	-	is the lower edge of circle1 at
 *or above the lower edge of circle2?
 */
Datum
circle_overabove(PG_FUNCTION_ARGS)
{




}

/*		circle_relop	-		is area(circle1) relop
 *area(circle2), within our accuracy constraint?
 */
Datum
circle_eq(PG_FUNCTION_ARGS)
{




}

Datum
circle_ne(PG_FUNCTION_ARGS)
{




}

Datum
circle_lt(PG_FUNCTION_ARGS)
{




}

Datum
circle_gt(PG_FUNCTION_ARGS)
{




}

Datum
circle_le(PG_FUNCTION_ARGS)
{




}

Datum
circle_ge(PG_FUNCTION_ARGS)
{




}

/*----------------------------------------------------------
 *	"Arithmetic" operators on circles.
 *---------------------------------------------------------*/

/* circle_add_pt()
 * Translation operator.
 */
Datum
circle_add_pt(PG_FUNCTION_ARGS)
{










}

Datum
circle_sub_pt(PG_FUNCTION_ARGS)
{










}

/* circle_mul_pt()
 * Rotation and scaling operators.
 */
Datum
circle_mul_pt(PG_FUNCTION_ARGS)
{










}

Datum
circle_div_pt(PG_FUNCTION_ARGS)
{










}

/*		circle_area		-		returns the area of the
 * circle.
 */
Datum
circle_area(PG_FUNCTION_ARGS)
{



}

/*		circle_diameter -		returns the diameter of the
 * circle.
 */
Datum
circle_diameter(PG_FUNCTION_ARGS)
{



}

/*		circle_radius	-		returns the radius of the
 * circle.
 */
Datum
circle_radius(PG_FUNCTION_ARGS)
{



}

/*		circle_distance -		returns the distance between
 *								  two circles.
 */
Datum
circle_distance(PG_FUNCTION_ARGS)
{











}

Datum
circle_contain_pt(PG_FUNCTION_ARGS)
{






}

Datum
pt_contained_circle(PG_FUNCTION_ARGS)
{






}

/*		dist_pc -		returns the distance between
 *						  a point and a circle.
 */
Datum
dist_pc(PG_FUNCTION_ARGS)
{











}

/*
 * Distance from a circle to a point
 */
Datum
dist_cpoint(PG_FUNCTION_ARGS)
{











}

/*		circle_center	-		returns the center point of the
 * circle.
 */
Datum
circle_center(PG_FUNCTION_ARGS)
{








}

/*		circle_ar		-		returns the area of the
 * circle.
 */
static float8
circle_ar(CIRCLE *circle)
{

}

/*----------------------------------------------------------
 *	Conversion operators.
 *---------------------------------------------------------*/

Datum
cr_circle(PG_FUNCTION_ARGS)
{











}

Datum
circle_box(PG_FUNCTION_ARGS)
{














}

/* box_circle()
 * Convert a box to a circle.
 */
Datum
box_circle(PG_FUNCTION_ARGS)
{











}

Datum
circle_poly(PG_FUNCTION_ARGS)
{












































}

/*
 * Convert polygon to circle
 *
 * The result must be preallocated.
 *
 * XXX This algorithm should use weighted means of line segments
 *	rather than straight average values of points - tgl 97/01/21.
 */
static void
poly_to_circle(CIRCLE *result, POLYGON *poly)
{




















}

Datum
poly_circle(PG_FUNCTION_ARGS)
{








}

/***********************************************************************
 **
 **		Private routines for multiple types.
 **
 ***********************************************************************/

/*
 *	Test to see if the point is inside the polygon, returns 1/0, or 2 if
 *	the point is on the polygon.
 *	Code adapted but not copied from integer-based routines in WN: A
 *	Server for the HTTP
 *	version 1.15.1, file wn/image.c
 *	http://hopf.math.northwestern.edu/index.html
 *	Description of algorithm:  http://www.linuxjournal.com/article/2197
 *							   http://www.linuxjournal.com/article/2029
 */

#define POINT_ON_POLYGON INT_MAX

static int
point_inside(Point *p, int npts, Point *plist)
{












































}

/* lseg_crossing()
 * Returns +/-2 if line segment crosses the positive X-axis in a +/- direction.
 * Returns +/-1 if one point is on the positive X-axis.
 * Returns 0 if both points are on the positive X-axis, or there is no crossing.
 * Returns POINT_ON_POLYGON if the segment contains (0,0).
 * Wow, that is one confusing API, but it is used above, and when summed,
 * can tell is if a point is in a polygon.
 */

static int
lseg_crossing(float8 x, float8 y, float8 prev_x, float8 prev_y)
{





































































}

static bool
plist_same(int npts, Point *p1, Point *p2)
{













































}

/*-------------------------------------------------------------------------
 * Determine the hypotenuse.
 *
 * If required, x and y are swapped to make x the larger number. The
 * traditional formula of x^2+y^2 is rearranged to factor x outside the
 * sqrt. This allows computation of the hypotenuse for significantly
 * larger values, and with a higher precision than when using the naive
 * formula.  In particular, this cannot overflow unless the final result
 * would be out-of-range.
 *
 * sqrt( x^2 + y^2 ) = sqrt( x^2( 1 + y^2/x^2) )
 *					 = x * sqrt( 1 + y^2/x^2 )
 *					 = x * sqrt( 1 + y/x * y/x )
 *
 * It is expected that this routine will eventually be replaced with the
 * C99 hypot() function.
 *
 * This implementation conforms to IEEE Std 1003.1 and GLIBC, in that the
 * case of hypot(inf,nan) results in INF, and not NAN.
 *-----------------------------------------------------------------------
 */
float8
pg_hypot(float8 x, float8 y)
{


















































}