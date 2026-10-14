/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#ifndef TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_
#define TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_

#include "tensorflow/lite/micro/compatibility.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"

namespace tflite {

/**
 * @addtogroup tflite_micro
 * @defgroup tflite_micro_all_ops_resolver All Ops Resolver
 * @{
 */

// The magic number in the template parameter is the maximum number of ops that
// can be added to AllOpsResolver. It can be increased if needed. And most
// applications that care about the memory footprint will want to directly use
// MicroMutableOpResolver and have an application specific template parameter.
// The examples directory has sample code for this.

/**
 * Registers all supported kernels
 *
 * @note This links all supported kernels into the application
 *       which will likely waste a lot of flash as most of the
 *       kernels are not actually used by the given .tflite model.
 */
class AllOpsResolver : public MicroMutableOpResolver<128> {
 public:
  AllOpsResolver()
  {
    // Please keep this list of Builtin Operators in alphabetical order.
    AddAbs();
    AddAdd();
    AddAddN();
    AddArgMax();
    AddArgMin();
    AddAssignVariable();
    AddAveragePool2D();
    AddBatchMatMul();
    AddBatchToSpaceNd();
    AddBroadcastArgs();
    AddBroadcastTo();
    AddCallOnce();
    AddCast();
    AddCeil();
    //AddCircularBuffer();
    AddConcatenation();
    AddConv2D();
    AddCos();
    AddCumSum();
    AddDepthToSpace();
    AddDepthwiseConv2D();
    AddDequantize();
    //AddDetectionPostprocess();
    AddDiv();
    AddDynamicUpdateSlice();
    AddElu();
    AddEmbeddingLookup();
    AddEqual();
    //AddEthosU();
    AddExp();
    AddExpandDims();
    AddFill();
    AddFloor();
    AddFloorDiv();
    AddFloorMod();
    AddFullyConnected();
    AddGather();
    AddGatherNd();
    AddGreater();
    AddGreaterEqual();
    AddHardSwish();
    AddIf();
    AddL2Normalization();
    AddL2Pool2D();
    AddLeakyRelu();
    AddLess();
    AddLessEqual();
    AddLog();
    AddLogicalAnd();
    AddLogicalNot();
    AddLogicalOr();
    AddLogistic();
    AddLogSoftmax();
    AddMaxPool2D();
    AddMaximum();
    AddMean();
    AddMinimum();
    AddMirrorPad();
    AddMul();
    AddNeg();
    AddNotEqual();
    AddPack();
    AddPad();
    AddPadV2();
    AddPrelu();
    AddQuantize();
    AddReadVariable();
    AddReduceMax();
    AddReduceMin();
    AddRelu();
    AddRelu6();
    AddReshape();
    AddResizeBilinear();
    AddResizeNearestNeighbor();
    AddReverseV2();
    AddRound();
    AddRsqrt();
    AddSelectV2();
    AddShape();
    AddSin();
    AddSlice();
    AddSoftmax();
    AddSpaceToBatchNd();
    AddSpaceToDepth();
    AddSplit();
    AddSplitV();
    AddSqrt();
    AddSquare();
    AddSquaredDifference();
    AddSqueeze();
    AddStridedSlice();
    AddSub();
    AddSum();
    AddSvdf();
    AddTanh();
    AddTranspose();
    AddTransposeConv();
    AddUnidirectionalSequenceLSTM();
    AddUnpack();
    AddVarHandle();
    AddWhile();
    AddZerosLike();
  }

 private:
  TF_LITE_REMOVE_VIRTUAL_DELETE
};

/**
 * @}
 */

}  // namespace tflite

#endif  // TENSORFLOW_LITE_MICRO_ALL_OPS_RESOLVER_H_
