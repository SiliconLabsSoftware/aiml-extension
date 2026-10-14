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
#include <type_traits>

/// @cond NO_DOXYGEN
#include "tflite_model_parameters_flatbuffer_generated.h"
/// @endcond


namespace npu_toolkit
{



/**
 * @addtogroup tflite_micro_model
 * @defgroup tflite_model_parameters
 * @{
 */

/**
 * @brief .tflite Model Parameters
 *
 * This provides access to application parameters stored in a .tflite model flatbuffer's metadata.
 * Application parameters are things like audio sampling rate, FFT bins, etc.
 *
 */
class TfliteModelParameters
{
private:
    /// @cond NO_DOXYGEN
    typedef const flatbuffers::Vector<flatbuffers::Offset<TfliteModelParametersSchema::Entry>> EntryVector;
    typedef flatbuffers::Vector<flatbuffers::Offset<TfliteModelParametersSchema::Entry>>::const_iterator EntryVectorIterator;
    typedef flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>> StringVector;
    typedef flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>::const_iterator StringVectorIterator;
    typedef flatbuffers::Vector<int32_t> Int32Vector;
    typedef flatbuffers::Vector<int32_t>::const_iterator Int32VectorIterator;
    typedef flatbuffers::Vector<float> FloatVector;
    typedef flatbuffers::Vector<float>::const_iterator FloatVectorIterator;
    /// @endcond

public:

    /**
     * The .tflite model metadata tag to search for the model parameters
     */
    static constexpr const char METADATA_TAG[] = "SL_PARAMSv1";

    /**
     * String list data type
     */
    struct StringList
    {
        struct Iterator
        {
            StringVectorIterator it;

            Iterator(StringVectorIterator it = StringVectorIterator());

            bool operator!=(Iterator rhs);
            const char* operator*();
            void operator++();
        };


        const StringVector *vector;

        StringList(const StringVector *vector = nullptr);

        uint32_t size() const;
        Iterator begin() const;
        Iterator end() const;
        const char* operator [](unsigned index) const;

        StringList(const StringList&);
        StringList& operator=(const StringList&);

        constexpr bool is_valid() const
        {
            return (vector != nullptr);
        }
    };

    /**
     * Int32 list data type
     */
    struct Int32List
    {
        struct Iterator
        {
            Int32VectorIterator it;

            Iterator(Int32VectorIterator it = Int32VectorIterator());

            bool operator!=(Iterator rhs);
            int32_t operator*();
            void operator++();
        };


        const Int32Vector *vector;

        Int32List(const Int32Vector *vector = nullptr);

        uint32_t size() const;
        Iterator begin() const;
        Iterator end() const;
        int32_t operator [](unsigned index) const;

        Int32List(const Int32List&);
        Int32List& operator=(const Int32List&);

        constexpr bool is_valid() const
        {
            return (vector != nullptr);
        }
    };


    /**
     * Float list data type
     */
    struct FloatList
    {
        struct Iterator
        {
            FloatVectorIterator it;

            Iterator(FloatVectorIterator it = FloatVectorIterator());

            bool operator!=(Iterator rhs);
            float operator*();
            void operator++();
        };


        const FloatVector *vector;

        FloatList(const FloatVector *vector = nullptr);

        uint32_t size() const;
        Iterator begin() const;
        Iterator end() const;
        float operator [](unsigned index) const;

        FloatList(const FloatList&);
        FloatList& operator=(const FloatList&);

        constexpr bool is_valid() const
        {
            return (vector != nullptr);
        }
    };

    /**
     * Generic value data type
     */
    struct Value : private TfliteModelParametersSchema::Entry
    {
        TfliteModelParametersSchema::Value type() const;
        const char* key() const;
        bool boolean() const;
        int8_t i8() const;
        uint8_t u8() const;
        int16_t i16() const;
        uint16_t u16() const;
        int32_t i32() const;
        uint32_t u32() const;
        int64_t i64() const;
        uint64_t u64() const;
        float f32() const;
        double f64() const;
        const char* str() const;
        StringList str_list() const;
        Int32List int32_list() const;
        FloatList float_list() const;
        const uint8_t* bin() const;
        uint32_t bin_length() const;
    };

    /**
     * C++ iterator
     */
    struct Iterator
    {
        EntryVectorIterator it;

        Iterator(EntryVectorIterator it);

        bool operator!=(Iterator rhs);
        const Value* operator*();
        void operator++();
    };

    /**
     * Initialize the given parameters with the
     * parameters in the metadata of the given flatubffer
     */
    static bool load_from_tflite_flatbuffer(
        const void* flatbuffer,
        TfliteModelParameters& parameters,
        const char* tag = nullptr
    );
    /**
     * Load the parameters from the given flatbuffer Dictionary
     */
    bool load(const TfliteModelParametersSchema::Dictionary *fb_dictionary);
    /**
     * Load the parameters from the given flatbuffer
     */
    bool load(const void* flatbuffer);
    /**
     * Load the parameters from the given flatbuffer
     */
    bool load_from_tflite_flatbuffer(const void *flatbuffer, const char* tag = nullptr);
    /**
     * Unload the parameters
     */
    void unload();

    /**
     * Return true if a parameters has the given key
     */
    bool contains(const char* key) const;
    /**
     * Return the value of the parameters with the given key
     */
    const Value* operator [](const char* key) const;

    /**
     * Print a human-readable summary of the parameters
     * with the given logger.
     */
    void print_summary() const;

    /**
     * C++ iterator begin()
     */
    Iterator begin() const;
    /**
     * C++ iterator begin()
     */
    Iterator end() const;

    /**
     * Return the numbers of available parameters
     */
    constexpr unsigned size() const
    {
        return (_fb_dictionary == nullptr) ? 0 : _fb_dictionary->entries()->size();
    }

    /**
     * Return true if the parameters are loaded
     */
    constexpr bool is_loaded() const
    {
        return _fb_dictionary != nullptr;
    }

    /**
     * Return a reference to the underlying flatbuffer dictionary
     */
    constexpr const TfliteModelParametersSchema::Dictionary* fb_dictionary() const
    {
        return _fb_dictionary;
    }

    /**
     * Return the generic value of the parameter with the given key
     */
    const Value* get(const char* key) const;
    /**
     * Obtain the string value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, const char* &value) const;
    /**
     * Obtain the uint8 value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, const uint8_t* &value) const;
    /**
     * Obtain the string list value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, StringList &value) const;
    /**
     * Obtain the int32 list value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, Int32List &value) const;
    /**
     * Obtain the float list value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, FloatList &value) const;
    /**
     * Obtain the binary value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    bool get(const char* key, const uint8_t* &data, uint32_t &length) const;
    /**
     * Obtain the templated value of the parameter with the given key
     *
     * @return true if the value was found, false else
     */
    template<typename T>
    bool get(const char* key, T &value) const
    {
        auto entry = get(key);
        if(entry != nullptr)
        {
            const auto entry_type = entry->type();
            #define _MODEL_PARAMS_ADD_GETTER(_type) \
            if(entry_type == TfliteModelParametersSchema::Value::_type) \
            { \
                value = static_cast<T>(entry->_type()); \
                return true; \
            }
            _MODEL_PARAMS_ADD_GETTER(boolean)
            else _MODEL_PARAMS_ADD_GETTER(i8)
            else _MODEL_PARAMS_ADD_GETTER(u8)
            else _MODEL_PARAMS_ADD_GETTER(i16)
            else _MODEL_PARAMS_ADD_GETTER(u16)
            else _MODEL_PARAMS_ADD_GETTER(i32)
            else _MODEL_PARAMS_ADD_GETTER(u32)
            else _MODEL_PARAMS_ADD_GETTER(i64)
            else _MODEL_PARAMS_ADD_GETTER(u64)
            else _MODEL_PARAMS_ADD_GETTER(f32)
            else _MODEL_PARAMS_ADD_GETTER(f64)
            #undef _MODEL_PARAMS_ADD_GETTER
        }
        return false;
    }

    /**
     * Default constructor
     */
    TfliteModelParameters() = default;
    TfliteModelParameters(const TfliteModelParameters&);
    TfliteModelParameters& operator=(const TfliteModelParameters&);

private:
    const TfliteModelParametersSchema::Dictionary *_fb_dictionary = nullptr;
};

/// @cond NO_DOXYGEN
using StringList = TfliteModelParameters::StringList;
using Int32List = TfliteModelParameters::Int32List;
using FloatList = TfliteModelParameters::FloatList;
/// @endcond

/**
 * @}
 */

} // namespace npu_toolkit