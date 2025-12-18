#include "pone_arena.h"
#include "pone_assert.h"
#include "pone_atomic.h"
#include "pone_gltf.h"
#include "pone_json.h"
#include "pone_math.h"
#include "pone_memory.h"
#include "pone_platform.h"
#include "pone_truetype.h"
#include "pone_types.h"
#include "pone_vulkan.h"
#include "pone_work_queue.h"

#include <math.h>
#include <stdio.h>

#define FRAME_OVERLAP 2

#define vk_get_instance_proc_addr(instance, fn)                                \
    PFN_##fn fn = (PFN_##fn)_vk_get_instance_proc_addr(instance, #fn);         \
    pone_assert(fn)

#define vk_get_device_proc_addr(device, fn)                                    \
    PFN_##fn fn = (PFN_##fn)vkGetDeviceProcAddr(device, #fn);                  \
    pone_assert(fn)

#define vk_check(res) pone_assert((res) == VK_SUCCESS)

#define PI_F32 3.14159265358979323846264338327950288f

struct VulkanFnDispatcher {
    VkInstance instance;
    VkDevice device;
    PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr;
    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr;
};

struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
};

struct Vec4 {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

struct Mat4 {
    f32 data[16];
};

Mat4 pone_mat4_zero() {
    return (Mat4){.data = {
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                  }};
}

Mat4 pone_mat4_identity() {
    return (Mat4){.data = {
                      1.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      1.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      1.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      0.0f,
                      1.0f,
                  }};
}

f32 *pone_mat4_get(Mat4 *m, usize i, usize j) { return &m->data[j * 4 + i]; }

void pone_mat4_set(Mat4 *m, usize i, usize j, f32 v) { m->data[j * 4 + i] = v; }

inline f32 pone_radians(f32 degrees) { return degrees * PI_F32 / 180.0f; }

Mat4 pone_mat4_perspective(f32 fov_y, f32 aspect, f32 z_near, f32 z_far) {
    Mat4 result = pone_mat4_zero();

    f32 rad = pone_radians(fov_y);
    f32 h = cosf(0.5f * rad) / sinf(0.5f * rad);
    f32 w = h / aspect;

    pone_mat4_set(&result, 0, 0, w);
    pone_mat4_set(&result, 1, 1, h);
    pone_mat4_set(&result, 2, 2, z_far / (z_far - z_near));
    pone_mat4_set(&result, 3, 2, -1.0f);
    pone_mat4_set(&result, 2, 3, -(z_far * z_near) / (z_far - z_near));

    return result;
}

Mat4 pone_mat4_mul(Mat4 *lhs, Mat4 *rhs) {
    Mat4 result = pone_mat4_zero();
    for (usize i = 0; i < 4; i++) {
        for (usize j = 0; j < 4; j++) {
            f32 *sum = pone_mat4_get(&result, i, j);
            for (usize k = 0; k < 4; k++) {
                f32 l = *pone_mat4_get(lhs, i, k);
                f32 r = *pone_mat4_get(rhs, k, j);
                *sum += l * r;
            }
        }
    }

    return result;
}

struct Vertex {
    Vec3 position;
    f32 uv_x;
    Vec3 normal;
    f32 uv_y;
    Vec4 color;
};

struct Mesh {
    u16 *indices;
    usize index_count;

    Vertex *vertices;
    usize vertex_count;
};

struct ComputePushConstants {
    Vec4 data1;
    Vec4 data2;
    Vec4 data3;
    Vec4 data4;
};

struct DrawPushConstants {
    Mat4 world_matrix;
    VkDeviceAddress vertex_buffer_addr;
};

struct PipelineBuilder {
    usize shader_stages_count;
    VkPipelineShaderStageCreateInfo *shader_stages;
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
    VkPipelineRasterizationStateCreateInfo rasterization_state;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineMultisampleStateCreateInfo multisample_state;
    VkPipelineLayout layout;
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
    VkPipelineRenderingCreateInfo rendering;
    VkFormat color_attachment_format;
};

void pipeline_builder_clear(PipelineBuilder *pipeline_builder) {
    pipeline_builder->input_assembly_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    pipeline_builder->rasterization_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    pipeline_builder->color_blend_attachment = {};
    pipeline_builder->multisample_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    pipeline_builder->layout = 0;
    pipeline_builder->depth_stencil_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    pipeline_builder->rendering = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipeline_builder->shader_stages = 0;
    pipeline_builder->shader_stages_count = 0;
}

void pipeline_builder_set_shaders(
    PipelineBuilder *pipeline_builder, usize shader_stage_count,
    VkPipelineShaderStageCreateInfo *shader_stages) {
    pipeline_builder->shader_stages_count = shader_stage_count;
    pipeline_builder->shader_stages = shader_stages;
}

void pipeline_builder_set_input_topology(PipelineBuilder *pipeline_builder,
                                         VkPrimitiveTopology topology) {
    pipeline_builder->input_assembly_state.topology = topology;
    pipeline_builder->input_assembly_state.primitiveRestartEnable = VK_FALSE;
}

void pipeline_builder_set_polygon_mode(PipelineBuilder *pipeline_builder,
                                       VkPolygonMode polygon_mode) {
    pipeline_builder->rasterization_state.polygonMode = polygon_mode;
    pipeline_builder->rasterization_state.lineWidth = 1.0f;
}

void pipeline_builder_set_cull_mode(PipelineBuilder *pipeline_builder,
                                    VkCullModeFlags cull_mode,
                                    VkFrontFace front_face) {
    pipeline_builder->rasterization_state.cullMode = cull_mode;
    pipeline_builder->rasterization_state.frontFace = front_face;
}

void pipeline_builder_set_multisampling_none(
    PipelineBuilder *pipeline_builder) {
    pipeline_builder->multisample_state.rasterizationSamples =
        VK_SAMPLE_COUNT_1_BIT;
    pipeline_builder->multisample_state.sampleShadingEnable = VK_FALSE;
    pipeline_builder->multisample_state.minSampleShading = 1.0f;
    pipeline_builder->multisample_state.pSampleMask = 0;
    pipeline_builder->multisample_state.alphaToCoverageEnable = VK_FALSE;
    pipeline_builder->multisample_state.alphaToOneEnable = VK_FALSE;
}

void pipeline_builder_disable_blending(PipelineBuilder *pipeline_builder) {
    pipeline_builder->color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipeline_builder->color_blend_attachment.blendEnable = VK_FALSE;
}

void pipeline_builder_set_color_attachment_format(
    PipelineBuilder *pipeline_builder, VkFormat format) {
    pipeline_builder->color_attachment_format = format;
    pipeline_builder->rendering.colorAttachmentCount = 1;
    pipeline_builder->rendering.pColorAttachmentFormats =
        &pipeline_builder->color_attachment_format;
}

void pipeline_builder_set_depth_format(PipelineBuilder *pipeline_builder,
                                       VkFormat format) {
    pipeline_builder->rendering.depthAttachmentFormat = format;
}

void pipeline_builder_disable_depth_test(PipelineBuilder *pipeline_builder) {
    pipeline_builder->depth_stencil_state.depthTestEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.depthWriteEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.depthCompareOp = VK_COMPARE_OP_NEVER;
    pipeline_builder->depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.stencilTestEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.front = {};
    pipeline_builder->depth_stencil_state.back = {};
    pipeline_builder->depth_stencil_state.minDepthBounds = 0.0f;
    pipeline_builder->depth_stencil_state.maxDepthBounds = 1.0f;
}

void pipeline_builder_enable_depth_test(PipelineBuilder *pipeline_builder,
                                        b8 depth_write_enable, VkCompareOp op) {
    pipeline_builder->depth_stencil_state.depthTestEnable = VK_TRUE;
    pipeline_builder->depth_stencil_state.depthWriteEnable =
        (VkBool32)depth_write_enable;
    pipeline_builder->depth_stencil_state.depthCompareOp = op;
    pipeline_builder->depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.stencilTestEnable = VK_FALSE;
    pipeline_builder->depth_stencil_state.front = {};
    pipeline_builder->depth_stencil_state.back = {};
    pipeline_builder->depth_stencil_state.minDepthBounds = 0.0f;
    pipeline_builder->depth_stencil_state.maxDepthBounds = 1.0f;
}

VkPipeline pipeline_builder_build(
    PipelineBuilder *pipeline_builder,
    PFN_vkCreateGraphicsPipelines vk_create_graphics_pipelines,
    VkAllocationCallbacks *allocation_callbacks, VkDevice device) {
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = 0,
        .scissorCount = 1,
        .pScissors = 0,
    };

    VkPipelineColorBlendStateCreateInfo color_blend_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &pipeline_builder->color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = (void *)&pipeline_builder->rendering,
        .flags = 0,
        .stageCount = (uint32_t)pipeline_builder->shader_stages_count,
        .pStages = pipeline_builder->shader_stages,
        .pVertexInputState = &vertex_input_state,
        .pInputAssemblyState = &pipeline_builder->input_assembly_state,
        .pTessellationState = 0,
        .pViewportState = &viewport_state,
        .pRasterizationState = &pipeline_builder->rasterization_state,
        .pMultisampleState = &pipeline_builder->multisample_state,
        .pDepthStencilState = &pipeline_builder->depth_stencil_state,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_builder->layout,
        .renderPass = 0,
        .subpass = 0,
        .basePipelineHandle = 0,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline;
    vk_check(vk_create_graphics_pipelines(device, 0, 1,
                                          &graphics_pipeline_create_info,
                                          allocation_callbacks, &pipeline));

    return pipeline;
}

static i32
find_memory_type_index(u32 type_bits,
                       VkPhysicalDeviceMemoryProperties2 *memory_properties,
                       VkMemoryPropertyFlags properties, u32 *index) {
    for (u32 i = 0; i < memory_properties->memoryProperties.memoryTypeCount;
         i++) {
        if ((type_bits & (1 << i)) &&
            (memory_properties->memoryProperties.memoryTypes[i].propertyFlags &
             properties) == properties) {
            *index = i;
            return 1;
        }
    }

    return 0;
}

static b8 global_running = 1;

b8 string_eq(u8 *s1, u8 *s2) {
    u8 *c1 = s1;
    u8 *c2 = s2;

    for (; *c1 && *c2; c1++, c2++) {
        if (*c1 != *c2) {
            return 0;
        }
    }

    return *c1 == *c2;
}

u8 *string_copy(u8 *dst, u8 *src, usize len) {
    return (u8 *)pone_memcpy((void *)dst, (void *)src, len);
}

static VKAPI_PTR void *vk_allocation(void *user_data, size_t size,
                                     size_t alignment,
                                     VkSystemAllocationScope allocation_scope) {
    Arena *arena = (Arena *)user_data;
    void *p = arena_alloc(arena, size);

    return p;
}

static VKAPI_PTR void *
vk_reallocation(void *user_data, void *original, size_t size, size_t alignment,
                VkSystemAllocationScope allocation_scope) {
    Arena *arena = (Arena *)user_data;
    void *p = arena_realloc(arena, original, size);

    return p;
}

static VKAPI_PTR void vk_free(void *user_data, void *p) {}

static void transition_image(PoneVkCommandBuffer *cmd, VkImage image,
                             VkImageLayout curr_layout,
                             VkImageLayout new_layout) {
    VkImageAspectFlags aspect_mask =
        (new_layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageSubresourceRange image_subresourece_range;
    image_subresourece_range.aspectMask = aspect_mask;
    image_subresourece_range.baseMipLevel = 0;
    image_subresourece_range.levelCount = VK_REMAINING_MIP_LEVELS;
    image_subresourece_range.baseArrayLayer = 0;
    image_subresourece_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkImageMemoryBarrier2 image_barrier;
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    image_barrier.pNext = 0;
    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask =
        VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
    image_barrier.oldLayout = curr_layout;
    image_barrier.newLayout = new_layout;
    image_barrier.srcQueueFamilyIndex = 0;
    image_barrier.dstQueueFamilyIndex = 0;
    image_barrier.image = image;
    image_barrier.subresourceRange = image_subresourece_range;

    VkDependencyInfo dependency_info;
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.pNext = 0;
    dependency_info.dependencyFlags = 0;
    dependency_info.memoryBarrierCount = 0;
    dependency_info.pMemoryBarriers = 0;
    dependency_info.bufferMemoryBarrierCount = 0;
    dependency_info.pBufferMemoryBarriers = 0;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &image_barrier;

    (cmd->dispatch->vk_cmd_pipeline_barrier_2)(cmd->handle, &dependency_info);
}

void copy_image_to_image(VkCommandBuffer cmd,
                         PFN_vkCmdBlitImage2 vkCmdBlitImage2, VkImage src,
                         VkExtent2D src_extent, VkImage dst,
                         VkExtent2D dst_extent) {
    VkImageBlit2 blit = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
        .pNext = 0,
        .srcSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .srcOffsets =
            {
                (VkOffset3D){.x = 0, .y = 0, .z = 0},
                (VkOffset3D){.x = (int32_t)src_extent.width,
                             .y = (int32_t)src_extent.height,
                             .z = 1},
            },
        .dstSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .dstOffsets =
            {
                (VkOffset3D){.x = 0, .y = 0, .z = 0},
                (VkOffset3D){.x = (int32_t)dst_extent.width,
                             .y = (int32_t)dst_extent.height,
                             .z = 1},
            },
    };

    VkBlitImageInfo2 blit_image_info = {
        .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
        .pNext = 0,
        .srcImage = src,
        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .dstImage = dst,
        .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .regionCount = 1,
        .pRegions = &blit,
        .filter = VK_FILTER_LINEAR,
    };

    vkCmdBlitImage2(cmd, &blit_image_info);
}

static void imgui_check_vk_result(VkResult err) { vk_check(err); }

static PFN_vkVoidFunction imgui_vulkan_loader(const char *name,
                                              void *user_data) {
    VulkanFnDispatcher *dispatcher = (VulkanFnDispatcher *)user_data;

    PFN_vkVoidFunction fn;
    if (string_eq((u8 *)name, (u8 *)"vkDestroySurfaceKHR") ||
        string_eq((u8 *)name, (u8 *)"vkEnumeratePhysicalDevices") ||
        string_eq((u8 *)name, (u8 *)"vkGetPhysicalDeviceProperties") ||
        string_eq((u8 *)name, (u8 *)"vkGetPhysicalDeviceMemoryProperties") ||
        string_eq((u8 *)name,
                  (u8 *)"vkGetPhysicalDeviceQueueFamilyProperties") ||
        string_eq((u8 *)name,
                  (u8 *)"vkGetPhysicalDeviceSurfaceCapabilitiesKHR") ||
        string_eq((u8 *)name, (u8 *)"vkGetPhysicalDeviceSurfaceFormatsKHR") ||
        string_eq((u8 *)name,
                  (u8 *)"vkGetPhysicalDeviceSurfacePresentModesKHR")) {
        fn =
            (dispatcher->vk_get_instance_proc_addr)(dispatcher->instance, name);
    } else {
        fn = (dispatcher->vk_get_device_proc_addr)(dispatcher->device, name);
    }
    return fn;
}

struct PoneThreadInfo {
    usize thread_index;
    PoneWorkQueue *work_queue;
    volatile b8 *work_available;
};

struct PoneThreadWorkData {
    PoneThreadInfo *thread_info;
    void *user_data;
};

#if defined(PONE_PLATFORM_LINUX)
#include "xdg-shell-client-protocol.h"
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

struct PoneWayland {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_shm *shm;
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    u32 width;
    u32 height;
    u32 stride;
    u32 format;
    b8 closed;
    b8 resize_requested;
    b8 ready_to_resize;
    b8 configured_once;
};

static void pone_wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
    wl_buffer_destroy(wl_buffer);
}

static const struct wl_buffer_listener wl_buffer_listener = {
    .release = pone_wl_buffer_release,
};

static i32 pone_wayland_create_buffer(const PoneWayland *wayland,
                                      struct wl_buffer **wl_buffer) {
    usize size = (usize)wayland->height * (usize)wayland->stride;

    i32 fd = memfd_create("pone-wl-shm", MFD_CLOEXEC);
    pone_assert(fd != -1);
    i32 ret = ftruncate(fd, size);
    pone_assert(ret != -1);

    struct wl_shm_pool *pool = wl_shm_create_pool(wayland->shm, fd, size);
    *wl_buffer =
        wl_shm_pool_create_buffer(pool, 0, wayland->width, wayland->height,
                                  wayland->stride, wayland->format);
    wl_shm_pool_destroy(pool);

    return fd;
}

static void pone_xdg_surface_configure(void *data,
                                       struct xdg_surface *xdg_surface,
                                       uint32_t serial) {
    PoneWayland *wayland = (PoneWayland *)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    if (wayland->resize_requested) {
        wayland->ready_to_resize = 1;
    }

    // struct wl_buffer *wl_buffer;
    // i32 fd = pone_wayland_create_buffer(wayland, &wl_buffer);
    //
    // usize size = (usize)wayland->height * (usize)wayland->stride;
    // u32 *buffer =
    //     (u32 *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // pone_assert(buffer != MAP_FAILED);
    // close(fd);
    //
    // for (usize y = 0; y < wayland->height; y++) {
    //     for (usize x = 0; x < wayland->width; x++) {
    //         u32 *pixel = (buffer + y * wayland->width + x);
    //         *pixel = 0x000000ff;
    //     }
    // }
    // munmap(data, size);
    //
    // wl_buffer_add_listener(wl_buffer, &wl_buffer_listener, 0);
    //
    // wl_surface_attach(wayland->surface, wl_buffer, 0, 0);
    // wl_surface_commit(wayland->surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = pone_xdg_surface_configure,
};

static void pone_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                                  uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = pone_xdg_wm_base_ping,
};

static void pone_wayland_wl_registry_global_handle(void *data,
                                                   struct wl_registry *registry,
                                                   uint32_t name,
                                                   const char *interface_cstr,
                                                   uint32_t version) {
    PoneWayland *wayland = (PoneWayland *)data;
    PoneString interface;
    pone_string_from_cstr(interface_cstr, &interface);

    printf("interface: '%s', version: %d, name: %d\n", interface_cstr, version,
           name);

    if (pone_string_eq_c_str((const PoneString *)&interface,
                             wl_compositor_interface.name)) {
        wayland->compositor = (struct wl_compositor *)wl_registry_bind(
            wayland->registry, name, &wl_compositor_interface, 4);
    } else if (pone_string_eq_c_str((const PoneString *)&interface,
                                    wl_shm_interface.name)) {
        wayland->shm = (struct wl_shm *)wl_registry_bind(
            wayland->registry, name, &wl_shm_interface, 1);
    } else if (pone_string_eq_c_str((const PoneString *)&interface,
                                    xdg_wm_base_interface.name)) {
        wayland->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
            wayland->registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wayland->xdg_wm_base, &xdg_wm_base_listener,
                                 (void *)wayland);
    }
}

static void pone_wayland_wl_registry_global_remove_handle(
    void *data, struct wl_registry *registry, uint32_t name) {
    // This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
    .global = pone_wayland_wl_registry_global_handle,
    .global_remove = pone_wayland_wl_registry_global_remove_handle,
};

static void pone_xdg_toplevel_configure(void *data,
                                        struct xdg_toplevel *xdg_toplevel,
                                        int32_t width, int32_t height,
                                        struct wl_array *states) {
    PoneWayland *wayland = (PoneWayland *)data;
    if (width == 0 || height == 0) {
        return;
    }

    wayland->width = width;
    wayland->height = height;
    wayland->stride = width * 4;
    wayland->resize_requested = 1;
}

static void pone_xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    PoneWayland *wayland = (PoneWayland *)data;
    wayland->closed = 1;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = pone_xdg_toplevel_configure,
    .close = pone_xdg_toplevel_close,
};

struct PoneFrameData {
    PoneVkCommandPool command_pool;
    PoneVkCommandBuffer command_buffer;
    PoneVkFence render_fence;
    PoneVkSemaphore swapchain_semaphore;
    PoneVkSemaphore render_semaphore;
};

static void pone_renderer_vk_destroy_swapchain(
    PoneVkDevice *device, PoneVkSwapchainKhr *swapchain,
    u32 swapchain_image_count, PoneVkImageView *swapchain_image_views) {
    pone_vk_destroy_swapchain_khr(device, swapchain);

    for (u32 i = 0; i < swapchain_image_count; i++) {
        PoneVkImageView *image_view = swapchain_image_views + i;
        pone_vk_destroy_image_view(device, image_view);
    }
}

struct PoneVkResizeSwapchainInfo {
    PoneVkSurface *surface;
    u32 min_image_count;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkExtent2D extent;
    u32 queue_family_index;
    VkSurfaceTransformFlagBitsKHR pre_transform;
    VkPresentModeKHR present_mode;
};

static void pone_renderer_vk_resize_swapchain(
    PoneVkDevice *device, PoneVkSwapchainKhr *swapchain,
    u32 swapchain_image_count, PoneVkImageView *swapchain_image_views,
    PoneVkResizeSwapchainInfo *resize_info, Arena *arena) {
    pone_vk_device_wait_idle(device);

    pone_renderer_vk_destroy_swapchain(device, swapchain, swapchain_image_count,
                                       swapchain_image_views);

    PoneVkSwapchainCreateInfoKhr swapchain_create_info = {
        .surface = resize_info->surface,
        .min_image_count = resize_info->min_image_count,
        .image_format = resize_info->format,
        .image_color_space = resize_info->color_space,
        .image_extent = resize_info->extent,
        .image_array_layers = 1,
        .image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
        .queue_family_index = resize_info->queue_family_index,
        .pre_transform = resize_info->pre_transform,
        .composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .present_mode = resize_info->present_mode,
        .clipped = 1,
    };

    usize arena_tmp_begin = arena->offset;
    PoneVkSwapchainKhr *new_swapchain =
        pone_vk_create_swapchain_khr(device, &swapchain_create_info, arena);
    u32 new_swapchain_image_count;
    pone_vk_get_swapchain_images_khr(device, new_swapchain,
                                     &new_swapchain_image_count, arena);
    pone_assert(new_swapchain_image_count == swapchain_image_count);
    for (usize i = 0; i < swapchain_image_count; i++) {
        usize arena_tmp_begin_2 = arena->offset;
        VkImageViewCreateInfo swapchain_image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = 0,
            .flags = 0,
            .image = new_swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = new_swapchain->image_format,
            .components =
                (VkComponentMapping){
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange = (VkImageSubresourceRange){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }};

        swapchain_image_views[i] = *pone_vk_create_image_view(
            device, &swapchain_image_view_create_info, arena);

        arena->offset = arena_tmp_begin_2;

        swapchain->images[i] = new_swapchain->images[i];
    }

    swapchain->handle = new_swapchain->handle;
    arena->offset = arena_tmp_begin;
}

int main(void) {
    Arena global_arena;
    global_arena.base = (void *)(usize)TERABYTES((usize)2);
    global_arena.offset = 0;
    global_arena.capacity = GIGABYTES((usize)2);
    global_arena.base =
        mmap(global_arena.base, global_arena.capacity, PROT_READ | PROT_WRITE,
             MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    pone_assert(global_arena.base != MAP_FAILED);

    Arena permanent_arena;
    pone_arena_create_sub_arena(&global_arena, GIGABYTES(1), &permanent_arena);
    Arena scratch_arena;
    pone_arena_create_sub_arena(&global_arena, GIGABYTES(1), &scratch_arena);

    PoneWayland wayland = {
        .width = 960,
        .height = 540,
        .format = WL_SHM_FORMAT_ABGR8888,
    };
    wayland.stride = wayland.width * 4;
    wayland.display = wl_display_connect(0);
    pone_assert(wayland.display);
    wayland.registry = wl_display_get_registry(wayland.display);
    pone_assert(wayland.registry);
    wl_registry_add_listener(wayland.registry, &registry_listener, &wayland);
    wl_display_roundtrip(wayland.display);
    pone_assert(wayland.compositor && wayland.shm && wayland.xdg_wm_base);
    wayland.surface = wl_compositor_create_surface(wayland.compositor);
    wayland.xdg_surface =
        xdg_wm_base_get_xdg_surface(wayland.xdg_wm_base, wayland.surface);
    xdg_surface_add_listener(wayland.xdg_surface, &xdg_surface_listener,
                             &wayland);
    wayland.xdg_toplevel = xdg_surface_get_toplevel(wayland.xdg_surface);
    xdg_toplevel_add_listener(wayland.xdg_toplevel, &xdg_toplevel_listener,
                              &wayland);
    xdg_toplevel_set_title(wayland.xdg_toplevel, "Pone Renderer");
    wl_surface_commit(wayland.surface);

    PoneVkInstance *instance = pone_vk_create_instance(&permanent_arena);
    PoneVkSurface *surface = pone_vk_create_wayland_surface_khr(
        instance, wayland.display, wayland.surface, &permanent_arena);
    PoneVkPhysicalDeviceFeatures physical_device_features = {
        .buffer_device_address = 1,
        .descriptor_indexing = 1,
        .synchronization_2 = 1,
        .dynamic_rendering = 1,
    };
    const char *required_extension_names_c_str[1] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    u32 required_extension_count = sizeof(required_extension_names_c_str) /
                                   sizeof(required_extension_names_c_str[0]);
    PoneString *required_extension_names =
        arena_alloc_array(&permanent_arena, required_extension_count, PoneString);
    for (usize i = 0; i < required_extension_count; i++) {
        pone_string_from_cstr(required_extension_names_c_str[i],
                              required_extension_names + i);
    }

    PoneVkPhysicalDeviceQuery physical_device_query = {
        .instance = instance,
        .surface = surface,
        .required_extension_count = required_extension_count,
        .required_extension_names = required_extension_names,
        .required_surface_format =
            (VkSurfaceFormatKHR){
                .format = VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
            },
        .required_present_mode = VK_PRESENT_MODE_FIFO_KHR,
        .required_features = &physical_device_features,
    };
    u32 queue_family_index;
    PoneVkPhysicalDevice *physical_device =
        pone_vk_select_optimal_physical_device(
            &physical_device_query, &permanent_arena, &queue_family_index);
    VkSurfaceCapabilitiesKHR surface_capabilities;
    pone_vk_physical_device_get_surface_capabilities(physical_device, surface,
                                                     &surface_capabilities);
    VkExtent2D surface_extent = {
        .width = wayland.width,
        .height = wayland.height,
    };
    if (surface_capabilities.currentExtent.width == U32_MAX ||
        surface_capabilities.currentExtent.height == U32_MAX) {
        surface_extent.width = PONE_CLAMP(
            surface_extent.width, surface_capabilities.minImageExtent.width,
            surface_capabilities.maxImageExtent.width);
        surface_extent.height = PONE_CLAMP(
            surface_extent.height, surface_capabilities.minImageExtent.height,
            surface_capabilities.maxImageExtent.height);
    } else {
        surface_extent = surface_capabilities.currentExtent;
    }
    VkPhysicalDeviceProperties2 physical_device_properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };
    pone_vk_physical_device_get_properties(physical_device,
                                           &physical_device_properties);
    printf("Selected device: %s\n",
           physical_device_properties.properties.deviceName);

    PoneVkDeviceCreateInfo device_create_info = {
        .queue_family_index = queue_family_index,
        .enabled_extension_count = required_extension_count,
        .enabled_extension_names = required_extension_names,
        .features = &physical_device_features,
    };
    PoneVkDevice *device = pone_vk_create_device(
        physical_device, &device_create_info, &permanent_arena);
    PoneVkSwapchainCreateInfoKhr swapchain_create_info = {
        .surface = surface,
        .min_image_count = surface_capabilities.minImageCount,
        .image_format = physical_device_query.required_surface_format.format,
        .image_color_space =
            physical_device_query.required_surface_format.colorSpace,
        .image_extent = surface_extent,
        .image_array_layers = 1,
        .image_usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .image_sharing_mode = VK_SHARING_MODE_EXCLUSIVE,
        .queue_family_index = queue_family_index,
        .pre_transform = surface_capabilities.currentTransform,
        .composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .present_mode = physical_device_query.required_present_mode,
        .clipped = 1,
    };
    PoneVkSwapchainKhr *swapchain = pone_vk_create_swapchain_khr(
        device, &swapchain_create_info, &permanent_arena);
    u32 swapchain_image_count;
    pone_vk_get_swapchain_images_khr(device, swapchain, &swapchain_image_count,
                                     &permanent_arena);
    PoneVkImageView *swapchain_image_views = arena_alloc_array(
        &permanent_arena, swapchain_image_count, PoneVkImageView);
    for (usize i = 0; i < swapchain_image_count; i++) {
        usize arena_tmp_begin = permanent_arena.offset;

        VkImageViewCreateInfo swapchain_image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = 0,
            .flags = 0,
            .image = swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->image_format,
            .components =
                (VkComponentMapping){
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange = (VkImageSubresourceRange){
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            }};

        swapchain_image_views[i] = *pone_vk_create_image_view(
            device, &swapchain_image_view_create_info, &permanent_arena);

        permanent_arena.offset = arena_tmp_begin;
    }
    VkDeviceQueueInfo2 device_queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
        .pNext = 0,
        .flags = 0,
        .queueFamilyIndex = queue_family_index,
        .queueIndex = 0,
    };
    PoneVkQueue *queue =
        pone_vk_get_device_queue(device, &device_queue_info, &permanent_arena);

    usize frame_overlap = 2;
    PoneFrameData *frame_datas =
        arena_alloc_array(&permanent_arena, frame_overlap, PoneFrameData);

    for (usize i = 0; i < frame_overlap; i++) {
        PoneVkCommandPool *command_pool = &frame_datas[i].command_pool;
        PoneVkCommandBuffer *command_buffer = &frame_datas[i].command_buffer;
        PoneVkFence *render_fence = &frame_datas[i].render_fence;
        PoneVkSemaphore *swapchain_semaphore =
            &frame_datas[i].swapchain_semaphore;
        PoneVkSemaphore *render_semaphore = &frame_datas[i].render_semaphore;

        VkCommandPoolCreateInfo command_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext = 0,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queue_family_index,
        };
        pone_vk_create_command_pool(device, &command_pool_create_info,
                                    command_pool);
        PoneVkCommandBufferAllocateInfo allocate_info = {
            .command_pool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .command_buffer_count = 1,
        };

        pone_vk_allocate_command_buffers(device, &allocate_info, &permanent_arena,
                                         command_buffer);

        VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = 0,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        pone_vk_create_fence(device, &fence_create_info, render_fence);

        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = 0,
            .flags = 0,
        };
        pone_vk_create_semaphore(device, &semaphore_create_info,
                                 swapchain_semaphore);
        pone_vk_create_semaphore(device, &semaphore_create_info,
                                 render_semaphore);
    }

    usize frame_index = 0;
    while (wl_display_dispatch(wayland.display) != -1 && !wayland.closed) {
        if (wayland.resize_requested && wayland.ready_to_resize) {
            PoneVkResizeSwapchainInfo resize_info = {
                .surface = surface,
                .min_image_count = surface_capabilities.minImageCount,
                .format = physical_device_query.required_surface_format.format,
                .color_space =
                    physical_device_query.required_surface_format.colorSpace,
                .extent =
                    (VkExtent2D){
                        .width = wayland.width,
                        .height = wayland.height,
                    },
                .queue_family_index = queue_family_index,
                .pre_transform = surface_capabilities.currentTransform,
                .present_mode = physical_device_query.required_present_mode,
            };

            pone_renderer_vk_resize_swapchain(
                device, swapchain, swapchain_image_count, swapchain_image_views,
                &resize_info, &permanent_arena);

            wl_surface_commit(wayland.surface);
            wayland.resize_requested = 0;
            wayland.ready_to_resize = 0;
        } else if (wayland.resize_requested) {
            pone_assert(0);
        }

        PoneFrameData *frame_data = frame_datas + (frame_index % frame_overlap);

        pone_vk_wait_for_fences(device, 1, &frame_data->render_fence, 1,
                                1000000000, &permanent_arena);
        pone_vk_reset_fences(device, 1, &frame_data->render_fence,
                             &permanent_arena);

        u32 swapchain_image_index;
        PoneVkAcquireNextImageInfoKhr acquire_swapchain_image_info = {
            .swapchain = swapchain,
            .timeout = 1000000000,
            .semaphore = &frame_data->swapchain_semaphore,
            .fence = 0,
        };
        VkResult acquire_ret = pone_vk_acquire_next_image_khr(
            device, &acquire_swapchain_image_info, &swapchain_image_index);
        if (acquire_ret == VK_ERROR_OUT_OF_DATE_KHR) {
            wayland.resize_requested = 1;
            continue;
        }
        pone_vk_begin_command_buffer(
            &frame_data->command_buffer,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        transition_image(&frame_data->command_buffer,
                         swapchain->images[swapchain_image_index],
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkClearColorValue clear_value = {{1.0f, 0.0f, 0.0f, 1.0f}};
        VkImageSubresourceRange clear_range = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS,
        };
        pone_vk_cmd_clear_color_image(&frame_data->command_buffer,
                                      swapchain->images[swapchain_image_index],
                                      VK_IMAGE_LAYOUT_GENERAL, &clear_value, 1,
                                      &clear_range);
        transition_image(&frame_data->command_buffer,
                         swapchain->images[swapchain_image_index],
                         VK_IMAGE_LAYOUT_GENERAL,
                         VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        pone_vk_end_command_buffer(&frame_data->command_buffer);

        VkCommandBufferSubmitInfo command_buffer_submit_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .pNext = 0,
            .commandBuffer = frame_data->command_buffer.handle,
            .deviceMask = 0,
        };

        VkSemaphoreSubmitInfo wait_semaphore_submit_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = 0,
            .semaphore = frame_data->swapchain_semaphore.handle,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            .deviceIndex = 0,
        };
        VkSemaphoreSubmitInfo signal_semaphore_submit_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = 0,
            .semaphore = frame_data->render_semaphore.handle,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .deviceIndex = 0,
        };
        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = 0,
            .flags = 0,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &wait_semaphore_submit_info,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &command_buffer_submit_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_semaphore_submit_info,
        };
        pone_vk_queue_submit_2(queue, 1, &submit_info,
                               frame_data->render_fence.handle);

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = 0,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame_data->render_semaphore.handle,
            .swapchainCount = 1,
            .pSwapchains = &swapchain->handle,
            .pImageIndices = &swapchain_image_index,
            .pResults = 0,
        };
        VkResult present_ret = pone_vk_queue_present_khr(queue, &present_info);
        if (present_ret == VK_ERROR_OUT_OF_DATE_KHR) {
            wayland.resize_requested = 1;
            continue;
        }

        frame_index++;
    }

    return 0;
}

#endif

#if defined(PONE_PLATFORM_WIN64)

#include <windows.h>

#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

LRESULT win32_main_window_callback(HWND hwnd, UINT message, WPARAM w_param,
                                   LPARAM l_param) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, w_param, l_param)) {
        return 1;
    }

    LRESULT result = 0;
    switch (message) {
    case WM_QUIT:
    case WM_DESTROY:
    case WM_CLOSE: {
        global_running = 0;
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        usize vk_code = w_param;
        // b8 was_down = (l_param & (1 << 30)) != 0;
        b8 is_down = (l_param & (1 << 31)) == 0;

        if (vk_code == VK_ESCAPE && is_down) {
            global_running = 0;
        }
    } break;
    default: {
        result = DefWindowProcA(hwnd, message, w_param, l_param);
    } break;
    }

    return result;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
               VkDebugUtilsMessageTypeFlagsEXT message_type,
               const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
               void *user_data) {
    Arena *scratch = (Arena *)user_data;

    const char *severity = 0;
    switch (message_severity) {
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: {
        severity = "[ERROR]";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: {
        severity = "[VERBOSE]";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: {
        severity = "[INFO]";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: {
        severity = "[WARNING]";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: {
    } break;
    }

    const char *type = 0;
    switch (message_type) {
    case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: {
        type = "[GENERAL]     ";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: {
        type = "[VALIDATION]  ";
    } break;
    case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: {
        type = "[PERFORMANCE] ";
    } break;
    }

    char *msg;
    i32 n = arena_sprintf(scratch, &msg, "%s%s%s\n", severity, type,
                          callback_data->pMessage);
    assert(n > 0);
    OutputDebugStringA(msg);

    arena_clear(scratch);
    return VK_FALSE;
}

static void load_shader_module(Arena *arena, char *filename, VkDevice device,
                               VkAllocationCallbacks *allocation_callbacks,
                               PFN_vkCreateShaderModule vk_create_shader_module,
                               VkShaderModule *shader_module) {
    usize shader_code_size;
    pone_assert(platform_read_file(filename, &shader_code_size, 0));
    pone_assert(shader_code_size % 4 == 0);
    void *shader_code = arena_alloc(arena, shader_code_size);
    pone_assert(shader_code);
    pone_assert(platform_read_file(filename, &shader_code_size, shader_code));

    VkShaderModuleCreateInfo shader_module_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .codeSize = shader_code_size,
        .pCode = (u32 *)shader_code,
    };

    vk_check(vk_create_shader_module(device, &shader_module_create_info,
                                     allocation_callbacks, shader_module));
}

b8 platform_read_file(char *filename, usize *size, void *contents) {
    assert(size);
    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0,
                                     OPEN_EXISTING, 0, 0);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle, &file_size)) {
        return 0;
    }
    if (!contents) {
        *size = file_size.QuadPart;
        CloseHandle(file_handle);
        return 1;
    }

    DWORD bytes_to_read =
        *size <= (usize)file_size.QuadPart ? *size : (usize)file_size.QuadPart;
    DWORD bytes_read = 0;
    if (!ReadFile(file_handle, contents, bytes_to_read, &bytes_read, 0) ||
        bytes_read != bytes_to_read) {
        CloseHandle(file_handle);
        return 0;
    }

    *size = (usize)bytes_read;
    CloseHandle(file_handle);
    return 1;
}

static void pone_produce_work(PoneThreadInfo *thread_info,
                              PoneWorkQueueData *data) {
    pone_assert(pone_work_queue_enqueue(thread_info->work_queue, data));
    _pone_atomic_store_n(thread_info->work_available, 1,
                         PONE_MEMORY_ORDERING_RELEASE);
    WakeByAddressSingle((void *)thread_info->work_available);
}

static b8 pone_try_consume_work(PoneThreadInfo *thread_info) {
    PoneWorkQueueData data;
    b8 original_work_avaiable = _pone_atomic_load_n(
        thread_info->work_available, PONE_MEMORY_ORDERING_ACQUIRE);
    if (original_work_avaiable) {
        if (pone_work_queue_dequeue(thread_info->work_queue, &data)) {
            _pone_atomic_store_n(
                thread_info->work_available,
                pone_work_queue_length(thread_info->work_queue) != 0,
                PONE_MEMORY_ORDERING_RELEASE);
            PoneThreadWorkData thread_data = {
                .thread_info = thread_info,
                .user_data = data.user_data,
            };
            (data.work)(&thread_data);
            return 1;
        }

        return 0;
    } else {
        return 0;
    }
}

static void pone_consume_work(PoneThreadInfo *thread_info) {
    PoneWorkQueueData data;
    for (;;) {
        b8 original_work_avaiable = _pone_atomic_load_n(
            thread_info->work_available, PONE_MEMORY_ORDERING_ACQUIRE);
        if (original_work_avaiable) {
            if (pone_work_queue_dequeue(thread_info->work_queue, &data)) {
                _pone_atomic_store_n(
                    thread_info->work_available,
                    pone_work_queue_length(thread_info->work_queue) != 0,
                    PONE_MEMORY_ORDERING_RELEASE);
                PoneThreadWorkData thread_data = {
                    .thread_info = thread_info,
                    .user_data = data.user_data,
                };
                (data.work)(&thread_data);
            }
        } else {
            WaitOnAddress(thread_info->work_available, &original_work_avaiable,
                          sizeof(b8), INFINITE);
        }
    }
}

static DWORD WINAPI pone_win32_consumer_thread_proc(LPVOID param) {
    pone_consume_work((PoneThreadInfo *)param);
    return 0;
}

static void pone_print_string(usize thread_index, char *s) {
    char buf[256];
    _snprintf(buf, 256, "Thread %d: %s\n", thread_index, s);
    OutputDebugStringA(buf);
}

static void pone_print_string_work(void *user_data) {
    PoneThreadWorkData *work_data = (PoneThreadWorkData *)user_data;
    pone_print_string(work_data->thread_info->thread_index,
                      (char *)work_data->user_data);
}

int WinMain(HINSTANCE hinstance, HINSTANCE hprevinstance, LPSTR cmd_line,
            int cmd_show) {
    WNDCLASSA window_class = {};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = win32_main_window_callback;
    window_class.hInstance = hinstance;
    // windowClass.hIcon;
    window_class.lpszClassName = "PoneWindowClass";

    if (RegisterClassA(&window_class)) {
        HWND hwnd = CreateWindowExA(
            0, window_class.lpszClassName, "Pone Engine",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hinstance, 0);
        if (hwnd) {
            RECT client_rect;
            pone_assert(GetClientRect(hwnd, &client_rect));

            void *base_address = (void *)(TERABYTES((usize)2));
            usize total_memory_size = GIGABYTES((usize)8);
            base_address = VirtualAlloc(base_address, total_memory_size,
                                        MEM_RESERVE, PAGE_READWRITE);
            pone_assert(base_address);
            Arena permanent_memory;
            permanent_memory.capacity = MEGABYTES(512);
            permanent_memory.offset = 0;
            permanent_memory.base =
                VirtualAlloc(base_address, permanent_memory.capacity,
                             MEM_COMMIT, PAGE_READWRITE);

            Arena transient_memory;
            transient_memory.capacity = GIGABYTES(1) + MEGABYTES(512);
            transient_memory.offset = 0;
            transient_memory.base = VirtualAlloc(
                (void *)((usize)base_address + total_memory_size -
                         transient_memory.capacity),
                transient_memory.capacity, MEM_COMMIT, PAGE_READWRITE);

            PoneWorkQueue work_queue;
            pone_work_queue_init(&work_queue, &permanent_memory);
            volatile b8 *work_available =
                (volatile b8 *)arena_alloc(&permanent_memory, sizeof(b8));
            *work_available = 0;

            usize thread_count = 8;
            PoneThreadInfo *thread_infos = arena_alloc_array(
                &permanent_memory, thread_count, PoneThreadInfo);
            thread_infos[0] = {
                .thread_index = 0,
                .work_queue = &work_queue,
                .work_available = work_available,
            };
            for (usize i = 1; i < thread_count; ++i) {
                thread_infos[i] = {
                    .thread_index = i,
                    .work_queue = &work_queue,
                    .work_available = work_available,
                };

                HANDLE thread_handle =
                    CreateThread(0, 0, pone_win32_consumer_thread_proc,
                                 &thread_infos[i], 0, 0);
                CloseHandle(thread_handle);
            }

            PoneWorkQueueData data[256];
            for (usize i = 0; i < 256; ++i) {
                data[i].work = pone_print_string_work;
                arena_sprintf(&transient_memory, (char **)&data[i].user_data,
                              "String %d", i);

                pone_produce_work(thread_infos, data + i);
            }

            while (pone_work_queue_length(thread_infos[0].work_queue) != 0) {
                pone_try_consume_work(thread_infos);
            }

            usize capacity = MEGABYTES((usize)32);
            Arena *global_arena = arena_create(capacity);
            Arena *scratch = arena_create(capacity);

            PoneTruetypeInput font_file;
            pone_assert(platform_read_file("../fonts/SourceCodePro-Regular.ttf",
                                           &font_file.length, 0));
            font_file.data = arena_alloc(&transient_memory, font_file.length);
            pone_assert(platform_read_file("../fonts/SourceCodePro-Regular.ttf",
                                           &font_file.length, font_file.data));
            PoneTrueTypeFont *font =
                pone_truetype_parse(font_file, &transient_memory);
            PoneTrueTypeSdfAtlas sdf_atlas;
            u32 sdf_size = 48;
            u32 sdf_pad = 6;
            pone_truetype_font_generate_sdf(font, sdf_size, sdf_pad,
                                            &permanent_memory,
                                            &transient_memory, &sdf_atlas);
            usize pgm_width = sdf_atlas.width;
            usize pgm_height = sdf_atlas.height;

            HANDLE pgm_file_handle =
                CreateFileA("sdf_output.pgm", GENERIC_WRITE, FILE_SHARE_WRITE,
                            0, CREATE_ALWAYS, 0, 0);
            usize pgm_header_scratch_offset = transient_memory.offset;
            char *pgm_header;
            i32 pgm_header_len =
                arena_sprintf(&transient_memory, &pgm_header,
                              "P2\n%d %d\n255\n", pgm_width, pgm_height);
            WriteFile(pgm_file_handle, pgm_header, pgm_header_len, 0, 0);
            transient_memory.offset = pgm_header_scratch_offset;

            for (usize pgm_y = 0; pgm_y < pgm_height; ++pgm_y) {
                for (usize pgm_x = 0; pgm_x < pgm_width; ++pgm_x) {
                    u32 pixel = sdf_atlas.buf[pgm_y * pgm_width + pgm_x];
                    u32 gray_value = (pixel & 0xFF000000) >> 24;

                    usize scratch_offset = transient_memory.offset;
                    char *buf;
                    i32 buf_len = arena_sprintf(&transient_memory, &buf, "%d",
                                                gray_value);
                    WriteFile(pgm_file_handle, buf, buf_len, 0, 0);
                    transient_memory.offset = scratch_offset;

                    if (pgm_x != pgm_width - 1) {
                        WriteFile(pgm_file_handle, " ", 1, 0, 0);
                    }
                }
                WriteFile(pgm_file_handle, "\n", 1, 0, 0);
            }
#if 0
            for (usize pgm_y = 0; pgm_y < pgm_height; ++pgm_y) {
                for (usize pgm_x = 0; pgm_x < pgm_width; ++pgm_x) {
                    usize sdf_index =
                        ((pgm_y / sdf_size) * (pgm_width / sdf_size)) +
                        pgm_x / sdf_size;
                    if (sdf_index < sdf_bitmap_count) {
                        u32 *sdf_bitmap = sdf_bitmaps[sdf_index];
                        usize sdf_x = pgm_x % sdf_size;
                        usize sdf_y = pgm_y % sdf_size;

                        u32 pixel = sdf_bitmap[(sdf_y * sdf_size) + sdf_x];
                        u32 gray_value = (pixel & 0xFF000000) >> 24;

                        usize scratch_offset = transient_memory.offset;
                        char *buf;
                        i32 buf_len = arena_sprintf(&transient_memory, &buf,
                                "%d", gray_value);
                        WriteFile(pgm_file_handle, buf, buf_len, 0, 0);

                        transient_memory.offset = scratch_offset;
                    } else {
                        WriteFile(pgm_file_handle, "0", 1, 0, 0);
                    }

                    if (pgm_x != pgm_width - 1) {
                        WriteFile(pgm_file_handle, " ", 1, 0, 0);
                    }
                }
                WriteFile(pgm_file_handle, "\n", 1, 0, 0);
            }
#endif

#if 0
            u32 *d_sdf_bitmap = sdf_bitmaps[92];
            for (usize sdf_y = 0; sdf_y < sdf_size; ++sdf_y) {
                for (usize sdf_x = 0; sdf_x < sdf_size; ++sdf_x) {
                    u32 pixel = d_sdf_bitmap[(sdf_y * sdf_size) + sdf_x];
                    u32 gray_value = (pixel & 0xFF000000) >> 24;

                    usize scratch_offset = transient_memory.offset;
                    char *buf;
                    i32 buf_len = arena_sprintf(&transient_memory, &buf, "%d",
                            gray_value);
                    WriteFile(pgm_file_handle, buf, buf_len, 0, 0);
                    if (sdf_x != 255) {
                        WriteFile(pgm_file_handle, " ", 1, 0, 0);
                    }

                    transient_memory.offset = scratch_offset;
                }
                WriteFile(pgm_file_handle, "\n", 1, 0, 0);
            }
#endif
            CloseHandle(pgm_file_handle);

            usize glb_size;
            assert(platform_read_file("../assets/basicmesh.glb", &glb_size, 0));
            void *glb_data = arena_alloc(scratch, glb_size);
            assert(platform_read_file("../assets/basicmesh.glb", &glb_size,
                                      glb_data));

            PoneGltf *basic_mesh_gltf = pone_gltf_parse(glb_data, scratch);
            usize mesh_count = basic_mesh_gltf->mesh_count;
            Mesh *meshes = arena_alloc_array(global_arena, mesh_count, Mesh);

            for (usize mesh_index = 0; mesh_index < mesh_count; ++mesh_index) {
                Mesh *mesh = meshes + mesh_index;

                void *index_data;
                usize index_data_size;

                void *position_data;
                usize position_data_size;

                void *normal_data;
                usize normal_data_size;

                void *uv_data;
                usize uv_data_size;

                for (usize mesh_primitive_index = 0;
                     mesh_primitive_index <
                     basic_mesh_gltf->meshes[mesh_index].primitive_count;
                     ++mesh_primitive_index) {

                    pone_gltf_get_mesh_primitive_index_data(
                        basic_mesh_gltf, mesh_index, mesh_primitive_index,
                        &index_data, &index_data_size);
                    pone_gltf_get_mesh_primitive_attribute_data(
                        basic_mesh_gltf, mesh_index, mesh_primitive_index,
                        (PoneString){.buf = (u8 *)"POSITION", .len = 8},
                        &position_data, &position_data_size);
                    pone_gltf_get_mesh_primitive_attribute_data(
                        basic_mesh_gltf, mesh_index, mesh_primitive_index,
                        (PoneString){.buf = (u8 *)"NORMAL", .len = 6},
                        &normal_data, &normal_data_size);
                    pone_gltf_get_mesh_primitive_attribute_data(
                        basic_mesh_gltf, mesh_index, mesh_primitive_index,
                        (PoneString){.buf = (u8 *)"TEXCOORD_0", .len = 10},
                        &uv_data, &uv_data_size);
                }

                mesh->index_count = index_data_size / 2;
                mesh->indices =
                    arena_alloc_array(global_arena, mesh->index_count, u16);
                pone_memcpy((void *)mesh->indices, index_data, index_data_size);

                mesh->vertex_count = position_data_size / 12;
                mesh->vertices =
                    arena_alloc_array(global_arena, mesh->vertex_count, Vertex);
                for (usize vertex_index = 0; vertex_index < mesh->vertex_count;
                     ++vertex_index) {
                    Vertex *vertex = mesh->vertices + vertex_index;

                    usize uv_data_offset = vertex_index * sizeof(Vec2);

                    // vertex->position = (Vec3){
                    //     .x = *(f32 *)((u8 *)position_data +
                    //                   position_data_offset),
                    //     .y = *((f32 *)((u8 *)position_data +
                    //                    position_data_offset) +
                    //            1),
                    //     .z = *((f32 *)((u8 *)position_data +
                    //                    position_data_offset) +
                    //            2),
                    // };
                    vertex->position = *((Vec3 *)position_data + vertex_index);
                    vertex->normal = *((Vec3 *)normal_data + vertex_index);
                    // vertex->normal = (Vec3){
                    //     .x = *(f32 *)((u8 *)normal_data +
                    //     normal_data_offset), .y = *((f32 *)((u8 *)normal_data
                    //     + normal_data_offset) +
                    //            1),
                    //     .z = *((f32 *)((u8 *)normal_data +
                    //     normal_data_offset) +
                    //            2),
                    // };
                    vertex->uv_x = *(f32 *)((u8 *)uv_data + uv_data_offset);
                    vertex->uv_y =
                        *((f32 *)((u8 *)uv_data + uv_data_offset) + 1);
                    vertex->color = (Vec4){
                        .x = vertex->normal.x,
                        .y = vertex->normal.y,
                        .z = vertex->normal.z,
                        .w = 1.0f,
                    };
                }
            }
            arena_clear(scratch);

            HMODULE vulkan_lib = LoadLibraryA("vulkan-1.dll");
            assert(vulkan_lib);

            PFN_vkGetInstanceProcAddr _vk_get_instance_proc_addr =
                (PFN_vkGetInstanceProcAddr)GetProcAddress(
                    vulkan_lib, "vkGetInstanceProcAddr");
            assert(_vk_get_instance_proc_addr);

            vk_get_instance_proc_addr(0, vkCreateInstance);
            vk_get_instance_proc_addr(0, vkEnumerateInstanceLayerProperties);
            vk_get_instance_proc_addr(0,
                                      vkEnumerateInstanceExtensionProperties);

            u8 **enabled_layer_names = 0;
            usize enabled_layer_count = 0;
            u8 **enabled_extension_names = 0;
            usize enabled_extension_count = 0;

            u32 property_count;
            vk_check(vkEnumerateInstanceLayerProperties(&property_count, 0));
            VkLayerProperties *layer_properties =
                (VkLayerProperties *)arena_alloc(
                    scratch, property_count * sizeof(VkLayerProperties));
            assert(layer_properties);
            vk_check(vkEnumerateInstanceLayerProperties(&property_count,
                                                        layer_properties));
            for (u32 i = 0; i < property_count; i++) {
                if (string_eq((u8 *)"VK_LAYER_KHRONOS_validation",
                              (u8 *)layer_properties[i].layerName)) {
                    printf("Validation layer supported\n");
                    enabled_layer_names =
                        (u8 **)arena_alloc(scratch, sizeof(u8 **));
                    assert(enabled_layer_names);

                    enabled_layer_names[0] = (u8 *)arena_alloc(scratch, 28);
                    assert(enabled_layer_names[0]);

                    string_copy(enabled_layer_names[0],
                                (u8 *)"VK_LAYER_KHRONOS_validation", 28);
                    enabled_layer_count++;

                    vk_check(vkEnumerateInstanceExtensionProperties(
                        (char *)enabled_layer_names[0], &property_count, 0));
                    VkExtensionProperties *extension_properties =
                        (VkExtensionProperties *)arena_alloc(
                            scratch,
                            property_count * sizeof(VkExtensionProperties));
                    assert(extension_properties);
                    vk_check(vkEnumerateInstanceExtensionProperties(
                        (char *)enabled_layer_names[0], &property_count,
                        extension_properties));

                    for (u32 i = 0; i < property_count; i++) {
                        if (string_eq(
                                (u8 *)extension_properties[i].extensionName,
                                (u8 *)VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                            printf("Debug utils extension "
                                   "supported\n");
                            enabled_extension_names =
                                (u8 **)arena_alloc(scratch, sizeof(u8 **));
                            assert(enabled_extension_names);
                            enabled_extension_names[0] =
                                (u8 *)arena_alloc(scratch, 19);
                            assert(enabled_layer_names[0]);
                            string_copy(enabled_extension_names[0],
                                        (u8 *)VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                        19);
                            enabled_extension_count++;
                        }
                    }
                }
            }
            vk_check(
                vkEnumerateInstanceExtensionProperties(0, &property_count, 0));
            VkExtensionProperties *extension_properties =
                (VkExtensionProperties *)arena_alloc(
                    scratch, property_count * sizeof(VkExtensionProperties));
            assert(extension_properties);
            vk_check(vkEnumerateInstanceExtensionProperties(
                0, &property_count, extension_properties));
            for (u32 i = 0; i < property_count; i++) {
                if (string_eq((u8 *)VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                              (u8 *)extension_properties[i].extensionName)) {
                    enabled_extension_names = (u8 **)arena_realloc(
                        scratch, enabled_extension_names,
                        sizeof(u8 **) * (enabled_extension_count + 1));
                    assert(enabled_extension_names);
                    enabled_extension_count++;
                    enabled_extension_names[enabled_extension_count - 1] =
                        (u8 *)arena_alloc(scratch, 21);
                    assert(
                        enabled_extension_names[enabled_extension_count - 1]);
                    string_copy(
                        enabled_extension_names[enabled_extension_count - 1],
                        (u8 *)VK_KHR_WIN32_SURFACE_EXTENSION_NAME, 21);
                } else if (string_eq(
                               (u8 *)VK_KHR_SURFACE_EXTENSION_NAME,
                               (u8 *)extension_properties[i].extensionName)) {
                    enabled_extension_names = (u8 **)arena_realloc(
                        scratch, enabled_extension_names,
                        sizeof(u8 **) * (enabled_extension_count + 1));
                    assert(enabled_extension_names);
                    enabled_extension_count++;
                    enabled_extension_names[enabled_extension_count - 1] =
                        (u8 *)arena_alloc(scratch, 15);
                    assert(
                        enabled_extension_names[enabled_extension_count - 1]);
                    string_copy(
                        enabled_extension_names[enabled_extension_count - 1],
                        (u8 *)VK_KHR_SURFACE_EXTENSION_NAME, 15);
                }
            }

            VkApplicationInfo app_info;
            app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app_info.pNext = 0;
            app_info.pApplicationName = "Example Vulkan Application";
            app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
            app_info.pEngineName = "Pone Engine";
            app_info.engineVersion = 0;
            app_info.apiVersion = VK_API_VERSION_1_4;

            Arena *debug_fmt_scratch = arena_create(KILOBYTES(4));
            VkDebugUtilsMessengerCreateInfoEXT
                debug_utils_messenger_create_info;
            debug_utils_messenger_create_info.sType =
                VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debug_utils_messenger_create_info.pNext = 0;
            debug_utils_messenger_create_info.flags = 0;
            debug_utils_messenger_create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debug_utils_messenger_create_info.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debug_utils_messenger_create_info.pfnUserCallback = debug_callback;
            debug_utils_messenger_create_info.pUserData =
                (void *)debug_fmt_scratch;

            VkInstanceCreateInfo instance_create_info;
            instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_create_info.pNext = &debug_utils_messenger_create_info;
            instance_create_info.flags = 0;
            instance_create_info.pApplicationInfo = &app_info;
            instance_create_info.enabledLayerCount = enabled_layer_count;
            instance_create_info.ppEnabledLayerNames =
                (char **)enabled_layer_names;
            instance_create_info.enabledExtensionCount =
                enabled_extension_count;
            instance_create_info.ppEnabledExtensionNames =
                (char **)enabled_extension_names;

            VkInstance instance;
            VkAllocationCallbacks allocation_callbacks;
            allocation_callbacks.pUserData = (void *)global_arena;
            allocation_callbacks.pfnAllocation = vk_allocation;
            allocation_callbacks.pfnReallocation = vk_reallocation;
            allocation_callbacks.pfnFree = vk_free;
            allocation_callbacks.pfnInternalAllocation = 0;
            allocation_callbacks.pfnInternalFree = 0;

            vk_check(vkCreateInstance(&instance_create_info,
                                      &allocation_callbacks, &instance));
            printf("Successfully created instance\n");
            arena_clear(scratch);

            vk_get_instance_proc_addr(instance, vkCreateDebugUtilsMessengerEXT);
            vk_get_instance_proc_addr(instance, vkEnumeratePhysicalDevices);
            vk_get_instance_proc_addr(instance, vkGetPhysicalDeviceProperties2);
            vk_get_instance_proc_addr(instance, vkGetPhysicalDeviceFeatures2);
            vk_get_instance_proc_addr(instance,
                                      vkGetPhysicalDeviceQueueFamilyProperties);
            vk_get_instance_proc_addr(instance,
                                      vkGetPhysicalDeviceMemoryProperties2);
            vk_get_instance_proc_addr(instance, vkCreateWin32SurfaceKHR);
            vk_get_instance_proc_addr(instance,
                                      vkGetPhysicalDeviceSurfaceSupportKHR);
            vk_get_instance_proc_addr(instance,
                                      vkEnumerateDeviceExtensionProperties);
            vk_get_instance_proc_addr(
                instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
            vk_get_instance_proc_addr(
                instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
            vk_get_instance_proc_addr(instance,
                                      vkGetPhysicalDeviceSurfaceFormatsKHR);
            vk_get_instance_proc_addr(instance, vkCreateDevice);
            vk_get_instance_proc_addr(instance, vkGetDeviceProcAddr);
            vk_get_instance_proc_addr(instance, vkDestroyInstance);
            vk_get_instance_proc_addr(instance, vkDestroySurfaceKHR);
            vk_get_instance_proc_addr(instance,
                                      vkDestroyDebugUtilsMessengerEXT);

            VkDebugUtilsMessengerEXT messenger;
            vk_check(vkCreateDebugUtilsMessengerEXT(
                instance, &debug_utils_messenger_create_info,
                &allocation_callbacks, &messenger));

            VkSurfaceKHR surface;
            VkWin32SurfaceCreateInfoKHR surface_create_info;
            surface_create_info.sType =
                VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surface_create_info.pNext = 0;
            surface_create_info.flags = 0;
            surface_create_info.hinstance = hinstance;
            surface_create_info.hwnd = hwnd;
            vk_check(vkCreateWin32SurfaceKHR(instance, &surface_create_info,
                                             &allocation_callbacks, &surface));

            VkPhysicalDevice selected_physical_device = VK_NULL_HANDLE;
            u32 *queue_family_index = 0;
            u32 physical_device_count;
            vk_check(vkEnumeratePhysicalDevices(instance,
                                                &physical_device_count, 0));
            VkPhysicalDevice *physical_devices =
                (VkPhysicalDevice *)arena_alloc(
                    scratch, physical_device_count * sizeof(VkPhysicalDevice));
            assert(physical_devices);
            vk_check(vkEnumeratePhysicalDevices(
                instance, &physical_device_count, physical_devices));
            u8 **enabled_device_extension_names = 0;
            usize enabled_device_extension_name_count = 0;

            VkPhysicalDeviceVulkan13Features physical_device_features_1_3;
            physical_device_features_1_3.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            physical_device_features_1_3.pNext = 0;

            VkPhysicalDeviceVulkan12Features physical_device_features_1_2;
            physical_device_features_1_2.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
            physical_device_features_1_2.pNext =
                (void *)&physical_device_features_1_3;

            VkPhysicalDeviceFeatures2 physical_device_features_2;
            physical_device_features_2.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physical_device_features_2.pNext =
                (void *)&physical_device_features_1_2;

            u32 image_count;
            VkFormat swapchain_image_format;
            VkColorSpaceKHR image_color_space;
            VkExtent2D swapchain_image_extent;
            VkSurfaceTransformFlagBitsKHR surface_transform;
            VkPresentModeKHR present_mode;
            usize scratch_offset = scratch->offset;
            for (u32 i = 0; i < physical_device_count; i++) {
                scratch->offset = scratch_offset;
                VkPhysicalDevice physical_device = physical_devices[i];

                // Device extension check
                vk_check(vkEnumerateDeviceExtensionProperties(
                    physical_device, 0, &property_count, 0));
                extension_properties = (VkExtensionProperties *)arena_alloc(
                    scratch, property_count * sizeof(VkExtensionProperties));
                assert(extension_properties);
                vk_check(vkEnumerateDeviceExtensionProperties(
                    physical_device, 0, &property_count, extension_properties));
                for (u32 j = 0; j < property_count; j++) {
                    if (string_eq((u8 *)extension_properties[j].extensionName,
                                  (u8 *)VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                        enabled_device_extension_names = (u8 **)arena_realloc(
                            scratch, enabled_device_extension_names,
                            (enabled_device_extension_name_count + 1) *
                                sizeof(u8 **));
                        assert(enabled_extension_names);
                        enabled_device_extension_name_count++;
                        enabled_device_extension_names
                            [enabled_device_extension_name_count - 1] =
                                (u8 *)arena_alloc(scratch, 17);
                        assert(enabled_device_extension_names
                                   [enabled_device_extension_name_count - 1]);
                        string_copy(
                            enabled_device_extension_names
                                [enabled_device_extension_name_count - 1],
                            (u8 *)VK_KHR_SWAPCHAIN_EXTENSION_NAME, 17);
                    } else if (
                        string_eq(
                            (u8 *)extension_properties[j].extensionName,
                            (u8 *)VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME)) {
                        enabled_device_extension_names = (u8 **)arena_realloc(
                            scratch, enabled_device_extension_names,
                            (enabled_device_extension_name_count + 1) *
                                sizeof(u8 **));
                        assert(enabled_device_extension_names);
                        enabled_device_extension_name_count++;
                        enabled_device_extension_names
                            [enabled_device_extension_name_count - 1] =
                                (u8 *)arena_alloc(scratch, 24);
                        assert(enabled_device_extension_names
                                   [enabled_device_extension_name_count - 1]);
                        string_copy(
                            enabled_device_extension_names
                                [enabled_device_extension_name_count - 1],
                            (u8 *)VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, 24);
                    }
                }
                if (enabled_device_extension_name_count != 2) {
                    continue;
                }

                // Queue family check
                vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                                         &property_count, 0);
                VkQueueFamilyProperties *queue_family_properties =
                    (VkQueueFamilyProperties *)arena_alloc(
                        scratch,
                        property_count * sizeof(VkQueueFamilyProperties));
                assert(queue_family_properties);
                vkGetPhysicalDeviceQueueFamilyProperties(
                    physical_device, &property_count, queue_family_properties);
                for (u32 i = 0; i < property_count; i++) {
                    VkBool32 surface_supported = 0;
                    vk_check(vkGetPhysicalDeviceSurfaceSupportKHR(
                        physical_device, i, surface, &surface_supported));
                    if (surface_supported) {
                        VkQueueFamilyProperties properties =
                            queue_family_properties[i];
                        if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                            queue_family_index =
                                (u32 *)arena_alloc(global_arena, sizeof(u32));
                            assert(queue_family_index);

                            *queue_family_index = i;
                            break;
                        }
                    }
                }
                if (!queue_family_index) {
                    continue;
                }

                VkSurfaceCapabilitiesKHR surface_capabilities;
                vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                    physical_device, surface, &surface_capabilities));

                u32 surface_format_count;
                vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physical_device, surface, &surface_format_count, 0));
                VkSurfaceFormatKHR *surface_formats =
                    (VkSurfaceFormatKHR *)arena_alloc(
                        scratch,
                        surface_format_count * sizeof(VkSurfaceFormatKHR));
                assert(surface_formats);
                vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    physical_device, surface, &surface_format_count,
                    surface_formats));

                u32 present_mode_count;
                vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &present_mode_count, 0));
                VkPresentModeKHR *present_modes =
                    (VkPresentModeKHR *)arena_alloc(
                        scratch, present_mode_count * sizeof(VkPresentModeKHR));
                assert(present_modes);
                vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    physical_device, surface, &present_mode_count,
                    present_modes));

                b8 surface_format_support = 0;
                for (u32 surface_format_index = 0;
                     surface_format_index < surface_format_count;
                     surface_format_index++) {
                    if (surface_formats[surface_format_index].format ==
                            VK_FORMAT_B8G8R8A8_UNORM &&
                        surface_formats[surface_format_index].colorSpace ==
                            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        swapchain_image_format =
                            surface_formats[surface_format_index].format;
                        image_color_space =
                            surface_formats[surface_format_index].colorSpace;
                        surface_format_support = 1;
                        break;
                    }
                }
                if (!surface_format_support) {
                    continue;
                }

                b8 present_mode_support = 0;
                for (u32 present_mode_index = 0;
                     present_mode_index < present_mode_count;
                     present_mode_index++) {
                    if (present_modes[present_mode_index] ==
                        VK_PRESENT_MODE_FIFO_KHR) {
                        present_mode_support = 1;
                        present_mode = VK_PRESENT_MODE_FIFO_KHR;
                        break;
                    }
                }
                if (!present_mode_support) {
                    continue;
                }

                if (surface_capabilities.currentExtent.width != U32_MAX) {
                    swapchain_image_extent = surface_capabilities.currentExtent;
                } else {
                    u32 client_width = client_rect.right - client_rect.left;
                    u32 client_height = client_rect.bottom - client_rect.top;

                    if (client_width <
                        surface_capabilities.minImageExtent.width) {
                        swapchain_image_extent.width =
                            surface_capabilities.minImageExtent.width;
                    } else if (client_width >
                                   surface_capabilities.minImageExtent.width &&
                               client_width <
                                   surface_capabilities.maxImageExtent.width) {
                        swapchain_image_extent.width = client_width;
                    } else {
                        swapchain_image_extent.width =
                            surface_capabilities.maxImageExtent.width;
                    }

                    if (client_height <
                        surface_capabilities.minImageExtent.height) {
                        swapchain_image_extent.height =
                            surface_capabilities.minImageExtent.height;
                    } else if (client_height >
                                   surface_capabilities.minImageExtent.height &&
                               client_height <
                                   surface_capabilities.maxImageExtent.height) {
                        swapchain_image_extent.height = client_height;
                    } else {
                        swapchain_image_extent.height =
                            surface_capabilities.maxImageExtent.height;
                    }
                }

                image_count = surface_capabilities.minImageCount;
                surface_transform = surface_capabilities.currentTransform;

                // Device feature check
                VkPhysicalDeviceProperties2 properties;
                properties.sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                properties.pNext = 0;
                vkGetPhysicalDeviceProperties2(physical_device, &properties);

                vkGetPhysicalDeviceFeatures2(physical_device,
                                             &physical_device_features_2);
                b8 supported = 1;
                VkBaseOutStructure *next =
                    (VkBaseOutStructure *)physical_device_features_2.pNext;
                while (next) {
                    if (next->sType ==
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES) {
                        VkPhysicalDeviceVulkan13Features *features_1_3 =
                            (VkPhysicalDeviceVulkan13Features *)next;
                        if (!features_1_3->dynamicRendering ||
                            !features_1_3->synchronization2) {
                            supported = 0;
                            break;
                        }
                    } else if (
                        next->sType ==
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES) {
                        VkPhysicalDeviceVulkan12Features *features_1_2 =
                            (VkPhysicalDeviceVulkan12Features *)next;
                        if (!features_1_2->bufferDeviceAddress ||
                            !features_1_2->descriptorIndexing) {
                            supported = 0;
                            break;
                        }
                    }
                    next = (VkBaseOutStructure *)next->pNext;
                }

                if (supported) {
                    selected_physical_device = physical_device;
                    printf("Selected GPU: %s\n",
                           properties.properties.deviceName);
                    break;
                }
            }
            assert(selected_physical_device);
            arena_clear(scratch);
            VkPhysicalDeviceMemoryProperties2 memory_properties = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2};
            vkGetPhysicalDeviceMemoryProperties2(selected_physical_device,
                                                 &memory_properties);

            f32 queue_priority = 1.0f;
            VkDeviceQueueCreateInfo queue_create_info;
            queue_create_info.sType =
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.pNext = 0;
            queue_create_info.flags = 0;
            queue_create_info.queueFamilyIndex = *queue_family_index;
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &queue_priority;

            VkDeviceCreateInfo device_create_info;
            device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_create_info.pNext = (void *)&physical_device_features_2;
            device_create_info.flags = 0;
            device_create_info.queueCreateInfoCount = 1;
            device_create_info.pQueueCreateInfos = &queue_create_info;
            device_create_info.ppEnabledLayerNames = 0;
            device_create_info.pEnabledFeatures = 0;
            device_create_info.enabledExtensionCount =
                enabled_device_extension_name_count;
            device_create_info.ppEnabledExtensionNames =
                (char **)enabled_device_extension_names;
            device_create_info.enabledLayerCount = 0;

            PoneVkDevice *pone_vk_device = pone_vk_create_device(
                vkCreateDevice, selected_physical_device, &device_create_info,
                &allocation_callbacks, global_arena, vkGetDeviceProcAddr);
            VkDevice device = pone_vk_device->device;
            vk_get_device_proc_addr(device, vkDestroyDevice);
            vk_get_device_proc_addr(device, vkGetDeviceQueue2);
            vk_get_device_proc_addr(device, vkCreateSwapchainKHR);
            vk_get_device_proc_addr(device, vkGetSwapchainImagesKHR);
            vk_get_device_proc_addr(device, vkAcquireNextImageKHR);
            vk_get_device_proc_addr(device, vkCreateImage);
            vk_get_device_proc_addr(device, vkCreateImageView);
            vk_get_device_proc_addr(device, vkGetImageMemoryRequirements2);
            vk_get_device_proc_addr(device, vkBindImageMemory2);
            vk_get_device_proc_addr(device, vkDestroyImage);
            vk_get_device_proc_addr(device, vkDestroyImageView);
            vk_get_device_proc_addr(device, vkCreateBuffer);
            vk_get_device_proc_addr(device, vkGetBufferMemoryRequirements2);
            vk_get_device_proc_addr(device, vkGetBufferDeviceAddress);
            vk_get_device_proc_addr(device, vkBindBufferMemory2);
            vk_get_device_proc_addr(device, vkDestroyBuffer);
            vk_get_device_proc_addr(device, vkAllocateMemory);
            vk_get_device_proc_addr(device, vkMapMemory);
            vk_get_device_proc_addr(device, vkUnmapMemory);
            vk_get_device_proc_addr(device, vkFreeMemory);
            vk_get_device_proc_addr(device, vkCreateCommandPool);
            vk_get_device_proc_addr(device, vkAllocateCommandBuffers);
            vk_get_device_proc_addr(device, vkResetCommandBuffer);
            vk_get_device_proc_addr(device, vkBeginCommandBuffer);
            vk_get_device_proc_addr(device, vkEndCommandBuffer);
            vk_get_device_proc_addr(device, vkCreateFence);
            vk_get_device_proc_addr(device, vkCreateSemaphore);
            vk_get_device_proc_addr(device, vkWaitForFences);
            vk_get_device_proc_addr(device, vkResetFences);
            vk_get_device_proc_addr(device, vkCreateDescriptorPool);
            vk_get_device_proc_addr(device, vkResetDescriptorPool);
            vk_get_device_proc_addr(device, vkDestroyDescriptorPool);
            vk_get_device_proc_addr(device, vkCreateDescriptorSetLayout);
            vk_get_device_proc_addr(device, vkDestroyDescriptorSetLayout);
            vk_get_device_proc_addr(device, vkAllocateDescriptorSets);
            vk_get_device_proc_addr(device, vkUpdateDescriptorSets);
            vk_get_device_proc_addr(device, vkCreateShaderModule);
            vk_get_device_proc_addr(device, vkDestroyShaderModule);
            vk_get_device_proc_addr(device, vkCreatePipelineLayout);
            vk_get_device_proc_addr(device, vkDestroyPipelineLayout);
            vk_get_device_proc_addr(device, vkCreateComputePipelines);
            vk_get_device_proc_addr(device, vkCreateGraphicsPipelines);
            vk_get_device_proc_addr(device, vkDestroyPipeline);
            vk_get_device_proc_addr(device, vkCmdPipelineBarrier2);
            vk_get_device_proc_addr(device, vkCmdClearColorImage);
            vk_get_device_proc_addr(device, vkCmdBlitImage2);
            vk_get_device_proc_addr(device, vkCmdBindPipeline);
            vk_get_device_proc_addr(device, vkCmdBindDescriptorSets);
            vk_get_device_proc_addr(device, vkCmdDispatch);
            vk_get_device_proc_addr(device, vkCmdBeginRendering);
            vk_get_device_proc_addr(device, vkCmdEndRendering);
            vk_get_device_proc_addr(device, vkCmdPushConstants);
            vk_get_device_proc_addr(device, vkCmdSetViewport);
            vk_get_device_proc_addr(device, vkCmdSetScissor);
            vk_get_device_proc_addr(device, vkCmdDraw);
            vk_get_device_proc_addr(device, vkCmdCopyBuffer);
            vk_get_device_proc_addr(device, vkCmdBindIndexBuffer);
            vk_get_device_proc_addr(device, vkCmdDrawIndexed);
            vk_get_device_proc_addr(device, vkQueueSubmit2);
            vk_get_device_proc_addr(device, vkQueuePresentKHR);
            vk_get_device_proc_addr(device, vkQueueWaitIdle);
            vk_get_device_proc_addr(device, vkDeviceWaitIdle);
            vk_get_device_proc_addr(device, vkDestroySwapchainKHR);
            vk_get_device_proc_addr(device, vkDestroyCommandPool);
            vk_get_device_proc_addr(device, vkDestroyFence);
            vk_get_device_proc_addr(device, vkDestroySemaphore);

            VkSwapchainCreateInfoKHR swapchain_create_info;
            swapchain_create_info.sType =
                VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swapchain_create_info.pNext = 0;
            swapchain_create_info.flags = 0;
            swapchain_create_info.surface = surface;
            swapchain_create_info.minImageCount = image_count;
            swapchain_create_info.imageFormat = swapchain_image_format;
            swapchain_create_info.imageColorSpace = image_color_space;
            swapchain_create_info.imageExtent = swapchain_image_extent;
            swapchain_create_info.imageArrayLayers = 1;
            swapchain_create_info.imageUsage =
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchain_create_info.queueFamilyIndexCount = 1;
            swapchain_create_info.pQueueFamilyIndices = queue_family_index;
            swapchain_create_info.preTransform = surface_transform;
            swapchain_create_info.compositeAlpha =
                VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapchain_create_info.presentMode = present_mode;
            swapchain_create_info.clipped = VK_TRUE;
            swapchain_create_info.oldSwapchain = 0;

            VkSwapchainKHR swapchain = pone_vk_create_swapchain_khr(
                pone_vk_device, &swapchain_create_info);

            u32 swapchain_image_count;
            VkImage *swapchain_images;
            pone_vk_get_swapchain_images_khr(
                pone_vk_device, swapchain, &swapchain_images,
                &swapchain_image_count, global_arena);

            VkImageView *swapchain_image_views = arena_alloc_array(
                global_arena, swapchain_image_count, VkImageView);

            for (usize swapchain_image_index = 0;
                 swapchain_image_index < swapchain_image_count;
                 swapchain_image_index++) {
                VkImageView *swapchain_image_view =
                    &swapchain_image_views[swapchain_image_index];
                VkImage swapchain_image =
                    swapchain_images[swapchain_image_index];

                VkImageViewCreateInfo swapchain_image_view_create_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .pNext = 0,
                    .flags = 0,
                    .image = swapchain_image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = swapchain_image_format,
                    .components =
                        (VkComponentMapping){
                            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                        },
                    .subresourceRange = (VkImageSubresourceRange){
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    }};

                pone_vk_create_image_view(pone_vk_device,
                                          &swapchain_image_view_create_info,
                                          swapchain_image_view);
            }

            VkExtent2D draw_extent = {
                .width = swapchain_image_extent.width,
                .height = swapchain_image_extent.height,
            };
            VkFormat draw_format = VK_FORMAT_R16G16B16A16_SFLOAT;
            VkImageCreateInfo draw_image_create_info;
            draw_image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            draw_image_create_info.pNext = 0;
            draw_image_create_info.flags = 0;
            draw_image_create_info.imageType = VK_IMAGE_TYPE_2D;
            draw_image_create_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
            draw_image_create_info.extent = (VkExtent3D){
                .width = swapchain_image_extent.width,
                .height = swapchain_image_extent.height,
                .depth = 1,
            };
            draw_image_create_info.mipLevels = 1;
            draw_image_create_info.arrayLayers = 1;
            draw_image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
            draw_image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            draw_image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                           VK_IMAGE_USAGE_STORAGE_BIT |
                                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            draw_image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            draw_image_create_info.queueFamilyIndexCount = 0;
            draw_image_create_info.pQueueFamilyIndices = 0;
            draw_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            PoneVkImage draw_image;
            pone_vk_create_image(pone_vk_device, &draw_image_create_info,
                                 &draw_image);
            VkMemoryRequirements2 draw_image_memory_requirements =
                pone_vk_get_image_memory_requirements_2(pone_vk_device,
                                                        &draw_image);

            VkImageCreateInfo depth_image_create_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_D32_SFLOAT,
                .extent = draw_image.extent,
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .queueFamilyIndexCount = 0,
                .pQueueFamilyIndices = 0,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            };
            PoneVkImage depth_image;
            pone_vk_create_image(pone_vk_device, &depth_image_create_info,
                                 &depth_image);
            VkMemoryRequirements2 depth_image_memory_requirements =
                pone_vk_get_image_memory_requirements_2(pone_vk_device,
                                                        &depth_image);

            usize vertex_buffer_size = meshes[2].vertex_count * sizeof(Vertex);
            VkBuffer vertex_buffer = pone_vk_create_buffer(
                pone_vk_device, vertex_buffer_size,
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
            VkMemoryRequirements2 vertex_buffer_memory_requirements =
                pone_vk_get_buffer_memory_requirements_2(pone_vk_device,
                                                         vertex_buffer);

            usize index_buffer_size = meshes[2].index_count * sizeof(u16);
            VkBuffer index_buffer =
                pone_vk_create_buffer(pone_vk_device, index_buffer_size,
                                      VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            VkMemoryRequirements2 index_buffer_memory_requirements =
                pone_vk_get_buffer_memory_requirements_2(pone_vk_device,
                                                         index_buffer);

            VkBuffer staging_buffer = pone_vk_create_buffer(
                pone_vk_device, vertex_buffer_size + index_buffer_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            VkMemoryRequirements2 staging_buffer_memory_requirements =
                pone_vk_get_buffer_memory_requirements_2(pone_vk_device,
                                                         staging_buffer);

            u32 memory_type_index;
            pone_assert(find_memory_type_index(
                draw_image_memory_requirements.memoryRequirements
                        .memoryTypeBits &
                    vertex_buffer_memory_requirements.memoryRequirements
                        .memoryTypeBits &
                    index_buffer_memory_requirements.memoryRequirements
                        .memoryTypeBits &
                    depth_image_memory_requirements.memoryRequirements
                        .memoryTypeBits,
                &memory_properties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &memory_type_index));
            VkMemoryAllocateFlagsInfo memory_allocate_flags_info = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
                .pNext = 0,
                .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
                .deviceMask = 0,
            };
            PoneVkMemoryAllocateInfo memory_allocate_info = {
                .p_next = (void *)&memory_allocate_flags_info,
                .allocation_size =
                    draw_image_memory_requirements.memoryRequirements.size +
                    depth_image_memory_requirements.memoryRequirements.size +
                    vertex_buffer_memory_requirements.memoryRequirements.size +
                    index_buffer_memory_requirements.memoryRequirements.size,
                .memory_type_index = memory_type_index,
            };
            VkDeviceMemory device_memory =
                pone_vk_allocate_memory(pone_vk_device, &memory_allocate_info);

            pone_assert(
                find_memory_type_index(staging_buffer_memory_requirements
                                           .memoryRequirements.memoryTypeBits,
                                       &memory_properties,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                       &memory_type_index));
            memory_allocate_info.p_next = 0;
            memory_allocate_info.allocation_size =
                staging_buffer_memory_requirements.memoryRequirements.size;
            memory_allocate_info.memory_type_index = memory_type_index;
            VkDeviceMemory staging_device_memory =
                pone_vk_allocate_memory(pone_vk_device, &memory_allocate_info);

            VkBindImageMemoryInfo bind_image_memory_infos[2] = {
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                    .pNext = 0,
                    .image = draw_image.image,
                    .memory = device_memory,
                    .memoryOffset = 0,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO,
                    .pNext = 0,
                    .image = depth_image.image,
                    .memory = device_memory,
                    .memoryOffset =
                        draw_image_memory_requirements.memoryRequirements.size,
                },
            };
            pone_vk_bind_image_memory_2(pone_vk_device, 2,
                                        bind_image_memory_infos);

            VkBindBufferMemoryInfo bind_buffer_memory_infos[3] = {
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                    .pNext = 0,
                    .buffer = vertex_buffer,
                    .memory = device_memory,
                    .memoryOffset =
                        draw_image_memory_requirements.memoryRequirements.size +
                        depth_image_memory_requirements.memoryRequirements.size,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                    .pNext = 0,
                    .buffer = index_buffer,
                    .memory = device_memory,
                    .memoryOffset =
                        draw_image_memory_requirements.memoryRequirements.size +
                        depth_image_memory_requirements.memoryRequirements
                            .size +
                        vertex_buffer_memory_requirements.memoryRequirements
                            .size,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO,
                    .pNext = 0,
                    .buffer = staging_buffer,
                    .memory = staging_device_memory,
                    .memoryOffset = 0,
                },
            };
            pone_vk_bind_buffer_memory_2(pone_vk_device, 3,
                                         bind_buffer_memory_infos);

            VkDeviceAddress vertex_buffer_device_address =
                pone_vk_get_buffer_device_address(pone_vk_device,
                                                  vertex_buffer);

            void *staging_data;
            pone_vk_map_memory(
                pone_vk_device, staging_device_memory, 0,
                staging_buffer_memory_requirements.memoryRequirements.size, 0,
                &staging_data);
            pone_memcpy(staging_data, (void *)meshes[2].vertices,
                        vertex_buffer_size);
            pone_memcpy((void *)((u8 *)staging_data + vertex_buffer_size),
                        (void *)meshes[2].indices, index_buffer_size);

            VkImageViewCreateInfo draw_image_view_create_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .image = draw_image.image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = draw_format,
                .components =
                    (VkComponentMapping){
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                .subresourceRange = (VkImageSubresourceRange){
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }};

            VkImageView draw_image_view;
            pone_vk_create_image_view(
                pone_vk_device, &draw_image_view_create_info, &draw_image_view);

            VkImageViewCreateInfo depth_image_view_create_info = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .image = depth_image.image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = depth_image.format,
                .components =
                    (VkComponentMapping){
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                .subresourceRange = (VkImageSubresourceRange){
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                }};

            VkImageView depth_image_view;
            pone_vk_create_image_view(pone_vk_device,
                                      &depth_image_view_create_info,
                                      &depth_image_view);

            VkDeviceQueueInfo2 queue_info;
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
            queue_info.pNext = 0;
            queue_info.flags = 0;
            queue_info.queueFamilyIndex = *queue_family_index;
            queue_info.queueIndex = 0;

            PoneVkQueue *pone_vk_queue = pone_vk_get_device_queue_2(
                pone_vk_device, &queue_info, global_arena);
            VkQueue queue = pone_vk_queue->queue;

            VkCommandPoolCreateInfo command_pool_create_info;
            command_pool_create_info.sType =
                VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            command_pool_create_info.pNext = 0;
            command_pool_create_info.flags =
                VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            command_pool_create_info.queueFamilyIndex = *queue_family_index;

            usize command_pool_count = FRAME_OVERLAP;
            VkCommandPool *command_pools = (VkCommandPool *)arena_alloc(
                global_arena, command_pool_count * sizeof(VkCommandPool));
            assert(command_pools);

            usize command_buffer_count = FRAME_OVERLAP;
            VkCommandBuffer *command_buffers = (VkCommandBuffer *)arena_alloc(
                global_arena, command_buffer_count * sizeof(VkCommandBuffer));
            assert(command_buffers);

            PoneVkCommandBuffer *pone_vk_command_buffers = arena_alloc_array(
                global_arena, command_buffer_count, PoneVkCommandBuffer);

            VkFenceCreateInfo fence_create_info;
            fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence_create_info.pNext = 0;
            fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

            usize fence_count = FRAME_OVERLAP;
            VkFence *fences = (VkFence *)arena_alloc(
                global_arena, fence_count * sizeof(VkFence));
            assert(fences);

            VkSemaphoreCreateInfo semaphore_create_info;
            semaphore_create_info.sType =
                VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            semaphore_create_info.pNext = 0;
            semaphore_create_info.flags = 0;

            usize swapchain_semaphore_count = FRAME_OVERLAP;
            VkSemaphore *swapchain_semaphores = (VkSemaphore *)arena_alloc(
                global_arena, swapchain_semaphore_count * sizeof(VkSemaphore));
            assert(swapchain_semaphores);

            usize render_semaphore_count = FRAME_OVERLAP;
            VkSemaphore *render_semaphores = (VkSemaphore *)arena_alloc(
                global_arena, render_semaphore_count * sizeof(VkSemaphore));
            assert(render_semaphores);

            for (usize i = 0; i < command_pool_count; i++) {
                vk_check(vkCreateCommandPool(device, &command_pool_create_info,
                                             &allocation_callbacks,
                                             &command_pools[i]));

                VkCommandBufferAllocateInfo command_buffer_allocate_info;
                command_buffer_allocate_info.sType =
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                command_buffer_allocate_info.pNext = 0;
                command_buffer_allocate_info.commandPool = command_pools[i];
                command_buffer_allocate_info.level =
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                command_buffer_allocate_info.commandBufferCount = 1;

                pone_vk_allocate_command_buffers(
                    pone_vk_device, &command_buffer_allocate_info,
                    &pone_vk_command_buffers[i], scratch);
                command_buffers[i] = pone_vk_command_buffers[i].command_buffer;

                vk_check(vkCreateFence(device, &fence_create_info,
                                       &allocation_callbacks, &fences[i]));
                vk_check(vkCreateSemaphore(device, &semaphore_create_info,
                                           &allocation_callbacks,
                                           &swapchain_semaphores[i]));
                vk_check(vkCreateSemaphore(device, &semaphore_create_info,
                                           &allocation_callbacks,
                                           &render_semaphores[i]));
            }
            VkCommandBufferBeginInfo begin_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .pNext = 0,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                .pInheritanceInfo = 0,
            };
            pone_vk_begin_command_buffer(&pone_vk_command_buffers[0],
                                         &begin_info);
            VkBufferCopy vertex_copy = {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = vertex_buffer_size,
            };
            pone_vk_cmd_copy_buffer(&pone_vk_command_buffers[0], staging_buffer,
                                    vertex_buffer, 1, &vertex_copy);

            VkBufferCopy index_copy = {
                .srcOffset = vertex_buffer_size,
                .dstOffset = 0,
                .size = index_buffer_size,
            };
            pone_vk_cmd_copy_buffer(&pone_vk_command_buffers[0], staging_buffer,
                                    index_buffer, 1, &index_copy);
            pone_vk_end_command_buffer(&pone_vk_command_buffers[0]);

            VkCommandBufferSubmitInfo command_buffer_submit_info;
            command_buffer_submit_info.sType =
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext = 0;
            command_buffer_submit_info.commandBuffer = command_buffers[0];
            command_buffer_submit_info.deviceMask = 0;

            VkSubmitInfo2 submit_info = {
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = 0,
                .flags = 0,
                .waitSemaphoreInfoCount = 0,
                .pWaitSemaphoreInfos = 0,
                .commandBufferInfoCount = 1,
                .pCommandBufferInfos = &command_buffer_submit_info,
                .signalSemaphoreInfoCount = 0,
                .pSignalSemaphoreInfos = 0,
            };
            pone_vk_queue_submit_2(pone_vk_queue, 1, &submit_info, 0);
            pone_vk_queue_wait_idle(pone_vk_queue);

            pone_vk_free_memory(pone_vk_device, staging_device_memory);
            pone_vk_destroy_buffer(pone_vk_device, staging_buffer);

            VkDescriptorPoolSize descriptor_pool_sizes[2] = {
                {
                    .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .descriptorCount = 10,
                },
                {
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount =
                        IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
                },
            };
            u32 max_sets = 0;
            for (usize i = 0; i < 2; i++) {
                max_sets += descriptor_pool_sizes[i].descriptorCount;
            }

            VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .maxSets = max_sets,
                .poolSizeCount = 2,
                .pPoolSizes = descriptor_pool_sizes,
            };

            VkDescriptorPool descriptor_pool;
            vk_check(vkCreateDescriptorPool(
                device, &descriptor_pool_create_info, &allocation_callbacks,
                &descriptor_pool));

            VkDescriptorSetLayoutBinding descriptor_set_layout_binding = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .pImmutableSamplers = 0,
            };
            VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info =
                {
                    .sType =
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .pNext = 0,
                    .flags = 0,
                    .bindingCount = 1,
                    .pBindings = &descriptor_set_layout_binding,
                };
            VkDescriptorSetLayout descriptor_set_layout;
            vk_check(vkCreateDescriptorSetLayout(
                device, &descriptor_set_layout_create_info,
                &allocation_callbacks, &descriptor_set_layout));

            VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext = 0,
                .descriptorPool = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &descriptor_set_layout,
            };

            VkDescriptorSet descriptor_sets;
            vk_check(vkAllocateDescriptorSets(
                device, &descriptor_set_allocate_info, &descriptor_sets));

            VkDescriptorImageInfo descriptor_image_info = {
                .sampler = 0,
                .imageView = draw_image_view,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };

            VkWriteDescriptorSet draw_image_write_descriptor_set = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext = 0,
                .dstSet = descriptor_sets,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .pImageInfo = &descriptor_image_info,
                .pBufferInfo = 0,
                .pTexelBufferView = 0,
            };

            vkUpdateDescriptorSets(device, 1, &draw_image_write_descriptor_set,
                                   0, 0);

            VkShaderModule gradient_shader;
            load_shader_module(scratch, "shaders/gradient.spv", device,
                               &allocation_callbacks, vkCreateShaderModule,
                               &gradient_shader);

            VkShaderModule sky_shader;
            load_shader_module(scratch, "shaders/sky.spv", device,
                               &allocation_callbacks, vkCreateShaderModule,
                               &sky_shader);

            VkPushConstantRange push_contant_range = {
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(ComputePushConstants),
            };

            VkPipelineLayoutCreateInfo compute_layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .setLayoutCount = 1,
                .pSetLayouts = &descriptor_set_layout,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_contant_range,
            };
            VkPipelineLayout compute_layout;
            vk_check(vkCreatePipelineLayout(device, &compute_layout_create_info,
                                            &allocation_callbacks,
                                            &compute_layout));

            VkPipelineShaderStageCreateInfo gradient_shader_stage_create_info =
                {
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext = 0,
                    .flags = 0,
                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                    .module = gradient_shader,
                    .pName = "main",
                    .pSpecializationInfo = 0,
                };
            VkPipelineShaderStageCreateInfo sky_shader_stage_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = sky_shader,
                .pName = "main",
                .pSpecializationInfo = 0,
            };

            VkComputePipelineCreateInfo *compute_pipeline_create_infos =
                (VkComputePipelineCreateInfo *)arena_alloc(
                    global_arena, 2 * sizeof(VkComputePipelineCreateInfo));
            assert(compute_pipeline_create_infos);
            compute_pipeline_create_infos[0] = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .stage = gradient_shader_stage_create_info,
                .layout = compute_layout,
                .basePipelineHandle = 0,
                .basePipelineIndex = 0,
            };
            compute_pipeline_create_infos[1] = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .stage = sky_shader_stage_create_info,
                .layout = compute_layout,
                .basePipelineHandle = 0,
                .basePipelineIndex = 0,
            };
            VkPipeline *pipelines =
                (VkPipeline *)arena_alloc(global_arena, 2 * sizeof(VkPipeline));
            assert(pipelines);

            vk_check(vkCreateComputePipelines(
                device, 0, 2, compute_pipeline_create_infos,
                &allocation_callbacks, pipelines));

            VkShaderModule triangle_vertex_shader;
            load_shader_module(global_arena,
                               "shaders/colored_triangle_mesh.vert.spv", device,
                               &allocation_callbacks, vkCreateShaderModule,
                               &triangle_vertex_shader);
            VkShaderModule triangle_fragment_shader;
            load_shader_module(global_arena,
                               "shaders/colored_triangle.frag.spv", device,
                               &allocation_callbacks, vkCreateShaderModule,
                               &triangle_fragment_shader);
            VkPipelineShaderStageCreateInfo graphics_pipeline_shader_stages[2] =
                {{
                     .sType =
                         VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     .pNext = 0,
                     .flags = 0,
                     .stage = VK_SHADER_STAGE_VERTEX_BIT,
                     .module = triangle_vertex_shader,
                     .pName = "main",
                     .pSpecializationInfo = 0,
                 },
                 {
                     .sType =
                         VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                     .pNext = 0,
                     .flags = 0,
                     .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                     .module = triangle_fragment_shader,
                     .pName = "main",
                     .pSpecializationInfo = 0,
                 }};

            push_contant_range = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                .offset = 0,
                .size = sizeof(DrawPushConstants),
            };
            VkPipelineLayoutCreateInfo graphics_pipeline_layout_create_info = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext = 0,
                .flags = 0,
                .setLayoutCount = 0,
                .pSetLayouts = 0,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges = &push_contant_range,
            };
            VkPipelineLayout graphics_pipeline_layout;
            vk_check(vkCreatePipelineLayout(
                device, &graphics_pipeline_layout_create_info,
                &allocation_callbacks, &graphics_pipeline_layout));

            PipelineBuilder pipeline_builder;
            pipeline_builder_clear(&pipeline_builder);
            pipeline_builder.layout = graphics_pipeline_layout;
            pipeline_builder_set_shaders(&pipeline_builder, 2,
                                         graphics_pipeline_shader_stages);
            pipeline_builder_set_input_topology(
                &pipeline_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
            pipeline_builder_set_polygon_mode(&pipeline_builder,
                                              VK_POLYGON_MODE_FILL);
            pipeline_builder_set_cull_mode(&pipeline_builder, VK_CULL_MODE_NONE,
                                           VK_FRONT_FACE_CLOCKWISE);
            pipeline_builder_set_multisampling_none(&pipeline_builder);
            pipeline_builder_disable_blending(&pipeline_builder);
            pipeline_builder_enable_depth_test(&pipeline_builder, 1,
                                               VK_COMPARE_OP_GREATER_OR_EQUAL);
            pipeline_builder_set_color_attachment_format(&pipeline_builder,
                                                         draw_format);
            pipeline_builder_set_depth_format(&pipeline_builder,
                                              depth_image.format);
            VkPipeline graphics_pipeline = pipeline_builder_build(
                &pipeline_builder, vkCreateGraphicsPipelines,
                &allocation_callbacks, device);

            ComputePushConstants background_effects[2] = {
                // Gradient
                {
                    .data1 = {.x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f},
                    .data2 = {.x = 0.0f, .y = 0.0f, .z = 1.0f, .w = 1.0f},
                    .data3 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f},
                    .data4 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f},
                },
                // Sky
                {
                    .data1 = {.x = 0.1f, .y = 0.2f, .z = 0.4f, .w = 0.97f},
                    .data2 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f},
                    .data3 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f},
                    .data4 = {.x = 0.0f, .y = 0.0f, .z = 0.0f, .w = 0.0f},
                }};

            vkDestroyShaderModule(device, triangle_fragment_shader,
                                  &allocation_callbacks);
            vkDestroyShaderModule(device, triangle_vertex_shader,
                                  &allocation_callbacks);
            vkDestroyShaderModule(device, sky_shader, &allocation_callbacks);
            vkDestroyShaderModule(device, gradient_shader,
                                  &allocation_callbacks);
            arena_clear(scratch);

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(hwnd);
            VulkanFnDispatcher dispatcher = {
                .instance = instance,
                .device = device,
                .vk_get_instance_proc_addr = _vk_get_instance_proc_addr,
                .vk_get_device_proc_addr = vkGetDeviceProcAddr,
            };
            ImGui_ImplVulkan_LoadFunctions(
                VK_API_VERSION_1_4, imgui_vulkan_loader, (void *)&dispatcher);
            ImGui_ImplVulkan_InitInfo init_info = {};
            init_info.ApiVersion =
                VK_API_VERSION_1_4; // Pass
                                    // in your value of
                                    // VkApplicationInfo::apiVersion, otherwise
                                    // will default to header version.
            init_info.Instance = instance;
            init_info.PhysicalDevice = selected_physical_device;
            init_info.Device = device;
            init_info.QueueFamily = *queue_family_index;
            init_info.Queue = queue;
            init_info.DescriptorPool = descriptor_pool;
            init_info.MinImageCount = 3;
            init_info.ImageCount = 3;
            init_info.Allocator = &allocation_callbacks;
            init_info.CheckVkResultFn = imgui_check_vk_result;
            init_info.UseDynamicRendering = 1;
            init_info.PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapchain_image_format,
            };
            init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            ImGui_ImplVulkan_Init(&init_info);

            arena_destroy(&scratch);
            u32 frame_counter = 0;
            i32 current_background_effect = 0;
            while (global_running) {
                u32 frame_index = frame_counter & 1;
                MSG message;
                while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessageA(&message);
                }

                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                if (ImGui::Begin("background")) {
                    ComputePushConstants *selected_background_effect =
                        &background_effects[current_background_effect];
                    ImGui::SliderInt("Effect Index", &current_background_effect,
                                     0, 1);
                    ImGui::SliderFloat4(
                        "data1", (float *)&selected_background_effect->data1,
                        0.0f, 1.0f);
                    ImGui::SliderFloat4(
                        "data2", (float *)&selected_background_effect->data2,
                        0.0f, 1.0f);
                    ImGui::SliderFloat4(
                        "data3", (float *)&selected_background_effect->data3,
                        0.0f, 1.0f);
                    ImGui::SliderFloat4(
                        "data4", (float *)&selected_background_effect->data4,
                        0.0f, 1.0f);
                }
                ImGui::End();
                ImGui::Render();

                VkFence *render_fence = &fences[frame_index];
                VkSemaphore swapchain_semaphore =
                    swapchain_semaphores[frame_index];
                VkSemaphore render_semaphore = render_semaphores[frame_index];
                VkCommandBuffer command_buffer = command_buffers[frame_index];
                VkPipeline pipeline = pipelines[current_background_effect];
                ComputePushConstants *background_effect =
                    &background_effects[current_background_effect];

                VkResult res;
                res = vkWaitForFences(device, 1, render_fence, 1, 1000000000);
                vk_check(res);
                res = vkResetFences(device, 1, render_fence);
                vk_check(res);

                u32 swapchain_image_index;
                vk_check(vkAcquireNextImageKHR(device, swapchain, 1000000000,
                                               swapchain_semaphore, 0,
                                               &swapchain_image_index));
                VkImage swapchain_image =
                    swapchain_images[swapchain_image_index];
                VkImageView swapchain_image_view =
                    swapchain_image_views[swapchain_image_index];

                VkCommandBufferBeginInfo command_buffer_begin_info;
                command_buffer_begin_info.sType =
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                command_buffer_begin_info.pNext = 0;
                command_buffer_begin_info.flags =
                    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                command_buffer_begin_info.pInheritanceInfo = 0;

                vk_check(vkResetCommandBuffer(command_buffer, 0));
                vk_check(vkBeginCommandBuffer(command_buffer,
                                              &command_buffer_begin_info));

                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 draw_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_GENERAL);

                // VkClearColorValue clear_value;
                // f32 flash = fabsf(sinf((f32)frame_counter / 120.0f));
                // clear_value = {{0.0f, 0.0f, flash, 1.0f}};
                //
                // VkImageSubresourceRange clear_range;
                // clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                // clear_range.baseMipLevel = 0;
                // clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
                // clear_range.baseArrayLayer = 0;
                // clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
                //
                // vkCmdClearColorImage(command_buffer, draw_image,
                //                      VK_IMAGE_LAYOUT_GENERAL,
                //                      &clear_value, 1, &clear_range);

                vkCmdBindPipeline(command_buffer,
                                  VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                vkCmdBindDescriptorSets(
                    command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                    compute_layout, 0, 1, &descriptor_sets, 0, 0);

                vkCmdPushConstants(
                    command_buffer, compute_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                    0, sizeof(ComputePushConstants), (void *)background_effect);

                vkCmdDispatch(command_buffer,
                              ceilf((f32)draw_extent.width / 16.0f),
                              ceilf((f32)draw_extent.height / 16.0f), 1);

                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 draw_image.image, VK_IMAGE_LAYOUT_GENERAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 depth_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

                VkRenderingAttachmentInfo color_attachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = 0,
                    .imageView = draw_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode = VK_RESOLVE_MODE_NONE,
                    .resolveImageView = 0,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {.float32 = {0.0f, 0.0f, 0.0f,
                                                         0.0f}}},
                };
                VkRenderingAttachmentInfo depth_attachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = 0,
                    .imageView = depth_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .resolveMode = VK_RESOLVE_MODE_NONE,
                    .resolveImageView = 0,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.depthStencil = {.depth = 0.0f,
                                                    .stencil = 0}},
                };
                VkRenderingInfo rendering_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext = 0,
                    .flags = 0,
                    .renderArea =
                        {
                            .offset = {0, 0},
                            .extent = draw_extent,
                        },
                    .layerCount = 1,
                    .viewMask = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &color_attachment,
                    .pDepthAttachment = &depth_attachment,
                    .pStencilAttachment = 0,
                };
                vkCmdBeginRendering(command_buffer, &rendering_info);
                vkCmdBindPipeline(command_buffer,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  graphics_pipeline);
                VkViewport viewport = {
                    .x = 0,
                    .y = 0,
                    .width = (float)draw_extent.width,
                    .height = (float)draw_extent.height,
                    .minDepth = 0.0f,
                    .maxDepth = 1.0f,
                };
                vkCmdSetViewport(command_buffer, 0, 1, &viewport);

                VkRect2D scissor = {
                    .offset =
                        {
                            .x = 0,
                            .y = 0,
                        },
                    .extent = draw_extent,
                };
                vkCmdSetScissor(command_buffer, 0, 1, &scissor);

                Mat4 view = {
                    .data = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -5.0f, 1.0f},
                };
                Mat4 proj = pone_mat4_perspective(
                    70.0f, (f32)draw_extent.width / (f32)draw_extent.height,
                    10000.f, 0.1f);
                f32 *proj_y = pone_mat4_get(&proj, 1, 1);
                *proj_y *= -1.0f;

                DrawPushConstants draw_push_constants = {
                    .world_matrix = pone_mat4_mul(&proj, &view),
                    // .data = {
                    //   1.0f, 0.0f, 0.0f, 0.0f,
                    //   0.0f, -1.0f, 0.0f, 0.0f,
                    //   0.0f, 0.0f, 1.0f, 0.0f,
                    //   0.0f, 0.0f, 0.0f, 1.0f
                    // }
                    .vertex_buffer_addr = vertex_buffer_device_address,
                };
                vkCmdPushConstants(command_buffer, graphics_pipeline_layout,
                                   VK_SHADER_STAGE_VERTEX_BIT, 0,
                                   sizeof(DrawPushConstants),
                                   (void *)&draw_push_constants);
                vkCmdBindIndexBuffer(command_buffer, index_buffer, 0,
                                     VK_INDEX_TYPE_UINT16);
                vkCmdDrawIndexed(command_buffer, meshes[2].index_count, 1, 0, 0,
                                 0);

                vkCmdEndRendering(command_buffer);

                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 draw_image.image,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 swapchain_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                copy_image_to_image(command_buffer, vkCmdBlitImage2,
                                    draw_image.image, draw_extent,
                                    swapchain_image, swapchain_image_extent);

                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 swapchain_image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                VkRenderingAttachmentInfo imgui_color_attachment = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .pNext = 0,
                    .imageView = swapchain_image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .resolveMode = VK_RESOLVE_MODE_NONE,
                    .resolveImageView = 0,
                    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {.color = {.float32 = {0.0f, 0.0f, 0.0f,
                                                         0.0f}}},
                };
                VkRenderingInfo imgui_rendering_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .pNext = 0,
                    .flags = 0,
                    .renderArea =
                        {
                            .offset = {0, 0},
                            .extent = swapchain_image_extent,
                        },
                    .layerCount = 1,
                    .viewMask = 0,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &imgui_color_attachment,
                    .pDepthAttachment = 0,
                    .pStencilAttachment = 0,
                };
                vkCmdBeginRendering(command_buffer, &imgui_rendering_info);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),
                                                command_buffer);
                vkCmdEndRendering(command_buffer);

                transition_image(command_buffer, vkCmdPipelineBarrier2,
                                 swapchain_image,
                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

                vk_check(vkEndCommandBuffer(command_buffer));

                VkCommandBufferSubmitInfo command_buffer_submit_info;
                command_buffer_submit_info.sType =
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                command_buffer_submit_info.pNext = 0;
                command_buffer_submit_info.commandBuffer = command_buffer;
                command_buffer_submit_info.deviceMask = 0;

                VkSemaphoreSubmitInfo wait_semaphore_submit_info;
                wait_semaphore_submit_info.sType =
                    VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                wait_semaphore_submit_info.pNext = 0;
                wait_semaphore_submit_info.semaphore = swapchain_semaphore;
                wait_semaphore_submit_info.value = 1;
                wait_semaphore_submit_info.stageMask =
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                wait_semaphore_submit_info.deviceIndex = 0;

                VkSemaphoreSubmitInfo signal_semaphore_submit_info;
                signal_semaphore_submit_info.sType =
                    VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                signal_semaphore_submit_info.pNext = 0;
                signal_semaphore_submit_info.semaphore = render_semaphore;
                signal_semaphore_submit_info.value = 1;
                signal_semaphore_submit_info.stageMask =
                    VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
                signal_semaphore_submit_info.deviceIndex = 0;

                VkSubmitInfo2 submit_info;
                submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                submit_info.pNext = 0;
                submit_info.flags = 0;
                submit_info.waitSemaphoreInfoCount = 1;
                submit_info.pWaitSemaphoreInfos = &wait_semaphore_submit_info;
                submit_info.commandBufferInfoCount = 1;
                submit_info.pCommandBufferInfos = &command_buffer_submit_info;
                submit_info.signalSemaphoreInfoCount = 1;
                submit_info.pSignalSemaphoreInfos =
                    &signal_semaphore_submit_info;

                vk_check(vkQueueSubmit2(queue, 1, &submit_info, *render_fence));

                VkPresentInfoKHR present_info;
                present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                present_info.pNext = 0;
                present_info.waitSemaphoreCount = 1;
                present_info.pWaitSemaphores = &render_semaphore;
                present_info.swapchainCount = 1;
                present_info.pSwapchains = &swapchain;
                present_info.pImageIndices = &swapchain_image_index;
                present_info.pResults = 0;

                vk_check(vkQueuePresentKHR(queue, &present_info));

                frame_counter++;
            }

            vk_check(vkDeviceWaitIdle(device));

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplWin32_Shutdown();

            vkDestroyPipeline(device, graphics_pipeline, &allocation_callbacks);
            vkDestroyPipelineLayout(device, graphics_pipeline_layout,
                                    &allocation_callbacks);
            for (usize i = 0; i < 2; i++) {
                vkDestroyPipeline(device, pipelines[i], &allocation_callbacks);
            }
            vkDestroyPipelineLayout(device, compute_layout,
                                    &allocation_callbacks);
            vkDestroyDescriptorSetLayout(device, descriptor_set_layout,
                                         &allocation_callbacks);
            vkDestroyDescriptorPool(device, descriptor_pool,
                                    &allocation_callbacks);

            for (usize i = 0; i < render_semaphore_count; i++) {
                vkDestroySemaphore(device, render_semaphores[i],
                                   &allocation_callbacks);
            }

            for (usize i = 0; i < swapchain_semaphore_count; i++) {
                vkDestroySemaphore(device, swapchain_semaphores[i],
                                   &allocation_callbacks);
            }

            for (usize i = 0; i < fence_count; i++) {
                vkDestroyFence(device, fences[i], &allocation_callbacks);
            }

            for (usize i = 0; i < command_pool_count; i++) {
                vkDestroyCommandPool(device, command_pools[i],
                                     &allocation_callbacks);
            }
            vkDestroyImageView(device, draw_image_view, &allocation_callbacks);
            vkDestroyImageView(device, depth_image_view, &allocation_callbacks);
            pone_vk_free_memory(pone_vk_device, device_memory);
            pone_vk_destroy_buffer(pone_vk_device, index_buffer);
            pone_vk_destroy_buffer(pone_vk_device, vertex_buffer);
            pone_vk_destroy_image(pone_vk_device, &depth_image);
            pone_vk_destroy_image(pone_vk_device, &draw_image);
            for (usize swapchain_image_index = 0;
                 swapchain_image_index < swapchain_image_count;
                 swapchain_image_index++) {
                VkImageView swapchain_image_view =
                    swapchain_image_views[swapchain_image_index];
                vkDestroyImageView(device, swapchain_image_view,
                                   &allocation_callbacks);
            }
            vkDestroySwapchainKHR(device, swapchain, &allocation_callbacks);
            vkDestroyDevice(device, &allocation_callbacks);
            vkDestroySurfaceKHR(instance, surface, &allocation_callbacks);
            vkDestroyDebugUtilsMessengerEXT(instance, messenger,
                                            &allocation_callbacks);
            vkDestroyInstance(instance, &allocation_callbacks);
            arena_destroy(&global_arena);

            return 0;
        } else {
            return 1;
        }
    } else {
        return 1;
    }
}
#endif
