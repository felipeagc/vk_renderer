#pragma once

#include <stdint.h>
#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EgEngine EgEngine;
typedef struct RgImage RgImage;
typedef struct RgCmdPool RgCmdPool;

EgImage egGenerateBRDFLUT(EgEngine *engine, RgCmdPool *cmd_pool, uint32_t dim);

#ifdef __cplusplus
}
#endif
