#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Engine Engine;
typedef struct RgImage RgImage;
typedef struct RgCmdPool RgCmdPool;

RgImage *GenerateBRDFLUT(Engine *engine, RgCmdPool *cmd_pool, uint32_t dim);

#ifdef __cplusplus
}
#endif
