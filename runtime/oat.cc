/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "oat.h"

#include <string.h>
#include <zlib.h>

#include "android-base/stringprintf.h"

#include "arch/instruction_set_features.h"
#include "base/bit_utils.h"
#include "base/strlcpy.h"

namespace art {

using android::base::StringPrintf;

constexpr uint8_t OatHeader::kOatMagic[4];
constexpr uint8_t OatHeader::kOatVersion[4];
constexpr const char OatHeader::kTrueValue[];
constexpr const char OatHeader::kFalseValue[];

static size_t ComputeOatHeaderSize(const SafeMap<std::string, std::string>* variable_data) {
  size_t estimate = 0U;
  if (variable_data != nullptr) {
    SafeMap<std::string, std::string>::const_iterator it = variable_data->begin();
    SafeMap<std::string, std::string>::const_iterator end = variable_data->end();
    for ( ; it != end; ++it) {
      estimate += it->first.length() + 1;
      estimate += it->second.length() + 1;
    }
  }
  return sizeof(OatHeader) + estimate;
}

OatHeader* OatHeader::Create(InstructionSet instruction_set,
                             const InstructionSetFeatures* instruction_set_features,
                             uint32_t dex_file_count,
                             const SafeMap<std::string, std::string>* variable_data) {
  // Estimate size of optional data.
  size_t needed_size = ComputeOatHeaderSize(variable_data);

  // Reserve enough memory.
  void* memory = operator new (needed_size);

  // Create the OatHeader in-place.
  return new (memory) OatHeader(instruction_set,
                                instruction_set_features,
                                dex_file_count,
                                variable_data);
}

/**
       magic: 标志OAT文件的一个魔数，等于‘oat\n’。

       version: OAT文件版本号，目前的值等于‘007、0’。

       adler32_checksum_: OAT头部检验和。

       instruction_set_: 本地机指令集，有四种取值，分别为  kArm(1)、kThumb2(2)、kX86(3)和kMips(4)。

       dex_file_count_: OAT文件包含的DEX文件个数。

       executable_offset_: oatexec段开始位置与oatdata段开始位置的偏移值。

       interpreter_to_interpreter_bridge_offset_和interpreter_to_compiled_code_bridge_offset_: ART运行时在启动的时候，可以通过-Xint选项指定所有类的方法都是解释执行的，这与传统的虚拟机使用解释器来执行类方法差不多。同时，有些类方法可能没有被翻译成本地机器指令，这时候也要求对它们进行解释执行。这意味着解释执行的类方法在执行的过程中，可能会调用到另外一个也是解释执行的类方法，也可能调用到另外一个按本地机器指令执行的类方法中。OAT文件在内部提供有两段trampoline代码，分别用来从解释器调用另外一个也是通过解释器来执行的类方法和从解释器调用另外一个按照本地机器执行的类方法。这两段trampoline代码的偏移位置就保存在成员变量 interpreter_to_interpreter_bridge_offset_和interpreter_to_compiled_code_bridge_offset_。

       jni_dlsym_lookup_offset_: 类方法在执行的过程中，如果要调用另外一个方法是一个JNI函数，那么就要通过存在放置jni_dlsym_lookup_offset_的一段trampoline代码来调用。

       portable_resolution_trampoline_offset_和quick_resolution_trampoline_offset_: 用来在运行时解析还未链接的类方法的两段trampoline代码。其中，portable_resolution_trampoline_offset_指向的trampoline代码用于Portable类型的Backend生成的本地机器指令，而quick_resolution_trampoline_offset_用于Quick类型的Backend生成的本地机器指令。

       portable_to_interpreter_bridge_offset_和quick_to_interpreter_bridge_offset_: 与interpreter_to_interpreter_bridge_offset_和interpreter_to_compiled_code_bridge_offset_的作用刚好相反，用来在按照本地机器指令执行的类方法中调用解释执行的类方法的两段trampoline代码。其中，portable_to_interpreter_bridge_offset_用于Portable类型的Backend生成的本地机器指令，而quick_to_interpreter_bridge_offset_用于Quick类型的Backend生成的本地机器指令。

       由于每一个应用程序都会依赖于boot.art文件，因此为了节省由打包在应用程序里面的classes.dex生成的OAT文件的体积，上述interpreter_to_interpreter_bridge_offset_、interpreter_to_compiled_code_bridge_offset_、jni_dlsym_lookup_offset_、portable_resolution_trampoline_offset_、portable_to_interpreter_bridge_offset_、quick_resolution_trampoline_offset_和quick_to_interpreter_bridge_offset_七个成员变量指向的trampoline代码段只存在于boot.art文件中。换句话说，在由打包在应用程序里面的classes.dex生成的OAT文件的oatdata段头部中，上述七个成员变量的值均等于0。

       image_file_location_data_: 用来创建Image空间的文件的路径的在内存中的地址。

       image_file_location_size_: 用来创建Image空间的文件的路径的大小。

       image_file_location_oat_data_begin_: 用来创建Image空间的OAT文件的oatdata段在内存的位置。

       image_file_location_oat_checksum_:  用来创建Image空间的OAT文件的检验和。

       上述四个成员变量记录了一个OAT文件所依赖的用来创建Image空间文件以及创建这个Image空间文件所使用的OAT文件的相关信息。

       通过OatFile类的成员函数Setup的第一部分代码的分析，我们就知道了，OAT文件的oatdata段在最开始保存着一个OAT头
**/
OatHeader::OatHeader(InstructionSet instruction_set,
                     const InstructionSetFeatures* instruction_set_features,
                     uint32_t dex_file_count,
                     const SafeMap<std::string, std::string>* variable_data)
    : adler32_checksum_(adler32(0L, Z_NULL, 0)),
      instruction_set_(instruction_set),
      instruction_set_features_bitmap_(instruction_set_features->AsBitmap()),
      dex_file_count_(dex_file_count),
      oat_dex_files_offset_(0),
      executable_offset_(0),
      interpreter_to_interpreter_bridge_offset_(0),
      interpreter_to_compiled_code_bridge_offset_(0),
      jni_dlsym_lookup_offset_(0),
      quick_generic_jni_trampoline_offset_(0),
      quick_imt_conflict_trampoline_offset_(0),
      quick_resolution_trampoline_offset_(0),
      quick_to_interpreter_bridge_offset_(0),
      image_patch_delta_(0),
      image_file_location_oat_checksum_(0),
      image_file_location_oat_data_begin_(0) {
  // Don't want asserts in header as they would be checked in each file that includes it. But the
  // fields are private, so we check inside a method.
  static_assert(sizeof(magic_) == sizeof(kOatMagic),
                "Oat magic and magic_ have different lengths.");
  static_assert(sizeof(version_) == sizeof(kOatVersion),
                "Oat version and version_ have different lengths.");

  memcpy(magic_, kOatMagic, sizeof(kOatMagic));
  memcpy(version_, kOatVersion, sizeof(kOatVersion));

  CHECK_NE(instruction_set, InstructionSet::kNone);

  // Flatten the map. Will also update variable_size_data_size_.
  Flatten(variable_data);
}

bool OatHeader::IsValid() const {
  if (memcmp(magic_, kOatMagic, sizeof(kOatMagic)) != 0) {
    return false;
  }
  if (memcmp(version_, kOatVersion, sizeof(kOatVersion)) != 0) {
    return false;
  }
  if (!IsAligned<kPageSize>(executable_offset_)) {
    return false;
  }
  if (!IsAligned<kPageSize>(image_patch_delta_)) {
    return false;
  }
  if (!IsValidInstructionSet(instruction_set_)) {
    return false;
  }
  return true;
}

std::string OatHeader::GetValidationErrorMessage() const {
  if (memcmp(magic_, kOatMagic, sizeof(kOatMagic)) != 0) {
    static_assert(sizeof(kOatMagic) == 4, "kOatMagic has unexpected length");
    return StringPrintf("Invalid oat magic, expected 0x%x%x%x%x, got 0x%x%x%x%x.",
                        kOatMagic[0], kOatMagic[1], kOatMagic[2], kOatMagic[3],
                        magic_[0], magic_[1], magic_[2], magic_[3]);
  }
  if (memcmp(version_, kOatVersion, sizeof(kOatVersion)) != 0) {
    static_assert(sizeof(kOatVersion) == 4, "kOatVersion has unexpected length");
    return StringPrintf("Invalid oat version, expected 0x%x%x%x%x, got 0x%x%x%x%x.",
                        kOatVersion[0], kOatVersion[1], kOatVersion[2], kOatVersion[3],
                        version_[0], version_[1], version_[2], version_[3]);
  }
  if (!IsAligned<kPageSize>(executable_offset_)) {
    return "Executable offset not page-aligned.";
  }
  if (!IsAligned<kPageSize>(image_patch_delta_)) {
    return "Image patch delta not page-aligned.";
  }
  if (!IsValidInstructionSet(instruction_set_)) {
    return StringPrintf("Invalid instruction set, %d.", static_cast<int>(instruction_set_));
  }
  return "";
}

const char* OatHeader::GetMagic() const {
  CHECK(IsValid());
  return reinterpret_cast<const char*>(magic_);
}

uint32_t OatHeader::GetChecksum() const {
  CHECK(IsValid());
  return adler32_checksum_;
}

void OatHeader::UpdateChecksumWithHeaderData() {
  UpdateChecksum(&instruction_set_, sizeof(instruction_set_));
  UpdateChecksum(&instruction_set_features_bitmap_, sizeof(instruction_set_features_bitmap_));
  UpdateChecksum(&dex_file_count_, sizeof(dex_file_count_));
  UpdateChecksum(&image_file_location_oat_checksum_, sizeof(image_file_location_oat_checksum_));
  UpdateChecksum(&image_file_location_oat_data_begin_, sizeof(image_file_location_oat_data_begin_));

  // Update checksum for variable data size.
  UpdateChecksum(&key_value_store_size_, sizeof(key_value_store_size_));

  // Update for data, if existing.
  if (key_value_store_size_ > 0U) {
    UpdateChecksum(&key_value_store_, key_value_store_size_);
  }

  UpdateChecksum(&executable_offset_, sizeof(executable_offset_));
  UpdateChecksum(&interpreter_to_interpreter_bridge_offset_,
                 sizeof(interpreter_to_interpreter_bridge_offset_));
  UpdateChecksum(&interpreter_to_compiled_code_bridge_offset_,
                 sizeof(interpreter_to_compiled_code_bridge_offset_));
  UpdateChecksum(&jni_dlsym_lookup_offset_, sizeof(jni_dlsym_lookup_offset_));
  UpdateChecksum(&quick_generic_jni_trampoline_offset_,
                 sizeof(quick_generic_jni_trampoline_offset_));
  UpdateChecksum(&quick_imt_conflict_trampoline_offset_,
                 sizeof(quick_imt_conflict_trampoline_offset_));
  UpdateChecksum(&quick_resolution_trampoline_offset_,
                 sizeof(quick_resolution_trampoline_offset_));
  UpdateChecksum(&quick_to_interpreter_bridge_offset_,
                 sizeof(quick_to_interpreter_bridge_offset_));
}

void OatHeader::UpdateChecksum(const void* data, size_t length) {
  DCHECK(IsValid());
  if (data != nullptr) {
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    adler32_checksum_ = adler32(adler32_checksum_, bytes, length);
  } else {
    DCHECK_EQ(0U, length);
  }
}

InstructionSet OatHeader::GetInstructionSet() const {
  CHECK(IsValid());
  return instruction_set_;
}

uint32_t OatHeader::GetInstructionSetFeaturesBitmap() const {
  CHECK(IsValid());
  return instruction_set_features_bitmap_;
}

uint32_t OatHeader::GetOatDexFilesOffset() const {
  DCHECK(IsValid());
  DCHECK_GT(oat_dex_files_offset_, sizeof(OatHeader));
  return oat_dex_files_offset_;
}

void OatHeader::SetOatDexFilesOffset(uint32_t oat_dex_files_offset) {
  DCHECK_GT(oat_dex_files_offset, sizeof(OatHeader));
  DCHECK(IsValid());
  DCHECK_EQ(oat_dex_files_offset_, 0u);

  oat_dex_files_offset_ = oat_dex_files_offset;
}

uint32_t OatHeader::GetExecutableOffset() const {
  DCHECK(IsValid());
  DCHECK_ALIGNED(executable_offset_, kPageSize);
  CHECK_GT(executable_offset_, sizeof(OatHeader));
  return executable_offset_;
}

void OatHeader::SetExecutableOffset(uint32_t executable_offset) {
  DCHECK_ALIGNED(executable_offset, kPageSize);
  CHECK_GT(executable_offset, sizeof(OatHeader));
  DCHECK(IsValid());
  DCHECK_EQ(executable_offset_, 0U);

  executable_offset_ = executable_offset;
}

const void* OatHeader::GetInterpreterToInterpreterBridge() const {
  return reinterpret_cast<const uint8_t*>(this) + GetInterpreterToInterpreterBridgeOffset();
}

uint32_t OatHeader::GetInterpreterToInterpreterBridgeOffset() const {
  DCHECK(IsValid());
  CHECK(interpreter_to_interpreter_bridge_offset_ == 0 ||
        interpreter_to_interpreter_bridge_offset_ >= executable_offset_);
  return interpreter_to_interpreter_bridge_offset_;
}

void OatHeader::SetInterpreterToInterpreterBridgeOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= executable_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(interpreter_to_interpreter_bridge_offset_, 0U) << offset;

  interpreter_to_interpreter_bridge_offset_ = offset;
}

const void* OatHeader::GetInterpreterToCompiledCodeBridge() const {
  return reinterpret_cast<const uint8_t*>(this) + GetInterpreterToCompiledCodeBridgeOffset();
}

uint32_t OatHeader::GetInterpreterToCompiledCodeBridgeOffset() const {
  DCHECK(IsValid());
  CHECK_GE(interpreter_to_compiled_code_bridge_offset_, interpreter_to_interpreter_bridge_offset_);
  return interpreter_to_compiled_code_bridge_offset_;
}

void OatHeader::SetInterpreterToCompiledCodeBridgeOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= interpreter_to_interpreter_bridge_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(interpreter_to_compiled_code_bridge_offset_, 0U) << offset;

  interpreter_to_compiled_code_bridge_offset_ = offset;
}

const void* OatHeader::GetJniDlsymLookup() const {
  return reinterpret_cast<const uint8_t*>(this) + GetJniDlsymLookupOffset();
}

uint32_t OatHeader::GetJniDlsymLookupOffset() const {
  DCHECK(IsValid());
  CHECK_GE(jni_dlsym_lookup_offset_, interpreter_to_compiled_code_bridge_offset_);
  return jni_dlsym_lookup_offset_;
}

void OatHeader::SetJniDlsymLookupOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= interpreter_to_compiled_code_bridge_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(jni_dlsym_lookup_offset_, 0U) << offset;

  jni_dlsym_lookup_offset_ = offset;
}

const void* OatHeader::GetQuickGenericJniTrampoline() const {
  return reinterpret_cast<const uint8_t*>(this) + GetQuickGenericJniTrampolineOffset();
}

uint32_t OatHeader::GetQuickGenericJniTrampolineOffset() const {
  DCHECK(IsValid());
  CHECK_GE(quick_generic_jni_trampoline_offset_, jni_dlsym_lookup_offset_);
  return quick_generic_jni_trampoline_offset_;
}

void OatHeader::SetQuickGenericJniTrampolineOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= jni_dlsym_lookup_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(quick_generic_jni_trampoline_offset_, 0U) << offset;

  quick_generic_jni_trampoline_offset_ = offset;
}

const void* OatHeader::GetQuickImtConflictTrampoline() const {
  return reinterpret_cast<const uint8_t*>(this) + GetQuickImtConflictTrampolineOffset();
}

uint32_t OatHeader::GetQuickImtConflictTrampolineOffset() const {
  DCHECK(IsValid());
  CHECK_GE(quick_imt_conflict_trampoline_offset_, quick_generic_jni_trampoline_offset_);
  return quick_imt_conflict_trampoline_offset_;
}

void OatHeader::SetQuickImtConflictTrampolineOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= quick_generic_jni_trampoline_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(quick_imt_conflict_trampoline_offset_, 0U) << offset;

  quick_imt_conflict_trampoline_offset_ = offset;
}

const void* OatHeader::GetQuickResolutionTrampoline() const {
  return reinterpret_cast<const uint8_t*>(this) + GetQuickResolutionTrampolineOffset();
}

uint32_t OatHeader::GetQuickResolutionTrampolineOffset() const {
  DCHECK(IsValid());
  CHECK_GE(quick_resolution_trampoline_offset_, quick_imt_conflict_trampoline_offset_);
  return quick_resolution_trampoline_offset_;
}

void OatHeader::SetQuickResolutionTrampolineOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= quick_imt_conflict_trampoline_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(quick_resolution_trampoline_offset_, 0U) << offset;

  quick_resolution_trampoline_offset_ = offset;
}

const void* OatHeader::GetQuickToInterpreterBridge() const {
  return reinterpret_cast<const uint8_t*>(this) + GetQuickToInterpreterBridgeOffset();
}

uint32_t OatHeader::GetQuickToInterpreterBridgeOffset() const {
  DCHECK(IsValid());
  CHECK_GE(quick_to_interpreter_bridge_offset_, quick_resolution_trampoline_offset_);
  return quick_to_interpreter_bridge_offset_;
}

void OatHeader::SetQuickToInterpreterBridgeOffset(uint32_t offset) {
  CHECK(offset == 0 || offset >= quick_resolution_trampoline_offset_);
  DCHECK(IsValid());
  DCHECK_EQ(quick_to_interpreter_bridge_offset_, 0U) << offset;

  quick_to_interpreter_bridge_offset_ = offset;
}

int32_t OatHeader::GetImagePatchDelta() const {
  CHECK(IsValid());
  return image_patch_delta_;
}

void OatHeader::RelocateOat(off_t delta) {
  CHECK(IsValid());
  CHECK_ALIGNED(delta, kPageSize);
  image_patch_delta_ += delta;
  if (image_file_location_oat_data_begin_ != 0) {
    image_file_location_oat_data_begin_ += delta;
  }
}

void OatHeader::SetImagePatchDelta(int32_t off) {
  CHECK(IsValid());
  CHECK_ALIGNED(off, kPageSize);
  image_patch_delta_ = off;
}

uint32_t OatHeader::GetImageFileLocationOatChecksum() const {
  CHECK(IsValid());
  return image_file_location_oat_checksum_;
}

void OatHeader::SetImageFileLocationOatChecksum(uint32_t image_file_location_oat_checksum) {
  CHECK(IsValid());
  image_file_location_oat_checksum_ = image_file_location_oat_checksum;
}

uint32_t OatHeader::GetImageFileLocationOatDataBegin() const {
  CHECK(IsValid());
  return image_file_location_oat_data_begin_;
}

void OatHeader::SetImageFileLocationOatDataBegin(uint32_t image_file_location_oat_data_begin) {
  CHECK(IsValid());
  CHECK_ALIGNED(image_file_location_oat_data_begin, kPageSize);
  image_file_location_oat_data_begin_ = image_file_location_oat_data_begin;
}

uint32_t OatHeader::GetKeyValueStoreSize() const {
  CHECK(IsValid());
  return key_value_store_size_;
}

const uint8_t* OatHeader::GetKeyValueStore() const {
  CHECK(IsValid());
  return key_value_store_;
}

// Advance start until it is either end or \0.
static const char* ParseString(const char* start, const char* end) {
  while (start < end && *start != 0) {
    start++;
  }
  return start;
}

const char* OatHeader::GetStoreValueByKey(const char* key) const {
  const char* ptr = reinterpret_cast<const char*>(&key_value_store_);
  const char* end = ptr + key_value_store_size_;

  while (ptr < end) {
    // Scan for a closing zero.
    const char* str_end = ParseString(ptr, end);
    if (str_end < end) {
      if (strcmp(key, ptr) == 0) {
        // Same as key. Check if value is OK.
        if (ParseString(str_end + 1, end) < end) {
          return str_end + 1;
        }
      } else {
        // Different from key. Advance over the value.
        ptr = ParseString(str_end + 1, end) + 1;
      }
    } else {
      break;
    }
  }
  // Not found.
  return nullptr;
}

bool OatHeader::GetStoreKeyValuePairByIndex(size_t index, const char** key,
                                            const char** value) const {
  const char* ptr = reinterpret_cast<const char*>(&key_value_store_);
  const char* end = ptr + key_value_store_size_;
  ssize_t counter = static_cast<ssize_t>(index);

  while (ptr < end && counter >= 0) {
    // Scan for a closing zero.
    const char* str_end = ParseString(ptr, end);
    if (str_end < end) {
      const char* maybe_key = ptr;
      ptr = ParseString(str_end + 1, end) + 1;
      if (ptr <= end) {
        if (counter == 0) {
          *key = maybe_key;
          *value = str_end + 1;
          return true;
        } else {
          counter--;
        }
      } else {
        return false;
      }
    } else {
      break;
    }
  }
  // Not found.
  return false;
}

size_t OatHeader::GetHeaderSize() const {
  return sizeof(OatHeader) + key_value_store_size_;
}

bool OatHeader::IsPic() const {
  return IsKeyEnabled(OatHeader::kPicKey);
}

bool OatHeader::IsDebuggable() const {
  return IsKeyEnabled(OatHeader::kDebuggableKey);
}

bool OatHeader::IsConcurrentCopying() const {
  return IsKeyEnabled(OatHeader::kConcurrentCopying);
}

bool OatHeader::IsNativeDebuggable() const {
  return IsKeyEnabled(OatHeader::kNativeDebuggableKey);
}

CompilerFilter::Filter OatHeader::GetCompilerFilter() const {
  CompilerFilter::Filter filter;
  const char* key_value = GetStoreValueByKey(kCompilerFilter);
  CHECK(key_value != nullptr) << "compiler-filter not found in oat header";
  CHECK(CompilerFilter::ParseCompilerFilter(key_value, &filter))
      << "Invalid compiler-filter in oat header: " << key_value;
  return filter;
}

bool OatHeader::KeyHasValue(const char* key, const char* value, size_t value_size) const {
  const char* key_value = GetStoreValueByKey(key);
  return (key_value != nullptr && strncmp(key_value, value, value_size) == 0);
}

bool OatHeader::IsKeyEnabled(const char* key) const {
  return KeyHasValue(key, kTrueValue, sizeof(kTrueValue));
}

void OatHeader::Flatten(const SafeMap<std::string, std::string>* key_value_store) {
  char* data_ptr = reinterpret_cast<char*>(&key_value_store_);
  if (key_value_store != nullptr) {
    SafeMap<std::string, std::string>::const_iterator it = key_value_store->begin();
    SafeMap<std::string, std::string>::const_iterator end = key_value_store->end();
    for ( ; it != end; ++it) {
      strlcpy(data_ptr, it->first.c_str(), it->first.length() + 1);
      data_ptr += it->first.length() + 1;
      strlcpy(data_ptr, it->second.c_str(), it->second.length() + 1);
      data_ptr += it->second.length() + 1;
    }
  }
  key_value_store_size_ = data_ptr - reinterpret_cast<char*>(&key_value_store_);
}

OatMethodOffsets::OatMethodOffsets(uint32_t code_offset) : code_offset_(code_offset) {
}

OatMethodOffsets::~OatMethodOffsets() {}

}  // namespace art
