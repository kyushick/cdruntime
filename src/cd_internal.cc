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

#include "cd_config.h"
#include "cd_internal.h"
#include "cd_path.h"
//#include "packer.h"
//#include "unpacker.h"
#include "cd_def_debug.h"
#include "phase_tree.h"
#include "runtime_info.h"
#include "packer.h"
#include "serializable.h"
//#include "machine_specific.h"
#include <setjmp.h>
using namespace cd;
using namespace common;
using namespace cd::internal;
using namespace ::interface;
using namespace cd::logging;
using namespace std;

#define BUGFIX_0327 1
#define _LOG_PROFILING 0
#define USE_ALLOC_SHM 1

#define LULESH_DEBUG
//#undef LULESH_DEBUG
#ifdef LULESH_DEBUG
#include "debug_user_data.h"
#endif

ProfMapType   common::profMap;
bool tuned::tuning_enabled = false;
uint32_t cd::new_phase = 0;
bool cd::just_reexecuted;
bool cd::first_complete = false;

CD_CLOCK_T cd::log_begin_clk;
CD_CLOCK_T cd::log_end_clk;
CD_CLOCK_T cd::log_elapsed_time;

uint64_t cd::gen_object_id=0;
uint64_t cd::state = 0;
// the very first failed phase
int64_t prev_phase = HEALTHY;

/*************************************************
 * failed_phase and failed_seqID are global flags
 * that indicate the state of a CD.
 * This is set when the first detection of error
 * and reset when CD commits.
 * rollback_point_ is also global flag
 * but it is reset to INVALID_ROLLBACK_POINT
 * when reexec is about to begin.
 * This is for checking another error during rollback.
 *************************************************/
int64_t cd::failed_phase = HEALTHY;
int64_t cd::failed_seqID = HEALTHY;
//int32_t cd::failed_level = HEALTHY;
CDFlagT *CD::rollback_point_ = NULL; 
#if _MPI_VER
CDFlagT *CD::pendingFlag_ = NULL; 
CDMailBoxT CD::pendingWindow_;
CDMailBoxT CD::rollbackWindow_;
#endif
//bool CD::need_reexec = false;
bool CD::need_escalation = false;
/*************************************************/

int iterator_entry_count=0;
std::unordered_map<string,packer::CDEntry*> cd::str_hash_map;
std::unordered_map<string,packer::CDEntry*>::hasher cd::str_hash;
std::map<ENTRY_TAG_T, string> cd::tag2str;
std::list<CommInfo> CD::entry_req_;
std::map<ENTRY_TAG_T, CommInfo> CD::entry_request_req_;
std::map<ENTRY_TAG_T, CommInfo> CD::entry_recv_req_;
std::list<EventHandler *> CD::cd_event_;
std::map<uint32_t, uint32_t> Util::object_id;
std::map<string, CDHandle *> CD::access_store_;
std::map<uint32_t, CDHandle *> CD::delete_store_;
//map<string, uint32_t> CD::exec_count_;
//EntryDirType::hasher cd::str_hash;
//std::map<uint64_t, string> cd::phase2str;
//std::unordered_map<uint64_t, string> tag2str;

//unordered_map<string,pair<int,int>> CD::profMap_;

// Node-local specifics
ColorT cd::node_color;
GroupT cd::node_group;
int cd::node_rank = 0;
int cd::node_size = 1;
int cd::head_in_node = 0;
int cd::node_name_sz=32;
char cd::node_name[MPI_MAX_PROCESSOR_NAME];
////////////////////////

void cd::internal::InitFileHandle(bool make_dir)
{
  packer::Initialize();
  char *prv_base_str = getenv( "CD_PRV_BASEPATH" );
  if(prv_base_str != NULL) {
    strcpy(packer::FileHandle::basepath, prv_base_str);
  }

  if(make_dir) {
    MakeFileDir(packer::FileHandle::basepath);
  }
}


void cd::internal::Initialize(void)
{
#if _MPI_VER
  MPI_Get_processor_name(cd::node_name, &cd::node_name_sz);
  MPI_Comm_split_type(GetRootCD()->color(), MPI_COMM_TYPE_SHARED, 
                      cd::myTaskID, 
                      MPI_INFO_NULL, &cd::node_color);
  PMPI_Comm_group(cd::node_color, &cd::node_group); 
  MPI_Comm_rank(cd::node_color, &cd::node_rank);
  MPI_Comm_size(cd::node_color, &cd::node_size);
  int local_head_in_node = 0;
  PMPI_Group_translate_ranks(cd::node_group, 1, &local_head_in_node, cd::whole_group, &head_in_node);
//  printf("[%s %d] node:%d/%d (%d)\n", cd::node_name, cd::myTaskID, 
//          cd::node_rank, cd::node_size, head_in_node);
  #if USE_ALLOC_SHM      
  /*int MPI_Win_allocate(MPI_Aint size, int disp_unit,
   *                     MPI_Info info, 
   *                     MPI_Comm comm, 
   *                     void *baseptr, MPI_Win *win)
   *  int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, 
   *                     MPI_Info info, 
   *                     MPI_Comm comm, MPI_Win *win)
   * int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit,
   *                             MPI_Info info, MPI_Comm comm, 
   *                             void *baseptr, MPI_Win *win);
   * 
   */
  MPI_Info win_info;
  MPI_Info_create(&win_info);
  MPI_Info_set(win_info, "alloc_shared_noncontig", "true");
  PMPI_Win_allocate_shared(sizeof(CDFlagT), sizeof(CDFlagT), 
                           win_info /*MPI_INFO_NULL*/, 
                           GetRootCD()->color(), 
                           &CD::pendingFlag_, &CD::pendingWindow_);
    PMPI_Win_allocate_shared(sizeof(CDFlagT), sizeof(CDFlagT), 
                             win_info /*MPI_INFO_NULL*/, 
                             GetRootCD()->color(), 
                             &CD::rollback_point_, &CD::rollbackWindow_);
    *CD::rollback_point_ = INVALID_ROLLBACK_POINT;
  MPI_Aint rp_size=-1;
  int rp_dist=-1;
//  if(cd::node_rank == 0) {
//    PMPI_Win_allocate_shared(sizeof(CDFlagT), sizeof(CDFlagT), 
//                             win_info /*MPI_INFO_NULL*/, 
//                             GetRootCD()->color(), 
//                             &CD::rollback_point_, &CD::rollbackWindow_);
//    *CD::rollback_point_ = INVALID_ROLLBACK_POINT;
//
//  } else {
//    PMPI_Win_allocate_shared(0, sizeof(CDFlagT), 
//                             win_info /*MPI_INFO_NULL*/, 
//                             GetRootCD()->color(), 
//                             &CD::rollback_point_, &CD::rollbackWindow_);
//    MPI_Win_shared_query(CD::rollbackWindow_, head_in_node, &rp_size, &rp_dist, &CD::rollback_point_);
//  }

//  printf("[%s %3d] node:%2d/%2d (%3d), rp:%3ld,%3d,%p, pd:%p\n", cd::node_name, cd::myTaskID, 
//          cd::node_rank, cd::node_size, head_in_node,
//          rp_size, rp_dist, CD::rollback_point_, CD::pendingFlag_);

//  PMPI_Win_allocate_shared(sizeof(CDFlagT), sizeof(CDFlagT), 
//                           MPI_INFO_NULL, GetRootCD()->color(), 
//                           CD::pendingFlag_, &CD::pendingWindow_);
  #else
  PMPI_Alloc_mem(sizeof(CDFlagT), MPI_INFO_NULL, &(CD::pendingFlag_));
  PMPI_Alloc_mem(sizeof(CDFlagT), MPI_INFO_NULL, &(CD::rollback_point_));

  PMPI_Win_create(CD::pendingFlag_, sizeof(CDFlagT), sizeof(CDFlagT), 
                 MPI_INFO_NULL, GetRootCD()->color(), &CD::pendingWindow_);
  PMPI_Win_create(CD::rollback_point_, sizeof(CDFlagT), sizeof(CDFlagT), 
                 MPI_INFO_NULL, GetRootCD()->color(), &CD::rollbackWindow_);
  *CD::rollback_point_ = INVALID_ROLLBACK_POINT;
  #endif
#else // Single process
  CD::rollback_point_ = new CDFlagT(INVALID_ROLLBACK_POINT);
//  CD::pendingFlag_ = new CDFlagT(0);
#endif

  // Initialize pending flag
  *CD::pendingFlag_ = 0;
  MPI_Info_free( &win_info );

#if _MPI_VER
  // Tag-related
  void *v;
  int flag;
//  ENTRY_TAG_T max_tag_size = 2147483647;

//  PMPI_Comm_set_attr( MPI_COMM_WORLD, MPI_TAG_UB, &max_tag_size ); // It does not work for MPI_TAG_UB 
  PMPI_Comm_get_attr( MPI_COMM_WORLD, MPI_TAG_UB, &v, &flag ); 

  ENTRY_TAG_T predefined_max_tag_bits = *(int *)v + 1;

//  cout << "predefined_max_tag_bits : " << predefined_max_tag_bits << endl;

  int cnt = sizeof(ENTRY_TAG_T)*8;
  while(cnt >= 0) {
    if((predefined_max_tag_bits >> cnt) & 0x1) {
      max_tag_bit = cnt;
      break;
    }
    else {
      cnt--;
    }
  }
//  max_tag_bit = 30;
//  cout << "max bits : " << max_tag_bit << endl;
  max_tag_level_bit = 6;
  max_tag_rank_bit = max_tag_bit-max_tag_level_bit-1;
  max_tag_task_bit = max_tag_rank_bit/2;

#endif
}

void cd::internal::Finalize(void)
{
#if _MPI_VER
  PMPI_Win_free(&CD::pendingWindow_);
  PMPI_Win_free(&CD::rollbackWindow_);
  #if USE_ALLOC_SHM
  // Do nothing? 
  #else
  PMPI_Free_mem(CD::pendingFlag_);
  PMPI_Free_mem(CD::rollback_point_);
  #endif
#else
  free(CD::rollback_point_);
//  free(CD::pendingFlag_);
#endif
  packer::Finalize();  
}


/// Actual CD Object only exists in a single node and in a single process.
/// Potentially copy of CD Object can exist but it should not be used directly. 
/// We also need to think about when maintaining copy of CD objects, 
/// how are they going to be synchrnoized  (the values, and the entries and all that)

/// Handle would be an accessor to this object. (Kind of an interface for these)
/// If CD Object resides in current process then, it is act as pointer. 
/// If CD Object does not reside in current process, then it will do the appropriate things. 

/// Right now I am making single threaded version so don't consider CDHandle too much 
/// Complication will be delayed until we start developing multithreaded or multi node version.

/// TODO: Desgin decision on CD Tree
/// CD Tree: If CD tree is managed by N nodes, 
/// node (cd_id % N) is where we need to ask for insert, delete, and peek operations. 
/// Distributed over N nodes, and the guide line is cd_id. 
/// If every node that exists handles cd tree, then this means cd tree is always local. 
/// Root CD will, in this case, be located at Node 0 for example. 
/// TODO: how do we implement preempt stop function? 

/// ISSUE Kyushick
/// level+1 when we create children CDs.
/// Originally we do that inside CD object creator, 
/// but it can be inappropriate in some case.
/// ex. cd object can be created by something else I guess..
/// So I think it would be more desirable to increase level
/// inside Create() 
CD::CD(void)
//  : file_handle_()
#if CD_MPI_ENABLED
    : incomplete_log_(DEFAULT_INCOMPL_LOG_SIZE)
#endif
{
  //stack_entry_ = NULL;
  Init();  
  is_window_reused_ = false;
  recreated_ = false;
  cd_type_ = kStrict;
  prv_medium_ = DEFAULT_MEDIUM;
  name_ = INITIAL_CDOBJ_NAME; 
  label_ = string(INITIAL_CDOBJ_LABEL); 
  sys_detect_bit_vector_ = 0;
  // Assuming there is only one CD Object across the entire system we initilize cd_id info here.
  cd_id_ = CDID();

  // Kyushick: Object ID should be unique for the copies of the same object?
  // For now, I decided it as an unique one
  cd_id_.object_id_ = Util::GenCDObjID();

  recoverObj_ = new RecoverObject;
//  need_reexec = false;
//  *rollback_point_ = 0;
  
#if comm_log
  // SZ
  // create instance for comm_log_ptr_ for relaxed CDs
  // if no parent assigned, then means this is root, so log mode will be kGenerateLog at creation point
  assert(comm_log_ptr_ == NULL);

  LOG_DEBUG("Set child_seq_id_ to 0\n");
  child_seq_id_ = 0;
  SetCDLoggingMode(kStrictCD);

#endif
}

// FIXME: only acquire root handle when needed. 
// Most of the time, this might not be required.
// Kyushick : I cannot fully understand this comment....
CD::CD(CDHandle *cd_parent, 
       const char *name, 
       const CDID& cd_id, 
       CDType cd_type, 
       PrvMediumT prv_medium, 
       uint64_t sys_bit_vector)
 :  cd_id_(cd_id), entry_directory_(32, NULL, NULL, cd::data_grow_unit, (MASK_MEDIUM(prv_medium) == kDRAM)? kVolatile : kMPIFile)
    //,
//    file_handle_(prv_medium, 
//                 ((cd_parent!=NULL)? cd_parent->ptr_cd_->file_handle_.GetBasePath() : FilePath::global_prv_path_), 
//                 cd_id.GetStringID() )
#if CD_MPI_ENABLED
    , incomplete_log_(DEFAULT_INCOMPL_LOG_SIZE)
#endif
{
  //stack_entry_ = NULL;
  Init(); 
#if CD_MPI_ENABLED
  PMPI_Comm_group(cd_id_.node_id_.color_, &(cd_id_.node_id_.task_group_));
#endif
  is_window_reused_ = false;

  cd_type_ = cd_type; 
  prv_medium_ = static_cast<PrvMediumT>(MASK_MEDIUM(prv_medium)); 
  name_ = (strcmp(name, NO_LABEL) == 0)? cd_id_.cd_name_.GetString() : name; 
  sys_detect_bit_vector_ = sys_bit_vector; 
//  entry_directory_.data_->SetMode(kVolatile);
  // FIXME 
  // cd_id_ = 0; 
  // We need to call which returns unique id for this cd. 
  // the id is recommeneded to have pre-allocated sections per node. 
  // This way, we don't need to have race condition to get unique id. 
  // Instead local counter is used to get the unique id.

  // Kyushick: Object ID should be unique for the copies of the same object?
  // For now, I decided it as an unique one
#if 1
  cd_id_.object_id_ = Util::GenCDObjID();
#else
  CD_DEBUG("exec_count[%s] = %u\n", cd_id_.GetPhaseID().c_str(), exec_count_[cd_id_.GetPhaseID()]);
  if(exec_count_[cd_id_.GetPhaseID()] == 0) {
    cd_id_.object_id_ = Util::GenCDObjID(cd_id_.level());
  }
  exec_count_[cd_id_.GetPhaseID()]++;
  CD_DEBUG("exec_count[%s] = %u\n", cd_id_.GetPhaseID().c_str(), exec_count_[cd_id_.GetPhaseID()]);
#endif
  // FIXME 
  // cd_id_.level() = parent_cd_id.level() + 1;
  // we need to get parent id ... 
  // but if they are not local, this might be hard to acquire.... 
  // perhaps we should assume that cd_id is always store in the handle ...
  // Kyushick : if we pass cd_parent handle to the children CD object when we create it,
  // this can be resolved. 

  InternalInitialize(cd_parent);
}

HeadCD::HeadCD()
{
  error_occurred = false;
}

HeadCD::HeadCD(CDHandle *cd_parent, 
               const char *name, 
               const CDID& cd_id, 
               CDType cd_type, 
               PrvMediumT prv_medium, 
               uint64_t sys_bit_vector)
  : CD(cd_parent, name, cd_id, cd_type, prv_medium, sys_bit_vector)
{
  error_occurred = false;
//  cd_parent_ = cd_parent;
}


double init_timer = 0.0;
void CD::Initialize(CDHandle *cd_parent, 
                    const char *name, 
                    const CDID& cd_id, 
                    CDType cd_type, 
                    PrvMediumT prv_medium, 
                    uint64_t sys_bit_vector)
{
  double tstart = CD_CLOCK();
  // In this function, it should not initialize cd_id here
//  file_handle_.CloseFile();
//  file_handle_.Initialize(
//      prv_medium, 
//      ((cd_parent!=NULL)? cd_parent->ptr_cd_->file_handle_.GetBasePath() : FilePath::global_prv_path_), 
//      cd_id_.GetStringID() + string("_XXXXXX") );
#if CD_MPI_ENABLED      
  incomplete_log_.clear();
#endif
  Init();  

  init_timer += CD_CLOCK() - tstart;
  
  entry_directory_.Init(true);

  cd_type_ = cd_type; 
  prv_medium_ = static_cast<PrvMediumT>(MASK_MEDIUM(prv_medium)); 
  if(name != NULL) {
    name_ = name;
  } else {
    name_ = cd_id_.GetStringID();
  }
  sys_detect_bit_vector_ = sys_bit_vector; 

#if comm_log
  if(comm_log_ptr_ != NULL) {
    delete comm_log_ptr_;
  }
#endif

#if CD_LIBC_LOG_ENABLED
  if(libc_log_ptr_ != NULL) {
    delete libc_log_ptr_;
  }
#endif

#if 1
  cd_id_.object_id_ = Util::GenCDObjID();
#else
  CD_DEBUG("exec_count[%s] = %u\n", cd_id_.GetPhaseID().c_str(), exec_count_[cd_id_.GetPhaseID()]);
  if(exec_count_[cd_id_.GetPhaseID()] == 0) {
    cd_id_.object_id_ = Util::GenCDObjID(cd_id_.level());
  }
  exec_count_[cd_id_.GetPhaseID()]++;
  CD_DEBUG("exec_count[%s] = %u\n", cd_id_.GetPhaseID().c_str(), exec_count_[cd_id_.GetPhaseID()]);
#endif

  InternalInitialize(cd_parent);

#if CD_MPI_ENABLED
  InitializeMailBox();
#endif
}

void CD::InternalInitialize(CDHandle *cd_parent)
{
  label_ = string(INITIAL_CDOBJ_LABEL);
  recoverObj_ = new RecoverObject;

  //iterator_entry_ = entry_directory_.begin();
  // FIXME
  restore_count_ = 0;
  preserve_count_ = 0;

  if(cd_parent != NULL) {
    if(cd_parent->ptr_cd_->reexecuted_ == true || cd_parent->ptr_cd_->recreated_ == true) {
      recreated_ = true;
    }
    else {
      recreated_ = false;
    }
  }
  else {
    recreated_ = false;
  }

#if comm_log
  // SZ
  // create instance for comm_log_ptr_
  // comm_log is per thread per CD
  // if cd_parent is not NULL, then inherit comm log mode from parent,
  // otherwise means current cd is a root cd, so comm log mode is to generate log
  if (cd_parent != NULL)
  {
    // Only create CommLog object for relaxed CDs
    assert(comm_log_ptr_ == NULL);
    if (MASK_CDTYPE(cd_type_) == kRelaxed)
    {
      //SZ: if parent is a relaxed CD, then copy comm_log_mode from parent
      if (MASK_CDTYPE(cd_parent->ptr_cd()->cd_type_) == kRelaxed)
      {
        CommLogMode parent_log_mode = cd_parent->ptr_cd()->comm_log_ptr_->GetCommLogMode();
        LOG_DEBUG("With cd_parent (%p) relaxed CD, creating CommLog with parent's mode %d\n", cd_parent, parent_log_mode);
        comm_log_ptr_ = new CommLog(this, parent_log_mode);
        if (parent_log_mode == kGenerateLog){
          SetCDLoggingMode(kRelaxedCDGen);
        }
        else{
          SetCDLoggingMode(kRelaxedCDRead);
        }
      }
      //SZ: if parent is a strict CD, then child CD is always in kGenerateLog mode at creation point
      else
      {
        LOG_DEBUG("With cd_parent (%p) strict CD, creating CommLog with kGenerateLog mode\n", cd_parent);
        comm_log_ptr_ = new CommLog(this, kGenerateLog);
        SetCDLoggingMode(kRelaxedCDGen);
      }
    }
    else 
    {
      SetCDLoggingMode(kStrictCD);
    }

    // set child's child_seq_id_  to 0 
    LOG_DEBUG("Set child's child_seq_id_ to 0\n");
    child_seq_id_ = 0;

    // FIXME: 10142017
#if 0
    uint32_t tmp_seq_id = cd_parent->ptr_cd()->child_seq_id_;
    LOG_DEBUG("With cd_parent = %p, set child's seq_id_ to parent's child_seq_id_(%d)\n", cd_parent, tmp_seq_id);
    cd_id_.SetSequentialID(tmp_seq_id);
#else
    cd_id_.SetSequentialID(0);
#endif

#if CD_LIBC_LOG_ENABLED
    //GONG
    libc_log_ptr_ = new CommLog(this, kGenerateLog);
#endif

  }
  else 
  {
    // Only create CommLog object for relaxed CDs
    assert(comm_log_ptr_ == NULL);
    if (MASK_CDTYPE(cd_type_) == kRelaxed)
    {
      LOG_DEBUG("With cd_parent = NULL, creating CommLog with mode kGenerateLog\n");
      comm_log_ptr_ = new CommLog(this, kGenerateLog);
      SetCDLoggingMode(kRelaxedCDGen);
    }
    else {
      SetCDLoggingMode(kStrictCD);
    }

#if CD_LIBC_LOG_ENABLED
    //GONG:
    libc_log_ptr_ = new CommLog(this, kGenerateLog);
#endif

    LOG_DEBUG("Set child's child_seq_id_ to 0\n");
    child_seq_id_ = 0;
  }

#endif

}

static inline
bool IsFailed(void)
{ return (failed_phase != HEALTHY); }

static inline
bool IsGood(void)
{ return (failed_phase == HEALTHY); }

inline
void CD::Init()
{
  reexecuted_ = false;
  reported_error_ = false;
  num_reexecution_ = 0;
  begin_ = false;
//  child_seq_id_ = 0;
  ctxt_prv_mode_ = kExcludeStack; 
  //ctxt_prv_mode_ = kIncludeStack; 
  cd_exec_mode_  = kSuspension;
  option_save_context_ = 0;

#if comm_log
  comm_log_ptr_ = NULL;
#endif
#if CD_LIBC_LOG_ENABLED
  libc_log_ptr_ = NULL;
  cur_pos_mem_alloc_log = 0;
  replay_pos_mem_alloc_log = 0;
#endif

// Kyushick : I think we already initialize cd_id_ object inside cd object creator (outside of Init method)
// So we do not have to get it here. 
// I think this should be inside CDID object creator because there is no information to pass from CD object to CDID object
//  cd_id_.domain_id_ = Util::GetCurrentDomainID();
//  cd_id_.object_id_ = Util::GenerateCDObjectID();
//  cd_id_.sequential_id_ = 0;

}

/// Kyushick:
/// What if a process is just killed by some reason, and this method could not be invoked?
/// Would it be safe to delete meta data and do proper operation when CD finish?
/// Or how about defining virtual CDErrT DeleteCD() for ~CD() ??
/// And if a process gets some signal for abort, it just call this DeleteCD() and
/// inside this function, we explicitly call ~CD() 
CD::~CD()
{
  // Erase all the CDEntries
//  for(std::list<CDEntry>::iterator it = entry_directory_.begin();
//      it != entry_directory_.end(); ++it) {
//    it->Delete();
//  }

  // Request to add me as a child to my parent
//  cd_parent_->RemoveChild(this);
  //FIXME : This will be done at the CDHandle::Destroy()

#if comm_log
  //Delete comm_log_ptr_
  if (comm_log_ptr_ != NULL)
  {
    LOG_DEBUG("Delete comm_log_ptr_\n");
    delete comm_log_ptr_;
  }
  else 
  {
    LOG_DEBUG("NULL comm_log_ptr_ pointer, this CD should be strict CD...\n");
  }
#endif
}

CDHandle *CD::Create(CDHandle *parent, 
                     const char *name, 
                     const CDID& child_cd_id, 
                     CDType cd_type, 
                     uint64_t sys_bit_vector, 
                     CD::CDInternalErrT *cd_internal_err)
{
  //printf("[%s]cd size:%zu\n", __func__, sizeof(*this));
  /// Create CD object with new CDID
  CDHandle *new_cd_handle = NULL;

  //CD_DEBUG("CD::Create %s\n", name);
  CD_ASSERT(GetBegin());
  // NOTE
  // Order b/w flush and creation is important!!!!
  // Otherwise, written_len_ for child's entry_directory_ is set wrong!!
  if(prv_medium_ != kDRAM) {
    entry_directory_.AppendTable();
    entry_directory_.data_->Flush();
//    child_entry_dir.data_->PadAndInit(entry_directory_.data_->next_head());
  }
  *cd_internal_err = InternalCreate(parent, name, child_cd_id, cd_type, sys_bit_vector, &new_cd_handle);
  assert(new_cd_handle != NULL);

  //CD_DEBUG("CD::Create done\n");

  // FIXME: flush before child execution
//  packer::CDPacker &child_entry_dir = new_cd_handle->ptr_cd_->entry_directory_;
//  if(prv_medium_ != kDRAM) {
//    entry_directory_.AppendTable();
//    entry_directory_.data_->Flush();
////    child_entry_dir.data_->PadAndInit(entry_directory_.data_->next_head());
//  }
  this->AddChild(new_cd_handle);

//  new_cd_handle->UpdateParam(new_cd_handle->level(), new_cd_handle->phase());
  return new_cd_handle;

}

CDHandle *CD::CreateRootCD(const char *name, 
                           const CDID& root_cd_id, 
                           CDType cd_type, 
/*                           const string &basefilepath,*/
                           uint64_t sys_bit_vector, 
                           CD::CDInternalErrT *cd_internal_err)
{
  packer::SetHead(myTaskID == 0);

  /// Create CD object with new CDID
  CDHandle *new_cd_handle = NULL;
//  PrvMediumT new_prv_medium = static_cast<PrvMediumT>(MASK_MEDIUM(cd_type));

  *cd_internal_err = InternalCreate(NULL, name, root_cd_id, cd_type, sys_bit_vector, &new_cd_handle);

  // Only root define hash function.
  str_hash = str_hash_map.hash_function();

  // It sets filepath for preservation with base filename.
  // If CD_PRV_BASEPATH is set in env, 
  // the file path will be basefilepath/CDID_XXXXXX.
//  if(new_prv_medium != kDRAM) {
//    new_cd_handle->ptr_cd_->file_handle_.SetFilePath(new_prv_medium, 
//                                                     /*basefilepath,*/
//                                                     root_cd_id.GetStringID());
//  }
  assert(new_cd_handle != NULL);

  return new_cd_handle;
}


CDErrT CD::Destroy(bool collective, bool need_destroy)
{
  CD_DEBUG("CD::Destroy\n");
  CDErrT err=CDErrT::kOK;
  InternalDestroy(collective, need_destroy);

  FreeCommResource();

  return err;
}

bool CD::CheckToReuseCD(const std::string &cd_obj_key) 
{
  bool is_reuse = (access_store_.find(phaseTree.current_->GetPhasePath(cd_obj_key)) != access_store_.end());
  if (is_reuse == false)
  {
    if(task_size() > 1) {
      uint32_t rollback_point = CheckRollbackPoint(false);
      CheckMailBox();
      //uint32_t rollback_point = SyncCDs(this, false);
      if(rollback_point <= level()) {
        Escalate(CDPath::GetCurrentCD(), false);
      }
    }
  }
  return is_reuse;
}

CD::CDInternalErrT 
CD::InternalCreate(CDHandle *parent, 
                   const char *name, 
                   const CDID& new_cd_id, 
                   CDType cd_type, 
                   uint64_t sys_bit_vector, 
                   CDHandle* *new_cd_handle)
{
  CD_DEBUG("Internal Create... level #%u, Node ID : %s\n", new_cd_id.level(), new_cd_id.node_id().GetString().c_str());
  PrvMediumT new_prv_medium = static_cast<PrvMediumT>(MASK_MEDIUM(cd_type));
  if(parent != NULL) {
    new_prv_medium = (new_prv_medium == 0)? parent->ptr_cd()->prv_medium_ : new_prv_medium;
  } else { // root case
    new_prv_medium = (new_prv_medium == 0)? DEFAULT_MEDIUM : new_prv_medium;
  }
  string cd_obj_key;
  if(parent == NULL) {
    cd_obj_key = name;
  } else {
    cd_obj_key = phaseTree.current_->GetPhasePath(name);
  }

  auto cdh_it = access_store_.find(cd_obj_key);
  if(cdh_it != access_store_.end()) {
    *new_cd_handle = cdh_it->second;
    CD_DEBUG("Reused! [%s] New Node ID: %s\n", 
             cd_obj_key.c_str(), 
             cdh_it->second->node_id().GetString().c_str());

    (*new_cd_handle)->ptr_cd_->Initialize(parent, name, new_cd_id, cd_type, new_prv_medium, sys_bit_vector);
  }
  else {

    CD_DEBUG("Newly Create!\n");
    int task_count = new_cd_id.task_count();
    uint32_t num_mailbox_to_create = 0;
    CD *new_cd = NULL;
    if( !new_cd_id.IsHead() ) {
      CD_DEBUG("Mask medium : %d\n", MASK_MEDIUM(cd_type));
      new_cd = new CD(parent, name, new_cd_id, 
                      static_cast<CDType>(MASK_CDTYPE(cd_type)), 
                      new_prv_medium, 
                      sys_bit_vector);
#if _MPI_VER
      if(task_count > 1) {
        num_mailbox_to_create = 1;
      } 
#endif
    }
    else {

      // Create a CD object for head.
      CD_DEBUG("Mask medium : %d\n", MASK_MEDIUM(cd_type));
      new_cd = new HeadCD(parent, name, new_cd_id, 
                          static_cast<CDType>(MASK_CDTYPE(cd_type)), 
                          new_prv_medium, 
                          sys_bit_vector);
#if _MPI_VER
      if(task_count > 1) {
        num_mailbox_to_create = task_count;
        head_in_levels = true;
      }
#endif
    }

#if _MPI_VER
    if(num_mailbox_to_create != 0) { 
      CD_DEBUG("# mailbox to create : %u\n", num_mailbox_to_create);
      PMPI_Alloc_mem(num_mailbox_to_create*sizeof(CDFlagT), 
                    MPI_INFO_NULL, &(new_cd->event_flag_));
    
      // Initialization of event flags
      for(uint32_t i=0; i<num_mailbox_to_create; i++) {
        new_cd->event_flag_[i] = 0;
      }

      /*int MPI_Win_allocate(MPI_Aint size, int disp_unit,
       *                     MPI_Info info, 
       *                     MPI_Comm comm, 
       *                     void *baseptr, MPI_Win *win)
       *  int MPI_Win_create(void *base, MPI_Aint size, int disp_unit, 
       *                     MPI_Info info, 
       *                     MPI_Comm comm, MPI_Win *win)
       * int MPI_Win_allocate_shared(MPI_Aint size, int disp_unit,
       * MPI_Info info, MPI_Comm comm, void *baseptr, MPI_Win *win);
       * 
       */
#if USE_ALLOC_SHM      
      // FIXME : should it be MPI_COMM_WORLD?
      // Create memory region where RDMA is enabled
      MPI_Info window_info;
      MPI_Info_create(&window_info);
      MPI_Info_set(window_info, "alloc_shared_noncontig", "true");
      PMPI_Win_allocate_shared(num_mailbox_to_create*sizeof(CDFlagT), sizeof(CDFlagT),
                               window_info /*MPI_INFO_NULL*/, 
                               new_cd_id.color(),
                               &new_cd->event_flag_, 
                               &(new_cd->mailbox_));
#else
      MPI_Win_create(new_cd->event_flag_, num_mailbox_to_create*sizeof(CDFlagT), sizeof(CDFlagT),
                     MPI_INFO_NULL, new_cd_id.color(), &(new_cd->mailbox_));
#endif
      CD_DEBUG("mpi win create for %u pending window done, new Node ID : %s\n", 
                task_count, new_cd_id.node_id_.GetString().c_str());
  
      MPI_Info_free(&window_info);
      new_cd->is_window_reused_ = false;
    } 
    else {
      new_cd->is_window_reused_ = true;
    }
  
    if(task_count > 1) {
      CD_DEBUG("Create pending/reexec windows %u level:%u %p\n", 
                task_count, new_cd_id.level(), &(pendingWindow_));
      // FIXME : should it be MPI_COMM_WORLD?
      CD_DEBUG("After Create pending/reexec windows %u level:%u %p \n", 
                task_count, new_cd_id.level(), &(pendingWindow_));
    }
#endif

    *new_cd_handle = new CDHandle(new_cd);
  
    access_store_[cd_obj_key] = *new_cd_handle;
    delete_store_[new_cd->cd_id_.object_id_] = *new_cd_handle;
  
//    AttachChildCD(new_cd);
    CD_DEBUG("Done. New Node ID: %s -- %s\n", 
             new_cd_id.node_id().GetString().c_str(), 
             (*new_cd_handle)->node_id().GetString().c_str());
  }


  return CD::CDInternalErrT::kOK;
}


void AttachChildCD(HeadCD *new_cd)
{
  // STUB
  // This routine is not needed for MPI-version CD runtime  
}

inline 
CD::CDInternalErrT CD::InternalDestroy(bool collective, bool need_destroy)
{
  // sequential_id_ is reinitialized in CD::InternalInitialize()
  
  if(need_destroy == false) {
    CD_DEBUG("clean up CD meta data (%d windows) at %s (%s) level #%u\n", 
             task_size(), name_.c_str(), label_.c_str(), level());
    // delete preservation files
//    if(prv_medium_ == kSSD || prv_medium_ == kHDD) {
//      file_handle_.Close();
//    }
//#if CD_MPI_ENABLED
//    else if(prv_medium_ == kPFS) {
//      pfs_handle_->Close();
//    }
//#endif

#if comm_log
    if (GetParentHandle()!=NULL)
    {
      GetParentHandle()->ptr_cd_->child_seq_id_ = cd_id_.sequential_id();
    }
#endif

//    entry_directory_.Clear(true);

#if _MPI_VER
    FinalizeMailBox();
#endif 
  }
  else {
    entry_directory_.Clear(false);
    if(recoverObj_ != NULL) { delete recoverObj_; recoverObj_ = NULL; }
#if _MPI_VER
    if(task_size() > 1 && (is_window_reused_==false)) {  
//      PMPI_Win_free(&pendingWindow_);
//      PMPI_Win_free(&rollbackWindow_);
      PMPI_Group_free(&cd_id_.node_id_.task_group_);
      //PMPI_Comm_free(&cd_id_.node_id_.color_);
      PMPI_Win_free(&mailbox_);
  #if USE_ALLOC_SHM
  // Do nothing? 
  #else
      PMPI_Free_mem(event_flag_);
  #endif
      CD_DEBUG("[%s Window] CD's Windows are destroyed.\n", __func__);
    }
    else
      CD_DEBUG("[%s Window] Task size == 1 or Window is reused.\n", __func__);
  
  

//    if( GetPlaceToPreserve() == kPFS ) {
//      delete pfs_handle_;
//    }
#endif

    delete this;
//    if(myTaskID == 0)
//      printf("init overhead : %lf\n", init_timer);
  } 
  
  return CDInternalErrT::kOK;
}

static inline 
void SetFailure(uint32_t phase, const char *label);

static inline 
void SetHealthy(void)
{
  failed_phase = HEALTHY;
  failed_seqID = HEALTHY;
  //need_escalation = false;
  //*rollback_point_ = INVALID_ROLLBACK_POINT;        
  // At this point, take the timer (update RuntimeInfo::clk_)
  // to measure the rest of execution time.
  CD_CLOCK_T now = CD_CLOCK();
  phaseTree.current_->FinishRecovery(now);
  reex_elapsed_time += now - global_reex_clk;
  global_reex_clk = now;
}

#if 0
inline
uint32_t CD::BeginPhase(uint32_t level, const string &label) 
{
  uint32_t phase = -1;
  if(tuned::tuning_enabled == false) {
    if(phaseTree.current_ != NULL) {
      phase = phaseTree.current_->GetPhaseNode(level, label, prv_medium_, sys_detect_bit_vector_);
//      auto tp = tuned::phaseNodeCache.find(phase);
      // overwrite from source code
//      if (tp == tuned::phaseNodeCache.end()) {
//        if (myTaskID == 0) printf("\n\t%s>>config.yaml is missing\n", std::string(2 << this->level(), ' ').c_str());
//        assert(tuned::phaseNodeCache.size() == 0);
//        phaseTree.current_->medium_ = prv_medium_;
//        phaseTree.current_->errortype_ = sys_detect_bit_vector_;
//      } else if (tp->second->label_ != label) {
//        if (myTaskID == 0) printf("\n\t%s>>different config.yaml (%s != %s)\n", std::string(2 << this->level(), ' ').c_str(), tp->second->label_.c_str(), label.c_str());
//        phaseTree.current_->medium_    = prv_medium_;
//        phaseTree.current_->errortype_ = sys_detect_bit_vector_;
//      } else {
//        if (myTaskID == 0) printf("\n\t%s>>valid config.yaml\n", std::string(2 << this->level(), ' ').c_str());
//      }
    } 
    // when root_ is activated for the first time
    else { 
      if(phaseTree.root_ == NULL) {
        phase = phaseTree.Init(level, label);
        phaseTree.root_->medium_    = kPFS;
        phaseTree.root_->errortype_ = -1ULL;
      } else {
        phase = phaseTree.ReInit();
      }
      PhaseNode *current = phaseTree.current_;
//      auto tp = tuned::phaseNodeCache.find(current->phase_);
//      // if there is correct phase and label in phaseNodeCache,
//      // use phase info there, instead of one from source code.
//      if (tp != tuned::phaseNodeCache.end() && tp->second->label_ == label) {
//        if (myTaskID == 0 && tp != tuned::phaseNodeCache.end()) printf("\n\t%s>>> [root] config.yaml is set by tuned::phaseNodeCache (%s != %s)\n", std::string(2 << this->level(), ' ').c_str(), tp->second->label_.c_str(), label.c_str());
        PhaseNode *tn = tuned::phaseNodeCache[current->phase_];
//        assert( tn == tp->second );
//        current->medium_   = tn->medium_;
//        current->errortype_= tn->errortype_;
//      } 
//      // otherwise, set it from source code
//      else {
//        if (myTaskID == 0) printf("\n\t%s>>[root] config.yaml is missing\n", std::string(2 << this->level(), ' ').c_str());
//        current->medium_    = prv_medium_;
//        current->errortype_ = sys_detect_bit_vector_;
//      }
      if(myTaskID == 0) printf("ROOT: (%x, %lx) <- (%x, %lx)\n", 
          current->medium_,
          current->errortype_, 
          tn->medium_, 
          tn->errortype_);
    }
//    phaseTree.current_->profile_.RecordBegin(failed_phase != HEALTHY, just_reexecuted, );
  }
  return phase; 
}
#endif

inline
uint32_t CD::BeginPhase(uint32_t level, const string &label) 
{
  uint32_t phase = -1;
  if (tuned::tuning_enabled == false) {
    if (phaseTree.current_ != NULL) {
      phase = phaseTree.current_->GetPhaseNode(level, label, prv_medium_, sys_detect_bit_vector_);
    } 
    // when root_ is activated for the first time
    else { 
      if (phaseTree.root_ == NULL) {
        phase = phaseTree.Init(level, label);
        phaseTree.root_->medium_    = kPFS;
        phaseTree.root_->errortype_ = -1ULL;
      } else {
        phase = phaseTree.ReInit();
      }
      PhaseNode *current = phaseTree.current_;
    }
    /* phaseTree.current_->profile_.RecordBegin(failed_phase != HEALTHY, just_reexecuted, ); */
  }

//  if(cd::myTaskID == 0) printf("\n%s[%16s] (%ld~%ld, acc:%ld)\n\n", std::string(2<<level, '\t').c_str(), __func__
//          , phaseNodeCache[phase]->seq_begin_
//          , phaseNodeCache[phase]->seq_end_
//          , phaseNodeCache[phase]->seq_acc_rb_);
  return phase; 
}
static inline 
void CompletePhase(uint32_t phase, bool is_reexec=false)
{
//  if(cd::myTaskID == 0) printf("\n%s[%16s] (%ld~%ld, acc:%ld)\n\n", std::string(2<<phaseNodeCache[phase]->level_, '\t').c_str(), __func__
//          , phaseNodeCache[phase]->seq_begin_
//          , phaseNodeCache[phase]->seq_end_
//          , phaseNodeCache[phase]->seq_acc_rb_);
  CD_ASSERT(phaseTree.current_);
  CD_DEBUG("%s (%ld, %ld, %ld) -> "
          , phaseNodeCache[phase]->label_.c_str()
          , phaseNodeCache[phase]->seq_begin_
          , phaseNodeCache[phase]->seq_end_
          , phaseNodeCache[phase]->seq_acc_rb_);

  phaseTree.current_->IncSeqID(IsGood() && is_reexec == false);
//  phaseTree.current_->seq_end_++; // reinit at failure
//  if(failed_phase == HEALTHY) {
//    phaseTree.current_->seq_acc_++; // no reinit
//  } else {
//    phaseTree.current_->seq_acc_rb_++; // no reinit
//  }
  if(tuned::tuning_enabled == false) {
    if ( IsFailed() ) { // reexecution
      CD_ASSERT(failed_phase >= 0);
      // Set failed state in phaseTree
      phaseTree.current_->state_ = kReexecution;
      // Just failed case is different from already failed case
//      phaseTree.current_->profile_.RecordRollback(ph);

    }
    if(phaseTree.current_ != NULL) {
      phaseTree.current_->last_completed_phase = phaseTree.current_->phase_;
      phaseTree.current_ = phaseTree.current_->parent_;
    } else {
      ERROR_MESSAGE("NULL phase is completed.\n");
    }
  }
  CD_DEBUG("(%ld, %ld, %ld)\n"
          , phaseNodeCache[phase]->seq_begin_
          , phaseNodeCache[phase]->seq_end_
          , phaseNodeCache[phase]->seq_acc_rb_);
}

/* CD::Begin()
 */ 
static uint32_t prv_phase_chk = -1U;
// Here we don't need to follow the exact CD API this is more like internal thing. 
// CDHandle will follow the standard interface. 
CDErrT CD::Begin(const char *label, bool collective)
{
//  CD_ASSERT_STR(begin_ == false || cd_exec_mode_ == kReexecution, "%s %s at %u-%u\n", 
//      __func__, label, level, cd_id_.sequential_id_);
  SetBegin();
  //printf("[%s] not here? \n", __func__);
  label_ = (strcmp(label, NO_LABEL) == 0)? name_ : label; 
  const uint32_t level = cd_id_.cd_name_.level_;
  if (myTaskID == 0) printf("\n[Before] lv:%u, %s, %s, bitvec:%lx, media:%x\n", 
      level, name_.c_str(), label_.c_str(), sys_detect_bit_vector_, prv_medium_);
  if(tuned::tuning_enabled == false) {
    cd_id_.cd_name_.phase_ = BeginPhase(level, label_);
    sys_detect_bit_vector_ = phaseTree.current_->errortype_;
    phaseTree.current_->UpdateTaskInfo(rank_in_level(), cd_id_.sibling_count(),
                                       task_in_color(), cd_id_.task_count());
//    if(myTaskID == 0) printf("lv:%u, %s, %s, bitvec:%lx\n", level, name_.c_str(), label_.c_str(), sys_detect_bit_vector_);
  } else { // phaseTree.current_ was updated at tuned::CDHandle before this
    cd_id_.cd_name_.phase_ = phaseTree.current_->phase_;
    sys_detect_bit_vector_ = phaseTree.current_->errortype_;
    CD_DEBUG("lv:%u, %s, %s, bitvec:%lx\n", level, name_.c_str(), label_.c_str(), sys_detect_bit_vector_);
    CD_ASSERT_STR(new_phase == phaseTree.current_->phase_,
                  "phase : %u != %u\n", new_phase,  phaseTree.current_->phase_);
  }
//  if (myTaskID == 0 && (prv_medium_ == kLocalDisk || prv_medium_ == kGlobalDisk)) {
//    printf("begin 1 medium:%x %x\n", prv_medium_, entry_directory_.data_->ftype());
//  }
  // Overwrite medium
  prv_medium_ = phaseTree.current_->medium_;
  if (myTaskID == 0) printf("[After] lv:%u, %s, %s, bitvec:%lx, media:%x\n", 
      level, name_.c_str(), label_.c_str(), sys_detect_bit_vector_, prv_medium_);
  const int64_t current_phase = cd_id_.cd_name_.phase_;
  profMap[current_phase] = &cd::phaseTree.current_->profile_; //getchar();


  // it is the first begin after Create()
  // or it is the begin after leaf
  // or it is the begin after the previous heterogeneous CD.
  // (sequential ID is initialized at Create())
  //if(cd_id_.sequential_id() == 0) 
  if(prev_phase != current_phase) 
  {
    bool returned_from_child_lv = (prev_phase != HEALTHY) ? (phaseNodeCache[prev_phase]->level_ > this->level()) : false;
    if (returned_from_child_lv == false) { 
      if ( IsGood() ) {
        //if (myTaskID == 0 && prev_phase != HEALTHY) printf("(prev_phase = %ld) ? %u > %u\n", prev_phase, phaseNodeCache[prev_phase]->level_,this->level());
        phaseTree.current_->MarkSeqID(cd_id_.sequential_id_); // set seq_begin_ = seq_end_
  //      phaseTree.current_->seq_end_ = cd_id_.sequential_id_;
      } else {
        phaseTree.current_->MarkSeqID(phaseTree.current_->seq_begin_); // set seq_begin_ = seq_end_
        if(myTaskID == 0) {
  //        printf("[Begin In Rollback] ");
  //        phaseTree.current_->PrintDetails();
        }
      }
    }
    prev_phase = current_phase;
  }


//  if(prv_phase_chk == -1U || prv_phase_chk != current_phase) 
  {
    PRINT_BOTH("%s lv:%u, %s, %s, (%ld, %u), bitvec:%lx, (curr:%ld,fphase:%ld), %ld~%ld\n", 
      IsFailed()? "REEX" : "EXEC",
      level, name_.c_str(), label_.c_str(), phaseTree.current_->seq_acc_rb_, cd_id_.sequential_id_, sys_detect_bit_vector_, 
      current_phase, failed_phase, phaseTree.current_->seq_begin_, phaseTree.current_->seq_end_);
  }


 // cd_name_.phase_ = GetPhase(level(), label_);

//  cd_name_.phase_ = phase;
//  phaseTree.target_->level_ = level_;
//  phaseTree.target_->phase_ = phase;
//  phaseTree.target_->prev_path_ + string("_") + string(label);
//  auto it = phaseMap[level].find(label);
//  auto it = phasePath.find(label);
//  if(it != phasePath.end()) {

//  printf("## Before chage %d %p ## [%s %u] %s %s\n", 
//      begin_, this, cd_id_.GetStringID().c_str(), myTaskID, name_.c_str(), label);
//  CD_ASSERT_STR(begin_ == false || cd_exec_mode_ == kReexecution, "%s %s at %u-%ld-%u\n", 
//      __func__, label, level, current_phase, cd_id_.sequential_id_);
//  begin_ = true;

  CD_DEBUG("[%s] %s %s\n", cd_id_.GetStringID().c_str(), name_.c_str(), label);
  PrintDebug();
//  cd_id_.cd_name_.UpdatePhase(label_);

#if comm_log
  //SZ: if in reexecution, need to unpack logs to childs
  if (GetParentHandle()!=NULL)
  {
#if _PGAS_VER
    // increment parent's SC value and record in current CD
    sync_counter_ = GetParentHandle()->ptr_cd()->GetSyncCounter();
    sync_counter_++;
    LOG_DEBUG("Increment SC at CD::Begin, with SC (%d) after.\n", sync_counter_);
#endif
    // Only need to if for both parent and child are relaxed CDs, 
    // if child is relaxed but parent is strict, then create the CommLog object with kGenerateLog mode
    // if child is strict, then do not need to do anything for comm_log_ptr_...
    if(MASK_CDTYPE(GetParentHandle()->ptr_cd_->cd_type_) == kRelaxed && MASK_CDTYPE(cd_type_) == kRelaxed)
    {
      if (GetParentHandle()->ptr_cd_->comm_log_ptr_->GetCommLogMode() == kReplayLog)
      {
        LOG_DEBUG("With comm log mode = kReplayLog, unpack logs to children\n");
        if (IsParentLocal())
        {
          comm_log_ptr_->SetCommLogMode(kReplayLog);
          CommLogErrT ret;
          ret = GetParentHandle()->ptr_cd_->comm_log_ptr_->UnpackLogsToChildCD(this);
          if (ret == kCommLogChildLogNotFound)
          {
            // need to reallocate table and data array...
            comm_log_ptr_->Realloc();
          }
        }
        else
        {
          comm_log_ptr_->SetCommLogMode(kReplayLog);
          //SZ: FIXME: need to figure out a way to unpack logs if child is not in the same address space with parent
          LOG_DEBUG("Should not be here to unpack logs!!\n");
        }
      }
    }

#if CD_LIBC_LOG_ENABLED
    //GONG: as long as parent CD is in replay(check with ), child CD needs to unpack logs
    if(GetParentHandle()->ptr_cd_->libc_log_ptr_->GetCommLogMode() == kReplayLog){
      LOG_DEBUG("unpack libc logs to children - replay mode\n");
      //the same issue as above: address space 
      if (IsParentLocal())
      {
          libc_log_ptr_->SetCommLogMode(kReplayLog);
          CommLogErrT ret;
          ret = GetParentHandle()->ptr_cd_->libc_log_ptr_->UnpackLogsToChildCD_libc(this);
          if (ret == kCommLogChildLogNotFound)
          {
            // need to reallocate table and data array...
            libc_log_ptr_->Realloc();
          }
      }
      else
      {
        LOG_DEBUG("Should not be here to unpack logs!! - libc\n");
      }
      cur_pos_mem_alloc_log = PullMemLogs();
      replay_pos_mem_alloc_log = 0;
    }

#endif
  }
#if _PGAS_VER
  else {
    // as CD_Begin will start a new epoch, so sync_counter_ will be 1 instead of 0
    sync_counter_ = 1;
    LOG_DEBUG("Increment SC at root's CD::Begin, with SC (%d) after.\n", sync_counter_);
  }
#endif


#endif // comm_log ends

  CD_DEBUG("Sync \n");
  if(cd_exec_mode_ == kReexecution || collective) {
    MYDBG("[Begin] Reexecution\n");
    SetRollbackPoint(INVALID_ROLLBACK_POINT, false);
//FIXME 0324 Kyushick    
//    SyncCDs(this);
  }

  if(cd_exec_mode_ == kSuspension) {
    cd_exec_mode_ = kExecution;
//FIXME 0324 Kyushick    
//    SyncCDs(this);
  }
#if CD_LIBC_LOGGING
  if ( IsGood() ) {
    assert(cd_exec_mode_ == kExecution);
    libc_log_id_ = logger::GetLogger()->Set(libc_log_begin_);
    libc_log_end_ = libc_log_begin_;
    if(myTaskID == 0) {
      MYDBG("log ft:%lu, log id:%lu, log begin:%lu\n", logger::GetLogger()->GetNextID(), libc_log_id_, libc_log_begin_);
//      logger::GetLogger()->Print();
    }
  }
  else {
  //else if(current_phase <= failed_phase) {
  //} else if(cd_exec_mode_ == kReexecution) {
    if(myTaskID == 0) {
      MYDBG("before log ft:%lu, log id:%lu, log begin:%lu\n", logger::GetLogger()->GetNextID(), libc_log_id_, libc_log_begin_);
    }
    logger::GetLogger()->Reset(libc_log_id_, libc_log_begin_);
    if(myTaskID == 0) {
      MYDBG("[Mode=%d] after log ft:%lu, log id:%lu, log begin:%lu\n", phaseTree.current_->state_, 
          logger::GetLogger()->GetNextID(), libc_log_id_, libc_log_begin_);
//      logger::GetLogger()->Print();
    }
  } 
//  else {
//    ERROR_MESSAGE("CD Begin with mode (%d) and state (curr:%ld > %ld failed).\n", cd_exec_mode_, current_phase, failed_phase);
//  }
#endif

//  if (myTaskID == 0 && (prv_medium_ == kLocalDisk || prv_medium_ == kGlobalDisk)) {
//    printf("begin 2 medium:%x %x -- check\n", prv_medium_, entry_directory_.data_->ftype());
//  }
  // If it is not from complete (reexecution)
  //if(failed_phase == HEALTHY) {
  if(cd_exec_mode_ != kReexecution) {
    if(prv_medium_ == kLocalMemory) {
      entry_directory_.data_->SetFileType(kVolatile);
    } 
    else {
      if(prv_medium_ == kLocalDisk) {
        if(entry_directory_.data_->ftype() != kPosixFile) {
          entry_directory_.data_->InitFile(kPosixFile);
        }
      }
      entry_directory_.data_->SetMode(kBoundedMode);
    }
    entry_directory_.data_->ReInit();
  }

  if ((prv_medium_ == kLocalDisk || prv_medium_ == kGlobalDisk)) {
    if(entry_directory_.data_->ftype() == kVolatile) {
      ERROR_MESSAGE("file medium setting is wrong:%x\n", prv_medium_);
    }
  }
  // NOTE: This point reset rollback_point_
#if 1 // debug
  uint32_t new_rollback_point = SyncCDs(this, true);
  SetRollbackPoint(new_rollback_point, false);
#else
  uint32_t new_rollback_point = CheckRollbackPoint(false);
#endif

//  const uint32_t level = level();
//  in Begin
  if(new_rollback_point < level) { 
    bool need_sync_next_cdh = CDPath::GetParentCD(level)->task_size() > task_size();
    phaseTree.current_->profile_.RecordRollback(true, kBegin); // measure timer and calculate sync time.
    SetFailure(current_phase, label_.c_str());
    GetCDToRecover( CDPath::GetCurrentCD(), need_sync_next_cdh )->ptr_cd()->Recover(level);
  } 
  else {
    need_escalation = false;
    *rollback_point_ = INVALID_ROLLBACK_POINT;        
  }


  if( cd_exec_mode_ != kReexecution ) { // normal execution
    num_reexecution_ = 0;
    cd_exec_mode_ = kExecution;
//    char tmp[16];
//    sprintf(tmp, "EXEC %s %u", label, level);
//    if(myTaskID == 0)
//      entry_directory_.table_->PrintEntry(tmp);
//    CDHandle *parent = GetParentCD();
//    if(parent != NULL) {
//
//      sprintf(tmp, "P EXEC %s %u", parent->GetLabel(), parent->ptr_cd()->level());
//      if(myTaskID == 0) 
//        parent->ptr_cd()->entry_directory_.table_->PrintEntry(tmp);
//
//    }
  }
  else {
//    char tmp[16];
//    sprintf(tmp, "REEX %s %u", label, level);
//    if(myTaskID == 0)
//      entry_directory_.table_->PrintEntry(tmp);
//    CDHandle *parent = GetParentCD();
//    if(parent != NULL) {
//
//      sprintf(tmp, "P REEX %s %u", parent->GetLabel(), parent->ptr_cd()->level());
//      if(myTaskID == 0) 
//        parent->ptr_cd()->entry_directory_.table_->PrintEntry(tmp);
//
//    }
    //printf("don't need reexec next time. Now it is in reexec mode\n");
//    need_reexec = false;
//    *rollback_point_ = INVALID_ROLLBACK_POINT;
  }
//  else {
//    cout << "Begin again! " << endl; //getchar();
//    num_reexecution_++ ;
//  }

  just_reexecuted = false;


//  if(MASK_CDTYPE(cd_type_) == kRelaxed) {printf("cdtype is relaxed\n");assert(0);}
//  if(GetCDLoggingMode() == kRelaxedCDGen) {printf("CDLoggingmode is relaxed cd gen\n");assert(0);}
//  if(GetCDLoggingMode() == kRelaxedCDRead) {printf("CDLoggingmode is relaxed cd read\n");assert(0);}

  return CDErrT::kOK;
}

// static
// https://software.intel.com/en-us/mpi-developer-reference-linux-asynchronous-progress-control
// https://software.intel.com/en-us/mpi-developer-guide-windows-prerequisite-steps
// https://software.intel.com/en-us/forums/intel-clusters-and-hpc-technology/topic/557206
uint32_t CD::SyncCDs(CD *cd_lv_to_sync, bool for_recovery)
{
#if CD_MPI_ENABLED

  #if CD_PROFILER_ENABLED
  double sync_time = 0.0;
  #endif

#if BUGFIX_0327

  uint32_t new_rollback_point = INVALID_ROLLBACK_POINT;
  const int64_t curr_phase = cd_lv_to_sync->phase();
  const int64_t now_seqID = phaseNodeCache[curr_phase]->seq_end_;
  const int64_t now_begID = phaseNodeCache[curr_phase]->seq_begin_;

  if(cd_lv_to_sync->task_size() > 1) {
    cd_lv_to_sync->CheckMailBox();
    CD_DEBUG("[phase:%ld, seq:%ld->%ld, (acc:%u, accrb:%u, seqid:%u)] fence in at %s level %u\n", 
        curr_phase, now_begID, now_seqID,
        phaseNodeCache[curr_phase]->seq_acc_,
        phaseNodeCache[curr_phase]->seq_acc_rb_,
        cd_lv_to_sync->cd_id_.sequential_id_,
        cd_lv_to_sync->name_.c_str(), cd_lv_to_sync->level()
        );
  #if CD_PROFILER_ENABLED 
    CD_CLOCK_T begin_here = CD_CLOCK();
  #endif

    //MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    //MPI_Win_flush_local_all(cd_lv_to_sync->mailbox_);
    
  #if CD_PROFILER_ENABLED
    sync_time += CD_CLOCK() - begin_here;
  #endif

//    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    cd_lv_to_sync->CheckMailBox();
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    cd_lv_to_sync->CheckMailBox();
//    cd_lv_to_sync->CheckMailBox();
    //new_rollback_point = cd_lv_to_sync->CheckRollbackPoint(true);
    //MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    new_rollback_point = cd_lv_to_sync->CheckRollbackPoint(false);

    CD_DEBUG("rollback point from head:%u (headID:%d at lv#%u)\n", 
              new_rollback_point, cd_lv_to_sync->head(), cd_lv_to_sync->level());

    uint32_t local_rollback_point = new_rollback_point;
    PMPI_Allreduce(&local_rollback_point, &new_rollback_point, 1, 
                   MPI_UNSIGNED, MPI_MIN, cd_lv_to_sync->color());
//    cd_lv_to_sync->CheckMailBox();

    CD_DEBUG("rollback point after broadcast:%u\n", new_rollback_point);

  } else {
    CD_DEBUG("[%s] No fence\n", __func__);
  }
#else // BUGFIX_0327
  if(cd_lv_to_sync->task_size() > 1) {

    cd_lv_to_sync->CheckMailBox();

    CD_DEBUG("[%s] fence 1 in at %s level %u\n", __func__, cd_lv_to_sync->name_.c_str(), cd_lv_to_sync->level());
  #if CD_PROFILER_ENABLED
    CD_CLOCK_T begin_here = CD_CLOCK();
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    sync_time += CD_CLOCK() - begin_here;

    cd_lv_to_sync->CheckMailBox();

    CD_DEBUG("[%s] fence 2 in at %s level %u\n", __func__, cd_lv_to_sync->name_.c_str(), cd_lv_to_sync->level());
    begin_here = CD_CLOCK();
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    sync_time += CD_CLOCK() - begin_here;
  #else
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
    cd_lv_to_sync->CheckMailBox();

    CD_DEBUG("[%s] fence 2 in at %s level %u\n", __func__, cd_lv_to_sync->name_.c_str(), cd_lv_to_sync->level());
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);
  #endif    
    if(cd_lv_to_sync->IsHead() == false) {
      cd_lv_to_sync->CheckMailBox();
    }
    CD_DEBUG("[%s] fence 3 in at %s level %u\n", __func__, cd_lv_to_sync->name_.c_str(), cd_lv_to_sync->level());
    MPI_Win_fence(0, cd_lv_to_sync->mailbox_);

    CD_DEBUG("[%s] fence out \n\n", __func__);
  } else {
    CD_DEBUG("[%s] No fence\n", __func__);
  }
#endif

#if CD_PROFILER_ENABLED
  if(for_recovery == false) { 
    normal_sync_time += sync_time;
    normal_sync_smpl += sync_time;
  } else {
    reexec_sync_time += sync_time; 
    reexec_sync_smpl += sync_time; 
  }
#endif
  return new_rollback_point; 

#else // CD_MPI_ENABLED ends
  return *CD::rollback_point_;
#endif
}

/// [IMPORTNANT NOTE]
/// phase ID preserves the execution order,
/// therefore, failed_phase will be never updated 
/// before reaching original failed phase
/// in reexecution with this assumption.
/// When it reaches the original failed phase in reexecution,
/// failed_phase must be reset to HEALTHY (-1) state 
static inline 
void SetFailure(uint32_t phase, const char *label) 
{
  if ( IsGood() ) {
    
    const int64_t curr_seqID = phaseTree.current_->seq_end_;
    const int64_t curr_begID = phaseTree.current_->seq_begin_;
    CD_DEBUG(">>> Failure Detected (%s), fphase:%ld==%u(current), fseqID:%ld==%ld (current) (beg:%ld) <<<\n", 
        label, failed_phase, phase, failed_seqID, curr_seqID, curr_begID);
    if(myTaskID == 0) printf(">>> Failure Detected (%s), fphase:%ld==%u, seqID:%ld==%ld (beg:%ld) <<<\n", 
        label, failed_phase, phase, failed_seqID, curr_seqID, curr_begID);
    failed_phase = phase;
    failed_seqID = phaseTree.current_->seq_end_;

    global_reex_clk = CD_CLOCK();
  }
}

void CD::Escalate(CDHandle *leaf, bool need_sync_to_reexec) {
#if CD_PROFILER_ENABLED
    prof_sync_clk = CD_CLOCK();
#endif
  const uint32_t level = cd_id_.cd_name_.level_;
  SetFailure(this->phase(), label_.c_str());
  CD *ptr_cd = GetCDToRecover(leaf, need_sync_to_reexec)->ptr_cd();
  CD_DEBUG("\n%s %s %u->%u\n\n", ptr_cd->label_.c_str(), ptr_cd->cd_id_.GetString().c_str(), level, ptr_cd->level());
  ptr_cd->Recover(level); 
}

// static
// [NOTE] CompleteLogs(true) must be called after SyncCDs()
// because some remote tasks may call MPI routines that may be associated with 
// the MPI request handle that the current task tries to cancel/probe
// while completing the CD. If we cancel/probe after SyncCDs(),
// it guarantees that the remote tasks successfully complete the MPI 
// routine and reach this SyncCDs() boundary to rollback together.
CDHandle *CD::GetCDToRecover(CDHandle *target, bool collective)
{
  PRINT_BOTH("######### %s lv %u seq:%ld->%ld (rollback_point: %d, failed_phase: %ld) (%s)\n", __func__, 
              target->level(), cd::phaseTree.current_->seq_end_, cd::failed_seqID,
              *rollback_point_, cd::failed_phase, target->label().c_str());
  
#if 0//CD_PROFILER_ENABLED
  static bool check_sync_clk = false;
  if(check_sync_clk == false) {
    prof_sync_clk = CD_CLOCK();
    end_clk = CD_CLOCK();
    elapsed_time += end_clk - begin_clk; 
    Profiler::profMap[level()][GetLabel()].compl_elapsed_time_ += end_clk - begin_clk;
    check_sync_clk = true;
  }
#endif
//#if CD_MPI_ENABLED
//      if(MASK_CDTYPE(target->ptr_cd_->cd_type_)==kRelaxed) {
//        target->ptr_cd_->ProbeIncompleteLogs();
//      }
//      else if(MASK_CDTYPE(target->ptr_cd_->cd_type_)==kStrict) {
//        CD_DEBUG("[%s]\n", __func__);
//        target->ptr_cd_->InvalidateIncompleteLogs();
//      }
//      else {
//        ERROR_MESSAGE("[%s] Wrong control path. CD type is %d\n", __func__, target->ptr_cd_->cd_type_);
//      }
//#endif
#if _MPI_VER
  // Before longjmp, it should decrement the event counts handled so far.
  target->ptr_cd_->DecPendingCounter();
#endif
  uint32_t level = target->level();
  uint32_t rollback_lv = target->ptr_cd()->CheckRollbackPoint(false);
  phaseTree.current_->ResetSeqID(rollback_lv);
  CD_DEBUG("[%s] level : %u (current) == %u (rollback_point)\n", 
            target->ptr_cd()->cd_id_.GetStringID().c_str(), level, rollback_lv);

  // Should be done before sync
  target->ptr_cd_->CompleteLogs(true);

  if(level == rollback_lv) {
    // for tasks that detect error at completion point or collective create point.
    // It already called SyncCDs() at that point,
    // so should not call SyncCDs again in this routine.
    // If the tasks needs to escalate several levels,
    // then it needs to call SyncCDs to be coordinated with
    // the other tasks in the other CDs of current level,
    // but in the same CDs at the level to escalate to.
#if BUGFIX_0327
      
    uint32_t new_rollback_point = (collective)? SyncCDs(target->ptr_cd(), true) : 
                                  target->ptr_cd()->CheckRollbackPoint(false/*true*/); // read from head
    if(new_rollback_point > rollback_lv) {
      CD_DEBUG("head did not notice error yet.....\n");
      new_rollback_point = rollback_lv;
      target->ptr_cd()->SetRollbackPoint(new_rollback_point, true);
    } else {
      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
    }
    // FIXME
//    if(collective) {
//      uint32_t new_rollback_point = SyncCDs(target->ptr_cd(), true);
//      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
//    } else {
//      uint32_t new_rollback_point = target->ptr_cd()->CheckRollbackPoint(true); // read from head
//      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
//    }
#else    
    if(collective) {
      SyncCDs(target->ptr_cd(), true);
    }

    if(target->task_size() > 1) {
      uint32_t new_rollback_point = target->ptr_cd()->CheckRollbackPoint(true); // read from head
      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
    }
#endif
    CD_DEBUG("new rb:%u, rollback_lv:%u, curr:%u\n", new_rollback_point, rollback_lv, level);
    // It is possible for other task set to rollback_point lower than original.
    // It handles that case.
    // FIXME (11.02.2016) : should check with new_rollback_point
//    if(level != rollback_lv)  
    if(level != new_rollback_point) {  // Detected escalation from sibling CDs.
#if CD_PROFILER_ENABLED
//      if(myTaskID == 0) printf("[%s] CD level #%u (%s)\n", __func__, level, target->ptr_cd_->label_.c_str()); 
      target->profiler_->FinishProfile();
#endif

      // In GetCDToRecover
      phaseTree.current_->profile_.RecordRollback(false); // measure timer and calculate sync time.
      //target->ptr_cd_->CompleteLogs(true);
      target->ptr_cd_->DeleteEntryDirectory();
      CompletePhase(target->phase(), true);
      target->ptr_cd_->UnsetBegin();
      target->Destroy();
      CDHandle *next_cdh = CDPath::GetCDLevel(--level);
//      bool need_sync_next_cdh = CDPath::GetParentCD(next_cdh->level())->task_size() > next_cdh->task_size();
      bool need_sync_next_cdh = next_cdh->task_size() > target->task_size();
      CD_DEBUG("next level#%u (curlv:%u!= %u(next_cdh) need sync? %u\n", 
         next_cdh->level(), level, new_rollback_point, need_sync_next_cdh);
      return GetCDToRecover(next_cdh, need_sync_next_cdh);
    }
    else { // Otherwise, now it is the time to reexecute!
      target->ptr_cd_->reported_error_ = false;
      //cdp->reported_error_ = false;
#if CD_PROFILER_ENABLED
      recovery_sync_time += CD_CLOCK() - prof_sync_clk;
      recovery_sync_smpl += CD_CLOCK() - prof_sync_clk;
//      check_sync_clk = false;
#endif
      //target->ptr_cd_->CompleteLogs(true);
      CompletePhase(target->phase(), true);
      target->ptr_cd_->UnsetBegin();
      return target;
    }
  } // level == rollback_lv case ends
  else if(level > rollback_lv && rollback_lv != INVALID_ROLLBACK_POINT) {

    // This synchronization corresponds to that in Complete or Create of other tasks in the different CDs.
    // The tasks in the same CD should reach here together. (No tasks execute further Complete/Create)
#if BUGFIX_0327
    uint32_t new_rollback_point = (collective)? SyncCDs(target->ptr_cd(), true) : 
                                  target->ptr_cd()->CheckRollbackPoint(false/*true*/); // read from head
    target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
//    if(collective) {
//      uint32_t new_rollback_point = SyncCDs(target->ptr_cd(), true);
//      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
//    } else {
//      uint32_t new_rollback_point = target->ptr_cd()->CheckRollbackPoint(true); // read from head
//      target->ptr_cd()->SetRollbackPoint(new_rollback_point, false);
//    }
#else
    if(collective) {
      SyncCDs(target->ptr_cd(), true);
    }
#endif

#if CD_PROFILER_ENABLED
//    if(myTaskID == 0) printf("[%s] CD level #%u (%s)\n", __func__, level, target->ptr_cd_->label_.c_str()); 
    target->profiler_->FinishProfile();
#endif

    phaseTree.current_->profile_.RecordRollback(false); // measure timer and calculate sync time.
//    CompletePhase(target->phase(), true);
//    target->ptr_cd_->UnsetBegin();
    //target->ptr_cd_->CompleteLogs(true);
    target->ptr_cd_->DeleteEntryDirectory();
    CompletePhase(target->phase(), true);
    target->ptr_cd_->UnsetBegin();
    target->Destroy(false);
    CDHandle *next_cdh = CDPath::GetCDLevel(--level);
//    bool need_sync_next_cdh = CDPath::GetParentCD(next_cdh->level())->task_size() > next_cdh->task_size();
    bool need_sync_next_cdh = next_cdh->task_size() > target->task_size();
    CD_DEBUG("level#%u (next_cdh) need sync? %u\n", next_cdh->level(), need_sync_next_cdh);
    return GetCDToRecover(next_cdh, need_sync_next_cdh);
  } else {
    ERROR_MESSAGE("Invalid eslcation point %u (current %u)\n", rollback_lv, level);
    return NULL;
  } 
} // GetCDToRecover ends

/* CD::Complete()
 * (1) Call all the user-defined error checking functions.
 *     Each error checking function should call its error handling function.(mostly restore() and reexec())  
 *
 */
CDErrT CD::Complete(bool update_preservations, bool collective)
{
  CDErrT ret = common::kCompleteError;
//  CD_ASSERT_STR(begin_ == true, "%s %s at %u-%u-%u\n", 
//      __func__, label_.c_str(), level(), phase(), cd_id_.sequential_id_);
//  UnsetBegin();
  uint32_t orig_rollback_point = CheckRollbackPoint(false);
//  bool my_need_reexec = need_reexec;
  const int64_t now_phase = phase();
  const int64_t now_seqID = phaseNodeCache[now_phase]->seq_end_;
  const int64_t now_begID = phaseNodeCache[now_phase]->seq_begin_;
  const int64_t seq_tot = phaseNodeCache[now_phase]->seq_acc_rb_;
  CD_DEBUG("%s %s %s (%ld, %u) %ld->%ld\t Reexec from %d (Before Sync), incomplete logs:%zu\n", 
          GetCDName().GetString().c_str(), 
          GetNodeID().GetString().c_str(), 
          IsFailed()? "REEX" : "EXEC",
          seq_tot, cd_id_.sequential_id_,
          now_begID, now_seqID,
          (int)orig_rollback_point, incomplete_log_.size());
//  printf("%s check this out\n", __func__);

#if CD_MPI_ENABLED
  // This is important synchronization point 
  // to guarantee the correctness of CD-enabled program.
  uint32_t new_rollback_point = orig_rollback_point;
  #if BUGFIX_0327
  if(collective && task_size() > 1) {
    new_rollback_point = SyncCDs(this, false);
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false); // FIXME 04152018
  } else {
    new_rollback_point = CheckRollbackPoint(false);
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false);
  }
  #else
  if(collective) {
    SyncCDs(this);
  }

  CD_DEBUG("%s %s (%u, %u)\t Reexec from %u (After Sync)\n", 
          GetCDName().GetString().c_str(), 
          GetNodeID().GetString().c_str(), 
          phaseTree.current_->seq_end_, 
          phaseTree.current_->seq_acc_, 
          orig_rollback_point);

  if(task_size() > 1 && (CDPath::GetCurrentCD() != GetRootCD())) {
    new_rollback_point = CheckRollbackPoint(true); // read from head
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false);
    CD_DEBUG("rollback point after set it locally:%u\n", new_rollback_point);
  } else {
    new_rollback_point = CheckRollbackPoint(false);
  }
  #endif

#else // CD_MPI_ENABLED ends
  //printf("[%s] okay?\n"); fflush(stdout);
  uint32_t new_rollback_point = CheckRollbackPoint(false); fflush(stdout);
  //printf("[%s] new rollack point = %u\n", __func__, new_rollback_point);
#endif
//  if(new_rollback_point != INVALID_ROLLBACK_POINT) { 
  if(new_rollback_point <= level()) { 
    // If another task set rollback_point lower than this task (error occurred at this task),
    // need_sync is false. 
    // Let's say it was set to 3. But another task set to 1. Then it is false;
    // Or originally it is INVALID_ROLLBACK_POINT, then it is false.
    // (ex. 0xFFFFFFF > any other level. 
    // Therefore, if orig_reexec_flag is INVALID_ROLLBACK_POINT,
    // this conditino is always false.
    // This is true only if this task got failed and set orig_reexec_flag to some level to reexecute from,
    // and no other task raised some lower escalation point than this level.
    // TODO: What if it is reported while calling GetCDToRecover??
    // This is tricky. 
    // Let's say, some other task is stuck at level 1. 
    // And task A set to level 3 from 4 (escalation)
    // It will somehow coordinate tasks in the same CD in level 3, not the other tasks in different CDs at level 3.
    // The tasks corresponding to the CD in level 3 will reach some point,
    // and it detected that it needs to go to level 1 by some tasks in the different CD in the same level 3.
    // 0309
    // Rethink the condition to sync
    // <Example>
    // Task0 is at level 4 and rollback_point is 3
    // The other thread is at level 3. And They are waiting for Task0.
    // This case Task0 must call sync.
    // 
    //
    //bool need_sync = orig_rollback_point <= *rollback_point_ && level() != *rollback_point_;
    // FIXME
//    printf("GetCDLevel : %u (cur %u), path size: %lu\n", *rollback_point_, level(), CDPath::uniquePath_->size());

    // If this task did not expect to reexecute, but is tunred out to be, it does not need sync.
    //bool need_sync = orig_rollback_point == INVALID_ROLLBACK_POINT
    bool need_sync = false;
//    bool need_sync = orig_rollback_point != INVALID_ROLLBACK_POINT; //orig_rollback_point <= *rollback_point_ && (CDPath::GetCDLevel(*rollback_point_)->task_size() != task_size());
//    bool need_sync = CDPath::GetParentCD(level())->task_size() > task_size();
//    printf("need_sync? %d = %u <= %u\n", need_sync, orig_rollback_point, *rollback_point_);
//    CD_DEBUG("need_sync? %d = %u <= %u\n", need_sync, orig_rollback_point, *rollback_point_);
//    GetCDToRecover(*rollback_point_ < cd_id_.cd_name_.level() && *rollback_point_ != INVALID_ROLLBACK_POINT)->Recover(false);
//    *rollback_point_ == level() -> false
//    bool collective = MPI_Group_compare(group());
    CD_DEBUG("## need_sync? %d = %u <= %u ##\n", need_sync, orig_rollback_point, new_rollback_point);
//    printf("## need_sync? %d = %u <= %u ##\n", need_sync, orig_rollback_point, new_rollback_point);
    
    phaseTree.current_->profile_.RecordRollback(true, kComplete);
    /// [IMPORTNANT NOTE]
    /// phase ID preserves the execution order,
    /// therefore, failed_phase will be never updated 
    /// before reaching original failed phase
    /// in reexecution with this assumption.
    /// When it reaches the original failed phase in reexecution,
    /// failed_phase must be reset to HEALTHY (-1) state 
    SetFailure(this->phase(), label_.c_str());

//    const uint32_t phase = this->phase();
////    failed_phase = (failed_phase < (int64_t)phase)? phase : failed_phase;
//    if(failed_phase == HEALTHY) {
//      const int64_t curr_seqID = phaseTree.current_->seq_end_;
//      const int64_t curr_begID = phaseTree.current_->seq_begin_;
//      CD_DEBUG(">>> Failure Detected (%s), fphase:%ld==%lu, seqID:%ld==%lu (beg:%lu) <<<\n", 
//          label_.c_str(), failed_phase, phase, failed_seqID, curr_seqID, curr_begID);
//      if(myTaskID == 0) printf(">>> Failure Detected (%s), fphase:%ld==%u, seqID:%ld==%ld (beg:%ld) <<<\n", 
//          label_.c_str(), failed_phase, phase, failed_seqID, curr_seqID, curr_begID);
//      failed_phase = phase;
//      failed_seqID = phaseTree.current_->seq_end_;
//    }
    
    // Now seq_end_ is initialized to the seq_id at the first begin after create
//    phaseTree.current_->seq_end_ = phaseTree.current_->seq_begin_;
    
//    assert(phaseTree.current_->seq_begin_ == 0);
    
//    // [CHECK 10142017] Should be true
//    if(failed_phase != HEALTHY) {
//      assert(failed_phase >= (int64_t)phase);
//    }
    const uint32_t level = cd_id_.cd_name_.level_;
    GetCDToRecover( CDPath::GetCurrentCD(), need_sync )->ptr_cd()->Recover(level);
  }
  else { // No error occurred

    CD_DEBUG("## Complete. No error! ##\n\n");
    const int64_t curr_phase = this->phase();
    const int64_t curr_seqID = phaseTree.current_->seq_end_;
    const int64_t curr_begID = phaseTree.current_->seq_begin_;
    //if(myTaskID == 0) { printf("## Complete. No error! ##, cur(%lu, %lu), failed(%ld,%ld) %ld\n",
    //    curr_phase, curr_seqID, failed_phase, failed_seqID, phaseTree.current_->seq_begin_); }

//    if(level() == 1) { cd::first_complete = true;   printf("first complete\n");   }
    if (IsFailed()) {
      if(myTaskID == 0) { printf(">>> Complete during rollback! Lv#%u (%s), phase:%lu, seqID:%lu (beg:%lu) <<<\n", 
                                     level(), label_.c_str(), curr_phase, curr_seqID, curr_begID); }

      if(failed_phase == curr_phase) {
        if(failed_seqID == curr_seqID) {
          SetHealthy();
//          failed_phase = HEALTHY;
//          failed_seqID = HEALTHY;
//          //need_escalation = false;
//          //*rollback_point_ = INVALID_ROLLBACK_POINT;        
//          // At this point, take the timer (update RuntimeInfo::clk_)
//          // to measure the rest of execution time.
//          CD_CLOCK_T now = CD_CLOCK();
//          phaseTree.current_->FinishRecovery(now);
//          reex_elapsed_time += now - global_reex_clk;
//          global_reex_clk = now;
          if(myTaskID == 0) { printf(">>> Reached failure point Lv#%u (%s), phase:%lu, seqID:%lu (beg:%lu) <<<\n", 
                                     level(), label_.c_str(), curr_phase, curr_seqID, curr_begID); }
          CD_DEBUG(">>> Reached failure point Lv#%u (%s), phase:%lu, seqID:%lu (beg:%lu) <<<\n", 
                   level(), label_.c_str(), curr_phase, curr_seqID, curr_begID);
        } else if(failed_seqID > curr_seqID) {
          PRINT_MPI(">>> No error Lv#%u (%s) but it was in recovery, phase:%ld==%lu, seqID:%ld==%lu (beg:%lu) <<<\n", 
              level(), label_.c_str(), failed_phase, curr_phase, failed_seqID, curr_seqID, curr_begID);
          CD_DEBUG(">>> No error Lv#%u (%s) but it was in recovery, fphase:%ld==%lu, seqID:%ld==%lu (beg:%lu) <<<\n", 
              level(), label_.c_str(), failed_phase, curr_phase, failed_seqID, curr_seqID, curr_begID);
        } else { // curr_seqID should not be greater than failed_seqID
          ERROR_MESSAGE("Phase(%u, %s): curr_seqID (%lu~%lu) should not be greater than failed_seqID (%ld)\n", 
                         phaseTree.current_->phase_, phaseTree.current_->label_.c_str(), curr_begID, curr_seqID, failed_seqID);
        }
      } 
    }

    reported_error_ = false;
    reexecuted_     = false;
    CompleteLogs();
  
    // Increase sequential ID by one
    cd_id_.sequential_id_++; // reinit at create/destroy

//    phaseTree.current_->seq_end_++; // reinit at failure
//    if(failed_phase == HEALTHY) {
//      phaseTree.current_->seq_acc_++; // no reinit
//    } else {
//      phaseTree.current_->seq_acc_rb_++; // no reinit
//    }
    
    /// It deletes entry directory in the CD (for every Complete() call). 
    /// We might modify this in the profiler to support the overlapped data among sequential CDs.
    DeleteEntryDirectory();
    ret = common::kOK;

    cd_exec_mode_ = kSuspension;
//    phaseTree.current_->profile_.exec_++;
    CompletePhase(this->phase());
    UnsetBegin();
  }

  CD_ASSERT(ret == common::kOK);
  return ret;
}


//void CD::CompletePhase(void) 
//{
//  if(phaseTree.target_ != NULL)
//    phaseTree.target_ = phaseTree.target_->parent_;
//}

CD::CDInternalErrT CD::CompleteLogs(bool is_rollback) 
{
  CD_DEBUG("[%s]\n", (is_rollback)? "DeleteLogs (rollback)":"LeaveLogs");

#if CD_LIBC_LOGGING
  CDHandle *cdh_parent = CDPath::GetParentCD(level());
  if(cdh_parent != NULL) {
    CD *parent = cdh_parent->ptr_cd();
    logger::GetLogger()->FreeMemory(libc_log_begin_);
    parent->libc_log_end_ = logger::GetLogger()->PushLogs(libc_log_begin_) + parent->libc_log_end_;
  }
#endif

#if comm_log
  // SZ: pack logs and push to parent
  if (GetParentHandle()!=NULL) {
    // This is for pushing complete log to parent
    if (MASK_CDTYPE(cd_type_) == kRelaxed) {
      if (IsParentLocal()) {
        if (IsNewLogGenerated() && MASK_CDTYPE(GetParentHandle()->ptr_cd_->cd_type_) == kRelaxed) {
          LOG_DEBUG("Pushing logs to parent...\n");
          comm_log_ptr_->PackAndPushLogs(GetParentHandle()->ptr_cd_);

#if CD_DEBUG_ENABLED
          LOG_DEBUG("\n~~~~~~~~~~~~~~~~~~~~~~~\n");
          LOG_DEBUG("\nchild comm_log print:\n");
          comm_log_ptr_->Print();
#endif

          //SZ: when child pushes logs to parent, parent has new log generated...
          GetParentHandle()->ptr_cd_->comm_log_ptr_->SetNewLogGenerated(true);
          
          ////SZ: if parent is in kReplayLog mode, but child has flipped back to kGenerateLog,
          ////    then parent needs to flip back to kGenerateLog 
          ////FIXME: need to coordinate with other child CDs, and what if some completed CDs reach end of logs, 
          ////      but others do not...
          //if (GetParentHandle()->ptr_cd_->comm_log_ptr_->GetCommLogMode()==kReplayLog && comm_log_ptr_->GetCommLogMode()==kGenerateLog)
          if (comm_log_ptr_->GetCommLogMode()==kGenerateLog) {
            GetParentHandle()->ptr_cd_->comm_log_ptr_->SetCommLogMode(kGenerateLog);
          }

#if CD_DEBUG_ENABLED
          LOG_DEBUG("\n~~~~~~~~~~~~~~~~~~~~~~~\n");
          LOG_DEBUG("parent comm_log print:\n");
          GetParentHandle()->ptr_cd_->comm_log_ptr_->Print();
#endif
        }

        comm_log_ptr_->Reset();
      }
      else  {
        //SZ: FIXME: need to figure out a way to push logs to parent that resides in other address space
        LOG_DEBUG("Should not come to here...\n");
      }

      // FIXME: 04132017
      // This part needs to be reviewed later!!
      if(cd_exec_mode_ == kExecution) {
        // push incomplete_log_ to parent
        if (IsParentLocal() && incomplete_log_.size()!=0) {
          //vector<struct IncompleteLogEntry> *pincomplog 
          //                                    = &(GetParentHandle()->ptr_cd_->incomplete_log_);
          CD *ptmp = GetParentHandle()->ptr_cd_;
    
          // push incomplete logs to parent
          ptmp->incomplete_log_.insert(ptmp->incomplete_log_.end(),
                                       incomplete_log_.begin(),
                                       incomplete_log_.end());
    
          // clear incomplete_log_ of current CD 
          incomplete_log_.clear();
        }
        else if (!IsParentLocal()) {
          //SZ: FIXME: need to figure out a way to push logs to parent that resides in other address space
          LOG_DEBUG("Should not come to here...\n"); assert(0);
        }
      } else if(cd_exec_mode_ == kReexecution || is_rollback) {
        ProbeIncompleteLogs();
      } else {
        ERROR_MESSAGE("Cannot complete logs in kSuspension mode (Relaxed CD) %d\n", cd_exec_mode_);
      }
    }
    else if (MASK_CDTYPE(cd_type_) == kStrict) {
      if(cd_exec_mode_ == kReexecution || is_rollback) {
        InvalidateIncompleteLogs();
      }
      //printf("%s %s %lu\n", GetCDID().GetString().c_str(), label_.c_str(), incomplete_log_.size());
    }
    else { 
      ERROR_MESSAGE("Wrong CD Type : %d\n", cd_type_);
    }
#if _LOG_PROFILING
    GetParentHandle()->ptr_cd()->CombineNumLogEntryAndLogVolume(num_log_entry_, tot_log_volume_);
    num_log_entry_ = 0;
    tot_log_volume_ = 0;
#endif

#if _PGAS_VER
    GetParentHandle()->ptr_cd()->SetSyncCounter(++sync_counter_); 
    LOG_DEBUG("Set parent's SC at CD::Complete with value (%d).\n", sync_counter_);
#endif
  }
#endif // comm_log ends

#if CD_LIBC_LOG_ENABLED
  //GONG
  if(GetParentHandle()!=NULL) {
    if(IsParentLocal()) {
      if(IsNewLogGenerated_libc())  {
        LOG_DEBUG("Pushing logs to parent...\n");
        libc_log_ptr_->PackAndPushLogs_libc(GetParentHandle()->ptr_cd_);
        //libc_log_ptr_->Print();
        GetParentHandle()->ptr_cd_->libc_log_ptr_->SetNewLogGenerated(true);
        if(libc_log_ptr_->GetCommLogMode()==kGenerateLog) {
          GetParentHandle()->ptr_cd_->libc_log_ptr_->SetCommLogMode(kGenerateLog);
        }
        //GetParentHandle()->ptr_cd_->libc_log_ptr_->Print();
      }

      libc_log_ptr_->Reset();
    }
    else {
      LOG_DEBUG("parent and child are in different memory space - libc\n");
    }
  }
//    std::cout<<"size: "<<mem_alloc_log_.size()<<std::endl;

    //GONG: DO actual free completed mem_alloc_log_
    std::vector<IncompleteLogEntry>::iterator it;
    for (it=mem_alloc_log_.begin(); it!=mem_alloc_log_.end(); it++)
    {
//      printf("check log %p %i %i\n", it->p_, it->complete_, it->pushed_);
      if(it->complete_)
      {       
        if(it->pushed_)
        {
//          printf(" free - completed + pushed - %p\n", it->p_);
          if(it->isrecv_)
            free(it->p_);
          else
            fclose((FILE*)it->p_);
          mem_alloc_log_.erase(it);
          it--;
        }
          if(it==mem_alloc_log_.end()){
            break;
        }
      }
    }
/*   for (it=mem_alloc_log_.begin(); it!=mem_alloc_log_.end(); it++)
    {
        printf(" log - %p\n", it->p_);
    }
*/
    //GONG: push mem_alloc_log_
  if(GetParentHandle()!=NULL)
  {
    if (IsParentLocal() && mem_alloc_log_.size()!=0)
    {
      CD *ptmp = GetParentHandle()->ptr_cd_;
      // push memory allocation logs to parent
      std::vector<IncompleteLogEntry>::iterator ii;
      for(it=mem_alloc_log_.begin(); it!=mem_alloc_log_.end(); it++)
      {
        bool found = false;      
        for(ii=ptmp->mem_alloc_log_.begin(); ii!=ptmp->mem_alloc_log_.end(); ii++)
        {
          if(ii->p_ == it->p_)
                  found = true;
        }
//        std::cout<<"push check: "<<it->p_<<" found: "<<found<<std::endl;      

        if(!found)
        {
          it->pushed_ = true;      
          ptmp->mem_alloc_log_.insert(ptmp->mem_alloc_log_.end(), *it);
//          if(it->complete_)
//            ptmp->mem_alloc_log_.end()->pushed_ = true;
        }
      }
      //remove log after pushing to parent 
      mem_alloc_log_.clear();
      //check parent's
/*     std::cout<<"size(parent) :"<<ptmp->mem_alloc_log_.size()<<" app_side: "<<app_side<<std::endl;
      for(ii=ptmp->mem_alloc_log_.begin(); ii!=ptmp->mem_alloc_log_.end(); ii++)
        printf("parent's %p %i %i\n", ii->p_, ii->complete_, ii->pushed_);
*/        
    }
    else if(!IsParentLocal())
    {
      LOG_DEBUG("Should not come to here...\n");
    }
  }


#endif


  return CDInternalErrT::kOK;
} // CD::Complete ends


CDErrT CD::Advance(bool collective)
{
  CDErrT ret = CDErrT::kOK;
#if 0
  uint32_t orig_rollback_point = CheckRollbackPoint(false);
//  bool my_need_reexec = need_reexec;
  CD_DEBUG("%s %s \t Reexec from %u (Before Sync)\n", 
          GetCDName().GetString().c_str(), GetNodeID().GetString().c_str(), orig_rollback_point);
//  printf("%s check this out\n", __func__);

#if CD_MPI_ENABLED
  // This is important synchronization point 
  // to guarantee the correctness of CD-enabled program.
  uint32_t new_rollback_point = orig_rollback_point;
#if BUGFIX_0327
  if(collective) {
    new_rollback_point = SyncCDs(this, false);
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false);
  } else {
    new_rollback_point = CheckRollbackPoint(false);
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false);
  }
#else
  if(collective) {
    SyncCDs(this);
  }

  CD_DEBUG("%s %s \t Reexec from %u (After Sync)\n", 
          GetCDName().GetString().c_str(), GetNodeID().GetString().c_str(), orig_rollback_point);

  if(task_size() > 1 && (CDPath::GetCurrentCD() != GetRootCD())) {
    new_rollback_point = CheckRollbackPoint(true); // read from head
    CD_DEBUG("rollback point from head:%u\n", new_rollback_point);
    new_rollback_point = SetRollbackPoint(new_rollback_point, false);
    CD_DEBUG("rollback point after set it locally:%u\n", new_rollback_point);
  } else {
    new_rollback_point = CheckRollbackPoint(false);
  }
#endif

#else // CD_MPI_ENABLED ends
  //printf("[%s] okay?\n"); fflush(stdout);
  uint32_t new_rollback_point = CheckRollbackPoint(false); fflush(stdout);
  //printf("[%s] new rollack point = %u\n", __func__, new_rollback_point);
#endif
//  if(new_rollback_point != INVALID_ROLLBACK_POINT) { 
  if(new_rollback_point <= level()) { 
    // If another task set rollback_point lower than this task (error occurred at this task),
    // need_sync is false. 
    // Let's say it was set to 3. But another task set to 1. Then it is false;
    // Or originally it is INVALID_ROLLBACK_POINT, then it is false.
    // (ex. 0xFFFFFFF > any other level. 
    // Therefore, if orig_reexec_flag is INVALID_ROLLBACK_POINT,
    // this conditino is always false.
    // This is true only if this task got failed and set orig_reexec_flag to some level to reexecute from,
    // and no other task raised some lower escalation point than this level.
    // TODO: What if it is reported while calling GetCDToRecover??
    // This is tricky. 
    // Let's say, some other task is stuck at level 1. 
    // And task A set to level 3 from 4 (escalation)
    // It will somehow coordinate tasks in the same CD in level 3, not the other tasks in different CDs at level 3.
    // The tasks corresponding to the CD in level 3 will reach some point,
    // and it detected that it needs to go to level 1 by some tasks in the different CD in the same level 3.
    // 0309
    // Rethink the condition to sync
    // <Example>
    // Task0 is at level 4 and rollback_point is 3
    // The other thread is at level 3. And They are waiting for Task0.
    // This case Task0 must call sync.
    // 
    //
    //bool need_sync = orig_rollback_point <= *rollback_point_ && level() != *rollback_point_;
    // FIXME
//    printf("GetCDLevel : %u (cur %u), path size: %lu\n", *rollback_point_, level(), CDPath::uniquePath_->size());

    // If this task did not expect to reexecute, but is tunred out to be, it does not need sync.
    //bool need_sync = orig_rollback_point == INVALID_ROLLBACK_POINT
    bool need_sync = false;
//    bool need_sync = orig_rollback_point != INVALID_ROLLBACK_POINT; //orig_rollback_point <= *rollback_point_ && (CDPath::GetCDLevel(*rollback_point_)->task_size() != task_size());
//    bool need_sync = CDPath::GetParentCD(level())->task_size() > task_size();
//    printf("need_sync? %d = %u <= %u\n", need_sync, orig_rollback_point, *rollback_point_);
//    CD_DEBUG("need_sync? %d = %u <= %u\n", need_sync, orig_rollback_point, *rollback_point_);
//    GetCDToRecover(*rollback_point_ < cd_id_.cd_name_.level() && *rollback_point_ != INVALID_ROLLBACK_POINT)->Recover(false);
//    *rollback_point_ == level() -> false
//    bool collective = MPI_Group_compare(group());
    CD_DEBUG("## need_sync? %d = %u <= %u ##\n", need_sync, orig_rollback_point, new_rollback_point);
//    printf("## need_sync? %d = %u <= %u ##\n", need_sync, orig_rollback_point, new_rollback_point);
#if CD_PROFILER_ENABLED
    end_clk = CD_CLOCK();
    prof_sync_clk = end_clk;
    elapsed_time += end_clk - begin_clk;  // Total CD overhead 
    compl_elapsed_time += end_clk - begin_clk; // Total Complete overhead
    Profiler::profMap[level()][label_.c_str()].compl_elapsed_time_ += end_clk - begin_clk; // Per-level Complete overhead
#endif
    GetCDToRecover( CDPath::GetCurrentCD(), need_sync )->ptr_cd()->Recover();
  }
  else {
    CD_DEBUG("## Complete. No error! ##\n\n");
    reported_error_ = false;
  }

  CompleteLogs();

  reexecuted_ = false;

  // Increase sequential ID by one
  cd_id_.sequential_id_++;
#endif
  return ret;
}


#if CD_LIBC_LOG_ENABLED
//GONG
bool CD::PushedMemLogSearch(void *p, CD *curr_cd)
{
  bool ret = false;
  CDHandle *cdh_temp = CDPath::GetParentCD(curr_cd->level());
  if(cdh_temp != NULL)
  {
//    cdh_temp = CDPath::GetParentCD(curr_cd->level());
    CD *parent_CD = cdh_temp->ptr_cd();
    if(parent_CD!=NULL)
    {
      if(parent_CD->mem_alloc_log_.size()!=0)
      {
        std::vector<IncompleteLogEntry>::iterator it;  
        for(it=parent_CD->mem_alloc_log_.begin(); it!=parent_CD->mem_alloc_log_.end();it++)    {
          if(it->p_ == p)
          {
            ret = true;
            //If isnt completed, change it completed.
//            it->complete_ = true;
            break;
          }
        }
      }
      else
      {
        ret = PushedMemLogSearch(p, parent_CD);
      }
    }
    else
    {
      CD_DEBUG("CANNOT find parent CD\n");
      assert(0);
//      exit(1);
    }
  }

  return ret;
}



unsigned int CD::PullMemLogs()
{
  unsigned int num_logs = 0;
  CDHandle *cdh_temp = CDPath::GetParentCD(level());
  std::vector<IncompleteLogEntry>::iterator it;  
  if(cdh_temp != NULL)
  {
    CD *parent_CD = cdh_temp->ptr_cd();
    if(parent_CD!=NULL)
    {
      for(it=parent_CD->mem_alloc_log_.begin(); it!=parent_CD->mem_alloc_log_.end();it++)
      { 
        //bring all logs lower than or equal to current CD
        CD_DEBUG("try to pull logs %p %ld %u %u\n", it->p_, it->flag_, it->level_, level());
        if(it->level_ >= level())
        {
          mem_alloc_log_.insert(mem_alloc_log_.end(), *it);      
          num_logs++;
        }
      }
    }
    else
    {
      CD_DEBUG("CANNOT find parent CD\n");
      assert(0);
    }
  }
  for(it=mem_alloc_log_.begin(); it!=mem_alloc_log_.end(); it++)
    CD_DEBUG("pulled logs %p %i %i %ld\n", it->p_, it->complete_, it->pushed_, it->flag_);
    CD_DEBUG("cur_pos_mem_alloc_log: %u\n", num_logs);

  return num_logs;

}





void *CD::MemAllocSearch(CD *curr_cd, unsigned int level, unsigned long index, void *p_update)
{
  void *ret = NULL;
  CDHandle *cdh_temp = CDPath::GetParentCD(curr_cd->level());
  if(cdh_temp != NULL)
  {
    CD *parent_CD = cdh_temp->ptr_cd();
    if(parent_CD!=NULL)
    {
      //GONG:       
      if(parent_CD->mem_alloc_log_.size()!=0)
      {
        if(!p_update)
        {
          CD_DEBUG("parent_CD->cur_pos_mem_alloc_log: %lu\n", parent_CD->cur_pos_mem_alloc_log);      
          std::vector<IncompleteLogEntry>::iterator it;
          for (it=parent_CD->mem_alloc_log_.begin(); it!=parent_CD->mem_alloc_log_.end(); it++){
            //should be unique!
            if(it->level_ == level && it->flag_ == (int64_t)index){
              CD_DEBUG("level: %u, index: %lu\n", level, index);   
              ret = it->p_;
//              break;
            }
          }
          //ret = parent_CD->mem_alloc_log_[parent_CD->cur_pos_mem_alloc_log].p_;

         }
         else
         {
           ret = parent_CD->mem_alloc_log_[parent_CD->cur_pos_mem_alloc_log].p_;
           parent_CD->mem_alloc_log_[parent_CD->cur_pos_mem_alloc_log].p_ = p_update;
         }
//       parent_CD->cur_pos_mem_alloc_log++;
       }
       else
       {
         CD_DEBUG("mem_alloc_log_.size()==0, search parent's log\n");      
         ret = MemAllocSearch(parent_CD, level, index, p_update);
      }
    }
    else
    {
      ERROR_MESSAGE("CANNOT find parent CD\n");
    }
  }
  else
  {
    ERROR_MESSAGE("rootCD is trying to search further\n");
  }
  
  if(ret == NULL)
  {
    ERROR_MESSAGE("somethig wrong!\n");
  }

  return ret;
}

#endif

void *CD::SerializeRemoteEntryDir(uint64_t &len_in_bytes) 
{
//  entry_directory_.GetTable()->FindWithAttr(kremote);
  uint64_t remote_entry_cnt = remote_entry_directory_map_.size();
  RemoteCDEntry *serialized = NULL;
  if(remote_entry_cnt != 0) {
    serialized = new RemoteCDEntry[remote_entry_cnt];
    uint64_t i=0;
    for(auto it = remote_entry_directory_map_.begin(); 
             it!= remote_entry_directory_map_.end(); 
             ++it) {
      serialized[i++] = *(it->second);
    }
  }
  len_in_bytes = remote_entry_cnt * sizeof(RemoteCDEntry); 
  return (void *)serialized;
}

void HeadCD::DeserializeRemoteEntryDir(void *object, uint64_t totsize)
{
  totsize /= sizeof(CDEntry);
  RemoteCDEntry *entry = reinterpret_cast<RemoteCDEntry *>(object);

  uint64_t begin = remote_entry_table_.tail();
  remote_entry_table_.Insert(entry, totsize);

  for(uint64_t i=0; i<totsize; i++) {
    RemoteCDEntry &remote_entry = remote_entry_table_[begin+i];
    remote_entry_directory_map_[remote_entry.id_] = &remote_entry;
    CD_ASSERT(remote_entry_table_[begin+i].id_ == entry[i].id_);
  }
}

#if 0
void CD::DeserializeRemoteEntryDir(EntryDirType &remote_entry_dir, void *object, uint64_t task_count, uint64_t unit_size) 
{
  TableStore<CDEntry> table;
  entry_directory_.GetTable()->FindWithAttr(kremote);
  void *unpacked_entry_p=0;
  uint64_t dwGetID=0;
  uint64_t return_size=0;
  char *begin = (char *)object;

  CD_DEBUG("\n[CD::DeseralizeRemoteEntryDir] addr: %p at level #%u", i
      object, CDPath::GetCurrentCD()->ptr_cd()->GetCDID().level());

  for(uint64_t i=0; i<task_count; i++) {
    Unpacker entry_dir_unpacker;
    while(1) {
      unpacked_entry_p = entry_dir_unpacker.GetNext(begin + i*unit_size, dwGetID, return_size);
      if(unpacked_entry_p == NULL) {

//      cddbg<<"DESER new ["<< cnt++ << "] i: "<< i <<"\ndwGetID : "<< dwGetID << endl;
//      cddbg<<"return size : "<< return_size << endl;
  
        break;
      }
      CDEntry *cd_entry = new CDEntry();
      cd_entry->Deserialize(unpacked_entry_p);

      if(CDPath::GetCurrentCD()->IsHead()) {
        CD_DEBUG("Entry check before insert to entry dir: %s, addr: %p\n", 
            tag2str[cd_entry->entry_tag_].c_str(), object);
      }

      remote_entry_dir.insert(std::pair<ENTRY_TAG_T, CDEntry*>(cd_entry->entry_tag_, cd_entry));
//    remote_entry_dir[cd_entry->entry_tag_] = cd_entry;
    }
  }
}
#endif

// FIXME
PrvMediumT CD::GetPlaceToPreserve()
{
  return prv_medium_;
}

/**********************************************************************************
 * CD::Preserve()
 **********************************************************************************
 * Every CDHandle::Preserve() calls this internal preserve call.
 * It counts # of Preserve() in execution / reexecution mode
 * in order to track "which point of program is the last point before rollback".
 * This point is important for libc and comm logging aspects.
 * The mode is represented with cd_exec_mode_.
 *
 * [NOTE] Use case.
 * One might catch up with some question on how to use Preserve()
 * when he/she wants to map multiple CD levels like SCR.
 * Let's consider three levels.
 *
 * 
 * while(cond)
 * {
 *    // Current domain is leaf level
 *    if(is_lv0_interval()) { parent->Preserve(A, sizeof(A), kCopy, "A"); 
 *                            child->Preserve(A, sizeof(A), kRef, "A"); 
 *                            leaf->Preserve(A, sizeof(A), kRef, "A"); } 
 *    else if(is_lv1_interval()) { child->Preserve(A, sizeof(A), kCopy, "A"); 
 *                            leaf->Preserve(A, sizeof(A), kRef, "A"); }
 *    else if(is_lv2_interval()) { leaf->Preserve(A, sizeof(A), kCopy, "A"); }
 *    DoCompute(A);
 * }
 *
 * Users might be confused that A might be restored multiple times
 * because of such a mapping with three successive preserve calls from
 * different levels to the same array A.
 * Note that the way to determine Preserve() or Restore() is based on
 * cd_exec_mode_ which is independent per level. 
 * When parent->Preserve() is performing restoration because of escalation for
 * example, it counts restore_count_ in parent level.
 * Therefore, during child/leaf-level reexecution, parent->Preserve() should be
 * never called in order not to increment preserve_count_ at parent level, which
 * will cause incorrect behavior by mapping. That is why we use different
 * if-else conditions for preserving A with multiple CD levels.
 *
 * [NOTE] kOutput use case.
 * while(cond)
 * {
 *    // Current domain is leaf level
 *    if(is_lv2_interval()) { leaf->Preserve(A, sizeof(A), kRef, "A"); }
 *    DoCompute(A);
 *    if(is_lv0_interval())      { parent_dummy->Preserve(A, sizeof(A), kOutput, "A"); }
 *    else if(is_lv1_interval()) { child_dummy->Preserve(A, sizeof(A), kOutput, "A"); }
 *
 * }
 **********************************************************************************
 
 **********************************************************************************
 * The following comments are old and may be out-dated, but left here just in case.
 **********************************************************************************
 * CD::Preserve(char *data_p, int data_l, enum preserveType prvTy, enum mediumLevel medLvl)
 * Register data information to preserve if needed.
 * (For now, since we restore per CD, this registration per cd_entry would be thought unnecessary.
 * We can already know the data to preserve which is likely to be corrupted in future, 
 * and its address information as well which is in the current memory space.
 * Main Purpose: 1. Initialize cd_entry information
 *      2. cd_entry::preserveEntry function call. 
 *         -> performs appropriate operation for preservation per cd_entry such as actual COPY.
 * We assume that the AS *dst_data could be known from Run-time system 
 * and it hands to us the AS itself from current rank or another rank.
 * However, regarding COPY type, it stores the back-up data in current memory space beforehand. 
 * So it allocates(ManGetNewAllocation) memory space for back-up data
 * and it copies the data from current or another rank to the address 
 * for the back-up data in my current memory space. 
 * And we assume that the data from current or another rank for preservation 
 * can be also known from Run-time system.
 * ManGetNewAllocation is for memory allocation for CD and cd_entry.
 * CD_MALLOC is for memory allocation for Preservation with COPY.
 *
 * Jinsuk: For re-execution we will use this function call to restore the data. 
 * So basically it needs to know whether it is in re-execution mode or not.
 * it is in re-execution mode, so instead of preserving data, restore the data.
 * Two options, one is to do the job here, 
 * another is that just skip and do nothing here but do the restoration job in different place, 
 * and go though all the CDEntry and call Restore() method. 
 * The later option seems to be more efficient but it is not clear that 
 * whether this brings some consistency issue as restoration is done at the very beginning 
 * while preservation was done one by one 
 * and sometimes there could be some computation in between the preservations.. (but wait is it true?)
 * 
 * Jinsuk: Because we want to make sure the order is the same as preservation, we go with  Wait...... It does not make sense... 
 * Jinsuk: For now let's do nothing and just restore the entire directory at once.
 * Jinsuk: Caveat: if user is going to read or write any memory space that will be eventually preserved, 
 * FIRST user need to preserve that region and use them. 
 * Otherwise current way of restoration won't work. 
 * Right now restore happens one by one. 
 * Everytime restore is called one entry is restored. 
 **********************************************************************************
 */

CDErrT CD::Preserve(void *data, 
                    uint64_t &len_in_bytes, 
                    CDPrvType preserve_mask, 
                    std::string my_name,          // data name      
                    std::string ref_name,         // reference name
//                    const char *my_name, 
//                    const char *ref_name, 
                    uint64_t ref_offset, 
                    const RegenObject *regen_object, 
                    PreserveUseT data_usage)
{
//  printf("[CD::Preserve] data addr: %p, len: %lu, entry name : %s, ref name: %s, [cd_exec_mode : %d], # reexec:%d\n", 
//           data, len_in_bytes, my_name, ref_name, cd_exec_mode_, num_reexecution_); 
  CDErrT ret = CDErrT::kOK;
//  uint64_t tag = cd_hash(my_name);
//  tag2str[tag] = my_name;
//  if (IsFailed() && phase() != failed_phase) {
//    printf("%d %s %u != %ld\n", myTaskID, IsFailed()? "FAIL" : "GOOD", phase(), failed_phase);
//    assert(0); return ret; }
  CD_DEBUG("LV%u data addr: %p, len: %lu, entry name : %s, ref name: %s, ref_offset:%lu, [cd_exec_mode : %d] rstr:%u < prsv:%u (fail:%u->%ld|%ld->%ld)\n", level(),
           data, len_in_bytes, my_name.c_str(), ref_name.c_str(), ref_offset, cd_exec_mode_, 
           restore_count_, preserve_count_, 
           this->phase(), cd::failed_phase, cd::phaseTree.current_->seq_end_, cd::failed_seqID);
//  printf("\n\n[CD::Preserve] data addr: %p, len: %lu, entry name : %s, ref name: %s, [cd_exec_mode : %d]\n", 
//           data, len_in_bytes, my_name, ref_name, cd_exec_mode_); 

//  CD_DEBUG("prv mask (%d) : %d(kCopy) %d(kRef) %d(kRegen) , kCoop : %d]\n\n",
//           preserve_mask,
//           CHECK_PRV_TYPE(preserve_mask, kCopy),
//           CHECK_PRV_TYPE(preserve_mask, kRef),
//           CHECK_PRV_TYPE(preserve_mask, kRegen),
//           CHECK_PRV_TYPE(preserve_mask, kCoop));
//  printf("%s %s\n", my_name.c_str(), ref_name.c_str());
  if (IsReexec() && myTaskID == 0) {
    CD_DEBUG("%s [%d] %s cd_exec_mode:%d ftype:%x\n", __func__, myTaskID, label_.c_str(), cd_exec_mode_, entry_directory_.data_->ftype());
  }

  if( CHECK_PRV_TYPE(preserve_mask, kOutput) ) { 

    if(is_koutput_disabled) {
      // do nothing
    } else if(cd_exec_mode_  == kExecution ) {    
      switch( InternalPreserve(data, len_in_bytes, preserve_mask, 
                               my_name, ref_name, ref_offset, 
                               regen_object, data_usage) ) {
        case CDInternalErrT::kOK            : {
                ret = CDErrT::kOK;
                break;
                                              }
        case CDInternalErrT::kExecModeError : {
                ret = CDErrT::kError;
                assert(0);
                break;
                                              }
        case CDInternalErrT::kEntryError    : {
                ret = CDErrT::kError;
                assert(0);
                break;
                                              }
        default : assert(0);
      }
    } else if(cd_exec_mode_ == kReexecution) { 
      // do nothing
    } else {
      ERROR_MESSAGE("Undefined execution state : %d\n", cd_exec_mode_);
    }

  } else {
    if(cd_exec_mode_  == kExecution ) {      // Normal execution mode -> Preservation
      if(myTaskID == 0) {
  //    printf("Preserv [%d %s]tag:%lu prv:%lu rst:%lu\n", myTaskID, my_name.c_str(), tag, preserve_count_, restore_count_);
      }
  //    cddbg<<"my_name "<< my_name<<endl;
      if(strcmp(my_name.c_str(), "MainLoop_symmX") == 0 || strcmp(my_name.c_str(), "locDom_Root") == 0 ) {
  //      printf("[%d] #################### %s ############:%lu\n", myTaskID, my_name.c_str(), tag);
      }
      switch( InternalPreserve(data, len_in_bytes, preserve_mask, 
                               my_name, ref_name, ref_offset, 
                               regen_object, data_usage) ) {
        case CDInternalErrT::kOK            : {
                preserve_count_++;
                ret = CDErrT::kOK;
                break;
                                              }
        case CDInternalErrT::kExecModeError : {
                ret = CDErrT::kError;
                assert(0);
                break;
                                              }
        case CDInternalErrT::kEntryError    : {
                ret = CDErrT::kError;
                assert(0);
                break;
                                              }
        default : assert(0);
      }
  
    }
    // FIXME: 09.21.2019
    // App data is restored when preserve call is invoked during re-execution. 
    // However, this becomes problemetic when we allow parent-level preservation 
    // inside child-level CD. 
    // This preserve call boundary indicates that the latest time when the user 
    // data must be preserved/restored. 
    // The earliest time for restoring the data is beginning of a CD and
    // that for preserving the data is when the data is the latest point when
    // the data is updated.
    //
    // New suggestion is to start restoring in the beginning of CDs with preservation
    // table, and wait for restoring the data when re-execution encounters
    // the preserve call (restore call internally). 
    // The initiation of restoration is performed at InternalReexecute() 
    // See also RestoreAll()
    else if(cd_exec_mode_ == kReexecution) { // Re-execution mode -> Restoration
      assert(dont_preserve == false);
      
      CD_DEBUG("Reexecution!!! entry directory size : %zu (medium:%d)\n\n", 
               entry_directory_.table_->tablesize(), prv_medium_);
  
      if( restore_count_ < preserve_count_ ) { // normal case
        
        CD_DEBUG("\t\t\tNow reexec %s Lv%u!!! %d rstr:%u < prsv:%u (fail:%u->%ld|%ld->%ld)\n\n", 
            my_name.c_str(), level(), iterator_entry_count++, restore_count_, preserve_count_, 
            this->phase(), cd::failed_phase, cd::phaseTree.current_->seq_end_, cd::failed_seqID);
        assert(cd::failed_phase >= 0 && cd::failed_seqID >= 0);
        Restore((char *)data, len_in_bytes, preserve_mask, my_name, ref_name);
        restore_count_++;
        if( restore_count_ == preserve_count_ ) { 
          PRINT_BOTH("@@@ Reset to kReexecution at %s @@@ %s (%ld), IsReexec():%s\n",
                  label_.c_str(), cd::phaseNodeCache[cd::failed_phase]->label_.c_str(), 
                  cd::failed_seqID, (IsReexec())? "YES":"NO");
          cd_exec_mode_ = kExecution;
          restore_count_ = 0;
        } 
        GetRemoteEntry(); 
      }
      else {  
        // abnormal case -> kReexecution mode, but iterator reaches the end.
        // NOT TRUE if we have reached this point that means now we should actually start preserving instead of restoring.. 
        // we reached the last preserve function call. 
        // Since we have reached the last point already now convert current execution mode into kExecution
        ERROR_MESSAGE("Error: Now in re-execution mode but preserve function is called " 
                      "more number of time than original, prv #: %lu, rst #: %lu, mode:%d\n", 
                      preserve_count_, restore_count_, cd_exec_mode_); 
      }
  
      CD_DEBUG("Reexecution mode finished...\n");
    }   // Re-execution mode ends
    else {  // Suspension mode
      ERROR_MESSAGE("Preserve() is called during kSuspension(%d)\n", cd_exec_mode_);
    }

  } // prv_mode != kOutput ends
  return ret; // we should not encounter this point
}



// Non-blocking Preserve
CDErrT CD::Preserve(CDEvent &cd_event,     
                    void *data_ptr, 
                    uint64_t &len, 
                    CDPrvType preserve_mask, 
                    std::string my_name,          // data name      
                    std::string ref_name,         // reference name
//                    const char *my_name, 
//                    const char *ref_name, 
                    uint64_t ref_offset, 
                    const RegenObject *regen_object, 
                    PreserveUseT data_usage)
{
  // stub
  return kError; 
}
#include "packer_prof.h"
CDEntry *CD::PreserveCopy(void *data, 
                          uint64_t &len_in_bytes, 
                          CDPrvType preserve_mask, 
                          const std::string &my_name, uint64_t id)
{
  CD_DEBUG("data:%p len:%lu mask:%x name:%s, id:%lx\n", data, len_in_bytes, preserve_mask, my_name.c_str(), id);
  CDEntry *pEntry = NULL;
  uint64_t attr = (CHECK_PRV_TYPE(preserve_mask, kCoop))? Attr::kremote : 0;
  attr = (CHECK_PRV_TYPE(preserve_mask, kOutput))? (Attr::koutput | attr) : attr;
  if( CHECK_PRV_TYPE(preserve_mask, kSerdes) ) {
  // CDEntry for reference has different format
  //  8B      8B            8B       8B
  // [ID] [ATTR|SIZE]    [OFFSET]   [SRC]
  //  ID  [ATTR|totsize] totoffset  tableoffset
  //
  //  +------------+   <--OFFSET
  //  |            |
  //  |            |
  //  +------------+
  //  |            |
  //  |   Data     |
  //  |            |
  //  +------------+   <--SRC
  //  |   Table    |
  //  +------------+   
  //
    attr |= Attr::knested;
//    entry_directory_.data_->PadZeros(0);
    const uint64_t packed_offset  = entry_directory_.data_->used();
    PackerSerializable *serializer = static_cast<PackerSerializable *>(data);
    // returns length of serialized object
    // FIXME: PreserveObject must append the table for serialized object to data chunk.
    // Serializer should use the same DataStore for in-place preservation,
    // but use the separate table. This table is different from entry_directory_,
    // and will be just appended in its DataStore, not in its TableStore.
    len_in_bytes = serializer->PreserveObject(entry_directory_, kCopy, my_name.c_str());
    
    tag2str[serializer->id_] = serializer->GetID();
    uint64_t table_offset = entry_directory_.data_->used();
    packer::MagicStore &magic = entry_directory_.data_->GetMagicStore();
    magic.table_offset_ = entry_directory_.AppendTable();
    magic.total_size_   = entry_directory_.data_->used();
    magic.entry_type_   = packer::kCDEntry;
    magic.reserved2_    = 0x12345678;
    magic.reserved_     = 0x01234567;
    uint64_t packed_total_size = entry_directory_.data_->used() - packed_offset;
    entry_directory_.data_->FlushMagic(&magic);
    pEntry = entry_directory_.table_->InsertEntry(
        CDEntry(id, attr, packed_total_size, packed_offset, (char *)table_offset));
    PRINT_MPI("[%s] Insert %s CDEntry(%lx, %lx, size:%lx, offset:%lx, tbl_offset:%lx, tblsize:%lx)), (%lx, %s %s), %zu\n", 
         label_.c_str(), my_name.c_str(),
         id, attr, packed_total_size, packed_offset, table_offset, packed_offset + packed_total_size - table_offset,
         serializer->id_, tag2str[serializer->id_].c_str(), serializer->GetID(), sizeof(CDEntry));
    char tmp[128];
    sprintf(tmp, "Domain %s : ", my_name.c_str());
    pEntry->Print(cdout, tmp);
    sprintf(tmp, "Prsv %s Lv%u %s ", my_name.c_str(), level(), IsFailed()? "REEX" : "EXEC");

    entry_directory_.table_->PrintEntry(cdout, tmp, GetCDEntryStr); /* FIXME: turn on for debug */
    if (myTaskID == 0)
      entry_directory_.table_->PrintEntry(stdout, tmp, GetCDEntryStr); /* FIXME: turn on for debug */
  }
  else { // preserve a single entry
    attr |= Attr::karray;
#ifdef LULESH_DEBUG
    char tmp[64];
    sprintf(tmp, "Preserve Domain lv:%u %s", level(), label_.c_str());
    ((Internal *)data)->Print(tmp, cdout);
#endif

//    CD_CLOCK_T then = CD_CLOCK();
    pEntry = entry_directory_.AddEntry((char *)data, CDEntry(id, attr, len_in_bytes, 0, (char *)data));
//    CD_CLOCK_T now = CD_CLOCK();
//    double elapsed = now - then;
//    printf("################# \t Preserve %s (%lx), size:%lx, at %p, time:%lf\n", my_name.c_str(), id, len_in_bytes, data, elapsed);
#ifdef LULESH_DEBUG
    CD_DEBUG("Preserve %s (%lx), size:%lx, at %p\n", my_name.c_str(), id, len_in_bytes, data);
    sprintf(tmp, "Domain %s : ", my_name.c_str());
    pEntry->Print(cdout, tmp);
#endif
  }
//#ifdef _DEBUG_0402        
#if 0
  if(myTaskID == 0) {
    printf("== Preserve Complete [%s->%s at lv #%u %u] cnt:%lu, tag:%u size:%lu, %p ==\n", 
      label_.c_str(), my_name.c_str(), level(), GetCurrentCD()->level(), 
      preserve_count_, (uint32_t)id, len_in_bytes, data);
  }
#endif
    
  if(prv_medium_ != kDRAM) {
    entry_directory_.data_->Flush(); 
  }
//  if(myTaskID == 0) { printf("prv size:%lu, %lu\n", phaseTree.current_->profile_.prv_copy_ + len_in_bytes, packer::time_mpiio_write.size); }
  return pEntry;
}

/**
 * Current (somewhat naive) approach for kOutput is to preserve output aa
 * after CD A to memory at dummy CD in advance, then write it to file 
 * at some point before a CD that is mapped to disk level and reads aa. 
 * If it is mapped to memory level, I thought it does not copy aa at the 
 * beginning of the next CD reading aa, but preserve via ref. 
 * (so the output is preserved via copy at dummy).
 */


// https://github.com/opcm/pcm
CD::CDInternalErrT 
CD::InternalPreserve(void *data, 
                     uint64_t &len_in_bytes, 
                     CDPrvType preserve_mask, 
                     const std::string &my_name, 
                     const std::string &ref_name, 
                     uint64_t ref_offset, 
                     const RegenObject *regen_object, 
                     PreserveUseT data_usage)
{
  CD_ASSERT_STR(cd_exec_mode_ == kExecution, 
      "InternalPreserve call was invoked not in kExecution mode: %u\n", cd_exec_mode_);
//  CD_DEBUG("[CD::InternalPreserve] cd_exec_mode : %d (should be normal exec mode)\n", cd_exec_mode_);
  CDInternalErrT err = kOK;
  assert(my_name.empty() == false);
  uint64_t id = (my_name.empty())? INVALID_NUM : cd_hash(my_name);
  uint64_t ref_id = 0;
  tag2str[id] = my_name;
  if(ref_name.empty()) { 
    ref_id = id;
    tag2str[ref_id] = my_name;
//    printf("ref_id is empty\n");
  } else {
    ref_id = cd_hash(ref_name);
    tag2str[ref_id] = ref_name;
//    printf("ref_id is valid\n");
  }

  if(cd::dont_preserve == false) {
    CDEntry *pEntry = NULL;
    
    // TODO
    // kOutput mode copies application data to packer immediately at Preserve()
    // It may be optimized in the way that it registers entry at Preserve()
    // and runtime decides actual copy from application space to preservation store.
    if( CHECK_ANY(preserve_mask, kCopy|kOutput) ) { // via-copy, so it saves data right now!
      CD_ASSERT_STR(my_name.empty() == false, 
                    "Entry name is not specified : %s\n", my_name.c_str());
      pEntry = PreserveCopy(data, len_in_bytes, preserve_mask, my_name, id);
  
    } // end of preserve via copy
    else if( CHECK_PRV_TYPE(preserve_mask, kRef) ) { // via-reference
      uint64_t attr = (CHECK_PRV_TYPE(preserve_mask, kCoop))? 
                      (Attr::kremote | Attr::krefer) : Attr::krefer;
      
      CD_DEBUG("Preservation via %d (reference)\n", GetPlaceToPreserve());
    
      // Let's just replace kRef entry ID with ref_id
      // if there is no entry name passed by user.
      //uint64_t id = (my_name.empty())? ref_id : cd_hash(my_name);
      //tag2str[id] = my_name;
  
      // CDEntry for reference has different format
      //  8B      8B            8B       8B
      // [ID] [ATTR|SIZE]    [OFFSET]   [SRC]
      //  ID  [ATTR|totsize] totoffset  REF_ID
      uint64_t size = len_in_bytes;
      
      // IMPORTANT NOTE for the case of kSerdes | kRef
      // Preserve() registers an entry only to packer table.
      // In Restore(), it searches for this entry with ref_name (ref_id).
      // If there is an CDEntry with ref_id somewhere,
      // it MUST have the entries encapsulated by serializable entry. (Assumption)
      if (CHECK_PRV_TYPE(preserve_mask, kSerdes)) { 
        PackerSerializable *serializer = static_cast<PackerSerializable *>(data);
        attr |= Attr::knested;// | Attr::ktable);
        size = serializer->GetTotalSize();
        len_in_bytes = size;
      } 
  
      // NOTE
      // for kRef | kIgnore, it does not search for tag when Preserve() is invoked.
      // So, if there is no matching entry with ref_name on rollback,
      // it will be failed.
      // The semantic of kRef is to search for ref_id, 
      // and if there is ref_id which is valid in the same level, do nothing.
      // if it is not valid, wait until valid.
      // If it is parent level, create entry with krefer.
      if (CHECK_PRV_TYPE(preserve_mask, kIgnore)) { 
        //pEntry = entry_directory_.AddEntry((char *)data, CDEntry(id, attr, size, ref_offset, (char *)ref_id));
        pEntry = entry_directory_.table_->InsertEntry(CDEntry(id, attr, size, ref_offset, (char *)ref_id));
      } else {
        uint32_t found_level = INVALID_NUM32;
        CDEntry *src = SearchEntry(ref_id, found_level, Attr::knoattr /* Attr::kempty FIXME 12112018 */ );
//        CDEntry *src = entry_directory_.table_->FindReverse(ref_id, 0);
        if(src != NULL) { // found entry
//          CD *ptr_cd = CDPath::GetCDLevel(found_level)->ptr_cd();
          while(src->invalid()); // do not create CDEntry, but just wait until valid
          src->size_.Unset(Attr::koutput);
          if (CHECK_PRV_TYPE(preserve_mask, kOutput) == false) { // kRef 
            pEntry = entry_directory_.table_->InsertEntry(CDEntry(id, attr, size, ref_offset, (char *)ref_id)); }
//          CD *ptr_cd = CDPath::GetCDLevel(found_level)->ptr_cd();
//          if(ptr_cd == this) {
//            while(src->invalid()); // do not create CDEntry, but just wait until valid
//          } else {
//            if (CHECK_PRV_TYPE(preserve_mask, kOutput) == false) { 
//              pEntry = entry_directory_.table_->InsertEntry(CDEntry(id, attr, size, ref_offset, (char *)ref_id));
//            }
//          }
        } else { // not found
          PRINT_MPI("No entry %lx %s (mask:%d) for kRef. Now Copy! lv:%u, entry #:%lu\n", 
                                ref_id, 
                                my_name.c_str(), preserve_mask, 
                                level(),
                                entry_directory_.table_->tablesize());
  
          if (CHECK_PRV_TYPE(preserve_mask, kOutput) == false) { 
            pEntry = PreserveCopy(data, len_in_bytes, preserve_mask, my_name, ref_id);
          }
  
        }
          
      }
  
      PRINT_MPI("[%s] Ref Insert %s CDEntry(%lx, %lu, size:%lu, offset:%lu, table_offset:%s(%lx)), tblsize:%lu\n", 
                 label_.c_str(), my_name.c_str(),
                 id, attr, size, ref_offset, ref_name.c_str(), ref_id, entry_directory_.table_->tablesize());
      // When restore data for reference of serdes object,
      // check ref and nested first.
      // then find ref_id, then read table from ref_id to ref_id+size.
      // Then restore data 
      err = CDInternalErrT::kOK;
    } // via-reference ends
    else if( CHECK_PRV_TYPE(preserve_mask, kRegen) ) { // via-regeneration
      //TODO
      ERROR_MESSAGE("Preservation via Regeneration is not supported, yet. :-(");
      err = CDInternalErrT::kOK;
    }
    else {  // Preservation Type is none of kCopy, kRef, kRegen.
      ERROR_MESSAGE("\nUnsupported preservation type : %d\n", preserve_mask);
    }
  
    // remote_entry_directory_map_ is a cache containing
    // local preservation entry (CDEntry object) pointers
    // in order to serialize them to send to HeadCD.
    if( (id != INVALID_NUM) && CHECK_PRV_TYPE(preserve_mask, kCoop) ) {
      CD_DEBUG("[CD::InternalPreserve] Error : kRef | kCoop\n"
               "Tried to preserve via reference "
               "but tried to allow itself as reference to other node. "
               "If it allow itself for reference locally, it is fine!");
      remote_entry_directory_map_[id] = pEntry;
      if(IsHead()) {
        static_cast<HeadCD *>(this)->remote_entry_table_.Insert(RemoteCDEntry(*pEntry));
      }
    }
  } // dont_preserve ends
//  else {
//    if (myTaskID == 0) {
//      std::cout << "do not preserve is true" << std::endl;
//    }
//  }
//    if (myTaskID == 0) {
//      std::cout << "do not preserve is " << cd::dont_preserve<<std::endl;
//    }
//  preserve_count_++;

  return err; 
}

/**********************************************************************************
 * CD::Restore()
 **********************************************************************************
 * Copy preserved data from packer to *data
 * This restoration method has all the information 
 * such as dst address, length, preservation type, reference ID.
 * therefore it only needs to search source entry for actual preserved data.
 * In the case of kCopy, the key (entry_str) should be its entry_name.
 * In the case of kRef, the key (entry_str) should be ref_name.
 **********************************************************************************
 * (1) Copy preserved data to application data. It calls restoreEntry() at each node of entryDirectory list.
 * (2) Do something for process state
 * (3) Logged data for recovery would be just replayed when reexecuted. We need to do something for this.
 **********************************************************************************/
CD::CDInternalErrT CD::Restore(char *data, uint64_t len_in_bytes, CDPrvType preserve_mask, 
                               const string &my_name, const string &ref_name)
{
  CD::CDInternalErrT ret = kOK; 
  CD_DEBUG("\t\t\tNow reexec %s!!! rstr:%u < prsv:%u (fail:%u->%ld/%ld->%ld)\n", 
      my_name.c_str(), level(), restore_count_, preserve_count_, this->phase(), cd::failed_phase, 
      cd::phaseTree.current_->seq_end_, cd::failed_seqID);
  const bool is_ref = CHECK_PRV_TYPE(preserve_mask, kRef);
  uint32_t found_level = INVALID_NUM32;
  uint64_t ref_id = 0; //cd_hash(ref_name);
  uint64_t entry_id = cd_hash(my_name);
  if(ref_name.empty()) { 
    ref_id = entry_id;
  } else {
    ref_id = cd_hash(ref_name);
  }
  uint64_t search_tag = (is_ref)? ref_id : entry_id;
  
  // Search the preservation entry which is not krefer.
  // It tries to search until it finds the entry without krefer.
  CDEntry *src = SearchEntry(search_tag, found_level, Attr::knoattr /* Attr::koutput FIXME 12102018 */);
  assert(src != NULL);
  if(src == NULL) {
    assert(0);
    CDHandle *parent_cd = GetCurrentCD();
    while( parent_cd != NULL ) {
      CD *pcd = parent_cd->ptr_cd();
#if CD_DEBUG_ENABLED
      if(myTaskID == 0) {
        char tmp[16];
        sprintf(tmp, "Restore Lv%u", pcd->level());
        pcd->entry_directory_.table_->PrintEntry(stdout, tmp, GetCDEntryStr);
      }
#endif
      uint64_t tag = search_tag;
      src = pcd->entry_directory_.table_->FindReverse(tag, Attr::knoattr /* Attr::koutput FIXME 12102018 */);
      if (src != NULL) { found_level = pcd->level(); break; }
      else { 
        CD_DEBUG("Not found %lx (%s) in level %u", tag, pcd->label(), pcd->level()); 
        char tmp[16];
        sprintf(tmp, "Restore Lv%u", pcd->level());
        pcd->entry_directory_.table_->PrintEntry(cdout, tmp, GetCDEntryStr);
      }
      parent_cd = CDPath::GetParentCD(pcd->level());
    }
  }
  CD_ASSERT_STR(src != NULL, "Failed to search %s %lx=%lx %s (%x) lv:%u, entry #:%ld\n", 
                     tag2str[search_tag].c_str(),
                     (is_ref)? ref_id : entry_id, search_tag,
                     (is_ref)? ref_name.c_str() : my_name.c_str(), preserve_mask, 
                     level(),
                     entry_directory_.table_->tablesize());
  assert(src);
  CD *ptr_cd = CDPath::GetCDLevel(found_level)->ptr_cd();
  assert(ptr_cd->level() == found_level);

//  PRINT_BOTH("  ** Restore %18s ** at %10s Lv%u %lu-%lu, (found at %10s Lv%u %lu-%lu) prvcnt:%lu==rstcnt:%lu\n", 
//             tag2str[search_tag].c_str(),
//             label_.c_str(), level(),   
//             phaseTree.current_->seq_begin_,
//             phaseTree.current_->seq_end_, 
//             ptr_cd->label_.c_str(), ptr_cd->level(), 
//             phaseNodeCache[ptr_cd->phase()]->seq_begin_,
//             phaseNodeCache[ptr_cd->phase()]->seq_end_,
//             preserve_count_, restore_count_);
  if( CHECK_PRV_TYPE(preserve_mask, kSerdes) ) {
    PackerSerializable *serializer = reinterpret_cast<PackerSerializable *>(data);
    // It is very important to pass entry_directory_ of CD level that has search_tag.
    PRINT_MPI("%p, %p, %p, %s, %s\n", ptr_cd->entry_directory_.table_, serializer, ptr_cd, ptr_cd->label_.c_str(), (is_ref)? ref_name.c_str() : my_name.c_str());
#if CD_DEBUG_ENABLED_INTERNAL
    serializer->Print();
#endif
    //if( CHECK_PRV_TYPE(preserve_mask, kIgnore) == false ) 
    {
      uint64_t restored_len = serializer->Deserialize(ptr_cd->entry_directory_, (is_ref)? ref_name.c_str() : my_name.c_str());
      if(restored_len == -1UL) {
        
        PRINT_MPI("Print Entries at %s (#%u)\n", ptr_cd->label_.c_str(), ptr_cd->level());
//        if(myTaskID == 0) {
//          ptr_cd->entry_directory_.table_->PrintEntry();
//        }
          CD_ASSERT_STR(0, "Not deserialized %s %u / %u\n", name_.c_str(), level(), ptr_cd->level());
      }
    }
//    CD_ASSERT_STR(restored_len == len_in_bytes, "restored len:%lu==%lu\n", restored_len, len_in_bytes);
//    if(prv_medium_ != kDRAM)
//      entry_directory_.data_->Flush();
  } else { // serdes ends
    // This will fetch from disk to memory
    // Potential benefit from prefetching app data from preserved data in
    // disk, overlapping reexecution of application.
    char tmp[128];
    int64_t seq_end = phaseNodeCache[phase()]->seq_end_;
    sprintf(tmp, "Restore Domain lv:%u %s seq:%ld phase:%u !=%ld(failed) %s", level(), label_.c_str(),  
        seq_end, phase(), cd::failed_seqID, IsFailed()? "FAIL" : "GOOD");
    src->Print(cdout, tmp, GetCDEntryStr);
//    if(myTaskID ==0) { 
//      printf("[%s, %s] It is not a serdes obj(%s,%s)\n", __func__, ptr_cd->label(), my_name.c_str(), ref_name.c_str());
//      entry_directory_.table_->PrintEntry();
//    }
//    CD_ASSERT_STR(src->src() == data, "%s src: %p==%p ",
//       (is_ref)? ref_name.c_str() : my_name.c_str(), src->src(), data);
    CD_ASSERT_STR(src->size() == len_in_bytes, "%s len: %lu==%lu ", 
        (is_ref)? ref_name.c_str() : my_name.c_str(), src->size(), len_in_bytes);
    assert(src->src() == data && src->size() == len_in_bytes);
#ifdef LULESH_DEBUG
    Internal befor;
    memcpy(&befor, data, sizeof(Internal));
    Internal *after = (Internal *)(data);
    CD_DEBUG("GetData(%p == %p, %lx=%lx, %lx); %s==%s %p==%p\n", 
        src->src(), data, len_in_bytes, sizeof(Internal), src->offset(), 
        this->label(), ptr_cd->label(), this, ptr_cd);
#endif
    ptr_cd->entry_directory_.data_->GetData(data /*src->src() FIXME*/, len_in_bytes, src->offset());
//    entry_directory_.data_->GetData((char *)&domain_obj, domain_entry.size(), domain_entry.offset());
#ifdef LULESH_DEBUG
//      char tmp[128];
//      int64_t seq_end = phaseNodeCache[phase()]->seq_end_;
//      sprintf(tmp, "Restore Domain lv:%u %s seq:%ld phase:%u !=%ld(failed) %s", level(), label_.c_str(),  
//          seq_end, phase(), cd::failed_seqID, IsFailed()? "FAIL" : "GOOD");
      befor.Print(*after, tmp, cdout);
#endif
    
    /*********************************************
     * Test for kRef case
     *********************************************/

    if( is_ref ) {
#if 0 // for now do not check this
      if(ref_name.empty() == false) {
        uint64_t my_id = cd_hash(my_name);
        uint32_t my_lv = INVALID_NUM32;
        CDEntry *dst = SearchEntry(my_id, my_lv);
        CD_ASSERT_STR(dst->src() == data, "dst: %p==%p ", dst->src(), data);
        CD_ASSERT_STR(dst->size() == len_in_bytes, "len: %lu==%lu ", dst->size(), len_in_bytes);
      }
#endif
    } else {
      CD_ASSERT_STR(src->src() == data, "%s src: %p==%p ",
         (is_ref)? ref_name.c_str() : my_name.c_str(), src->src(), data);
    }
#if 0
    CDEntry *pentry = entry_directory_.Restore(tag, (char *)data, len_in_bytes);//, (char *)data);i
    if(myTaskID == 0) {
      if(pentry == NULL) {
        printf("Not Found [%d %s]tag:%lu prv:%lu rst:%lu\n", 
            myTaskID, my_name.c_str(), tag, preserve_count_, restore_count_);
        assert(0);
      } else {
        printf("Restore [%d %s]tag:%lu prv:%lu rst:%lu\n", 
            myTaskID, my_name.c_str(), tag, preserve_count_, restore_count_);
      }
    }
#endif
//  if( CHECK_PRV_TYPE(preserve_mask, kSerdes) == false) {
//    packer::CDErrType pret = entry_directory_.Restore(tag);//, (char *)data);
//  } else {
//    // this read data chunk to some buffer spacem, which will be the buffer for
//    // packer object in deserializer. it will be deallocated after deserialization.
//    char *nested_obj = entry_directory_.Restore(tag);
//    PackerSerializable *deserializer = static_cast<PackerSerializable *>(data);
//    deserializer->Deserialize(nested_obj);
//  }
  }
  return ret;
}


/**********************************************************************************
 * CD::Restore(const string &entry_name)
 **********************************************************************************
 * This method requires to search both src and dst, then copy.
 **********************************************************************************/
CD::CDInternalErrT CD::Restore(const string &entry_name) {

  return kOK;
}
/**********************************************************************************
 * CD::RestoreAll()
 * Copy all the preserved data to application data.
 * There are two methods to restore data from preserved data.
 * One is to restore at Preserve(), which delays each restoration 
 * at the very last moment.
 * The other is to restore all the application data 
 * right before re-executing CD. 
 *
 * This restoration method is not supported, yet. (01/29/2018)
 *
 * TODO : Restoration scheduling
 * Begin point and Preserve() indicates the possible time window to
 * restore application data from preservation medium. Depending on
 * the window size, CD runtime might be able to asynchronously
 * copy each data from preservation space to application space. 
 * This might reduce/hide restoration time.
 **********************************************************************************/
CDErrT CD::RestoreAll()
{
  const int64_t current_phase = cd_id_.cd_name_.phase_;
  int64_t seq_begin = phaseNodeCache[current_phase]->seq_begin_;
  int64_t seq_end = phaseNodeCache[current_phase]->seq_end_;
  int64_t seq_acc_rb = phaseNodeCache[current_phase]->seq_acc_rb_;
  PRINT_BOTH("RestoreAll: %s lv:%u, %s, %s, (%ld, %u), bitvec:%lx, (curr:%ld,fphase:%ld), %ld~%ld->%ld\n", 
      IsFailed()? "REEX" : "EXEC",
      level(), name_.c_str(), label_.c_str(), 
      seq_acc_rb, cd_id_.sequential_id_, sys_detect_bit_vector_, 
      current_phase, failed_phase, seq_begin, seq_end, cd::failed_seqID);
  restore_count_ = 0;
  char tmp[64];
  sprintf(tmp, "RollbackAll %s Lv%u %ld->%ld", label_.c_str(), level(), seq_end, cd::failed_seqID);
#if CD_DEBUG_ENABLED
  if (myTaskID == 0)
    entry_directory_.table_->PrintEntry(stdout, tmp, GetCDEntryStr);
  entry_directory_.table_->PrintEntry(cdout, tmp, GetCDEntryStr);
#endif
  TableStore<CDEntry> obj_entries;
  uint64_t entry_num = entry_directory_.table_->FindWithAttr(Attr::karray, 0, &obj_entries, true);
  sprintf(tmp, "Rollback %lu %s Lv%u %ld->%ld", entry_num, label_.c_str(), level(), seq_end, cd::failed_seqID);
  obj_entries.PrintEntry(cdout, tmp, GetCDEntryStr);
  for(uint32_t i=0; i<entry_num; i++) {
    CDEntry domain_entry = obj_entries[i];
    Internal domain_obj;
    domain_entry.Print(cdout, tmp, GetCDEntryStr);
    if (myTaskID == 0)
      domain_entry.Print(stdout, tmp, GetCDEntryStr);
    entry_directory_.data_->GetData((char *)&domain_obj, domain_entry.size(), domain_entry.offset());
    domain_obj.Print(tmp, cdout);
  }

#if 0
  entry = entry_directory_.table_->FindReverse(tag, attr);
  
  const bool is_ref = CHECK_PRV_TYPE(preserve_mask, kRef);
  uint32_t found_level = INVALID_NUM32;
  uint64_t ref_id = 0; //cd_hash(ref_name);
  uint64_t entry_id = cd_hash(my_name);
  if(ref_name.empty()) { 
    ref_id = entry_id;
  } else {
    ref_id = cd_hash(ref_name);
  }
  uint64_t search_tag = (is_ref)? ref_id : entry_id;
  
  // Search the preservation entry which is not krefer.
  // It tries to search until it finds the entry without krefer.
  CDEntry *src = SearchEntry(search_tag, found_level, Attr::koutput);
  if(src == NULL) {
    CDHandle *parent_cd = GetCurrentCD();
    while( parent_cd != NULL ) {
      CD *ptr_cd = parent_cd->ptr_cd();
      if(myTaskID == 0) {
        char tmp[16];
        sprintf(tmp, "Restore %u", ptr_cd->level());
        ptr_cd->entry_directory_.table_->PrintEntry(tmp, GetCDEntryStr);
      }
      uint64_t tag = search_tag;
      src = ptr_cd->entry_directory_.table_->FindReverse(tag, Attr::koutput);
      parent_cd = CDPath::GetParentCD(ptr_cd->level());
    }
  }
  CD_ASSERT_STR(src != NULL, "Failed to search %s %lx=%lx %s (%x) lv:%u, entry #:%ld\n", 
                     tag2str[search_tag].c_str(),
                     (is_ref)? ref_id : entry_id, search_tag,
                     (is_ref)? ref_name.c_str() : my_name.c_str(), preserve_mask, 
                     level(),
                     entry_directory_.table_->tablesize());
  assert(src);
  CD *ptr_cd = CDPath::GetCDLevel(found_level)->ptr_cd();
  if( CHECK_PRV_TYPE(preserve_mask, kSerdes) ) {
    PackerSerializable *serializer = reinterpret_cast<PackerSerializable *>(data);
    // It is very important to pass entry_directory_ of CD level that has search_tag.
    PRINT_MPI("%p, %p, %p, %s, %s\n", ptr_cd->entry_directory_.table_, serializer, ptr_cd, ptr_cd->label_.c_str(), (is_ref)? ref_name.c_str() : my_name.c_str());
#if CD_DEBUG_ENABLED_INTERNAL
    serializer->Print();
#endif
    //if( CHECK_PRV_TYPE(preserve_mask, kIgnore) == false ) 
    {
      uint64_t restored_len = serializer->Deserialize(ptr_cd->entry_directory_, (is_ref)? ref_name.c_str() : my_name.c_str());
      if(restored_len == -1UL) {
        
        PRINT_MPI("Print Entries at %s (#%u)\n", ptr_cd->label_.c_str(), ptr_cd->level());
          CD_ASSERT_STR(0, "Not deserialized %s %u / %u\n", name_.c_str(), level(), ptr_cd->level());
      }
    }
  } else {
    // This will fetch from disk to memory
    // Potential benefit from prefetching app data from preserved data in
    // disk, overlapping reexecution of application.
    CD_ASSERT_STR(src->size() == len_in_bytes, "%s len: %lu==%lu ", 
        (is_ref)? ref_name.c_str() : my_name.c_str(), src->size(), len_in_bytes);
    ptr_cd->entry_directory_.data_->GetData(src->src(), len_in_bytes, src->offset());
    
  }
#endif
  return CDErrT::kOK;
}



/* CD::Detect()
 * (1) Call all the user-defined error checking functions.
 *   Each error checking function should call its error handling function.(mostly restore() and reexec())  
 * (2) 
 *
 */
CD::CDInternalErrT CD::Detect(uint32_t &rollback_point)
{
  CD::CDInternalErrT internal_err = kOK;
  rollback_point = -1;
  // STUB

  return internal_err;
}

void CD::Recover(uint32_t level, bool collective)
//void CD::Recover()
{
  PRINT_MPI("entry dir size:%lu\n", entry_directory_.table_->tablesize());
  const uint32_t rollback_point = *rollback_point_;
  const int64_t curr_phase = phase();
  const int64_t curr_seqID = phaseNodeCache[curr_phase]->seq_end_;
  const int64_t curr_begID = phaseNodeCache[curr_phase]->seq_begin_;
//  const int64_t leaf_phase = CDPath::GetCDLevel(level)->ptr_cd_->phase();
//  const int64_t leaf_seqID = phaseNodeCache[leaf_phase]->seq_end_;
//  const int64_t leaf_begID = phaseNodeCache[leaf_phase]->seq_begin_;
  if(rollback_point < level) {
    CD_DEBUG(">>> Escalation %u->%u (%s), fphase:%ld==%ld, seqID:%ld==%ld (beg:%ld) <<<\n", 
        level, rollback_point, label_.c_str(), failed_phase, curr_phase,  
        failed_seqID, curr_seqID, curr_begID);
  }
  recoverObj_->Recover(this); 
} 


CD::CDInternalErrT CD::Assert(bool test)
{
  //printf("[%s] assert %d\n", __func__, test);

  CDInternalErrT internal_err = kOK;


#if _MPI_VER
  CDHandle *cdh = CDPath::GetCurrentCD();
  if(test==false) {

    if(cdh->level() == level()) { // leaf CD
      if(task_size() > 1) {
        // If current level's task_size is larger than 1,
        // upper-level task_size is always larger than 1.
        if(IsHead()) {
          SetRollbackPoint(level(), false);
        } else { 
          SetMailBox(kErrorOccurred);
        }
      } else { // a single task in a CD.
        //printf("[%s] set rollback point\n", __func__);
        SetRollbackPoint(level(), false);
      }
    }
    else if(cdh->level() > level()) { // escalation
      if(task_size() > 1) {
        SetMailBox(kErrorOccurred);
        SetRollbackPoint(level(), false);
      } else { // a single task in a CD.
        SetRollbackPoint(level(), false);
      }
    }
    else {
      ERROR_MESSAGE("(leaf) %u < %u (failed level)\n", cdh->level(), level());
    }
  }
  CheckMailBox();
#else // CD_MPI_ENABLED ends
  if(test == false) {
    SetRollbackPoint(level(), false);
  }
#endif

  return internal_err;
}

//void CD::SetRecoverFlag(void) {
//  // Before Assert(false), some other tasks might raise error flag in this task,
//  // and that can be less than this point, which means escalation request.
//  // rollback_point was set to a number less than current task's level,
//  // Then do not set it to current task's level, because it needs to be escalated.
//  if(*rollback_point_ > level()) {
//    *rollback_point_ = level();
//  }
//  else {
//    need_escalation = true;
//  }
//}

CD::CDInternalErrT CD::RegisterDetection(uint32_t system_name_mask, 
                                     uint32_t system_loc_mask)
{
  // STUB
  return CDInternalErrT::kOK;
} 

CD::CDInternalErrT CD::RegisterRecovery(uint32_t error_name_mask, 
                                    uint32_t error_loc_mask, 
                                    RecoverObject *recover_object)
{
  if(recoverObj_ != NULL) delete recoverObj_;
  recoverObj_ = recover_object;
  return CDInternalErrT::kOK;
}

CD::CDInternalErrT CD::RegisterRecovery(uint32_t error_name_mask, 
                                    uint32_t error_loc_mask, 
                                    CDErrT(*recovery_func)(std::vector< SysErrT > errors))
{
  // STUB
  return CDInternalErrT::kOK;
}


CDErrT CD::Reexecute(void)
{

  return InternalReexecute();
}

CDErrT HeadCD::Reexecute(void)
{

  return InternalReexecute();
}

CDErrT CD::InternalReexecute(void)
{
  // SZ: FIXME: this is not safe because programmers may have their own RecoverObj
  //            we need to change the cd_exec_mode_ and comm_log_mode_ outside this function.
  // KL: I think it should be here, because recovery action might not be reexecution, but something else.

  PRINT_BOTH("    (( NOW ROLLBACK )) Lv%u %s reexecuted : %d, reexecution # : %d\n", level(), label(), reexecuted_, num_reexecution_);
  // This is very very tricky!!
  // non-head task will get the rollback_level_ of head task,
  // but if head already finished GetCDToRecover routine, then
  // set this rollback_point_ before the other task see it,
  // they will observe the wrong flag.
  // To avoid this situation, it is necessary to put synch before setting the flag.
  //
  // It is also possible case that current task sets reexec from upper level,
  // but actually it was reexecuting some lower level CDs. 
  // while executing lower-level reexecution,
  // rollback_point was set to upper level, 
  // which means escalation request from another task. 
  // Therefore, this flag should be carefully reset to exec mode.
  // level == rollback_point enough condition for resetting to exec mode,
  // because nobody overwrited these flags set by current flag.
  // (In other word, nobody requested escalation requests)
  // This current task is performing reexecution corresponding to these flag set by itself.
//  if(task_size() > 1) {
//    MPI_Win_fence(0, mailbox_);
//    //PMPI_Win_lock(MPI_LOCK_EXCLUSIVE, GetRootCD()->task_in_color(), 0, rollbackWindow_);
//    need_escalation = false;
//    *rollback_point_ = INVALID_ROLLBACK_POINT;        
//    //PMPI_Win_unlock(GetRootCD()->task_in_color(), rollbackWindow_);
//    MPI_Win_fence(0, mailbox_);
//  } else {
//    need_escalation = false;
//    *rollback_point_ = INVALID_ROLLBACK_POINT;        
//  }


  // This is a flag to differentiate the case 
  // beween recreated begin and escalated begin.
  // The very initial "Begin" (rollback point) due to escalation 
  // may be considered specially.
  // (ex. calculating sync time)
  just_reexecuted = true;
  num_reexecution_++;

#if comm_log
  // SZ
  //// change the comm_log_mode_ into CommLog class
  //comm_log_mode_ = kReplayLog;  
  if (MASK_CDTYPE(cd_type_) == kRelaxed) {
    comm_log_ptr_->ReInit();
    SetCDLoggingMode(kRelaxedCDRead);
  }
  
  //SZ: reset to child_seq_id_ = 0 
  LOG_DEBUG("Reset child_seq_id_ to 0 at the point of re-execution\n");
  child_seq_id_ = 0;
#endif

#if CD_LIBC_LOG_ENABLED
  //GONG
  if(libc_log_ptr_!=NULL) {
    libc_log_ptr_->ReInit();
    LOG_DEBUG("reset log_table_reexec_pos_\n");
    //  libc_log_ptr_->Print();
  }
#endif

  //TODO We need to make sure that all children has stopped before re-executing this CD.
  Stop();

  //printf("[%s]Rollback!\n", __func__);
//  printf("######### level : %d (rollback_point: %d) (%s)\n", level(), *rollback_point_, name_.c_str());

  // CDPrologue(); in CDHandle::Complete() should be paired with CDEpilouge() here.
  CDEpilogue();
  //TODO We need to consider collective re-start. 
  if(ctxt_prv_mode_ == kExcludeStack) {

    CD_DEBUG("longjmp\n");
   
//    CD_DEBUG("\n\nlongjmp \t %d at level #%u\n", (tmp<<jmp_buffer_).rdbuf(), level());

    longjmp(jmp_buffer_, jmp_val_);
  }
  else if (ctxt_prv_mode_ == kIncludeStack) {
    CD_DEBUG("setcontext at level : %d (rollback_point: %d) (%s)\n", level(), *rollback_point_, name_.c_str());
    setcontext(&ctxt_); 
  }

  return CDErrT::kOK;
}






// Let's say re-execution is being performed and thus all the children should be stopped, 
// need to provide a way to stop currently running task and then re-execute from some point. 
CDErrT CD::Stop(CDHandle *cdh)
{
  //TODO Stop current CD.... here how? what needs to be done? 
  // may be wait for others to complete? wait for an event?
  //if current thread (which stop function is running) and the real cd's thread id is different, 
  //that means we need to stop real cd's thread..
  //otherwise what should we do here? nothing?

  // Maybe blocking Recv here? or PMPI_Wait()?
  
  return CDErrT::kOK;
}

CDErrT CD::Resume(void)
{
  return CDErrT::kOK;
}

CDErrT CD::AddChild(CDHandle *cd_child) 
{ 
  // Do nothing?
  if(cd_child->IsHead()) {
    //Send it to Head
  }

  return CDErrT::kOK;  
}

CDErrT CD::RemoveChild(CDHandle *cd_child) 
{
  // Do nothing?
  return CDErrT::kOK;
}


//CD::CDInternalErrT CD::CreateInternalMemory(CD *cd_ptr, const CDID& new_cd_id)
//{
//  int task_count = new_cd_id.task_count();
//  cddbg << "in CD::Create Internal Memory"<<endl;
//  if(task_count > 1) {
//    for(int i=0; i<task_count; ++i) {
//    
//      PMPI_Win_create(NULL, 0, 1,
//                     PMPI_INFO_NULL, new_cd_id.color(), &((cd_ptr->mailbox_)[i]));
//  
//    }
//  }
//  else {
//    cddbg << "CD::mpi win create for "<< task_count << " mailboxes"<<endl;
//    PMPI_Win_create(NULL, 0, 1,
//                   PMPI_INFO_NULL, new_cd_id.color(), cd_ptr->mailbox_);
//    
//  }
//
//  return CD::CDInternalErrT::kOK;
//}
//
//
//CD::CDInternalErrT CD::DestroyInternalMemory(CD *cd_ptr)
//{
//
//  cddbg << "in CD::Destroy Internal Memory"<<endl;
//  int task_count = cd_id_.task_count();
//  if(task_count > 1) {
//    cddbg << "mpi win free for "<< task_count << " mailboxes"<<endl;
//    for(int i=0; i<task_count; ++i) {
//      cddbg << i << endl;
//      PMPI_Win_free(&(cd_ptr->mailbox_[i]));
//    }
//  }
//  else {
//    cddbg << "mpi win free for one mailbox"<<endl;
//    PMPI_Win_free(cd_ptr->mailbox_);
//  }
//
//  return CD::CDInternalErrT::kOK;
//}

//CD::CDInternalErrT HeadCD::CreateInternalMemory(HeadCD *cd_ptr, const CDID& new_cd_id)
//{
//  cddbg << "HeadCD create internal memory " << endl;
//  int task_count = new_cd_id.task_count();
//#if _MPI_VER
////  if(new_cd_id.color() == PMPI_COMM_WORLD) {
////    cddbg << "\n\nthis is root! " << task_count << "\n\n"<<endl;
////    PMPI_Alloc_mem(sizeof(CDFlagT)*task_count, 
////                  PMPI_INFO_NULL, &event_flag_);
////    mailbox_ = new CDMailBoxT[task_count];
////    for(int i=0; i<task_count; ++i) {
////      PMPI_Win_create(&((event_flag_)[i]), 1, sizeof(CDFlagT),
////                     PMPI_INFO_NULL, PMPI_COMM_WORLD, &((mailbox_)[i]));
////    }
////
////
////    exit(-1);
////    return CD::CDInternalErrT::kOK;
////  }
//
//  PMPI_Alloc_mem(sizeof(CDFlagT)*task_count, 
//                PMPI_INFO_NULL, &(cd_ptr->event_flag_));
//
//  if(task_count > 1) {
//
//    cddbg << "HeadCD mpi win create for "<< task_count << " mailboxes"<<endl;
//    mailbox_ = new CDMailBoxT[task_count];
//    for(int i=0; i<task_count; ++i) {
//      PMPI_Win_create(&((cd_ptr->event_flag_)[i]), 1, sizeof(CDFlagT),
//                     PMPI_INFO_NULL, new_cd_id.color(), &((cd_ptr->mailbox_)[i]));
//    }
//
////      PMPI_Win_create(event_flag_, task_count, sizeof(CDFlagT),
////                     PMPI_INFO_NULL, new_cd_id.color(), mailbox_);
//
//  }
//  else {
//    cddbg << "HeadCD mpi win create for "<< task_count << " mailboxes"<<endl;
//    PMPI_Win_create(cd_ptr->event_flag_, task_count, sizeof(CDFlagT),
//                   PMPI_INFO_NULL, new_cd_id.color(), cd_ptr->mailbox_);
//    
//  }
//
////  PMPI_Win_allocate(task_count*sizeof(CDFlagT), sizeof(CDFlagT), PMPI_INFO_NULL, new_cd_id.color(), &event_flag_, &mailbox_);
//  //cddbgBreak();
//#endif
//  return CD::CDInternalErrT::kOK;
//}
//
//CD::CDInternalErrT HeadCD::DestroyInternalMemory(HeadCD *cd_ptr)
//{
//#if _MPI_VER
//  cddbg << "in HeadCD::Destroy"<<endl;
//  int task_count = cd_id_.task_count();
//  if(task_count > 1) {
//    cddbg << "HeadCD mpi win free for "<< task_count << " mailboxes"<<endl;
//    for(int i=0; i<task_count; ++i) {
//      cddbg << i << endl;
//      PMPI_Win_free(&(cd_ptr->mailbox_[i]));
//    }
//  }
//  else {
//    cddbg << "HeadCD mpi win free for one mailbox"<<endl;
//    PMPI_Win_free(cd_ptr->mailbox_);
//  }
//  PMPI_Free_mem(cd_ptr->event_flag_);
//
//
//
////  PMPI_Win_free(&mailbox_);
//#endif
//  return CD::CDInternalErrT::kOK;
//}






HeadCD::~HeadCD()
{
//  DestroyInternalMemory();
//    PMPI_Free_mem(event_flag_);
}

CDHandle *HeadCD::Create(CDHandle *parent, 
                     const char *name, 
                     const CDID& child_cd_id, 
                     CDType cd_type, 
                     uint64_t sys_bit_vector, 
                     CD::CDInternalErrT *cd_internal_err)
{
//  printf("[%s]cd size:%zu\n", __func__,sizeof(*this));

  // CD runtime system are not able to be aware of how many children the current HeadCD task will have.
  // So it does not make sense to create mailboxes for children CD's HeadCD tasks at the HeadCD creation time.
  // It does make sense to create those mailboxes when it create children CDs.
  //
//  HeadCD *ptr_headcd = dynamic_cast<HeadCD*>(ptr_cd_);


//  int family_mailbox_count = child_cd_id.sibling_count() + 1;
//  for(int i=0; i<num_children; ++i) {
//    PMPI_Win_create(&((ptr_headcd->family_event_flag_)[i]), 1, sizeof(CDFlagT),
//                   PMPI_INFO_NULL, new_cd_id.color(), &((ptr_headcd->family_mailbox_)[i]));
//  }

  // NOTE
  // Order b/w flush and creation is important!!!!
  // Otherwise, written_len_ for child's entry_directory_ is set wrong!!
  if(prv_medium_ != kDRAM) {
    entry_directory_.AppendTable();
    entry_directory_.data_->Flush();
//    child_entry_dir.data_->PadAndInit(entry_directory_.data_->next_head());
  }

  /// Create CD object with new CDID
  CDHandle *new_cd_handle = NULL;
  *cd_internal_err = InternalCreate(parent, name, child_cd_id, cd_type, sys_bit_vector, &new_cd_handle);
  assert(new_cd_handle != NULL);

  // FIXME: flush before child execution
//  if(prv_medium_ != kDRAM) {
//    entry_directory_.AppendTable();
//    entry_directory_.data_->Flush();
//  }
//  packer::CDPacker &child_entry_dir = new_cd_handle->ptr_cd_->entry_directory_;
//  if(prv_medium_ != kDRAM) {
//    entry_directory_.AppendTable();
//    entry_directory_.data_->Flush();
////    child_entry_dir.data_->PadAndInit(entry_directory_.data_->next_head());
//  }

  this->AddChild(new_cd_handle);

  return new_cd_handle;
}


CDErrT HeadCD::Destroy(bool collective, bool need_destroy)
{
  CD_DEBUG("HeadCD::Destroy\n");
  CDErrT err=CDErrT::kOK;

  InternalDestroy(collective, need_destroy);
  FreeCommResource();

  return err;
}

CDErrT HeadCD::Stop(CDHandle *cdh)
{

//  if(IsLocalObject()){
//    if( !cd_children_.empty() ) {
//      // Stop children CDs
//      int res;
//      std::list<CD>::iterator itstr = cd_children_.begin(); 
//      std::list<CD>::iterator itend = cd_children_.end();
//      while(1) {
//        if(itstr < itend) {
//          res += (*it).Stop(cdh);
//        }
//        if(res == cd_children_.size()) {
//          break;
//        }
//      }
//    
//    }
//    else {  // terminate
//      return 1;
//    }
//  }
//  else {
//    // return RemoteStop(cdh);
//  }

  return CDErrT::kOK;
}


CDErrT HeadCD::Resume(void)
{
  //FIXME: return value needs to be fixed 

//  for( std::list<CD>::iterator it = cd_children_.begin(), it_end = cd_children_.end(); 
//       it != it_end ; ++it) {
//    (*it).Resume();
//  }

  return CDErrT::kOK;
}


CDErrT HeadCD::AddChild(CDHandle *cd_child) 
{
  if(cd_child->IsHead()) {
    CD_DEBUG("It is not desirable to let the same task be head twice!\n");
//    cd_children_.push_back(cd_child->ptr_cd()->cd_id().task());

//    GatherChildHead();
  }

  return CDErrT::kOK;  
}


//FIXME 11112014
//void CD::GatherChildHead(const CDID& child_cd_id)
//{
//#if _MPI_VER
//
//  // Gather heads of children CDs
//  int send_buf;
//  int send_count;
//  int recv_buf[cd_id().sibling_count()];
//  int recv_count;
//
//  PMPI_Send(, send_count, PMPI_INTEGER, recv_buf, recv_count, PMPI_INTEGER, cd_id().node_id().head(), cd_id().color());
//
//#endif
//}
//
//void Head::GatherChildHead(void)
//{
//#if _MPI_VER
//
//  // Gather heads of children CDs
//  int recv_buf[cd_id().sibling_count()];
//  PMPI_Status status;
//  for(int i=1; i<cd_id().sibling_count(); ++i) {
//    PMPI_Recv(&(recv_buf[i]), 1, PMPI_INTEGER, PMPI_ANY_SOURCE, 0, cd_id().color(), &status);
//  }
//
//#endif
//}

CDErrT HeadCD::RemoveChild(CDHandle *cd_child) 
{
  //FIXME Not optimized operation. 
  // Search might be slow, perhaps change this to different data structure. 
  // But then insert operation will be slower.
  cd_children_.remove(cd_child);

  return CDErrT::kOK;
}


CDHandle *HeadCD::cd_parent(void)
{
  return cd_parent_;
}

void HeadCD::set_cd_parent(CDHandle *cd_parent)
{
  cd_parent_ = cd_parent;
}


// If it is found locally, cast type to RemoteCDEntry
CDEntry *HeadCD::GetEntry(ENTRY_TAG_T entry_name, uint16_t attr) 
{
  CD_DEBUG("\nCD::GetEntry : %u - %s\n", entry_name, tag2str[entry_name].c_str());
  CDEntry *entry = InternalGetEntry(entry_name, attr);
  if(entry != NULL) {
    entry = remote_entry_table_.FindReverse(entry_name, attr);
  }
  return entry;
}

CDEntry *CD::GetEntry(ENTRY_TAG_T entry_name, uint16_t attr) 
{
  CD_DEBUG("\nCD::InternalGetEntry : %u - %s\n", entry_name, tag2str[entry_name].c_str());
  return InternalGetEntry(entry_name, attr);
}

CDEntry *CD::InternalGetEntry(ENTRY_TAG_T entry_name, uint16_t attr)
{
  CDEntry *entry = NULL;
  auto jt = remote_entry_directory_map_.find(entry_name);
  if(jt != remote_entry_directory_map_.end()) {
    entry = jt->second;
  } else {
    uint64_t tag = entry_name;
    entry = entry_directory_.table_->FindReverse(tag, attr);
  }
  
  if(entry != NULL) {
    // if krefer is 1, not found
    entry = (entry->size_.Check(Attr::krefer) == 0)? entry : NULL;
    PRINT_MPI("Found %s (%lx) %s\n", tag2str[entry_name].c_str(), entry_name, (entry != NULL)? "Copy":"But Ref");
  } else {
    CD_DEBUG("[InternalGetEntry Failed] There is no entry for reference of %s at level #%u\n", 
        tag2str[entry_name].c_str(), level());
  }
  return entry; 
}

void CD::DeleteEntryDirectory(void)
{
//  if(myTaskID == 0) printf("Complete : %s\n", label_.c_str());
  CD_DEBUG("Delete entry directory!\n");
  preserve_count_ = 0;
  restore_count_ = 0;
  entry_directory_.Clear(true); // reuse true
  remote_entry_directory_map_.clear();
}

void HeadCD::DeleteEntryDirectory(void)
{
//  if(myTaskID == 0) printf("Complete : %s\n", label_.c_str());
  CD_DEBUG("Delete entry directory!\n");
  preserve_count_ = 0;
  restore_count_ = 0;
  entry_directory_.Clear(true);
  remote_entry_directory_map_.clear();
  remote_entry_table_.Free(true);
}

#if CD_LIBC_LOG_ENABLED
//GONG
CommLogErrT CD::CommLogCheckAlloc_libc(unsigned long length)
{
  LogPrologue();
  CommLogErrT ret = libc_log_ptr_->CheckChildLogAlloc(length);
  LogEpilogue();
  return ret;
}

//GONG
CommLogMode CD::GetCommLogMode_libc()
{
  return libc_log_ptr_->GetCommLogMode();
}

//GONG
bool CD::IsNewLogGenerated_libc()
{
  return libc_log_ptr_->IsNewLogGenerated_();
}

#endif




#if comm_log
//SZ
CommLogErrT CD::CommLogCheckAlloc(unsigned long length)
{
  LogPrologue();
  CommLogErrT ret = comm_log_ptr_->CheckChildLogAlloc(length);
  LogEpilogue();
  return ret;
}


//SZ 
bool CD::IsParentLocal()
{
  //FIXME: for now assuming cd_parent_ is always local
  //       need to implement inside CDID object to test if parent is local, such as using process_id_
  return 1;
}

//SZ
CDHandle *CD::GetParentHandle()
{
  return CDPath::GetParentCD(level());
}



// KL
// When task has matching MPI calls such as the pair of MPI_Irecv and MPI_Wait,
// it is possible to call MPI_Irecv twice and MPI_Wait once.
// Possible situation is like below.
// Task 1 called MPI_Irecv and got escalation event before reaching MPI_Wait. 
// It escalates to beginning of CD and invokes MPI_Irecv again. 
// Then it calls MPI_Wait. 
// The problem is that strict CD does not log MPI calls 
// but regenerate messages in reexecution path.
// Possible solution is invalidating the original MPI_Irecv with MPI_Cancel 
// inside CD runtime and calls new MPI_Irecv in reexecution path.
CommLogErrT CD::InvalidateIncompleteLogs(void)
{
  LogPrologue();
  //printf("### [%s] %s at level #%u\n", __func__, label_.c_str(), level());
  //if(incomplete_log_.size()!=0) 
  if (0)
  {
    PRINT_BOTH("### [%s] incomplete logs: %zu at level #%u\n", label_.c_str(), incomplete_log_.size(), level());
//    if(myTaskID ==7) printf("### [%s] %s Incomplete log size: %lu at level #%u\n", __func__, label_.c_str(), incomplete_log_.size(), level());
  }
// FIXME: 20180212 
// Should take a look at the below code for invalidating comms.
//
// http://mpi-forum.org/docs/mpi-1.1/mpi-11-html/node50.html
// http://mpi-forum.org/docs/mpi-1.1/mpi-11-html/node47.html
//
// FIXME: 20180921
// Also consider to use MPI_Request_get_status.
// Unlike MPI_Test, MPI_Request_get_status does not deallocate or deactivate the
// request. A call to one of the test/wait routines or MPI_Request_free should
// be made to release the request object.
// http://mpi.deino.net/mpi_functions/MPI_Request_get_status.html
//
//
#if 1
  for(auto it=incomplete_log_.begin(); it!=incomplete_log_.end();) {
    bool handled = true;
    char err_msg[256];
    //printf("[%d] Trying to test %p (#:%zu)...", myTaskID, it->flag_, incomplete_log_.size());
    sprintf(err_msg, "[%d] Invalidate ptr:0x%lx (#:%zu) Test...", myTaskID, it->flag_, incomplete_log_.size());
    std::string err_str(err_msg);
    //PRINT_BOTH("[%d] Invalidate ptr:0x%lx (#:%zu) Test...", myTaskID, it->flag_, incomplete_log_.size());
    MPI_Status status;
//    int is_probed = -1;
//    MPI_Iprobe(it->taskID_, it->tag_, it->comm_, &is_probed, &status);
//    if(is_probed < 1) 
    MPI_Request &t_req = *(MPI_Request *)(it->flag_);
    int done = -1;
#if 1
    if(t_req != MPI_REQUEST_NULL) {
      //PRINT_BOTH("Failed...Test...");
      PMPI_Test(&t_req, &done, &status);
      if(done > 0) {
        err_str.append("SUCCESS");
        //PRINT_BOTH("SUCCESS\n");
      } else {
#if 0
        err_str.append("FAILED >>> Try Wait...");
        PMPI_Wait(&t_req, &status);
        err_str.append("SUCCESS");
#else
        err_str.append("FAILED");
        handled = false;
/*
        err_str.append("FAILED >>> Try Cancel...");
        int is_cancelled = -1;
        PMPI_Cancel(&t_req);
        PMPI_Test_cancelled(&status, &is_cancelled);
        if(is_cancelled <= 0) {
          err_str.append("FAILED TO CANCEL");
          handled = false;
        } else {
          err_str.append("SUCCESS");
        }
*/
#endif
      } 
    } else { // MPI_REQUEST_NULL
      err_str.append("DELETE NULL REQ");
      //PRINT_BOTH("DELETE NULL REQ\n");
    }
    PRINT_BOTH("%s\n", err_str.c_str());
    if (handled)
      it = incomplete_log_.erase(it);
    else
      ++it;
#else
      PRINT_BOTH("SUCCESS\n");
      PRINT_BOTH("Probed?\n");
      assert(0);
      int done = -1;
      PMPI_Test(&t_req, &done, &status);
      it = incomplete_log_.erase(it);
      assert(done);
#endif
  }
#else
#endif
  // blocking until all incomplete logs are handled
//  PRINT_BOTH("Testing %zu\n", incomplete_log_.size());
  uint64_t iters = 0;
  while(incomplete_log_.size() > 0) {
    for (auto it=incomplete_log_.begin(); it!=incomplete_log_.end(); ) {
      MPI_Request &t_req = *(MPI_Request *)(it->flag_);
      MPI_Status status;
      int flag = -1;
      PMPI_Test(&t_req, &flag, &status);
      if (flag > 0 || t_req == MPI_REQUEST_NULL) it = incomplete_log_.erase(it);
      else ++it;
    }
    if (iters++ > 1000000) break;
  }
//  PRINT_BOTH("Canceling %zu\n", incomplete_log_.size());
  iters = 0;
  while(incomplete_log_.size() > 0) {
    for (auto it=incomplete_log_.begin(); it!=incomplete_log_.end(); ) {
      MPI_Request &t_req = *(MPI_Request *)(it->flag_);
      MPI_Status status;
      int is_cancelled = -1;
      PMPI_Cancel(&t_req);
      PMPI_Test_cancelled(&status, &is_cancelled);
      if(is_cancelled > 0 || t_req == MPI_REQUEST_NULL) it = incomplete_log_.erase(it);
      else ++it;
    }
    if (iters++ > 1000000) break;
  }
  for (auto it=incomplete_log_.begin(); it!=incomplete_log_.end(); ) {
    it = incomplete_log_.erase(it);
  }
  // PRINT_BOTH("Done\n");
  LogEpilogue();
  return kCommLogOK;
}

// KL
// This function call is for resolving the above problem in relaxed CD case.
// CD could just escalate in the case that there are some incomplete logs in failure.
// However, it can be resolved by letting the corresponding message
// be delivered, but not completed in the process of recovery action.
// Before longjmp to the beginning of CD, CD runtime can keep probing asynchronously 
// all the incomplete logs happened in the current CD until all of them are probed.
// Then, it reexecute from the beginning. This mechanism assumed that every incomplete
// logs will be eventually resolved, which should be correct.
// Otherwise, the normal app semantic will be also problematic because it will keep waiting
// at MPI_Wait in the case of MPI_Irecv in application side.
CommLogErrT CD::ProbeIncompleteLogs(void)
{
  LogPrologue();
  //printf("### [%s] %s at level #%u\n", __func__, label_.c_str(), level());
  if(incomplete_log_.size()!=0) {
    CD_DEBUG("### [%s] %s Incomplete log size: %lu at level #%u\n", __func__, label_.c_str(), incomplete_log_.size(), level());
//    printf("### [%s] %s Incomplete log size: %lu at level #%u\n", __func__, label_.c_str(), incomplete_log_.size(), level());
  }
#if _MPI_VER
  const size_t num_log = incomplete_log_.size();
  MPI_Status incompl_log_stat[num_log];

  uint32_t idx=0;
  for(auto it=incomplete_log_.begin(); it!=incomplete_log_.end(); ++it) {
    // It will be blocking until each incomplete msg is resolved.
    PMPI_Probe(it->taskID_, it->tag_, it->comm_,  &(incompl_log_stat[idx]));
    idx++;
  }
#endif
  LogEpilogue();
  return kCommLogOK;
}

bool CD::IsSend(MsgFlagT flag)
{
  bool is_send = false;
  auto it = incomplete_log_.find(flag);
  if(it != incomplete_log_.end()) {
    is_send = (it->isrecv_ == false);
  }
  
  return is_send;
}

bool CD::DeleteIncompleteLog(MsgFlagT flag)
{
  bool deleted = false;
//  for(auto it=incomplete_log_.begin(); it!=incomplete_log_.end(); ++it) {
//    auto jt = it;
//    if(it->flag_ == flag) {
//      incomplete_log_.erase(jt);
//      deleted = true;
//    }
//  }
  //printf("%s:*request=%d\n", __func__, flag);
  auto it = incomplete_log_.find(flag);
  if(it != incomplete_log_.end()) {
    incomplete_log_.erase(it);
    deleted = true;
    CD_DEBUG("FIND flag 0x%lx, incomplete logs:%zu\n", flag, incomplete_log_.size());
  }
  
  return deleted;
}


//SZ
CommLogErrT CD::ProbeAndLogData(MsgFlagT flag)
{
  LogPrologue();
  // look for the entry in incomplete_log_
  int found = 0;
  std::vector<IncompleteLogEntry>::iterator it;
  CD *tmp_cd = this;
  LOG_DEBUG("size of incomplete_log_=%ld\n",incomplete_log_.size());
  for (it=incomplete_log_.begin(); it!=incomplete_log_.end(); ++it) {
    LOG_DEBUG("it->flag_=%ld, and flag=%ld\n", it->flag_, flag);
    if (it->flag_ == flag) {
      found = 1;
      LOG_DEBUG("Found the entry in incomplete_log_ in current CD\n");
      break;
    }
  }

  if (found == 0) {
    // recursively go up to search parent's incomplete_log_
    while (tmp_cd->GetParentHandle() != NULL) {
      tmp_cd = tmp_cd->GetParentHandle()->ptr_cd_; 
      LOG_DEBUG("tmp_cd's level=%lu\n",(unsigned long)tmp_cd->cd_id_.level());
      LOG_DEBUG("tmp_cd->incomplete_log_.size()=%ld\n",
                    tmp_cd->incomplete_log_.size());
      for (it = tmp_cd->incomplete_log_.begin(); 
           it != tmp_cd->incomplete_log_.end(); 
           ++it) 
      {
        if (it->flag_ == flag) {
          found = 1;
          LOG_DEBUG("Found the entry in incomplete_log_ in one parent CD\n");
          break;
        }
      }
      if (found) {
        break;
      }
    }

    if (!found)
    {
      //ERROR_MESSAGE("Do not find corresponding Isend/Irecv incomplete log!!\n")
      LOG_DEBUG("Possible bug: Isend/Irecv incomplete log NOT found (maybe deleted by PMPI_Test)!!\n")
    }
  }
  
  // ProbeAndLogData for comm_log_ptr_ if relaxed CD
  // If NOT found, then work has been completed by previous Test Ops
  if (comm_log_ptr_ != NULL && found) {
    // ProbeAndLogData:
    // 1) change Isend/Irecv entry to complete state if there is any
    // 2) log data if Irecv
#if CD_DEBUG_ENABLED
    LOG_DEBUG("Print Incomplete Log before calling comm_log_ptr_->ProbeAndLogData\n");
    PrintIncompleteLog();
#endif

    // For inter-CD message, we record payload, otherwise, just record event itself.
    // FIXME
    if(it->intra_cd_msg_ == false) { 
      bool found = comm_log_ptr_->ProbeAndLogData(it->addr_, it->length_, flag, it->isrecv_);
      if (!found)  {
        CD *tmp_cd = this;
        while (tmp_cd->GetParentHandle() != NULL)  {
          //tmp_cd = tmp_cd->GetParentHandle()->ptr_cd_; 
          if (MASK_CDTYPE(tmp_cd->GetParentHandle()->ptr_cd()->cd_type_)==kRelaxed) {
            found = tmp_cd->GetParentHandle()->ptr_cd()->comm_log_ptr_
              ->ProbeAndLogDataPacked(it->addr_, it->length_, flag, it->isrecv_);
            if (found)
              break;
          }
          else {
            break;
          }
          tmp_cd = tmp_cd->GetParentHandle()->ptr_cd_; 
        }
        if (!found)
        {
          LOG_DEBUG("Possible bug: not found the incomplete logs...\n");
        }
      }
  
    }
    else {
      LOG_DEBUG("Skipped copying msg payload because it is intra-CD message!\n");
    }
    // need to log that wait op completes 
#if _MPI_VER
    comm_log_ptr_->LogData((void*)flag, 0, it->taskID_);
//    comm_log_ptr_->LogData(&flag, 0, it->taskID_);
#elif _PGAS_VER
    comm_log_ptr_->LogData((void*)flag, 0, it->taskID_);
#endif
  }
  else if (comm_log_ptr_ != NULL) {
    // need to log that wait op completes 
#if _MPI_VER
   // std::cout << it->taskID_ << std::endl;
//    if( &*it != 0 ) // FIXME
    ERROR_MESSAGE("[%s] Wrong control flow!\n", __func__);
      comm_log_ptr_->LogData((void*)flag, 0, it->taskID_);
//    comm_log_ptr_->LogData(&flag, 0, it->taskID_);
#elif _PGAS_VER
    comm_log_ptr_->LogData((void*)flag, 0, it->taskID_);
#endif
  }

  // delete the incomplete log entry
  if(found)
    tmp_cd->incomplete_log_.erase(it);

  LogEpilogue();
  return kCommLogOK;
}

//SZ
CommLogErrT CD::LogData(const void *data_ptr, unsigned long length, uint32_t task_id, 
                      bool completed, MsgFlagT flag, bool isrecv, bool isrepeated, 
                      bool intra_cd_msg, int tag, ColorT comm)
{
  LogPrologue();
  CommLogErrT ret;
  if (comm_log_ptr_ == NULL) {
    ERROR_MESSAGE("Null pointer of comm_log_ptr_ when trying to log data!\n");
    ret = kCommLogError;
  }
#if _LOG_PROFILING
  num_log_entry_++;
  tot_log_volume_+=length;
#endif
  ret = comm_log_ptr_->LogData(data_ptr, length, task_id, completed, flag, isrecv, isrepeated, intra_cd_msg, tag, comm);
  LogEpilogue();
  return ret;
}

//SZ
CommLogErrT CD::ProbeData(const void *data_ptr, unsigned long length)
{
  LogPrologue();
  CommLogErrT ret;
  if (comm_log_ptr_ == NULL) {
    ERROR_MESSAGE("Null pointer of comm_log_ptr_ when trying to read data!\n");
    ret = kCommLogError;
  }
  ret = comm_log_ptr_->ProbeData(data_ptr, length);
  if (ret == kCommLogCommLogModeFlip){
    SetCDLoggingMode(kRelaxedCDGen);
    LOG_DEBUG("CDLoggingMode changed to %d\n", GetCDLoggingMode());
  }

  LogEpilogue();
  return ret;
}

//SZ
CommLogErrT CD::ReadData(void *data_ptr, unsigned long length)
{
  LogPrologue();
  CommLogErrT ret;
  if (comm_log_ptr_ == NULL)
  {
    ERROR_MESSAGE("Null pointer of comm_log_ptr_ when trying to read data!\n");
    return kCommLogError;
  }
  ret = comm_log_ptr_->ReadData(data_ptr, length);
  if (ret == kCommLogCommLogModeFlip){
    SetCDLoggingMode(kRelaxedCDGen);
    LOG_DEBUG("CDLoggingMode changed to %d\n", GetCDLoggingMode());
  }
  else if(ret == kCommLogMissing) {
    // Report escalation
  }
  LogEpilogue();

  return ret;
}

//SZ
CommLogMode CD::GetCommLogMode()
{
  if(comm_log_ptr_ == NULL) return kInvalidLogMode;
  else return comm_log_ptr_->GetCommLogMode();
}

//SZ
bool CD::IsNewLogGenerated()
{
  return comm_log_ptr_->IsNewLogGenerated_();
}


void CD::PrintIncompleteLog()
{
  if (incomplete_log_.size()==0) return;
  LOG_DEBUG("incomplete_log_.size()=%ld\n", incomplete_log_.size());
  for (std::vector<IncompleteLogEntry>::iterator ii=incomplete_log_.begin();
        ii != incomplete_log_.end(); ii++)
  {
    LOG_DEBUG("\nPrint Incomplete Log information:\n");
    LOG_DEBUG("    addr_=%lx\n", ii->addr_);
    LOG_DEBUG("    length_=%ld\n", ii->length_);
    LOG_DEBUG("    flag_=%ld\n", ii->flag_);
    LOG_DEBUG("    complete_=%d\n", ii->complete_);
    LOG_DEBUG("    isrecv_=%d\n", ii->isrecv_);
  }
}
#endif
//commLog ends 

//CDType CD::GetCDType()
//{ return static_cast<CDType>(MASK_CDTYPE(cd_type_)); }

CD::CDInternalErrT CD::InvokeAllErrorHandler(void) {
  CDInternalErrT err = kOK;
  CDHandle *cdp = CDPath::GetCoarseCD(CDPath::GetCurrentCD());
  CD *cd = cdp->ptr_cd();
  while(cdp != NULL) {
    CD_DEBUG("=========== %16s lv %u events: %zu pending counter:%u, mailbox:%lx\n", 
        cdp->GetLabel(), cd->level(), cd->cd_event_.size(), *(cd->pendingFlag_), 
        cd->IsHead() ? (cd->event_flag_)[cd->task_in_color()] : *(cd->event_flag_));

    err = cdp->ptr_cd_->InvokeErrorHandler();
    cdp = CDPath::GetParentCD(cdp->level());
  }
  return err;
}


CD::CDInternalErrT CD::InvokeErrorHandler(void)
{
//  CD_DEBUG("\n[CD::InvokeErrorHandler] event queue size : %zu\n", cd_event_.size());
  CDInternalErrT cd_err = kOK;
  CD_DEBUG("====================================== events: %zu pending counter:%u, mailbox:%lx\n", cd_event_.size(), *pendingFlag_, IsHead() ? event_flag_[task_in_color()] : *event_flag_);

  while(!cd_event_.empty()) {
    CD_DEBUG("==== cd event size : %zu ====\n", cd_event_.size());

    EventHandler *cd_event_handler = cd_event_.front();
    cd_event_handler->HandleEvent();
//    delete cd_event_handler;
    CD_DEBUG("before pop events:%zu\n", cd_event_.size());
    cd_event_.pop_front();
    CD_DEBUG("after  pop events:%zu\n", cd_event_.size());
  }
  CD_DEBUG("================================================== pending counter:%u, mailbox:%lx\n", *pendingFlag_, IsHead() ? event_flag_[task_in_color()] : *event_flag_);
  
#if _MPI_VER
//  CD_DEBUG("Handled : %d, current pending flag (# pending events) : %d\n", handled_event_count, *pendingFlag_);
#endif


  return cd_err;
}

//CD::CDInternalErrT HeadCD::InvokeErrorHandler(void)
//{
//  cddbg << "HeadCD::CDInternalErrT CD::InvokeErrorHandler(void), event queue size : " << cd_event_.size() << ", (head) event queue size : " << cd_event_.size()<< endl;
//  CDInternalErrT cd_err = kOK;
//
//  while(!cd_event_.empty()) {
//    EventHandler *headcd_event_handler = cd_event_.back();
//    headcd_event_handler->HandleEvent();
//    delete headcd_event_handler;
//    cd_event_.pop_back();
//  }
//  while(!cd_event_.empty()) {
//    EventHandler *cd_event_handler = cd_event_.back();
//    cd_event_handler->HandleEvent();
//    delete cd_event_handler;
//    cd_event_.pop_back();
//  }
//
//
//  cddbg << "Handled event count : " << handled_event_count << endl;
//  cddbg << "currnt pending count : " << *pendingFlag_ << endl;
//
//
//
//  return cd_err;
//}

void *CD::Serialize(uint64_t &len_in_bytes)
{
  return (void *)&len_in_bytes;
}
void CD::Deserialize(void *object)
{
  //printf("Deserialize(%p)\n", object);
}

void *HeadCD::Serialize(uint64_t &len_in_bytes)
{
  return (void *)&len_in_bytes;
}
void HeadCD::Deserialize(void *object)
{
  //printf("Deserialize(%p)\n", object);
}
//-------------------------------------------------------



CDEntry *CD::SearchEntry(ENTRY_TAG_T tag_to_search, uint32_t &found_level, uint16_t attr)
{
  CD_DEBUG("Search Entry at Lv%u (%s): %lx (tag:%s) at level #%u \n", level(), name_.c_str(), tag_to_search, tag2str[tag_to_search].c_str(), level());

  CDHandle *parent_cd = CDPath::GetCurrentCD();
  CDEntry *entry = NULL;

  while( parent_cd != NULL ) {
    CD *ptr_cd = parent_cd->ptr_cd();
    CD_DEBUG("InternalGetEntry at %s level #%u\n", ptr_cd->label_.c_str(), ptr_cd->level());
    PRINT_MPI("Search %s (%lx) at level #%u (%s)\n", tag2str[tag_to_search].c_str(), tag_to_search,
              ptr_cd->level(), ptr_cd->label_.c_str());
    entry = ptr_cd->InternalGetEntry(tag_to_search, attr);
  
    if(entry != NULL) {
      assert(entry->size_.Check(Attr::krefer) == 0);
      found_level = ptr_cd->level();
       
      CD_DEBUG("Finally Found %s (%lx) at level #%u=%u (%s)\n", tag2str[tag_to_search].c_str(), tag_to_search,
          ptr_cd->GetCDID().level(), found_level, ptr_cd->label_.c_str());
      PRINT_MPI("Finally Found %s (%lx) at level #%u=%u (%s)\n", tag2str[tag_to_search].c_str(), tag_to_search,
          ptr_cd->GetCDID().level(), found_level, ptr_cd->label_.c_str());
      break;
    }
    else {
      parent_cd = CDPath::GetParentCD(ptr_cd->level());
      if(parent_cd != NULL) {
        CD_DEBUG("Gotta go to upper level! -> %s at level#%u\n", 
            parent_cd->GetName(), ptr_cd->GetCDID().level());
//        if(myTaskID == 0) {  printf("Gotta go to upper level! -> %s at level#%u\n", 
//            parent_cd->GetName(), ptr_cd->GetCDID().level()); }
      }
    }
  } 

  CD_DEBUG("%s. Check entry %lx at Node ID %s, CDName %s Lv%u\n", 
           (entry != NULL)? "Found":"NotFound",
           tag_to_search, GetNodeID().GetString().c_str(), GetCDName().GetString().c_str(), found_level);
  
//  if(entry == NULL) {
//    if(cd::failed_phase != HEALTHY && myTaskID == 0) {
//      char tmp[16];
//      sprintf(tmp, "SH %u", level());
//      entry_directory_.table_->PrintEntry(tmp);
//    }
//    assert(0);
//  }
  return entry;
}


bool CD::CanRecover(uint64_t error_name_mask, 
                    uint64_t error_location_mask)
{
  // STUB
  return true;
}

CD::CDInternalErrT CD::Sync(ColorT color)
{

#if _MPI_VER
  PMPI_Barrier(color);
#endif

  return CDInternalErrT::kOK;
}

CD::CDInternalErrT CD::SyncFile(void)
{

  // STUB
  //
  return CDInternalErrT::kOK;
}
  

CD *CD::GetCoarseCD(CD* curr_cd)
{
  while( curr_cd->task_size() == 1 ) {
    if(curr_cd == CDPath::GetRootCD()->ptr_cd()) {
      //CD_DEBUG("There is a single task in the root CD\n");
//        assert(0);
      return curr_cd;
    }
    curr_cd = CDPath::GetParentCD(curr_cd->level())->ptr_cd();
  } 
  return curr_cd;
}


//uint64_t GetCDEntryID(const char *str)
uint64_t cd::GetCDEntryID(const std::string &str)
{
//  printf("%s : str: %s\n", __func__, str);
//  if(str == NULL) {
//    printf("null?\n");
//  }
  //std::string entry_str(str);
  uint64_t id = cd_hash(str);
  tag2str[id] = str;
  //uint64_t id = cd_hash(std::string(str));
//  std::unordered_map<uint64_t, string>::const_iterator it = tag2str.find(id);
//  if(it == tag2str.end()) {
//    tag2str[id] = entry_str;
//  }
//  tag2str[id] = str;
  return id;
}

const char *cd::GetCDEntryStr(uint64_t id)
{
  return tag2str[id].c_str();
}
