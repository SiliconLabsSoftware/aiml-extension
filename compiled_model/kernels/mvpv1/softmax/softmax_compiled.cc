/*******************************************************************************
 * # License
 * <b>Copyright 2026 Silicon Laboratories Inc. www.silabs.com</b>
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



#include "tflite_micro_model_config.h"
#include "tensorflow/lite/micro/kernels/kernel_util.h"
#include "mvpv1_kernel_utils.hpp"
#include "compiled_registration_helper.hpp"

// NOTE: The MVPv1 does not support the Softmax kernel so we directly call the CMSIS-NN implementation

namespace tflite {

extern int          cmsis_nn_softmax_get_init_size();
extern TfLiteStatus cmsis_nn_softmax_prepare(TfLiteContext* context, TfLiteNode* node);
extern TfLiteStatus cmsis_nn_softmax_invoke(TfLiteContext* context, TfLiteNode* node);


namespace
{
using namespace npu_toolkit;

void* init(TfLiteContext* context, const char* buffer, size_t length)
{
    return context->AllocatePersistentBuffer(context, cmsis_nn_softmax_get_init_size());
}

}  // namespace


TFLMRegistration Register_SOFTMAX() {
    return tflite::micro::RegisterOp(
        init,
        cmsis_nn_softmax_prepare,
        cmsis_nn_softmax_invoke
    );
}

}  // namespace tflite