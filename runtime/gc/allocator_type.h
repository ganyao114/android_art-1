/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ALLOCATOR_TYPE_H_
#define ART_RUNTIME_GC_ALLOCATOR_TYPE_H_

#include <iosfwd>

namespace art {
namespace gc {

// Different types of allocators.
enum AllocatorType {
  //表示在Bump Pointer Space中分配对象
  kAllocatorTypeBumpPointer,  // Use BumpPointer allocator, has entrypoints.
  //表示要在由Bump Pointer Space提供的线程局部分配缓冲区中分配对象
  kAllocatorTypeTLAB,  // Use TLAB allocator, has entrypoints.
  //表示要在Ros Alloc Space分配对象
  kAllocatorTypeRosAlloc,  // Use RosAlloc allocator, has entrypoints.
  //表示要在Dl Malloc Space分配对象
  kAllocatorTypeDlMalloc,  // Use dlmalloc allocator, has entrypoints.
  //表示要在Non Moving Space分配对象
  kAllocatorTypeNonMoving,  // Special allocator for non moving objects, doesn't have entrypoints.
  //表示要在Large Object Space分配对象
  kAllocatorTypeLOS,  // Large object space, also doesn't have entrypoints.
  kAllocatorTypeRegion,
  kAllocatorTypeRegionTLAB,
};
std::ostream& operator<<(std::ostream& os, const AllocatorType& rhs);

inline constexpr bool IsTLABAllocator(AllocatorType allocator) {
  return allocator == kAllocatorTypeTLAB || allocator == kAllocatorTypeRegionTLAB;
}

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ALLOCATOR_TYPE_H_
