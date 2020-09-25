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
#include "oneflow/core/kernel/new_kernel_util.h"

namespace oneflow {

template<typename TargetT, typename T>
class CpuInTopkKernel final : public user_op::OpKernel {
 public:
  CpuInTopkKernel() = default;
  ~CpuInTopkKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* targets = ctx->Tensor4ArgNameAndIndex("targets", 0);
    const user_op::Tensor* predictions = ctx->Tensor4ArgNameAndIndex("predictions", 0);
    const int32_t k = ctx->Attr<int32_t>("k");
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);

    CHECK_EQ(targets->shape().At(0), predictions->shape().At(0));

    const TargetT* target_ptr = targets->dptr<TargetT>();
    const T* prediction_ptr = predictions->dptr<T>();
    int8_t* out_ptr = out->mut_dptr<int8_t>();

    const int32_t targets_num = predictions->shape().At(0);
    const int32_t classes_num = predictions->shape().At(1);
    FOR_RANGE(int32_t, batch_idx, 0, targets_num) {
      TargetT target = target_ptr[batch_idx];

      bool cannot_say = (target >= classes_num)
                        || !std::isfinite(prediction_ptr[batch_idx * classes_num + target]);

      int32_t more_probable_classes = 0;
      if (!cannot_say) {
        const T target_prediction = prediction_ptr[batch_idx * classes_num + target];
        FOR_RANGE(int32_t, class_idx, 0, classes_num) {
          T pred = prediction_ptr[batch_idx * classes_num + class_idx];

          if (!std::isfinite(pred)) {
            cannot_say = true;
            break;
          } else if (pred > target_prediction) {
            ++more_probable_classes;
            if (more_probable_classes > k) break;
          }
        }
      }
      out_ptr[batch_idx] = cannot_say ? false : (more_probable_classes < k);
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_CPU_IN_TOP_K_KERNEL(target_dtype, dtype)                                         \
  REGISTER_USER_KERNEL("in_top_k")                                                                \
      .SetCreateFn<CpuInTopkKernel<target_dtype, dtype>>()                                        \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                                         \
                       & (user_op::HobDataType("targets", 0) == GetDataType<target_dtype>::value) \
                       & (user_op::HobDataType("predictions", 0) == GetDataType<dtype>::value));

REGISTER_CPU_IN_TOP_K_KERNEL(int32_t, float)
REGISTER_CPU_IN_TOP_K_KERNEL(int64_t, float)

}  // namespace oneflow
