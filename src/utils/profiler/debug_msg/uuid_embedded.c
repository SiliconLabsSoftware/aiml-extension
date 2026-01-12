/***************************************************************************//**
 * @file uuid_embedded.c
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
#include "uuid_embedded.h"
#if (SL_ML_ENABLE_PROFILER_DEBUG_MSG)
/* --------------------------------------------------------------------------
   Tiny xorshift64* PRNG (freestanding, 8 bytes of state)
   - You MUST seed it with non-zero state before use (uuid_seed()).
---------------------------------------------------------------------------*/
static uint64_t uuid_prng_state = 0u;
//static int count = 1;
uint64_t xorshift64star(uint64_t *s)
{
    uint64_t x = *s;
    /* Avoid zero state */
    if (x == 0u) { x = 0x9E3779B97F4A7C15ULL ^ ((uint64_t)platform_entropy32() << 32); }
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* Seed the PRNG with any non-zero 64-bit value. Call once at boot. */
void uuid_seed(uint64_t seed)
{
    /* Mix in platform entropy to reduce predictability if available */
    uint64_t mix = ((uint64_t)platform_entropy32() << 32) ^ (uint64_t)platform_entropy32();
    uuid_prng_state = (seed ^ mix);
    if (uuid_prng_state == 0u) { uuid_prng_state = 0xD1B54A32D192ED03ULL; }
    /* Stir the state a few times */
    (void)xorshift64star(&uuid_prng_state);
    (void)xorshift64star(&uuid_prng_state);
    (void)xorshift64star(&uuid_prng_state);
}

/* Fill buffer with random bytes from the PRNG */
void uuid_randbytes(uint8_t *dst, size_t n)
{
    while (n) {
        uint64_t r = xorshift64star(&uuid_prng_state); //count * 1111111111111111111ULL;
        for (unsigned i = 0; i < 8 && n; ++i, --n) {
            *dst++ = (uint8_t)(r & 0xFFu);
            r >>= 8;
        }
    }
    //count++;
}

/* Generate RFC 4122 UUID version 4 (random) in binary (16 bytes). */
void uuid_v4_generate(uint8_t out16[16])
{
    uuid_randbytes(out16, 16);

    /* Set version: high nibble of byte 6 = 0b0100 (version 4) */
    out16[6] = (uint8_t)((out16[6] & 0x0Fu) | 0x40u);

    /* Set variant: two MSBs of byte 8 = 0b10 (RFC 4122) */
    out16[8] = (uint8_t)((out16[8] & 0x3Fu) | 0x80u);
}

/* Convert 16-byte UUID to canonical string "8-4-4-4-12" (36 chars + NUL). */
void uuid_to_string(const uint8_t u[16], char out37[37])
{
    static const char hex[16] = "0123456789abcdef";
    int pos = 0;
    for (int i = 0; i < 16; ++i) {
        /* Insert hyphens after byte indices: 3,5,7,9 (0-based) */
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            out37[pos++] = '-';
        }
        out37[pos++] = hex[(u[i] >> 4) & 0x0F];
        out37[pos++] = hex[u[i] & 0x0F];
    }
    out37[pos] = '\0';
}
#endif //(SL_ML_ENABLE_PROFILER_DEBUG_MSG)