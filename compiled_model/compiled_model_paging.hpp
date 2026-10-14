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
#include <cassert>

#include "tensorflow/lite/c/common.h"
#include "compiled_model_paging_interface.hpp"
#include "compiled_model_context.hpp"


namespace npu_toolkit
{

struct DmaContext;

/**
 * @addtogroup compiled_model
 * @defgroup compiled_model_paging Compiled Model Paging
 * @{
 * This manages the paging of an ML model's weights & bias tensors during
 * inference. The page-able data includes:
 * - Weights/filters tensor
 * - Bias tensor
*/
class CompiledModelPaging
{

public:

    /**
     * Create a CompiledModelPaging object in a
     * "persistent" buffer of the tensor arena.
     */
    static CompiledModelPaging* create(
        TfLiteContext* context,
        const PagingConfig& paging_config
    );

    /**
     * nitialize this CompiledModelPaging context for the current model.
     * This is invoke during CompiledModelContext::init()
    */
    bool init(
        TfLiteContext *context,
        const void* compiled_data
    );

    /**
     * Initialize this CompiledModelPaging context for the current model.
     * This is invoke during CompiledModelContext::load()
     */
    bool load(TfLiteContext *context, const void* compiled_data);

    /**
     * De-initialize this CompiledModelPaging context for the current model.
     * This is invoke during CompiledModelContext::deinit()
     */
    void deinit();


    /**
     * Prepare the current layer for paging.
     * his waits for any tensor buffers to be paged
     * into the paging RAM buffer.
    */
    bool begin_layer(TfLiteContext *context, uint16_t* n_programs);

    /**
     * Return the compiled accelerator registers
     * for the next program that should be executed
     * on the accelerator(s).
     */
    bool get_next_program(CompiledProgramInfo *info);

    /**
     * Wait for any buffers required by the current program to be paged
     * into the RAM buffer.
    */
    bool wait();

    /**
     *  Release any paged buffers that are no longer needed.
    */
    void release();

    /**
     * Return the base address for the given memory region
     */
    inline uintptr_t get_base_address(uint8_t memory_region) const
    {
        assert(_dma_context != nullptr);
        return (uintptr_t)((memory_region == CompressedProgramConfig::DEFAULT_MEMORY_REGION_ID) ?
            _dma_context->paging_buffer :
            _dma_context->memory_region_addrs[memory_region]);
    }

private:
    /**
     * Point to the current compiled item.
     * At the beginning of each layer this is a
     * PagingCompiledLayerConfig object. Then,
     * for each accelerator program, this is a PagingCompressedProgramConfig.
     *
     * This points to the "layer_config" portion of the paging buffer.
    */

    const PagingCompiledLinkedItem* _current_item;
    /**
     * Pointer to hardware-specific DMA context.
     */
    DmaContext* _dma_context;

    /**
     *  Flag that indicates if the current model layer uses paging.
     */
    bool _layer_paging_enabled;

    /**
     *  Class constructor, called by CompiledModelPaging::create()
     */
    CompiledModelPaging(void* dma_context);

};

/**
@}
*/


} // namespace npu_toolkit