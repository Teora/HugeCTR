/*
 * Copyright (c) 2019, NVIDIA CORPORATION.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "HugeCTR/include/layers/elu_layer.hpp"

#include "HugeCTR/include/data_parser.hpp"
#include "HugeCTR/include/general_buffer.hpp"
#include "gtest/gtest.h"
#include "utest/test_utils.h"

#include <math.h>
#include <vector>

using namespace std;
using namespace HugeCTR;

namespace {

const float eps = 1e-6;

void elu_cpu(const float* in, float* out, int len, float alpha) {
  for (int i = 0; i < len; ++i) {
    out[i] = (in[i] < 0) ? alpha * (exp(in[i]) - 1) : in[i];
  }
}

void elu_bprop_cpu(const float* d_out, float* d_in, int len, float alpha) {
  for (int i = 0; i < len; ++i) {
    d_in[i] = (d_in[i] < 0) ? alpha * exp(d_in[i]) * d_out[i] : d_out[i];
  }
}

void elu_test(int dim0, int dim1, float alpha) {
  GeneralBuffer<float> buf;
  vector<int> dims = {dim0, dim1};
  Tensor<float> in_tensor(dims, buf);
  Tensor<float> out_tensor(dims, buf);
  buf.init(0);

  const int len = dim0 * dim1;
  float* d_in = in_tensor.get_ptr();
  float* d_out = out_tensor.get_ptr();
  float* h_in = (float*)malloc(len * sizeof(float));
  float* h_out = (float*)malloc(len * sizeof(float));
  float* h_expected = (float*)malloc(len * sizeof(float));

  GaussianDataSimulator<float> simulator(0.0, 1.0, -2.0, 2.0);
  EluLayer elu_layer(in_tensor, out_tensor, alpha, 0);

  // fprop
  for (int i = 0; i < len; ++i) {
    h_in[i] = simulator.get_num();
  }
  cudaMemcpy(d_in, h_in, len * sizeof(float), cudaMemcpyHostToDevice);
  elu_layer.fprop(cudaStreamDefault);
  cudaMemcpy(h_out, d_out, len * sizeof(float), cudaMemcpyDeviceToHost);

  elu_cpu(h_in, h_expected, len, alpha);
  ASSERT_TRUE(test::compare_array_approx<float>(h_out, h_expected, len, eps));

  // bprop
  for (int i = 0; i < len; ++i) {
    h_in[i] = simulator.get_num();
    h_out[i] = simulator.get_num();
    h_expected[i] = h_in[i];
  }
  cudaMemcpy(d_in, h_in, len * sizeof(float), cudaMemcpyHostToDevice);
  cudaMemcpy(d_out, h_out, len * sizeof(float), cudaMemcpyHostToDevice);
  elu_layer.bprop(cudaStreamDefault);
  cudaMemcpy(h_in, d_in, len * sizeof(float), cudaMemcpyDeviceToHost);

  elu_bprop_cpu(h_out, h_expected, len, alpha);
  ASSERT_TRUE(test::compare_array_approx<float>(h_in, h_expected, len, eps));

  free(h_in);
  free(h_out);
  free(h_expected);
}

}  // namespace

TEST(elu_layer, fprop_and_bprop) {
  elu_test(10, 20, 1.0);
  elu_test(10, 500, 1.0);
  elu_test(512, 1024 * 2, 1.0);
}
