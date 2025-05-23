/* Generated by CIL v. 1.7.3 */
/* print_CIL_Input is true */

typedef unsigned long size_t;
typedef long __off_t;
typedef long __off64_t;
struct _IO_FILE;
struct _IO_FILE;
typedef struct _IO_FILE FILE;
typedef void _IO_lock_t;
struct _IO_marker {
   struct _IO_marker *_next ;
   struct _IO_FILE *_sbuf ;
   int _pos ;
};
struct _IO_FILE {
   int _flags ;
   char *_IO_read_ptr ;
   char *_IO_read_end ;
   char *_IO_read_base ;
   char *_IO_write_base ;
   char *_IO_write_ptr ;
   char *_IO_write_end ;
   char *_IO_buf_base ;
   char *_IO_buf_end ;
   char *_IO_save_base ;
   char *_IO_backup_base ;
   char *_IO_save_end ;
   struct _IO_marker *_markers ;
   struct _IO_FILE *_chain ;
   int _fileno ;
   int _flags2 ;
   __off_t _old_offset ;
   unsigned short _cur_column ;
   signed char _vtable_offset ;
   char _shortbuf[1] ;
   _IO_lock_t *_lock ;
   __off64_t _offset ;
   void *__pad1 ;
   void *__pad2 ;
   void *__pad3 ;
   void *__pad4 ;
   size_t __pad5 ;
   int _mode ;
   char _unused2[(15UL * sizeof(int ) - 4UL * sizeof(void *)) - sizeof(size_t )] ;
};
struct _job {
   struct _job *next ;
   struct _job *prev ;
   int val ;
   short priority ;
};
typedef struct _job Ele;
struct list {
   Ele *first ;
   Ele *last ;
   int mem_count ;
};
typedef struct list List;
#pragma merger("0","/tmp/cil-CCtPIRzp.i","")
#pragma merger("0","/tmp/cil-8u09MzPP.i","")
extern struct _IO_FILE *stdin ;
extern struct _IO_FILE *stdout ;
extern int fprintf(FILE * __restrict  __stream , char const   * __restrict  __format 
                   , ...) ;
extern int fscanf(FILE * __restrict  __stream , char const   * __restrict  __format 
                  , ...)  __asm__("__isoc99_fscanf")  ;
extern int malloc() ;
Ele *new_ele(int new_num ) 
{













}
List *new_list(void) 
{













}
List *append_ele(List *a_list , Ele *a_ele ) 
{



















}
Ele *find_nth(List *f_list , int n ) 
{


























}
List *del_ele(List *d_list , Ele *d_ele ) 
{























}
extern int free() ;
void free_ele(Ele *ptr ) 
{








}
int alloc_proc_num  ;
int num_processes  ;
Ele *cur_proc  ;
List *prio_queue[4]  ;
List *block_queue  ;
void schedule(void) ;
void finish_process(void) 
{















}
void finish_all_processes(void) 
{





















}
void schedule(void) 
{
























}
void upgrade_process_prio(int prio , float ratio ) 
{




























}
void unblock_process(float ratio ) 
{






















}
void quantum_expire(void) 
{














}
void block_process(void) 
{













}
Ele *new_process(int prio ) 
{













}
void add_process(int prio ) 
{









}
void init_prio_queue(int prio , int num_proc ) 
{


























}
void initialize(void) 
{







}
extern int atoi() ;
void main(int argc , char **argv ) 
{





















































































































































}