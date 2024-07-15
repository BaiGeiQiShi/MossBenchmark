                                                                            
   
                   
                                               
   
                                                                         
                                                                        
   
   
                  
                                                
   
                                                                            
   
#include "postgres.h"

#include "access/multixact.h"
#include "access/twophase_rmgr.h"
#include "pgstat.h"
#include "storage/lock.h"
#include "storage/predicate.h"

const TwoPhaseCallback twophase_recover_callbacks[TWOPHASE_RM_MAX_ID + 1] = {
    NULL,                                      
    lock_twophase_recover,                   
    NULL,                                      
    multixact_twophase_recover,                   
    predicatelock_twophase_recover                    
};

const TwoPhaseCallback twophase_postcommit_callbacks[TWOPHASE_RM_MAX_ID + 1] = {
    NULL,                                      
    lock_twophase_postcommit,                
    pgstat_twophase_postcommit,                
    multixact_twophase_postcommit,                
    NULL                                              
};

const TwoPhaseCallback twophase_postabort_callbacks[TWOPHASE_RM_MAX_ID + 1] = {
    NULL,                                     
    lock_twophase_postabort,                
    pgstat_twophase_postabort,                
    multixact_twophase_postabort,                
    NULL                                             
};

const TwoPhaseCallback twophase_standby_recover_callbacks[TWOPHASE_RM_MAX_ID + 1] = {
    NULL,                                      
    lock_twophase_standby_recover,           
    NULL,                                      
    NULL,                                         
    NULL                                              
};
