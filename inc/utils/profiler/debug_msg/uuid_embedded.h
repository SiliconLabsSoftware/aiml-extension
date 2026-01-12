/***************************************************************************//**
 * @file uuid_embedded.h
 * @brief minimal, freestanding UUIDv4 for embedded systems.
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
#ifndef UUID_EMBEDDED_H
#define UUID_EMBEDDED_H

#include "sl_ml_profiler_debug_channel.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
/* C-compatible headers only */
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
   Platform entropy hook (optional).
   Provide your own definition in another TU; this weak default returns 0.
   Good sources: TRNG, unique chip ID, RTC counter, ADC noise, etc.
---------------------------------------------------------------------------*/
__attribute__((weak)) uint32_t platform_entropy32(void) { return 0u; }

uint64_t xorshift64star(uint64_t *s);

/* Seed the PRNG with any non-zero 64-bit value. Call once at boot. */
void uuid_seed(uint64_t seed);

/* Fill buffer with random bytes from the PRNG */
void uuid_randbytes(uint8_t *dst, size_t n);

/* Generate RFC 4122 UUID version 4 (random) in binary (16 bytes). */
void uuid_v4_generate(uint8_t out16[16]);

/* Convert 16-byte UUID to canonical string "8-4-4-4-12" (36 chars + NUL). */
void uuid_to_string(const uint8_t u[16], char out37[37]);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)
#endif /* UUID_EMBEDDED_H */