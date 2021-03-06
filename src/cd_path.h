/*
Copyright 2014, The University of Texas at Austin 
All rights reserved.

THIS FILE IS PART OF THE CONTAINMENT DOMAINS RUNTIME LIBRARY

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met: 

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer. 

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution. 

3. Neither the name of the copyright holder nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _CD_PATH_H 
#define _CD_PATH_H

/**
 * @file cd_path.h
 * @author Kyushick Lee, Song Zhang, Seong-Lyong Gong, Ali Fakhrzadehgan, Jinsuk Chung, Mattan Erez
 * @date March 2015
 *
 * @brief Containment Domains API v0.2 (C++)
 */


#include <vector>
#include "cd_handle.h"
//#include "null_cd_handle.h"
#include "cd_def_debug.h"
/** \addtogroup cd_hierarchy  
 *
 * @{
 */
namespace cd {
  namespace internal {

// TODO
// Using DCL(Double-Checking Locking), try to reduce some synch overhead for GetCDPath for multiple threads.
// DCL will check if uniquePath_ is created or not, it performs synch only when it is not created before.
// Using volatile keyword, we can let uniquePath_ be initialized correctly for multi-threading.

/** 
 * Kyushick : CDPath object should be unique for one process, so I adopted Singleton pattern for it.
 * But the difference is that I modified it for something like Memento pattern for it also, 
 * because it also performs like CareTaker object in that pattern.
 * this case CareTaker object manages the previous CDHandle*. 
 * CDHandle is mapped to Originator which is an interface
 * and Memento object is mapped to CD object
 * Actually our design is not exactly the same as Memento pattern,
 * in that user nodifies when to restore explicitly in program as well in Memento pattern,
 * but in our case, we restore program state inside our runtime system which is implicit to user (user does not know about when error happens)
 * I put this CDPath as a Class rather than just static global variable, so it can be more object-oriented
 * because related methods such as GetCurrentCD, GetRootCD, etc, is encapsulated in this CDPath class.
 * It will be more convenient to use it with "using namespace   CDPath", and the usage for those methods will be the same as before.
 */
class CDPath : public std::vector<cd::CDHandle *> {
  friend class cd::CDHandle;
  friend class CD;
  friend class HeadCD;
  friend class HandleAllReexecute;
  friend class HandleEntrySearch;
  friend class HandleEntrySend;
  friend cd::CDHandle *cd::GetCurrentCD(void);
  friend cd::CDHandle *cd::GetLeafCD(void);
  friend cd::CDHandle *cd::GetRootCD(void);
  friend cd::CDHandle *cd::GetParentCD(void);
  friend cd::CDHandle *cd::GetParentCD(int current_level);
  friend cd::CDHandle *cd::CD_Init(int numTask, int myTask, PrvMediumT prv_medium);
  friend void      cd::CD_Finalize(void);

private:
  static CDPath *uniquePath_;
public:
  CDPath(void) {
  
#if CD_MPI_ENABLED == 0 && CD_AUTOMATED == 1
//    CDHandle *root = CD_Init(1, 0);
//    CD_Begin(root);
#endif
  }
  ~CDPath(void) {
#if CD_MPI_ENABLED == 0 && CD_AUTOMATED == 1
//    CD_Complete(back());
//    CD_Finalize();
#endif
  }

public:
 /** @brief Get CDHandle of Root CD 
  *
  * \return Pointer to CDHandle of root
  */
  static CDPath* GetCDPath(void) 
  {
    if(uniquePath_ == NULL) 
      uniquePath_ = new CDPath();
    return uniquePath_;
  }

 /** @brief Get CDHandle of current (leaf) CD level. 
  *
  * \return Pointer to the CDHandle at the leaf CD level.
  */
  static CDHandle *GetCurrentCD(void) 
  {
    //printf("GetCurrentCD is called\n");
    if(uniquePath_ != NULL ) {
      //printf("path is not null\n");
      if( !uniquePath_->empty() ) {
        //printf("path is not empty\n");
        //if( uniquePath_->back()->GetExecMode() == kExecution || kReexecution ) 
        if( uniquePath_->back()->GetExecMode() != kSuspension) 
        {
          
          //printf("Active CD is %d\n", uniquePath_->back()->level() );
          return uniquePath_->back();
        }
        else {
          //printf("parent called is %d\n", uniquePath_->back()->level() );
          return GetParentCD(uniquePath_->back()->level());
        }
      }
    }
    return NULL;
  }
  
 /** @brief Get CDHandle of leaf CD level. 
  *
  * \return Pointer to the CDHandle at the leaf CD level.
  */
  static CDHandle *GetLeafCD(void) 
  {
//    printf("GetLeafCD is called\n");
    if(uniquePath_ != NULL ) {
//      printf("path is not null\n");
      if( !uniquePath_->empty() ) {
        return uniquePath_->back();
      }
    }
    return NULL;
  }

 /** @brief Get CDHandle of Root CD 
  *
  * \return Pointer to CDHandle of root
  */
  static CDHandle *GetRootCD(void)    
  { 
    if(uniquePath_ != NULL) { 
      if( !uniquePath_->empty() ) {
        return uniquePath_->front(); 
      }
    }
    return NULL;
  }

 /** @brief Get CDHandle to this CD's parent
  *
  * \return Pointer to CDHandle of parent
  */
  static CDHandle *GetParentCD(void)
  { 
    if(uniquePath_ != NULL) {
      if( uniquePath_->size() > 1 ) {
        return uniquePath_->at(uniquePath_->size()-2); 
      }
    }
    return NULL;
  }

 /** @brief Get CDHandle to this CD's parent
  *
  * \return Pointer to CDHandle of parent
  */
  static CDHandle *GetParentCD(int current_level)
  { 
    //CD_DEBUG("CDPath::GetParentCD current level : %d\n", current_level);
//    //printf("CDPath::GetParentCD current level : %d at %d\n", current_level, myTaskID);
//    if(current_level > 100) {
//      std::cout << GetCurrentCD()->GetCDName() << " / " << GetCurrentCD()->node_id() << std::endl;
//
//    }

    if(uniquePath_ != NULL ) {
      if( uniquePath_->size() > 1 ) {
        if(current_level >= 1) { 
          return uniquePath_->at(current_level-1);
        }
      }
    }
    return NULL;
  }

 /** @brief Get a CDHandle at a specific level in CDPath 
  *
  * \return Pointer to CDHandle at a level
  */
  static CDHandle *GetCDLevel(uint32_t level)
  { 
    for (auto &cdh : *uniquePath_) 
      CD_DEBUG("CD Path Check: %s %s Lv#u\n", cdh->GetName(), cdh->GetLabel(), cdh->level());
    if(uniquePath_ != NULL) {
      size_t num_levels = uniquePath_->size();
      if( num_levels > 0 ) {
        if(level < num_levels) {
          return uniquePath_->at(level); 
        } else {
          ERROR_MESSAGE("num_levels %zu <= %u level\n", num_levels, level);
//          uint32_t s = uniquePath_->size();
//          CD_DEBUG("level %u >= %u #cdobj\n", level, s);
        }
      }
    }
    return NULL;
  }

 /** @brief Get a CDHandle at the lowest level where there are more than one task. 
  *
  *  This is normally used internally.
  *
  * \return Pointer to CDHandle that has multiple tasks in it.
  */
  static CDHandle *GetCoarseCD(CDHandle *curr_cdh) {
    while( curr_cdh->task_size() == 1 ) {
      if(curr_cdh == GetRootCD()) {
        //CD_DEBUG("There is a single task in the root CD\n");
//        assert(0);
        return curr_cdh;
      }
      curr_cdh = CDPath::GetParentCD(curr_cdh->level());
    } 
    return curr_cdh;
  }

//  CD *GetCoarseCD(CD* curr_cd);

  static CDHandle *GetNullCD(void) {
    return null_cd;
  }

};

/** @} */ // End cd_hierarchy



/*
class CDPath {
private:
//  volatile static CDPath uniquePath_;
  static std::atomic<CDPath*> uniquePath_;
  static std::mutex mutex_;
  CDPath(void) {}
public:
  static CDPath* GetCDPath(void) {
    CDPath* tmp = uniquePath_.load(std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(tmp == nullptr) {
      std::lock_quard<std::mutex> lock(mutex_);
      tmp = uniquePath_.load(std::memory_order_relaxed);
      if(tmp == nullptr) {
        tmp = new CDPath();
        std::atomic_thread_fence(std::memory_order_release);
        uniquePath_.store(tmp, std::memory_order_relaxed);
      }
    }
    return tmp;
  }
};
*/

  } // namespace internal ends
} // namespace cd ends
//#if CD_MPI_ENABLED == 0 && CD_AUTOMATED == 1
//extern CDPath cd_path;
//#endif


#endif
