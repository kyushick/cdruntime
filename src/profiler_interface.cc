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
#if _PROFILER

#include "cd_path.h"
#include "cd_global.h"
#include "cd_profiler.h"
#include "util.h"
#include <cstdint>

//#include "cd_internal.h"
//#include "cd_global.h"
#include "cd_def_internal.h" 
//#include "cd_handle.h"
using namespace cd;
using namespace std;

clock_t cd::prof_begin_clk;
clock_t cd::prof_end_clk;

Profiler *cd::interface::CreateProfiler(PROFILER_TYPE prof_type, void *arg)
{
  switch(prof_type) {
    case NULLPROFILER :
      return dynamic_cast<Profiler *>(new NullProfiler());
    case CDPROFILER : 
      return dynamic_cast<Profiler *>(new CDProfiler(static_cast<CDProfiler *>(arg)));
    default :
      ERROR_MESSAGE("Undifined Profiler Type!\n");
  }
  return NULL;

}


  const uint32_t level = level();
  auto rit = num_exec_map_[level].find(name_);
  if(rit == num_exec_map_[level].end()) { 
    num_exec_map_[level][name_] = RuntimeInfo(1,0,0.0,0.0);
  } else {
    num_exec_map_[level][name_].total_exec_ += 1;
  }

  num_exec_map_[level][name_].total_exec_time_ += end - begin;
  num_exec_map_[level][name_].total_exec_time_ += end - begin;

void Profiler::RecordBegin(bool reexecution)
{
  const uint32_t level = level();
  auto rit = num_exec_map_[level].find(name_);
  if(rit == num_exec_map_[level].end()) { 
    num_exec_map_[level][name_] = RuntimeInfo(1,0,0.0,0.0);
  } else {
    num_exec_map_[level][name_].total_exec_ += 1;
  }
  if(reexecution) {
    prof_clk_end = clock();

    prof_clk_begin = clock();
}

void Profiler::RecordEnd(bool reexecution)
{
  prof_clk_end = clock();
  const double period = prof_clk_end - prof_clk_begin;
  num_exec_map_[level][name_].total_exec_time_ += period;
  if(reexecution) {
    num_exec_map_[level][name_].reexec_time_ += period;
    num_exec_map_[level][name_].reexec_ += 1;
  }
}
#endif
