#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include "rg.h"

#ifdef _MSC_VER
    #pragma warning(disable:4996)
#endif

#ifdef _WIN32
    #define VK_USE_PLATFORM_WIN32_KHR
#else
    #define VK_USE_PLATFORM_XLIB_KHR
#endif

#include "volk.h"

enum
{
    RG_ARRAY_INITIAL_CAPACITY = 16,
    RG_MAX_DESCRIPTOR_SET_BINDINGS = 32,
};

#define RG_ALIGN(n, to) (((n) % (to)) ? ((n) + ((to) - ((n) % (to)))) : (n))
#define RG_STATIC_ARRAY_SIZE(arr) (sizeof(arr)/sizeof(arr[0]))
#define RG_MAX(a, b) (((a) > (b)) ? (a) : (b))
#define RG_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define RG_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

#define VK_CHECK(res)                           \
    do {                                        \
        const VkResult vk__result = res;        \
        if (vk__result < 0)                     \
        {                                       \
            fprintf(                            \
                stderr,                         \
                "[%s:%u] Vulkan error: %d\n",   \
                __FILE__,                       \
                __LINE__,                       \
                vk__result);                    \
            exit(1);                            \
        }                                       \
    } while(0)

static void *rgArrayGrow(void *ptr, size_t *cap, size_t wanted_cap, size_t item_size)
{
    if (!ptr)
    {
        size_t desired_cap = ((wanted_cap == 0) ? RG_ARRAY_INITIAL_CAPACITY : wanted_cap);
        *cap = desired_cap;
        return malloc(item_size * desired_cap);
    }

    size_t desired_cap = ((wanted_cap == 0) ? ((*cap) * 2) : wanted_cap);
    *cap = desired_cap;
    return realloc(ptr, (desired_cap * item_size));
}

#define ARRAY_OF(type)  \
    struct              \
    {                   \
        type *ptr;      \
        size_t len;     \
        size_t cap;     \
    }

#define arrFull(a) ((a)->ptr ? ((a)->len >= (a)->cap) : 1)

#define arrAlloc(a, count) \
    ((a)->ptr = rgArrayGrow((a)->ptr, &(a)->cap, (count), sizeof(*((a)->ptr))), \
     (a)->len = (count))

#define arrPush(a, item)                                              \
    (arrFull(a) ? (a)->ptr =                                          \
        rgArrayGrow((a)->ptr, &(a)->cap, 0, sizeof(*((a)->ptr))) : 0, \
     (a)->ptr[(a)->len++] = (item))

#define arrPop(a) ((a)->len > 0 ? ((a)->len--, &(a)->ptr[(a)->len]) : NULL)

#define arrFree(a)                     \
    do                                 \
    {                                  \
        if ((a)->ptr) free((a)->ptr);  \
        (a)->ptr = NULL;               \
        (a)->len = 0;                  \
        (a)->cap = 0;                  \
    } while (0)

// Types {{{
typedef struct RgHashmap
{
    uint64_t size;
    uint64_t *hashes;
    uint64_t *values;
} RgHashmap;

typedef enum RgAllocationType
{
    RG_ALLOCATION_TYPE_UNKNOWN,
    RG_ALLOCATION_TYPE_GPU_ONLY,
    RG_ALLOCATION_TYPE_CPU_TO_GPU,
    RG_ALLOCATION_TYPE_GPU_TO_CPU,
} RgAllocationType;

typedef struct RgMemoryChunk
{
    size_t used;
    bool split;
} RgMemoryChunk;

typedef struct RgMemoryBlock
{
    VkDeviceMemory handle;
    size_t size;
    uint32_t memory_type_index;
    RgAllocationType type;

    RgMemoryChunk *chunks;
    uint32_t chunk_count;
    void *mapping;
} RgMemoryBlock;

typedef struct RgAllocator
{
    RgDevice *device;
    ARRAY_OF(RgMemoryBlock*) blocks;
} RgAllocator;

typedef struct RgAllocationInfo
{
    RgAllocationType type;
    VkMemoryRequirements requirements;
    bool dedicated;
} RgAllocationInfo;

typedef struct RgAllocation
{
    size_t size;
    size_t offset;
    bool dedicated;
    union
    {
        struct
        {
            RgMemoryBlock *block;
            size_t chunk_index;
        };
        struct
        {
            VkDeviceMemory dedicated_memory;
            void *dedicated_mapping;
        };
    };
} RgAllocation;

struct RgDevice
{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_callback;
    bool enable_debug_markers;

    VkPhysicalDevice physical_device;
    VkDevice device;

    VkPhysicalDeviceProperties physical_device_properties;
    VkPhysicalDeviceFeatures physical_device_features;
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;

    VkPhysicalDeviceFeatures enabled_features;

    VkExtensionProperties *extension_properties;
    uint32_t num_extension_properties;

    VkQueueFamilyProperties *queue_family_properties;
    uint32_t num_queue_family_properties;

    uint32_t graphics_queue_family_index;
    uint32_t compute_queue_family_index;
    uint32_t transfer_queue_family_index;

    VkQueue graphics_queue;
    VkQueue compute_queue;
    VkQueue transfer_queue;

    RgAllocator *allocator;
};

struct RgSwapchain
{
    RgDevice *device;

    void *window_handle;
    void *display_handle;
    bool vsync;
    RgFormat depth_format; // RG_FORMAT_UNDEFINED for no depth buffer

    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    uint32_t queue_family_index;

    VkExtent2D extent;
	VkFormat color_format;
	VkColorSpaceKHR color_space;

    RgImage *depth_image;

    uint32_t image_count;
    VkImage *images;
    VkImageView *image_views;
    VkSemaphore *present_complete_semaphores;

    uint32_t current_image_index;
    uint32_t current_semaphore_index;

    ARRAY_OF(VkSemaphore) wait_semaphores;
    ARRAY_OF(VkFence) wait_fences;

    RgRenderPass *render_pass;
};

struct RgCmdPool
{
    RgQueueType queue_type;
    VkCommandPool cmd_pool;
};

struct RgCmdBuffer
{
    RgDevice *device;
    VkQueue queue;

    VkCommandBuffer cmd_buffer;
    VkSemaphore semaphore;
    VkFence fence;

    ARRAY_OF(VkSemaphore) wait_semaphores;
    ARRAY_OF(VkPipelineStageFlags) wait_stages;

    RgRenderPass *current_render_pass;
    RgPipeline *current_pipeline;
    VkPipelineBindPoint current_bind_point;
};

struct RgRenderPass
{
    VkRenderPass render_pass;
    uint64_t hash;
    uint32_t color_attachment_count;

    uint32_t width;
    uint32_t height;

    uint32_t current_framebuffer;
    uint32_t framebuffer_count;
    VkFramebuffer *framebuffers;
};

struct RgBuffer
{
    RgBufferInfo info;
    VkBuffer buffer;
    RgAllocation allocation;
};

struct RgImage
{
    RgImageInfo info;
    VkImage image;
    RgAllocation allocation;
    VkImageView view;
};

typedef union RgDescriptor
{
    VkDescriptorImageInfo image;
    VkDescriptorBufferInfo buffer;
} RgDescriptor;

typedef struct RgDescriptorSetPool RgDescriptorSetPool;

struct RgDescriptorSet
{
    VkDescriptorSet set;
    RgDescriptorSetPool *pool;
};

struct RgDescriptorSetPool
{
    RgDescriptorSetLayout *set_layout;

    VkDescriptorPool pool;
    RgDescriptorSet *sets;
    uint32_t set_count;
    ARRAY_OF(RgDescriptorSet *) free_list;
};

struct RgDescriptorSetLayout
{
    RgDevice *device;
    VkDescriptorSetLayout set_layout;
    VkDescriptorSetLayoutBinding *bindings;
    uint32_t binding_count;

    ARRAY_OF(RgDescriptorSetPool *) pools;
};

typedef enum RgPipelineType
{
    RG_PIPELINE_TYPE_GRAPHICS,
    RG_PIPELINE_TYPE_COMPUTE,
} RgPipelineType;

struct RgPipeline
{
    RgDevice *device;
    RgPipelineType type;

    VkPipelineLayout pipeline_layout;

    union
    {
        struct
        {
            RgHashmap instances;

            uint32_t vertex_stride;
            uint32_t num_vertex_attributes;
            RgVertexAttribute *vertex_attributes;

            RgPolygonMode       polygon_mode;
            RgCullMode          cull_mode;
            RgFrontFace         front_face;
            RgPrimitiveTopology topology;

            RgPipelineBlendState        blend;
            RgPipelineDepthStencilState depth_stencil;

            char* vertex_entry;
            char* fragment_entry;

            VkShaderModule vertex_shader;
            VkShaderModule fragment_shader;
        } graphics;

        struct
        {
            VkPipeline instance;
            VkShaderModule shader;
        } compute;
    };
};
// }}}

// Hashing {{{
static void fnvHashReset(uint64_t *hash)
{
    *hash = 14695981039346656037ULL;
}

static void fnvHashUpdate(uint64_t *hash, uint8_t *bytes, size_t count)
{
    for (uint64_t i = 0; i < count; ++i)
    {
        *hash = ((*hash) * 1099511628211) ^ bytes[i];
    }
}

static uint64_t rgHashRenderPass(VkRenderPassCreateInfo *ci)
{
    uint64_t hash = 0;
    fnvHashReset(&hash);

    fnvHashUpdate(
        &hash,
        (uint8_t *)ci->pAttachments,
        ci->attachmentCount * sizeof(*ci->pAttachments));

    for (uint32_t i = 0; i < ci->subpassCount; i++)
    {
        const VkSubpassDescription *subpass = &ci->pSubpasses[i];
        fnvHashUpdate(
            &hash,
            (uint8_t *)&subpass->pipelineBindPoint,
            sizeof(subpass->pipelineBindPoint));

        fnvHashUpdate(&hash, (uint8_t *)&subpass->flags, sizeof(subpass->flags));

        if (subpass->pColorAttachments)
        {
            fnvHashUpdate(
                &hash,
                (uint8_t *)subpass->pColorAttachments,
                subpass->colorAttachmentCount * sizeof(*subpass->pColorAttachments));
        }

        if (subpass->pResolveAttachments)
        {
            fnvHashUpdate(
                &hash,
                (uint8_t *)subpass->pResolveAttachments,
                subpass->colorAttachmentCount * sizeof(*subpass->pResolveAttachments));
        }

        if (subpass->pDepthStencilAttachment)
        {
            fnvHashUpdate(
                &hash,
                (uint8_t *)subpass->pDepthStencilAttachment,
                sizeof(*subpass->pDepthStencilAttachment));
        }

        if (subpass->pInputAttachments)
        {
            fnvHashUpdate(
                &hash,
                (uint8_t *)subpass->pInputAttachments,
                subpass->inputAttachmentCount * sizeof(*subpass->pInputAttachments));
        }

        if (subpass->pPreserveAttachments)
        {
            fnvHashUpdate(
                &hash,
                (uint8_t *)subpass->pPreserveAttachments,
                subpass->preserveAttachmentCount * sizeof(*subpass->pPreserveAttachments));
        }
    }

    fnvHashUpdate(
        &hash,
        (uint8_t *)ci->pDependencies,
        ci->dependencyCount * sizeof(*ci->pDependencies));

    return hash;
}

// }}}

// Hashmap {{{
static void rgHashmapInit(RgHashmap *hashmap, size_t size);
static void rgHashmapDestroy(RgHashmap *hashmap);
static void rgHashmapGrow(RgHashmap *hashmap);
static void rgHashmapSet(RgHashmap *hashmap, uint64_t hash, uint64_t value);
static uint64_t *rgHashmapGet(RgHashmap *hashmap, uint64_t hash);

static void rgHashmapInit(RgHashmap *hashmap, size_t size)
{
    memset(hashmap, 0, sizeof(*hashmap));

    hashmap->size = size;
    assert(hashmap->size > 0);

    // Round up to nearest power of two
    hashmap->size -= 1;
    hashmap->size |= hashmap->size >> 1;
    hashmap->size |= hashmap->size >> 2;
    hashmap->size |= hashmap->size >> 4;
    hashmap->size |= hashmap->size >> 8;
    hashmap->size |= hashmap->size >> 16;
    hashmap->size |= hashmap->size >> 32;
    hashmap->size += 1;

    // Init memory
    hashmap->hashes = (uint64_t *)malloc(hashmap->size * sizeof(uint64_t));
    memset(hashmap->hashes, 0, hashmap->size * sizeof(uint64_t));
    hashmap->values = (uint64_t *)malloc(hashmap->size * sizeof(uint64_t));
    memset(hashmap->values, 0, hashmap->size * sizeof(uint64_t));
}

static void rgHashmapDestroy(RgHashmap *hashmap)
{
    free(hashmap->values);
    free(hashmap->hashes);
}

static void rgHashmapGrow(RgHashmap *hashmap)
{
    uint64_t old_size = hashmap->size;
    uint64_t *old_hashes = hashmap->hashes;
    uint64_t *old_values = hashmap->values;

    hashmap->size *= 2;
    hashmap->hashes = (uint64_t *)malloc(hashmap->size * sizeof(uint64_t));
    memset(hashmap->hashes, 0, hashmap->size * sizeof(uint64_t));
    hashmap->values = (uint64_t *)malloc(hashmap->size * sizeof(uint64_t));
    memset(hashmap->values, 0, hashmap->size * sizeof(uint64_t));

    for (uint64_t i = 0; i < old_size; i++)
    {
        if (old_hashes[i] != 0)
        {
            rgHashmapSet(hashmap, old_hashes[i], old_values[i]);
        }
    }

    free(old_hashes);
    free(old_values);
}

static void rgHashmapSet(RgHashmap *hashmap, uint64_t hash, uint64_t value)
{
    assert(hash != 0);

    uint64_t i = hash & (hashmap->size - 1); // hash % size
    uint64_t iters = 0;

    while ((hashmap->hashes[i] != hash) && hashmap->hashes[i] != 0 &&
           iters < hashmap->size)
    {
        i = (i + 1) & (hashmap->size - 1); // (i+1) % size
        iters += 1;
    }

    if (iters >= hashmap->size)
    {
        rgHashmapGrow(hashmap);
        rgHashmapSet(hashmap, hash, value);
        return;
    }

    hashmap->hashes[i] = hash;
    hashmap->values[i] = value;
}

static uint64_t *rgHashmapGet(RgHashmap *hashmap, uint64_t hash)
{
    uint64_t i = hash & (hashmap->size - 1); // hash % size
    uint64_t iters = 0;

    while ((hashmap->hashes[i] != hash) && hashmap->hashes[i] != 0 &&
           iters < hashmap->size)
    {
        i = (i + 1) & (hashmap->size - 1); // (i+1) % size
        iters += 1;
    }

    if (iters >= hashmap->size)
    {
        return NULL;
    }

    if (hashmap->hashes[i] != 0)
    {
        return &hashmap->values[i];
    }

    return NULL;
}
// }}}

// Type conversions {{{
static VkFormat rgFormatToVk(RgFormat fmt)
{
    switch (fmt)
    {
    case RG_FORMAT_UNDEFINED: return VK_FORMAT_UNDEFINED;

    case RG_FORMAT_R8_UNORM: return VK_FORMAT_R8_UNORM;
    case RG_FORMAT_RG8_UNORM: return VK_FORMAT_R8G8_UNORM;
    case RG_FORMAT_RGB8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
    case RG_FORMAT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;

    case RG_FORMAT_R8_UINT: return VK_FORMAT_R8_UINT;
    case RG_FORMAT_RG8_UINT: return VK_FORMAT_R8G8_UINT;
    case RG_FORMAT_RGB8_UINT: return VK_FORMAT_R8G8B8_UINT;
    case RG_FORMAT_RGBA8_UINT: return VK_FORMAT_R8G8B8A8_UINT;

    case RG_FORMAT_R16_UINT: return VK_FORMAT_R16_UINT;
    case RG_FORMAT_RG16_UINT: return VK_FORMAT_R16G16_UINT;
    case RG_FORMAT_RGB16_UINT: return VK_FORMAT_R16G16B16_UINT;
    case RG_FORMAT_RGBA16_UINT: return VK_FORMAT_R16G16B16A16_UINT;

    case RG_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT;
    case RG_FORMAT_RG32_UINT: return VK_FORMAT_R32G32_UINT;
    case RG_FORMAT_RGB32_UINT: return VK_FORMAT_R32G32B32_UINT;
    case RG_FORMAT_RGBA32_UINT: return VK_FORMAT_R32G32B32A32_UINT;

    case RG_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
    case RG_FORMAT_RG32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
    case RG_FORMAT_RGB32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
    case RG_FORMAT_RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;

    case RG_FORMAT_R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
    case RG_FORMAT_RG16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
    case RG_FORMAT_RGBA16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;

    case RG_FORMAT_BGRA8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
    case RG_FORMAT_BGRA8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;

    case RG_FORMAT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
    case RG_FORMAT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
    case RG_FORMAT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
    case RG_FORMAT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
    case RG_FORMAT_D16_UNORM: return VK_FORMAT_D16_UNORM;

    case RG_FORMAT_BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
    case RG_FORMAT_BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    }
    assert(0);
    return 0;
}

static VkFilter rgFilterToVk(RgFilter value)
{
    switch (value)
    {
    case RG_FILTER_LINEAR: return VK_FILTER_LINEAR;
    case RG_FILTER_NEAREST: return VK_FILTER_NEAREST;
    }
    assert(0);
    return 0;
}

static VkSamplerAddressMode rgAddressModeToVk(RgSamplerAddressMode value)
{
    switch (value)
    {
    case RG_SAMPLER_ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case RG_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
    assert(0);
    return 0;
}

static VkBorderColor rgBorderColorToVk(RgBorderColor value)
{
    switch (value)
    {
    case RG_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case RG_BORDER_COLOR_INT_TRANSPARENT_BLACK:
        return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    case RG_BORDER_COLOR_FLOAT_OPAQUE_BLACK: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case RG_BORDER_COLOR_INT_OPAQUE_BLACK: return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    case RG_BORDER_COLOR_FLOAT_OPAQUE_WHITE: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case RG_BORDER_COLOR_INT_OPAQUE_WHITE: return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    }
    assert(0);
    return 0;
}

static VkIndexType rgIndexTypeToVk(RgIndexType index_type)
{
    switch (index_type)
    {
    case RG_INDEX_TYPE_UINT16: return VK_INDEX_TYPE_UINT16;
    case RG_INDEX_TYPE_UINT32: return VK_INDEX_TYPE_UINT32;
    }
    assert(0);
    return 0;
}

static VkCullModeFlagBits rgCullModeToVk(RgCullMode cull_mode)
{
    switch (cull_mode)
    {
    case RG_CULL_MODE_NONE: return VK_CULL_MODE_NONE;
    case RG_CULL_MODE_BACK: return VK_CULL_MODE_BACK_BIT;
    case RG_CULL_MODE_FRONT: return VK_CULL_MODE_FRONT_BIT;
    case RG_CULL_MODE_FRONT_AND_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
    }
    assert(0);
    return 0;
}

static VkFrontFace rgFrontFaceToVk(RgFrontFace front_face)
{
    switch (front_face)
    {
    case RG_FRONT_FACE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
    case RG_FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    assert(0);
    return 0;
}

static VkPolygonMode rgPolygonModeToVk(RgPolygonMode polygon_mode)
{
    switch (polygon_mode)
    {
    case RG_POLYGON_MODE_FILL: return VK_POLYGON_MODE_FILL;
    case RG_POLYGON_MODE_LINE: return VK_POLYGON_MODE_LINE;
    case RG_POLYGON_MODE_POINT: return VK_POLYGON_MODE_POINT;
    }
    assert(0);
    return 0;
}

static VkPrimitiveTopology rgPrimitiveTopologyToVk(RgPrimitiveTopology value)
{
    switch (value)
    {
    case RG_PRIMITIVE_TOPOLOGY_LINE_LIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }
    assert(0);
    return 0;
}

static VkDescriptorType rgDescriptorTypeToVk(RgDescriptorType type)
{
    switch (type)
    {
    case RG_DESCRIPTOR_UNIFORM_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case RG_DESCRIPTOR_STORAGE_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case RG_DESCRIPTOR_STORAGE_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case RG_DESCRIPTOR_IMAGE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case RG_DESCRIPTOR_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
    case RG_DESCRIPTOR_IMAGE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    assert(0);
    return 0;
}

static VkShaderStageFlags rgShaderStageToVk(RgShaderStage shader_stage)
{
    VkShaderStageFlags vk_shader_stage = 0;
    if ((shader_stage & RG_SHADER_STAGE_FRAGMENT) == RG_SHADER_STAGE_FRAGMENT)
    {
        vk_shader_stage |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    if ((shader_stage & RG_SHADER_STAGE_VERTEX) == RG_SHADER_STAGE_VERTEX)
    {
        vk_shader_stage |= VK_SHADER_STAGE_VERTEX_BIT;
    }
    if ((shader_stage & RG_SHADER_STAGE_COMPUTE) == RG_SHADER_STAGE_COMPUTE)
    {
        vk_shader_stage |= VK_SHADER_STAGE_COMPUTE_BIT;
    }
    return vk_shader_stage;
}

static VkImageAspectFlags rgImageAspectToVk(RgImageAspect aspect)
{
    VkImageAspectFlags vk_aspect = 0;
    if ((aspect & RG_IMAGE_ASPECT_COLOR) == RG_IMAGE_ASPECT_COLOR)
    {
        vk_aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
    }
    if ((aspect & RG_IMAGE_ASPECT_DEPTH) == RG_IMAGE_ASPECT_DEPTH)
    {
        vk_aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if ((aspect & RG_IMAGE_ASPECT_STENCIL) == RG_IMAGE_ASPECT_STENCIL)
    {
        vk_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    return vk_aspect;
}

static VkCompareOp rgCompareOpToVk(RgCompareOp compare_op)
{
    switch (compare_op)
    {
    case RG_COMPARE_OP_NEVER: return VK_COMPARE_OP_NEVER;
    case RG_COMPARE_OP_LESS: return VK_COMPARE_OP_LESS;
    case RG_COMPARE_OP_EQUAL: return VK_COMPARE_OP_EQUAL;
    case RG_COMPARE_OP_LESS_OR_EQUAL: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case RG_COMPARE_OP_GREATER: return VK_COMPARE_OP_GREATER;
    case RG_COMPARE_OP_NOT_EQUAL: return VK_COMPARE_OP_NOT_EQUAL;
    case RG_COMPARE_OP_GREATER_OR_EQUAL: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case RG_COMPARE_OP_ALWAYS: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_NEVER;
}
// }}}

// Device memory allocator {{{
static int32_t
rgFindMemoryProperties2(
        const VkPhysicalDeviceMemoryProperties* memory_properties,
        uint32_t memory_type_bits_requirement,
        VkMemoryPropertyFlags required_properties)
{
    uint32_t memory_count = memory_properties->memoryTypeCount;

    for (uint32_t memory_index = 0; memory_index < memory_count; ++memory_index)
    {
        uint32_t memory_type_bits = (1 << memory_index);
        bool is_required_memory_type = memory_type_bits_requirement & memory_type_bits;

        VkMemoryPropertyFlags properties =
            memory_properties->memoryTypes[memory_index].propertyFlags;
        bool has_required_properties =
            (properties & required_properties) == required_properties;

        if (is_required_memory_type && has_required_properties)
        {
            return (int32_t)memory_index;
        }
    }

    // failed to find memory type
    return -1;
}

static VkResult rgFindMemoryProperties(
    RgAllocator *allocator,
    const RgAllocationInfo *info,
    int32_t *memory_type_index,
    VkMemoryPropertyFlagBits *required_properties)
{
    switch (info->type)
    {
    case RG_ALLOCATION_TYPE_GPU_ONLY:
        *required_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case RG_ALLOCATION_TYPE_CPU_TO_GPU:
        *required_properties =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case RG_ALLOCATION_TYPE_GPU_TO_CPU:
        *required_properties =
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    case RG_ALLOCATION_TYPE_UNKNOWN: break;
    }

    *memory_type_index = rgFindMemoryProperties2(
        &allocator->device->physical_device_memory_properties,
        info->requirements.memoryTypeBits,
        *required_properties);

    if (*memory_type_index == -1)
    {
        // We try again!

        switch (info->type)
        {
        case RG_ALLOCATION_TYPE_GPU_ONLY:
            *required_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
            break;
        case RG_ALLOCATION_TYPE_CPU_TO_GPU:
            *required_properties =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case RG_ALLOCATION_TYPE_GPU_TO_CPU:
            *required_properties =
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            break;
        case RG_ALLOCATION_TYPE_UNKNOWN: break;
        }

        *memory_type_index = rgFindMemoryProperties2(
            &allocator->device->physical_device_memory_properties,
            info->requirements.memoryTypeBits,
            *required_properties);
    }

    if (*memory_type_index == -1)
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    return VK_SUCCESS;
}

static inline RgMemoryChunk *rgMemoryChunkParent(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    if (index == 0) return NULL;

    index = (index - 1) / 2;
    if (index < 0) return NULL;

    return &block->chunks[index];
}

static inline RgMemoryChunk *rgMemoryChunkLeftChild(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    index = 2 * index + 1;
    if (index >= block->chunk_count) return NULL;

    return &block->chunks[index];
}

static inline RgMemoryChunk *rgMemoryChunkRightChild(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    index = 2 * index + 2;
    if (index >= block->chunk_count) return NULL;

    return &block->chunks[index];
}

static inline size_t rgMemoryChunkSize(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    // Tree level of the chunk starting from 0
    size_t tree_level = floor(log2((double)(index+1)));

    // chunk_size = block->size / pow(2, tree_level);
    size_t chunk_size = block->size >> tree_level;
    assert(chunk_size >= 1);
    return chunk_size;
}

static inline size_t rgMemoryChunkOffset(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    if (index == 0) return 0;

    RgMemoryChunk *parent = rgMemoryChunkParent(block, chunk);
    assert(parent);

    size_t parent_offset = rgMemoryChunkOffset(block, parent);

    if (index & 1)
    {
        // Right
        parent_offset += rgMemoryChunkSize(block, chunk);
    }

    return parent_offset;
}

static inline void rgMemoryChunkUpdateUsage(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk)
{
    if (chunk->split)
    {
        RgMemoryChunk *left = rgMemoryChunkLeftChild(block, chunk);
        RgMemoryChunk *right = rgMemoryChunkRightChild(block, chunk);
        chunk->used = left->used + right->used;
    }

    RgMemoryChunk *parent = rgMemoryChunkParent(block, chunk);
    if (parent)
    {
        rgMemoryChunkUpdateUsage(block, parent);
    }
}

static inline RgMemoryChunk *rgMemoryChunkSplit(
    RgMemoryBlock *block,
    RgMemoryChunk *chunk,
    size_t size,
    size_t alignment)
{
    assert(chunk);

    ptrdiff_t index = (ptrdiff_t)(chunk - block->chunks);
    assert(index >= 0);

    const size_t chunk_size = rgMemoryChunkSize(block, chunk);
    const size_t chunk_offset = rgMemoryChunkOffset(block, chunk);

    if ((chunk_size - chunk->used) < size) return NULL;

    RgMemoryChunk *left = rgMemoryChunkLeftChild(block, chunk);
    RgMemoryChunk *right = rgMemoryChunkRightChild(block, chunk);

    const size_t left_offset = chunk_offset;
    const size_t right_offset = chunk_offset + (chunk_size / 2);

    bool can_split = true;

    // We have to split into aligned chunks
    can_split &= ((left_offset % alignment == 0) || (right_offset % alignment == 0));

    // We have to be able to split into the required size
    can_split &= (size <= (chunk_size / 2));

    // We have to be able to have enough space to split
    can_split &= (chunk->used <= (chunk_size / 2));

    // chunk needs to have children in order to split
    can_split &= ((left != NULL) && (right != NULL));

    /* printf("index = %ld\n", index); */
    /* printf("size = %zu\n", size); */
    /* printf("alignment = %zu\n", alignment); */
    /* printf("chunk->split = %u\n", (uint32_t)chunk->split); */
    /* printf("chunk->used = %zu\n", chunk->used); */
    /* printf("chunk_size = %zu\n", chunk_size); */
    /* printf("chunk_offset = %zu\n", chunk_offset); */

    if (can_split)
    {
        if (!chunk->split)
        {
            // Chunk is not yet split, so do it now
            chunk->split = true;

            left->split = false;
            left->used = chunk->used;

            right->split = false;
            right->used = 0;
        }

        RgMemoryChunk *returned_chunk = NULL;

        returned_chunk = rgMemoryChunkSplit(block, left, size, alignment);
        if (returned_chunk) return returned_chunk;

        returned_chunk = rgMemoryChunkSplit(block, right, size, alignment);
        if (returned_chunk) return returned_chunk;
    }

    // We can't split, but if the chunk meets the requirements, return it
    if ((!chunk->split) &&
        (chunk->used == 0) &&
        (chunk_size >= size) &&
        (chunk_offset % alignment == 0))
    {
        return chunk;
    }

    return NULL;
}

static inline void rgMemoryChunkJoin(RgMemoryBlock *block, RgMemoryChunk *chunk)
{
    assert(chunk->split);

    RgMemoryChunk *left = rgMemoryChunkLeftChild(block, chunk);
    RgMemoryChunk *right = rgMemoryChunkRightChild(block, chunk);

    bool can_join = true;
    can_join &= (left->used == 0);
    can_join &= (!left->split);
    can_join &= (right->used == 0);
    can_join &= (!right->split);
    
    if (can_join)
    {
        // Join
        chunk->split = false;
        chunk->used = 0;

        RgMemoryChunk *parent = rgMemoryChunkParent(block, chunk);
        if (parent)
        {
            rgMemoryChunkJoin(block, parent);
        }
    }
}

static VkResult rgMemoryBlockAllocate(
    RgMemoryBlock *block,
    size_t size,
    size_t alignment,
    RgAllocation *allocation)
{
    assert(block->chunk_count > 0);
    RgMemoryChunk *chunk = &block->chunks[0];
    chunk = rgMemoryChunkSplit(
        block,
        chunk,
        size,
        alignment);

    if (!chunk)
    {
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    assert(chunk->used == 0);
    chunk->used = size;

    rgMemoryChunkUpdateUsage(block, chunk);

    size_t offset = rgMemoryChunkOffset(block, chunk);

    allocation->block = block;
    allocation->size = size;
    allocation->offset = offset;
    allocation->chunk_index = (size_t)(chunk - block->chunks);

    return VK_SUCCESS;
}

static void rgMemoryBlockFree(
    RgMemoryBlock *block,
    const RgAllocation *allocation)
{
    size_t chunk_index = allocation->chunk_index;
    RgMemoryChunk *chunk = &block->chunks[chunk_index];

    chunk->used = 0;
    rgMemoryChunkUpdateUsage(block, chunk);

    RgMemoryChunk *parent = rgMemoryChunkParent(block, chunk);
    if (parent) rgMemoryChunkJoin(block, parent);
}

static VkResult
rgAllocatorCreateMemoryBlock(
    RgAllocator *allocator,
    RgAllocationInfo *info,
    RgMemoryBlock **out_block)
{
    VkResult result = VK_SUCCESS;

    int32_t memory_type_index = -1;
    VkMemoryPropertyFlagBits required_properties = 0;
    result = rgFindMemoryProperties(allocator, info, &memory_type_index, &required_properties);
    if (result != VK_SUCCESS) return result;

    assert(memory_type_index >= 0);

    const uint64_t DEFAULT_DEVICE_MEMBLOCK_SIZE = 256 * 1024 * 1024;
    const uint64_t DEFAULT_HOST_MEMBLOCK_SIZE = 64 * 1024 * 1024;

    uint64_t memblock_size = 0;
    if (required_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        memblock_size = DEFAULT_HOST_MEMBLOCK_SIZE;
    }
    else
    {
        memblock_size = DEFAULT_DEVICE_MEMBLOCK_SIZE;
    }

    memblock_size = RG_MAX(info->requirements.size, memblock_size);

    // Round memblock_size to the next power of 2
    memblock_size--;
    memblock_size |= memblock_size >> 1;
    memblock_size |= memblock_size >> 2;
    memblock_size |= memblock_size >> 4;
    memblock_size |= memblock_size >> 8;
    memblock_size |= memblock_size >> 16;
    memblock_size |= memblock_size >> 32;
    memblock_size++;

    assert(memblock_size >= info->requirements.size);

    VkMemoryAllocateInfo vk_allocate_info;
    memset(&vk_allocate_info, 0, sizeof(vk_allocate_info));
    vk_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    vk_allocate_info.allocationSize = memblock_size;
    vk_allocate_info.memoryTypeIndex = (uint32_t)memory_type_index;

    VkDeviceMemory vk_memory = VK_NULL_HANDLE;
    result = vkAllocateMemory(
        allocator->device->device,
        &vk_allocate_info,
        NULL,
        &vk_memory);

    if (result != VK_SUCCESS) return result;

    void *mapping = NULL;

    if (required_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        result = vkMapMemory(
            allocator->device->device,
            vk_memory,
            0,
            VK_WHOLE_SIZE,
            0,
            &mapping);

        if (result != VK_SUCCESS)
        {
            vkFreeMemory(allocator->device->device, vk_memory, NULL);
            return result;
        }
    }

    RgMemoryBlock *block = malloc(sizeof(*block));
    memset(block, 0, sizeof(*block));

    block->handle = vk_memory;
    block->mapping = mapping;
    block->size = memblock_size;
    block->memory_type_index = (uint32_t)memory_type_index;
    block->type = info->type;
    block->chunk_count = RG_MIN(2 * 256 - 1, 2 * memblock_size - 1);
    block->chunks = malloc(sizeof(*block->chunks) * block->chunk_count);
    memset(block->chunks, 0, sizeof(*block->chunks) * block->chunk_count);

    arrPush(&allocator->blocks, block);

    *out_block = block;
    
    return result;
}

static void
rgAllocatorFreeMemoryBlock(RgAllocator *allocator, RgMemoryBlock *block)
{
    if (block->mapping)
    {
        vkUnmapMemory(
            allocator->device->device,
            block->handle);
    }
    vkFreeMemory(allocator->device->device, block->handle, NULL);
    free(block->chunks);
    free(block);
}

static RgAllocator *rgAllocatorCreate(RgDevice *device)
{
    RgAllocator *allocator = malloc(sizeof(*allocator));
    memset(allocator, 0, sizeof(*allocator));

    allocator->device = device;

    return allocator;
}

static void rgAllocatorDestroy(RgAllocator *allocator)
{
    for (uint32_t i = 0; i < allocator->blocks.len; ++i)
    {
        RgMemoryBlock *block = allocator->blocks.ptr[i];
        rgAllocatorFreeMemoryBlock(allocator, block);
    }
    arrFree(&allocator->blocks);
    free(allocator);
}

static VkResult rgAllocatorAllocate(
    RgAllocator *allocator,
    RgAllocationInfo *info,
    RgAllocation *allocation)
{
    memset(allocation, 0, sizeof(*allocation));

    if (info->dedicated)
    {
        VkResult result = VK_SUCCESS;

        int32_t memory_type_index = -1;
        VkMemoryPropertyFlagBits required_properties = 0;
        result = rgFindMemoryProperties(allocator, info, &memory_type_index, &required_properties);
        if (result != VK_SUCCESS) return result;

        assert(memory_type_index >= 0);

        VkMemoryAllocateInfo vk_allocate_info;
        memset(&vk_allocate_info, 0, sizeof(vk_allocate_info));
        vk_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        vk_allocate_info.allocationSize = info->requirements.size;
        vk_allocate_info.memoryTypeIndex = (uint32_t)memory_type_index;

        VkDeviceMemory vk_memory = VK_NULL_HANDLE;
        result = vkAllocateMemory(
            allocator->device->device,
            &vk_allocate_info,
            NULL,
            &vk_memory);

        if (result != VK_SUCCESS) return result;

        void *mapping = NULL;

        if (required_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            result = vkMapMemory(
                allocator->device->device,
                vk_memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &mapping);

            if (result != VK_SUCCESS)
            {
                vkFreeMemory(allocator->device->device, vk_memory, NULL);
                return result;
            }
        }

        allocation->dedicated = true;
        allocation->dedicated_memory = vk_memory;
        allocation->dedicated_mapping = mapping;
        allocation->size = info->requirements.size;
        allocation->offset = 0;

        return result;
    }

    for (int32_t i = ((int32_t)allocator->blocks.len)-1; i >= 0; --i)
    {
        RgMemoryBlock *block = allocator->blocks.ptr[i];

        if (info->type == block->type)
        {
            VkResult result = rgMemoryBlockAllocate(
                block,
                info->requirements.size,
                info->requirements.alignment,
                allocation);

            if (info->type == RG_ALLOCATION_TYPE_CPU_TO_GPU
                || info->type == RG_ALLOCATION_TYPE_GPU_TO_CPU)
            {
                assert(block->mapping);
            }

            if (result != VK_SUCCESS) continue;

            return VK_SUCCESS;
        }
    }

    // We could not allocate from any existing memory block, so let's make a new one
    RgMemoryBlock *block = NULL;
    VkResult result = rgAllocatorCreateMemoryBlock(allocator, info, &block);
    if (result != VK_SUCCESS) return result;

    result = rgMemoryBlockAllocate(
        block,
        info->requirements.size,
        info->requirements.alignment,
        allocation);

    return result;
}

static void rgAllocatorFree(RgAllocator *allocator, const RgAllocation *allocation)
{
    if (allocation->dedicated)
    {
        VK_CHECK(vkDeviceWaitIdle(allocator->device->device));
        if (allocation->dedicated_mapping)
        {
            vkUnmapMemory(
                allocator->device->device,
                allocation->dedicated_memory);
        }
        vkFreeMemory(allocator->device->device, allocation->dedicated_memory, NULL);
    }
    else
    {
        rgMemoryBlockFree(allocation->block, allocation);
    }
}

static VkResult rgMapAllocation(RgAllocator *allocator, const RgAllocation *allocation, void **ppData)
{
    (void)allocator;

    if (allocation->dedicated)
    {
        if (allocation->dedicated_mapping)
        {
            *ppData = allocation->dedicated_mapping;
            return VK_SUCCESS;
        }

        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    assert(allocation->block->mapping);
    if (allocation->block->mapping)
    {
        *ppData = ((uint8_t*)allocation->block->mapping) + allocation->offset;
        return VK_SUCCESS;
    }
    return VK_ERROR_MEMORY_MAP_FAILED;
}

static void rgUnmapAllocation(RgAllocator *allocator, const RgAllocation *allocation)
{
    // No-op for now
    (void)allocator;
    (void)allocation;
}
// }}}

// Device {{{
static VKAPI_ATTR VkBool32 VKAPI_CALL rgDebugMessageCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    (void)messageSeverity;
    (void)messageTypes;
    (void)pUserData;
    fprintf(stderr, "Validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static bool rgExtensionSupported(RgDevice *device, const char *ext_name)
{
    for (uint32_t i = 0; i < device->num_extension_properties; ++i)
    {
        if (strcmp(ext_name, device->extension_properties[i].extensionName) == 0)
        {
            return true;
        }
    }

    return false;
}

static uint32_t rgGetQueueFamilyIndex(RgDevice *device, VkQueueFlagBits queue_flags)
{
    // Dedicated queue for compute
    // Try to find a queue family index that supports compute but not graphics
    if (queue_flags & VK_QUEUE_COMPUTE_BIT)
    {
        for (uint32_t i = 0; i < device->num_queue_family_properties; i++)
        {
            if ((device->queue_family_properties[i].queueFlags & queue_flags) &&
                ((device->queue_family_properties[i].queueFlags &
                  VK_QUEUE_GRAPHICS_BIT) == 0))
            {
                return i;
            }
        }
    }

    if (queue_flags & VK_QUEUE_TRANSFER_BIT)
    {
        for (uint32_t i = 0; i < device->num_queue_family_properties; i++)
        {
            if ((device->queue_family_properties[i].queueFlags & queue_flags) &&
                ((device->queue_family_properties[i].queueFlags &
                  VK_QUEUE_GRAPHICS_BIT) == 0) &&
                ((device->queue_family_properties[i].queueFlags &
                  VK_QUEUE_COMPUTE_BIT) == 0))
            {
                return i;
            }
        }
    }

    // For other queue types or if no separate compute queue is present,
    // return the first one to support the requested flags
    for (uint32_t i = 0; i < device->num_queue_family_properties; i++)
    {
        if (device->queue_family_properties[i].queueFlags & queue_flags)
        {
            return i;
        }
    }

    assert(0);
    return UINT32_MAX;
}

RgFormat rgGetSupportedDepthFormat(RgDevice *device, bool check_sampling_support)
{
    // All depth formats may be optional, so we need to find a suitable depth format to use
    RgFormat depth_formats[] = {
        RG_FORMAT_D32_SFLOAT_S8_UINT,
        RG_FORMAT_D32_SFLOAT,
        RG_FORMAT_D24_UNORM_S8_UINT,
        RG_FORMAT_D16_UNORM_S8_UINT,
        RG_FORMAT_D16_UNORM 
    };

    for (uint32_t i = 0; i < RG_STATIC_ARRAY_SIZE(depth_formats); ++i)
    {
        RgFormat format = depth_formats[i];

        VkFormatProperties format_properties;
        vkGetPhysicalDeviceFormatProperties(
                device->physical_device, rgFormatToVk(format), &format_properties);
        // Format must support depth stencil attachment for optimal tiling
        if (format_properties.optimalTilingFeatures &
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            if (check_sampling_support)
            {
                if (!(format_properties.optimalTilingFeatures &
                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
                {
                    continue;
                }
            }
            return format;
        }
    }

    fprintf(stderr, "Could not find a matching depth format");
    exit(1);
}

RgDevice *rgDeviceCreate(const RgDeviceInfo *info)
{
    RgDevice *device = malloc(sizeof(*device));
    memset(device, 0, sizeof(RgDevice));

    fprintf(stderr, info->enable_validation ?
            "Using validation layers\n" :
            "NOT using validation layers\n");

    VK_CHECK(volkInitialize());

    ARRAY_OF(char *) instance_layer_names = {0};
    ARRAY_OF(char *) instance_extension_names = {0};

    if (info->enable_validation)
    {
        arrPush(&instance_layer_names, "VK_LAYER_KHRONOS_validation");
        arrPush(&instance_extension_names, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    arrPush(&instance_extension_names, "VK_KHR_surface");
#if defined(_WIN32)
    arrPush(&instance_extension_names, "VK_KHR_win32_surface");
#else
    arrPush(&instance_extension_names, "VK_KHR_xlib_surface");
#endif

    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Test app";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Test engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instance_ci = {0};
    instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo = &app_info;
    instance_ci.ppEnabledLayerNames = (const char *const*)instance_layer_names.ptr;
    instance_ci.enabledLayerCount = (uint32_t)instance_layer_names.len;
    instance_ci.ppEnabledExtensionNames = (const char *const*)instance_extension_names.ptr;
    instance_ci.enabledExtensionCount = (uint32_t)instance_extension_names.len;

    VK_CHECK(vkCreateInstance(&instance_ci, NULL, &device->instance));

    arrFree(&instance_layer_names);
    arrFree(&instance_extension_names);

    volkLoadInstance(device->instance);

    if (info->enable_validation)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {0};
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = &rgDebugMessageCallback;

        VK_CHECK(vkCreateDebugUtilsMessengerEXT(
            device->instance, &debug_create_info, NULL, &device->debug_callback));
    }

    uint32_t num_physical_devices = 0;
    vkEnumeratePhysicalDevices(device->instance, &num_physical_devices, NULL);
    VkPhysicalDevice *physical_devices = malloc(sizeof(VkPhysicalDevice) * num_physical_devices);
    vkEnumeratePhysicalDevices(device->instance, &num_physical_devices, physical_devices);

    if (num_physical_devices == 0)
    {
        fprintf(stderr, "No physical devices found\n");
        exit(1);
    }

    device->physical_device = physical_devices[0];
    free(physical_devices);

    vkGetPhysicalDeviceProperties(
        device->physical_device, &device->physical_device_properties);
    vkGetPhysicalDeviceFeatures(
        device->physical_device, &device->physical_device_features);
    vkGetPhysicalDeviceMemoryProperties(
        device->physical_device, &device->physical_device_memory_properties);

    vkGetPhysicalDeviceQueueFamilyProperties(
        device->physical_device, &device->num_queue_family_properties, NULL);
    device->queue_family_properties = 
        malloc(sizeof(VkQueueFamilyProperties) * device->num_queue_family_properties);
    vkGetPhysicalDeviceQueueFamilyProperties(
        device->physical_device,
        &device->num_queue_family_properties,
        device->queue_family_properties);

    vkEnumerateDeviceExtensionProperties(
            device->physical_device, NULL, &device->num_extension_properties, NULL);
    device->extension_properties = malloc(
            sizeof(*device->extension_properties) * device->num_extension_properties);
    vkEnumerateDeviceExtensionProperties(
            device->physical_device,
            NULL, 
            &device->num_extension_properties,
            device->extension_properties);

    fprintf(
        stderr,
        "Using physical device: %s\n",
        device->physical_device_properties.deviceName);

    if (device->physical_device_features.samplerAnisotropy)
    {
        device->enabled_features.samplerAnisotropy = VK_TRUE;
    }

    if (device->physical_device_features.fillModeNonSolid)
    {
        device->enabled_features.fillModeNonSolid = VK_TRUE;
    }

    VkQueueFlags requested_queue_types = 
        VK_QUEUE_GRAPHICS_BIT |
        VK_QUEUE_COMPUTE_BIT |
        VK_QUEUE_TRANSFER_BIT;

    ARRAY_OF(VkDeviceQueueCreateInfo) queue_create_infos = {0};
    const float default_queue_priority = 0.0f;

    if (requested_queue_types & VK_QUEUE_GRAPHICS_BIT)
    {
        device->graphics_queue_family_index =
            rgGetQueueFamilyIndex(device, VK_QUEUE_GRAPHICS_BIT);
        VkDeviceQueueCreateInfo queue_info = {0};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = device->graphics_queue_family_index;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &default_queue_priority;
        arrPush(&queue_create_infos, queue_info);
    }
    else
    {
        device->graphics_queue_family_index = UINT32_MAX;
    }

    if (requested_queue_types & VK_QUEUE_COMPUTE_BIT)
    {
        device->compute_queue_family_index =
            rgGetQueueFamilyIndex(device, VK_QUEUE_COMPUTE_BIT);
        if (device->compute_queue_family_index != device->graphics_queue_family_index)
        {
            VkDeviceQueueCreateInfo queue_info = {0};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = device->compute_queue_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &default_queue_priority;
            arrPush(&queue_create_infos, queue_info);
        }
    }
    else
    {
        device->compute_queue_family_index = device->graphics_queue_family_index;
    }

    if (requested_queue_types & VK_QUEUE_TRANSFER_BIT)
    {
        device->transfer_queue_family_index =
            rgGetQueueFamilyIndex(device, VK_QUEUE_TRANSFER_BIT);
        if ((device->transfer_queue_family_index != device->graphics_queue_family_index) &&
            (device->transfer_queue_family_index != device->compute_queue_family_index))
        {
            VkDeviceQueueCreateInfo queue_info = {0};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = device->transfer_queue_family_index;
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &default_queue_priority;
            arrPush(&queue_create_infos, queue_info);
        }
    }
    else
    {
        device->transfer_queue_family_index = device->graphics_queue_family_index;
    }

    ARRAY_OF(char*) device_extensions = {0};
    if (rgExtensionSupported(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
    {
        arrPush(&device_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    if (rgExtensionSupported(device, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
    {
        arrPush(&device_extensions, VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
        device->enable_debug_markers = true;
    }

    VkDeviceCreateInfo device_create_info = {0};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = (uint32_t)queue_create_infos.len;
    device_create_info.pQueueCreateInfos = queue_create_infos.ptr;
    device_create_info.pEnabledFeatures = &device->enabled_features;
    device_create_info.enabledExtensionCount = (uint32_t)device_extensions.len;
    device_create_info.ppEnabledExtensionNames = (const char* const*)device_extensions.ptr;

    VK_CHECK(vkCreateDevice(
                device->physical_device,
                &device_create_info,
                NULL,
                &device->device));

	vkGetDeviceQueue(
            device->device,
            device->graphics_queue_family_index,
            0,
            &device->graphics_queue);
	vkGetDeviceQueue(
            device->device,
            device->compute_queue_family_index,
            0,
            &device->compute_queue);
	vkGetDeviceQueue(
            device->device,
            device->transfer_queue_family_index,
            0,
            &device->transfer_queue);

    arrFree(&device_extensions);
    arrFree(&queue_create_infos);

    device->allocator = rgAllocatorCreate(device);

    return device;
}

void rgDeviceDestroy(RgDevice *device)
{
    if (!device) return;

    VK_CHECK(vkDeviceWaitIdle(device->device));

    rgAllocatorDestroy(device->allocator);

    vkDestroyDevice(device->device, NULL);
    if (device->debug_callback)
    {
        vkDestroyDebugUtilsMessengerEXT(device->instance, device->debug_callback, NULL);
    }
    vkDestroyInstance(device->instance, NULL);

    free(device->extension_properties);
    free(device->queue_family_properties);
    free(device);
}

void rgDeviceGetLimits(RgDevice *device, RgLimits *limits)
{
    const VkPhysicalDeviceLimits *vk_limits =
        &device->physical_device_properties.limits;
    *limits = (RgLimits) {
        .max_bound_descriptor_sets =
            vk_limits->maxBoundDescriptorSets,

        .min_texel_buffer_offset_alignment =
            vk_limits->minTexelBufferOffsetAlignment,

        .min_uniform_buffer_offset_alignment =
            vk_limits->minUniformBufferOffsetAlignment,

        .min_storage_buffer_offset_alignment =
            vk_limits->minStorageBufferOffsetAlignment,
    };
}
// }}}

// Buffer {{{
RgBuffer *rgBufferCreate(RgDevice *device, const RgBufferInfo *info)
{
    RgBuffer *buffer = (RgBuffer *)malloc(sizeof(RgBuffer));
    memset(buffer, 0, sizeof(*buffer));

    buffer->info = *info;

    assert(buffer->info.size > 0);
    assert(buffer->info.memory > 0);
    assert(buffer->info.usage > 0);

    VkBufferCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.size = buffer->info.size;

    if (buffer->info.usage & RG_BUFFER_USAGE_VERTEX)
        ci.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (buffer->info.usage & RG_BUFFER_USAGE_INDEX)
        ci.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (buffer->info.usage & RG_BUFFER_USAGE_UNIFORM)
        ci.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (buffer->info.usage & RG_BUFFER_USAGE_TRANSFER_SRC)
        ci.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (buffer->info.usage & RG_BUFFER_USAGE_TRANSFER_DST)
        ci.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (buffer->info.usage & RG_BUFFER_USAGE_STORAGE)
        ci.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VK_CHECK(vkCreateBuffer(
        device->device,
        &ci,
        NULL,
        &buffer->buffer));

    RgAllocationInfo alloc_info = {0};
    vkGetBufferMemoryRequirements(device->device, buffer->buffer, &alloc_info.requirements);

    switch (buffer->info.memory)
    {
    case RG_BUFFER_MEMORY_HOST: alloc_info.type = RG_ALLOCATION_TYPE_CPU_TO_GPU; break;
    case RG_BUFFER_MEMORY_DEVICE: alloc_info.type = RG_ALLOCATION_TYPE_GPU_ONLY; break;
    }

    /* alloc_info.dedicated = true; */
    VK_CHECK(rgAllocatorAllocate(device->allocator, &alloc_info, &buffer->allocation));

    if (buffer->allocation.dedicated)
    {
        VK_CHECK(vkBindBufferMemory(
            device->device,
            buffer->buffer,
            buffer->allocation.dedicated_memory,
            buffer->allocation.offset));
    }
    else
    {
        VK_CHECK(vkBindBufferMemory(
            device->device,
            buffer->buffer,
            buffer->allocation.block->handle,
            buffer->allocation.offset));
    }

    return buffer;
}

void rgBufferDestroy(RgDevice *device, RgBuffer *buffer)
{
    VK_CHECK(vkDeviceWaitIdle(device->device));
    if (buffer->buffer)
    {
        rgAllocatorFree(device->allocator, &buffer->allocation);
        vkDestroyBuffer(device->device, buffer->buffer, NULL);
    }

    free(buffer);
}

void *rgBufferMap(RgDevice *device, RgBuffer *buffer)
{
    void *ptr;
    VK_CHECK(rgMapAllocation(device->allocator, &buffer->allocation, &ptr));
    return ptr;
}

void rgBufferUnmap(RgDevice *device, RgBuffer *buffer)
{
    rgUnmapAllocation(device->allocator, &buffer->allocation);
}

void rgBufferUpload(
    RgDevice *device,
    RgCmdPool *cmd_pool,
    RgBuffer *buffer,
    size_t offset,
    size_t size,
    void *data)
{
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    RgBufferInfo buffer_info;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.size = size;
    buffer_info.usage = RG_BUFFER_USAGE_TRANSFER_SRC;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;

    RgBuffer *staging = rgBufferCreate(device, &buffer_info);

    void *staging_ptr = rgBufferMap(device, staging);
    memcpy(staging_ptr, data, size);
    rgBufferUnmap(device, staging);

    VkFenceCreateInfo fence_info;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(device->device, &fence_info, NULL, &fence));

    VkCommandBufferAllocateInfo alloc_info;
    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = cmd_pool->cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device->device, &alloc_info, &cmd_buffer));

    VkCommandBufferBeginInfo begin_info;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));

    VkBufferCopy region;
    memset(&region, 0, sizeof(region));
    region.srcOffset = 0;
    region.dstOffset = offset;
    region.size = size;
    vkCmdCopyBuffer(cmd_buffer, staging->buffer, buffer->buffer, 1, &region);

    VK_CHECK(vkEndCommandBuffer(cmd_buffer));

    VkSubmitInfo submit;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buffer;

    VK_CHECK(vkQueueSubmit(device->graphics_queue, 1, &submit, fence));

    VK_CHECK(vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(device->device, fence, NULL);

    vkFreeCommandBuffers(
            device->device,
            cmd_pool->cmd_pool,
            1,
            &cmd_buffer);

    rgBufferDestroy(device, staging);
}
// }}}

// Image {{{
RgImage *rgImageCreate(RgDevice *device, const RgImageInfo *image_info)
{
    RgImage *image = (RgImage *)malloc(sizeof(RgImage));
    memset(image, 0, sizeof(*image));

    assert(image_info->extent.width > 0);
    assert(image_info->extent.height > 0);
    assert(image_info->extent.depth > 0);
    assert(image_info->sample_count > 0);
    assert(image_info->mip_count > 0);
    assert(image_info->layer_count > 0);
    assert(image_info->format != RG_FORMAT_UNDEFINED);

    image->info = *image_info;

    {
        VkImageCreateInfo ci;
        memset(&ci, 0, sizeof(ci));
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = rgFormatToVk(image_info->format);
        ci.extent.width = image_info->extent.width;
        ci.extent.height = image_info->extent.height;
        ci.extent.depth = image_info->extent.depth;
        ci.mipLevels = image_info->mip_count;
        ci.arrayLayers = image_info->layer_count;
        ci.samples = (VkSampleCountFlagBits)image_info->sample_count;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;

        if (image_info->layer_count == 6)
        {
            ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        if (image_info->usage & RG_IMAGE_USAGE_SAMPLED)
            ci.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (image_info->usage & RG_IMAGE_USAGE_TRANSFER_DST)
            ci.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (image_info->usage & RG_IMAGE_USAGE_TRANSFER_SRC)
            ci.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (image_info->usage & RG_IMAGE_USAGE_STORAGE)
            ci.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (image_info->usage & RG_IMAGE_USAGE_COLOR_ATTACHMENT)
            ci.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (image_info->usage & RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT)
            ci.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VK_CHECK(vkCreateImage(device->device, &ci, NULL, &image->image));

        VkMemoryDedicatedRequirements dedicated_requirements = {0};
        dedicated_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

        VkMemoryRequirements2 memory_requirements = {0};
        memory_requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memory_requirements.pNext = &dedicated_requirements;

        VkImageMemoryRequirementsInfo2 image_requirements_info = {0};
        image_requirements_info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
        image_requirements_info.image = image->image;

        vkGetImageMemoryRequirements2(
                device->device, &image_requirements_info, &memory_requirements);

        RgAllocationInfo alloc_info = {0};
        alloc_info.type = RG_ALLOCATION_TYPE_GPU_ONLY;
        alloc_info.requirements = memory_requirements.memoryRequirements;
        alloc_info.dedicated = dedicated_requirements.prefersDedicatedAllocation ||
            dedicated_requirements.requiresDedicatedAllocation;

        size_t granularity = device->physical_device_properties
            .limits.bufferImageGranularity;

        alloc_info.requirements.size = RG_ALIGN(
                alloc_info.requirements.size, granularity);
        alloc_info.requirements.alignment = RG_ALIGN(
                alloc_info.requirements.alignment, granularity);
        /* alloc_info.dedicated = true; */

        VK_CHECK(rgAllocatorAllocate(device->allocator, &alloc_info, &image->allocation));

        if (image->allocation.dedicated)
        {
            VK_CHECK(vkBindImageMemory(
                        device->device,
                        image->image,
                        image->allocation.dedicated_memory,
                        image->allocation.offset));
        }
        else
        {
            VK_CHECK(vkBindImageMemory(
                        device->device,
                        image->image,
                        image->allocation.block->handle,
                        image->allocation.offset));
        }
    }

    {
        VkImageViewCreateInfo ci;
        memset(&ci, 0, sizeof(ci));
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ci.image = image->image;
        ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ci.format = rgFormatToVk(image_info->format);
        ci.subresourceRange.baseMipLevel = 0;
        ci.subresourceRange.levelCount = image_info->mip_count;
        ci.subresourceRange.baseArrayLayer = 0;
        ci.subresourceRange.layerCount = image_info->layer_count;

        if (image_info->layer_count == 6)
        {
            ci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }

        if (image_info->aspect & RG_IMAGE_ASPECT_COLOR)
        {
            ci.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_COLOR_BIT;
        }

        if (image_info->aspect & RG_IMAGE_ASPECT_DEPTH)
        {
            ci.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        if (image_info->aspect & RG_IMAGE_ASPECT_STENCIL)
        {
            switch (image_info->format)
            {
            case RG_FORMAT_D16_UNORM_S8_UINT:
            case RG_FORMAT_D24_UNORM_S8_UINT:
            case RG_FORMAT_D32_SFLOAT_S8_UINT:
                ci.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                break;
            default: break;
            }
        }

        VK_CHECK(vkCreateImageView(device->device, &ci, NULL, &image->view));
    }

    return image;
}

void rgImageDestroy(RgDevice *device, RgImage *image)
{
    if (!image) return;

    VK_CHECK(vkDeviceWaitIdle(device->device));

    vkDestroyImageView(device->device, image->view, NULL);
    vkDestroyImage(device->device, image->image, NULL);
    rgAllocatorFree(device->allocator, &image->allocation);

    free(image);
}

void rgImageUpload(
    RgDevice *device,
    RgCmdPool *cmd_pool,
    RgImageCopy *dst,
    RgExtent3D *extent,
    size_t size,
    void *data)
{
    VkCommandBuffer cmd_buffer = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;

    RgBufferInfo buffer_info;
    memset(&buffer_info, 0, sizeof(buffer_info));
    buffer_info.size = size;
    buffer_info.usage = RG_BUFFER_USAGE_TRANSFER_SRC;
    buffer_info.memory = RG_BUFFER_MEMORY_HOST;

    RgBuffer *staging = rgBufferCreate(device, &buffer_info);

    void *staging_ptr = rgBufferMap(device, staging);
    memcpy(staging_ptr, data, size);
    rgBufferUnmap(device, staging);

    VkFenceCreateInfo fence_info;
    memset(&fence_info, 0, sizeof(fence_info));
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(device->device, &fence_info, NULL, &fence));

    VkCommandBufferAllocateInfo alloc_info;
    memset(&alloc_info, 0, sizeof(alloc_info));
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = cmd_pool->cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device->device, &alloc_info, &cmd_buffer));

    VkCommandBufferBeginInfo begin_info;
    memset(&begin_info, 0, sizeof(begin_info));
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buffer, &begin_info));

    VkImageSubresourceRange subresource_range;
    memset(&subresource_range, 0, sizeof(subresource_range));
    subresource_range.aspectMask = rgImageAspectToVk(dst->image->info.aspect);
    subresource_range.baseMipLevel = dst->mip_level;
    subresource_range.levelCount = 1;
    subresource_range.baseArrayLayer = dst->array_layer;
    subresource_range.layerCount = 1;

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = dst->image->image;
    barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(
        cmd_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier);

    VkBufferImageCopy region;
    memset(&region, 0, sizeof(region));
    region.imageSubresource.aspectMask = rgImageAspectToVk(dst->image->info.aspect);
    region.imageSubresource.mipLevel = dst->mip_level;
    region.imageSubresource.baseArrayLayer = dst->array_layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset.x = dst->offset.x;
    region.imageOffset.y = dst->offset.y;
    region.imageOffset.z = dst->offset.z;
    region.imageExtent.width = extent->width;
    region.imageExtent.height = extent->height;
    region.imageExtent.depth = extent->depth;

    vkCmdCopyBufferToImage(
        cmd_buffer,
        staging->buffer,
        dst->image->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    vkCmdPipelineBarrier(
        cmd_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &barrier);

    VK_CHECK(vkEndCommandBuffer(cmd_buffer));

    VkSubmitInfo submit;
    memset(&submit, 0, sizeof(submit));
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buffer;

    VK_CHECK(vkQueueSubmit(device->graphics_queue, 1, &submit, fence));

    VK_CHECK(vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX));
    vkDestroyFence(device->device, fence, NULL);

    vkFreeCommandBuffers(
            device->device,
            cmd_pool->cmd_pool,
            1,
            &cmd_buffer);

    rgBufferDestroy(device, staging);
}
// }}}

// Sampler {{{
struct RgSampler
{
    RgSamplerInfo info;
    VkSampler sampler;
};

RgSampler *rgSamplerCreate(RgDevice *device, RgSamplerInfo *info)
{
    RgSampler *sampler = (RgSampler *)malloc(sizeof(RgSampler));
    memset(sampler, 0, sizeof(*sampler));

    sampler->info = *info;

    if (sampler->info.min_lod == 0.0f && sampler->info.max_lod == 0.0f)
    {
        sampler->info.max_lod = 1.0f;
    }

    if (sampler->info.max_anisotropy == 0.0f)
    {
        sampler->info.max_anisotropy = 1.0f;
    }

    assert(sampler->info.max_lod >= sampler->info.min_lod);

    VkSamplerCreateInfo ci;
    memset(&ci, 0, sizeof(ci));
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.magFilter = rgFilterToVk(sampler->info.mag_filter);
    ci.minFilter = rgFilterToVk(sampler->info.min_filter);
    ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    ci.addressModeU = rgAddressModeToVk(sampler->info.address_mode);
    ci.addressModeV = rgAddressModeToVk(sampler->info.address_mode);
    ci.addressModeW = rgAddressModeToVk(sampler->info.address_mode);
    ci.minLod = sampler->info.min_lod;
    ci.maxLod = sampler->info.max_lod;
    ci.maxAnisotropy = sampler->info.max_anisotropy;
    ci.anisotropyEnable = (VkBool32)sampler->info.anisotropy;
    ci.borderColor = rgBorderColorToVk(sampler->info.border_color);
    VK_CHECK(vkCreateSampler(device->device, &ci, NULL, &sampler->sampler));

    return sampler;
}

void rgSamplerDestroy(RgDevice *device, RgSampler *sampler)
{
    if (!sampler) return;

    VK_CHECK(vkDeviceWaitIdle(device->device));

    vkDestroySampler(device->device, sampler->sampler, NULL);

    free(sampler);
}
// }}}

// Swapchain {{{
static void rgSwapchainCreateResources(RgSwapchain *swapchain)
{
    swapchain->current_image_index = 0;
    swapchain->current_semaphore_index = 0;

    uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(
            swapchain->device->physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_family_properties = 
        malloc(sizeof(*queue_family_properties) * queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(
            swapchain->device->physical_device, &queue_family_count, queue_family_properties);

    VkBool32 *supports_present = malloc(sizeof(*supports_present) * queue_family_count);
    for (uint32_t i = 0; i < queue_family_count; i++) 
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(
                swapchain->device->physical_device,
                i,
                swapchain->surface,
                &supports_present[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphics_queue_family_index = UINT32_MAX;
    uint32_t present_queue_family_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_family_count; i++) 
    {
        if ((queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) 
        {
            if (graphics_queue_family_index == UINT32_MAX) 
            {
                graphics_queue_family_index = i;
            }

            if (supports_present[i] == VK_TRUE) 
            {
                graphics_queue_family_index = i;
                present_queue_family_index = i;
                break;
            }
        }
    }
    if (present_queue_family_index == UINT32_MAX) 
    {	
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queue_family_count; ++i) 
        {
            if (supports_present[i] == VK_TRUE) 
            {
                present_queue_family_index = i;
                break;
            }
        }
    }

    if (graphics_queue_family_index == UINT32_MAX ||
        present_queue_family_index == UINT32_MAX) 
    {
        fprintf(
            stderr,
            "Could not find a graphics and/or presenting queue!\n");
        exit(1);
    }

    if (graphics_queue_family_index != present_queue_family_index) 
    {
        fprintf(
            stderr,
            "Separate graphics and presenting queues are not supported yet!\n");
        exit(1);
    }

	swapchain->queue_family_index = graphics_queue_family_index;

    uint32_t surface_format_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
                swapchain->device->physical_device,
                swapchain->surface, &surface_format_count, NULL));
    assert(surface_format_count > 0);
    VkSurfaceFormatKHR *surface_formats =
        malloc(sizeof(*surface_formats) * surface_format_count);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
                swapchain->device->physical_device,
                swapchain->surface,
                &surface_format_count,
                surface_formats));

    if ((surface_format_count == 1) &&
        (surface_formats[0].format == VK_FORMAT_UNDEFINED))
    {
        swapchain->color_format = VK_FORMAT_B8G8R8A8_UNORM;
        swapchain->color_space = surface_formats[0].colorSpace;
    }
    else
    {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (uint32_t i = 0; i < surface_format_count; ++i)
        {
            VkSurfaceFormatKHR surface_format = surface_formats[i];
            if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                swapchain->color_format = surface_format.format;
                swapchain->color_space = surface_format.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM)
        {
            swapchain->color_format = surface_formats[0].format;
            swapchain->color_space = surface_formats[0].colorSpace;
        }
    }

    free(surface_formats);
    free(supports_present);
    free(queue_family_properties);

    //
    // Swapchain creation
    //

    VkSurfaceCapabilitiesKHR surf_caps = {0};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                swapchain->device->physical_device,
                swapchain->surface,
                &surf_caps));

    uint32_t present_mode_count = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
                swapchain->device->physical_device,
                swapchain->surface,
                &present_mode_count,
                NULL));
    assert(present_mode_count > 0);

    VkPresentModeKHR *present_modes = malloc(sizeof(*present_modes) * present_mode_count);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
                swapchain->device->physical_device,
                swapchain->surface,
                &present_mode_count,
                present_modes));

    VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!swapchain->vsync)
    {
        for (uint32_t i = 0; i < present_mode_count; i++)
        {
            if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if ((swapchain_present_mode != VK_PRESENT_MODE_MAILBOX_KHR) &&
                    (present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
            {
                swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desired_number_of_swapchain_images = surf_caps.minImageCount + 1;
    if ((surf_caps.maxImageCount > 0) && 
        (desired_number_of_swapchain_images > surf_caps.maxImageCount))
    {
        desired_number_of_swapchain_images = surf_caps.maxImageCount;
    }


    VkSurfaceTransformFlagsKHR pre_transform;
    if (surf_caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        // We prefer a non-rotated transform
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        pre_transform = surf_caps.currentTransform;
    }

    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    VkCompositeAlphaFlagBitsKHR composite_alpha_flags[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (uint32_t i = 0; i < RG_STATIC_ARRAY_SIZE(composite_alpha_flags); ++i)
    {
        VkCompositeAlphaFlagBitsKHR composite_alpha_flag = composite_alpha_flags[i];
        if (surf_caps.supportedCompositeAlpha & composite_alpha_flag) {
            composite_alpha = composite_alpha_flag;
            break;
        }
    }

    VkSwapchainKHR old_swapchain = swapchain->swapchain;

    // If width (and height) equals the special value 0xFFFFFFFF,
    // the size of the surface will be set by the swapchain
    if (surf_caps.currentExtent.width == (uint32_t)-1)
    {
        swapchain->extent = surf_caps.currentExtent;
    }

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                swapchain->device->physical_device,
                swapchain->surface,
                &surf_caps));

    swapchain->extent.width = RG_CLAMP(
        swapchain->extent.width,
        surf_caps.minImageExtent.width,
        surf_caps.maxImageExtent.width
    );

    swapchain->extent.height = RG_CLAMP(
        swapchain->extent.height,
        surf_caps.minImageExtent.height,
        surf_caps.maxImageExtent.height
    );

    VkSwapchainCreateInfoKHR swapchain_ci = {0};
    swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_ci.pNext = NULL;
    swapchain_ci.surface = swapchain->surface;
    swapchain_ci.minImageCount = desired_number_of_swapchain_images;
    swapchain_ci.imageFormat = swapchain->color_format;
    swapchain_ci.imageColorSpace = swapchain->color_space;
    swapchain_ci.imageExtent = swapchain->extent;
    swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_ci.preTransform = (VkSurfaceTransformFlagBitsKHR)pre_transform;
    swapchain_ci.imageArrayLayers = 1;
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices = NULL;
    swapchain_ci.presentMode = swapchain_present_mode;
    swapchain_ci.oldSwapchain = old_swapchain;
    // Setting clipped to VK_TRUE allows the implementation to discard
    // rendering outside of the surface area
    swapchain_ci.clipped = VK_TRUE;
    swapchain_ci.compositeAlpha = composite_alpha;

	// Enable transfer source on swap chain images if supported
	if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
		swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	// Enable transfer destination on swap chain images if supported
	if (surf_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
		swapchain_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

    VK_CHECK(vkCreateSwapchainKHR(
                swapchain->device->device, &swapchain_ci, NULL, &swapchain->swapchain));

    vkDestroySwapchainKHR(
            swapchain->device->device,
            old_swapchain,
            NULL);

	VK_CHECK(vkGetSwapchainImagesKHR(
                swapchain->device->device,
                swapchain->swapchain,
                &swapchain->image_count,
                NULL));

    swapchain->images = malloc(sizeof(VkImage) * swapchain->image_count);
    swapchain->image_views = malloc(sizeof(VkImageView) * swapchain->image_count);
    swapchain->present_complete_semaphores =
        malloc(sizeof(VkSemaphore) * swapchain->image_count);

	VK_CHECK(vkGetSwapchainImagesKHR(
                swapchain->device->device,
                swapchain->swapchain,
                &swapchain->image_count,
                swapchain->images));

    for (uint32_t i = 0; i < swapchain->image_count; ++i)
    {
        VkImageViewCreateInfo color_attachment_view = {0};
		color_attachment_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		color_attachment_view.pNext = NULL;
		color_attachment_view.format = swapchain->color_format;
		color_attachment_view.components = (VkComponentMapping){
			.r = VK_COMPONENT_SWIZZLE_R,
			.g = VK_COMPONENT_SWIZZLE_G,
			.b = VK_COMPONENT_SWIZZLE_B,
			.a = VK_COMPONENT_SWIZZLE_A
		};
		color_attachment_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		color_attachment_view.subresourceRange.baseMipLevel = 0;
		color_attachment_view.subresourceRange.levelCount = 1;
		color_attachment_view.subresourceRange.baseArrayLayer = 0;
		color_attachment_view.subresourceRange.layerCount = 1;
		color_attachment_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		color_attachment_view.flags = 0;
		color_attachment_view.image = swapchain->images[i];

		VK_CHECK(vkCreateImageView(
                    swapchain->device->device,
                    &color_attachment_view,
                    NULL,
                    &swapchain->image_views[i]));
    }

    free(present_modes);

    for (uint32_t i = 0; i < swapchain->image_count; ++i)
    {
        VkSemaphoreCreateInfo semaphore_create_info = {0};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VK_CHECK(vkCreateSemaphore(
                    swapchain->device->device,
                    &semaphore_create_info,
                    NULL,
                    &swapchain->present_complete_semaphores[i]));
    }

    bool has_depth_buffer = swapchain->depth_format != RG_FORMAT_UNDEFINED;

    if (has_depth_buffer)
    {
        RgImageInfo image_info = {0};
        image_info.extent = (RgExtent3D){
            swapchain->extent.width,
            swapchain->extent.height,
            1,
        };
        image_info.sample_count = 1;
        image_info.mip_count = 1;
        image_info.layer_count = 1;
        image_info.usage = RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT;
        image_info.aspect = RG_IMAGE_ASPECT_DEPTH;
        image_info.format = swapchain->depth_format;

        if (image_info.format >= RG_FORMAT_D16_UNORM_S8_UINT)
        {
            image_info.aspect |= RG_IMAGE_ASPECT_STENCIL;
        }

        swapchain->depth_image = rgImageCreate(swapchain->device, &image_info);
    }

    //
    // Create render pass
    //
    memset(swapchain->render_pass, 0, sizeof(*swapchain->render_pass));

    swapchain->render_pass->width = swapchain->extent.width;
    swapchain->render_pass->height = swapchain->extent.height;
    swapchain->render_pass->color_attachment_count = 1;

    VkAttachmentDescription attachments[2] = {0};

    uint32_t attachment_count = 1;
    if (has_depth_buffer) attachment_count++;

    VkAttachmentReference color_reference = {0};
    VkAttachmentReference depth_stencil_reference = {0};

    {
        VkAttachmentDescription desc = {0};
        desc.format = swapchain->color_format;
        desc.samples = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        attachments[0] = desc;

        color_reference.attachment = 0;
        color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    if (has_depth_buffer)
    {
        VkAttachmentDescription desc = {0};
        desc.format = rgFormatToVk(swapchain->depth_image->info.format);
        desc.samples = (VkSampleCountFlagBits)swapchain->depth_image->info.sample_count;
        desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments[1] = desc;

        depth_stencil_reference.attachment = 1;
        depth_stencil_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDependency dependencies[2];
    memset(dependencies, 0, sizeof(dependencies));

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription subpass_description = {0};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = 1;
    subpass_description.pColorAttachments = &color_reference;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = NULL;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = NULL;
    subpass_description.pResolveAttachments = NULL;
    if (has_depth_buffer)
    {
        subpass_description.pDepthStencilAttachment = &depth_stencil_reference;
    }

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = RG_STATIC_ARRAY_SIZE(dependencies);
    render_pass_info.pDependencies = dependencies;

    VK_CHECK(vkCreateRenderPass(
                swapchain->device->device,
                &render_pass_info,
                NULL,
                &swapchain->render_pass->render_pass));

    swapchain->render_pass->hash = rgHashRenderPass(&render_pass_info);

    swapchain->render_pass->framebuffer_count = swapchain->image_count;
    swapchain->render_pass->framebuffers =
        malloc(sizeof(VkFramebuffer) * swapchain->render_pass->framebuffer_count);

    VkImageView fb_attachments[2] = {0};

    VkFramebufferCreateInfo frame_buffer_create_info = {0};
    frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_create_info.pNext = NULL;
    frame_buffer_create_info.renderPass = swapchain->render_pass->render_pass;
    frame_buffer_create_info.attachmentCount = attachment_count;
    frame_buffer_create_info.pAttachments = fb_attachments;
    frame_buffer_create_info.width = swapchain->render_pass->width;
    frame_buffer_create_info.height = swapchain->render_pass->height;
    frame_buffer_create_info.layers = 1;

    if (has_depth_buffer)
    {
        fb_attachments[1] = swapchain->depth_image->view;
    }

    for (uint32_t i = 0; i < swapchain->image_count; ++i)
    {
        fb_attachments[0] = swapchain->image_views[i];

        VK_CHECK(vkCreateFramebuffer(
                    swapchain->device->device,
                    &frame_buffer_create_info,
                    NULL,
                    &swapchain->render_pass->framebuffers[i]));
    }
}

static void rgSwapchainDestroyResources(RgSwapchain *swapchain)
{
    VK_CHECK(vkDeviceWaitIdle(swapchain->device->device));

    if (swapchain->depth_image)
    {
        rgImageDestroy(swapchain->device, swapchain->depth_image);
    }

    for (uint32_t i = 0; i < swapchain->render_pass->framebuffer_count; ++i)
    {
        vkDestroyFramebuffer(swapchain->device->device,
                swapchain->render_pass->framebuffers[i], NULL);
    }

    vkDestroyRenderPass(swapchain->device->device, swapchain->render_pass->render_pass, NULL);

    for (uint32_t i = 0; i < swapchain->image_count; ++i)
    {
        vkDestroyImageView(
                swapchain->device->device,
                swapchain->image_views[i],
                NULL);
        vkDestroySemaphore(
                swapchain->device->device,
                swapchain->present_complete_semaphores[i],
                NULL);
    }

    free(swapchain->render_pass->framebuffers);
    free(swapchain->images);
    free(swapchain->image_views);
    free(swapchain->present_complete_semaphores);
}

RgSwapchain *rgSwapchainCreate(RgDevice *device, const RgSwapchainInfo *info)
{
    RgSwapchain *swapchain = malloc(sizeof(*swapchain));
    memset(swapchain, 0, sizeof(*swapchain));

    swapchain->device = device;
    swapchain->display_handle = info->display_handle;
    swapchain->window_handle = info->window_handle;
    swapchain->vsync = info->vsync;
    swapchain->depth_format = info->depth_format;

    swapchain->extent.width = info->width;
    swapchain->extent.height = info->height;

    swapchain->render_pass = malloc(sizeof(*swapchain->render_pass));
    memset(swapchain->render_pass, 0, sizeof(*swapchain->render_pass));

#ifdef _WIN32
    VkWin32SurfaceCreateInfoKHR surface_create_info = {0};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(NULL);
    surface_create_info.hwnd = (HWND)swapchain->window_handle;
    VK_CHECK(vkCreateWin32SurfaceKHR(
                device->instance,
                &surface_create_info,
                NULL,
                &swapchain->surface));
#else
    VkXlibSurfaceCreateInfoKHR surface_create_info = {0};
    surface_create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_create_info.dpy = (Display*)swapchain->display_handle;
    surface_create_info.window = (Window)swapchain->window_handle;
    VK_CHECK(vkCreateXlibSurfaceKHR(
                device->instance,
                &surface_create_info,
                NULL,
                &swapchain->surface));
#endif

    rgSwapchainCreateResources(swapchain);

    return swapchain;
}

void rgSwapchainDestroy(RgDevice *device, RgSwapchain *swapchain)
{
    if (!swapchain) return;

    rgSwapchainDestroyResources(swapchain);

    vkDestroySwapchainKHR(
            device->device,
            swapchain->swapchain,
            NULL);

    vkDestroySurfaceKHR(
            device->instance,
            swapchain->surface,
            NULL);

    free(swapchain->render_pass);

    arrFree(&swapchain->wait_semaphores);
    arrFree(&swapchain->wait_fences);
    free(swapchain);
}

RgRenderPass *rgSwapchainGetRenderPass(RgSwapchain *swapchain)
{
    return swapchain->render_pass;
}

void rgSwapchainWaitForCommands(RgSwapchain *swapchain, RgCmdBuffer *wait_cmd_buffer)
{
    arrPush(&swapchain->wait_semaphores, wait_cmd_buffer->semaphore);
    arrPush(&swapchain->wait_fences, wait_cmd_buffer->fence);
}

void rgSwapchainAcquireImage(RgSwapchain *swapchain)
{
    VkResult result = VK_SUBOPTIMAL_KHR;
    do 
    {
        swapchain->current_semaphore_index = 
            (swapchain->current_semaphore_index + 1) % swapchain->image_count;

        result = vkAcquireNextImageKHR(
                swapchain->device->device,
                swapchain->swapchain,
                UINT64_MAX,
                swapchain->present_complete_semaphores[swapchain->
                    current_semaphore_index],
                VK_NULL_HANDLE,
                &swapchain->current_image_index);
        if ((result == VK_ERROR_OUT_OF_DATE_KHR) || (result == VK_SUBOPTIMAL_KHR)) 
        {
            rgSwapchainDestroyResources(swapchain);
            rgSwapchainCreateResources(swapchain);
        }
        else
        {
            VK_CHECK(result);
        }       

        swapchain->render_pass->current_framebuffer =
            swapchain->current_image_index;
    } while (result != VK_SUCCESS);
}

void rgSwapchainPresent(RgSwapchain *swapchain)
{
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->swapchain;
    present_info.pImageIndices = &swapchain->current_image_index;
    present_info.pWaitSemaphores = swapchain->wait_semaphores.ptr;
    present_info.waitSemaphoreCount = (uint32_t)swapchain->wait_semaphores.len;

    VkResult result = vkQueuePresentKHR(swapchain->device->graphics_queue, &present_info);

    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) 
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) 
        {
            // Swap chain is no longer compatible with the surface and needs to be recreated
            rgSwapchainDestroyResources(swapchain);
            rgSwapchainCreateResources(swapchain);
        }
        else
        {
            VK_CHECK(result);
        }
    }

    /* if (result != VK_ERROR_OUT_OF_DATE_KHR) */
    if (swapchain->wait_fences.len > 0)
    {
        VK_CHECK(vkWaitForFences(
                    swapchain->device->device,
                    (uint32_t)swapchain->wait_fences.len,
                    swapchain->wait_fences.ptr,
                    VK_TRUE,
                    1ULL * 1000000000ULL // 1 second timeout
                    ));

        VK_CHECK(vkResetFences(
                    swapchain->device->device,
                    (uint32_t)swapchain->wait_fences.len,
                    swapchain->wait_fences.ptr));
    }

    swapchain->wait_semaphores.len = 0;
    swapchain->wait_fences.len = 0;
}
// }}}

// RenderPass {{{
RgRenderPass *rgRenderPassCreate(RgDevice *device, const RgRenderPassInfo *info)
{
    RgRenderPass *render_pass = malloc(sizeof(*render_pass));
    memset(render_pass, 0, sizeof(*render_pass));

    render_pass->color_attachment_count = info->color_attachment_count;

    uint32_t attachment_count = 0;
    attachment_count += info->color_attachment_count;
    if (info->depth_stencil_attachment) attachment_count++;

    VkAttachmentDescription *attachments =
        malloc(sizeof(*attachments) * attachment_count);
    VkAttachmentReference *color_references =
        malloc(sizeof (*color_references) * info->color_attachment_count);
    VkAttachmentReference depth_stencil_reference = {0};

    uint32_t width = 0, height = 0;

    uint32_t current_attachment = 0;

    for (uint32_t i = 0; i < info->color_attachment_count; ++i)
    {
        RgImage *image = info->color_attachments[i];

        if (width == 0 || height == 0)
        {
            width = image->info.extent.width;
            height = image->info.extent.height;
        }
        else
        {
            assert(width == image->info.extent.width);
            assert(height == image->info.extent.height);
        }

        VkAttachmentDescription desc = {0};
        desc.format = rgFormatToVk(image->info.format);
        desc.samples = (VkSampleCountFlagBits)image->info.sample_count;
        desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        attachments[current_attachment] = desc;

        VkAttachmentReference reference = {0};
        reference.attachment = current_attachment;
        reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_references[i] = reference;

        current_attachment++;
    }

    if (info->depth_stencil_attachment)
    {
        RgImage *image = info->depth_stencil_attachment;

        if (width == 0 || height == 0)
        {
            width = image->info.extent.width;
            height = image->info.extent.height;
        }
        else
        {
            assert(width == image->info.extent.width);
            assert(height == image->info.extent.height);
        }

        VkAttachmentDescription desc = {0};
        desc.format = rgFormatToVk(image->info.format);
        desc.samples = (VkSampleCountFlagBits)image->info.sample_count;
        desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        attachments[current_attachment] = desc;

        depth_stencil_reference.attachment = current_attachment;
        depth_stencil_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        current_attachment++;
    }

    assert(current_attachment == attachment_count);

    VkSubpassDependency dependencies[2];
    memset(dependencies, 0, sizeof(dependencies));

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription subpass_description = {0};
    subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description.colorAttachmentCount = info->color_attachment_count;
    subpass_description.pColorAttachments = color_references;
    subpass_description.inputAttachmentCount = 0;
    subpass_description.pInputAttachments = NULL;
    subpass_description.preserveAttachmentCount = 0;
    subpass_description.pPreserveAttachments = NULL;
    subpass_description.pResolveAttachments = NULL;
    if (info->depth_stencil_attachment)
    {
        subpass_description.pDepthStencilAttachment = &depth_stencil_reference;
    }

    VkRenderPassCreateInfo render_pass_info = {0};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = attachment_count;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_description;
    render_pass_info.dependencyCount = RG_STATIC_ARRAY_SIZE(dependencies);
    render_pass_info.pDependencies = dependencies;

    VK_CHECK(vkCreateRenderPass(
                device->device,
                &render_pass_info,
                NULL,
                &render_pass->render_pass));

    render_pass->hash = rgHashRenderPass(&render_pass_info);

    current_attachment = 0;
    VkImageView *fb_attachments = malloc(sizeof(*fb_attachments) * attachment_count);

    for (uint32_t i = 0; i < info->color_attachment_count; ++i)
    {
        RgImage *image = info->color_attachments[i];
        fb_attachments[current_attachment] = image->view;
        current_attachment++;
    }

    if (info->depth_stencil_attachment)
    {
        RgImage *image = info->depth_stencil_attachment;
        fb_attachments[current_attachment] = image->view;
        current_attachment++;
    }

    render_pass->framebuffer_count = 1;
    render_pass->framebuffers =
        malloc(sizeof(*render_pass->framebuffers) * render_pass->framebuffer_count);

    VkFramebufferCreateInfo frame_buffer_create_info = {0};
    frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frame_buffer_create_info.pNext = NULL;
    frame_buffer_create_info.renderPass = render_pass->render_pass;
    frame_buffer_create_info.attachmentCount = attachment_count;
    frame_buffer_create_info.pAttachments = fb_attachments;
    frame_buffer_create_info.width = width;
    frame_buffer_create_info.height = height;
    frame_buffer_create_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(
                device->device,
                &frame_buffer_create_info,
                NULL,
                &render_pass->framebuffers[0]));

    free(color_references);
    free(attachments);
    free(fb_attachments);

    render_pass->width = width;
    render_pass->height = height;

    return render_pass;
}

void rgRenderPassDestroy(RgDevice *device, RgRenderPass *render_pass)
{
    if (!render_pass) return;

    VK_CHECK(vkDeviceWaitIdle(device->device));

    for (uint32_t i = 0; i < render_pass->framebuffer_count; ++i)
    {
        vkDestroyFramebuffer(device->device, render_pass->framebuffers[i], NULL);
    }

    vkDestroyRenderPass(device->device, render_pass->render_pass, NULL);

    free(render_pass->framebuffers);
    free(render_pass);
}
// }}}

// Descriptor set pool {{{
static RgDescriptorSetPool *rgDescriptorSetPoolCreate(
        RgDescriptorSetLayout *set_layout,
        uint32_t set_count)
{
    RgDescriptorSetPool *pool = malloc(sizeof(*pool));
    memset(pool, 0, sizeof(*pool));

    pool->set_count = set_count;
    pool->set_layout = set_layout;

    ARRAY_OF(VkDescriptorPoolSize) pool_sizes = {0};

    for (VkDescriptorSetLayoutBinding *binding = set_layout->bindings;
         binding != set_layout->bindings + set_layout->binding_count;
         ++binding)
    {
        VkDescriptorPoolSize pool_size = {0};
        pool_size.descriptorCount = binding->descriptorCount * pool->set_count;
        pool_size.type = binding->descriptorType;
        arrPush(&pool_sizes, pool_size);
    }

    VkDescriptorPoolCreateInfo descriptor_pool_info = {0};
    descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptor_pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptor_pool_info.maxSets = pool->set_count;
    descriptor_pool_info.pPoolSizes = pool_sizes.ptr;
    descriptor_pool_info.poolSizeCount = (uint32_t)pool_sizes.len;
    VK_CHECK(vkCreateDescriptorPool(
                set_layout->device->device,
                &descriptor_pool_info,
                NULL,
                &pool->pool));

    VkDescriptorSetLayout *set_layouts =
        malloc(sizeof(VkDescriptorSetLayout) * pool->set_count);
    VkDescriptorSet *sets = 
        malloc(sizeof(VkDescriptorSet) * pool->set_count);

    for (uint32_t i = 0; i < pool->set_count; ++i)
    {
        set_layouts[i] = set_layout->set_layout;
    }

    VkDescriptorSetAllocateInfo set_alloc_info = {0};
    set_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc_info.descriptorPool = pool->pool;
    set_alloc_info.descriptorSetCount = pool->set_count;
    set_alloc_info.pSetLayouts = set_layouts; 

    VK_CHECK(vkAllocateDescriptorSets(
        set_layout->device->device,
        &set_alloc_info,
        sets));

    pool->sets = 
        malloc(sizeof(RgDescriptorSet) * pool->set_count);
    arrAlloc(&pool->free_list, pool->set_count);
    for (uint32_t i = 0; i < pool->set_count; ++i)
    {
        pool->sets[i].set = sets[i];
        pool->sets[i].pool = pool;
        pool->free_list.ptr[i] = &pool->sets[i];
    }

    free(set_layouts);
    free(sets);
    arrFree(&pool_sizes);

    return pool;
}

static void rgDescriptorSetPoolDestroy(
        RgDevice *device,
        RgDescriptorSetPool *pool)
{
    vkDestroyDescriptorPool(device->device, pool->pool, NULL);
    arrFree(&pool->free_list);
    free(pool->sets);
    free(pool);
}
// }}}

// Descriptor set layout {{{
RgDescriptorSetLayout *rgDescriptorSetLayoutCreate(
        RgDevice *device,
        const RgDescriptorSetLayoutInfo *info)
{
    RgDescriptorSetLayout *set_layout = malloc(sizeof(*set_layout));
    memset(set_layout, 0, sizeof(*set_layout));

    set_layout->device = device;

    set_layout->bindings = malloc(sizeof(*set_layout->bindings) * info->entry_count);
    memset(set_layout->bindings, 0, sizeof(*set_layout->bindings) * info->entry_count);
    set_layout->binding_count = info->entry_count;

    assert(info->entry_count <= RG_MAX_DESCRIPTOR_SET_BINDINGS);
    for (uint32_t b = 0; b < info->entry_count; ++b)
    {
        RgDescriptorSetLayoutEntry *entry = &info->entries[b];
        VkDescriptorSetLayoutBinding *binding = &set_layout->bindings[entry->binding];

        binding->binding = entry->binding;
        binding->descriptorType = rgDescriptorTypeToVk(entry->type);
        binding->descriptorCount = entry->count;
        binding->stageFlags = rgShaderStageToVk(entry->shader_stages);
    }

    VkDescriptorSetLayoutCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create_info.bindingCount = set_layout->binding_count;
    create_info.pBindings = set_layout->bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(
        device->device, &create_info, NULL, &set_layout->set_layout));

    return set_layout;
}

void rgDescriptorSetLayoutDestroy(
        RgDevice *device,
        RgDescriptorSetLayout *set_layout)
{
    for (RgDescriptorSetPool **pool = set_layout->pools.ptr;
         pool != set_layout->pools.ptr + set_layout->pools.len;
         ++pool)
    {
        rgDescriptorSetPoolDestroy(device, *pool);
    }

    vkDestroyDescriptorSetLayout(device->device, set_layout->set_layout, NULL);

    arrFree(&set_layout->pools);
    free(set_layout->bindings);
    free(set_layout);
}
// }}}

// Descriptor set {{{
RgDescriptorSet *rgDescriptorSetCreate(RgDevice *device, const RgDescriptorSetInfo *info)
{
    RgDescriptorSetLayout *set_layout = info->layout;
    RgDescriptorSet *descriptor_set = NULL;

    for (int64_t i = set_layout->pools.len-1; i >= 0; --i)
    {
        RgDescriptorSetPool *pool = set_layout->pools.ptr[i];
        if (pool->free_list.len == 0) continue;

        RgDescriptorSet *set = pool->free_list.ptr[pool->free_list.len-1];
        pool->free_list.len--;
        descriptor_set = set;
        break;
    }

    if (descriptor_set == NULL)
    {
        uint32_t pool_set_count = 8;
        if (set_layout->pools.len > 0)
        {
            // Double the set count of the previous pool
            pool_set_count =
                set_layout->pools.ptr[set_layout->pools.len-1]->set_count * 2;
        }
        pool_set_count = RG_MIN(pool_set_count, 128);

        RgDescriptorSetPool *new_pool =
            rgDescriptorSetPoolCreate(set_layout, pool_set_count);
        arrPush(&set_layout->pools, new_pool);

        return rgDescriptorSetCreate(device, info);
    }

    VkWriteDescriptorSet writes[RG_MAX_DESCRIPTOR_SET_BINDINGS];
    VkDescriptorBufferInfo buffer_infos[RG_MAX_DESCRIPTOR_SET_BINDINGS];
    VkDescriptorImageInfo image_infos[RG_MAX_DESCRIPTOR_SET_BINDINGS];
    for (uint32_t i = 0; i < info->entry_count; ++i)
    {
        VkWriteDescriptorSet *write = &writes[i];
        memset(write, 0, sizeof(*write));

        RgDescriptorSetEntry *entry = &info->entries[i];

        write->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write->dstSet = descriptor_set->set;
        write->dstBinding = entry->binding;
        write->descriptorCount = 1;
        write->descriptorType = set_layout->bindings[entry->binding].descriptorType;

        if (entry->buffer)
        {
            VkDescriptorBufferInfo *buffer_info = &buffer_infos[i];
            write->pBufferInfo = buffer_info;

            buffer_info->buffer = entry->buffer->buffer;
            buffer_info->offset = entry->offset;
            buffer_info->range = entry->size;
            if (entry->size == 0)
            {
                buffer_info->range = VK_WHOLE_SIZE;
            }
        }

        if (entry->image)
        {
            VkDescriptorImageInfo *image_info = &image_infos[i];
            write->pImageInfo = image_info;

            image_info->imageView = entry->image->view;
            image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        if (entry->sampler)
        {
            VkDescriptorImageInfo *image_info = &image_infos[i];
            write->pImageInfo = image_info;

            image_info->sampler = entry->sampler->sampler;
        }
    }

    vkUpdateDescriptorSets(set_layout->device->device, info->entry_count, writes, 0, NULL);

    return descriptor_set;
}

void rgDescriptorSetDestroy(RgDevice *device, RgDescriptorSet *descriptor_set)
{
    (void)device;

    RgDescriptorSetPool *pool = descriptor_set->pool;
    // Return the set to the free list
    arrPush(&pool->free_list, descriptor_set);
    assert(pool->free_list.len <= pool->set_count);
}
// }}}

// Pipeline {{{
RgPipeline *rgGraphicsPipelineCreate(RgDevice *device, const RgGraphicsPipelineInfo *info)
{
    RgPipeline *pipeline = malloc(sizeof(RgPipeline));
    memset(pipeline, 0, sizeof(*pipeline));

    pipeline->device = device;
    pipeline->type = RG_PIPELINE_TYPE_GRAPHICS;

    pipeline->graphics.polygon_mode = info->polygon_mode;
    pipeline->graphics.cull_mode = info->cull_mode;
    pipeline->graphics.front_face = info->front_face;
    pipeline->graphics.topology = info->topology;
    pipeline->graphics.blend = info->blend;
    pipeline->graphics.depth_stencil = info->depth_stencil;

    pipeline->graphics.vertex_stride = info->vertex_stride;

    if (info->vertex_entry)
    {
        size_t str_size = strlen(info->vertex_entry) + 1;
        pipeline->graphics.vertex_entry = malloc(str_size);
        memcpy(pipeline->graphics.vertex_entry, info->vertex_entry, str_size);
    }
    if (info->fragment_entry)
    {
        size_t str_size = strlen(info->fragment_entry) + 1;
        pipeline->graphics.fragment_entry = malloc(str_size);
        memcpy(pipeline->graphics.fragment_entry, info->fragment_entry, str_size);
    }

    // Vertex attributes
    pipeline->graphics.num_vertex_attributes = info->num_vertex_attributes;
    pipeline->graphics.vertex_attributes = malloc(
        pipeline->graphics.num_vertex_attributes *
        sizeof(*pipeline->graphics.vertex_attributes));
    memcpy(
        pipeline->graphics.vertex_attributes,
        info->vertex_attributes,
        pipeline->graphics.num_vertex_attributes *
        sizeof(*pipeline->graphics.vertex_attributes));

    rgHashmapInit(&pipeline->graphics.instances, 8);

    //
    // Create pipeline layout
    //

    VkDescriptorSetLayout *vk_set_layouts =
        malloc(sizeof(*vk_set_layouts) * info->set_layout_count);

    for (uint32_t i = 0; i < info->set_layout_count; ++i)
    {
        vk_set_layouts[i] = info->set_layouts[i]->set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info;
    memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = info->set_layout_count;
    pipeline_layout_info.pSetLayouts = vk_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    VK_CHECK(vkCreatePipelineLayout(
        device->device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout));

    free(vk_set_layouts);

    if (info->vertex && info->vertex_size > 0)
    {
        VkShaderModuleCreateInfo module_create_info;
        memset(&module_create_info, 0, sizeof(module_create_info));
        module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_create_info.codeSize = info->vertex_size;
        module_create_info.pCode = (uint32_t *)info->vertex;

        VK_CHECK(vkCreateShaderModule(
            device->device,
            &module_create_info,
            NULL,
            &pipeline->graphics.vertex_shader));
    }

    if (info->fragment && info->fragment_size > 0)
    {
        VkShaderModuleCreateInfo module_create_info;
        memset(&module_create_info, 0, sizeof(module_create_info));
        module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        module_create_info.codeSize = info->fragment_size;
        module_create_info.pCode = (uint32_t *)info->fragment;

        VK_CHECK(vkCreateShaderModule(
            device->device,
            &module_create_info,
            NULL,
            &pipeline->graphics.fragment_shader));
    }

    return pipeline;
}

RgPipeline *rgComputePipelineCreate(RgDevice *device, const RgComputePipelineInfo *info)
{
    RgPipeline *pipeline = (RgPipeline *)malloc(sizeof(RgPipeline));
    memset(pipeline, 0, sizeof(*pipeline));

    pipeline->device = device;
    pipeline->type = RG_PIPELINE_TYPE_COMPUTE;

    assert(info->code && info->code_size > 0);

    //
    // Create pipeline layout
    //

    VkDescriptorSetLayout *vk_set_layouts =
        malloc(sizeof(*vk_set_layouts) * info->set_layout_count);

    for (uint32_t i = 0; i < info->set_layout_count; ++i)
    {
        vk_set_layouts[i] = info->set_layouts[i].set_layout;
    }

    VkPipelineLayoutCreateInfo pipeline_layout_info;
    memset(&pipeline_layout_info, 0, sizeof(pipeline_layout_info));
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = info->set_layout_count;
    pipeline_layout_info.pSetLayouts = vk_set_layouts;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = NULL;

    VK_CHECK(vkCreatePipelineLayout(
        device->device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout));

    free(vk_set_layouts);

    VkShaderModuleCreateInfo module_create_info;
    memset(&module_create_info, 0, sizeof(module_create_info));
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.codeSize = info->code_size;
    module_create_info.pCode = (uint32_t *)info->code;

    VK_CHECK(vkCreateShaderModule(
        device->device, &module_create_info, NULL, &pipeline->compute.shader));

    VkPipelineShaderStageCreateInfo stage_create_info;
    memset(&stage_create_info, 0, sizeof(stage_create_info));

    stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_create_info.module = pipeline->compute.shader;
    stage_create_info.pName = info->entry;

    VkComputePipelineCreateInfo pipeline_create_info;
    memset(&pipeline_create_info, 0, sizeof(pipeline_create_info));

    pipeline_create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_create_info.stage = stage_create_info;
    pipeline_create_info.layout = pipeline->pipeline_layout;

    vkCreateComputePipelines(
        device->device,
        VK_NULL_HANDLE,
        1,
        &pipeline_create_info,
        NULL,
        &pipeline->compute.instance);

    return pipeline;
}

void rgPipelineDestroy(RgDevice *device, RgPipeline *pipeline)
{
    VK_CHECK(vkDeviceWaitIdle(device->device));

    vkDestroyPipelineLayout(device->device, pipeline->pipeline_layout, NULL);

    switch (pipeline->type)
    {
    case RG_PIPELINE_TYPE_GRAPHICS:
    {
        if (pipeline->graphics.vertex_entry)
        {
            free(pipeline->graphics.vertex_entry);
        }
        if (pipeline->graphics.fragment_entry)
        {
            free(pipeline->graphics.fragment_entry);
        }

        for (uint32_t i = 0; i < pipeline->graphics.instances.size; ++i)
        {
            if (pipeline->graphics.instances.hashes[i] != 0)
            {
                VkPipeline instance = VK_NULL_HANDLE;
                memcpy(
                    &instance,
                    &pipeline->graphics.instances.values[i],
                    sizeof(VkPipeline));
                assert(instance != VK_NULL_HANDLE);
                vkDestroyPipeline(device->device, instance, NULL);
            }
        }

        free(pipeline->graphics.vertex_attributes);

        if (pipeline->graphics.vertex_shader)
        {
            vkDestroyShaderModule(device->device, pipeline->graphics.vertex_shader, NULL);
        }

        if (pipeline->graphics.fragment_shader)
        {
            vkDestroyShaderModule(device->device, pipeline->graphics.fragment_shader, NULL);
        }

        rgHashmapDestroy(&pipeline->graphics.instances);
        break;
    }

    case RG_PIPELINE_TYPE_COMPUTE:
    {
        if (pipeline->compute.shader)
        {
            vkDestroyShaderModule(device->device, pipeline->compute.shader, NULL);
        }

        if (pipeline->compute.instance)
        {
            vkDestroyPipeline(device->device, pipeline->compute.instance, NULL);
        }
        break;
    }
    }

    free(pipeline);
}

static VkPipeline rgGraphicsPipelineGetInstance(
    RgDevice *device, RgPipeline *pipeline, RgRenderPass *render_pass)
{
    uint64_t *found = rgHashmapGet(&pipeline->graphics.instances, render_pass->hash);
    if (found)
    {
        VkPipeline instance;
        memcpy(&instance, found, sizeof(VkPipeline));
        return instance;
    }

    ARRAY_OF(VkPipelineShaderStageCreateInfo) stages = {0};

    assert(pipeline->type == RG_PIPELINE_TYPE_GRAPHICS);

    if (pipeline->graphics.vertex_shader)
    {
        VkPipelineShaderStageCreateInfo stage = {0};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage.module = pipeline->graphics.vertex_shader;
        stage.pName =
            pipeline->graphics.vertex_entry ?
            pipeline->graphics.vertex_entry : "main";
        arrPush(&stages, stage);
    }

    if (pipeline->graphics.fragment_shader)
    {
        VkPipelineShaderStageCreateInfo stage = {0};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage.module = pipeline->graphics.fragment_shader;
        stage.pName =
            pipeline->graphics.fragment_entry ?
            pipeline->graphics.fragment_entry : "main";
        arrPush(&stages, stage);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {0};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription vertex_binding = {0};

    VkVertexInputAttributeDescription *attributes =
        malloc(sizeof(*attributes) * pipeline->graphics.num_vertex_attributes);
    memset(attributes, 0, sizeof(*attributes) * pipeline->graphics.num_vertex_attributes);

    if (pipeline->graphics.vertex_stride > 0)
    {
        assert(pipeline->graphics.num_vertex_attributes > 0);

        vertex_binding.binding = 0;
        vertex_binding.stride = pipeline->graphics.vertex_stride;

        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &vertex_binding;

        for (uint32_t i = 0; i < pipeline->graphics.num_vertex_attributes; ++i)
        {
            attributes[i].binding = 0;
            attributes[i].location = i;
            attributes[i].format =
                rgFormatToVk(pipeline->graphics.vertex_attributes[i].format);
            attributes[i].offset = pipeline->graphics.vertex_attributes[i].offset;
        }

        vertex_input_info.vertexAttributeDescriptionCount =
            pipeline->graphics.num_vertex_attributes;
        vertex_input_info.pVertexAttributeDescriptions = attributes;
    }

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = rgPrimitiveTopologyToVk(pipeline->graphics.topology);
    input_assembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = NULL; // No need for this, using dynamic state
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = NULL; // No need for this, using dynamic state

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = rgPolygonModeToVk(pipeline->graphics.polygon_mode);
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = rgCullModeToVk(pipeline->graphics.cull_mode);
    rasterizer.frontFace = rgFrontFaceToVk(pipeline->graphics.front_face);
    rasterizer.depthBiasEnable = pipeline->graphics.depth_stencil.bias_enable;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 0.0f;          // Optional
    multisampling.pSampleMask = NULL;               // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = pipeline->graphics.depth_stencil.test_enable;
    depth_stencil.depthWriteEnable = pipeline->graphics.depth_stencil.write_enable;
    depth_stencil.depthCompareOp =
        rgCompareOpToVk(pipeline->graphics.depth_stencil.compare_op);

    VkPipelineColorBlendAttachmentState color_blend_attachment_enabled = {0};
    color_blend_attachment_enabled.blendEnable = VK_TRUE;
    color_blend_attachment_enabled.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_enabled.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_enabled.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_enabled.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_enabled.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_enabled.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_enabled.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment_disabled = {0};
    color_blend_attachment_disabled.blendEnable = VK_FALSE;
    color_blend_attachment_disabled.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_disabled.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_disabled.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_disabled.srcAlphaBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_disabled.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_disabled.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_disabled.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    uint32_t num_color_attachments = render_pass->color_attachment_count;

    VkPipelineColorBlendAttachmentState *blend_infos =
        malloc(sizeof(*blend_infos) * num_color_attachments);

    if (pipeline->graphics.blend.enable)
    {
        for (uint32_t i = 0; i < num_color_attachments; ++i)
        {
            blend_infos[i] = color_blend_attachment_enabled;
        }
    }
    else
    {
        for (uint32_t i = 0; i < num_color_attachments; ++i)
        {
            blend_infos[i] = color_blend_attachment_disabled;
        }
    }

    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY; // Optional
    color_blending.attachmentCount = num_color_attachments;
    color_blending.pAttachments = blend_infos;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = RG_STATIC_ARRAY_SIZE(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = (uint32_t)stages.len;
    pipeline_info.pStages = stages.ptr;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = pipeline->pipeline_layout;
    pipeline_info.renderPass = render_pass->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VkPipeline instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(
        device->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &instance));

    arrFree(&stages);
    free(attributes);
    free(blend_infos);

    uint64_t instance_id = 0;
    memcpy(&instance_id, &instance, sizeof(VkPipeline));
    rgHashmapSet(&pipeline->graphics.instances, render_pass->hash, instance_id);

    return instance;
}
// }}}

// Command buffers {{{
RgCmdPool *rgCmdPoolCreate(RgDevice *device, RgQueueType type)
{
    RgCmdPool *cmd_pool = malloc(sizeof(*cmd_pool));
    memset(cmd_pool, 0, sizeof(*cmd_pool));

    cmd_pool->queue_type = type;

    VkCommandPoolCreateInfo cmd_pool_info = {0};
    cmd_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmd_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    switch (type)
    {
    case RG_QUEUE_TYPE_GRAPHICS:
        cmd_pool_info.queueFamilyIndex = device->graphics_queue_family_index;
        break;
    case RG_QUEUE_TYPE_COMPUTE:
        cmd_pool_info.queueFamilyIndex = device->compute_queue_family_index;
        break;
    case RG_QUEUE_TYPE_TRANSFER:
        cmd_pool_info.queueFamilyIndex = device->transfer_queue_family_index;
        break;
    }

    VK_CHECK(vkCreateCommandPool(
                device->device,
                &cmd_pool_info,
                NULL,
                &cmd_pool->cmd_pool));

    return cmd_pool;
}

void rgCmdPoolDestroy(RgDevice *device, RgCmdPool *cmd_pool)
{
    vkDestroyCommandPool(device->device, cmd_pool->cmd_pool, NULL);
    free(cmd_pool);
}

RgCmdBuffer *rgCmdBufferCreate(RgDevice *device, RgCmdPool *cmd_pool)
{
    RgCmdBuffer *cmd_buffer = malloc(sizeof(*cmd_buffer));
    memset(cmd_buffer, 0, sizeof(*cmd_buffer));

    cmd_buffer->device = device;

    switch (cmd_pool->queue_type)
    {
    case RG_QUEUE_TYPE_GRAPHICS:
        cmd_buffer->queue = device->graphics_queue;
        break;
    case RG_QUEUE_TYPE_COMPUTE:
        cmd_buffer->queue = device->compute_queue;
        break;
    case RG_QUEUE_TYPE_TRANSFER:
        cmd_buffer->queue = device->transfer_queue;
        break;
    }

    VkCommandBufferAllocateInfo cmd_buf_allocate_info = {0};
    cmd_buf_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buf_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buf_allocate_info.commandPool = cmd_pool->cmd_pool;
    cmd_buf_allocate_info.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(
                device->device,
                &cmd_buf_allocate_info,
                &cmd_buffer->cmd_buffer));

    VkSemaphoreCreateInfo semaphore_create_info = {0};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(
                device->device, 
                &semaphore_create_info,
                NULL,
                &cmd_buffer->semaphore));

    VkFenceCreateInfo fence_create_info = {0};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    /* fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; */

    VK_CHECK(vkCreateFence(
                device->device, 
                &fence_create_info,
                NULL,
                &cmd_buffer->fence));

    return cmd_buffer;
}

void rgCmdBufferDestroy(RgDevice *device, RgCmdPool *cmd_pool, RgCmdBuffer *cmd_buffer)
{
    VK_CHECK(vkDeviceWaitIdle(device->device));

    vkDestroyFence(device->device, cmd_buffer->fence, NULL);
    vkDestroySemaphore(device->device, cmd_buffer->semaphore, NULL);
    vkFreeCommandBuffers(device->device, cmd_pool->cmd_pool, 1, &cmd_buffer->cmd_buffer);
    arrFree(&cmd_buffer->wait_semaphores);
    arrFree(&cmd_buffer->wait_stages);
    free(cmd_buffer);
}

void rgCmdBufferBegin(RgCmdBuffer *cmd_buffer)
{
    VkCommandBufferBeginInfo cmd_buf_info = {0};
    cmd_buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buf_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd_buffer->cmd_buffer, &cmd_buf_info));
}

void rgCmdBufferEnd(RgCmdBuffer *cmd_buffer)
{
    if (cmd_buffer->current_render_pass)
    {
        vkCmdEndRenderPass(cmd_buffer->cmd_buffer);
    }

    cmd_buffer->current_render_pass = NULL;

    VK_CHECK(vkEndCommandBuffer(cmd_buffer->cmd_buffer));
}

void rgCmdBufferWaitForPresent(RgCmdBuffer *cmd_buffer, RgSwapchain *swapchain)
{
    arrPush(&cmd_buffer->wait_semaphores,
            swapchain->present_complete_semaphores[swapchain->current_semaphore_index]);
    arrPush(&cmd_buffer->wait_stages, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
}

void rgCmdBufferWaitForCommands(RgCmdBuffer *cmd_buffer, RgCmdBuffer *wait_cmd_buffer)
{
    arrPush(&cmd_buffer->wait_semaphores, wait_cmd_buffer->semaphore);
    arrPush(&cmd_buffer->wait_stages, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void rgCmdBufferSubmit(RgCmdBuffer *cmd_buffer)
{
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    assert(cmd_buffer->wait_semaphores.len == cmd_buffer->wait_stages.len);
    submit_info.waitSemaphoreCount = (uint32_t)cmd_buffer->wait_semaphores.len;
    submit_info.pWaitDstStageMask = cmd_buffer->wait_stages.ptr;
    submit_info.pWaitSemaphores = cmd_buffer->wait_semaphores.ptr;

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &cmd_buffer->semaphore;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buffer->cmd_buffer;
    VK_CHECK(vkQueueSubmit(
                cmd_buffer->queue,
                1,
                &submit_info,
                cmd_buffer->fence));

    // Reset waits
    cmd_buffer->wait_stages.len = 0;
    cmd_buffer->wait_semaphores.len = 0;
}

void rgCmdBindPipeline(RgCmdBuffer *cmd_buffer, RgPipeline *pipeline)
{
    cmd_buffer->current_pipeline = pipeline;

    switch (pipeline->type)
    {
    case RG_PIPELINE_TYPE_GRAPHICS:
    {
        cmd_buffer->current_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VkPipeline vk_pipeline = rgGraphicsPipelineGetInstance(
                cmd_buffer->device,
                pipeline,
                cmd_buffer->current_render_pass);
        vkCmdBindPipeline(
            cmd_buffer->cmd_buffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk_pipeline);
        break;
    }
    case RG_PIPELINE_TYPE_COMPUTE:
    {
        cmd_buffer->current_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        vkCmdBindPipeline(
            cmd_buffer->cmd_buffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            pipeline->compute.instance);
        break;
    }
    }
}

void rgCmdBindDescriptorSet(
        RgCmdBuffer *cmd_buffer,
        uint32_t index,
        RgDescriptorSet *set,
        uint32_t dynamic_offset_count,
        uint32_t *dynamic_offsets)
{
    vkCmdBindDescriptorSets(
        cmd_buffer->cmd_buffer,
        cmd_buffer->current_bind_point,
        cmd_buffer->current_pipeline->pipeline_layout,
        index,
        1,
        &set->set,
        dynamic_offset_count,
        dynamic_offsets);
}

void rgCmdSetRenderPass(
        RgCmdBuffer *cmd_buffer,
        RgRenderPass *render_pass,
        uint32_t clear_value_count,
        RgClearValue *clear_values)
{
    if (cmd_buffer->current_render_pass)
    {
        vkCmdEndRenderPass(cmd_buffer->cmd_buffer);
    }

    cmd_buffer->current_render_pass = render_pass;

    VkRenderPassBeginInfo render_pass_begin_info = {0};
    render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_begin_info.pNext = NULL;
    render_pass_begin_info.renderPass = render_pass->render_pass;
    render_pass_begin_info.renderArea.offset.x = 0;
    render_pass_begin_info.renderArea.offset.y = 0;
    render_pass_begin_info.renderArea.extent.width = render_pass->width;
    render_pass_begin_info.renderArea.extent.height = render_pass->height;
    render_pass_begin_info.clearValueCount = clear_value_count;
    render_pass_begin_info.pClearValues = (VkClearValue*)clear_values;
    render_pass_begin_info.framebuffer =
        render_pass->framebuffers[render_pass->current_framebuffer];

    vkCmdBeginRenderPass(
            cmd_buffer->cmd_buffer,
            &render_pass_begin_info,
            VK_SUBPASS_CONTENTS_INLINE);

    // Update dynamic viewport state
    VkViewport viewport = {0};
    viewport.width = (float)render_pass->width;
    viewport.height = (float)render_pass->height;
    viewport.minDepth = (float)0.0f;
    viewport.maxDepth = (float)1.0f;
    vkCmdSetViewport(cmd_buffer->cmd_buffer, 0, 1, &viewport);

    // Update dynamic scissor state
    VkRect2D scissor = {0};
    scissor.extent.width = render_pass->width;
    scissor.extent.height = render_pass->height;
    vkCmdSetScissor(cmd_buffer->cmd_buffer, 0, 1, &scissor);
}

void rgCmdBindVertexBuffer(
        RgCmdBuffer *cmd_buffer,
        RgBuffer *vertex_buffer,
        size_t offset)
{
    vkCmdBindVertexBuffers(
            cmd_buffer->cmd_buffer,
            0,
            1,
            &vertex_buffer->buffer,
            &offset);
}

void rgCmdBindIndexBuffer(
        RgCmdBuffer *cmd_buffer,
        RgBuffer *index_buffer,
        size_t offset,
        RgIndexType index_type)
{
    vkCmdBindIndexBuffer(
            cmd_buffer->cmd_buffer,
            index_buffer->buffer,
            offset,
            rgIndexTypeToVk(index_type));
}

void rgCmdDraw(
        RgCmdBuffer *cmd_buffer,
        uint32_t vertex_count,
        uint32_t instance_count,
        uint32_t first_vertex,
        uint32_t first_instance)
{
    vkCmdDraw(
            cmd_buffer->cmd_buffer,
            vertex_count,
            instance_count,
            first_vertex,
            first_instance);
}

void rgCmdDrawIndexed(
        RgCmdBuffer *cmd_buffer,
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t  vertex_offset,
        uint32_t first_instance)
{
    vkCmdDrawIndexed(
            cmd_buffer->cmd_buffer,
            index_count,
            instance_count,
            first_index,
            vertex_offset,
            first_instance);
}

void rgCmdDispatch(
        RgCmdBuffer *cmd_buffer,
        uint32_t group_count_x,
        uint32_t group_count_y,
        uint32_t group_count_z)
{
    vkCmdDispatch(
            cmd_buffer->cmd_buffer,
            group_count_x,
            group_count_y,
            group_count_z);
}
// }}}

