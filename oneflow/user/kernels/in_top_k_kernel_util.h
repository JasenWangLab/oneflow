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
#ifndef ONEFLOW_USER_KERNELS_IN_TOP_K_KERNEL_UTIL_H_
#define ONEFLOW_USER_KERNELS_IN_TOP_K_KERNEL_UTIL_H_

#include "oneflow/core/device/device_context.h"

namespace oneflow {

template<DeviceType device_type, typename T, typename K>
struct InTopkKernelUtil {
  static void InTopk(DeviceCtx* ctx, const int classes_num, const T* targets, const K* predictions,
                     const int k, int8_t* out);
};

#define INSTANTIATE_IN_TOP_K_FUNCTOR(device_type_v, dtype_pair, pred_dtype_pair) \
  template struct InTopkKernelUtil<device_type_v, OF_PP_PAIR_FIRST(dtype_pair),  \
                                   OF_PP_PAIR_FIRST(pred_dtype_pair)>;

}  // namespace oneflow

#endif  // ONEFLOW_USER_KERNELS_IN_TOP_K_KERNEL_UTIL_H_
