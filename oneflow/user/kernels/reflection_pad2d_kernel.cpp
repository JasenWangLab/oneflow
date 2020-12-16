/*
Copyright 2020 The OneFlow Authors. All rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/device/memory_copier.h"
#include "oneflow/core/kernel/new_kernel_util.h"
#include "oneflow/core/common/nd_index_offset_helper.h"
#include "oneflow/user/kernels/reflection_pad2d_kernel_util.h"

namespace oneflow {
namespace user_op {


template<DeviceType device_type, typename T>
class ReflectionPad2dKernel final : public OpKernel {
 public:
  ReflectionPad2dKernel() = default;
  ~ReflectionPad2dKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const Tensor* x = ctx->Tensor4ArgNameAndIndex("x", 0);
    Tensor* y = ctx->Tensor4ArgNameAndIndex("y", 0);
    const auto& padding = ctx->Attr<std::vector<int64_t>>("padding");
    const std::string data_format = ctx->Attr<std::string>("data_format");
    const int64_t ndims = x->shape().NumAxes();
    CHECK_EQ(padding.size(), ndims);
    int64_t c_idx, h_idx, w_idx;
    if (data_format == "NCHW") {
      c_idx = 1; 
      h_idx = 2;
      w_idx = 3;
    } else {
      h_idx = 1;
      w_idx = 2;
      c_idx = 3;
    }

    int64_t pad_left = padding[w_idx];
    int64_t pad_top = padding[h_idx];

    int64_t n_batch = y->shape().At(0);
    int64_t n_channel = y->shape().At(c_idx);
    int64_t y_height = y->shape().At(h_idx);
    int64_t y_width = y->shape().At(w_idx);
    int64_t x_height = x->shape().At(h_idx);
    int64_t x_width = x->shape().At(w_idx);

    T * dest = y->mut_dptr<T>();
    const T* src = x->dptr<T>();


    ReflectionPad2dFunctor<device_type, T>()(
        ctx->device_ctx(), src, dest, n_batch, n_channel, 
        y_height, y_width, x_height, x_width, pad_left, pad_top
    );

  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_REFLECTION_PAD2D_KERNELS(dev, dtype)   \
  REGISTER_USER_KERNEL("reflection_pad2d")              \
      .SetCreateFn<ReflectionPad2dKernel<dev, dtype>>() \
      .SetIsMatchedHob((user_op::HobDeviceTag() == dev) \
                       & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value));

#ifdef WITH_CUDA
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kGPU, float)
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kGPU, double)
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kGPU, float16)
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kGPU, int32_t)
#endif
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kCPU, float)
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kCPU, double)
REGISTER_REFLECTION_PAD2D_KERNELS(DeviceType::kCPU, int32_t)

template<DeviceType device_type, typename T>
class ReflectionPad2dGradKernel final : public OpKernel {
 public:
  ReflectionPad2dGradKernel() = default;
  ~ReflectionPad2dGradKernel() = default;

 private:
  void Compute(KernelComputeContext* ctx) const override {
    const Tensor* dy = ctx->Tensor4ArgNameAndIndex("dy", 0);
    Tensor* dx = ctx->Tensor4ArgNameAndIndex("dx", 0);
    const auto& padding = ctx->Attr<std::vector<int64_t>>("padding");
    const std::string data_format = ctx->Attr<std::string>("data_format");
    const int64_t ndims = dy->shape().NumAxes();
    CHECK_EQ(padding.size(), ndims);

    int64_t c_idx, h_idx, w_idx;
    if (data_format == "NCHW") {
      c_idx = 1; 
      h_idx = 2;
      w_idx = 3;
    } else {
      h_idx = 1;
      w_idx = 2;
      c_idx = 3;
    }

  
    int64_t pad_left = padding[w_idx];
    int64_t pad_top = padding[h_idx];
    int64_t n_batch = dy->shape().At(0);
    int64_t n_channel = dy->shape().At(c_idx);
    int64_t dy_height = dy->shape().At(h_idx);
    int64_t dy_width = dy->shape().At(w_idx);
    int64_t dx_height = dx->shape().At(h_idx);
    int64_t dx_width = dx->shape().At(w_idx);

    const T* src = dy->dptr<T>();
    T * dest = dx->mut_dptr<T>();

    ReflectionPad2dGradFunctor<device_type, T>()(
      ctx->device_ctx(), src, dest, n_batch, n_channel, 
      dy_height, dy_width, dx_height, dx_width, pad_left, pad_top
    );

    


  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(dev, dtype)  \
  REGISTER_USER_KERNEL("reflection_pad2d_grad")             \
      .SetCreateFn<ReflectionPad2dGradKernel<dev, dtype>>() \
      .SetIsMatchedHob((user_op::HobDeviceTag() == dev)     \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

#ifdef WITH_CUDA
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kGPU, float)
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kGPU, double)
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kGPU, float16)
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kGPU, int32_t)
#endif
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kCPU, float)
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kCPU, double)
REGISTER_REFLECTION_PAD2D_GRAD_KERNELS(DeviceType::kCPU, int32_t)

}  // namespace user_op
}  // namespace oneflow