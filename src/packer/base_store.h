#ifndef _BASE_STORE_H
#define _BASE_STORE_H

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <utility>
#include "define.h"
#define ENTRY_TYPE_CNT 8
#define BASE_ENTRY_CNT 128
//#define DATA_GROW_UNIT 65536 // 64KB
#define DATA_GROW_UNIT 4096 // 4KB
#define TABLE_GROW_UNIT 4096 // 4KB (128 EntryT)
#define BUFFER_INSUFFICIENT 0xFFFFFFFFFFFFFFFF
namespace cd {

//enum PackerErrT {
//  kOK = 0,
//  kAlreadyAllocated,
//  kMallocFailed,
//  kAlreadyFree,
//  kNotFound,
//  kOutOfTable
//};

enum EntryType {
  kBaseEntry = 1,
  kCDEntry   = 2,
  kLogEntry  = 3,
};



//struct MagicStore {
//  uint64_t total_size_;
//  uint64_t table_offset_;
//  uint32_t entry_type_;
//  uint32_t reserved_;
//  uint64_t reserved2_; // 32 B
//  char pad_[480];
//  MagicStore(void) : total_size_(0), table_offset_(0), entry_type_(0) {}
//  MagicStore(uint64_t total_size, uint64_t table_offset=0, uint32_t entry_type=0) 
//    : total_size_(total_size), table_offset_(table_offset), entry_type_(entry_type) {}
//  void Print(void) {
//    printf("[%s] %lu %lu %u\n", __func__, total_size_, table_offset_, entry_type_);
//  }
//};

//struct SizeFieldType {
//  uint64_t size:48;
//  uint64_t attr:16;
//};
//
//union SizeType { 
//  unsigned long size;
//  SizeFieldType code;
//};


//    uint64_t id_   :64;
//    uint64_t size_  :48;
//    uint64_t attr_  :16;
//    uint64_t offset_:64;
//    char *   src;
// attr should be
// type   3bit(entrytype) 
// dirty  1bit
// medium 2bit (dram, file, remote, regen, ...)
// 
struct BaseEntry {
    uint64_t id_;
    uint64_t size_;
    uint64_t offset_;
    BaseEntry(void) : id_(0), size_(0), offset_(0) {}
    BaseEntry(uint64_t id, uint64_t size) 
      : id_(id), size_(size), offset_(0) {}
    BaseEntry(uint64_t id, uint64_t size, uint64_t offset) 
      : id_(id), size_(size), offset_(offset) {}
    BaseEntry(const BaseEntry &that) {
      copy(that);
    }
    BaseEntry &SetOffset(uint64_t offset) { offset_ = offset; return *this; }
    BaseEntry &SetSrc(char *src) { return *this; }
    BaseEntry &operator=(const BaseEntry &that) {
      copy(that);
      return *this;
    }
    void copy(const BaseEntry &that) {
      id_    = that.id_;
      size_   = that.size_;
      offset_ = that.offset_;
    }
    void Print(void) {
      printf("%12lx %12lx %12lx\n", id_, size_, offset_);
    }
    uint64_t size(void)   { return size_; }
    uint64_t offset(void) { return offset_; }
};

struct CDEntry {
    uint64_t id_;
    uint64_t size_;
    uint64_t offset_;
    char *src_;
    CDEntry(void) : id_(0), size_(0), offset_(0), src_(0) {}
    CDEntry(uint64_t id, uint64_t size, char *src) 
      : id_(id), size_(size), offset_(0), src_(src) {}
    CDEntry(uint64_t id, uint64_t size, uint64_t offset, char *src=0) 
      : id_(id), size_(size), offset_(offset), src_(src) {}
    CDEntry(const CDEntry &that) {
      copy(that);
    }
    CDEntry &SetOffset(uint64_t offset) { offset_ = offset; return *this; }
    CDEntry &SetSrc(char *src) { src_ = src; return *this; }
    CDEntry &operator=(const CDEntry &that) {
      copy(that);
      return *this;
    }
    void copy(const CDEntry &that) {
      id_    = that.id_;
      size_   = that.size_;
      offset_ = that.offset_;
      src_    = that.src_;
    }
    void Print(void) {
      printf("%12lx %12lx %12lx %p\n", id_, size_, offset_, src_);
    }
    uint64_t size(void)   { return size_; }
    uint64_t offset(void) { return offset_; }
    char    *src(void)    { return src_; }
};

//extern uint32_t entry_type[ENTRY_TYPE_CNT];

} // namespace cd ends


#endif