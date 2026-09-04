#pragma once

#include <stddef.h>
#include <stdint.h>

/** Inflate zlib RGB565 blob into caller buffer (must hold raw_len bytes). */
bool asset_zlib_inflate_rgb565(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_len);
