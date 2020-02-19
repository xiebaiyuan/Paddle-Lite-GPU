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

#include "../test_include.h"
#include "operators/logical_op.h"

namespace paddle_mobile_lens {

void LogicalAnd(const framework::Tensor *inputX,
                const framework::Tensor *inputY, framework::Tensor *output) {
  auto x_data = inputX->data<bool>();
  auto y_data = inputY->data<bool>();
  auto output_data = output->data<bool>();
  for (int i = 0; i < inputX->numel(); ++i) {
    *output_data = *x_data && *y_data;
    x_data++;
    y_data++;
    output_data++;
  }
}

int TestLogicalAndOp(const std::vector<int> input_shape) {
  framework::DDim input_dims = framework::make_ddim(input_shape);
  VariableNameMap inputs;
  VariableNameMap outputs;
  auto scope = std::make_shared<framework::Scope>();
  inputs["X"] = std::vector<std::string>({"inputX"});
  inputs["Y"] = std::vector<std::string>({"inputY"});
  outputs["Out"] = std::vector<std::string>({"output"});

  auto x_var = scope.get()->Var("inputX");
  auto x = x_var->template GetMutable<framework::LoDTensor>();
  SetupTensor<bool>(x, input_dims, 0, 1);

  auto y_var = scope.get()->Var("inputY");
  auto y = y_var->template GetMutable<framework::LoDTensor>();
  SetupTensor<bool>(y, input_dims, 0, 1);

  auto output_var = scope.get()->Var("output");
  framework::AttributeMap attrs;

  auto *op = new operators::LogicalAndOp<CPU, float>(
      "logical_and", inputs, outputs, attrs, scope.get());

  op->InferShape();
  op->Init();
  op->Run();

  auto output = output_var->template Get<framework::LoDTensor>();
  framework::Tensor output_cmp;
  bool *output_cmp_data = output_cmp.mutable_data<bool>(output->dims());
  LogicalAnd(x, y, &output_cmp);

  const bool *output_data = output->data<bool>();
  for (int i = 0; i < output->numel(); ++i) {
    if (output_data[i] != output_cmp_data[i]) {
      LOG(kLOG_INFO) << "output_data[" << i << "] = " << output_data[i]
                     << ", output_cmp_data[" << i
                     << "] = " << output_cmp_data[i];
      delete op;
      exit(1);
    }
  }
}
}  // namespace paddle_mobile_lens

int main() {
  paddle_mobile_lens::TestLogicalAndOp({1, 1, 2, 3});
  paddle_mobile_lens::TestLogicalAndOp({1, 3, 11, 12});
  paddle_mobile_lens::TestLogicalAndOp({1, 16, 32, 32});
  DLOG << "test logical_and op pass.";
  return 0;
}
