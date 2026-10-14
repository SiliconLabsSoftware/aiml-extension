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
#pragma once

#include <cstdint>
#include <cstring>
#include "tensorflow/lite/c/common.h"


namespace npu_toolkit
{


/**
 * @addtogroup tflite_micro_model_types
 * @{
 */


 /**
  * Helper class to access the dimensions of a tensor
  */
struct TfliteTensorShape
{
    /**
     * The maximum number of dimensions this tensor object can hold
     * Note that 5 is the number that TF uses as well.
     */
    static constexpr const unsigned MAX_DIMENSIONS = 5;

    /**
     * The dimensions
     */
    uint32_t dims[MAX_DIMENSIONS] = { 0 };
    /**
     * The number of dimensions
     */
    uint8_t length = 0;

    /**
     * Default constructor
     */
    TfliteTensorShape() = default;
    /**
     * Construct from TfLiteIntArray
     */
    TfliteTensorShape(const TfLiteIntArray* dims);
    /**
     * Initialize from TfLiteIntArray
     */
    void init(const TfLiteIntArray* dims);

    /**
     * The total number of elements in the tensor
     */
    uint32_t flat_size() const;
    /**
     * Overloaded operator to access individual dimension
     */
    uint32_t operator [](int i) const;
    /**
     * Convert shape to human-readable string
     */
    char* to_str(char* str_buffer) const;
};


/**
 * Helper class to access a model tensor
 */
struct TfliteTensorView : public TfLiteTensor
{
    /**
     * The number of bytes required by the individual elements of the tensor,
     * e.g. int8 -> 1 byte
     */
    unsigned element_size() const;
    /**
     * Convert tensor to human-readable string
     */
    const char* to_str(char* str_buffer = nullptr) const;
    /**
     * Return a @ref TfliteTensorShape object for this tensor
     */
    TfliteTensorShape shape() const;

    /**
     * Return the dequantized value for the quantized tensor at the given index
     */
    template <typename qtype>
    float dequantized_value(int index) const
    {
        const qtype quant_val = static_cast<const qtype*>(data.raw_const)[index];
        return (((float)quant_val) - params.zero_point) * params.scale;
    }

    /**
     * Create a @ref TfliteTensorView instance from a @ref TfLiteEvalTensor
     */
    static TfliteTensorView from_eval_tensor(const TfLiteEvalTensor& eval_tensor)
    {
        TfliteTensorView view;
        memset((void*)&view, 0, sizeof(view));
        view.data = eval_tensor.data;
        view.dims = eval_tensor.dims;
        view.type = eval_tensor.type;
        view.bytes = view.shape().flat_size() * view.element_size();
        return view;
    }

};

/**
 * Helper function to dequantize a value
 */
template <typename qtype>
float dequantized_value(const TfLiteQuantizationParams& params, qtype value)
{
    return (((float)value) - params.zero_point) * params.scale;
}

/**
 * Helper function to convert @ref TfLiteType to a string
 */
const char* to_str(TfLiteType dtype);

/**
 * @}
 */


} // namespace npu_toolkit