#lib_LIBRARIES = libcd.a
include_HEADERS = cd.h \
									cd_capi.h \
									cd_global.h \
									cd_def_interface.h \
									cd_def_common.h \
									cd_def_debug.h \
									cd_features.h \
									cd_handle.h \
									tuned_cd_handle.h \
									runtime_info.h \
									node_id.h \
									phase_tree.h \
									sys_err_t.h \
									serdes.h \
									serializable.h \
									cd_containers.hpp \
									profiler_interface.h \
									error_injector.h 

# Libtool-related
lib_LTLIBRARIES = libcd.la

libcd_la_SOURCES = \
  	                cd_internal.cc \
  	                cd_comm_internal.cc \
  	                cd_comm_log.cc \
  	                cd_event.cc \
  	                cd_handle.cc \
										runtime_info.cc \
  	                tuned_cd_handle.cc \
  	                cd_id.cc \
										phase_tree.cc \
  	                cd_malloc.cc \
  	                cd_mpiwrapper.cc \
  	                cd_name_t.cc \
  	                cd_profiler.cc \
  	                event_handler.cc \
  	                libc_rn.cc \
  	                node_id.cc \
  	                error_injector.cc \
										profiler_interface.cc \
  	                recover_object.cc \
										serdes.cc \
  	                sys_err_t.cc \
										cd_capi.cc 
CD_ABS_PATH=${abs_top_builddir}
#MY_LDFLAGS=-L${CD_ABS_PATH}/plugin/wrapper -lwrapLibc -L${CD_ABS_PATH}/src/persistence -lpacker -L${YAML_PATH}/lib -lyaml -ldl
#libcd_la_LDFLAGS = -Wl,-rpath=${CD_ABS_PATH}/plugin/wrapper -Wl,-rpath=${CD_ABS_PATH}/src/persistence -Wl,-rpath=${YAML_PATH}/lib -export-dynamic #-export-symbols-regex
CD_LFLAG  = -L${CD_ABS_PATH}/src/persistence -lpacker -L${YAML_PATH}/lib -lyaml -ldl
LOG_LFLAG = -L${CD_ABS_PATH}/plugin/wrapper -lwrapLibc 
CD_RPATH  = -Wl,-rpath=${CD_ABS_PATH}/src/persistence -Wl,-rpath=${YAML_PATH}/lib -export-dynamic #-export-symbols-regex
LOG_RPATH = -Wl,-rpath=${CD_ABS_PATH}/plugin/wrapper
MY_LDFLAGS = ${LOG_LFLAG} ${CD_LFLAG}
libcd_la_LDFLAGS = ${LOG_RPATH} ${CD_RPATH} 
libcd_la_CPPFLAGS = -I${CD_ABS_PATH}/src/persistence -I${CD_ABS_PATH}/plugin/wrapper -I${YAML_PATH}/include
#libcd_la_CPPFLAGS = $(cpp_flags)
libcd_la_LIBADD = $(MY_LDFLAGS) $(external_libs) 

ACLOCAL_AMFLAGS = -I m4

##lib_LIBRARIES = libcd.a
#include_HEADERS = cd.h \
#									cd_capi.h \
#									cd_global.h \
#									cd_def_interface.h \
#									cd_def_common.h \
#									cd_def_debug.h \
#									cd_features.h \
#									cd_handle.h \
#									node_id.h \
#									sys_err_t.h \
#									serdes.h \
#									serializable.h \
#									profiler_interface.h \
#									error_injector.h \
##  	              packer.h 
#
## Libtool-related
#lib_LTLIBRARIES = libcd.la
#
#libcd_la_SOURCES = \
#  	                cd_internal.cc \
#  	                cd_comm_internal.cc \
#  	                cd_comm_log.cc \
#  	                cd_entry.cc \
#  	                cd_event.cc \
#  	                cd_handle.cc \
#  	                cd_id.cc \
#  	                cd_file_handle.cc \
#  	                cd_malloc.cc \
#  	                cd_mpiwrapper.cc \
#  	                cd_name_t.cc \
#  	                cd_pfs.cc \
#  	                cd_profiler.cc \
#  	                data_handle.cc \
#  	                event_handler.cc \
#  	                libc_rn.cc \
#  	                node_id.cc \
#  	                packer.cc \
#  	                error_injector.cc \
#										profiler_interface.cc \
#  	                recover_object.cc \
#										serdes.cc \
#  	                sys_err_t.cc \
#										cd_capi.cc 
