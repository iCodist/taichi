#pragma once
#include "common.h"
#include <mutex>
#include <vector>

TLANG_NAMESPACE_BEGIN

class UnifiedAllocator;

UnifiedAllocator *&allocator();

class UnifiedAllocator {
  std::vector<char> _data;
  void *_cuda_data{};
  std::size_t size;
  bool gpu;

  // put these two on the unified memroy so that GPU can have access
 public:
  void *data;
  void **head;
  void **tail;
  std::mutex lock;

 public:
  UnifiedAllocator() {
    data = nullptr;
  }

  UnifiedAllocator(std::size_t size, bool gpu);

#if defined(TC_GPU)
  __device__ static void *alloc_gpu(void *&head, int size) {
    return (void *)atomicAdd(reinterpret_cast<unsigned long long *>(&head),
                             size);
  }
#endif

  __host__ void *alloc(std::size_t size) {
    std::lock_guard<std::mutex> _(lock);
    auto ret = *head;
    *head = (char *)(*head) + size;
    return ret;
  }

  ~UnifiedAllocator();

  void memset(unsigned char val);

  bool initialized() const {
    return data != nullptr;
  }

  UnifiedAllocator operator=(const UnifiedAllocator &) = delete;

  static void create();

  static void free();
};

TC_FORCE_INLINE __host__ __device__ void *allocate(std::size_t size) {
#if __CUDA_ARCH__
  auto addr = allocator()->alloc_gpu(*device_head, size);
#else
  auto addr = allocator()->alloc(size);
#endif
  return addr;
}

template <typename T>
TC_FORCE_INLINE __host__ __device__ T *allocate() {
  auto addr = allocate(sizeof(T));
  return new (addr) T();
}

template <typename T, typename... Args>
__device__ __host__ T *create_unified(Args&& ...args) {
  auto addr = allocate(sizeof(T));
  return new (addr) T(std::forward<Args>(args)...);
}

TLANG_NAMESPACE_END
