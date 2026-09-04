#include "asset_zlib.h"

#include <string.h>
#include "esp_log.h"
#include "miniz.h"

static const char *TAG = "asset_zlib";

bool asset_zlib_inflate_rgb565(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_len)
{
    if (!src || !dst || src_len == 0 || dst_len == 0) {
        return false;
    }

    tinfl_decompressor decomp;
    tinfl_init(&decomp);

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (1) {
        size_t in_bytes = src_len - in_pos;
        size_t out_bytes = dst_len - out_pos;
        tinfl_status st = tinfl_decompress(&decomp, src + in_pos, &in_bytes, dst, dst + out_pos,
                                         &out_bytes, TINFL_FLAG_PARSE_ZLIB_HEADER);
        in_pos += in_bytes;
        out_pos += out_bytes;
        if (st == TINFL_STATUS_DONE) {
            return out_pos == dst_len;
        }
        if (st < TINFL_STATUS_DONE) {
            ESP_LOGE(TAG, "inflate failed st=%d out=%u/%u", (int)st, (unsigned)out_pos,
                     (unsigned)dst_len);
            return false;
        }
    }
}
