#ifndef RG_H
#define RG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RgDevice RgDevice;
typedef struct RgSwapchain RgSwapchain;
typedef struct RgPipeline RgPipeline;
typedef struct RgBuffer RgBuffer;
typedef struct RgImage RgImage;
typedef struct RgSampler RgSampler;
typedef struct RgCmdPool RgCmdPool;
typedef struct RgCmdBuffer RgCmdBuffer;
typedef struct RgRenderPass RgRenderPass;
typedef struct RgDescriptorSetLayout RgDescriptorSetLayout;
typedef struct RgDescriptorSet RgDescriptorSet;
typedef struct RgPipelineLayout RgPipelineLayout;
typedef uint32_t RgFlags;

typedef struct RgLimits
{
    uint32_t max_bound_descriptor_sets;
    size_t min_texel_buffer_offset_alignment;
    size_t min_uniform_buffer_offset_alignment;
    size_t min_storage_buffer_offset_alignment;
} RgLimits;

typedef enum RgQueueType
{
    RG_QUEUE_TYPE_GRAPHICS,
    RG_QUEUE_TYPE_COMPUTE,
    RG_QUEUE_TYPE_TRANSFER,
} RgQueueType;

typedef enum RgFormat
{
    RG_FORMAT_UNDEFINED = 0,

    RG_FORMAT_R8_UNORM = 1,
    RG_FORMAT_RG8_UNORM = 2,
    RG_FORMAT_RGB8_UNORM = 3,
    RG_FORMAT_RGBA8_UNORM = 4,

    RG_FORMAT_R8_UINT = 5,
    RG_FORMAT_RG8_UINT = 6,
    RG_FORMAT_RGB8_UINT = 7,
    RG_FORMAT_RGBA8_UINT = 8,

    RG_FORMAT_R16_UINT = 9,
    RG_FORMAT_RG16_UINT = 10,
    RG_FORMAT_RGB16_UINT = 11,
    RG_FORMAT_RGBA16_UINT = 12,

    RG_FORMAT_R32_UINT = 13,
    RG_FORMAT_RG32_UINT = 14,
    RG_FORMAT_RGB32_UINT = 15,
    RG_FORMAT_RGBA32_UINT = 16,

    RG_FORMAT_R32_SFLOAT = 17,
    RG_FORMAT_RG32_SFLOAT = 18,
    RG_FORMAT_RGB32_SFLOAT = 19,
    RG_FORMAT_RGBA32_SFLOAT = 20,

    RG_FORMAT_BGRA8_UNORM = 21,
    RG_FORMAT_BGRA8_SRGB = 22,

    RG_FORMAT_R16_SFLOAT = 23,
    RG_FORMAT_RG16_SFLOAT = 24,
    RG_FORMAT_RGBA16_SFLOAT = 25,

    RG_FORMAT_D16_UNORM = 26,
    RG_FORMAT_D32_SFLOAT = 27,
    RG_FORMAT_D16_UNORM_S8_UINT = 28,
    RG_FORMAT_D24_UNORM_S8_UINT = 29,
    RG_FORMAT_D32_SFLOAT_S8_UINT = 30,

    RG_FORMAT_BC7_UNORM = 31,
    RG_FORMAT_BC7_SRGB = 32,
} RgFormat;

typedef enum RgImageUsage
{
	RG_IMAGE_USAGE_SAMPLED                  = 1 << 0,
	RG_IMAGE_USAGE_TRANSFER_DST             = 1 << 1,
	RG_IMAGE_USAGE_TRANSFER_SRC             = 1 << 2,
	RG_IMAGE_USAGE_STORAGE                  = 1 << 3,
	RG_IMAGE_USAGE_COLOR_ATTACHMENT         = 1 << 4,
	RG_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT = 1 << 5,
} RgImageUsage;

typedef enum RgImageAspect
{
	RG_IMAGE_ASPECT_COLOR   = 1 << 0,
	RG_IMAGE_ASPECT_DEPTH   = 1 << 1,
	RG_IMAGE_ASPECT_STENCIL = 1 << 2,
} RgImageAspect;

typedef enum RgFilter
{
	RG_FILTER_LINEAR = 0,
	RG_FILTER_NEAREST = 1,
} RgFilter;

typedef enum RgSamplerAddressMode
{
    RG_SAMPLER_ADDRESS_MODE_REPEAT = 0,
    RG_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT = 1,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE = 2,
    RG_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER = 3,
    RG_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE= 4,
} RgSamplerAddressMode;

typedef enum RgBorderColor
{
    RG_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK = 0,
    RG_BORDER_COLOR_INT_TRANSPARENT_BLACK = 1,
    RG_BORDER_COLOR_FLOAT_OPAQUE_BLACK = 2,
    RG_BORDER_COLOR_INT_OPAQUE_BLACK = 3,
    RG_BORDER_COLOR_FLOAT_OPAQUE_WHITE = 4,
    RG_BORDER_COLOR_INT_OPAQUE_WHITE = 5,
} RgBorderColor;

typedef struct RgOffset3D
{
    int32_t x, y, z;
} RgOffset3D;

typedef struct RgExtent3D
{
    uint32_t width, height, depth;
} RgExtent3D;

typedef struct RgOffset2D
{
    int32_t x, y;
} RgOffset2D;

typedef struct RgExtent2D
{
    uint32_t width, height;
} RgExtent2D;

typedef struct RgRect2D
{
    RgOffset2D offset;
    RgExtent2D extent;
} RgRect2D;

typedef struct RgViewport
{
    float x;
    float y;
    float width;
    float height;
    float min_depth;
    float max_depth;
} RgViewport;

typedef struct RgClearDepthStencilValue {
    float    depth;
    uint32_t stencil;
} RgClearDepthStencilValue;

typedef union RgClearColorValue {
    float    float32[4];
    int32_t  int32[4];
    uint32_t uint32[4];
} RgClearColorValue;

typedef union RgClearValue {
    RgClearColorValue        color;
    RgClearDepthStencilValue depth_stencil;
} RgClearValue;

typedef struct RgDeviceInfo
{
    bool enable_validation;
} RgDeviceInfo;

typedef struct RgImageInfo
{
    RgExtent3D extent;
	RgFormat format;
	RgFlags usage;
	RgFlags aspect;
	uint32_t sample_count;
	uint32_t mip_count;
	uint32_t layer_count;
} RgImageInfo;

typedef struct RgSamplerInfo
{
    bool anisotropy;
    float max_anisotropy;
    float min_lod;
    float max_lod;
    RgFilter mag_filter;
    RgFilter min_filter;
    RgSamplerAddressMode address_mode;
    RgBorderColor border_color;
} RgSamplerInfo;

typedef enum RgBufferUsage
{
	RG_BUFFER_USAGE_VERTEX       = 1 << 0,
	RG_BUFFER_USAGE_INDEX        = 1 << 1,
	RG_BUFFER_USAGE_UNIFORM      = 1 << 2,
	RG_BUFFER_USAGE_TRANSFER_SRC = 1 << 3,
	RG_BUFFER_USAGE_TRANSFER_DST = 1 << 4,
	RG_BUFFER_USAGE_STORAGE      = 1 << 5,
} RgBufferUsage;

typedef enum RgBufferMemory
{
	RG_BUFFER_MEMORY_HOST = 1,
	RG_BUFFER_MEMORY_DEVICE,
} RgBufferMemory;

typedef struct RgBufferInfo
{
	size_t size;
	RgFlags usage;
	RgBufferMemory memory;
} RgBufferInfo;

typedef struct RgSwapchainInfo
{
    void *display_handle;
    void *window_handle;
    RgSwapchain *old_swapchain;
    bool vsync;
    RgFormat depth_format; // RG_FORMAT_UNDEFINED for no depth buffer

    uint32_t width;
    uint32_t height;
} RgSwapchainInfo;

typedef struct RgRenderPassInfo
{
    RgImage **color_attachments;
    uint32_t color_attachment_count;

    RgImage *depth_stencil_attachment;
} RgRenderPassInfo;

typedef enum RgDescriptorType
{
    RG_DESCRIPTOR_UNIFORM_BUFFER         = 1,
    RG_DESCRIPTOR_UNIFORM_BUFFER_DYNAMIC = 2,
    RG_DESCRIPTOR_STORAGE_BUFFER         = 3,
    RG_DESCRIPTOR_STORAGE_BUFFER_DYNAMIC = 4,
    RG_DESCRIPTOR_IMAGE                  = 5,
    RG_DESCRIPTOR_SAMPLER                = 6,
    RG_DESCRIPTOR_IMAGE_SAMPLER          = 7,
} RgDescriptorType;

typedef enum RgShaderStage
{
    RG_SHADER_STAGE_FRAGMENT     = 1 << 0,
    RG_SHADER_STAGE_VERTEX       = 1 << 1,
    RG_SHADER_STAGE_COMPUTE      = 1 << 2,
    RG_SHADER_STAGE_ALL_GRAPHICS = RG_SHADER_STAGE_FRAGMENT | RG_SHADER_STAGE_VERTEX,
    RG_SHADER_STAGE_ALL          = 0x7FFFFFFF,
} RgShaderStage;

typedef struct RgDescriptorSetLayoutEntry
{
    uint32_t binding;
    RgDescriptorType type;
    RgFlags shader_stages;
    uint32_t count;
} RgDescriptorSetLayoutEntry;

typedef struct RgDescriptorSetLayoutInfo
{
    RgDescriptorSetLayoutEntry *entries;
    uint32_t entry_count;
} RgDescriptorSetLayoutInfo;

typedef struct RgPipelineLayoutInfo
{
    RgDescriptorSetLayout **set_layouts;
    uint32_t set_layout_count;
} RgPipelineLayoutInfo;

typedef struct RgDescriptorSetEntry
{
    uint32_t binding;
    uint32_t descriptor_count;

    RgBuffer *buffer;
    size_t size;
    size_t offset;

    RgImage *image;
    RgSampler *sampler;
} RgDescriptorSetEntry;

typedef enum RgIndexType
{
    RG_INDEX_TYPE_UINT32 = 0,
    RG_INDEX_TYPE_UINT16 = 1,
} RgIndexType;

typedef enum RgPolygonMode
{
    RG_POLYGON_MODE_FILL  = 0,
    RG_POLYGON_MODE_LINE  = 1,
    RG_POLYGON_MODE_POINT = 2,
} RgPolygonMode;

typedef enum RgPrimitiveTopology
{
    RG_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST = 0,
    RG_PRIMITIVE_TOPOLOGY_LINE_LIST     = 1,
} RgPrimitiveTopology;

typedef enum RgFrontFace
{
    RG_FRONT_FACE_CLOCKWISE         = 0,
    RG_FRONT_FACE_COUNTER_CLOCKWISE = 1,
} RgFrontFace;

typedef enum RgCullMode
{
    RG_CULL_MODE_NONE           = 0,
    RG_CULL_MODE_BACK           = 1,
    RG_CULL_MODE_FRONT          = 2,
    RG_CULL_MODE_FRONT_AND_BACK = 3,
} RgCullMode;

typedef struct RgVertexAttribute
{
    RgFormat format;
    uint32_t offset;
} RgVertexAttribute;

typedef struct RgPipelineBlendState
{
    bool enable;
} RgPipelineBlendState;

typedef enum RgCompareOp
{
    RG_COMPARE_OP_NEVER = 0,
    RG_COMPARE_OP_LESS = 1,
    RG_COMPARE_OP_EQUAL = 2,
    RG_COMPARE_OP_LESS_OR_EQUAL = 3,
    RG_COMPARE_OP_GREATER = 4,
    RG_COMPARE_OP_NOT_EQUAL = 5,
    RG_COMPARE_OP_GREATER_OR_EQUAL = 6,
    RG_COMPARE_OP_ALWAYS = 7,
} RgCompareOp;

typedef struct RgPipelineDepthStencilState
{
    bool test_enable;
    bool write_enable;
    bool bias_enable;
    RgCompareOp compare_op;
} RgPipelineDepthStencilState;

typedef struct RgGraphicsPipelineInfo
{
    RgPolygonMode       polygon_mode;
    RgCullMode          cull_mode;
    RgFrontFace         front_face;
    RgPrimitiveTopology topology;

    RgPipelineBlendState        blend;
    RgPipelineDepthStencilState depth_stencil;

    uint32_t           vertex_stride;
    uint32_t           num_vertex_attributes;
    RgVertexAttribute *vertex_attributes;

    RgPipelineLayout *pipeline_layout;

    const uint8_t *vertex;
    size_t         vertex_size;
    const char    *vertex_entry;

    const uint8_t *fragment;
    size_t         fragment_size;
    const char    *fragment_entry;
} RgGraphicsPipelineInfo;

typedef struct RgComputePipelineInfo
{
    RgPipelineLayout *pipeline_layout;

    const uint8_t *code;
    size_t         code_size;
    const char    *entry;
} RgComputePipelineInfo;

typedef struct RgImageCopy
{
    RgImage   *image;
    uint32_t   mip_level;
    uint32_t   array_layer;
    RgOffset3D offset;
} RgImageCopy;

typedef struct RgBufferCopy
{
    RgBuffer *buffer;
    size_t    offset;
    uint32_t  row_length;
    uint32_t  image_height;
} RgBufferCopy;

typedef struct RgImageRegion
{
    uint32_t base_mip_level;
    uint32_t mip_count;
    uint32_t base_array_layer;
    uint32_t layer_count;
} RgImageRegion;

RgDevice *rgDeviceCreate(const RgDeviceInfo *info);
void rgDeviceDestroy(RgDevice *device);
void rgDeviceGetLimits(RgDevice *device, RgLimits *limits);

RgBuffer *rgBufferCreate(RgDevice *device, const RgBufferInfo *info);
void rgBufferDestroy(RgDevice *device, RgBuffer *buffer);
void *rgBufferMap(RgDevice *device, RgBuffer *buffer);
void rgBufferUnmap(RgDevice *device, RgBuffer *buffer);
void rgBufferUpload(
        RgDevice *device,
        RgCmdPool *cmd_pool,
        RgBuffer *buffer,
        size_t offset,
        size_t size,
        void *data);

RgImage *rgImageCreate(RgDevice *device, const RgImageInfo *info);
void rgImageDestroy(RgDevice *device, RgImage *image);
void rgImageUpload(
    RgDevice *device,
    RgCmdPool *cmd_pool,
    RgImageCopy *dst,
    RgExtent3D *extent,
    size_t size,
    void *data);

RgSampler *rgSamplerCreate(RgDevice *device, RgSamplerInfo *info);
void rgSamplerDestroy(RgDevice *device, RgSampler *sampler);

RgSwapchain *rgSwapchainCreate(RgDevice *device, const RgSwapchainInfo *info);
void rgSwapchainDestroy(RgDevice *device, RgSwapchain *swapchain);
RgRenderPass *rgSwapchainGetRenderPass(RgSwapchain *swapchain);
void rgSwapchainWaitForCommands(RgSwapchain *swapchain, RgCmdBuffer *wait_cmd_buffer);
void rgSwapchainAcquireImage(RgSwapchain *swapchain);
void rgSwapchainPresent(RgSwapchain *swapchain);

RgRenderPass *rgRenderPassCreate(RgDevice *device, const RgRenderPassInfo *info);
void rgRenderPassDestroy(RgDevice *device, RgRenderPass *render_pass);

RgDescriptorSetLayout *rgDescriptorSetLayoutCreate(
        RgDevice *device, const RgDescriptorSetLayoutInfo *info);
void rgDescriptorSetLayoutDestroy(RgDevice *device, RgDescriptorSetLayout *set_layout);

RgPipelineLayout *rgPipelineLayoutCreate(
        RgDevice *device, const RgPipelineLayoutInfo *info);
void rgPipelineLayoutDestroy(RgDevice *device, RgPipelineLayout *pipeline_layout);

RgDescriptorSet *rgDescriptorSetCreate(RgDevice *device, RgDescriptorSetLayout *set_layout);
void rgDescriptorSetUpdate(
    RgDevice *device, 
    RgDescriptorSet *descriptorset,
    const RgDescriptorSetEntry *entries,
    uint32_t entry_count);
void rgDescriptorSetDestroy(RgDevice *device, RgDescriptorSet *descriptor_set);

RgPipeline *rgGraphicsPipelineCreate(RgDevice *device, const RgGraphicsPipelineInfo *info);
RgPipeline *rgComputePipelineCreate(RgDevice *device, const RgComputePipelineInfo *info);
void rgPipelineDestroy(RgDevice *device, RgPipeline *pipeline);

RgCmdPool *rgCmdPoolCreate(RgDevice *device, RgQueueType type);
void rgCmdPoolDestroy(RgDevice *device, RgCmdPool *cmd_pool);
RgCmdBuffer *rgCmdBufferCreate(RgDevice *device, RgCmdPool *cmd_pool);
void rgCmdBufferDestroy(RgDevice *device, RgCmdPool *cmd_pool, RgCmdBuffer *cmd_buffer);
void rgCmdBufferBegin(RgCmdBuffer *cmd_buffer);
void rgCmdBufferEnd(RgCmdBuffer *cmd_buffer);
void rgCmdBufferWaitForPresent(RgCmdBuffer *cmd_buffer, RgSwapchain *swapchain);
void rgCmdBufferWaitForCommands(RgCmdBuffer *cmd_buffer, RgCmdBuffer *wait_cmd_buffer);
void rgCmdBufferWait(RgDevice *device, RgCmdBuffer *cmd_buffer);
void rgCmdBufferSubmit(RgCmdBuffer *cmd_buffer);

void rgCmdBindPipeline(RgCmdBuffer *cmd_buffer, RgPipeline *pipeline);
void rgCmdBindDescriptorSet(
        RgCmdBuffer *cmd_buffer,
        uint32_t index,
        RgDescriptorSet *set,
        uint32_t dynamic_offset_count,
        uint32_t *dynamic_offsets);
void rgCmdSetRenderPass(
        RgCmdBuffer *cmd_buffer,
        RgRenderPass *render_pass,
        uint32_t clear_value_count,
        RgClearValue *clear_values);
void rgCmdBindVertexBuffer(
        RgCmdBuffer *cmd_buffer,
        RgBuffer *vertex_buffer,
        size_t offset);
void rgCmdBindIndexBuffer(
        RgCmdBuffer *cmd_buffer,
        RgBuffer *index_buffer,
        size_t offset,
        RgIndexType index_type);
void rgCmdDraw(
        RgCmdBuffer *cmd_buffer,
        uint32_t vertex_count,
        uint32_t instance_count,
        uint32_t first_vertex,
        uint32_t first_instance);
void rgCmdDrawIndexed(
        RgCmdBuffer *cmd_buffer,
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t  vertex_offset,
        uint32_t first_instance);
void rgCmdDispatch(
        RgCmdBuffer *cmd_buffer,
        uint32_t group_count_x,
        uint32_t group_count_y,
        uint32_t group_count_z);

#ifdef __cplusplus
}
#endif

#endif // RG_H
