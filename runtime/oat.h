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

#ifndef ART_RUNTIME_OAT_H_
#define ART_RUNTIME_OAT_H_

#include <vector>

#include "arch/instruction_set.h"
#include "base/macros.h"
#include "base/safe_map.h"
#include "compiler_filter.h"
#include "dex/dex_file.h"

namespace art {

class InstructionSetFeatures;

class PACKED(4) OatHeader {
 public:
  static constexpr uint8_t kOatMagic[] = { 'o', 'a', 't', '\n' };
  // Last oat version changed reason: Math.pow() intrinsic.
  static constexpr uint8_t kOatVersion[] = { '1', '3', '8', '\0' };

  static constexpr const char* kImageLocationKey = "image-location";
  static constexpr const char* kDex2OatCmdLineKey = "dex2oat-cmdline";
  static constexpr const char* kDex2OatHostKey = "dex2oat-host";
  static constexpr const char* kPicKey = "pic";
  static constexpr const char* kDebuggableKey = "debuggable";
  static constexpr const char* kNativeDebuggableKey = "native-debuggable";
  static constexpr const char* kCompilerFilter = "compiler-filter";
  static constexpr const char* kClassPathKey = "classpath";
  static constexpr const char* kBootClassPathKey = "bootclasspath";
  static constexpr const char* kConcurrentCopying = "concurrent-copying";
  static constexpr const char* kCompilationReasonKey = "compilation-reason";

  static constexpr const char kTrueValue[] = "true";
  static constexpr const char kFalseValue[] = "false";


  static OatHeader* Create(InstructionSet instruction_set,
                           const InstructionSetFeatures* instruction_set_features,
                           uint32_t dex_file_count,
                           const SafeMap<std::string, std::string>* variable_data);

  bool IsValid() const;
  std::string GetValidationErrorMessage() const;
  const char* GetMagic() const;
  uint32_t GetChecksum() const;
  void UpdateChecksumWithHeaderData();
  void UpdateChecksum(const void* data, size_t length);
  uint32_t GetDexFileCount() const {
    DCHECK(IsValid());
    return dex_file_count_;
  }
  uint32_t GetOatDexFilesOffset() const;
  void SetOatDexFilesOffset(uint32_t oat_dex_files_offset);
  uint32_t GetExecutableOffset() const;
  void SetExecutableOffset(uint32_t executable_offset);

  const void* GetInterpreterToInterpreterBridge() const;
  uint32_t GetInterpreterToInterpreterBridgeOffset() const;
  void SetInterpreterToInterpreterBridgeOffset(uint32_t offset);
  const void* GetInterpreterToCompiledCodeBridge() const;
  uint32_t GetInterpreterToCompiledCodeBridgeOffset() const;
  void SetInterpreterToCompiledCodeBridgeOffset(uint32_t offset);

  const void* GetJniDlsymLookup() const;
  uint32_t GetJniDlsymLookupOffset() const;
  void SetJniDlsymLookupOffset(uint32_t offset);

  const void* GetQuickGenericJniTrampoline() const;
  uint32_t GetQuickGenericJniTrampolineOffset() const;
  void SetQuickGenericJniTrampolineOffset(uint32_t offset);
  const void* GetQuickResolutionTrampoline() const;
  uint32_t GetQuickResolutionTrampolineOffset() const;
  void SetQuickResolutionTrampolineOffset(uint32_t offset);
  const void* GetQuickImtConflictTrampoline() const;
  uint32_t GetQuickImtConflictTrampolineOffset() const;
  void SetQuickImtConflictTrampolineOffset(uint32_t offset);
  const void* GetQuickToInterpreterBridge() const;
  uint32_t GetQuickToInterpreterBridgeOffset() const;
  void SetQuickToInterpreterBridgeOffset(uint32_t offset);

  int32_t GetImagePatchDelta() const;
  void RelocateOat(off_t delta);
  void SetImagePatchDelta(int32_t off);

  InstructionSet GetInstructionSet() const;
  uint32_t GetInstructionSetFeaturesBitmap() const;

  uint32_t GetImageFileLocationOatChecksum() const;
  void SetImageFileLocationOatChecksum(uint32_t image_file_location_oat_checksum);
  uint32_t GetImageFileLocationOatDataBegin() const;
  void SetImageFileLocationOatDataBegin(uint32_t image_file_location_oat_data_begin);

  uint32_t GetKeyValueStoreSize() const;
  const uint8_t* GetKeyValueStore() const;
  const char* GetStoreValueByKey(const char* key) const;
  bool GetStoreKeyValuePairByIndex(size_t index, const char** key, const char** value) const;

  size_t GetHeaderSize() const;
  bool IsPic() const;
  bool IsDebuggable() const;
  bool IsNativeDebuggable() const;
  CompilerFilter::Filter GetCompilerFilter() const;
  bool IsConcurrentCopying() const;

 private:
  bool KeyHasValue(const char* key, const char* value, size_t value_size) const;

  OatHeader(InstructionSet instruction_set,
            const InstructionSetFeatures* instruction_set_features,
            uint32_t dex_file_count,
            const SafeMap<std::string, std::string>* variable_data);

  // Returns true if the value of the given key is "true", false otherwise.
  bool IsKeyEnabled(const char* key) const;

  void Flatten(const SafeMap<std::string, std::string>* variable_data);

  //标志OAT文件的一个魔数，等于‘oat\n’
  uint8_t magic_[4];
  //OAT文件版本号
  uint8_t version_[4];
  //OAT头部检验和
  uint32_t adler32_checksum_;
  //本地机指令集，为  kArm(1)、kThumb2(2)、kX86(3)和kMips(4) 等
  InstructionSet instruction_set_;
  uint32_t instruction_set_features_bitmap_;
  //OAT文件包含的DEX文件个数
  uint32_t dex_file_count_;
  //oatexec段开始位置与oatdata段开始位置的偏移值
  uint32_t oat_dex_files_offset_;
  uint32_t executable_offset_;
  //ART运行时在启动的时候，可以通过-Xint选项指定所有类的方法都是解释执行的，这与传统的虚拟机使用解释器来执行类方法差不多。同时，有些类方法可能没有被翻译成本地机器指令，这时候也要求对它们进行解释执行。这意味着解释执行的类方法在执行的过程中，可能会调用到另外一个也是解释执行的类方法，也可能调用到另外一个按本地机器指令执行的类方法中。OAT文件在内部提供有两段trampoline代码，分别用来从解释器调用另外一个也是通过解释器来执行的类方法和从解释器调用另外一个按照本地机器执行的类方法。这两段trampoline代码的偏移位置就保存在成员变量 interpreter_to_interpreter_bridge_offset_和interpreter_to_compiled_code_bridge_offset_。
  uint32_t interpreter_to_interpreter_bridge_offset_;
  uint32_t interpreter_to_compiled_code_bridge_offset_;
  //类方法在执行的过程中，如果要调用另外一个方法是一个JNI函数，那么就要通过存在放置jni_dlsym_lookup_offset_的一段trampoline代码来调用
  uint32_t jni_dlsym_lookup_offset_;
  //quick oat 方法 ->(箭头表示调用跳转) jni 方法的 trampoline 代码的偏移位置
  uint32_t quick_generic_jni_trampoline_offset_;
  //quick oat 方法 -> imt(接口方法/虚方法 > 43) 方法的 trampoline 代码的偏移位置
  /**
    IMT，即 Interface Method Table，保存着一个 Class “实现的” 所有接口函数对应的 ArtMethod 指针，线性表：
    在 7.0上，这个表是镶嵌在一个对象的 Class所在的内存中的，而在 8.0上，在class中只记录了这个数组的首地址，
    数组开辟在另外的内存位置；7.0上，table大小是 64，在 8.0上，table大小是 43，这些数据可能是 google根据经验得来的；
    这个 table应该只是为了性能，为了更快的调用接口函数而准备的一个table，用来记录一定数量的接口函数的实现。
    那么问题来了，如果接口函数超过这个数量(64/43)后，IMT中存放不完了，怎么办？【这就引出了后面的 IMT Conflict Table】
    
    这个table里保存的 ArtMethod有两种情况：
      1.一种是真正的函数实现，在查找到时，直接使用即可；
      2.一种是一个 Runtime Method： 
    可能是 imt_unimplemented_method_ 或者 imt_conflict_method_ ，（这两个函数是在虚拟机创建的时候生成的）
    可能是 运行时新 Resolve Interface 函数后，新创建的 conflict method，
    也可能是 quick alloc array 相关的函数实现 art_quick_alloc_array_resolved32_region_tlab；
   **/
  uint32_t quick_imt_conflict_trampoline_offset_;
  //quick oat 方法 -> 静态类方法的 trampoline 代码的偏移位置,静态类中方法是延迟 resolve 的，需要在调用的时候初始化静态类，就是 resolve 的过程
  uint32_t quick_resolution_trampoline_offset_;
  //quick oat 方法 -> 解释执行的方法的 trampoline 代码的偏移位置
  uint32_t quick_to_interpreter_bridge_offset_;

  // The amount that the image this oat is associated with has been patched.
  int32_t image_patch_delta_;
  //用来创建Image空间的OAT文件的检验和
  uint32_t image_file_location_oat_checksum_;
  //用来创建Image空间的OAT文件的oatdata段在内存的位置。
  uint32_t image_file_location_oat_data_begin_;

  uint32_t key_value_store_size_;
  uint8_t key_value_store_[0];  // note variable width data at end
  //结束并且对齐
  DISALLOW_COPY_AND_ASSIGN(OatHeader);
};

// OatMethodOffsets are currently 5x32-bits=160-bits long, so if we can
// save even one OatMethodOffsets struct, the more complicated encoding
// using a bitmap pays for itself since few classes will have 160
// methods.
enum OatClassType {
  kOatClassAllCompiled = 0,   // OatClass is followed by an OatMethodOffsets for each method.
  kOatClassSomeCompiled = 1,  // A bitmap of which OatMethodOffsets are present follows the OatClass.
  kOatClassNoneCompiled = 2,  // All methods are interpreted so no OatMethodOffsets are necessary.
  kOatClassMax = 3,
};

std::ostream& operator<<(std::ostream& os, const OatClassType& rhs);

class PACKED(4) OatMethodOffsets {
 public:
  explicit OatMethodOffsets(uint32_t code_offset = 0);

  ~OatMethodOffsets();

  OatMethodOffsets(const OatMethodOffsets&) = default;
  OatMethodOffsets& operator=(const OatMethodOffsets&) = default;

  uint32_t code_offset_;
};

}  // namespace art

#endif  // ART_RUNTIME_OAT_H_
