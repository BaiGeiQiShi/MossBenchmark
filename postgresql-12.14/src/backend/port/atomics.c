                                                                            
   
             
                                                     
   
                                                                         
   
   
                  
                                
   
                                                                            
   
#include "postgres.h"

#include "miscadmin.h"
#include "port/atomics.h"
#include "storage/spin.h"

#ifdef PG_HAVE_MEMORY_BARRIER_EMULATION
#ifdef WIN32
#error "barriers are required (and provided) on WIN32 platforms"
#endif
#include <signal.h>
#endif

#ifdef PG_HAVE_MEMORY_BARRIER_EMULATION
void
pg_spinlock_barrier(void)
{
     
                                                                          
               
     
                                                                          
                                                                            
                                                                             
     
  (void)kill(PostmasterPid, 0);
}
#endif

#ifdef PG_HAVE_COMPILER_BARRIER_EMULATION
void
pg_extern_compiler_barrier(void)
{
                  
}
#endif

#ifdef PG_HAVE_ATOMIC_FLAG_SIMULATION

void
pg_atomic_init_flag_impl(volatile pg_atomic_flag *ptr)
{
  StaticAssertStmt(sizeof(ptr->sema) >= sizeof(slock_t), "size mismatch of atomic_flag vs slock_t");

#ifndef HAVE_SPINLOCKS

     
                                                                           
                                                                            
                                                      
     
  s_init_lock_sema((slock_t *)&ptr->sema, true);
#else
  SpinLockInit((slock_t *)&ptr->sema);
#endif

  ptr->value = false;
}

bool
pg_atomic_test_set_flag_impl(volatile pg_atomic_flag *ptr)
{
  uint32 oldval;

  SpinLockAcquire((slock_t *)&ptr->sema);
  oldval = ptr->value;
  ptr->value = true;
  SpinLockRelease((slock_t *)&ptr->sema);

  return oldval == 0;
}

void
pg_atomic_clear_flag_impl(volatile pg_atomic_flag *ptr)
{
  SpinLockAcquire((slock_t *)&ptr->sema);
  ptr->value = false;
  SpinLockRelease((slock_t *)&ptr->sema);
}

bool
pg_atomic_unlocked_test_flag_impl(volatile pg_atomic_flag *ptr)
{
  return ptr->value == 0;
}

#endif                                     

#ifdef PG_HAVE_ATOMIC_U32_SIMULATION
void
pg_atomic_init_u32_impl(volatile pg_atomic_uint32 *ptr, uint32 val_)
{
  StaticAssertStmt(sizeof(ptr->sema) >= sizeof(slock_t), "size mismatch of atomic_uint32 vs slock_t");

     
                                                                          
                                                
     
#ifndef HAVE_SPINLOCKS
  s_init_lock_sema((slock_t *)&ptr->sema, true);
#else
  SpinLockInit((slock_t *)&ptr->sema);
#endif
  ptr->value = val_;
}

void
pg_atomic_write_u32_impl(volatile pg_atomic_uint32 *ptr, uint32 val)
{
     
                                                                        
                                                                             
                                                                  
     
  SpinLockAcquire((slock_t *)&ptr->sema);
  ptr->value = val;
  SpinLockRelease((slock_t *)&ptr->sema);
}

bool
pg_atomic_compare_exchange_u32_impl(volatile pg_atomic_uint32 *ptr, uint32 *expected, uint32 newval)
{
  bool ret;

     
                                                                          
                                                                        
                                                                            
                                                                         
                                                                        
           
     
  SpinLockAcquire((slock_t *)&ptr->sema);

                                      
  ret = ptr->value == *expected;
  *expected = ptr->value;
  if (ret)
  {
    ptr->value = newval;
  }

                        
  SpinLockRelease((slock_t *)&ptr->sema);

  return ret;
}

uint32
pg_atomic_fetch_add_u32_impl(volatile pg_atomic_uint32 *ptr, int32 add_)
{
  uint32 oldval;

  SpinLockAcquire((slock_t *)&ptr->sema);
  oldval = ptr->value;
  ptr->value += add_;
  SpinLockRelease((slock_t *)&ptr->sema);
  return oldval;
}

#endif                                    

#ifdef PG_HAVE_ATOMIC_U64_SIMULATION

void
pg_atomic_init_u64_impl(volatile pg_atomic_uint64 *ptr, uint64 val_)
{
  StaticAssertStmt(sizeof(ptr->sema) >= sizeof(slock_t), "size mismatch of atomic_uint64 vs slock_t");

     
                                                                          
                                                
     
#ifndef HAVE_SPINLOCKS
  s_init_lock_sema((slock_t *)&ptr->sema, true);
#else
  SpinLockInit((slock_t *)&ptr->sema);
#endif
  ptr->value = val_;
}

bool
pg_atomic_compare_exchange_u64_impl(volatile pg_atomic_uint64 *ptr, uint64 *expected, uint64 newval)
{
  bool ret;

     
                                                                          
                                                                        
                                                                            
                                                                         
                                                                        
           
     
  SpinLockAcquire((slock_t *)&ptr->sema);

                                      
  ret = ptr->value == *expected;
  *expected = ptr->value;
  if (ret)
  {
    ptr->value = newval;
  }

                        
  SpinLockRelease((slock_t *)&ptr->sema);

  return ret;
}

uint64
pg_atomic_fetch_add_u64_impl(volatile pg_atomic_uint64 *ptr, int64 add_)
{
  uint64 oldval;

  SpinLockAcquire((slock_t *)&ptr->sema);
  oldval = ptr->value;
  ptr->value += add_;
  SpinLockRelease((slock_t *)&ptr->sema);
  return oldval;
}

#endif                                    
