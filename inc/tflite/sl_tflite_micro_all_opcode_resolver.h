// 
/***************************************************************************//**
 * @file sl_tflite_micro_all_opcode_resolver.h
 * @brief Macro to instantiate and initialize opcode resolver with 
 * all ops available in micro_mutable_op_resolver.h
 *******************************************************************************
 * # License
 * <b>Copyright 2025 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#ifndef SL_TFLITE_MICRO_ALL_OPCODE_RESOLVER_H
#define SL_TFLITE_MICRO_ALL_OPCODE_RESOLVER_H

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

#define SL_TFLITE_MICRO_OPCODE_RESOLVER(opcode_resolver) \
static tflite::MicroMutableOpResolver<98> opcode_resolver; \
opcode_resolver.AddAbs(); \
opcode_resolver.AddAdd(); \
opcode_resolver.AddAddN(); \
opcode_resolver.AddArgMax(); \
opcode_resolver.AddArgMin(); \
opcode_resolver.AddAssignVariable(); \
opcode_resolver.AddAveragePool2D(); \
opcode_resolver.AddBatchToSpaceNd(); \
opcode_resolver.AddBroadcastArgs(); \
opcode_resolver.AddBroadcastTo(); \
opcode_resolver.AddCallOnce(); \
opcode_resolver.AddCast(); \
opcode_resolver.AddCeil(); \
opcode_resolver.AddCircularBuffer(); \
opcode_resolver.AddConcatenation(); \
opcode_resolver.AddConv2D(); \
opcode_resolver.AddCos(); \
opcode_resolver.AddCumSum(); \
opcode_resolver.AddDepthToSpace(); \
opcode_resolver.AddDepthwiseConv2D(); \
opcode_resolver.AddDequantize(); \
opcode_resolver.AddDetectionPostprocess(); \
opcode_resolver.AddDiv(); \
opcode_resolver.AddElu(); \
opcode_resolver.AddEqual(); \
opcode_resolver.AddEthosU(); \
opcode_resolver.AddExp(); \
opcode_resolver.AddExpandDims(); \
opcode_resolver.AddFill(); \
opcode_resolver.AddFloor(); \
opcode_resolver.AddFloorDiv(); \
opcode_resolver.AddFloorMod(); \
opcode_resolver.AddFullyConnected(); \
opcode_resolver.AddGather(); \
opcode_resolver.AddGatherNd(); \
opcode_resolver.AddGreater(); \
opcode_resolver.AddGreaterEqual(); \
opcode_resolver.AddHardSwish(); \
opcode_resolver.AddIf(); \
opcode_resolver.AddL2Normalization(); \
opcode_resolver.AddL2Pool2D(); \
opcode_resolver.AddLeakyRelu(); \
opcode_resolver.AddLess(); \
opcode_resolver.AddLessEqual(); \
opcode_resolver.AddLog(); \
opcode_resolver.AddLogicalAnd(); \
opcode_resolver.AddLogicalNot(); \
opcode_resolver.AddLogicalOr(); \
opcode_resolver.AddLogistic(); \
opcode_resolver.AddLogSoftmax(); \
opcode_resolver.AddMaximum(); \
opcode_resolver.AddMaxPool2D(); \
opcode_resolver.AddMirrorPad(); \
opcode_resolver.AddMean(); \
opcode_resolver.AddMinimum(); \
opcode_resolver.AddMul(); \
opcode_resolver.AddNeg(); \
opcode_resolver.AddNotEqual(); \
opcode_resolver.AddPack(); \
opcode_resolver.AddPad(); \
opcode_resolver.AddPadV2(); \
opcode_resolver.AddPrelu(); \
opcode_resolver.AddQuantize(); \
opcode_resolver.AddReadVariable(); \
opcode_resolver.AddReduceMax(); \
opcode_resolver.AddRelu(); \
opcode_resolver.AddRelu6(); \
opcode_resolver.AddReshape(); \
opcode_resolver.AddResizeBilinear(); \
opcode_resolver.AddResizeNearestNeighbor(); \
opcode_resolver.AddRound(); \
opcode_resolver.AddRsqrt(); \
opcode_resolver.AddSelectV2(); \
opcode_resolver.AddShape(); \
opcode_resolver.AddSin(); \
opcode_resolver.AddSlice(); \
opcode_resolver.AddSoftmax(); \
opcode_resolver.AddSpaceToBatchNd(); \
opcode_resolver.AddSpaceToDepth(); \
opcode_resolver.AddSplit(); \
opcode_resolver.AddSplitV(); \
opcode_resolver.AddSqueeze(); \
opcode_resolver.AddSqrt(); \
opcode_resolver.AddSquare(); \
opcode_resolver.AddSquaredDifference(); \
opcode_resolver.AddStridedSlice(); \
opcode_resolver.AddSub(); \
opcode_resolver.AddSum(); \
opcode_resolver.AddSvdf(); \
opcode_resolver.AddTanh(); \
opcode_resolver.AddTransposeConv(); \
opcode_resolver.AddTranspose(); \
opcode_resolver.AddUnpack(); \
opcode_resolver.AddUnidirectionalSequenceLSTM(); \
opcode_resolver.AddVarHandle(); \
opcode_resolver.AddWhile(); \
opcode_resolver.AddZerosLike();
//opcode_resolver.AddWindow();


#endif // SL_TFLITE_MICRO_ALL_OPCODE_RESOLVER_H
