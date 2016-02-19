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
#include "cd_features.h"

#if CD_PROFILER_ENABLED

//#include "cd_path.h"
//#include "cd_global.h"
#include "profiler_interface.h"
#include "cd_def_internal.h" 

//#include "cd_internal.h"
#include "cd_global.h"
//#include "cd_handle.h"
using namespace cd;
using namespace cd::interface;
using namespace std;

clock_t cd::prof_begin_clk;
clock_t cd::prof_end_clk;
clock_t cd::prof_sync_clk;
std::map<uint32_t,std::map<std::string,RuntimeInfo>> Profiler::num_exec_map;

Profiler *Profiler::CreateProfiler(int prof_type, void *arg)
{
  switch(prof_type) {
    case 0 :
      return new Profiler(static_cast<CDHandle *>(arg));
//    case CDPROFILER : 
//      return dynamic_cast<Profiler *>(new CDProfiler(static_cast<CDProfiler *>(arg)));
    default :
      ERROR_MESSAGE("Undifined Profiler Type!\n");
  }
  return NULL;

}

void Profiler::BeginRecord(void)
{
  const uint32_t level = cdh_->level();
  string name = cdh_->GetName();
  auto rit = num_exec_map[level].find(name);

  if(rit == num_exec_map[level].end()) { 
    num_exec_map[level][name] = RuntimeInfo(1);
  } else {
    num_exec_map[level][name].total_exec_ += 1;
  }

  if(cdh_->GetExecMode() == kReexecution) {
    end_clk_  = clock();
    sync_clk_ = clock();
    num_exec_map[level][name].total_time_ += (double)(end_clk_  - begin_clk_) / CLOCKS_PER_SEC;
    num_exec_map[level][name].sync_time_  += (double)(sync_clk_ - begin_clk_) / CLOCKS_PER_SEC;
    num_exec_map[level][name].reexec_     += 1;
    reexecuted_ = true;
  }
  begin_clk_ = clock();
}

void Profiler::EndRecord(void)
{
  const uint32_t level = cdh_->level();
  string name = cdh_->GetName();
  end_clk_ = clock();
  num_exec_map[level][name].total_time_ += (double)(end_clk_ - begin_clk_) / CLOCKS_PER_SEC;
  if(reexecuted_) {
    num_exec_map[level][name].reexec_time_ += (double)(end_clk_  - begin_clk_) / CLOCKS_PER_SEC;
    reexecuted_ = false;
  }
}

void Profiler::RecordProfile(ProfileType profile_type, uint64_t profile_data)
{
  const uint32_t level = cdh_->level();
  string name = cdh_->GetName();
  switch(profile_type) {
    case PRV_COPY_DATA: {
      num_exec_map[level][name].prv_copy_ = profile_data;
      break;
    }
    case PRV_REF_DATA : {
      num_exec_map[level][name].prv_ref_ = profile_data;
      break;
    }
    case MSG_LOGGING : {
      num_exec_map[level][name].msg_logging_ = profile_data;
      break;
    }
    default:
      ERROR_MESSAGE("Invalid profile type to record : %d\n", profile_type);
  }
}
string RuntimeInfo::GetString(void)
{
  char stringout[256];
  snprintf(stringout, 256, 
    "# Execution:\t%lu\n# Reexecution:\t%lu\nTotal Execution Time:\t%lf[s]\nReexecution Time:\t%lf[s]\nSync Time (CDs):\t%lf[s]\nPreservation(Total):\t%lu[B]\nPreservation(Ref):\t%lu[B]\nComm Logging:\t%lu[B]\nError Vector:\t%lx", 
                         total_exec_,
                         reexec_,
                         total_time_,
                         reexec_time_,
                         sync_time_,
                         prv_copy_,
                         prv_ref_,
                         msg_logging_,
                         sys_err_vec_);
  return string(stringout);
}

void Profiler::Print(void) {
  for(auto it=num_exec_map.begin(); it!=num_exec_map.end(); ++it) {
    CD_DEBUG("Level %u --------------------------------\n", it->first);
    for(auto jt=it->second.begin(); jt!=it->second.end(); ++jt) {
      CD_DEBUG("%s : %s\n", jt->first.c_str(), jt->second.GetString().c_str());
    }
    CD_DEBUG("\n");
  }
  CD_DEBUG("-----------------------------------------\n");
}

void MergeInfoPerLevel(RuntimeInfo &info_total, const RuntimeInfo &info_per_level) {


}

RuntimeInfo Profiler::GetTotalInfo(void) {
  RuntimeInfo info_total;
  for(auto it=num_exec_map.begin(); it!=num_exec_map.end(); ++it) {
    RuntimeInfo info_per_level;
    CD_DEBUG("Level %u --------------------------------\n", it->first);
    //if(myTaskID == 0)
      printf("Level %u --------------------------------\n", it->first);
    for(auto jt=it->second.begin(); jt!=it->second.end(); ++jt) { //map<string,RuntimeInfo>>
      CD_DEBUG("%s : %s\n", jt->first.c_str(), jt->second.GetString().c_str());
      info_per_level += jt->second;
    }
    CD_DEBUG("-- Summary --\n");
    CD_DEBUG("%s", info_per_level.GetString().c_str());
    CD_DEBUG("\n");
    //if(myTaskID == 0) 
    {
      printf("-- Summary --\n");
      printf("%s", info_per_level.GetString().c_str());
      printf("\n");
    }
    MergeInfoPerLevel(info_total, info_per_level);
  }
  CD_DEBUG("-----------------------------------------\n");
  CD_DEBUG("Total Summary ---------------------------\n");
  CD_DEBUG("%s", info_total.GetString().c_str());
  CD_DEBUG("-----------------------------------------\n");
  return info_total;
}

#endif // profiler enabled
