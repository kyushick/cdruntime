#ifndef _NULLCD_HANDLE_H 
#define _NULLCD_HANDLE_H

#include <stdint.h>
#include "util.h"
#include "cd_handle.h"
#include "regen_object.h"
#define FUNC_ATTR inline __attribute__((always_inline))

//using namespace cd;

//class cd::RegenObject;
namespace tuned {
class CDPath;
class CDHandle;
typedef uint32_t PhaseID;

void AddHandle(CDHandle *);
void DeleteHandle(void);
CDHandle *TunedCD_Init(int numTask, int myTask, PrvMediumT prv_medium);
void TunedCD_Finalize(void);

class CDHandle {
  friend class tuned::CDPath;
  friend CDHandle *TunedCD_Init(int numTask, int myTask, PrvMediumT prv_medium);
  friend void TunedCD_Finalize(void);
  friend void DeleteHandle(void);
    std::map<uint32_t, ParamEntry> config_;
    cd::CDHandle *handle_;
    uint32_t level_;
    uint32_t phase_;
    bool active_;
    bool level_created_;
    uint32_t next_merging_phase_;
  private:
    CDHandle(cd::CDHandle *handle, uint32_t level, string name) {
      handle_ = handle;
      active_ = false;
      level_created_ = false;
      level_  = level;
//      printf("## Create TunedCDHAndle ##");
//      phase_  = cd::GetPhase(level_, name, false);
      // Update root's tuning parameters
      UpdateParam(level_, phase_);
    }
   ~CDHandle(void) {}

  public:
/** \addtogroup tunable_api 
 * @{
 */
   /**@brief Non-collective Create (Single task)
    *
    * @return Returns a pointer to the handle of the newly created
    * child CD.\n Returns 0 on an error (error code returned in a parameter).
    *
    */
    FUNC_ATTR
    CDHandle *Create(const char *name=NO_LABEL,//!< [in] Optional user-specified
                                               //!< name that can be used to "re-create" the same CD object
                                               //!< if it was not destroyed yet; useful for resuing preserved
                                               //!< state in CD trees that are not loop based.
                     int cd_type=kDefaultCD,   //!< [in] Strict or relaxed. 
                                               //!< User can also set preservation medium for children CDs 
                                               //!< when creating them. (ex. kStrict | kHDD)
                     uint32_t err_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     uint32_t err_loc_mask=0,  //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     CDErrT *error=0           //!< [in,out] Pointer for error return value 
                                               //!< (kOK on success and kError on failure) 
                                               //!< no error value returned if error=0.
                     ) 
    {
      // check prev_phase for this level.
      // if it is the same, follow the prev_phase params
      // otherwise, check ConfigEntry for this phase of the level currently
      // being created.
      CDHandle *new_handle = new CDHandle(handle_, level_ + 1, name);
      printf("[Tune %s lv:%u phase:%u] -> lv:%u phase:%u, interval:%ld\n", 
          __func__, level_, phase_, new_handle->level_, new_handle->phase_, config_[phase_].interval_); getchar();
      if(new_handle->IsActive()) {
       new_handle->handle_ = handle_->Create(name, cd_type, err_name_mask, err_loc_mask, error);
      }
      AddHandle(new_handle);
      return new_handle;
    }
 
   /**
    * @brief Collective Create
    * 
    *
    * @return Returns a pointer to the handle of the newly created child CD.\n
    *         Returns 0 on an error.
    *
    */
    FUNC_ATTR
    CDHandle *Create(uint32_t numchildren,     //!< [in] The total number of CDs that will be 
                                               //!< collectively created by the current CD object.
                                               //!< This collective CDHandle::Create() waits for 
                                               //!< all tasks in the current CD to arrive before 
                                               //!< creating new children.
                     const char *name=NO_LABEL,      //!< [in] Optional user-specified name that can be used to 
                                               //!< "re-create" the same CD object if it was not destroyed yet; 
                                               //!< useful for resuing preserved state in CD trees that are not loop based.
                     int cd_type=kDefaultCD,   //!< [in] Strict or relaxed. 
                                               //!< User can also set preservation medium for children CDs 
                                               //!< when creating them. (ex. kStrict | kDRAM)
                     uint32_t err_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     uint32_t err_loc_mask=0,  //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     CDErrT *error=0           //!< [in,out] Pointer for error return value 
                                               //!< (kOK on success and kError on failure) 
                                               //!< no error value returned if error=0.
                     )
    { 
      CDHandle *new_handle = new CDHandle(handle_, level_ + 1, name);
      printf("[Tune %s lv:%u phase:%u] -> lv:%u phase:%u, interval:%ld\n", 
          __func__, level_, phase_, new_handle->level_, new_handle->phase_, config_[phase_].interval_); getchar();
      if(new_handle->IsActive()) {
       new_handle->handle_ = handle_->Create(numchildren, name, cd_type, err_name_mask, err_loc_mask, error);
      }
      AddHandle(new_handle);
      return new_handle;
    } 



  
   /**
    * @brief Collective Create
    * 
    *
    * @return Returns a pointer to the handle of the newly created
    * child CD; returns 0 on an error.
    *
    */
    FUNC_ATTR      
    CDHandle *Create(uint32_t color,           //!< [in] The "color" of the new child to which this task will belong to.
                     uint32_t task_in_color,   //!< [in] The total number of tasks that are collectively creating
                                               //!< the child numbered "color"; the collective waits for this number
                                               //!< of tasks to arrive before creating the child.
                     uint32_t  numchildren,    //!< [in] The total number of CDs that will be collectively 
                                               //!< created by the current CD object.
                                               //!< This collective CDHandle::Create() waits for 
                                               //!< all tasks in the current CD to arrive before creating new children.
                     const char *name=NO_LABEL,      //!< [in] Optional user-specified name that can be used to 
                                               //!< "re-create" the same CD object if it was not destroyed yet; 
                                               //!< useful for resuing preserved state in CD trees that are not loop based.
                     int cd_type=kDefaultCD,   //!< [in] Strict or relaxed. 
                                               //!< User can also set preservation medium for children CDs 
                                               //!< when creating them. (ex. kStrict | kDRAM)
                     uint32_t err_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     uint32_t err_loc_mask=0,  //!< [in] each `1` in the mask indicates that this CD
                                               //!< should be able to recover from that error type.
                     CDErrT *error=0           //!< [in,out] Pointer for error return value 
                                               //!< (kOK on success and kError on failure) 
                                               //!< no error value returned if error=0.
                     )
    { 
      CDHandle *new_handle = new CDHandle(handle_, level_ + 1, name);
      printf("[Tune %s lv:%u phase:%u] -> lv:%u phase:%u, interval:%ld\n", 
          __func__, level_, phase_, new_handle->level_, new_handle->phase_, config_[phase_].interval_); getchar();
      if(new_handle->IsActive()) {
       new_handle->handle_ = handle_->Create(color, task_in_color, numchildren, 
                                          name, cd_type, err_name_mask, err_loc_mask, error);
      }
      AddHandle(new_handle);
      return new_handle;
    } 

 
   /**
    * @brief Collective Create and Begin
    *
    * @return Returns a pointer to the handle of the newly created
    * child CD; returns 0 on an error.
    *
    */
    FUNC_ATTR
    CDHandle *CreateAndBegin(uint32_t numchildren,     //!< [in] The total number of CDs that 
                                                       //!< will be collectively created by the current CD object.
                                                       //!< This collective CDHandle::Create() waits for 
                                                       //!< all tasks in the current CD to arrive 
                                                       //!< before creating new children.
                             const char *name=NO_LABEL,      //!< [in] Optional user-specified name that can be used to 
                                                       //!< "re-create" the same CD object if it was not destroyed yet; 
                                                       //!< useful for resuing preserved state in CD trees 
                                                       //!< that are not loop based.
                             int cd_type=kDefaultCD,   //!< [in] Strict or relaxed. 
                                                       //!< User can also set preservation medium for children CDs 
                                                       //!< when creating them. (ex. kStrict | kDRAM)
                             uint32_t err_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                       //!< should be able to recover from that error type.
                             uint32_t err_loc_mask=0,  //!< [in] each `1` in the mask indicates that this CD
                                                       //!< should be able to recover from that error type.
                             CDErrT *error=0           //!< [in,out] Pointer for error return value 
                                                       //!< (kOK on success and kError on failure) 
                                                       //!< no error value returned if error=0.
                             )
    { 
      CDHandle *new_handle = new CDHandle(handle_, level_ + 1, name);
      printf("[Tune %s lv:%u phase:%u] -> lv:%u phase:%u, interval:%ld\n", 
          __func__, level_, phase_, new_handle->level_, new_handle->phase_, config_[phase_].interval_); getchar();
      if(new_handle->IsActive()) {
        new_handle->handle_ = handle_->CreateAndBegin(numchildren, name, cd_type, err_name_mask, err_loc_mask, error);
      }
      AddHandle(new_handle);
      return new_handle;
    } 

   /** @brief Destroys a CD
    *
    * @return May return kError if instance was already destroyed, but
    * may also return kOK in such a case.
    *
    */
    FUNC_ATTR
    CDErrT Destroy(bool collective=false //!< [in] if `true`, destroy is done as a collective across all tasks that
                                         //!< created the CD; otherwise the behavior is that only one task destroys 
                                         //!< the actual object while the rest just delete the local CDHandle.
                  )
    { 
      printf("[Tune %s lv:%u phase:%u]\n", __func__, level_, phase_); getchar();
//      if(active_) { 
      if(level_created_) { 
        common::CDErrT ret = handle_->Destroy(collective);
        DeleteHandle();
        return ret;
      } else {
        return common::kOK;
      }
    } 

   /** @brief Begins a CD 
    *
    *
    * @return Returns kOK when successful and kError otherwise.
    * @sa Complete()
    */
    FUNC_ATTR
    CDErrT Begin(bool collective=true,//!< [in] Specifies whether this call is a collective across all tasks 
                                      //!< contained by this CD or whether its to be run by a single task 
                                      //!< only with the programmer responsible for synchronization. 
                 const char *label=NO_LABEL,
                 const uint64_t &sys_err_vec=0
                )
    {
      // Update phase 
      printf("## Tuned Begin ##");
      phase_ = cd::GetPhase(level_, 
                        (strcmp(label, NO_LABEL) == 0)? handle_->name() : label,
                        false);
      ParamEntry &param = config_[phase_];
      printf("[Tune %s lv:%u phase:%u] count:%lu <= interval:%ld\n", 
          __func__, level_, phase_, param.count_, param.interval_); getchar();
      
      if(param.interval_ > 0 && param.count_ % param.interval_ == 0) { 
        active_ = true;
        return handle_->Begin(collective, label, sys_err_vec);
      } else { // if this phase is merged, interval_ == 0
        active_ = false;
        return common::kOK;
      }
    } 

   /** @brief Completes a CD 
    *
    * @return Returns kOK when successful and kError otherwise.
    * @sa Begin()
    */
    FUNC_ATTR
    CDErrT Complete(bool terminate=false,
                    bool collective=true, //!< [in] Specifies whether
                                          //!< this call is a collective
                                          //!< across all tasks contained
                                          //!< by this CD or whether its to
                                          //!< be run by a single task only
                                          //!< with the programmer
                                          //!< responsible for
                                          //!< synchronization
                    bool update_prv=false //!< [in] Flag that
                                          //!< indicates whether preservation should be
                                          //!< updated on Complete rather than discarding all 
                                          //!< preserved state. If `true` then Complete
                                          //!< followed by Begin can be much more efficient
                                          //!< if the majority of preserved data overlaps
                                          //!< between these two consecutive uses of the CD
                                          //!< object (this enables the Cray CD
                                          //!< AdvancePointInTime functionality).
                   )
    { 
//      if(count_++ % interval_ == interval_ - 1) {
      ParamEntry &param = config_[config_[phase_].merge_begin_];
      
      printf("[Tune %s lv:%u phase:%u]] count:%lu <= interval:%ld\n", 
          __func__, level_, phase_, param.count_, param.interval_ - 1); getchar();
      if(phase_ != config_[phase_].merge_end_) { 
        return common::kOK;
      } else if(terminate || ((param.interval_ >= 0) && (param.count_ % param.interval_ == param.interval_-1))) {
        param.count_++; 
        return handle_->Complete(collective, update_prv);
      } else {
        return common::kOK;
      }
    } 

   /**@brief (Non-collective) Preserve data to be restored when recovering. 
    *        (typically reexecuting the CD from right after its Begin() call.)
    *
    * @return kOK on success and kError otherwise.
    *
    * \sa Complete()
    */
    FUNC_ATTR
    CDErrT Preserve(void *data_ptr=0,              //!< [in] pointer to data to be preserved;
                                                   //!< __currently must be in same address space
                                                   //!< as calling task, but will extend to PGAS fat pointers later 
                    uint64_t len=0,                //!< [in] Length of preserved data (Bytes)
                    uint32_t preserve_mask=kCopy,  //!< [in] Allowed types of preservation 
                                                   //!< (e.g., kCopy|kRef|kRegen), default only via copy
                    const char *my_name=NO_LABEL,  //!< [in] Optional C-string representing the name of this
                                                   //!< preserved data for later preserve-via-reference
                    const char *ref_name=NO_LABEL, //!< [in] Optional C-string representing
                                                   //!< a user-specified name that was set 
                                                   //!< by a previous preserve call at the parent.; 
                                                   //!< __Do we also need an offset into parent preservation?__
                    uint64_t ref_offset=0,         //!< [in] explicit offset within the named region at the other CD 
                                                   //!< (for restoration via reference)
                    const cd::RegenObject *regenObj=0, //!< [in] optional user-specified function for
                                                   //!< regenerating values instead of restoring by copying
                    PreserveUseT dataUsage=kUnsure //!< [in] This flag is used
                                                   //!< to optimize consecutive Complete/Begin calls
                                                   //!< where there is significant overlap in
                                                   //!< preserved state that is unmodified (see Complete()).
                    )
    { 
      ParamEntry &param = config_[phase_];
      printf("[Tune %s lv:%u phase:%u] name:%s count:%lu <= interval:%ld\n", 
          __func__, level_, phase_, my_name, param.count_, param.interval_ - 1); getchar();
      if(active_) { 
        return handle_->Preserve(data_ptr, len, preserve_mask, my_name, ref_name, ref_offset, regenObj, dataUsage);
      } else {
        return common::kOK;
      }
    } 

   /**@brief (Non-collective) Preserve user-defined class object
    * 
    * @return kOK on success and kError otherwise.
    *
    * \sa Serdes, Complete()
    */
    FUNC_ATTR
    CDErrT Preserve(cd::Serializable &serdes,          //!< [in] Serdes object in user-defined class.
                                                   //!< This will be invoked in CD runtime
                                                   //!< to de/serialize class object
                    uint32_t preserve_mask=kCopy,  //!< [in] Allowed types of preservation 
                                                   //!< (e.g., kCopy|kRef|kRegen), default only via copy
                    const char *my_name=NO_LABEL,  //!< [in] Optional C-string representing the name of this
                                                   //!< preserved data for later preserve-via-reference
                    const char *ref_name=NO_LABEL, //!< [in] Optional C-string representing
                                                   //!< a user-specified name that was set
                                                   //!< by a previous preserve call at the parent; 
                                                   //!< __Do we also need an offset into parent preservation?__
                    uint64_t ref_offset=0,         //!< [in] explicit offset within the named region at the other CD 
                                                   //!< (for restoration via reference)
                    const cd::RegenObject *regenObj=0, //!< [in] optional user-specified function for
                                                   //!< regenerating values instead of restoring by copying
                    PreserveUseT dataUsage=kUnsure //!< [in] This flag is used
                                                   //!< to optimize consecutive Complete/Begin calls
                                                   //!< where there is significant overlap in
                                                   //!< preserved state that is unmodified (see Complete()).
                    )
    { 
      ParamEntry &param = config_[phase_];
      printf("[Tune %s lv:%u phase:%u] name:%s count:%lu <= interval:%ld\n", 
          __func__, level_, phase_, my_name, param.count_, param.interval_ - 1); getchar();
      if(active_) { 
        return handle_->Preserve(serdes, preserve_mask, my_name, ref_name, ref_offset);
      } else {
        return common::kOK;
      }
    } 

   /** @brief (Not supported yet) Non-blocking preserve data to be restored when recovering 
    *  (typically reexecuting the CD from right after its Begin() call.
    *
    *  @return kOK on success and kError otherwise.
    *
    *  \sa Complete()
    */
    FUNC_ATTR
    CDErrT Preserve(cd::CDEvent &cd_event,             //!< [in,out] enqueue this call onto the cd_event
                    void *data_ptr=0,              //!< [in] pointer to data to be preserved;
                                                   //!< __currently must be in same address space as calling task, 
                                                   //!< but will extend to PGAS fat pointers later
                    uint64_t len=0,                //!< [in] Length of preserved data (Bytes)
                    uint32_t preserve_mask=kCopy,  //!< [in] Allowed types of preservation 
                                                   //!< (e.g.,kCopy|kRef|kRegen), default only via copy
                    const char *my_name=NO_LABEL,  //!< [in] Optional C-string representing the name of this
                                                   //!< preserved data for later preserve-via-reference
                    const char *ref_name=NO_LABEL, //!< [in] Optional C-string representing a user-specified name 
                                                   //!< that was set by a previous preserve call at the parent.; 
                                                   //!< __Do we also need an offset into parent preservation?__
                    uint64_t ref_offset=0,         //!< [in] explicit offset within the named region at the other CD
                    const cd::RegenObject *regenObj=0, //!< [in] optional user-specified function for
                                                   //!< regenerating values instead of restoring by copying
                    PreserveUseT dataUsage=kUnsure //!< [in] This flag is used to optimize consecutive Complete/Begin calls
                                                   //!< where there is significant overlap in preserved state 
                                                   //!< that is unmodified (see Complete()).
                    )
    { 
      ParamEntry &param = config_[phase_];
      printf("[Tune %s lv:%u phase:%u] name:%s count:%lu <= interval:%ld\n", 
          __func__, level_, phase_, my_name, param.count_, param.interval_ - 1); getchar();
      if(active_) { 
        return handle_->Preserve(cd_event, data_ptr, len, preserve_mask, my_name, ref_name, ref_offset);
      } else {
        return common::kOK;
      }
    } 

   /** @brief User-provided detection function for failing a CD
    * 
    *  @return kOK when the assertion call completed successfully
    *
    */
    FUNC_ATTR
    CDErrT CDAssert(bool test_true,             //!< [in] Boolean to be asserted as true.
                    const cd::SysErrT* err_report=0 //!< [in,out] An optional error report that will be
                                                //!< used during recovery and for system diagnostics. 
                    )
    { 
      printf("[Tune %s lv:%u phase:%u] %d\n", 
          __func__, level_, phase_, test_true); getchar();
      if(active_) { 
        return handle_->CDAssert(test_true, err_report);
      } else {
        return common::kOK;
      }
    } 

   /** @brief User-provided detection function for failing a CD
    * 
    *  @return kOK when the assertion call completed successfully
    *  \warning May not be implemented yet.
    */
    FUNC_ATTR
    CDErrT CDAssertFail(bool test_true,             //!< [in] Boolean to be asserted as true.
                        const cd::SysErrT* err_report=0 //!< [in,out] An optional error report that will be
                                                    //!< used during recovery and for system diagnostics. 
                        )
    { 
      if(active_) { 
        return handle_->CDAssertFail(test_true, err_report);
      } else {
        return common::kOK;
      }
    } 

   /** @brief User-provided detection function for failing a CD
    *
    *  @return kOK when the assertion call completed successfully
    */
    FUNC_ATTR
    CDErrT CDAssertNotify(bool test_true,             //!< [in] Boolean to be asserted as true.
                          const cd::SysErrT* err_report=0 //!< [in,out] An optional error report that will be
                                                      //!< used during recovery and for system diagnostics.
                         )
    { 
      if(active_) { 
        return handle_->CDAssertNotify(test_true, err_report);
      } else {
        return common::kOK;
      }
    } 

  
   /** @brief Check whether any errors occurred while CD the executed
    *
    *  @return kOK when there is no failure. For optionally returning a CD runtime 
    *  error code indicating some bug with Detect()
    *  any errors or failures detected during this CDs execution.
    *
    */
    FUNC_ATTR
    std::vector<cd::SysErrT> Detect(CDErrT* err_ret_val=0 //!< [in,out] Pointer to a variable 
            //!<for optionally returning a CD runtime error code indicating some bug with Detect().
                 )
    { 
      printf("[Tune %s lv:%u phase:%u] \n", 
          __func__, level_, phase_); getchar();
      if(active_) { 
        return handle_->Detect(err_ret_val);
      } else {
        return std::vector<cd::SysErrT>();
      }
    } 

/** @} */ // Ends tunable_api


/** @ingroup cd_detection */
/** @ingroup register_detection_recovery */
/**
 *  @{
 */

   /** @brief Declare that this CD can detect certain errors/failures by user-defined detectors.
    *
    * The intent of this method is to specify to the autotuner that
    * detection is possible. This is needed in order to balance between
    * fine-grained and coarse-grained CDs and associated recovery.
    *
    * @return kOK on success.
    */
    FUNC_ATTR
    CDErrT RegisterDetection(uint32_t system_name_mask, //!< [in] each `1` in
                                                        //!< the mask indicates that this CD
                                                        //!< should be able to detect any errors
                                                        //!< that are meaningful to the application
                                                        //!< (in the error type mask).
                             uint32_t system_loc_mask   //!< [in] each `1` in
                                                        //!< the mask indicates that this CD
                                                        //!< should be able to detect any errors
                                                        //!< that are meaningful to the application
                                                        //!< (in the error type mask).
                            )
    { return handle_->RegisterDetection(system_name_mask, system_loc_mask); }

   /** @brief Register that this CD can recover from certain errors/failures
    *
    * This method serves two purposes:
    *  It extends the specification of intent to recover provided in
    *  Create(). 
    *  It enables registering a customized recovery routine by
    *  inheriting from the cd::RecoverObject class.
    *
    * @return kOK on success.
    *
    * \sa Create(), cd::RecoverObject
    *
    *\todo Does registering recovery also imply turning on detection?
    *Or is that done purely through RequireErrorProbability()? 
    */

    FUNC_ATTR
    CDErrT RegisterRecovery(uint32_t error_name_mask,   //!< [in] each `1` in
                                                        //!< the mask indicates that this CD
                                                        //!< should be able to recover from that
                                                        //!< error type.
                            uint32_t error_loc_mask,    //!< [in] each `1` in
                                                        //!< the mask indicates that this CD
                                                        //!< should be able to recover from that
                                                        //!< error type.
                            cd::RecoverObject* recoverObj=0 //!< [in] pointer
                                                        //!< to an object that contains the customized
                                                        //!< recovery routine; if unspecified,
                                                        //!< default recovery is used.
                            )
    { return handle_->RegisterRecovery(error_name_mask, error_loc_mask, recoverObj); }
  
    FUNC_ATTR
    CDErrT RegisterRecovery(
             uint32_t error_name_mask, //!< [in] each `1` in
                                       //!< the mask indicates that this CD
                                       //!< should be able to recover from that
                                       //!< error type.
             uint32_t error_loc_mask,  //!< [in] each `1` in
                                       //!< the mask indicates that this CD
                                       //!< should be able to recover from that
                                       //!< error type.
             cd::RecoveryFuncType recovery_func=0 
                                       //!< [in] function pointer for customized
                                       //!< recovery routine; if unspecified, default recovery is used.
             )
    { return handle_->RegisterRecovery(error_name_mask, error_loc_mask, recovery_func); }
    /** \todo What about specifying leniant communication-related errors
     *  for relaxed-CDs context?
     */

/** @} */ // Ends register_detection_recovery group

/**@addtogroup cd_split
 * @{
 */
   /**@brief Register a method to split CDs to create chidlren CDs. 
    * 
    * __SPMD programming model specific__
    * When CD runtime create children CDs, it is necessary to split the task group of the current CD,
    * then re-index each task in new task groups for children CDs. 
    * By default, it splits the task group of the current CD in three dimension.
    * In general, how many children group to split, 
    * how may tasks there are in current task group, current task ID are required.
    * With these three information, split method populates new color (task group ID), new task ID appropriately.
    *
    * @return Returns CD-related error code.
    * @param [in] function pointer or object to split CD.
    */
    FUNC_ATTR
    CDErrT RegisterSplitMethod(cd::SplitFuncT split_func) 
    { return handle_->RegisterSplitMethod(split_func); }
/** @} */ // Ends cd_split

/** \addtogroup cd_error_probability Methods for Interacting with the CD Framework and Tuner
 *
 * @{
 */

   /** @brief Ask the CD framework to estimate error/fault rate.
    *
    * Each CD will experience a certain rate of failure/error for
    * different failure/error mechanisms. These rates depend on the
    * system, the duration of the CD, its memory footprint, etc. The CD
    * framework can estimate the expected rate of each fault mechanism
    * given its knowledge of the CD and system properties.
    *
    * @return probability that the queried number of error/failure of the type
    * queried will occur during the execution of this CD.
    *
    * \note Should this be some rate instead? Seems like it would be
    * easier for the programmer to deal with number and probability,
    * but is it?
    *
    * \todo Decide on rate vs. number+probability
    */
    FUNC_ATTR
    float GetErrorProbability(cd::SysErrT error_type, //!< [in] Type of
                //!error/failure
                //!queried
            uint32_t error_num //!< [in] Number of
                //!< errors/failures queried.
            )
    { return handle_->GetErrorProbability(error_type, error_num); }

   /** @brief Request the CD framework to reach a certain error/failure probability.
    *
    * Each CD will experience a certain rate of failure/error for
    * different failure/error mechanisms. These rates depend on the
    * system, the duration of the CD, its memory footprint, etc. The CD
    * framework may be able to apply automatic resilience techniques,
    * such as replication, to ensure certain errors/failures will be
    * tolerated (or simply) detected.
    *
    * @return probability that at least `error_num` errors/failurse of the type
    * queried will occur during the execution of this CD, _after_ the 
    * automatic techniques are applied. Should be less than or equal to
    * requested `probability` if successful.
    *
    * \note Should this be some rate instead? Seems like it would be
    * easier for the programmer to deal with number and probability,
    * but is it?
    *
    * \todo Decide on rate vs. number+probability
    *
    */
    FUNC_ATTR
    float RequireErrorProbability(cd::SysErrT error_type, //!< [in] Type of
          //!< error/failure
          //!< queried.
          uint32_t error_num, //!< [in] Number of
          //!< errors/failures queried.
          float probability, //!< [in] Requested
          //!< maximum probability of `num_errors` errors/failures
          //!< not being detected or even occurring during
          //!< CD execution.
          bool fail_over=true //!< [in] Should
          //!< redundancy be added just to detect the
          //!< specified  error type (`false`) or should
          //!< enough redundancy be added to tolerate the
          //!< error (fail-over/forward-error-correction/...)
          )
    { return RequireErrorProbability(error_type, error_num, probability, fail_over); }

/** @} */ // End cd_error_probability group  =======================================


/**@addtogroup error_injector
 * @{
 */
   /**@brief Register memory error injection method into CD runtime system.
    *
    * This is registered once in a program generally, and will be invoked at Preserve().
    * Then, some location in the application data can be corrupted (manipulated) by CD runtime,
    * and application will perform its computation with this corrupted value.
    * The wrong results from wrong inputs will be supposed to be detected at the end of CD.
    * This error injection is used for development phase, not in production run.
    * 
    * So, ERROR_INJECTION_ENABLED=0 to turn off this error injection functinality.
    * Even though there is some code lines regarding error injection, 
    * it will not affect any errors with this compile-time flag off.
    *
    * (See \ref sec_example_error_injection)
    *
    * @param [in] created memory error injector object.
    */

    FUNC_ATTR
    void RegisterMemoryErrorInjector(MemoryErrorInjector *memory_error_injector)
    { handle_->RegisterMemoryErrorInjector(memory_error_injector); }

   /**@brief Register error injection method into CD runtime system.
    * 
    * This is registered per CDHandle::Create(),
    * otherwise error injection information will be passed from parent. 
    * (Just task list to fail, not CD list to fail. 
    * It is because CD hierarchy could be changed by CDHandle::Create(), 
    * but task ID (mpi rank in MPI version) will be invariant.)
    * 
    * This error injection is used for development phase, not in production run.
    * So, ERROR_INJECTION_ENABLED=0 to turn off this error injection functinality.
    * Even though there is some code lines regarding error injection, 
    * it will not affect any errors with this compile-time flag off.
    * -\ref sec_example_error_injection
    * @param [in] newly created error injector object.
    */
    FUNC_ATTR
    void RegisterErrorInjector(CDErrorInjector *cd_error_injector) 
    { handle_->RegisterErrorInjector(cd_error_injector); }

 /** @} */ // Ends of error_injector

//    ///@brief Get CDHandle to this CD's parent
//    ///@return Pointer to CDHandle of parent
//    FUNC_ATTR
//    CDHandle *GetParent(void)    const { return GetParent(level_); }

    ///@brief Operator to check equality between CDHandles.
//    bool operator==(const cd::CDHandle &other) const { return *handle == other; }

    inline void UpdateParam(uint32_t level, uint32_t phase) 
    {
//      auto it = config.mapping_.find(level);
//      assert(it == config.mapping_.end());
//      auto jt = it-find(phase);
//      assert(jt == jt->end());
      int64_t interval = cd::config.mapping_[level][phase].interval_;
      if(interval == 0) {
        assert(phase <= 0);
        uint32_t merge_target = config_[phase - 1].merge_begin_;
        config_[phase].merge_begin_ = merge_target;
        config_[phase].interval_    = 0;
        config_[phase].merge_end_ = config_[merge_target].merge_end_;
        assert(config_[phase+1].interval_ != 0);
      } else {
        config_[phase].merge_begin_ = phase; // current phase for the next phase init
        config_[phase].interval_    = interval;
        uint32_t next_phase = phase + 1;
        while(1) {
          if(cd::config.mapping_[level][next_phase].interval_ != 0) {
            config_[phase].merge_end_  = next_phase - 1;
            break;
          } else {
            next_phase++;
          }
        }
      }
      config_[phase].error_mask_ = cd::config.mapping_[level][phase].failure_type_;
    }

    // check prev_phase for this level.
    // if it is the same, follow the prev_phase params
    // otherwise, check ConfigEntry for this phase of the level currently
    // being created.
    inline bool IsActive() {
      level_created_ = (config_[phase_].interval_ > 0);
      return level_created_;
    }
//    inline bool NeedCreateLevel(string label) 
//    {
//      const uint32_t next_level = level_ + 1;
//      uint32_t phase = GetPhase(next_level, label, false);
//      return (config.mapping_[next_level][phase].interval_ > 0);
//    }
};




#define CD_Begin(...) FOUR_ARGS_MACRO(__VA_ARGS__, CD_BEGIN3, CD_BEGIN2, CD_BEGIN1, CD_BEGIN0)(__VA_ARGS__)

// Macros for setjump / getcontext
// So users should call this in their application, not call cd_handle_->Begin().
#define CD_BEGIN0(X) if((X)->ctxt_prv_mode() == kExcludeStack) setjmp((X)->jmp_buffer_);  else getcontext(&(X)->ctxt_) ; (X)->CommitPreserveBuff(); (X)->Begin();
#define CD_BEGIN1(X,Y) if((X)->ctxt_prv_mode() == kExcludeStack) setjmp((X)->jmp_buffer_);  else getcontext(&(X)->ctxt_) ; (X)->CommitPreserveBuff(); (X)->Begin(Y);
#define CD_BEGIN2(X,Y,Z) if((X)->ctxt_prv_mode() == kExcludeStack) setjmp((X)->jmp_buffer_);  else getcontext(&(X)->ctxt_) ; (X)->CommitPreserveBuff(); (X)->Begin(Y,Z);
#define CD_BEGIN3(X,Y,Z,W) if((X)->ctxt_prv_mode() == kExcludeStack) setjmp((X)->jmp_buffer_);  else getcontext(&(X)->ctxt_) ; (X)->CommitPreserveBuff(); (X)->Begin(Y,Z,W);

#define CD_Complete(X) (X)->Complete()   


class CDPath : public std::vector<tuned::CDHandle *> {
//  friend class tuned::CDHandle;
  friend tuned::CDHandle *tuned::GetCurrentCD(void);
  friend tuned::CDHandle *tuned::GetLeafCD(void);
  friend tuned::CDHandle *tuned::GetRootCD(void);
  friend tuned::CDHandle *tuned::GetParentCD(void);
  friend tuned::CDHandle *tuned::GetParentCD(int current_level);
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
    if(uniquePath_ != NULL) {
      //printf("path is not null\n");
      if( !uniquePath_->empty() ) {
        //printf("path is not empty\n");
        if( uniquePath_->back()->handle_->GetExecMode() == cd::kExecution || cd::kReexecution ) {
          
          //printf("Active CD is %d\n", uniquePath_->back()->level() );
          return uniquePath_->back();
        }
        else {
          //printf("parent called is %d\n", uniquePath_->back()->level() );
          return GetParentCD(uniquePath_->back()->handle_->level());
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
    //printf("GetCurrentCD is called\n");
    if(uniquePath_ != NULL ) {
      //printf("path is not null\n");
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
    if(uniquePath_ != NULL) {
      if( uniquePath_->size() > 0 ) {
        if(level < (uint32_t)uniquePath_->size()) {
          return uniquePath_->at(level); 
        } else {
//          uint32_t s = uniquePath_->size();
          //CD_DEBUG("level %u >= %u #cdobj\n", level, s);
        }
      }
    }
    return NULL;
  }

// /** @brief Get a CDHandle at the lowest level where there are more than one task. 
//  *
//  *  This is normally used internally.
//  *
//  * \return Pointer to CDHandle that has multiple tasks in it.
//  */
//  static CDHandle *GetCoarseCD(CDHandle *curr_cdh) {
//    while( curr_cdh->handle_->task_size() == 1 ) {
//      if(curr_cdh == GetRootCD()) {
//        //CD_DEBUG("There is a single task in the root CD\n");
////        assert(0);
//        return curr_cdh;
//      }
//      curr_cdh = CDPath::GetParentCD(curr_cdh->level());
//    } 
//    return curr_cdh;
//  }

//  CD *GetCoarseCD(CD* curr_cd);


};

void AddHandle(CDHandle *handle)
{
  CDPath::GetCDPath()->push_back(handle);
}

void DeleteHandle() 
{
  delete CDPath::GetCDPath()->back();
  CDPath::GetCDPath()->pop_back();
}

} // namespace tuned ends




#endif



#if 0
class NullCDHandle {
    uint64_t p; 
    uint64_t count;
    uint32_t phase;
    uint32_t level;
  public:
    virtual CDHandle *Create(const char *name=0, //!< [in] Optional user-specified
                                         //!< name that can be used to "re-create" the same CD object
                                         //!< if it was not destroyed yet; useful for resuing preserved
                                         //!< state in CD trees that are not loop based.
                     int cd_type=kDefaultCD, //!< [in] Strict or relaxed. 
                                             //!< User can also set preservation medium for children CDs 
                                             //!< when creating them. (ex. kStrict | kHDD)
                     uint32_t error_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                 //!< should be able to recover from that error type.
                     uint32_t error_loc_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                //!< should be able to recover from that error type.
                     bool     collective=false,
                     CDErrT *error=0 //!< [in,out] Pointer for error return value 
                                     //!< (kOK on success and kError on failure) 
                                     //!< no error value returned if error=0.
                     ) 
    { return CDPath::GetCurrentCD(); } 

/**
 * @brief Collective Create
 * 
 *
 * @return Returns a pointer to the handle of the newly created child CD.\n
 *         Returns 0 on an error.
 *
 */
    virtual CDHandle *Create(uint32_t  numchildren, //!< [in] The total number of CDs that will be 
                                            //!< collectively created by the current CD object.
                                            //!< This collective CDHandle::Create() waits for 
                                            //!< all tasks in the current CD to arrive before 
                                            //!< creating new children.
                     const char *name, //!< [in] Optional user-specified name that can be used to 
                                       //!< "re-create" the same CD object if it was not destroyed yet; 
                                       //!< useful for resuing preserved state in CD trees that are not loop based.
                     int cd_type=kDefaultCD, //!< [in] Strict or relaxed. 
                                             //!< User can also set preservation medium for children CDs 
                                             //!< when creating them. (ex. kStrict | kDRAM)
                     uint32_t error_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                 //!< should be able to recover from that error type.
                     uint32_t error_loc_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                //!< should be able to recover from that error type.
                     CDErrT *error=0 //!< [in,out] Pointer for error return value 
                                     //!< (kOK on success and kError on failure) 
                                     //!< no error value returned if error=0.
                     )
    { return CDPath::GetCurrentCD(); } 



  
/**
 * @brief Collective Create
 * 
 *
 * @return Returns a pointer to the handle of the newly created
 * child CD; returns 0 on an error.
 *
 */
    CDHandle *CreateSW(uint32_t onOff,        //!< [in] Tuning can be done with this switch.
                     uint32_t color,              //!< [in] The "color" of the new child to which this task will belong to.
                     uint32_t task_in_color, //!< [in] The total number of tasks that are collectively creating
                                                  //!< the child numbered "color"; the collective waits for this number
                                                  //!< of tasks to arrive before creating the child.
                     uint32_t  numchildren, //!< [in] The total number of CDs that will be collectively 
                                            //!< created by the current CD object.
                                             //!< This collective CDHandle::Create() waits for 
                                             //!< all tasks in the current CD to arrive before creating new children.
                     const char *name, //!< [in] Optional user-specified name that can be used to 
                                       //!< "re-create" the same CD object if it was not destroyed yet; 
                                       //!< useful for resuing preserved state in CD trees that are not loop based.
                     int cd_type=kDefaultCD, //!< [in] Strict or relaxed. 
                                             //!< User can also set preservation medium for children CDs 
                                             //!< when creating them. (ex. kStrict | kDRAM)
                     uint32_t error_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                 //!< should be able to recover from that error type.
                     uint32_t error_loc_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                //!< should be able to recover from that error type.
                     CDErrT *error=0 //!< [in,out] Pointer for error return value 
                                     //!< (kOK on success and kError on failure) 
                                     //!< no error value returned if error=0.
                     );

 
/**
 * @brief Collective Create and Begin
 *
 *
 * @return Returns a pointer to the handle of the newly created
 * child CD; returns 0 on an error.
 *
 */
    CDHandle *CreateAndBeginSW(uint32_t onOff,        //!< [in] Tuning can be done with this switch.
                             uint32_t  numchildren, //!< [in] The total number of CDs that will be collectively created by the current CD object.
                                             //!< This collective CDHandle::Create() waits for all tasks in the current CD to arrive before creating new children.
                             const char *name, //!< [in] Optional user-specified name that can be used to "re-create" the same CD object
                                               //!< if it was not destroyed yet; useful for resuing preserved state in CD trees that are not loop based.
                             int cd_type=kDefaultCD, //!< [in] Strict or relaxed. 
                                                     //!< User can also set preservation medium for children CDs 
                                                     //!< when creating them. (ex. kStrict | kDRAM)
                             uint32_t error_name_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                         //!< should be able to recover from that error type.
                             uint32_t error_loc_mask=0, //!< [in] each `1` in the mask indicates that this CD
                                                        //!< should be able to recover from that error type.
                             CDErrT *error=0 //!< [in,out] Pointer for error return value 
                                             //!< (kOK on success and kError on failure) 
                                             //!< no error value returned if error=0.
                             );

  /** @brief Destroys a CD
   *
   * @return May return kError if instance was already destroyed, but
   * may also return kOK in such a case.
   *
   */
    CDErrT DestroySW(uint32_t onOff,        //!< [in] Tuning can be done with this switch.
                   bool collective=false //!< [in] if `true`, destroy is done as a collective across all tasks that
                                         //!< created the CD; otherwise the behavior is that only one task destroys 
                                         //!< the actual object while the rest just delete the local CDHandle.
                  );

  /** @brief Begins a CD 
   *
   *
   * @return Returns kOK when successful and kError otherwise.
   * @sa Complete()
   */
    CDErrT BeginSW(uint32_t onOff,        //!< [in] Tuning can be done with this switch.
                 bool collective=true,//!< [in] Specifies whether this call is a collective across all tasks 
                                      //!< contained by this CD or whether its to be run by a single task 
                                      //!< only with the programmer responsible for synchronization. 
                 const char *label=NULL,
                 const uint64_t &sys_err_vec=0
                );

  /** @brief Completes a CD 
   *
   * @return Returns kOK when successful and kError otherwise.
   * @sa Begin()
   */
    CDErrT CompleteSW(uint32_t onOff,        //!< [in] Tuning can be done with this switch.
                    bool collective=true, //!< [in] Specifies whether
                                         //!< this call is a collective
                                         //!< across all tasks contained
                                         //!< by this CD or whether its to
                                         //!< be run by a single task only
                                         //!< with the programmer
                                         //!< responsible for
                                         //!< synchronization
                    bool update_preservations=false //!< [in] Flag that
                                                //!< indicates whether preservation should be
                                                //!< updated on Complete rather than discarding all 
                                                //!< preserved state. If `true` then Complete
                                                //!< followed by Begin can be much more efficient
                                                //!< if the majority of preserved data overlaps
                                                //!< between these two consecutive uses of the CD
                                                //!< object (this enables the Cray CD
                                                //!< AdvancePointInTime functionality).
                   );
/**@brief (Non-collective) Preserve data to be restored when recovering. 
 *        (typically reexecuting the CD from right after its Begin() call.)
 *
 * @return kOK on success and kError otherwise.
 *
 * \sa Complete()
 */
    CDErrT PreserveSW(uint32_t onOff,   //!< [in] Tuning can be done with this switch.
                    void *data_ptr=0, //!< [in] pointer to data to be preserved;
                                      //!< __currently must be in same address space
                                      //!< as calling task, but will extend to PGAS fat pointers later 
                    uint64_t len=0,   //!< [in] Length of preserved data (Bytes)
                    uint32_t preserve_mask=kCopy, //!< [in] Allowed types of preservation 
                                                  //!< (e.g., kCopy|kRef|kRegen), default only via copy
                    const char *my_name=0,  //!< [in] Optional C-string representing the name of this
                                            //!< preserved data for later preserve-via-reference

                    const char *ref_name=0, //!< [in] Optional C-string representing
                                            //!< a user-specified name that was set by a previous preserve call at the parent.; 
                                            //!< __Do we also need an offset into parent preservation?__

                    uint64_t ref_offset=0,  //!< [in] explicit offset within the named region at the other CD (for restoration via reference)
                    const cd::RegenObject *regen_object=0, //!< [in] optional user-specified function for
                                                       //!< regenerating values instead of restoring by copying

                    PreserveUseT data_usage=kUnsure //!< [in] This flag is used
                                                    //!< to optimize consecutive Complete/Begin calls
                                                    //!< where there is significant overlap in
                                                    //!< preserved state that is unmodified (see Complete()).
                    );


/**@brief (Non-collective) Preserve user-defined class object
 * 
 * @return kOK on success and kError otherwise.
 *
 * \sa Serdes, Complete()
 */
    CDErrT PreserveSW(uint32_t onOff,       //!< [in] Tuning can be done with this switch.
                    cd::Serializable &serdes, //!< [in] Serdes object in user-defined class.
                                          //!< This will be invoked in CD runtime
                                          //!< to de/serialize class object
                    uint32_t preserve_mask=kCopy, //!< [in] Allowed types of preservation 
                                                  //!< (e.g., kCopy|kRef|kRegen), default only via copy
                    const char *my_name=0,  //!< [in] Optional C-string representing the name of this
                                            //!< preserved data for later preserve-via-reference

                    const char *ref_name=0, //!< [in] Optional C-string representing
                                            //!< a user-specified name that was set by a previous preserve call at the parent.; 
                                            //!< __Do we also need an offset into parent preservation?__
                    uint64_t ref_offset=0,  //!< [in] explicit offset within the named region at the other CD 
                                            //!< (for restoration via reference)
                    const cd::RegenObject *regen_object=0, //!< [in] optional user-specified function for
                                                       //!< regenerating values instead of restoring by copying

                    PreserveUseT data_usage=kUnsure //!< [in] This flag is used
                                                    //!< to optimize consecutive Complete/Begin calls
                                                    //!< where there is significant overlap in
                                                    //!< preserved state that is unmodified (see Complete()).
                    );

/** @brief (Not supported yet) Non-blocking preserve data to be restored when recovering 
 * (typically reexecuting the CD from right after its Begin() call.
 *
 * @return kOK on success and kError otherwise.
 *
 * \sa Complete()
 */
    CDErrT PreserveSW(uint32_t onOff,   //!< [in] Tuning can be done with this switch.
                    cd::CDEvent &cd_event, //!< [in,out] enqueue this call onto the cd_event
                    void *data_ptr=0, //!< [in] pointer to data to be preserved;
                                      //!< __currently must be in same address space as calling task, 
                                      //!< but will extend to PGAS fat pointers later
                    uint64_t len=0,   //!< [in] Length of preserved data (Bytes)
                    uint32_t preserve_mask=kCopy, //!< [in] Allowed types of preservation 
                                                  //!< (e.g.,kCopy|kRef|kRegen), default only via copy
                    const char *my_name=0, //!< [in] Optional C-string representing the name of this
                                           //!< preserved data for later preserve-via-reference
                    const char *ref_name=0, //!< [in] Optional C-string representing a user-specified name 
                                            //!< that was set by a previous preserve call at the parent.; 
                                            //!< __Do we also need an offset into parent preservation?__
                    uint64_t ref_offset=0, //!< [in] explicit offset within the named region at the other CD
                    const cd::RegenObject *regen_object=0, //!< [in] optional user-specified function for
                                                       //!< regenerating values instead of restoring by copying
                    PreserveUseT data_usage=kUnsure //!< [in] This flag is used to optimize consecutive Complete/Begin calls
                                                    //!< where there is significant overlap in preserved state 
                                                    //!< that is unmodified (see Complete()).
                    );



  /** @brief User-provided detection function for failing a CD
   * 
   * @return kOK when the assertion call completed successfully
   *
   */
    CDErrT CDAssertSW(uint32_t onOff,  //!< [in] Tuning can be done with this switch.
                    bool test_true, //!< [in] Boolean to be asserted as true.
                    const cd::SysErrT* error_to_report=0
                    //!< [in,out] An optional error report that will be
                    //!< used during recovery and for system diagnostics. 
                    );

  /** @brief User-provided detection function for failing a CD
   * 
   * @return kOK when the assertion call completed successfully
   *
   * \warning May not be implemented yet.
   */
    CDErrT CDAssertFailSW(uint32_t onOff, //!< [in] Tuning can be done with this switch.
        bool test_true, //!< [in] Boolean to be asserted
           //!< as true.
           const cd::SysErrT* error_to_report=0
           //!< [in,out] An optional error report that will be
           //!< used during recovery and for system
           //!< diagnostics. 
           );

  /** @brief User-provided detection function for failing a CD
   *
   * @return kOK when the assertion call completed successfully
   */
    CDErrT CDAssertNotifySW(uint32_t onOff, //!< [in] Tuning can be done with this switch.
        bool test_true, //!< [in] Boolean to be asserted
           //!< as true.
           const cd::SysErrT* error_to_report=0
           //!< [in,out] An optional error report that will be
           //!< used during recovery and for system
           //!< diagnostics.
           );

  
  /** @brief Check whether any errors occurred while CD the executed
   *
   * @return kOK when there is no failure. For optionally returning a CD runtime 
   * error code indicating some bug with Detect()
   * any errors or failures detected during this CDs execution.
   *
   */
    CDErrT DetectSW(uint32_t onOff, //!< [in] Tuning can be done with this switch.
         std::vector<cd::SysErrT> *err_vec=NULL
            //!< [in,out] Pointer to a error vector
            //!<  that is detected during this CDs execution.
                               );
#endif
