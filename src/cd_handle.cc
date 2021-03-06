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

#include <sys/stat.h>
#include <cmath>
#include <time.h>
#include "cd_config.h"
#include "cd_features.h"
#include "cd_handle.h"
#include "cd_def_internal.h"
#include "cd_path.h"
#include "node_id.h"
#include "sys_err_t.h"
#include "cd_internal.h"
#include "cd_def_preserve.h"
#include "phase_tree.h"
#include "packer_prof.h"
//#include "persistence/define.h"
//#include "machine_specific.h"
//#include "profiler_interface.h"
//#include "cd_global.h"
//#include "error_injector.h"
#if CD_PROFILER_ENABLED
#include "cd_profiler.h"
#endif

using namespace cd;
using namespace common;
using namespace ::interface;
using namespace cd::internal;
using namespace std;
#define STOPHANDLE 
CDHandle *cd::null_cd = NULL;
/// KL
/// cddbg is a global variable to be used for debugging purpose.
/// General usage is that it stores any strings, numbers as a string internally,
/// and when cddbg.flush() is called, it write the internally stored strings to file,
/// then clean up the internal storage.
//#if CD_DEBUG_ENABLED
cd::DebugBuf cd::cddbg;
//#endif

FILE *cd::cdout=NULL;
FILE *cd::cdoutApp=NULL;
//std::string cd::output_basepath;

#define RANDOM_SEED 17
#if CD_ERROR_INJECTION_ENABLED
MemoryErrorInjector *CDHandle::memory_error_injector_ = NULL;
SystemErrorInjector *CDHandle::system_error_injector_ = NULL;
#define CHECK_SYS_ERR_VEC(X,Y) \
  (((X) & (Y)) == (X))

#endif
char host_name[64] = "NoNamed";
char exec_name[64] = "NoNamed";
char ftype_name[64] = "NoNamed";
char start_date[64] = "NoNamed";
char end_date[64] = "NoNamed";
char *exec_details = NULL;
char *exec_iterations = NULL;
int cd::app_input_size = 0;
int cd_task_id = 0;
int cd_task_sz = 1;
std::vector<uint32_t> cd::total_errors;

bool cd::is_koutput_disabled = false;
bool cd::is_error_free = false;
bool cd::runtime_initialized = false;
bool cd::runtime_activated = false;
bool cd::orig_app_side = true;
bool cd::orig_disabled = false;
bool cd::orig_msg_app_side = true;
bool cd::orig_msg_disabled = false;
bool packer::orig_disabled = false;
bool packer::orig_appside = true;
bool tuned::orig_disabled = false;
bool tuned::orig_appside = true;
float cd::err_scale = 0;
bool cd::dont_preserve = false;
bool cd::dont_cdop = false;
bool cd::dont_error = false;
int dont_cdop_int = 0;
double cd::tot_rtov[4];
uint64_t cd::data_grow_unit=DATA_GROW_UNIT;
CD_CLOCK_T cd::cdr_elapsed_time=0;
CD_CLOCK_T cd::global_reex_clk=0;
CD_CLOCK_T cd::tot_begin_clk=0;
CD_CLOCK_T cd::tot_end_clk=0;
CD_CLOCK_T cd::begin_clk=0;
CD_CLOCK_T cd::end_clk=0;
CD_CLOCK_T tuned::begin_clk=0;
CD_CLOCK_T tuned::end_clk=0;
CD_CLOCK_T tuned::elapsed_time;
CD_CLOCK_T cd::body_elapsed_time=0;
CD_CLOCK_T cd::reex_elapsed_time=0;
CD_CLOCK_T cd::normal_sync_time=0;
CD_CLOCK_T cd::reexec_sync_time=0;
CD_CLOCK_T cd::recovery_sync_time=0;
CD_CLOCK_T cd::prv_elapsed_time=0;
CD_CLOCK_T cd::rst_elapsed_time=0;
CD_CLOCK_T cd::create_elapsed_time=0;
CD_CLOCK_T cd::destroy_elapsed_time=0;
CD_CLOCK_T cd::begin_elapsed_time=0;
CD_CLOCK_T cd::compl_elapsed_time=0;
CD_CLOCK_T cd::advance_elapsed_time=0;

CD_CLOCK_T cd::body_elapsed_smpl=0;
CD_CLOCK_T cd::reex_elapsed_smpl=0;
CD_CLOCK_T cd::normal_sync_smpl=0;
CD_CLOCK_T cd::reexec_sync_smpl=0;
CD_CLOCK_T cd::recovery_sync_smpl=0;
CD_CLOCK_T cd::prv_elapsed_smpl=0;
CD_CLOCK_T cd::rst_elapsed_smpl=0;
CD_CLOCK_T cd::create_elapsed_smpl=0;
CD_CLOCK_T cd::destroy_elapsed_smpl=0;
CD_CLOCK_T cd::begin_elapsed_smpl=0;
CD_CLOCK_T cd::compl_elapsed_smpl=0;
CD_CLOCK_T cd::advance_elapsed_smpl=0;

std::vector<float> elapsed_trace;
std::vector<float> nm_sync_trace;
std::vector<float> rx_sync_trace;
std::vector<float> rc_sync_trace;
std::vector<float> prvtime_trace;
std::vector<float> rsttime_trace;
std::vector<float> creatcd_trace;
std::vector<float> destroy_trace;
std::vector<float> begincd_trace;
std::vector<float> complcd_trace;
std::vector<float> advance_trace;
std::vector<float> mailbox_trace;

uint64_t profile_counter = 0;
void cd_update_profile(void)
{
  profile_counter++;
  elapsed_trace.push_back(      body_elapsed_smpl);     body_elapsed_smpl=0;
                                                        reex_elapsed_smpl=0;
  nm_sync_trace.push_back(      normal_sync_smpl);       normal_sync_smpl=0;
  rx_sync_trace.push_back(      reexec_sync_smpl);       reexec_sync_smpl=0;
  rc_sync_trace.push_back(    recovery_sync_smpl);     recovery_sync_smpl=0;
  prvtime_trace.push_back(      prv_elapsed_smpl);       prv_elapsed_smpl=0;
  rsttime_trace.push_back(      rst_elapsed_smpl);       rst_elapsed_smpl=0;
  creatcd_trace.push_back(   create_elapsed_smpl);    create_elapsed_smpl=0;
  destroy_trace.push_back(  destroy_elapsed_smpl);   destroy_elapsed_smpl=0;
  begincd_trace.push_back(    begin_elapsed_smpl);     begin_elapsed_smpl=0;
  complcd_trace.push_back(    compl_elapsed_smpl);     compl_elapsed_smpl=0;
  advance_trace.push_back(  advance_elapsed_smpl);   advance_elapsed_smpl=0;
  mailbox_trace.push_back(  mailbox_elapsed_smpl);   mailbox_elapsed_smpl=0;
}

void cd::GatherProfile(void)
{
  packer::Time::GatherBW();
  float *elapsed_acc = NULL;
  float *nm_sync_acc = NULL;
  float *rx_sync_acc = NULL;
  float *rc_sync_acc = NULL;
  float *prvtime_acc = NULL;
  float *rsttime_acc = NULL;
  float *creatcd_acc = NULL;
  float *destroy_acc = NULL;
  float *begincd_acc = NULL;
  float *complcd_acc = NULL;
  float *advance_acc = NULL;
  float *mailbox_acc = NULL;



  int tot_elems = profile_counter * totalTaskSize;
  if(myTaskID == 0) {
    elapsed_acc = (float *)malloc(tot_elems * sizeof(float));
    nm_sync_acc = (float *)malloc(tot_elems * sizeof(float));
    rx_sync_acc = (float *)malloc(tot_elems * sizeof(float));
    rc_sync_acc = (float *)malloc(tot_elems * sizeof(float));
    prvtime_acc = (float *)malloc(tot_elems * sizeof(float));
    rsttime_acc = (float *)malloc(tot_elems * sizeof(float));
    creatcd_acc = (float *)malloc(tot_elems * sizeof(float));
    destroy_acc = (float *)malloc(tot_elems * sizeof(float));
    begincd_acc = (float *)malloc(tot_elems * sizeof(float));
    complcd_acc = (float *)malloc(tot_elems * sizeof(float));
    advance_acc = (float *)malloc(tot_elems * sizeof(float));
    mailbox_acc = (float *)malloc(tot_elems * sizeof(float));

  }
  assert(profile_counter == elapsed_trace.size());
  assert(profile_counter == nm_sync_trace.size());
  assert(profile_counter == rx_sync_trace.size());
  assert(profile_counter == rc_sync_trace.size());
  assert(profile_counter == prvtime_trace.size());
  MPI_Gather(elapsed_trace.data(), profile_counter, MPI_FLOAT, elapsed_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(nm_sync_trace.data(), profile_counter, MPI_FLOAT, nm_sync_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(rx_sync_trace.data(), profile_counter, MPI_FLOAT, rx_sync_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(rc_sync_trace.data(), profile_counter, MPI_FLOAT, rc_sync_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(prvtime_trace.data(), profile_counter, MPI_FLOAT, prvtime_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(rsttime_trace.data(), profile_counter, MPI_FLOAT, rsttime_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(creatcd_trace.data(), profile_counter, MPI_FLOAT, creatcd_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(destroy_trace.data(), profile_counter, MPI_FLOAT, destroy_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(begincd_trace.data(), profile_counter, MPI_FLOAT, begincd_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(complcd_trace.data(), profile_counter, MPI_FLOAT, complcd_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(advance_trace.data(), profile_counter, MPI_FLOAT, advance_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Gather(mailbox_trace.data(), profile_counter, MPI_FLOAT, mailbox_acc, profile_counter, MPI_FLOAT, 0, MPI_COMM_WORLD);
  if(myTaskID == 0) {
    char tmpfile[512];
    sprintf(tmpfile, "prof_trace.%s.%s.%d.%s.json", exec_name, (exec_details!=NULL)? exec_details : "NoInput", totalTaskSize, start_date);
    FILE *tfp = fopen(tmpfile, "w"); 
    if(tfp == 0) { printf("failed to open %s\n", tmpfile); }
    else {
      fprintf(tfp, "{\n");
      fprintf(tfp, "  \"name\":\"%s\",\n", exec_name);
      fprintf(tfp, "  \"input\":%s,\n", (exec_details!=NULL)? exec_details : "NoInput");
      fprintf(tfp, "  \"iters\":%d,\n", app_input_size);
      fprintf(tfp, "  \"nTask\":%d,\n", totalTaskSize);
      fprintf(tfp, "  \"GlobalDisk BW\": [%lf,%lf,%lf,%lf]\n", packer::time_mpiio_write.bw_avg, packer::time_mpiio_write.bw_std, packer::time_mpiio_write.bw_min, packer::time_mpiio_write.bw_max);
      fprintf(tfp, "  \"LocalDisk BW\":[%lf,%lf,%lf,%lf]\n", packer::time_posix_write.bw_avg, packer::time_posix_write.bw_std, packer::time_posix_write.bw_min, packer::time_posix_write.bw_max);
      fprintf(tfp, "  \"LocalMemory BW\":[%lf,%lf,%lf,%lf]\n", packer::time_copy.bw_avg, packer::time_copy.bw_std, packer::time_copy.bw_min, packer::time_copy.bw_max);
      fprintf(tfp, "  \"prof\": {\n");
      PrintPackerProf(tfp);
  fprintf(tfp, "    \"elapsed\": [%f", elapsed_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", elapsed_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"nm_sync\": [%f", nm_sync_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", nm_sync_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"rx_sync\": [%f", rx_sync_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", rx_sync_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"rc_sync\": [%f", rc_sync_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", rc_sync_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"prvtime\": [%f", prvtime_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", prvtime_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"rsttime\": [%f", rsttime_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", rsttime_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"creatcd\": [%f", creatcd_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", creatcd_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"destroy\": [%f", destroy_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", destroy_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"begincd\": [%f", begincd_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", begincd_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"complcd\": [%f", complcd_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", complcd_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"advance\": [%f", advance_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", advance_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"mailbox\": [%f", mailbox_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", mailbox_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"sync\": [%f", nm_sync_acc[0] + rx_sync_acc[0] + rc_sync_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", nm_sync_acc[i] + rx_sync_acc[i] + rc_sync_acc[i]); } fprintf(tfp, "],\n");
  fprintf(tfp, "    \"cdrt\": [%f", begincd_acc[0] + complcd_acc[0] + creatcd_acc[0] + destroy_acc[0]); for(int i=1; i<tot_elems; i++) { fprintf(tfp, ",%f", begincd_acc[i] + complcd_acc[i] + creatcd_acc[i] + destroy_acc[i]); } fprintf(tfp, "]\n");
      fprintf(tfp, "  }\n");
      fprintf(tfp, "}\n");
      fclose(tfp);
    }
    free(elapsed_acc);
    free(nm_sync_acc);
    free(rx_sync_acc);
    free(rc_sync_acc);
    free(prvtime_acc);
    free(rsttime_acc);
    free(creatcd_acc);
    free(destroy_acc);
    free(begincd_acc);
    free(complcd_acc);
    free(advance_acc);
    free(mailbox_acc);
  }
}

double cd::recvavg[PROF_GLOBAL_STATISTICS_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
double cd::recvstd[PROF_GLOBAL_STATISTICS_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
double cd::recvmin[PROF_GLOBAL_STATISTICS_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0};
double cd::recvmax[PROF_GLOBAL_STATISTICS_NUM] = {0,0,0,0,0,0,0,0,0,0,0,0,0};

/// KL
/// app_side is a global variable to differentiate application side and CD runtime side.
/// This switch is adopted to turn off runtime logging in CD runtime side,
/// and turn on only in application side.
/// Plus, it enables to control on/off of logging for a specific routine.
bool cd::app_side = true;

/// KL
/// These global variables are used because MAX_TAG_UB is different from MPI libraries.
/// For example, MAX_TAG_UB of MPICH is 29 bits and that of OpenMPI is 31 bits.
/// MPI_TAG is important because it is used to generate an unique message tag in the protocol for preservation-via-reference.
/// It is not possible to set MPI_TAG_UB to some value by application.
/// So, I decided to check the MAX_TAG_UB value from MPI runtime at initialization routine
/// and set the right bit positions in CDID::GenMsgTag(ENTRY_TAG_T tag), CDID::GenMsgTagForSameCD(int msg_type, int task_in_color) functions.
#if CD_MPI_ENABLED
MPI_Group cd::whole_group;
int  cd::max_tag_bit = 0;
int  cd::max_tag_level_bit = 0;
int  cd::max_tag_task_bit  = 0;
int  cd::max_tag_rank_bit  = 0;
#endif
int cd::myTaskID = 0;
int cd::totalTaskSize = 1;

//#if CD_MPI_ENABLED == 0 && CD_AUTOMATED == 1
//CDPath cd_path;
/// KL
/// uniquePath is a singleton object per process, which is used for CDPath.
//CDPath *CDPath::uniquePath_ = &cd_path;
//#else 
cd::internal::CDPath *cd::internal::CDPath::uniquePath_ = NULL;
//#endif

namespace cd {

enum {
  TOT_AVG=0,
  TOT_VAR,
  REEX_AVG,
  REEX_VAR,
  CD_AVG,
  CD_VAR,
  CD_NS_AVG,
  CD_NS_VAR,
  CD_RS_AVG,
  CD_RS_VAR,
  CD_ES_AVG,
  CD_ES_VAR,
  MSG_AVG,
  MSG_VAR,
  LOG_AVG,
  LOG_VAR,
  PRV_AVG,
  PRV_VAR,
  RST_AVG,
  RST_VAR,
  CREAT_AVG,
  CREAT_VAR,
  DESTROY_AVG,
  DESTROY_VAR,
  BEGIN_AVG,
  BEGIN_VAR,
  COMPL_AVG,
  COMPL_VAR,
  PROF_STATISTICS_NUM
};

enum {
  LV_PRV_AVG=0,
  LV_PRV_VAR,
  LV_RST_AVG,
  LV_RST_VAR,
  LV_CREAT_AVG,
  LV_CREAT_VAR,
  LV_DESTROY_AVG,
  LV_DESTROY_VAR,
  LV_BEGIN_AVG,
  LV_BEGIN_VAR,
  LV_COMPL_AVG,
  LV_COMPL_VAR,
  PROF_LEVEL_STATISTICS_NUM
};

enum {
  kProfDomain=1,
  kProfData=2,
};

//static inline
//void RecordProfDomain(uint32_t phase, uint32_t type, bool _reexec, double elapsed, uint64_t len=0)
//{
//  if(is_reexec == false) { // normal execution
//    if(type == kProfDomain) { // begin/complete
//      begin_elapsed_time += elapsed;
//      profMap[phase]->begin_elapsed_time_ += elapsed;
//    } else if(type == kProfData) { // Preserve
//      prv_elapsed_time += elapsed; 
//      profMap[phase]->prv_elapsed_time_ += elapsed; 
//      
//    }
//
//    if(type == kProfData) { 
//      prv_elapsed_time += elapsed; 
//      profMap[phase]->prv_elapsed_time_ += elapsed; 
//    } else { 
//      rst_elapsed_time += elapsed; 
//      profMap[phase]->rst_elapsed_time_ += elapsed; 
//    } 
//  } else { // reexecution 
//    
//  } 
//}
//
//void RecordProfData(uint32_t phase, const std::string &entry_str, uint32_t type, bool is_reexec, double elapsed, uint64_t len=0)
//{
//  if(is_reexec == false) { // normal execution
//    RuntimeInfo *ri = profMap[phase];
//    prv_elapsed_time += elapsed; 
//    ri->prv_elapsed_time_ += elapsed; 
//    ri->prv_copy_ += len;
//    if( CHECK_TYPE(type, kOutput) ) {
//      ri->per_entry_[entry_str].size_ += len;
//      ri->per_entry_[entry_str].type_ = ;
//      profMap[phase]->per_entry_[id].type_ = type;
//    } else {
//      profMap[phase]->per_entry_[id].size_ += len;
//      profMap[phase]->per_entry_[id].type_ = type;
//    }
//
//  } else { // reexecution 
//    rst_elapsed_time += elapsed; 
//    profMap[phase]->rst_elapsed_time_ += elapsed; 
//  } 
//}

static inline
void SetDebugFilepath(int myTask, string &dbg_basepath) 
{
#if CD_DEBUG_DEST == CD_DEBUG_TO_FILE || CD_DEBUG_ENABLED
  char *dbg_base_str = getenv( "CD_DBG_BASEPATH" );
  if(dbg_base_str != NULL) {
    dbg_basepath = dbg_base_str;
  }

  if(myTask == 0) {
    MakeFileDir(dbg_basepath.c_str());
  }
#endif
}

static inline
void OpenDebugFilepath(int myTask, const string &dbg_basepath)
{
#if CD_DEBUG_DEST == CD_DEBUG_TO_FILE
  char dbg_log_filename[] = CD_DBG_FILENAME;
  char dbg_filepath[256]={};
  snprintf(dbg_filepath, 256, "%s/%s_%d", dbg_basepath.c_str(), dbg_log_filename, myTask);
  cdout = fopen(dbg_filepath, "w");
  packer_stream = cdout;
//  printf("cdout:%p\n", cdout);
#endif

#if CD_DEBUG_ENABLED
  char app_dbg_log_filename[] = CD_DBGAPP_FILENAME;
  char app_dbg_filepath[256]={};
  snprintf(app_dbg_filepath, 256, "%s/%s_%d", dbg_basepath.c_str(), app_dbg_log_filename, myTask);
  cddbg.open(app_dbg_filepath);
#endif
}

static inline
void InitErrorInjection(int myTask)
{
  double random_seed = 0.0;
  if(myTask == 0) {
    random_seed = CD_CLOCK();
    CD_DEBUG("Random seed: %lf\n", random_seed);
  }

//  random_seed = 137378;
#if CD_MPI_ENABLED  
  PMPI_Bcast(&random_seed, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

  srand48(random_seed * (double)(RANDOM_SEED + myTask));
}

void InitDir(int myTask, int numTask)
{
  // Initialize static vars
  myTaskID      = myTask;
  totalTaskSize = numTask;

  string dbg_basepath(CD_DEFAULT_DEBUG_OUT);
  SetDebugFilepath(myTask, dbg_basepath);

  //printf("cdout:%p\n", cdout);
  internal::InitFileHandle(myTask == 0);
 
#if CD_MPI_ENABLED 
  // Synchronization is needed. 
  // Otherwise, some task may execute CD_DEBUG before head creates directory 
  PMPI_Barrier(MPI_COMM_WORLD);
#endif

  OpenDebugFilepath(myTask, dbg_basepath);


}
//void *sp_init = NULL;
//static void __attribute__((constructor)) get_init_stack_ptr(void)
//{
//  GetStackPtr(&sp_init);
//  printf("init stack:%p\n", sp_init);
//}
#if CD_MPI_ENABLED
char cd_err_str[256];
int cd_err_len = 0, cd_err_class = 0;
inline void CheckMPIError(int err) 
{
  if(err != MPI_SUCCESS) {
    MPI_Error_class(err, &cd_err_class);
    MPI_Error_string(err, cd_err_str, &cd_err_len);
    CDHandle *cdl = GetLeafCD();
    if(cdl != NULL) {
      printf("[%d] MPI ERROR (%s, %d):\n%s\n", myTaskID, cdl->GetLabel(), cdl->GetExecMode(), cd_err_str); fflush(stdout);
    } else {
      printf("[%d]MPI ERROR:\n%s\n", myTaskID, cd_err_str); fflush(stdout);
    }
    assert(0);
  }
}

//MPI_Errhandler *cd::mpi_err_handler = NULL;
void CD_MPI_ErrHandler(MPI_Comm *comm, int *err, ...)
{
  CheckMPIError(*err);
}
#endif
/// KL
CDHandle *CD_Init(int numTask, int myTask, PrvMediumT prv_medium)
{
  CDPrologue();
  
  gethostname(host_name, 64);
  char *is_noprv  = getenv("CD_NO_PRESERVE");
  char *is_nocd   = getenv("CD_NO_OPERATE");
  char *is_noerror= getenv("CD_NO_ERROR");
  char *is_err_scale = getenv( "KEEP_TOTAL_FAILURE_RATE_SAME" );
  cd::err_scale = (is_err_scale != NULL) ? atof(is_err_scale) : 0.;
  cd::dont_preserve = (is_noprv != NULL)? true:false;
  cd::dont_cdop     = (is_nocd != NULL)? true:false;
  cd::dont_error    = (is_noerror != NULL)? true:false;
  cd::dont_preserve|= cd::dont_cdop;
  cd::dont_error   |= cd::dont_preserve;

  dont_cdop_int = (dont_cdop)? 1 : 0;
#if CD_MPI_ENABLED
  MPI_Errhandler mpi_err_handler;
  // Define MPI error handler if necessary
  PMPI_Comm_create_errhandler(CD_MPI_ErrHandler, &mpi_err_handler);
  PMPI_Comm_set_errhandler(MPI_COMM_WORLD, mpi_err_handler);
#endif

  // Initialize static vars
  logger::taskID = myTask;
  myTaskID      = myTask;
  totalTaskSize = numTask;

  cd_task_id = myTask;
  cd_task_sz = numTask;

  cd::tot_begin_clk = CD_CLOCK();
//  char *cd_config_file = getenv("CD_OUTPUT_BASE");
//  if(cd_config_file != NULL) {
//    cd::output_basepath = cd_config_file;
//  }
//  else {
//    cd::output_basepath = CD_DEFAULT_OUTPUT_BASE;
//  }
//  printf("\n@@ Check %d\n", CD_TUNING_ENABLED);
  char *disable_koutput  = getenv("CD_KOUTPUT_DISABLED");
  is_koutput_disabled = (disable_koutput != NULL)? true : false;
  exec_details    = getenv("CD_EXEC_DETAILS");
  exec_iterations = getenv("CD_EXEC_ITERS");
  app_input_size  = (exec_iterations!=NULL)? atoi(exec_iterations) : 0;
  char *dgu = getenv("CD_DATA_GROW_UNIT");
  data_grow_unit  = (dgu!=NULL)? atol(dgu) : data_grow_unit;

#if CD_TUNING_ENABLED == 0
  char *cd_config_file = getenv("CD_CONFIG_FILENAME");
  if(cd_config_file != NULL) {
    cd::config.LoadConfig(cd_config_file, myTask);
    //printf("loadthis??\n");
  } else {
    cd::config.LoadConfig(CD_DEFAULT_CONFIG, myTask);
    //printf("load??\n");
  }
#endif
  if (cd::dont_error) {
      config.Init();
      is_error_free = true;
  }
/*
  // Initialize static vars
  myTaskID      = myTask;
  totalTaskSize = numTask;
 
//  char *cd_config_file = getenv("CD_CONFIG_FILENAME");
//  if(cd_config_file != NULL) {
//    cd::config.LoadConfig(cd_config_file);
//  } else {
//    cd::config.LoadConfig(CD_DEFAULT_CONFIG);
//  }

  string dbg_basepath(CD_DEFAULT_DEBUG_OUT);
  SetDebugFilepath(myTask, dbg_basepath);

  printf("cdout:%p\n", cdout);
  internal::InitFileHandle(myTask == 0);
 
#if CD_MPI_ENABLED 
  // Synchronization is needed. 
  // Otherwise, some task may execute CD_DEBUG before head creates directory 
  PMPI_Barrier(MPI_COMM_WORLD);
#endif

  OpenDebugFilepath(myTask, dbg_basepath);
*/
  InitDir(myTask, numTask);


  // Create Root CD
  NodeID new_node_id = NodeID(ROOT_COLOR, myTask, ROOT_HEAD_ID, numTask);
#if CD_MPI_ENABLED 
  new_node_id = CDHandle::GenNewNodeID(ROOT_HEAD_ID, new_node_id, false);
  //PMPI_Comm_group(new_node_id.color_, &whole_group); 
  whole_group = new_node_id.task_group_; 
#endif
  cd::runtime_initialized = true;
  time_t right_now = time(NULL);
  struct tm *p = localtime(&right_now);
  strftime(start_date, 64, "%F-%A-%H-%M-%S", p);
  CD::CDInternalErrT internal_err;
  CDHandle *root_cd_handle = CD::CreateRootCD( (strcmp(exec_name, "NoNamed") == 0)? DEFAULT_ROOT_LABEL : exec_name, 
                                              CDID(CDNameT(0), new_node_id), 
                                              static_cast<CDType>(kStrict | prv_medium), 
                                             /* FilePath::global_prv_path_,*/ 
                                              ROOT_SYS_DETECT_VEC, &internal_err);
  CDPath::GetCDPath()->push_back(root_cd_handle);

  // Create windows for pendingFlag and rollback_point 
  cd::internal::Initialize();

#if CD_PROFILER_ENABLED
  // Profiler-related
  root_cd_handle->profiler_->InitViz();
#endif

#if CD_ERROR_INJECTION_ENABLED
  InitErrorInjection(myTaskID);
  // To be safe
  CDHandle::memory_error_injector_ = NULL;
  CDHandle::system_error_injector_ = new SystemErrorInjector(config);
#endif

#if CD_DEBUG_ENABLED
  cddbg.flush();
#endif

  end_clk = CD_CLOCK();
  if(cd::myTaskID == 0) printf("CD Init time:%lfs\n", CD_CLK_MEA(end_clk - cd::tot_begin_clk));
  CDEpilogue(); // It enables logging.

  return root_cd_handle;
}

inline void WriteDbgStream(void)
{
#if CD_DEBUG_DEST == CD_DEBUG_TO_FILE
  fclose(cdout);
#endif

#if CD_DEBUG_ENABLED
  cddbg.flush();
  cddbg.close();
#endif
}

void CD_Finalize(void)
{
  //GONG
  CDPrologue();

  time_t right_now = time(NULL);
  struct tm *p = localtime(&right_now);
  strftime(end_date, 64, "%F-%A-%H-%M-%S", p);
//  start_date[strlen(start_date) - 1] = '\0';
  cd::tot_end_clk = CD_CLOCK();

  CD_DEBUG("========================= Finalize [%d] ===============================\n", myTaskID);

  CD_ASSERT_STR(CDPath::GetCDPath()->size()==1, 
      "path size:%lu\n", CDPath::GetCDPath()->size()); // There should be only on CD which is root CD
  assert(CDPath::GetCDPath()->back()!=NULL);


  // Get total aggregated errors
  std::map<int64_t, uint32_t> &error_count = CDHandle::system_error_injector_->sc_.error_count_;
  int idx = 0;
  uint32_t error_cnt = 0;
  uint32_t error_cnt_loc = error_count.size();
  uint32_t errors[error_cnt_loc];
  for (auto &v : error_count) {
    errors[idx++] = v.second;
  }
  PMPI_Allreduce(&error_cnt_loc, &error_cnt, 1, MPI_UNSIGNED, MPI_MAX, MPI_COMM_WORLD);
  assert(error_cnt_loc <= error_cnt);
  total_errors.resize(error_cnt);
  PMPI_Allreduce(errors, total_errors.data(), error_cnt, MPI_UNSIGNED, MPI_SUM, MPI_COMM_WORLD);
  if (myTaskID == 0) {
    for (int i=0; i<idx; i++) {
      printf("errors : %u %u\n", total_errors[i], errors[i]);
    }
  }
#if CD_PROFILER_ENABLED
  // Profiler-related  
  CDPath::GetRootCD()->profiler_->FinalizeViz();
#endif


//#if CD_ERROR_INJECTION_ENABLED
//  if(CDHandle::memory_error_injector_ != NULL);
//    delete CDHandle::memory_error_injector_;
//
//  if(CDHandle::system_error_injector_ != NULL);
//    delete CDHandle::system_error_injector_;
//#endif
//  CDPath::GetRootCD()->InternalDestroy(false);
//
//  cd::internal::Finalize();
#if CD_PROFILER_ENABLED 
//  std::map<uint32_t, RuntimeInfo> runtime_info;
//  RuntimeInfo summary = Profiler::GetTotalInfo(runtime_info);
//  runtime_info[100] = summary;
#endif
#if CD_PROFILER_ENABLED
  const double cd_elapsed            = CD_CLK_MEA(cd::body_elapsed_time);
  const double reex_elapsed          = CD_CLK_MEA(cd::reex_elapsed_time);
  const double normal_sync_elapsed   = CD_CLK_MEA(cd::normal_sync_time);
  const double reexec_sync_elapsed   = CD_CLK_MEA(cd::reexec_sync_time);
  const double recovery_sync_elapsed = CD_CLK_MEA(cd::recovery_sync_time);
  const double prv_elapsed           = CD_CLK_MEA(cd::prv_elapsed_time);
  const double rst_elapsed           = CD_CLK_MEA(cd::rst_elapsed_time);
  const double create_elapsed        = CD_CLK_MEA(cd::create_elapsed_time);
  const double destroy_elapsed       = CD_CLK_MEA(cd::destroy_elapsed_time);
  const double begin_elapsed         = CD_CLK_MEA(cd::begin_elapsed_time);
  const double compl_elapsed         = CD_CLK_MEA(cd::compl_elapsed_time);
  const double msg_elapsed           = CD_CLK_MEA(cd::msg_elapsed_time);
  const double log_elapsed           = CD_CLK_MEA(cd::log_elapsed_time);
  const double tot_elapsed           = CD_CLK_MEA(cd::tot_end_clk - cd::tot_begin_clk);
//  printf("tot elapsed:%lf\n", tot_elapsed);
  double sendbuf[PROF_STATISTICS_NUM]  = {
                         tot_elapsed, 
                         tot_elapsed * tot_elapsed,
                         reex_elapsed, 
                         reex_elapsed * reex_elapsed,
                         cd_elapsed,
                         cd_elapsed  * cd_elapsed,
                         normal_sync_elapsed, 
                         normal_sync_elapsed * normal_sync_elapsed, 
                         reexec_sync_elapsed, 
                         reexec_sync_elapsed * reexec_sync_elapsed, 
                         recovery_sync_elapsed, 
                         recovery_sync_elapsed * recovery_sync_elapsed, 
                         msg_elapsed,
                         msg_elapsed * msg_elapsed,
                         log_elapsed,
                         log_elapsed * log_elapsed,
                         prv_elapsed,
                         prv_elapsed * prv_elapsed,
                         rst_elapsed,
                         rst_elapsed * rst_elapsed,
                         create_elapsed,
                         create_elapsed * create_elapsed,
                         destroy_elapsed,
                         destroy_elapsed * destroy_elapsed,
                         begin_elapsed,
                         begin_elapsed * begin_elapsed,
                         compl_elapsed,
                         compl_elapsed * compl_elapsed,
                        };
//  printf("\n\n=====================================\n");
//  printf("%lf\t%lf\t%lf\t%lf\n", sendbuf[0],sendbuf[1],sendbuf[2],sendbuf[3]);
//  printf("=====================================\n\n");
  double recvbuf[PROF_STATISTICS_NUM] = {0.0,};
#if CD_MPI_ENABLED  
  MPI_Reduce(sendbuf, recvbuf, PROF_STATISTICS_NUM, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
#endif
  double tot_elapsed_avg     = recvbuf[TOT_AVG]/cd::totalTaskSize;
  double reex_elapsed_avg    = recvbuf[REEX_AVG]/cd::totalTaskSize;
  double cd_elapsed_avg      = recvbuf[CD_AVG]/cd::totalTaskSize;
  double cd_ns_elapsed_avg   = recvbuf[CD_NS_AVG]/cd::totalTaskSize;
  double cd_rs_elapsed_avg   = recvbuf[CD_RS_AVG]/cd::totalTaskSize;
  double cd_es_elapsed_avg   = recvbuf[CD_ES_AVG]/cd::totalTaskSize;
  double msg_elapsed_avg     = recvbuf[MSG_AVG]/cd::totalTaskSize;
  double log_elapsed_avg     = recvbuf[LOG_AVG]/cd::totalTaskSize;
  double prv_elapsed_avg     = recvbuf[PRV_AVG]/cd::totalTaskSize;
  double rst_elapsed_avg     = recvbuf[RST_AVG]/cd::totalTaskSize;
  double create_elapsed_avg  = recvbuf[CREAT_AVG]/cd::totalTaskSize;
  double destroy_elapsed_avg = recvbuf[DESTROY_AVG]/cd::totalTaskSize;
  double begin_elapsed_avg   = recvbuf[BEGIN_AVG]/cd::totalTaskSize;
  double compl_elapsed_avg   = recvbuf[COMPL_AVG]/cd::totalTaskSize;

  double tot_elapsed_var     = (tot_elapsed_avg    != 0)? sqrt(recvbuf[TOT_VAR]     / cd::totalTaskSize - tot_elapsed_avg * tot_elapsed_avg) : 0;
  double reex_elapsed_var    = (reex_elapsed_avg   != 0)? sqrt(recvbuf[REEX_VAR]    / cd::totalTaskSize - reex_elapsed_avg * reex_elapsed_avg) : 0;
  double cd_elapsed_var      = (cd_elapsed_avg     != 0)? sqrt(recvbuf[CD_VAR]      / cd::totalTaskSize - cd_elapsed_avg * cd_elapsed_avg) : 0;
  double cd_ns_elapsed_var   = (cd_ns_elapsed_avg  != 0)? sqrt(recvbuf[CD_NS_VAR]   / cd::totalTaskSize - cd_ns_elapsed_avg * cd_ns_elapsed_avg) : 0;
  double cd_rs_elapsed_var   = (cd_rs_elapsed_avg  != 0)? sqrt(recvbuf[CD_RS_VAR]   / cd::totalTaskSize - cd_rs_elapsed_avg * cd_rs_elapsed_avg) : 0;
  double cd_es_elapsed_var   = (cd_es_elapsed_avg  != 0)? sqrt(recvbuf[CD_ES_VAR]   / cd::totalTaskSize - cd_es_elapsed_avg * cd_es_elapsed_avg) : 0;
  double msg_elapsed_var     = (msg_elapsed_avg    != 0)? sqrt(recvbuf[MSG_VAR]     / cd::totalTaskSize - msg_elapsed_avg * msg_elapsed_avg) : 0;
  double log_elapsed_var     = (log_elapsed_avg    != 0)? sqrt(recvbuf[LOG_VAR]     / cd::totalTaskSize - log_elapsed_avg * log_elapsed_avg) : 0;
  double prv_elapsed_var     = (prv_elapsed_avg    != 0)? sqrt(recvbuf[PRV_VAR]     / cd::totalTaskSize - prv_elapsed_avg * prv_elapsed_avg) : 0;
  double rst_elapsed_var     = (rst_elapsed_avg    != 0)? sqrt(recvbuf[RST_VAR]     / cd::totalTaskSize - rst_elapsed_avg * rst_elapsed_avg) : 0;
  double create_elapsed_var  = (create_elapsed_avg != 0)? sqrt(recvbuf[CREAT_VAR]   / cd::totalTaskSize - create_elapsed_avg * create_elapsed_avg) : 0;
  double destroy_elapsed_var = (destroy_elapsed_avg!= 0)? sqrt(recvbuf[DESTROY_VAR] / cd::totalTaskSize - destroy_elapsed_avg * destroy_elapsed_avg) : 0;
  double begin_elapsed_var   = (begin_elapsed_avg  != 0)? sqrt(recvbuf[BEGIN_VAR]   / cd::totalTaskSize - begin_elapsed_avg * begin_elapsed_avg) : 0;
  double compl_elapsed_var   = (compl_elapsed_avg  != 0)? sqrt(recvbuf[COMPL_VAR]   / cd::totalTaskSize - compl_elapsed_avg * compl_elapsed_avg) : 0;


  double trecvavg[] = { tot_elapsed_avg     ,
              reex_elapsed_avg    ,
              cd_elapsed_avg      ,
              cd_ns_elapsed_avg   ,
              cd_rs_elapsed_avg   ,
              cd_es_elapsed_avg   ,
              msg_elapsed_avg     ,
              log_elapsed_avg     ,
              prv_elapsed_avg     ,
              rst_elapsed_avg     ,
              create_elapsed_avg  ,
              destroy_elapsed_avg ,
              begin_elapsed_avg   ,
              compl_elapsed_avg }; 

  double trecvstd[] = { tot_elapsed_var     ,
              reex_elapsed_var    ,
              cd_elapsed_var      ,
              cd_ns_elapsed_var   ,
              cd_rs_elapsed_var   ,
              cd_es_elapsed_var   ,
              msg_elapsed_var     ,
              log_elapsed_var     ,
              prv_elapsed_var     ,
              rst_elapsed_var     ,
              create_elapsed_var  ,
              destroy_elapsed_var ,
              begin_elapsed_var   ,
              compl_elapsed_var   };

  memcpy(recvavg, trecvavg, sizeof(double) * PROF_GLOBAL_STATISTICS_NUM);
  memcpy(recvstd, trecvstd, sizeof(double) * PROF_GLOBAL_STATISTICS_NUM);

  double sendbuf_min[PROF_GLOBAL_STATISTICS_NUM]  = {
                         tot_elapsed, 
                         reex_elapsed, 
                         cd_elapsed,
                         normal_sync_elapsed, 
                         reexec_sync_elapsed, 
                         recovery_sync_elapsed, 
                         msg_elapsed,
                         log_elapsed,
                         prv_elapsed,
                         rst_elapsed,
                         create_elapsed,
                         destroy_elapsed,
                         begin_elapsed,
                         compl_elapsed
  };
#if CD_MPI_ENABLED  
  PMPI_Reduce(sendbuf_min, recvmin, PROF_GLOBAL_STATISTICS_NUM, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
  PMPI_Reduce(sendbuf_min, recvmax, PROF_GLOBAL_STATISTICS_NUM, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
#endif

//  std::map<uint32_t, CDOverheadVar> lv_runtime_info;
//  for(auto it=runtime_info.begin(); it!=runtime_info.end(); ++it) {
//    double sendbuf_lv[PROF_LEVEL_STATISTICS_NUM]  = {
//                           it->second.prv_elapsed_time_, 
//                           it->second.prv_elapsed_time_ *     it->second.prv_elapsed_time_,
//                           it->second.rst_elapsed_time_, 
//                           it->second.rst_elapsed_time_ *     it->second.rst_elapsed_time_,
//                           it->second.create_elapsed_time_,
//                           it->second.create_elapsed_time_ *  it->second.create_elapsed_time_,
//                           it->second.destroy_elapsed_time_,
//                           it->second.destroy_elapsed_time_ * it->second.destroy_elapsed_time_,
//                           it->second.begin_elapsed_time_,
//                           it->second.begin_elapsed_time_ *   it->second.begin_elapsed_time_,
//                           it->second.compl_elapsed_time_,
//                           it->second.compl_elapsed_time_ *   it->second.compl_elapsed_time_
//                          };
//    double recvbuf_lv[PROF_LEVEL_STATISTICS_NUM] = {0.0,};
//#if CD_MPI_ENABLED  
//    PMPI_Reduce(sendbuf_lv, recvbuf_lv, PROF_LEVEL_STATISTICS_NUM, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
//#endif
//    lv_runtime_info[it->first].prv_elapsed_time_ = recvbuf_lv[LV_PRV_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].prv_elapsed_time_var_ = (recvbuf_lv[LV_PRV_VAR] - recvbuf_lv[LV_PRV_AVG]*recvbuf_lv[LV_PRV_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//    lv_runtime_info[it->first].rst_elapsed_time_ = recvbuf_lv[LV_RST_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].rst_elapsed_time_var_ = (recvbuf_lv[LV_RST_VAR] - recvbuf_lv[LV_RST_AVG]*recvbuf_lv[LV_RST_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//    lv_runtime_info[it->first].create_elapsed_time_ = recvbuf_lv[LV_CREAT_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].create_elapsed_time_var_ = (recvbuf_lv[LV_CREAT_VAR] - recvbuf_lv[LV_CREAT_AVG]*recvbuf_lv[LV_CREAT_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//    lv_runtime_info[it->first].destroy_elapsed_time_ = recvbuf_lv[LV_DESTROY_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].destroy_elapsed_time_var_ = (recvbuf_lv[LV_DESTROY_VAR] - recvbuf_lv[LV_DESTROY_AVG]*recvbuf_lv[LV_DESTROY_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//    lv_runtime_info[it->first].begin_elapsed_time_ = recvbuf_lv[LV_BEGIN_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].begin_elapsed_time_var_ = (recvbuf_lv[LV_BEGIN_VAR] - recvbuf_lv[LV_BEGIN_AVG]*recvbuf_lv[LV_BEGIN_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//    lv_runtime_info[it->first].compl_elapsed_time_ = recvbuf_lv[LV_COMPL_AVG]/cd::totalTaskSize;
//    lv_runtime_info[it->first].compl_elapsed_time_var_ = (recvbuf_lv[LV_COMPL_VAR] - recvbuf_lv[LV_COMPL_AVG]*recvbuf_lv[LV_COMPL_AVG]/cd::totalTaskSize)/cd::totalTaskSize;
//
//  }
  cd::tot_rtov[0]= recvavg[CREAT_PRF] + recvavg[DSTRY_PRF] + recvavg[BEGIN_PRF] +recvavg[COMPL_PRF];
  cd::tot_rtov[1]= recvstd[CREAT_PRF] + recvstd[DSTRY_PRF] + recvstd[BEGIN_PRF] +recvstd[COMPL_PRF];
  cd::tot_rtov[2]= recvmin[CREAT_PRF] + recvmin[DSTRY_PRF] + recvmin[BEGIN_PRF] +recvmin[COMPL_PRF];
  cd::tot_rtov[3]= recvmax[CREAT_PRF] + recvmax[DSTRY_PRF] + recvmax[BEGIN_PRF] +recvmax[COMPL_PRF];
  cd::mailbox_elapsed_time_in_sec = CD_CLK_MEA(cd::mailbox_elapsed_time);
  if(cd::myTaskID == 0) {
    printf("\n\n=============================================================\n");
    printf("                       AVG          STD          MIN          MAX\n");
    printf("Total time  : %4le %4le %4le %4le\n", recvavg[TOTAL_PRF], recvstd[TOTAL_PRF], recvmin[TOTAL_PRF], recvmax[TOTAL_PRF]);
    printf("Preserve[s] : %4le %4le %4le %4le\n", recvavg[PRV_PRF]  , recvstd[PRV_PRF]  , recvmin[PRV_PRF]  , recvmax[PRV_PRF]  ); 
    printf("Restore [s] : %4le %4le %4le %4le\n", recvavg[RST_PRF]  , recvstd[RST_PRF]  , recvmin[RST_PRF]  , recvmax[RST_PRF]  ); 
    printf("Rollback    : %4le %4le %4le %4le\n", recvavg[REEX_PRF], recvstd[REEX_PRF], recvmin[REEX_PRF], recvmax[REEX_PRF]);
    printf("Msg time    : %4le %4le %4le %4le\n", recvavg[MSG_PRF]  , recvstd[MSG_PRF]  , recvmin[MSG_PRF]  , recvmax[MSG_PRF]  );
    printf("CD overhead : %4le %4le %4le %4le\n", tot_rtov[0], tot_rtov[1], tot_rtov[2], tot_rtov[3]); 
    printf("CD Body     : %4le %4le %4le %4le\n", recvavg[CDOVH_PRF], recvstd[CDOVH_PRF], recvmin[CDOVH_PRF], recvmax[CDOVH_PRF]); 
    printf("Normal Sync : %4le %4le %4le %4le\n", recvavg[CD_NS_PRF], recvstd[CD_NS_PRF], recvmin[CD_NS_PRF], recvmax[CD_NS_PRF]); 
    printf("Reexec Sync : %4le %4le %4le %4le\n", recvavg[CD_RS_PRF], recvstd[CD_RS_PRF], recvmin[CD_RS_PRF], recvmax[CD_RS_PRF]); 
    printf("Recovr Sync : %4le %4le %4le %4le\n", recvavg[CD_ES_PRF], recvstd[CD_ES_PRF], recvmin[CD_ES_PRF], recvmax[CD_ES_PRF]); 
    printf("Create      : %4le %4le %4le %4le\n", recvavg[CREAT_PRF], recvstd[CREAT_PRF], recvmin[CREAT_PRF], recvmax[CREAT_PRF]); 
    printf("Destroy     : %4le %4le %4le %4le\n", recvavg[DSTRY_PRF], recvstd[DSTRY_PRF], recvmin[DSTRY_PRF], recvmax[DSTRY_PRF]); 
    printf("Begin       : %4le %4le %4le %4le\n", recvavg[BEGIN_PRF], recvstd[BEGIN_PRF], recvmin[BEGIN_PRF], recvmax[BEGIN_PRF]); 
    printf("Complete    : %4le %4le %4le %4le\n", recvavg[COMPL_PRF], recvstd[COMPL_PRF], recvmin[COMPL_PRF], recvmax[COMPL_PRF]); 
    printf("Libc   time : %4le %4le %4le %4le\n", recvavg[LOG_PRF]  , recvstd[LOG_PRF]  , recvmin[LOG_PRF]  , recvmax[LOG_PRF]  );
#if CD_PROFILER_ENABLED & CD_MPI_ENABLED
    printf("Mailbox     : %7.4lf \n", cd::mailbox_elapsed_time_in_sec); 
#endif
    printf("Ratio : %lf (Total) %lf (CD runtime) %lf (logging)\n", 
                            ((cd_elapsed_avg+msg_elapsed_avg) / tot_elapsed_avg) * 100,
                             (cd_elapsed_avg / tot_elapsed_avg) * 100, 
                             (msg_elapsed_avg / tot_elapsed_avg) * 100);

    printf("elapsed_ti:%lf==%lf\n", body_elapsed_time,      body_elapsed_smpl);
    printf("reex_time :%lf==%lf\n", reex_elapsed_time,      reex_elapsed_smpl);
    printf("normal_syn:%lf==%lf\n", normal_sync_time,       normal_sync_smpl);
    printf("reexec_syn:%lf==%lf\n", reexec_sync_time,       reexec_sync_smpl);
    printf("recovery_s:%lf==%lf\n", recovery_sync_time,     recovery_sync_smpl);
    printf("prv_elapse:%lf==%lf\n", prv_elapsed_time,       prv_elapsed_smpl);
    printf("rst_elapse:%lf==%lf\n", rst_elapsed_time,       rst_elapsed_smpl);
    printf("create_ela:%lf==%lf\n", create_elapsed_time,    create_elapsed_smpl);
    printf("destroy_el:%lf==%lf\n", destroy_elapsed_time,   destroy_elapsed_smpl);
    printf("begin_elap:%lf==%lf\n", begin_elapsed_time,     begin_elapsed_smpl);
    printf("compl_elap:%lf==%lf\n", compl_elapsed_time,     compl_elapsed_smpl);
    printf("advance_el:%lf==%lf\n", advance_elapsed_time,   advance_elapsed_smpl);
    printf("mailbox_el:%lf==%lf\n", cd::mailbox_elapsed_time_in_sec, mailbox_elapsed_smpl);

    printf("\n\n============================================\n");
//#if CD_PROFILER_ENABLED 
//    printf("Profile Result =================================\n");
//    for(auto it=lv_runtime_info.begin(); it!=lv_runtime_info.end(); ++it) {
//      printf("-- CD Overhead Level #%u ---------\n", it->first);
//      it->second.Print();
//    }
//    printf("================================================\n\n");
//#endif
  }
  GatherProfile();
#endif // CD_PROFILER_ENABLED_ENDS



#if CD_ERROR_INJECTION_ENABLED
  if(CDHandle::memory_error_injector_ != NULL)
    delete CDHandle::memory_error_injector_;

  if(CDHandle::system_error_injector_ != NULL)
    delete CDHandle::system_error_injector_;
#endif

  for(auto rit=CD::delete_store_.rbegin(); rit!=CD::delete_store_.rend(); ++rit) {
    if(rit->first == 0)
      break; 
    rit->second->ptr_cd_->Destroy(false, true);
    delete rit->second;
  }
  //cd::internal::Finalize();
  assert(CDPath::GetCDPath()->size() == 1);
  GetRootCD()->ptr_cd()->Destroy(false, true);
  delete CDPath::GetCDPath()->back(); // delete root
  CDPath::GetCDPath()->pop_back();

  cd::internal::Finalize();
  if(cd::runtime_activated) {
    cd::phaseTree.root_->GatherStats();
    cd::phaseTree.PrintStats();
  } else {
    if(cd::myTaskID == 0) {
      PhaseNode::PrintOutputJson(NULL);  
    }
  }
  //tuned::phaseTree.PrintStats();

#if CD_DEBUG_ENABLED
  WriteDbgStream();
#endif
  // call packer finalization at the very last moment for safety
  //packer::Finalize();  
  CDEpilogue();
#if CD_LIBC_LOGGING
  logger::Fini();
  logger::disabled = true;
#endif
}


////////////////////////////////////////////////////////
//
// CD Accessors
//
CDHandle *GetCurrentCD(void) 
{ return CDPath::GetCurrentCD(); }

CDHandle *GetLeafCD(void)
{ return CDPath::GetLeafCD(); }
  
CDHandle *GetRootCD(void)    
{ return CDPath::GetRootCD(); }

CDHandle *GetParentCD(void)
{ return CDPath::GetParentCD(); }

CDHandle *GetParentCD(int current_level)
{ return CDPath::GetParentCD(current_level); }

////////////////////////////////////////////////////////
//
// tuned CD Accessors
//
//namespace tuned 
//{
//
//tuned::CDHandle *GetCurrentCD(void) 
//{ return tuned::CDPath::GetCurrentCD(); }
//
//tuned::CDHandle *GetLeafCD(void)
//{ return tuned::CDPath::GetLeafCD(); }
//  
//tuned::CDHandle *GetRootCD(void)    
//{ return tuned::CDPath::GetRootCD(); }
//
//tuned::CDHandle *GetParentCD(void)
//{ return tuned::CDPath::GetParentCD(); }
//
//tuned::CDHandle *GetParentCD(int current_level)
//{ return tuned::CDPath::GetParentCD(current_level); }
//}
////////////////////////////////////////////////////////
//
// Split the node in three dimension
// (x,y,z)
// taskID = x + y * nx + z * (nx*ny)
// x = taskID % nx
// y = ( taskID % ny ) - x 
// z = r / (nx*ny)
int SplitCD_3D(const int& my_task_id, 
               const int& my_size, 
               const int& num_children, 
               int& new_color, 
               int& new_task)
{
  // Get current task group size
  int new_size = (int)((double)my_size / num_children);
  int num_x = round(pow(my_size, 1/3.));
  int num_y = num_x;
  int num_z = num_x;

  int num_children_x = 0;
  if(num_children > 1)       num_children_x = round(pow(num_children, 1/3.));
  else if(num_children == 1) num_children_x = 1;
  else assert(0);

  int num_children_y = num_children_x;
  int num_children_z = num_children_x;

  int new_num_x = num_x / num_children_x;
  int new_num_y = num_y / num_children_y;
  int new_num_z = num_z / num_children_z;

  assert(num_x*num_y*num_z == my_size);
  assert(num_children_x * num_children_y * num_children_z == (int)num_children);
  assert(new_num_x * new_num_y * new_num_z == new_size);
  assert(num_x != 0);
  assert(num_y != 0);
  assert(num_z != 0);
  assert(new_num_x != 0);
  assert(new_num_y != 0);
  assert(new_num_z != 0);
  assert(num_children_x != 0);
  assert(num_children_y != 0);
  assert(num_children_z != 0);
  
  int taskID = my_task_id; 
  int sz = num_x*num_y;
  int Z = (double)taskID / sz;
  int tmp = taskID % sz;
  int Y = (double)tmp / num_x;
  int X = tmp % num_x;

  new_color = (int)(X / new_num_x + (Y / new_num_y)*num_children_x + (Z / new_num_z)*(num_children_x * num_children_y));
  new_task  = (int)(X % new_num_x + (Y % new_num_y)*new_num_x      + (Z % new_num_z)*(new_num_x * new_num_y));
  
  return 0;
}

// CD split default
int SplitCD_1D(const int& my_task_id, 
                    const int& my_size, 
                    const int& num_children,
                    int& new_color, 
                    int& new_task)
{
  if(my_size%num_children){
    ERROR_MESSAGE("Cannot evenly split current CD with size=%d, and num_children=%d\n", my_size, num_children);
  }

  int new_size = my_size / num_children;
  new_color=my_task_id / new_size;
  new_task=my_task_id % new_size;
  
  return 0;
}

} // namespace cd ends







// CDHandle Member Methods ------------------------------------------------------------

CDHandle::CDHandle()
  : ptr_cd_(0)//, node_id_(-1)/*, ctxt_(CDPath::GetRootCD()->ctxt_)*/
{
  // FIXME
  assert(0);
  SplitCD = &SplitCD_1D;

#if CD_PROFILER_ENABLED
  profiler_ = Profiler::CreateProfiler(0, this);
#endif

#if CD_ERROR_INJECTION_ENABLED
  cd_error_injector_ = NULL;
#endif
//  count_ = 0;
//  interval_ = -1;
//  error_mask_ = -1;
//  active_ = false;


//  if(node_id().size() > 1)
//    PMPI_Win_create(pendingFlag_, 1, sizeof(CDFlagT), PMPI_INFO_NULL, PMPI_COMM_WORLD, &pendingWindow_);
//  else
//    cddbg << "KL : size is 1" << endl;
  
}

/// Design decision when we are receiving ptr_cd, do we assume that pointer is usable? 
/// That means is the handle is being created at local? 
/// Perhaps, we should not assume this 
/// as we will sometimes create a handle for an object that is located at remote.. right?
///
/// CDID set up. ptr_cd_ set up. parent_cd_ set up. children_cd_ will be set up later by children's Create call.
/// sibling ID set up, cd_info set up
/// clear children list
/// request to add me as a children to parent (to Head CD object)
CDHandle::CDHandle(CD *ptr_cd) 
  : ptr_cd_(ptr_cd)//, node_id_(ptr_cd->cd_id_.node_id_)/*, ctxt_(ptr_cd->ctxt_)*/
{
  SplitCD = &SplitCD_1D;

#if CD_PROFILER_ENABLED
  profiler_ = Profiler::CreateProfiler(0, this);
#endif

#if CD_ERROR_INJECTION_ENABLED
  cd_error_injector_ = NULL;
#endif
//  count_ = 0;
//  interval_ = -1;
//  error_mask_ = -1;
//  active_ = false;

//  if(node_id_.size() > 1)
//    PMPI_Win_create(pendingFlag_, 1, sizeof(CDFlagT), PMPI_INFO_NULL, PMPI_COMM_WORLD, &pendingWindow_);
//  else
//    cddbg << "KL : size is 1" << endl;
}

CDHandle::~CDHandle()
{
  // request to delete me in the children list to parent (to Head CD object)
  if(ptr_cd_ != NULL) {
    // We should do this at Destroy(), not creator?
//    RemoveChild(this);
    
  } 
  else {  // There is no CD for this CDHandle!!

  }

#if CD_ERROR_INJECTION_ENABLED
  if(cd_error_injector_ != NULL)
    delete cd_error_injector_;
#endif

#if CD_PROFILER_ENABLED
  delete profiler_;
#endif

}

void CDHandle::Init(CD *ptr_cd)
{ 
  ptr_cd_   = ptr_cd;
  //node_id_  = ptr_cd_->cd_id_.node_id_;
}

int CDHandle::SelectHead(uint32_t task_size)
{
  CD_ASSERT(task_size != 0);
  return (level() + 1) % task_size;
}

CDErrT CDHandle::RegisterSplitMethod(SplitFuncT split_func)
{ 
  SplitCD = split_func; 
  return common::kOK;
}


NodeID CDHandle::GenNewNodeID(int new_head, NodeID &node_id, bool is_reuse)
{
  // just set the same as parent.
  NodeID new_node_id(node_id);
#if CD_MPI_ENABLED
  if(is_reuse == false) {
    //printf("[%d] NowReuse 1\n", cd::myTaskID);
    PMPI_Comm_dup(node_id.color_, &(new_node_id.color_));
    PMPI_Comm_group(new_node_id.color_, &(new_node_id.task_group_));
  }
#endif
  new_node_id.set_head(new_head);
  //new_node.init_node_id(node_id_.color(), 0, INVALID_HEAD_ID,1);
  return new_node_id;
}

////////////////////////////////////////////////////////////////////
//
// Internal Begin
//
////////////////////////////////////////////////////////////////////

// Non-collective
CDHandle *CDHandle::Create(const char *name, 
                           int cd_type, 
                           uint32_t error_name_mask, 
                           uint32_t error_loc_mask, 
                           CDErrT *error )
{
  //GONG
  CDPrologue();
  CDHandle *new_cd_handle = this;
  if (cd::dont_cdop == false) {
  TUNE_DEBUG("[Real %s %s lv:%u phase:%d\n", __func__, name, level(), phase()); STOPHANDLE;
//  printf("[Real %s %s lv:%u phase:%d\n", __func__, name, level(), phase()); STOPHANDLE;
  //CheckMailBox();

  // Create CDHandle for a local node
  // Create a new CDHandle and CD object
  // and populate its data structure correctly (CDID, etc...)
  uint64_t sys_bit_vec = SetSystemBitVector(error_name_mask, error_loc_mask);
  bool is_reuse =  ptr_cd_->CheckToReuseCD(name);
  NodeID new_node_id = GenNewNodeID(SelectHead(task_size()), node_id(), true/*is_reuse*/);

  if(is_reuse == false) {
    CollectHeadInfoAndEntry(new_node_id); 
  } // otherwise, it will be done later.
  else {
    CD_DEBUG("Skip CollectHeadInfoAndEntry %s\n", GetName());
  }


  // Generate CDID
  CDNameT new_cd_name(ptr_cd_->GetCDName(), 1, 0, name);
  //CD_DEBUG("New CD Name : %s\n", new_cd_name.GetString().c_str());

  // Then children CD get new MPI rank ID. (task ID) I think level&taskID should be also pair.
  CD::CDInternalErrT internal_err;
  new_cd_handle = ptr_cd_->Create(this, name, CDID(new_cd_name, new_node_id), 
                                            static_cast<CDType>(cd_type), sys_bit_vec, &internal_err);

  CDPath::GetCDPath()->push_back(new_cd_handle);
//  getchar();
  end_clk = CD_CLOCK();
  const double elapsed = end_clk - begin_clk;
  cd::phaseTree.current_->profile_.RecordCreate(elapsed);
//  create_elapsed_time += elapsed;
//  create_elapsed_smpl += elapsed;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->create_elapsed_time_ += elapsed;
////  profMap[phase()]->create_elapsed_time_ += end_clk - begin_clk;
//#endif
  PRINT_MPI("** [Create] %s at level %u \n", 
            new_cd_handle->ptr_cd_->name_.c_str(), new_cd_handle->level());
  }
//cdcreate1_label:
  CDEpilogue();
  return new_cd_handle;
}

// Collective
CDHandle *CDHandle::Create(uint32_t  num_children,
                           const char *name, 
                           int cd_type, 
                           uint32_t error_name_mask, 
                           uint32_t error_loc_mask, 
                           CDErrT *error)
{
  CDPrologue();
  CDHandle *new_cd_handle = this;
  if (cd::dont_cdop == false) { //goto cdcreate2_label;
  TUNE_DEBUG("[Real %s %s lv:%u phase:%d\n", __func__, name, level(), phase()); STOPHANDLE;
#if CD_MPI_ENABLED

  // Create a new CDHandle and CD object
  // and populate its data structure correctly (CDID, etc...)
  //  This is an extremely powerful mechanism for dividing a single communicating group of processes into k subgroups, 
  //  with k chosen implicitly by the user (by the number of colors asserted over all the processes). 
  //  Each resulting communicator will be non-overlapping. 
  //  Such a division could be useful for defining a hierarchy of computations, 
  //  such as for multigrid, or linear algebra.
  //  
  //  Multiple calls to PMPI_COMM_SPLIT can be used to overcome the requirement 
  //  that any call have no overlap of the resulting communicators (each process is of only one color per call). 
  //  In this way, multiple overlapping communication structures can be created. 
  //  Creative use of the color and key in such splitting operations is encouraged.
  //  
  //  Note that, for a fixed color, the keys need not be unique. 
  //  It is PMPI_COMM_SPLIT's responsibility to sort processes in ascending order according to this key, 
  //  and to break ties in a consistent way. 
  //  If all the keys are specified in the same way, 
  //  then all the processes in a given color will have the relative rank order 
  //  as they did in their parent group. 
  //  (In general, they will have different ranks.)
  //  
  //  Essentially, making the key value zero for all processes of a given color means that 
  //  one doesn't really care about the rank-order of the processes in the new communicator.  

  // Create CDHandle for multiple tasks (MPI rank or threads)

  //CheckMailBox();


//  Sync();
  uint64_t sys_bit_vec = SetSystemBitVector(error_name_mask, error_loc_mask);
  int new_size = (int)((double)node_id().size() / num_children);
  int new_color=0, new_task = 0;
  int err=0;

//  ColorT new_comm = 0;
//  NodeID new_node_id(new_comm, INVALID_TASK_ID, INVALID_HEAD_ID, new_size);
  NodeID new_node_id(node_id());

//  CD_DEBUG("[Before] old: %s, new: %s\n", node_id_.GetString().c_str(), new_node_id.GetString().c_str());
  bool is_reuse = ptr_cd_->CheckToReuseCD(name);
  if(num_children > 1) {
    err = SplitCD(node_id().task_in_color(), node_id().size(), num_children, new_color, new_task);
    new_node_id = GenNewNodeID(color(), new_color, new_task, SelectHead(new_size), is_reuse);
//    assert(new_size == new_node_id.size());
  }
  else if(num_children == 1) {
    new_node_id = GenNewNodeID(SelectHead(task_size()), node_id(), true /* is_reuse */);
  }
  else {
    ERROR_MESSAGE("Number of children to create is wrong.\n");
  }

  // Generate CDID
  CDNameT new_cd_name(ptr_cd_->GetCDName(), num_children, new_color, name);
  CD_DEBUG("%s #: %u (%s) Mode:%d, Lv:%d, Name:%s, RemoteEntries:%zu\n", 
      name, num_children, node_id().GetString().c_str(), ptr_cd()->cd_exec_mode_, 
      ptr_cd()->level(), new_cd_name.GetString().c_str(),
      ptr_cd_->remote_entry_directory_map_.size());

  // Sync buffer with file to prevent important metadata 
  // or preservation file at buffer in parent level from being lost.
  ptr_cd_->SyncFile();

  // Make a superset of CD preservation entries
  if(is_reuse == false) {
    CollectHeadInfoAndEntry(new_node_id); 
  } // otherwise, it will be done later.
  else {
    CD_DEBUG("Skip CollectHeadInfoAndEntry %s\n", GetName());
//    if(myTaskID == 5)
//      printf("first time! %s\n", name);
  }
  // Then children CD get new MPI rank ID. (task ID) I think level&taskID should be also pair.
  CD::CDInternalErrT internal_err;
//  CD::CDInternalErrrT err = kOK;
  new_cd_handle = ptr_cd_->Create(this, name, CDID(new_cd_name, new_node_id), 
                                            static_cast<CDType>(cd_type), sys_bit_vec, &internal_err);


  CDPath::GetCDPath()->push_back(new_cd_handle);

  if(err<0) {
    ERROR_MESSAGE("CDHandle::Create failed.\n"); assert(0);
  }


#else

  CDHandle *new_cd_handle = Create(name, cd_type, error_name_mask, error_loc_mask, error );

#endif

  end_clk = CD_CLOCK();
  const double elapsed = end_clk - begin_clk;
  cd::phaseTree.current_->profile_.RecordCreate(elapsed);
//  create_elapsed_time += end_clk - begin_clk;
//  create_elapsed_smpl += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->create_elapsed_time_ += end_clk - begin_clk;
//#endif
  PRINT_MPI("** [Create] %s at level %u \n", 
            new_cd_handle->ptr_cd_->name_.c_str(), new_cd_handle->level());
  }
//cdcreate2_label:
  CDEpilogue();

  return new_cd_handle;
}


// Collective
CDHandle *CDHandle::Create(uint32_t color, 
                           uint32_t task_in_color, 
                           uint32_t num_children, 
                           const char *name, 
                           int cd_type, 
                           uint32_t error_name_mask, 
                           uint32_t error_loc_mask, 
                           CDErrT *error )
{
  CDPrologue();
  CDHandle *new_cd_handle = this;
  if (cd::dont_cdop == false) { // goto cdcreate3_label;
  TUNE_DEBUG("[Real %s %s lv:%u phase:%d\n", __func__, name, level(), phase()); STOPHANDLE;
#if CD_MPI_ENABLED

  uint64_t sys_bit_vec = SetSystemBitVector(error_name_mask, error_loc_mask);
  int err=0;

  bool is_reuse = ptr_cd_->CheckToReuseCD(name);
//  ColorT new_comm;
//  NodeID new_node_id(new_comm, INVALID_TASK_ID, INVALID_HEAD_ID, num_children);
  NodeID new_node_id = GenNewNodeID(this->color(), color, task_in_color, SelectHead(task_size()/num_children), is_reuse);

  // Generate CDID
  CDNameT new_cd_name(ptr_cd_->GetCDName(), num_children, color, name);

  // Sync buffer with file to prevent important metadata 
  // or preservation file at buffer in parent level from being lost.
  ptr_cd_->SyncFile();

  // Make a superset
  if(is_reuse == false) {
    CollectHeadInfoAndEntry(new_node_id); 
  } // otherwise, it will be done later.
  else {
    CD_DEBUG("Skip CollectHeadInfoAndEntry %s\n", GetName());
  }
  // Then children CD get new MPI rank ID. (task ID) I think level&taskID should be also pair.
  CD::CDInternalErrT internal_err;
  new_cd_handle = ptr_cd_->Create(this, name, CDID(new_cd_name, new_node_id), 
                                            static_cast<CDType>(cd_type), sys_bit_vec, &internal_err);
  
  CDPath::GetCDPath()->push_back(new_cd_handle);

  if(err<0) {
    ERROR_MESSAGE("CDHandle::Create failed.\n"); assert(0);
  }


#else
  CDHandle *new_cd_handle = Create(name, cd_type, error_name_mask, error_loc_mask, error );
#endif

  end_clk = CD_CLOCK();
  const double elapsed = end_clk - begin_clk;
  cd::phaseTree.current_->profile_.RecordCreate(elapsed);
//  create_elapsed_time += end_clk - begin_clk;
//  create_elapsed_smpl += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->create_elapsed_time_ += end_clk - begin_clk;
//#endif
  PRINT_MPI("** [Create] %s at level %u \n", 
            new_cd_handle->ptr_cd_->name_.c_str(), new_cd_handle->level());
  } 
//cdcreate3_label:
  CDEpilogue();
  return new_cd_handle;
}


CDHandle *CDHandle::CreateAndBegin(uint32_t num_children, 
                                   const char *name, 
                                   int cd_type, 
                                   uint32_t error_name_mask, 
                                   uint32_t error_loc_mask, 
                                   CDErrT *error )
{
  CDPrologue();
  CDHandle *new_cdh = this;
  if (cd::dont_cdop == false) { //goto cdcreate4_label;
  TUNE_DEBUG("[Real %s %s lv:%u phase:%d\n", __func__, name, level(), phase()); STOPHANDLE;
  new_cdh = Create(num_children, name, static_cast<CDType>(cd_type), error_name_mask, error_loc_mask, error);
  CD_CLOCK_T clk = CD_CLOCK();
  const double elapsed = clk - begin_clk;
  cd::phaseTree.current_->profile_.RecordCreate(elapsed);
//  create_elapsed_time += clk - begin_clk;
//  create_elapsed_smpl += clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->create_elapsed_time_ += end_clk - begin_clk;
//#endif
  bool need_sync = just_reexecuted;

  new_cdh->Begin(name, false);

  const bool is_reexec = (failed_phase != HEALTHY);
  cd::phaseTree.current_->profile_.RecordBegin(is_reexec, need_sync);
//  end_clk = CD_CLOCK();
//  begin_elapsed_time += end_clk - begin_clk;
//  begin_elapsed_smpl += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->begin_elapsed_time_ += end_clk - begin_clk;
//#endif
  PRINT_MPI("** [Create] %s at level %u \n", 
            new_cdh->ptr_cd_->name_.c_str(), new_cdh->level());
  }
//cdcreate4_end:
  CDEpilogue();
  return new_cdh;
}


CDErrT CDHandle::Destroy(bool collective) 
{
  CDPrologue();
  if (cd::dont_cdop) goto cddestroy_label;
  CDErrT err;
  TUNE_DEBUG("[Real %s] %u %d\n", __func__, level(), phase());
//  printf("[Real %s] %u %d\n", __func__, level(), phase()); STOPHANDLE;
  uint32_t phase = ptr_cd_->phase();//phase();
  {
  std::string label(GetLabel());

  CD_DEBUG("%s %s at level %u (reexecInfo %d (%u))\n", ptr_cd_->name_.c_str(), ptr_cd_->name_.c_str(), 
                                                       level(), need_reexec(), *CD::rollback_point_);
  PRINT_MPI("** [Destroy] %s %s at level %u (reexecInfo %d (%u))\n", ptr_cd_->name_.c_str(), ptr_cd_->label_.c_str(), 
                                                       level(), need_reexec(), *CD::rollback_point_);
  
  err = InternalDestroy(collective);

  end_clk = CD_CLOCK();
  cd::phaseTree.current_->profile_.RecordDestory(end_clk - begin_clk);
//  destroy_elapsed_time += end_clk - begin_clk;
//  destroy_elapsed_smpl += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  auto it = profMap.find(phase);
//  if(it != profMap.end())
//    it->second->destroy_elapsed_time_ += end_clk - begin_clk;
//#endif
  }
cddestroy_label:
  CDEpilogue();

  return err;
}

CDErrT CDHandle::InternalDestroy(bool collective, bool need_destroy)
{
  CDErrT err;
 
  if ( collective ) {
    //  Sync();
  }

  // It could be a local execution or a remote one.
  // These two should be atomic 

  if(this != CDPath::GetRootCD()) { 

    // Mark that this is destroyed
    // this->parent_->is_child_destroyed = true;

  } 
  else {

  }

  if(CDPath::GetCDPath()->size() > 1) {
    CDPath::GetParentCD()->RemoveChild(this);
  }

#if CD_PROFILER_ENABLED
  CD_DEBUG("Calling finish profiler\n");
  
  profiler_->Delete();

  //if(ptr_cd()->cd_exec_mode_ == 0) { 
//    profiler_->FinishProfile();
  //}
#endif

  assert(CDPath::GetCDPath()->size()>0);
  assert(CDPath::GetCDPath()->back() != NULL);

  err = ptr_cd()->Destroy(collective, need_destroy);

  if(need_destroy) {
    delete CDPath::GetCDPath()->back();
  }
//  getchar();
  CDPath::GetCDPath()->pop_back();

   
  return err;
}

//StackEntry *new_stack_entry;
//void PreserveStack(StackEntry *stack) 
//{
//  printf("### after rollback\n");
//  memcpy(stack->sp_, stack->preserved_stack_, stack->stack_size_);
//}

//CDErrT CDHandle::Begin(const char *label,
//                       bool collective,
//                       const uint64_t &sys_err_vec
//                      )
//{
//  CD_ASSERT(ptr_cd_);
//  if(ctxt_prv_mode() == kExcludeStack) 
//    setjmp(*jmp_buffer());
//  else { 
//    new_stack_entry = new StackEntry;
////    getcontext(ctxt());
//    GetStackPtr(&new_stack_entry->sp_);
//    printf("now stack:%p - %p\n", new_stack_entry->sp_, sp_init);
//    int32_t new_stack_size = (char *)sp_init - (char *)new_stack_entry->sp_;
//    printf("stacksize:%d\n", new_stack_size);
//    if(new_stack_entry->stack_size_ < new_stack_size) {
//      new_stack_entry->stack_size_ = new_stack_size;
//      
//      if(new_stack_entry->preserved_stack_ != NULL) 
//        free(new_stack_entry->preserved_stack_);
//      new_stack_entry->preserved_stack_ = malloc(new_stack_entry->stack_size_);
//    } else {
//      new_stack_entry->stack_size_ = new_stack_size;
//      if(new_stack_entry->preserved_stack_ == NULL)
//        new_stack_entry->preserved_stack_ = malloc(new_stack_entry->stack_size_);
//    }
//    printf("stack cpy:%p %p %u\n", new_stack_entry->preserved_stack_, new_stack_entry->sp_, new_stack_entry->stack_size_);
//    memcpy(new_stack_entry->preserved_stack_, new_stack_entry->sp_, new_stack_entry->stack_size_);
//    ptr_cd_->stack_entry_ = new_stack_entry;
//    getcontext(ctxt());
//
//    ctxt()->uc_stack.ss_sp = new_stack_entry->shadow_stack_;
//    ctxt()->uc_stack.ss_size = STACK_SIZE;
//    ctxt()->uc_stack.ss_flags = 0;
//    makecontext(ctxt(), (void (*) (void))PreserveStack, 1, ptr_cd_->stack_entry_);
//  }
//
//  if(ptr_cd()->cd_exec_mode_ == kReexecution) {
//    TUNE_DEBUG("Reexecution %s at %u\n", label, ptr_cd()->level());
//    printf("Reexecution %s at %u\n", label, ptr_cd()->level());
//  }
//  TUNE_DEBUG("[Real %s %s lv:%u phase:%d]\n", __func__, label, level(), phase());  
////         CommitPreserveBuff();
//  common::CDErrT ret = InternalBegin(label, collective, sys_err_vec);
//  TUNE_DEBUG("[Real %s %s lv:%u phase:%d]\n", __func__, label, level(), phase());  
//  return ret;
//}

//inline
//CDErrT CDHandle::Begin(bool collective, const char *label, const uint64_t &sys_error_vec)
//{
//  assert(ptr_cd_);
//  if(ptr_cd_->ctxt_prv_mode_ == kExcludeStack) 
//    setjmp(ptr_cd_->jmp_buffer_);
//  else 
//    getcontext(&(ptr_cd_->ctxt_));
//
//  CommitPreserveBuff();
//  return InternalBegin(collective, label, sys_error_vec);
//}

CDErrT CDHandle::Begin(const char *label, bool collective, const uint64_t &sys_error_vec)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdbegin_label;
  CD_ASSERT_STR(ptr_cd_ != 0, "[%d at lv %u] label %s of %p\n", myTaskID, level(), label, ptr_cd_);

  // sys_error_vec is zero, then do not update it in Begin.
  if(sys_error_vec != 0) 
    ptr_cd_->sys_detect_bit_vector_ = sys_error_vec;

  bool need_sync = just_reexecuted;

  CDErrT err = ptr_cd_->Begin(label, collective);

//#if CD_PROFILER_ENABLED
//  // Profile-related
//  profiler_->StartProfile();
//#endif
//
//  end_clk = CD_CLOCK();
//  begin_elapsed_time += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->begin_elapsed_time_ += end_clk - begin_clk;
//#endif
  //CDEpilogue(phaseTree.current_->profile_.RecordBegin());
  const bool is_reexec = (failed_phase != HEALTHY);
  if(is_reexec) {
    const uint64_t curr_phase = ptr_cd_->phase();
    const uint64_t curr_seqID = cd::phaseTree.current_->seq_end_;
    const uint64_t curr_begID = cd::phaseTree.current_->seq_begin_;
    CD_DEBUG("[REEXEC]%s (%s) " 
             "fphase:%ld<-%lu, seqID:%ld<-%lu(beg:%lu)\n", 
             ptr_cd_->cd_id_.GetStringID().c_str(), label,
             cd::failed_phase, curr_phase, cd::failed_seqID, curr_seqID, curr_begID);
    PRINT_MPI("** [REEXEC] %s %s (%s) " 
               "fphase:%ld<-%lu, seqID:%ld<-%lu(beg:%lu)\n", 
               ptr_cd_->name_.c_str(), ptr_cd_->cd_id_.GetStringID().c_str(), label,
               cd::failed_phase, curr_phase, cd::failed_seqID, curr_seqID, curr_begID);
  } else {
    const uint64_t curr_phase = ptr_cd_->phase();
    const uint64_t curr_seqID = cd::phaseTree.current_->seq_end_;
    const uint64_t curr_begID = cd::phaseTree.current_->seq_begin_;

    PRINT_MPI("** [Begin] %s %s (%s) " 
             "fphase:%ld<-%lu, (%lu~%lu->%lu)\n", 
             ptr_cd_->name_.c_str(), ptr_cd_->cd_id_.GetStringID().c_str(), label,
             cd::failed_phase, curr_phase, curr_begID, curr_seqID, cd::failed_seqID);
//    if (cd::myTaskID == 0) 
//      printf("** [Begin] %s %s (%s) " 
//             "fphase:%ld==%lu, (%lu~%lu->%lu)\n", 
//             ptr_cd_->name_.c_str(), ptr_cd_->cd_id_.GetStringID().c_str(), label,
//             cd::failed_phase, curr_phase, curr_begID, curr_seqID, cd::failed_seqID);
  }
  const bool is_reexecution = (GetExecMode() == kReexecution);
  cd::phaseTree.current_->profile_.RecordBegin(is_reexec, need_sync);
cdbegin_label:
  CDEpilogue();
#if CD_LIBC_LOGGING
  app_side = true;
  logger::disabled = false;
#endif
  return err;
}

CDErrT CDHandle::Complete(bool update_preservations, bool collective)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdcompl_label;
  TUNE_DEBUG("[Real %s lv:%u phase:%d]\n", __func__, level(), phase()); STOPHANDLE;
  PRINT_MPI("** [%s] %s %s at level %u (reexecInfo %d (%u))\n", __func__, ptr_cd_->name_.c_str(), ptr_cd_->label_.c_str(), 
                                                                      level(), need_reexec(), *CD::rollback_point_);
  CD_DEBUG("%s %s at level %u (reexecInfo %d (%u))\n", ptr_cd_->name_.c_str(), ptr_cd_->label_.c_str(), 
                                                                      level(), need_reexec(), *CD::rollback_point_);

//  printf("[%s] %s %s at level %u (reexecInfo %d (%u))\n", __func__, ptr_cd_->name_.c_str(), ptr_cd_->name_.c_str(), 
//                                                                      level(), need_reexec(), *CD::rollback_point_);
  if (cd::dont_cdop) assert(0);
  // Call internal Complete routine
  assert(ptr_cd_ != 0);
  CD_ASSERT_STR(GetCurrentCD()->ptr_cd() == ptr_cd_, "Level %u is not complete before completing %u\n",
        GetCurrentCD()->level(), level() );

  // After Complete(), phaseTree.current_ changes to its parent
  PhaseNode *current = cd::phaseTree.current_;


  // This part may be a bit tricky. failed_phase is reset to HEALTHY
  // in the case that current phase of CD is the end of the last failed point.
  // Otherwise failed_phase is not healthy and increment reexec_
  const bool is_reexec = (failed_phase != HEALTHY);
  if(is_reexec && IsHead() && myTaskID == 0) {
    const uint64_t curr_phase = ptr_cd_->phase();
    const uint64_t curr_seqID = cd::phaseTree.current_->seq_end_;
    const uint64_t curr_begID = cd::phaseTree.current_->seq_begin_;
    printf(">>> [%s %4d] %s (%s) " 
             "fphase:%ld==%lu(cur), seqID:%ld==%lu(beg:%lu)\n", 
             __func__,
             cd::myTaskID, ptr_cd_->cd_id_.GetStringID().c_str(), GetLabel(),
             cd::failed_phase, curr_phase, cd::failed_seqID, curr_seqID, curr_begID);
  }
  // Profile will be acquired inside CD::Complete()
  CDErrT ret = ptr_cd_->Complete(update_preservations, collective);

//#if CD_PROFILER_ENABLED
//  // Profile-related
//  profiler_->FinishProfile();
//#endif
//
//  end_clk = CD_CLOCK();
//  compl_elapsed_time += end_clk - begin_clk;
//#if CD_PROFILER_ENABLED
//  profMap[phase()]->compl_elapsed_time_ += end_clk - begin_clk;
//#endif

//  // This part may be a bit tricky. failed_phase is reset to HEALTHY
//  // in the case that current phase of CD is the end of the last failed point.
//  // Otherwise failed_phase is not healthy and increment reexec_
//  bool is_reexec = (failed_phase != HEALTHY);
  //if(myTaskID == 0) printf("is reexec:%d\n", is_reexec);
  current->profile_.RecordComplete(is_reexec, level() == 1);
cdcompl_label:
  CDEpilogue();
  //CDEpilogue();

  return ret;
}

CDErrT CDHandle::Advance(bool collective)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdadvance_label;

  CDErrT ret = ptr_cd_->Advance(collective);

  end_clk = CD_CLOCK();
  advance_elapsed_time += end_clk - begin_clk;
  advance_elapsed_smpl += end_clk - begin_clk;
#if CD_PROFILER_ENABLED
  profMap[phase()]->advance_elapsed_time_ += end_clk - begin_clk;
#endif
cdadvance_label:
  CDEpilogue();
  return ret;
};

CDErrT CDHandle::Preserve(void *data_ptr, 
                          uint64_t len, 
                          uint32_t preserve_mask, 
                          const char *my_name, 
                          const char *ref_name, 
                          uint64_t ref_offset, 
                          const RegenObject *regen_object, 
                          PreserveUseT data_usage)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdprv1_label;

  /// Preserve meta-data
  /// Accumulated volume of data to be preserved for Sequential CDs. 
  /// It will be averaged out with the number of seq. CDs.
  assert(ptr_cd_ != 0);
  assert(len > 0);
  bool is_reexec = (GetExecMode() == kReexecution);
  CDErrT err;
  bool is_active_leaf = (GetCurrentCD() == this);
  // is_active_leaf is false AND IsReexec() true --> jump
  //if(is_reexec || is_active_leaf || IsReexec() == false) 
  if(1)
  {
    std::string entry_name(my_name);
    CD_DEBUG("@@@ %s (%18s) (%12s) (%12s) (Prv: %s, %s, %s)\n", 
                (is_reexec)? "Restore" : "Preserv",  my_name, GetCurrentCD()->GetLabel(), GetLabel(),
                CHECK_PRV_TYPE(preserve_mask,kRef)? "Refr":"Copy", 
                (is_active_leaf)? "ACTIVE":"INACTIVE",
                (IsReexec())? "REEX":"EXEC");
    err = ptr_cd_->Preserve(data_ptr, len, (CDPrvType)preserve_mask, 
                                   entry_name, ref_name, ref_offset, 
                                   regen_object, data_usage);
#if CD_ERROR_INJECTION_ENABLED
    if(memory_error_injector_ != NULL) {
      memory_error_injector_->PushRange(data_ptr, len/sizeof(int), sizeof(int), my_name);
      memory_error_injector_->Inject();
    }
#endif
  
    uint32_t phase = this->phase();
    cd::phaseNodeCache[phase]->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
    //cd::phaseTree.current_->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
  } 
  else {
    CD_DEBUG("@@@ SKIP IT (%18s) (%12s) (%12s) (Prv: %s, %s, %s to %s %ld)\n", 
                my_name, GetCurrentCD()->GetLabel(), GetLabel(),
                CHECK_PRV_TYPE(preserve_mask,kRef)? "Refr":"Copy", 
                (is_active_leaf)? "ACTIVE":"INACTIVE",
                (IsReexec())? "REEX":"EXEC",
                cd::phaseNodeCache[cd::failed_phase]->label_.c_str(), cd::failed_seqID);
  }
cdprv1_label:
  CDEpilogue();
  return CHECK_PRV_TYPE(preserve_mask, kRef)? (CDErrT)0 : (CDErrT)len;
}

CDErrT CDHandle::Preserve(Serializable &serdes,                           
                          uint32_t preserve_mask, 
                          const char *my_name, 
                          const char *ref_name, 
                          uint64_t ref_offset, 
                          const RegenObject *regen_object, 
                          PreserveUseT data_usage)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdprv2_label;

  /// Preserve meta-data
  /// Accumulated volume of data to be preserved for Sequential CDs. 
  /// It will be averaged out with the number of seq. CDs.
  assert(ptr_cd_ != 0); 
  uint64_t len = 0;
  bool is_reexec = (GetExecMode() == kReexecution);
  CDErrT err;
  bool is_active_leaf = (GetCurrentCD() == this);
  // is_active_leaf is false AND IsReexec() true --> jump
  //if(is_reexec || IsReexec() == false) 
  //if(is_reexec || is_active_leaf || IsReexec() == false) 
  if(1)
  {
    std::string entry_name(my_name);
    //if(CHECK_PRV_TYPE(preserve_mask,kRef) == false || is_reexec) 
    CD_DEBUG("@@@ %s (%18s) (%12s) (%12s) (Prv: %s, %s, %s)\n", 
                (is_reexec)? "Restore" : "Preserv",  my_name, GetCurrentCD()->GetLabel(), GetLabel(),
                CHECK_PRV_TYPE(preserve_mask,kRef)? "Refr":"Copy", 
                (is_active_leaf)? "ACTIVE":"INACTIVE",
                (IsReexec())? "REEX":"EXEC");
    err = ptr_cd_->Preserve((void *)&serdes, len, (CDPrvType)(kSerdes | preserve_mask), 
                                   entry_name, ref_name, ref_offset, 
                                   regen_object, data_usage);
    //assert(len > 0);
  
    uint32_t phase = this->phase();
    cd::phaseNodeCache[phase]->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
    //cd::phaseTree.current_->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
  } 
  else {
    CD_DEBUG("@@@ SKIP IT (%18s) (%12s) (%12s) (Prv: %s, %s, %s to %s %ld)\n", 
                my_name, GetCurrentCD()->GetLabel(), GetLabel(),
                CHECK_PRV_TYPE(preserve_mask,kRef)? "Refr":"Copy", 
                (is_active_leaf)? "ACTIVE":"INACTIVE",
                (IsReexec())? "REEX":"EXEC",
                cd::phaseNodeCache[cd::failed_phase]->label_.c_str(), cd::failed_seqID);
  }
  
cdprv2_label:
  CDEpilogue();
  return CHECK_PRV_TYPE(preserve_mask, kRef)? (CDErrT)0 : (CDErrT)len;
}

CDErrT CDHandle::Preserve(CDEvent &cd_event, 
                          void *data_ptr, 
                          uint64_t len, 
                          uint32_t preserve_mask, 
                          const char *my_name, 
                          const char *ref_name, 
                          uint64_t ref_offset, 
                          const RegenObject *regen_object, 
                          PreserveUseT data_usage )
{
  CDPrologue();
  if (cd::dont_cdop) goto cdprv3_label;

  assert(ptr_cd_ != 0);
  assert(len > 0);
//#if CD_PROFILER_ENABLED
//  bool is_reexec = (GetExecMode() == kReexecution);
//#endif
  bool is_reexec = (GetExecMode() == kReexecution);
  CDErrT err;
  {
  std::string entry_name(my_name);
  // TODO CDEvent object need to be handled separately, 
  // this is essentially shared object among multiple nodes.
  err = ptr_cd_->Preserve(data_ptr, len, (CDPrvType)preserve_mask, 
                              entry_name, ref_name, ref_offset,  
                              regen_object, data_usage);

#if CD_ERROR_INJECTION_ENABLED
  if(memory_error_injector_ != NULL) {
    memory_error_injector_->PushRange(data_ptr, len/sizeof(int), sizeof(int), my_name);
    memory_error_injector_->Inject();
  }
#endif

//#if CD_PROFILER_ENABLED
//  if(is_reexecution) {
//    if(CHECK_PRV_TYPE(preserve_mask,kCopy)) {
//      profiler_->RecordProfile(PRV_COPY_DATA, len);
//    }
//    else if(CHECK_PRV_TYPE(preserve_mask,kRef)) {
//      profiler_->RecordProfile(PRV_REF_DATA, len);
//    }
//  }
//#endif
//
//  end_clk = CD_CLOCK();
//  double elapsed = end_clk - begin_clk;
//  uint32_t phase = this->phase();
//  prv_elapsed_time += elapsed;
//#if CD_PROFILER_ENABLED
//  if(is_reexecution) {
//    profMap[phase]->prv_elapsed_time_ += elapsed;
//  } else {
//    profMap[phase]->rst_elapsed_time_ += elapsed;
//  }
//#endif
  //CDEpilogue();
  
  uint32_t phase = this->phase();
  cd::phaseNodeCache[phase]->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
  //cd::phaseTree.current_->profile_.RecordData(entry_name, len, preserve_mask, is_reexec);
  }
cdprv3_label:
  CDEpilogue();
  return err;
}

CDErrT CDHandle::CDAssert(bool test, const SysErrT *error_to_report)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdassert_label;
//  CD_DEBUG("Assert : %d at level %u\n", ptr_cd()->cd_exec_mode_, ptr_cd()->level());

  assert(ptr_cd_ != 0);
  CDErrT err = common::kOK;

#if CD_PROFILER_ENABLED
//    if(!test_true) {
//      profiler_->Delete();
//    }
#endif


#if CD_ERROR_INJECTION_ENABLED
  if(cd_error_injector_ != NULL) {
    if(cd_error_injector_->Inject()) {
      test = false;
      err = kAppError;
    }
  }
#endif

  CD::CDInternalErrT internal_err = ptr_cd_->Assert(test);
  if(internal_err == CD::CDInternalErrT::kErrorReported)
    err = kAppError;

cdassert_label:
  CDEpilogue();
  return err;
}

CDErrT CDHandle::CDAssertFail (bool test_true, const SysErrT *error_to_report)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdassert2_label;

  if( IsHead() ) {

  }
  else {
    // It is at remote node so do something for that.
  }

cdassert2_label:
  CDEpilogue();
  return common::kOK;
}

CDErrT CDHandle::CDAssertNotify(bool test_true, const SysErrT *error_to_report)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdassert3_label;
  if( IsHead() ) {
    // STUB
  }
  else {
    // It is at remote node so do something for that.
  }

cdassert3_label:
  CDEpilogue();
  return common::kOK;
}

std::vector<SysErrT> CDHandle::Detect(CDErrT *err_ret_val)
{
  CDPrologue();
  std::vector<SysErrT> ret_prepare;
  if (cd::dont_cdop) goto cddetect_label;
  CD_DEBUG("[%s] check mode : %d at %s %s level %u (reexecInfo %d (%u))\n", 
      ptr_cd_->cd_id_.GetString().c_str(), ptr_cd()->cd_exec_mode_, 
      ptr_cd_->name_.c_str(), ptr_cd_->name_.c_str(), 
      level(), need_reexec(), *CD::rollback_point_);

  CDErrT err = common::kOK;
  uint32_t rollback_point = INVALID_ROLLBACK_POINT;

  int err_desc = (int)ptr_cd_->Detect(rollback_point);
#if CD_ERROR_INJECTION_ENABLED
  err_desc = CheckErrorOccurred(rollback_point);
#endif

#if CD_MPI_ENABLED

  CD_DEBUG("[%s] rollback %u -> %u (%d == %d)\n", 
      ptr_cd_->cd_id_.GetStringID().c_str(), 
      level(), rollback_point, err_desc, CD::CDInternalErrT::kErrorReported);

  if(err_desc == CD::CDInternalErrT::kErrorReported) {
    err = kError;
    // FIXME
    CD_DEBUG("[%d] #### Error Injected:%x Rollback Level #%u (%s %s) ###\n", myTaskID, err_desc,
             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 
    printf("[%d] #### Error Injected:%x Rollback Level #%u (%s %s) ###\n", myTaskID, err_desc,
             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 

    CDHandle *rb_cdh = CDPath::GetCDLevel(rollback_point);
    assert(rb_cdh != NULL);

#if 0
    if(rb_cdh->task_size() > 1) {
      rb_cdh->SetMailBox(kErrorOccurred);
    } 

    // If there is a single task in a CD, everybody is the head in that level.
    if(task_size() > 1) {
      if(IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      }
    } else {

      if((rb_cdh->task_size() == 1) || rb_cdh->IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      } else {
        // If the level of rollback_point has more tasks than current task,
        // let head inform the current task about escalation.
        ptr_cd_->SetRollbackPoint(level(), false);
      }
    }
#else
//-------------------------------------------------
    if(task_size() > 1) {
      // If current level's task_size is larger than 1,
      // upper-level task_size is always larger than 1.
      rb_cdh->SetMailBox(kErrorOccurred);
      if(IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      } else { // FIXME
        ptr_cd_->SetRollbackPoint(rollback_point, true);
        ptr_cd_->SetRollbackPoint(rollback_point, false);
//        ptr_cd_->SetRollbackPoint(level(), false);
      }
    } else {
      // It is possible that upper-level task_size is larger than 1, 
      // even though current level's task_size is 1.
      if(rb_cdh->task_size() > 1) {
        rb_cdh->SetMailBox(kErrorOccurred);
        if(rb_cdh->IsHead()) {
          ptr_cd_->SetRollbackPoint(rollback_point, false);
        } else {
          // If the level of rollback_point has more tasks than current task,
          // let head inform the current task about escalation.
          ptr_cd_->SetRollbackPoint(level(), false);
        }
      } else {
        // task_size at current level is 1, and
        // task_size at rollback level is also 1.
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      }
    }
#endif
    err = kAppError;
    
  } // kErrorReported ends
  else {
#if CD_ERROR_INJECTION_ENABLED
/*
    CD_DEBUG("EIE Before\n");
    CD_DEBUG("Is it NULL? %p, recreated? %d, reexecuted? %d\n", cd_error_injector_, ptr_cd_->recreated(), ptr_cd_->reexecuted());
    if(cd_error_injector_ != NULL && ptr_cd_ != NULL) {
      CD_DEBUG("EIE It is after : reexec # : %d, exec mode : %d at level #%u\n", ptr_cd_->num_reexecution_, GetExecMode(), level());
      CD_DEBUG("recreated? %d, recreated? %d\n", ptr_cd_->recreated(), ptr_cd_->reexecuted());
      if(cd_error_injector_->Inject() && ptr_cd_->recreated() == false && ptr_cd_->reexecuted() == false) {
        CD_DEBUG("EIE Reached SetMailBox. recreated? %d, reexecuted? %d\n", ptr_cd_->recreated(), ptr_cd_->reexecuted());
        SetMailBox(kErrorOccurred);
        err = kAppError;

        
        PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
        CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());

      }
      else {
        
        PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
        CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());
        CheckMailBox();

      }
    }
    else {
      
//      PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
//      CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());
//      CheckMailBox();
    }
*/    

#endif

//    PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
    CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id().GetString().c_str());
  }

  CheckMailBox();


#else // CD_MPI_ENABLED ends
  if(err_desc == CD::CDInternalErrT::kErrorReported) {
    // FIXME
//    printf("### Error Injected.");
    PRINT_MPI(">> Error Injected >>  Rollback Level #%u (%s %s) ###\n", 
             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 
    CD_DEBUG(">> Error Injected >>  Rollback Level #%u (%s %s) ###\n", 
             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 
    ptr_cd_->SetRollbackPoint(rollback_point, false);
  } else {
//    printf("err:  %d\n", err_desc);
  }
#endif

  if(err_ret_val != NULL)
    *err_ret_val = err;
#if CD_DEBUG_DEST == 1
  fflush(cdout);
#endif
cddetect_label:
  CDEpilogue();
  return ret_prepare;

}

////////////////////////////////////////////////////////////////////
//
// Internal Ends
//
////////////////////////////////////////////////////////////////////


char *CDHandle::GetName(void) const
{
  if(ptr_cd_ != NULL) {
    return const_cast<char*>(ptr_cd_->name_.c_str());  
  }
  else {
    return NULL;
  }
}

char *CDHandle::GetLabel(void) const
{
  if(ptr_cd_ != NULL) {
    return const_cast<char*>(ptr_cd_->label_.c_str());  
  }
  else {
    return NULL;
  }
}

std::string &CDHandle::name(void)                { return ptr_cd_->name_; }
std::string &CDHandle::label(void)               { return ptr_cd_->label_; }
CDID     &CDHandle::GetCDID(void)             { return ptr_cd_->cd_id_; }
CDNameT  &CDHandle::GetCDName(void)           { return ptr_cd_->cd_id_.cd_name_; }
NodeID   &CDHandle::node_id(void)             { return ptr_cd_->cd_id_.node_id_; }
CD       *CDHandle::ptr_cd(void)        const { return ptr_cd_; }
void      CDHandle::SetCD(CD *ptr_cd)         { ptr_cd_=ptr_cd; }
uint32_t  CDHandle::level(void)         const { return ptr_cd_->GetCDName().level(); }
uint32_t  CDHandle::phase(void)         const { return ptr_cd_->GetCDName().phase(); }
uint32_t  CDHandle::rank_in_level(void) const { return ptr_cd_->GetCDName().rank_in_level(); }
uint32_t  CDHandle::sibling_num(void)   const { return ptr_cd_->GetCDName().size(); }
ColorT   &CDHandle::color(void)               { return ptr_cd_->cd_id_.node_id_.color_; }
int       CDHandle::task_in_color(void) const { return ptr_cd_->cd_id_.node_id_.task_in_color_; }
int       CDHandle::head(void)          const { return ptr_cd_->cd_id_.node_id_.head_; }
int       CDHandle::task_size(void)     const { return ptr_cd_->cd_id_.node_id_.size_; }
GroupT   &CDHandle::group(void)               { return ptr_cd_->cd_id_.node_id_.task_group_; }
int       CDHandle::GetExecMode(void)   const { return ptr_cd_->cd_exec_mode_; }
int       CDHandle::GetSeqID(void)      const { return ptr_cd_->GetCDID().sequential_id(); }
CDHandle *CDHandle::GetParent(void)     const { return CDPath::GetParentCD(ptr_cd_->GetCDName().level()); }
bool      CDHandle::IsHead(void)        const { return ptr_cd_->cd_id_.node_id_.IsHead(); } // FIXME
// FIXME
// For now task_id_==0 is always Head which is not good!
// head is always task id 0 for now

bool CDHandle::operator==(CDHandle &other) 
{
  bool ptr_cd = (other.ptr_cd() == this->ptr_cd());
  bool color  = (other.color()  == this->color());
  bool task   = (other.task_in_color()   == task_in_color());
  bool size   = (other.task_size()   == task_size());
  return (ptr_cd && color && task && size);
}

CDErrT CDHandle::Stop()
{ return ptr_cd_->Stop(); }

CDErrT CDHandle::AddChild(CDHandle *cd_child)
{
  CDErrT err=INITIAL_ERR_VAL;
  ptr_cd_->AddChild(cd_child);
  return err;
}

CDErrT CDHandle::RemoveChild(CDHandle *cd_child)  
{
  CDErrT err=INITIAL_ERR_VAL;
  ptr_cd_->RemoveChild(cd_child);
  return err;
}

#if 0
CDErrT CDHandle::CDAssert (bool test, const SysErrT *error_to_report)
{
  CDPrologue();
//  CD_DEBUG("Assert : %d at level %u\n", ptr_cd()->cd_exec_mode_, ptr_cd()->level());

  assert(ptr_cd_ != 0);
  CDErrT err = kOK;

#if CD_PROFILER_ENABLED
//    if(!test_true) {
//      profiler_->Delete();
//    }
#endif


#if CD_ERROR_INJECTION_ENABLED
  if(cd_error_injector_ != NULL) {
    if(cd_error_injector_->Inject()) {
      test = false;
      err = kAppError;
    }
  }
#endif

  CD::CDInternalErrT internal_err = ptr_cd_->Assert(test);
  if(internal_err == CD::CDInternalErrT::kErrorReported)
    err = kAppError;

  CDEpilogue();
  return err;
}

CDErrT CDHandle::CDAssertFail (bool test_true, const SysErrT *error_to_report)
{
  CDPrologue();
  if( IsHead() ) {

  }
  else {
    // It is at remote node so do something for that.
  }

  CDEpilogue();
  return kOK;
}

CDErrT CDHandle::CDAssertNotify(bool test_true, const SysErrT *error_to_report)
{
  CDPrologue();
  if( IsHead() ) {
    // STUB
  }
  else {
    // It is at remote node so do something for that.
  }

  CDEpilogue();
  return kOK;
}

std::vector<SysErrT> CDHandle::Detect(CDErrT *err_ret_val)
{
  CDPrologue();
  CD_DEBUG("[%s] check mode : %d at %s %s level %u (reexecInfo %d (%u))\n", 
      ptr_cd_->cd_id_.GetString().c_str(), ptr_cd()->cd_exec_mode_, 
      ptr_cd_->name_.c_str(), ptr_cd_->name_.c_str(), 
      level(), need_reexec(), *CD::rollback_point_);

  std::vector<SysErrT> ret_prepare;
  CDErrT err = kOK;
  uint32_t rollback_point = INVALID_ROLLBACK_POINT;

  int err_desc = (int)ptr_cd_->Detect(rollback_point);
#if CD_ERROR_INJECTION_ENABLED
  err_desc = CheckErrorOccurred(rollback_point);
#endif

#if CD_MPI_ENABLED

  CD_DEBUG("[%s] rollback %u -> %u (%d == %d)\n", 
      ptr_cd_->cd_id_.GetStringID().c_str(), 
      rollback_point, level(), err_desc, CD::CDInternalErrT::kErrorReported);

  if(err_desc == CD::CDInternalErrT::kErrorReported) {
    err = kError;
    // FIXME
    CD_PRINT("### Error Injected. Rollback Level #%u (%s %s) ###\n", 
             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 

    CDHandle *rb_cdh = CDPath::GetCDLevel(rollback_point);
    assert(rb_cdh != NULL);

#if 0
    if(rb_cdh->task_size() > 1) {
      rb_cdh->SetMailBox(kErrorOccurred);
    } 

    // If there is a single task in a CD, everybody is the head in that level.
    if(task_size() > 1) {
      if(IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      }
    } else {

      if((rb_cdh->task_size() == 1) || rb_cdh->IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      } else {
        // If the level of rollback_point has more tasks than current task,
        // let head inform the current task about escalation.
        ptr_cd_->SetRollbackPoint(level(), false);
      }
    }
#else
//-------------------------------------------------
    if(task_size() > 1) {
      // If current level's task_size is larger than 1,
      // upper-level task_size is always larger than 1.
      rb_cdh->SetMailBox(kErrorOccurred);
      if(IsHead()) {
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      } else { // FIXME
//        ptr_cd_->SetRollbackPoint(rollback_point, false);
//        ptr_cd_->SetRollbackPoint(level(), false);
      }
    } else {
      // It is possible that upper-level task_size is larger than 1, 
      // even though current level's task_size is 1.
      if(rb_cdh->task_size() > 1) {
        rb_cdh->SetMailBox(kErrorOccurred);
        if(rb_cdh->IsHead()) {
          ptr_cd_->SetRollbackPoint(rollback_point, false);
        } else {
          // If the level of rollback_point has more tasks than current task,
          // let head inform the current task about escalation.
          ptr_cd_->SetRollbackPoint(level(), false);
        }
      } else {
        // task_size at current level is 1, and
        // task_size at rollback level is also 1.
        ptr_cd_->SetRollbackPoint(rollback_point, false);
      }
    }
#endif
    err = kAppError;
    
  }
  else {
#if CD_ERROR_INJECTION_ENABLED
/*
    CD_DEBUG("EIE Before\n");
    CD_DEBUG("Is it NULL? %p, recreated? %d, reexecuted? %d\n", cd_error_injector_, ptr_cd_->recreated(), ptr_cd_->reexecuted());
    if(cd_error_injector_ != NULL && ptr_cd_ != NULL) {
      CD_DEBUG("EIE It is after : reexec # : %d, exec mode : %d at level #%u\n", ptr_cd_->num_reexecution_, GetExecMode(), level());
      CD_DEBUG("recreated? %d, recreated? %d\n", ptr_cd_->recreated(), ptr_cd_->reexecuted());
      if(cd_error_injector_->Inject() && ptr_cd_->recreated() == false && ptr_cd_->reexecuted() == false) {
        CD_DEBUG("EIE Reached SetMailBox. recreated? %d, reexecuted? %d\n", ptr_cd_->recreated(), ptr_cd_->reexecuted());
        SetMailBox(kErrorOccurred);
        err = kAppError;

        
        PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
        CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());

      }
      else {
        
        PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
        CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());
        CheckMailBox();

      }
    }
    else {
      
//      PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
//      CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id_.GetString().c_str());
//      CheckMailBox();
    }
*/    

#endif

//    PMPI_Win_fence(0, CDPath::GetCoarseCD(this)->ptr_cd()->mailbox_);
    CD_DEBUG("\n\n[Barrier] CDHandle::Detect 1 - %s / %s\n\n", ptr_cd_->GetCDName().GetString().c_str(), node_id().GetString().c_str());
  }

  CheckMailBox();


#else // CD_MPI_ENABLED ends
  if(err_desc == CD::CDInternalErrT::kErrorReported) {
    // FIXME
//    printf("### Error Injected.");
//    printf(" Rollback Level #%u (%s %s) ###\n", 
//             rollback_point, ptr_cd_->cd_id_.GetStringID().c_str(), ptr_cd_->label_.c_str()); 
    ptr_cd_->SetRollbackPoint(rollback_point, false);
  } else {
//    printf("err:  %d\n", err_desc);
  }
#endif

  if(err_ret_val != NULL)
    *err_ret_val = err;
#if CD_DEBUG_DEST == 1
  fflush(cdout);
#endif
  CDEpilogue();
  return ret_prepare;

}
#endif


CDErrT CDHandle::RegisterRecovery (uint32_t error_name_mask, uint32_t error_loc_mask, RecoverObject *recover_object)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdregister1_label;

  if( IsHead() ) {
    // STUB
  }
  else {
    // It is at remote node so do something for that.
  }
cdregister1_label:
  CDEpilogue();
  return common::kOK;
}

CDErrT CDHandle::RegisterDetection (uint32_t system_name_mask, uint32_t system_loc_mask)
{
  CDPrologue();
  if (cd::dont_cdop) goto cdregister2_label;
  if( IsHead() ) {
    // STUB
  }
  else {
    // It is at remote node so do something for that.

  }
cdregister2_label:
  CDEpilogue();

  return common::kOK;
}

float CDHandle::GetErrorProbability (SysErrT error_type, uint32_t error_num)
{

  CDPrologue();
  CDEpilogue();
  return 0;
}

float CDHandle::RequireErrorProbability (SysErrT error_type, uint32_t error_num, float probability, bool fail_over)
{

  CDPrologue();
  CDEpilogue();

  return 0;
}




#if CD_ERROR_INJECTION_ENABLED
//void CDHandle::RegisterErrorInjector(const ErrorType &error_type, ErrorInjector *error_injector)
//{
//  app_side = false;
//
//  CD_DEBUG("RegisterErrorInjector: %d at level #%u\n", GetExecMode(), level());
//  if(cd_error_injector_ == NULL) {
//    if( recreated() == false && reexecuted() == false ) {
//      CD_DEBUG("Registered!!\n");
//      error_injector_dir_[error_type] = error_injector;
//    }
//    else {
//      CD_DEBUG("Failed to be Registered!!\n");
//      delete cd_error_injector;
//    }
//  }
//  else {
//    CD_DEBUG("No injector registered!!\n");
//  }
//
//  error_injector_dir_[error_type] = error_injector;
//
//  app_side = true;
//}

void CDHandle::RegisterMemoryErrorInjector(MemoryErrorInjector *memory_error_injector)
{ CDHandle::memory_error_injector_ = memory_error_injector; }

void CDHandle::RegisterErrorInjector(CDErrorInjector *cd_error_injector)
{
  app_side = false;
  CD_DEBUG("RegisterErrorInjector: %d at level #%u\n", GetExecMode(), level());
  if(cd_error_injector_ == NULL && recreated() == false && reexecuted() == false) {
    CD_DEBUG("Registered!!\n");
    cd_error_injector_ = cd_error_injector;
    cd_error_injector_->task_in_color_ = task_in_color();
    cd_error_injector_->rank_in_level_ = rank_in_level();
  }
  else {
    CD_DEBUG("Failed to be Registered!!\n");
    delete cd_error_injector;
  }
  app_side = true;
}


int CDHandle::CheckErrorOccurred(uint32_t &rollback_point)
{
  bool during_rollback = ptr_cd_->IsFailed() ? true : false;
  uint64_t sys_err_vec = system_error_injector_->Inject();
  //if (during_rollback) sys_err_vec = 0; // FIXME
  bool found = false;
  CD_DEBUG("[%s] sys_err_vec : %lx\n", ptr_cd_->cd_id_.GetStringID().c_str(), sys_err_vec);
  if(sys_err_vec == NO_ERROR_INJECTED) {
    return (int)CD::CDInternalErrT::kOK;
  } else {
    
    PRINT_MPI("[%s %d] sys_err_vec : %lx\n", ptr_cd_->cd_id_.GetStringID().c_str(), myTaskID, sys_err_vec);
    CDHandle *cdh = this;
    // Find CD level that claimed "the raised sys_err_vec is covered."
    // If sys_err_vec > 
    while(cdh != NULL) {

//      printf("CHECK (syndrom:%lx == vec:%lx) = %d, lv:%u, %s\n", 
//          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
//          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
//          cdh->level(), cdh->GetLabel());
      CD_DEBUG("CHECK %lx %lx = %d, lv:%u, %s\n", 
          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
          cdh->level(), cdh->GetLabel());
      CD_DEBUG("CHECK %lx %lx = %d\n", sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_));
//      printf("CHECK %lx %lx = %d\n", sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_));
      if(CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_)) {
        rollback_point = cdh->level();
        found = true;
//        CD_DEBUG("\nFOUND LEVEL FOR REEXEC \n");
        CD_DEBUG("FOUND LEVEL FOR REEXEC at #%u\n\n", rollback_point);
        break;
      }
      cdh = CDPath::GetParentCD(cdh->level());
    }
    if(rollback_point < level()) {
      printf("\n[%d] >>>> Escalation Lv%u (%lu-%lu)->Lv%u during %s (syndrom:%lx == vec:%lx) = %d, lv:%u, %s\n", 
          cd::myTaskID, level(), phaseTree.current_->seq_begin_, phaseTree.current_->seq_end_, 
          rollback_point, 
          (IsReexec())? "REEX" : "EXEC",
          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
          cdh->level(), cdh->GetLabel());
      CD_DEBUG("\n>>>> Escalation Lv%u (%lu-%lu)->Lv%u during %s (syndrom:%lx == vec:%lx) = %d, lv:%u, %s\n", 
          level(), phaseTree.current_->seq_begin_, phaseTree.current_->seq_end_, 
          rollback_point, 
          (IsReexec())? "REEX" : "EXEC",
          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
          cdh->level(), cdh->GetLabel());
    } else if(rollback_point == level()) {

      const uint64_t curr_phase = ptr_cd_->phase();
      const uint64_t curr_seqID = cd::phaseTree.current_->seq_end_;
      const uint64_t curr_begID = cd::phaseTree.current_->seq_begin_;
      printf("[%d] >>>> Rollback (%s) during %s (syndrom:%lx == vec:%lx) = %d, lv:%u, %s phase:%ld==%lu, seqID:%ld==%lu(beg:%lu)\n", 
          cd::myTaskID, ptr_cd_->label_.c_str(),
          (IsReexec())? "REEX" : "EXEC",
          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
          cdh->level(), cdh->GetLabel(), 
          cd::failed_phase, curr_phase, cd::failed_seqID, curr_seqID, curr_begID);
      CD_DEBUG(">>>> Rollback (%s) during %s  (syndrom:%lx == vec:%lx) = %d, lv:%u, %s fphase:%ld==%lu, seqID:%ld==%lu(beg:%lu)\n", 
          ptr_cd_->label_.c_str(),
          (IsReexec())? "REEX" : "EXEC",
          sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_, 
          CHECK_SYS_ERR_VEC(sys_err_vec, cdh->ptr_cd_->sys_detect_bit_vector_),
          cdh->level(), cdh->GetLabel(), 
          cd::failed_phase, curr_phase, cd::failed_seqID, curr_seqID, curr_begID);

    }

    if(found == false) assert(0);
    return (int)CD::CDInternalErrT::kErrorReported;
  }
}

#endif

void CDHandle::Recover (uint32_t error_name_mask, 
                        uint32_t error_loc_mask, 
                        std::vector<SysErrT> errors)
{
  // STUB
}


int CDHandle::ctxt_prv_mode()
{
  return (int)ptr_cd_->ctxt_prv_mode_;
}

void CDHandle::CommitPreserveBuff()
{
  /*
  CDPrologue();
  CD_ASSERT(ptr_cd_ != NULL);
//  if(ptr_cd_->cd_exec_mode_ ==CD::kExecution){
  if(ptr_cd_->ctxt_prv_mode_ == kExcludeStack) {
//  cddbg << "Commit jmp buffer!" << endl; cddbgBreak();
//  cddbg << "cdh: " << jmp_buffer_ << ", cd: " << ptr_cd_->jmp_buffer_ << ", size: "<< sizeof(jmp_buf) << endl; cddbgBreak();
    memcpy(ptr_cd_->jmp_buffer_, jmp_buffer_, sizeof(jmp_buf));
    ptr_cd_->jmp_val_ = jmp_val_;
//  cddbg << "cdh: " << jmp_buffer_ << ", cd: " << ptr_cd_->jmp_buffer_ << endl; cddbgBreak();
  }
  else {
//    ptr_cd_->ctxt_ = this->ctxt_;
  }
//  }
  CDEpilogue();
  */
}


uint64_t CDHandle::SetSystemBitVector(uint64_t error_name_mask, uint64_t error_loc_mask)
{
  uint64_t sys_bit_vec = 0;
  if(error_name_mask == 0) {
    // STUB
  }
  else {
    // STUB
  }

  if(error_loc_mask == 0) {
    // STUB
  }
  else {
    // STUB
  }

  // FIXME
  sys_bit_vec = error_name_mask | error_loc_mask;

  return sys_bit_vec;
}







CDErrT CDHandle::CheckMailBox(void)
{
#if CD_MPI_ENABLED
  CDErrT cd_err = ptr_cd()->CheckMailBox();
  return cd_err;
#else
  return kOK;
#endif
}

CDErrT CDHandle::SetMailBox(CDEventT event)
{
#if CD_MPI_ENABLED
  CD_DEBUG("%s at level #%u\n", __func__, level());
  return ptr_cd()->SetMailBox(event);
#else
  return kOK;
#endif
}


ucontext_t *CDHandle::ctxt()
{
  return &(ptr_cd_->ctxt_);
}

jmp_buf *CDHandle::jmp_buffer()
{
  return &(ptr_cd_->jmp_buffer_);
} 


bool     CDHandle::recreated(void)     const { return ptr_cd_->recreated_; }
bool     CDHandle::reexecuted(void)    const { return ptr_cd_->reexecuted_; }
bool     CDHandle::need_reexec(void)   const { return *CD::rollback_point_ != INVALID_ROLLBACK_POINT; }
uint32_t CDHandle::rollback_point(void)  const { return *CD::rollback_point_; }
CDType   CDHandle::GetCDType(void)     const { return ptr_cd_->GetCDType(); }
int      IsReexec(void)  { return cd::failed_phase != HEALTHY; }
#if CD_MPI_ENABLED
int CDHandle::GetCommLogMode(void) const {return ptr_cd_->GetCommLogMode(); }
int CDHandle::GetCDLoggingMode(void) const {return ptr_cd_->cd_logging_mode_;}
#endif

#if CD_MPI_ENABLED && CD_TEST_ENABLED
void CDHandle::PrintCommLog(void) const {
  ptr_cd_->comm_log_ptr_->Print();
}
#endif


NodeID &CDHandle::GetNodeID(void) { return ptr_cd_->cd_id_.node_id_; }
