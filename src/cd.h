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

#ifndef _CD_H 
#define _CD_H
#include "cd_global.h"
#include "cd_handle.h"
#include "cd_entry.h"
#include "util.h"
#include "cd_id.h"
#include "cd_log_handle.h"
#if _WORK
#include "transaction.h"
#endif

#include <cassert>
#include <list>
#include <vector>
#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <setjmp.h>
#include <ucontext.h>
#include <map>
//#include <unordered_map>
#include <functional>
//#include <array>

using namespace cd;

/// TODO Implement serialize and deserialize of this instance
class cd::CD {
    /// The friends of CD class
    friend class cd::RegenObject;   
    friend class cd::CDEntry;   
  public:
    /// CD class internal enumerators 
    enum CDExecMode  {kExecution=0, 
                      kReexecution, 
                      kSuspension
                     };

    enum CtxtPrvMode { kExcludeStack=0, 
                       kIncludeStack
                     };

    enum CDInternalErrT { kOK=0, 
                          kExecModeError, 
                          kEntryError 
                        };  

  protected: 
    CDID cd_id_;
  public:

    // Label for Begin/Complete pair. It is mainly for Loop interation.
    // The Begin/Complete pair that has the same computation will have the same label_
    // and we can optimize CD with this label_ later.
    std::vector<std::string> label_;
    
    // Name of this CD
    std::string     name_;

    // Flag for Strict or Relexed CD
    CDType          cd_type_;

    /// Set rollback point and options
    CtxtPrvMode     ctxt_prv_mode_;
    ucontext_t      ctxt_;
    jmp_buf         jump_buffer_;
    uint64_t        option_save_context_; 

    /// Flag for normal execution or reexecution.
    CDExecMode      cd_exec_mode_;
    int             num_reexecution_;

    /// Detection-related meta data
    // bit vector that will mask error types that can be handled in this CD
    uint64_t        sys_detect_bit_vector_;

    // Registered user-defined error detection routine
    std::list<std::function<int(void)>> usr_detect_func_list_;  //custom_detect_func;
    

    /// FIXME later this should be map or list, 
    /// sometimes it might be better to have map structure 
    /// Sometimes list would be beneficial depending on 
    /// how frequently search functionality is required.
    /// Here we define binary search tree structure... 
    /// Then insert only the ones who has ref names.

    /// 09.23.2014
    /// There are two data structure for each CD entry. 
    /// entry_directory_ is a super set of entry_directory_map_
    /// entry_directory_map_ is for preservation via reference
    /// If ref_name is passed by preservation call, it means it allows this entry to be referred to by children CDs.
    std::list<CDEntry> entry_directory_; 
    
    // Only CDEntries that has refname will be pushed into this data structure for later quick search.
    std::map<std::string, CDEntry*> entry_directory_map_;   
    
/*  
09.23.2014 
It is not complete yet. I am thinking of some way to implement like cd_advance semantic which should allow
update the preserved data. 
 __________________________________________________________________________________________________________
|_case_|_entry_name_|_ref_name_|______________Meaning__________________|___________Property________________|
|      |            |          |I copied app data for preservation     | Modification in my preserved      |
|  0   |      X     |     X    |but don't allow reference from         | copy of data is fine.             |
|______|____________|__________|my_children.___________________________|___________________________________| 
|      |            |          |I do not have a copy of app data       | Modification in preserved data    |
|  1   |      X     |     O    |but will refer to my ancestor's entry  | is not allowed.                   |
|______|____________|__________|_______________________________________|___________________________________|
|      |            |          |I copied app data for preservation     | Modification is not fine becuase  | 
|  2   |      O     |     X    |and also allow reference from          | my children CD may refer to my    |
|______|____________|__________|my_children.___________________________|_preserved_data.___________________| 
|      |            |          |I do not have a copy of app data       | Modification is not               |
|  3   |      O     |     O    |but will refer to my ancestor's entry  |                                   |
|______|____________|__________|and_also_allow_reference_from_children_|___________________________________|
*/

    /// This shall be used for re-execution. 
    /// We will restore the value one by one.
    /// Should not be CDEntry*. 
    std::list<CDEntry>::iterator iterator_entry_;   
  
    //  std::vector<cd_log> log_directory_;
    //  pthread_mutex_t mutex_;      //
    //  pthread_mutex_t log_directory_mutex_;

    // handler for log-related things
    CDLogHandle log_handle_;


  public:
    CD();

    CD(cd::CDHandle* cd_parent, 
       const char* name, 
       CDID cd_id, 
       CDType cd_type, 
       uint64_t sys_bit_vector);

    virtual ~CD();

    void Initialize(cd::CDHandle* cd_parent, 
                    const char* name, 
                    CDID cd_id, 
                    CDType cd_type, 
                    uint64_t sys_bit_vector);
    void Init(void);
  
    CDID GetCDID(void) { return cd_id_; }

    CD* Create(cd::CDHandle* cd_parent, 
               const char* name, 
               const CDID& cd_id, 
               CDType cd_type, 
               uint64_t sys_bit_vector, 
               CDErrT* cd_err);

    CDErrT Destroy();

    CDErrT Begin(bool collective=true, 
                 const char* label=0);

    CDErrT Complete(bool collective=true, 
                    bool update_preservations=true);


  /// Jinsuk: Really simple preservation function. 
  /// This will use default storage. 
  /// It can be set when creating this instance, 
  /// or later by calling set_ something function. 
  
    CDErrT Preserve(void *data,                   // address in the local process 
                    uint64_t len_in_bytes,        // data size to preserve
                    CDPreserveT preserve_mask=kCopy, // preservation method
                    const char *my_name=0,        // data name
                    const char *ref_name=0,       // reference name
                    uint64_t ref_offset=0,        // reference offset
                    const RegenObject * regen_object=0, // regen object
                    PreserveUseT data_usage=kUnsure);   // for optimization
  
    // Collective version
    // Kyushick : Why do we need collective Preserve call as an API?
    // I think we need collective method when CDs want to preserve data to Global file system
    // So user set some medium type
    CDErrT Preserve(CDEvent &cd_event,            // Event object to synch
                    void *data_ptr, 
                    uint64_t len, 
                    CDPreserveT preserve_mask=kCopy, 
                    const char *my_name=0, 
                    const char *ref_name=0, 
                    uint64_t ref_offset=0, 
                    const RegenObject *regen_object=0, 
                    PreserveUseT data_usage=kUnsure);
  
    CDErrT Detect(); 
    CDErrT Restore();
  
//  DISCUSS: Jinsuk: About longjmp setjmp: By running some experiement, 
//  I confirm that this only works when stack is just there. 
//  That means, if we want to jump to a function context 
//  which does not exist in stack anymore, 
//  you will eventually get segfault. 
//  Thus, CD begin and complete should be in the same function context,
//  or at least complete should be deper in the stack 
//  so that we can jump back to the existing stack pointer. 
//  I think that Just always pairing them in the same function is much cleaner. 
  
    CDErrT Assert(bool test);
  
    CDErrT Reexecute(void);
  
    // Utilities -----------------------------------------------------

    // GetPlaceToPreserve() decides actual medium to preserve data.
    // TODO Currently, it preserves in memory for everything.
    // we can change it to preserve file by flag when we compile cd runtime. (with MEMORY=0)
    // We need some good strategy to decide the most efficient medium of the CD for preservation.
    PrvMediumT GetPlaceToPreserve(void);

    // These should be virtual because I am going to use polymorphism for those methods.
    virtual CDErrT Stop(cd::CDHandle* cdh=NULL);
    virtual CDErrT Resume(void);
    virtual CDErrT AddChild(cd::CDHandle* cd_child);
    virtual CDErrT RemoveChild(cd::CDHandle* cd_child);
  
    CDInternalErrT InternalPreserve(void *data, 
                                    uint64_t len_in_bytes,
                                    CDPreserveT preserve_mask, 
                                    const char *my_name, 
                                    const char *ref_name, 
                                    uint64_t ref_offset, 
                                    const RegenObject * regen_object, 
                                    PreserveUseT data_usage);
  
    // copy should happen for the part that is needed.. 
    // so serializing entire CDEntry class does not make sense. 

    // Search CDEntry with entry_name given. It is needed when its children preserve data via reference and search through its ancestors. If it cannot find in current CD object, it outputs NULL 
    CDEntry* InternalGetEntry(std::string entry_name); 
  
    // This comment is previous one, so may be confusing for current design. 
    //
    // When Restore is called, it should be copied the only part ... smartly. 
    // It means we need to send a copy request to remote node. 
    // If it is in PGAS this might be a little easier to handle. 
    // For MPI, it could be a little tricky but anyways possible.
    // When it is serialized they should not do the deep copy for datahandle... 
    // so basically address information is transfered but not the actual data... 
    // FIXME for now let's just consider single process or single thread environment.. 
    // so we can always return CDEntry without worries. 
    // But later we will need to trasfer this object 
    // and then new CDEntry and then return with proper value. 
    
    // Actual malloced data or created file for the preservation is deleted in this routine. 
    void DeleteEntryDirectory(void);
    
 };


class cd::HeadCD : public cd::CD {
  public:
    /// Link information of CD hierarchy   
    /// This is a kind of CD object but represents the corresponding task group of each CD
    /// Therefore, CDTree is generated with CD object. 
    /// parent CD object is always in the same process.
    /// But Head's parent CD object is in the Head process of parent CD.
    /// CDHandle cd_parent_ should be an accessor for this Head process.
    /// Regarding children CDs, 
    /// this Head's CD has actual copy of the Head children's CD object.
    /// If children CD is gone, this Head CD sends children CD.
    /// So, when we create CDs, 
    /// we should send Head CDHandle and its CD to its parent CD
    std::list<cd::CDHandle*> cd_children_;
    cd::CDHandle*            cd_parent_;

    HeadCD();
    HeadCD(cd::CDHandle* cd_parent, 
             const char* name, 
             CDID cd_id, 
             CDType cd_type, 
             uint64_t sys_bit_vector);
    virtual ~HeadCD();

    cd::CDHandle*  cd_parent(void);
    void           set_cd_parent(cd::CDHandle* cd_parent);

    virtual CDErrT Stop(cd::CDHandle* cdh=NULL);
    virtual CDErrT Resume(void); // Does this make any sense?
    virtual CDErrT AddChild(cd::CDHandle* cd_child); 
    virtual CDErrT RemoveChild(cd::CDHandle* cd_child); 
};


#endif
