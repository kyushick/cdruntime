#ifndef _PHASE_TREE
#define _PHASE_TREE

#include <vector>
#include <list>
#include <map>
#include <cstdint>
#include "runtime_info.h"
#include "cd_def_debug.h"
//class PhaseMap : public std::map<std::string, uint32_t> {
//  public:
//    //std::string prev_phase_;
//    uint32_t phase_gen_;
//};
typedef std::map<std::string, uint32_t> PhasePathType;
typedef std::map<uint32_t, std::map<std::string, uint32_t>> PhaseMapType;

namespace common {
//struct MeasuredProf {
//
//};
// PhaseNode is crated when Begin() is invoked.
//
struct PhaseNode {
    // unique identifier for each node in phaseTree
    uint32_t level_;
    uint32_t phase_;

    // info in task space
    uint32_t sibling_id_;
    uint32_t sibling_size_;
    uint32_t task_id_;
    uint32_t task_size_;

    CDExecMode state_;

    // from execution profile. required for tuning 
    int64_t  count_;

    // from tuner
    int64_t  interval_;
    int64_t  errortype_;

    // links for phaseTree
    PhaseNode *parent_;
    // left and right is sort of caching 
    // *(--(parent->children_->find(this))) for left
    // *(++(parent->children_->find(this))) for right
    PhaseNode *left_;
    PhaseNode *right_;
    std::list<PhaseNode *> children_;

    // label corresponding to each node
    std::string label_;

    // from execution profile
    RuntimeInfo profile_;
    static uint32_t phase_gen;
    static uint32_t max_phase;
  public:
    // for cd::phaseTree
    PhaseNode(PhaseNode *parent, uint32_t level, const std::string &label, CDExecMode state) 
      : level_(level), phase_(phase_gen), 
        sibling_id_(0), sibling_size_(1), task_id_(0), task_size_(1), state_(state),
        count_(0), interval_(-1), errortype_(-1), label_(label), profile_(phase_gen)
    {
      TUNE_DEBUG("PhaseNode %s\n", label.c_str());
      Init(parent, level);
      phase_gen++;
    }

    // Creator for tuned::phaseTree
    // tuned::phaseTree is generated by the script from tuner before actully
    // executing application. 
    PhaseNode(PhaseNode *parent, uint32_t level, uint32_t phase)
      : level_(level), phase_(phase_gen), 
        sibling_id_(0), sibling_size_(1), task_id_(0), task_size_(1), state_(kExecution),
        count_(0), interval_(-1), errortype_(-1), profile_(phase_gen)
    {
      CD_ASSERT_STR(phase == phase_, "PhaseNode(%u == %u)\n", phase_, phase);
      Init(parent, level);
      phase_gen++;
    }

    void Init(PhaseNode *parent, uint32_t level)
    {
      if(parent != NULL) {
        parent_ = parent;
        if(parent_->children_.empty()) {
          left_  = NULL;
          right_ = NULL;
        } else {
          PhaseNode *prev_phase = parent_->children_.back();
          prev_phase->right_ = this;
          left_ = prev_phase;
          right_ = NULL;
        }
        parent_->AddChild(this);
      } else { // for root
        parent_ = NULL;
        left_   = NULL;
        right_  = NULL;
      }
    } 

    void Delete(void) 
    {
      // Recursively reach the leaves, then delete from them to root.
      for(auto it=children_.begin(); it!=children_.end(); ++it) {
        (*it)->Delete();
      }
      left_ = NULL;
      right_ = NULL;
      delete this;
    }

    PhaseNode *GetNextNode(const std::string &label) 
    {
      PhaseNode *next = NULL;
      TUNE_DEBUG("%s %s == %s\n", __func__, label.c_str(), label_.c_str());
      if(label_ == label) {
        next = this;
      } else {
        for(auto it=children_.rbegin(); it!=children_.rend(); ++it) {
          TUNE_DEBUG("%s == %s\n", label.c_str(), (*it)->label_.c_str());
          if((*it)->label_ == label) { next = *it; break; }
        }
      }
      CD_ASSERT(next);
      return next;
    }

    uint32_t GetPhaseNode(uint32_t level, const std::string &label);
    PhaseNode *GetLeftMostNode(void)
    {
      PhaseNode *leftmost = NULL;
      if(left_ != NULL && left_->interval_ == 0) {
        assert(left_ != this);
        TUNE_DEBUG("%u ", phase_);
        leftmost = left_->GetLeftMostNode();
        TUNE_DEBUG(" -> %u", phase_);
      } else {
        TUNE_DEBUG("Left %u", phase_);
        leftmost = this;
      }
      return leftmost;
    }

    PhaseNode *GetRightMostNode(void)
    {
      PhaseNode *rightmost = NULL;
      if(right_ != NULL && right_->interval_ == 0) {
        TUNE_DEBUG("%u -> ", phase_);
        rightmost = right_->GetRightMostNode();
      } else {
        TUNE_DEBUG("%u Right", phase_);
        rightmost = this;
      }
      return rightmost;
    }

    void AddChild(PhaseNode *child) 
    {
//      TUNE_DEBUG("%s %p (%s, %u)\n", __func__, child, child->label_.c_str(), child->phase_);
      children_.push_back(child);
    }
    void Print(void); 
    void PrintAll(void); 
    void PrintInputYAML(void); 
    void PrintOutputYAML(void); 
    void PrintProfile(void); 
    std::string GetPhasePath(void);
    std::string GetPhasePath(const std::string &label);

    // When failed_phase is reached after failure, 
    // reset state_ to kExecution and measure reexec_time_
    // First, it goes up to the most parent node which is failed,
    // then reset every descendant nodes from the node. 
    void FinishRecovery(CD_CLOCK_T now) 
    {
      CD_ASSERT(state_ == kReexecution);
      profile_.RecordRecoveryComplete(now);
      if(parent_ == NULL || parent_->state_ != kReexecution) { 
        ResetToExec();
      } else {
        FinishRecovery(now);
      }
    }
 
    // ResetToExec for every tasks 
    void ResetToExec(void) 
    {
      state_ = kExecution;
      for(auto it=children_.begin(); it!=children_.end(); ++it) {
        (*it)->ResetToExec();
      }
    }
};

struct PhaseTree {
    PhaseNode *root_;
    PhaseNode *current_;
  public:
    PhaseTree(void) : root_(NULL), current_(NULL) {}
    ~PhaseTree();
  
    uint32_t Init(uint64_t level,  const std::string &label)
    { 
      root_ = new PhaseNode(NULL, level, label, kExecution); 
      current_ = root_;
      return current_->phase_; 
    }
  
    void Print(int format=0) 
    {
      if(cd::myTaskID == 0) { 
      switch(0) {
        case 0: root_->PrintInputYAML(); break;
        case 1: root_->Print(); break;
      }
      }
    }
    
    void PrintProfile(void) 
    { root_->PrintProfile(); }
};

} // namespace common ends

using namespace common;

namespace cd {
// phaseMap generates phase ID for CDNameT at each level.
// Different label can create different phase,
// therefore CD runtime compares the label of current CD being created
// with that of preceding sequential CD at the same level. See GetPhase()
// 
// The scope of phase ID is limited to the corresponding level,
// which means that the same label at different levels are different phases.
extern common::PhaseTree phaseTree;
extern PhasePathType phasePath;
//extern std::vector<PhaseNode *> phaseNodeCache;
extern std::map<uint32_t, PhaseNode *> phaseNodeCache;
extern uint32_t new_phase;
} // namespace cd ends



namespace tuned {
extern common::PhaseTree phaseTree;     // Populated from config file
extern std::map<uint32_t, PhaseNode *> phaseNodeCache;
//extern PhaseMapType phaseMap;
extern PhasePathType phasePath;     // Populated from config file
                                    // TunedCDHandle will check tuned::phasePath
                                    // to determine the new phase at Begin
}

#endif
