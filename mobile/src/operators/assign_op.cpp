/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#ifdef ASSIGN_OP

#include "operators/assign_op.h"

namespace paddle_mobile_lens {
namespace operators {

template <typename Dtype, typename T>
void AssignOp<Dtype, T>::InferShape() const {
  PADDLE_MOBILE_ENFORCE(this->param_.Input() != nullptr,
                        "Input (X) of Assign op should not be null.");
  PADDLE_MOBILE_ENFORCE(this->param_.Output() != nullptr,
                        "Output (Output) of Assign op should not be null.");
}

}  // namespace operators
}  // namespace paddle_mobile_lens

namespace ops = paddle_mobile_lens::operators;

#ifdef PADDLE_MOBILE_CPU
REGISTER_OPERATOR_CPU(assign, ops::AssignOp);
#endif

#endif  // ASSIGN_OP
