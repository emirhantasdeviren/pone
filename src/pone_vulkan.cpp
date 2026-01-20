#include "pone_vulkan.h"
#include "pone_assert.h"
#include "pone_memory.h"
#include <dlfcn.h>
#include <unistd.h>

#define pone_vk_get_instance_proc_addr(instance, fp, var, fn)                  \
    var = (PFN_##fp)(fn)((instance), #fp);                                     \
    pone_assert(var)
#define pone_vk_get_device_proc_addr(device, fp, var, fn)                      \
    var = (PFN_##fp)(fn)((device), #fp);                                       \
    pone_assert(var)

static void pone_vk_command_buffer_dispatch_init(
    PoneVkCommandBufferDispatch *dispatch, VkDevice device,
    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr) {
    pone_vk_get_device_proc_addr(device, vkBeginCommandBuffer,
                                 dispatch->vk_begin_command_buffer,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkEndCommandBuffer,
                                 dispatch->vk_end_command_buffer,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdCopyBuffer,
                                 dispatch->vk_cmd_copy_buffer,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdPipelineBarrier2,
                                 dispatch->vk_cmd_pipeline_barrier_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdClearColorImage,
                                 dispatch->vk_cmd_clear_color_image,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdCopyBufferToImage2,
                                 dispatch->vk_cmd_copy_buffer_to_image_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdCopyBuffer2,
                                 dispatch->vk_cmd_copy_buffer_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdBeginRendering,
                                 dispatch->vk_cmd_begin_rendering,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdBindPipeline,
                                 dispatch->vk_cmd_bind_pipeline,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdSetViewport,
                                 dispatch->vk_cmd_set_viewport,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCmdSetScissor,
                                 dispatch->vk_cmd_set_scissor,
                                 vk_get_device_proc_addr);
}

static void
pone_vk_device_dispatch_init(PoneVkDeviceDispatch *dispatch, VkDevice device,
                             PFN_vkGetDeviceProcAddr vk_get_device_proc_addr) {
    pone_vk_get_device_proc_addr(device, vkDestroyDevice,
                                 dispatch->vk_destroy_device,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkDeviceWaitIdle,
                                 dispatch->vk_device_wait_idle,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateBuffer,
                                 dispatch->vk_create_buffer,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkDestroyBuffer,
                                 dispatch->vk_destroy_buffer,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetBufferMemoryRequirements2,
                                 dispatch->vk_get_buffer_memory_requirements_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkBindBufferMemory2,
                                 dispatch->vk_bind_buffer_memory_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetBufferDeviceAddress,
                                 dispatch->vk_get_buffer_device_address,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateImage,
                                 dispatch->vk_create_image,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateImageView,
                                 dispatch->vk_create_image_view,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetImageMemoryRequirements2,
                                 dispatch->vk_get_image_memory_requirements_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkBindImageMemory2,
                                 dispatch->vk_bind_image_memory_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkDestroyImage,
                                 dispatch->vk_destroy_image,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkDestroyImageView,
                                 dispatch->vk_destroy_image_view,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateSampler,
                                 dispatch->vk_create_sampler,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkAllocateMemory,
                                 dispatch->vk_allocate_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkFreeMemory, dispatch->vk_free_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkMapMemory, dispatch->vk_map_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetDeviceQueue2,
                                 dispatch->vk_get_device_queue_2,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateCommandPool,
                                 dispatch->vk_create_command_pool,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkAllocateCommandBuffers,
                                 dispatch->vk_allocate_command_buffers,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateSwapchainKHR,
                                 dispatch->vk_create_swapchain_khr,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetSwapchainImagesKHR,
                                 dispatch->vk_get_swapchain_images_khr,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkDestroySwapchainKHR,
                                 dispatch->vk_destroy_swapchain_khr,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkAcquireNextImageKHR,
                                 dispatch->vk_acquire_next_image_khr,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateFence,
                                 dispatch->vk_create_fence,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkWaitForFences,
                                 dispatch->vk_wait_for_fences,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkResetFences,
                                 dispatch->vk_reset_fences,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateSemaphore,
                                 dispatch->vk_create_semaphore,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateDescriptorPool,
                                 dispatch->vk_create_descriptor_pool,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateDescriptorSetLayout,
                                 dispatch->vk_create_descriptor_set_layout,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkAllocateDescriptorSets,
                                 dispatch->vk_allocate_descriptor_sets,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateShaderModule,
                                 dispatch->vk_create_shader_module,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreatePipelineLayout,
                                 dispatch->vk_create_pipeline_layout,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkCreateGraphicsPipelines,
                                 dispatch->vk_create_graphics_pipelines,
                                 vk_get_device_proc_addr);

    dispatch->vk_get_device_proc_addr = vk_get_device_proc_addr;
}

static void pone_vk_queue_dispatch_init(PoneVkQueueDispatch *dispatch,
                                        PoneVkDevice *device) {
    pone_vk_get_device_proc_addr(device->handle, vkQueueSubmit2,
                                 dispatch->vk_queue_submit_2,
                                 device->dispatch->vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device->handle, vkQueueWaitIdle,
                                 dispatch->vk_queue_wait_idle,
                                 device->dispatch->vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device->handle, vkQueuePresentKHR,
                                 dispatch->vk_queue_present_khr,
                                 device->dispatch->vk_get_device_proc_addr);
}

static void pone_vk_loader_init(const char *path, PoneVkLoader *loader) {
    void *so_handle;
    so_handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    pone_assert(so_handle);

    PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr =
        (PFN_vkGetInstanceProcAddr)dlsym(so_handle, "vkGetInstanceProcAddr");
    pone_assert(vk_get_instance_proc_addr);

    PFN_vkCreateInstance vk_create_instance;
    PFN_vkEnumerateInstanceLayerProperties
        vk_enumerate_instance_layer_properties;
    PFN_vkEnumerateInstanceExtensionProperties
        vk_enumerate_instance_extension_properties;
    pone_vk_get_instance_proc_addr(0, vkCreateInstance, vk_create_instance,
                                   vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(0, vkEnumerateInstanceLayerProperties,
                                   vk_enumerate_instance_layer_properties,
                                   vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(0, vkEnumerateInstanceExtensionProperties,
                                   vk_enumerate_instance_extension_properties,
                                   vk_get_instance_proc_addr);

    *loader = (PoneVkLoader){
        .so_handle = so_handle,
        .vk_get_instance_proc_addr = vk_get_instance_proc_addr,
        .vk_create_instance = vk_create_instance,
        .vk_enumerate_instance_layer_properties =
            vk_enumerate_instance_layer_properties,
        .vk_enumerate_instance_extension_properties =
            vk_enumerate_instance_extension_properties,
    };
}

static void pone_vk_instance_dispatch_init(PoneVkInstance *instance) {
    pone_vk_get_instance_proc_addr(
        instance->instance, vkCreateDebugUtilsMessengerEXT,
        instance->dispatch->vk_create_debug_utils_messenger_ext,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkEnumeratePhysicalDevices,
        instance->dispatch->vk_enumerate_physical_devices,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceProperties2,
        instance->dispatch->vk_get_physical_device_properties_2,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceFeatures2,
        instance->dispatch->vk_get_physical_device_features_2,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceQueueFamilyProperties,
        instance->dispatch->vk_get_physical_device_queue_family_properties,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceMemoryProperties2,
        instance->dispatch->vk_get_physical_device_memory_properties_2,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkCreateWaylandSurfaceKHR,
        instance->dispatch->vk_create_wayland_surface_khr,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceSurfaceSupportKHR,
        instance->dispatch->vk_get_physical_device_surface_support_khr,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkEnumerateDeviceExtensionProperties,
        instance->dispatch->vk_enumerate_device_extension_properties,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
        instance->dispatch->vk_get_physical_device_surface_capabilities_khr,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceSurfacePresentModesKHR,
        instance->dispatch->vk_get_physical_device_surface_present_modes_khr,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkGetPhysicalDeviceSurfaceFormatsKHR,
        instance->dispatch->vk_get_physical_device_surface_formats_khr,
        instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(instance->instance, vkCreateDevice,
                                   instance->dispatch->vk_create_device,
                                   instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(instance->instance, vkGetDeviceProcAddr,
                                   instance->dispatch->vk_get_device_proc_addr,
                                   instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(instance->instance, vkDestroyInstance,
                                   instance->dispatch->vk_destroy_instance,
                                   instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(instance->instance, vkDestroySurfaceKHR,
                                   instance->dispatch->vk_destroy_surface_khr,
                                   instance->loader->vk_get_instance_proc_addr);
    pone_vk_get_instance_proc_addr(
        instance->instance, vkDestroyDebugUtilsMessengerEXT,
        instance->dispatch->vk_destroy_debug_utils_messenger_ext,
        instance->loader->vk_get_instance_proc_addr);
}

static b8 pone_vk_check_instance_layer_support(PoneVkLoader *loader,
                                               const PoneString *layer_names,
                                               usize layer_name_count,
                                               Arena *arena) {
    u32 property_count;
    VkResult ret =
        loader->vk_enumerate_instance_layer_properties(&property_count, 0);
    pone_vk_check(ret);
    usize tmp_offset_begin = arena->offset;
    VkLayerProperties *layer_properties =
        arena_alloc_array(arena, property_count, VkLayerProperties);
    ret = loader->vk_enumerate_instance_layer_properties(&property_count,
                                                         layer_properties);
    pone_vk_check(ret);

    for (usize layer_name_index = 0; layer_name_index < layer_name_count;
         layer_name_index++) {
        const PoneString *layer_name = layer_names + layer_name_index;
        b8 found_layer = 0;
        for (usize property_index = 0; property_index < property_count;
             property_index++) {
            VkLayerProperties *layer_property =
                layer_properties + property_index;

            if (pone_string_eq_c_str(layer_name, layer_property->layerName)) {
                found_layer = 1;
                break;
            }
        }

        if (!found_layer) {
            return 0;
        }
    }
    arena->offset = tmp_offset_begin;

    return 1;
}

static b8 pone_vk_check_instance_extension_support(
    PoneVkLoader *loader, const PoneString *extension_names,
    usize extension_name_count, Arena *arena) {
    u32 property_count;
    VkResult ret = loader->vk_enumerate_instance_extension_properties(
        0, &property_count, 0);
    pone_vk_check(ret);
    usize tmp_offset_begin = arena->offset;
    VkExtensionProperties *extension_properties =
        arena_alloc_array(arena, property_count, VkExtensionProperties);
    ret = loader->vk_enumerate_instance_extension_properties(
        0, &property_count, extension_properties);
    pone_vk_check(ret);

    for (usize extension_name_index = 0;
         extension_name_index < extension_name_count; extension_name_index++) {
        const PoneString *extension_name =
            extension_names + extension_name_index;
        b8 found_extension = 0;
        for (usize property_index = 0; property_index < property_count;
             property_index++) {
            VkExtensionProperties *extension_property =
                extension_properties + property_index;

            if (pone_string_eq_c_str(extension_name,
                                     extension_property->extensionName)) {
                found_extension = 1;
                break;
            }
        }

        if (!found_extension) {
            return 0;
        }
    }
    arena->offset = tmp_offset_begin;

    return 1;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL pone_vk_debug_utils_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    Arena *arena = (Arena *)user_data;

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
    i32 n = arena_sprintf(arena, &msg, "%s%s%s\n", severity, type,
                          callback_data->pMessage);
    pone_assert(n > 0);
    pone_assert(write(1, msg, n) == n);

    arena_clear(arena);
    return VK_FALSE;
}

PoneVkInstance *pone_vk_create_instance(Arena *arena) {
    Arena *debug_utils_messenger_arena =
        (Arena *)arena_alloc(arena, sizeof(Arena));
    pone_arena_create_sub_arena(arena, KILOBYTES(4),
                                debug_utils_messenger_arena);

    PoneVkInstance *instance =
        (PoneVkInstance *)arena_alloc(arena, sizeof(PoneVkInstance));
    instance->loader = (PoneVkLoader *)arena_alloc(arena, sizeof(PoneVkLoader));
    instance->dispatch = (PoneVkInstanceDispatch *)arena_alloc(
        arena, sizeof(PoneVkInstanceDispatch));
    pone_vk_loader_init("libvulkan.so", instance->loader);

    usize arena_tmp_begin = arena->offset;
    usize required_layer_count = 1;
    PoneString required_layers = (PoneString){
        .buf = (u8 *)"VK_LAYER_KHRONOS_validation",
        .len = 27,
    };
    pone_assert(pone_vk_check_instance_layer_support(
        instance->loader, &required_layers, required_layer_count, arena));

    usize enabled_layer_count = required_layer_count;
    char **enabled_layers = (char **)&required_layers.buf;

    usize required_extension_count = 3;
    PoneString *required_extensions =
        arena_alloc_array(arena, required_extension_count, PoneString);
    pone_string_from_cstr(VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                          required_extensions);
    pone_string_from_cstr(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
                          required_extensions + 1);
    pone_string_from_cstr(VK_KHR_SURFACE_EXTENSION_NAME,
                          required_extensions + 2);
    pone_assert(pone_vk_check_instance_extension_support(
        instance->loader, required_extensions, required_extension_count,
        arena));

    usize enabled_extension_count = required_extension_count;
    char **enabled_extensions =
        arena_alloc_array(arena, enabled_extension_count, char *);
    for (usize i = 0; i < enabled_extension_count; i++) {
        char **enabled_extension = enabled_extensions + i;
        const PoneString *required_extension = required_extensions + i;

        *enabled_extension =
            arena_alloc_array(arena, required_extension->len + 1, char);
        pone_memcpy((void *)*enabled_extension, (void *)required_extension->buf,
                    required_extension->len);
        (*enabled_extension)[required_extension->len] = '\0';
    }

    VkApplicationInfo app_info;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = 0;
    app_info.pApplicationName = "Pone Renderer";
    app_info.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.pEngineName = "Pone Engine";
    app_info.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_4;

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = pone_vk_debug_utils_messenger_callback,
        .pUserData = (void *)debug_utils_messenger_arena,
    };
    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = (void *)&debug_utils_messenger_create_info,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = (uint32_t)enabled_layer_count,
        .ppEnabledLayerNames = (const char *const *)enabled_layers,
        .enabledExtensionCount = (uint32_t)enabled_extension_count,
        .ppEnabledExtensionNames = (const char *const *)enabled_extensions,
    };
    pone_vk_check((instance->loader->vk_create_instance)(
        &instance_create_info, 0, &instance->instance));

    pone_vk_instance_dispatch_init(instance);

    pone_vk_check((instance->dispatch->vk_create_debug_utils_messenger_ext)(
        instance->instance, &debug_utils_messenger_create_info, 0,
        &instance->debug_utils_messenger_ext));

    arena->offset = arena_tmp_begin;
    return instance;
}

PoneVkSurface *pone_vk_create_wayland_surface_khr(PoneVkInstance *instance,
                                                  struct wl_display *wl_display,
                                                  struct wl_surface *wl_surface,
                                                  Arena *arena) {
    VkWaylandSurfaceCreateInfoKHR surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = wl_display,
        .surface = wl_surface,
    };

    PoneVkSurface *surface =
        (PoneVkSurface *)arena_alloc(arena, sizeof(PoneVkSurface));
    pone_vk_check((instance->dispatch->vk_create_wayland_surface_khr)(
        instance->instance, &surface_create_info, 0, &surface->surface));

    return surface;
}

static b8 pone_vk_physical_device_check_extension_support(
    PoneVkInstance *instance, VkPhysicalDevice physical_device,
    PoneString *required_extension_names, usize required_extension_count,
    Arena *arena) {

    u32 available_extension_count;
    pone_vk_check(instance->dispatch->vk_enumerate_device_extension_properties(
        physical_device, 0, &available_extension_count, 0));
    VkExtensionProperties *available_extensions = arena_alloc_array(
        arena, available_extension_count, VkExtensionProperties);
    pone_vk_check(instance->dispatch->vk_enumerate_device_extension_properties(
        physical_device, 0, &available_extension_count, available_extensions));

    for (usize i = 0; i < required_extension_count; i++) {
        b8 found = 0;
        PoneString *required_extension_name = required_extension_names + i;
        for (usize j = 0; j < available_extension_count; j++) {
            VkExtensionProperties *available_extension =
                available_extensions + j;

            if (pone_string_eq_c_str(required_extension_name,
                                     available_extension->extensionName)) {
                found = 1;
                break;
            }
        }

        if (!found) {
            return 0;
        }
    }

    return 1;
}

struct PoneVkQueueFamilySupportCheck {
    VkPhysicalDevice physical_device;
    u32 queue_family_index;
    VkQueueFamilyProperties *queue_family_properties;
};

static b8 pone_vk_physical_device_check_queue_family_support(
    PoneVkInstance *instance, PoneVkSurface *surface,
    PoneVkQueueFamilySupportCheck *query) {
    VkBool32 surface_support = 0;
    pone_vk_check(
        (instance->dispatch->vk_get_physical_device_surface_support_khr)(
            query->physical_device, query->queue_family_index, surface->surface,
            &surface_support));
    if (!surface_support) {
        return 0;
    }

    if (!(query->queue_family_properties[query->queue_family_index].queueFlags &
          VK_QUEUE_GRAPHICS_BIT)) {
        return 0;
    }

    if (!(query->queue_family_properties[query->queue_family_index].queueFlags &
          VK_QUEUE_TRANSFER_BIT)) {
        return 0;
    }

    return 1;
}

static b8 pone_vk_physical_device_select_optimal_queue_family_index(
    PoneVkPhysicalDeviceQuery *query, VkPhysicalDevice physical_device,
    Arena *arena, u32 *queue_family_index) {
    PoneVkInstance *instance = query->instance;
    PoneVkSurface *surface = query->surface;
    usize arena_tmp_begin = arena->offset;

    u32 queue_family_properties_count;
    (instance->dispatch->vk_get_physical_device_queue_family_properties)(
        physical_device, &queue_family_properties_count, 0);
    VkQueueFamilyProperties *queue_family_properties = arena_alloc_array(
        arena, queue_family_properties_count, VkQueueFamilyProperties);
    (instance->dispatch->vk_get_physical_device_queue_family_properties)(
        physical_device, &queue_family_properties_count,
        queue_family_properties);

    for (u32 i = 0; i < queue_family_properties_count; i++) {
        PoneVkQueueFamilySupportCheck query = {
            .physical_device = physical_device,
            .queue_family_index = i,
            .queue_family_properties = queue_family_properties,
        };
        if (pone_vk_physical_device_check_queue_family_support(
                instance, surface, &query)) {
            arena->offset = arena_tmp_begin;
            *queue_family_index = i;
            return 1;
        }
    }

    arena->offset = arena_tmp_begin;
    return 0;
}

static b8 pone_vk_physical_device_check_surface_format_support(
    PoneVkInstance *instance, VkPhysicalDevice physical_device,
    PoneVkSurface *surface, VkSurfaceFormatKHR required_surface_format,
    Arena *arena) {
    usize arena_tmp_begin = arena->offset;
    u32 available_surface_format_count;
    pone_vk_check((
        instance->dispatch->vk_get_physical_device_surface_formats_khr)(
        physical_device, surface->surface, &available_surface_format_count, 0));
    VkSurfaceFormatKHR *available_surface_formats = arena_alloc_array(
        arena, available_surface_format_count, VkSurfaceFormatKHR);
    pone_vk_check(
        (instance->dispatch->vk_get_physical_device_surface_formats_khr)(
            physical_device, surface->surface, &available_surface_format_count,
            available_surface_formats));

    for (u32 i = 0; i < available_surface_format_count; i++) {
        VkSurfaceFormatKHR *available_surface_format =
            available_surface_formats + i;

        if (available_surface_format->format ==
                required_surface_format.format &&
            available_surface_format->colorSpace ==
                required_surface_format.colorSpace) {
            arena->offset = arena_tmp_begin;
            return 1;
        }
    }

    arena->offset = arena_tmp_begin;
    return 0;
}

static b8 pone_vk_physical_device_check_surface_present_mode_support(
    PoneVkInstance *instance, VkPhysicalDevice physical_device,
    PoneVkSurface *surface, VkPresentModeKHR required_present_mode,
    Arena *arena) {
    usize arena_tmp_begin = arena->offset;

    u32 available_present_mode_count;
    pone_vk_check((
        instance->dispatch->vk_get_physical_device_surface_present_modes_khr)(
        physical_device, surface->surface, &available_present_mode_count, 0));
    VkPresentModeKHR *available_present_modes = arena_alloc_array(
        arena, available_present_mode_count, VkPresentModeKHR);
    pone_vk_check(
        (instance->dispatch->vk_get_physical_device_surface_present_modes_khr)(
            physical_device, surface->surface, &available_present_mode_count,
            available_present_modes));

    for (u32 i = 0; i < available_present_mode_count; i++) {
        if (available_present_modes[i] == required_present_mode) {
            arena->offset = arena_tmp_begin;
            return 1;
        }
    }

    arena->offset = arena_tmp_begin;
    return 0;
}

static b8 pone_vk_physical_device_check_feature_support(
    PoneVkInstance *instance, VkPhysicalDevice physical_device,
    PoneVkPhysicalDeviceFeatures *required_features) {
    VkPhysicalDeviceVulkan13Features available_features_1_3 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    VkPhysicalDeviceVulkan12Features available_features_1_2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = (void *)&available_features_1_3,
    };
    VkPhysicalDeviceFeatures2 available_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = (void *)&available_features_1_2,
    };
    (instance->dispatch->vk_get_physical_device_features_2)(
        physical_device, &available_features);

    VkBaseInStructure *next = (VkBaseInStructure *)&available_features;

    while (next) {
        switch (next->sType) {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: {
            VkPhysicalDeviceVulkan12Features *available_features_1_2 =
                (VkPhysicalDeviceVulkan12Features *)next;

            if (required_features->buffer_device_address) {
                if (!available_features_1_2->bufferDeviceAddress) {
                    return 0;
                }
            }

            if (required_features->descriptor_indexing) {
                if (!available_features_1_2->descriptorIndexing) {
                    return 0;
                }
            }
        } break;
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: {
            VkPhysicalDeviceVulkan13Features *available_features_1_3 =
                (VkPhysicalDeviceVulkan13Features *)next;

            if (required_features->synchronization_2) {
                if (!available_features_1_3->synchronization2) {
                    return 0;
                }
            }
            if (required_features->dynamic_rendering) {
                if (!available_features_1_3->dynamicRendering) {
                    return 0;
                }
            }
        } break;
        default: {
        } break;
        }

        next = (VkBaseInStructure *)next->pNext;
    }

    return 1;
}

static b8 pone_vk_physical_device_check_all_support(
    PoneVkPhysicalDeviceQuery *query, VkPhysicalDevice physical_device,
    Arena *arena, u32 *queue_family_index) {
    u32 tmp_queue_family_index;

    b8 support =
        pone_vk_physical_device_check_extension_support(
            query->instance, physical_device, query->required_extension_names,
            query->required_extension_count, arena) &&
        pone_vk_physical_device_select_optimal_queue_family_index(
            query, physical_device, arena, &tmp_queue_family_index) &&
        pone_vk_physical_device_check_surface_format_support(
            query->instance, physical_device, query->surface,
            query->required_surface_format, arena) &&
        pone_vk_physical_device_check_surface_present_mode_support(
            query->instance, physical_device, query->surface,
            query->required_present_mode, arena) &&
        pone_vk_physical_device_check_feature_support(
            query->instance, physical_device, query->required_features);

    if (support) {
        *queue_family_index = tmp_queue_family_index;
    }

    return support;
}

PoneVkPhysicalDevice *
pone_vk_select_optimal_physical_device(PoneVkPhysicalDeviceQuery *query,
                                       Arena *arena, u32 *queue_family_index) {
    PoneVkInstance *instance = query->instance;
    PoneVkPhysicalDevice *selected_physical_device =
        (PoneVkPhysicalDevice *)arena_alloc(arena,
                                            sizeof(PoneVkPhysicalDevice));
    pone_memset((void *)selected_physical_device, 0,
                sizeof(PoneVkPhysicalDevice));
    usize arena_tmp_begin = arena->offset;

    u32 physical_device_count;
    pone_vk_check(instance->dispatch->vk_enumerate_physical_devices(
        instance->instance, &physical_device_count, 0));
    VkPhysicalDevice *physical_devices =
        arena_alloc_array(arena, physical_device_count, VkPhysicalDevice);
    pone_vk_check(instance->dispatch->vk_enumerate_physical_devices(
        instance->instance, &physical_device_count, physical_devices));

    for (usize i = 0; i < physical_device_count; i++) {
        VkPhysicalDevice physical_device = physical_devices[i];
        if (pone_vk_physical_device_check_all_support(
                query, physical_device, arena, queue_family_index)) {
            selected_physical_device->handle = physical_device;
            selected_physical_device->instance = instance;

            selected_physical_device->memory_properties =
                (VkPhysicalDeviceMemoryProperties2){
                    .sType =
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2,
                };
            (instance->dispatch->vk_get_physical_device_memory_properties_2)(
                physical_device, &selected_physical_device->memory_properties);
            break;
        }
    }

    pone_assert(selected_physical_device->handle);

    arena->offset = arena_tmp_begin;
    return selected_physical_device;
}

void pone_vk_physical_device_get_surface_capabilities(
    PoneVkPhysicalDevice *physical_device, PoneVkSurface *surface,
    VkSurfaceCapabilitiesKHR *surface_capabilities) {
    pone_vk_check((physical_device->instance->dispatch
                       ->vk_get_physical_device_surface_capabilities_khr)(
        physical_device->handle, surface->surface, surface_capabilities));
}

void pone_vk_physical_device_get_properties(
    PoneVkPhysicalDevice *physical_device,
    VkPhysicalDeviceProperties2 *properties) {
    (physical_device->instance->dispatch->vk_get_physical_device_properties_2)(
        physical_device->handle, properties);
}

static VkPhysicalDeviceFeatures2 *
pone_vk_physical_device_features_convert_to_vk(
    PoneVkPhysicalDeviceFeatures *features, Arena *arena) {
    VkPhysicalDeviceFeatures2 *vk_features =
        (VkPhysicalDeviceFeatures2 *)arena_alloc(
            arena, sizeof(VkPhysicalDeviceFeatures2));
    *vk_features = (VkPhysicalDeviceFeatures2){
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };

    if (features->buffer_device_address || features->descriptor_indexing) {
        VkPhysicalDeviceVulkan12Features *vk_features_1_2 =
            (VkPhysicalDeviceVulkan12Features *)arena_alloc(
                arena, sizeof(VkPhysicalDeviceVulkan12Features));
        *vk_features_1_2 = (VkPhysicalDeviceVulkan12Features){
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .descriptorIndexing = features->descriptor_indexing,
            .bufferDeviceAddress = features->buffer_device_address,
        };

        void **next = &vk_features->pNext;
        while (*next) {
            VkBaseOutStructure *vk_struct = (VkBaseOutStructure *)*next;
            next = (void **)&vk_struct->pNext;
        }

        *next = (void *)vk_features_1_2;
    }
    if (features->synchronization_2 || features->dynamic_rendering) {
        VkPhysicalDeviceVulkan13Features *vk_features_1_3 =
            (VkPhysicalDeviceVulkan13Features *)arena_alloc(
                arena, sizeof(VkPhysicalDeviceVulkan13Features));
        *vk_features_1_3 = (VkPhysicalDeviceVulkan13Features){
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = features->synchronization_2,
            .dynamicRendering = features->dynamic_rendering,
        };

        void **next = &vk_features->pNext;
        while (*next) {
            VkBaseOutStructure *vk_struct = (VkBaseOutStructure *)*next;
            next = (void **)&vk_struct->pNext;
        }

        *next = (void *)vk_features_1_3;
    }

    return vk_features;
}

PoneVkDevice *pone_vk_create_device(PoneVkPhysicalDevice *physical_device,
                                    PoneVkDeviceCreateInfo *device_create_info,
                                    Arena *arena) {
    PoneVkDevice *device =
        (PoneVkDevice *)arena_alloc(arena, sizeof(PoneVkDevice));
    device->dispatch = (PoneVkDeviceDispatch *)arena_alloc(
        arena, sizeof(PoneVkDeviceDispatch));
    device->command_buffer_dispatch =
        (PoneVkCommandBufferDispatch *)arena_alloc(
            arena, sizeof(PoneVkCommandBufferDispatch));

    usize arena_tmp_begin = arena->offset;
    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = device_create_info->queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    VkPhysicalDeviceFeatures2 *physical_device_features =
        pone_vk_physical_device_features_convert_to_vk(
            device_create_info->features, arena);
    u32 enabled_extension_count = device_create_info->enabled_extension_count;
    char **enabled_extension_names_c_str =
        arena_alloc_array(arena, enabled_extension_count, char *);
    for (usize i = 0; i < enabled_extension_count; i++) {
        PoneString *enabled_extension_name =
            device_create_info->enabled_extension_names + i;
        char **enabled_extension_name_c_str = enabled_extension_names_c_str + i;
        *enabled_extension_name_c_str =
            arena_alloc_array(arena, enabled_extension_name->len + 1, char);
        pone_memcpy((void *)*enabled_extension_name_c_str,
                    (void *)enabled_extension_name->buf,
                    enabled_extension_name->len);
        (*enabled_extension_name_c_str)[enabled_extension_name->len] = '\0';
    }

    VkDeviceCreateInfo vk_device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = (void *)physical_device_features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledExtensionCount = enabled_extension_count,
        .ppEnabledExtensionNames = enabled_extension_names_c_str,
    };

    pone_vk_check((physical_device->instance->dispatch->vk_create_device)(
        physical_device->handle, &vk_device_create_info, 0, &device->handle));
    pone_vk_device_dispatch_init(
        device->dispatch, device->handle,
        physical_device->instance->dispatch->vk_get_device_proc_addr);
    pone_vk_command_buffer_dispatch_init(
        device->command_buffer_dispatch, device->handle,
        physical_device->instance->dispatch->vk_get_device_proc_addr);
    device->allocation_callbacks = 0;

    arena->offset = arena_tmp_begin;
    return device;
}

void pone_vk_destroy_device(PoneVkDevice *device) {
    (device->dispatch->vk_destroy_device)(device->handle,
                                          device->allocation_callbacks);
}

void pone_vk_device_wait_idle(PoneVkDevice *device) {
    pone_vk_check((device->dispatch->vk_device_wait_idle)(device->handle));
}

PoneVkQueue *pone_vk_get_device_queue(PoneVkDevice *device,
                                      VkDeviceQueueInfo2 *queue_info,
                                      Arena *arena) {
    PoneVkQueue *queue = (PoneVkQueue *)arena_alloc(arena, sizeof(PoneVkQueue));
    (device->dispatch->vk_get_device_queue_2)(
        device->handle, (const VkDeviceQueueInfo2 *)queue_info, &queue->handle);
    pone_assert(queue->handle);
    pone_vk_queue_dispatch_init(&queue->dispatch, device);

    return queue;
}

void pone_vk_queue_submit_2(PoneVkQueue *queue, usize submit_count,
                            VkSubmitInfo2 *submits, VkFence fence) {
    pone_vk_check((queue->dispatch.vk_queue_submit_2)(
        queue->handle, (uint32_t)submit_count, (const VkSubmitInfo2 *)submits,
        fence));
}

void pone_vk_queue_wait_idle(PoneVkQueue *queue) {
    pone_vk_check((queue->dispatch.vk_queue_wait_idle)(queue->handle));
}

VkResult pone_vk_queue_present_khr(PoneVkQueue *queue,
                                   VkPresentInfoKHR *present_info) {
    return (queue->dispatch.vk_queue_present_khr)(queue->handle, present_info);
}

void pone_vk_allocate_command_buffers(
    PoneVkDevice *device, PoneVkCommandBufferAllocateInfo *allocate_info,
    Arena *arena, PoneVkCommandBuffer *command_buffers) {
    VkCommandBufferAllocateInfo command_buffer_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = 0,
        .commandPool = allocate_info->command_pool->handle,
        .level = allocate_info->level,
        .commandBufferCount = allocate_info->command_buffer_count,
    };
    usize arena_tmp_begin = arena->offset;
    VkCommandBuffer *handles = arena_alloc_array(
        arena, allocate_info->command_buffer_count, VkCommandBuffer);

    pone_vk_check((device->dispatch->vk_allocate_command_buffers)(
        device->handle, &command_buffer_allocate_info, handles));

    for (usize i = 0; i < allocate_info->command_buffer_count; i++) {
        command_buffers[i].handle = handles[i];
        command_buffers[i].dispatch = device->command_buffer_dispatch;
    }

    arena->offset = arena_tmp_begin;
}

void pone_vk_create_command_pool(PoneVkDevice *device,
                                 VkCommandPoolCreateInfo *create_info,
                                 PoneVkCommandPool *pool) {
    VkCommandPool handle;
    pone_vk_check((device->dispatch->vk_create_command_pool)(
        device->handle, create_info, device->allocation_callbacks, &handle));

    *pool = (PoneVkCommandPool){
        .handle = handle,
    };
}

void pone_vk_begin_command_buffer(PoneVkCommandBuffer *command_buffer,
                                  VkCommandBufferUsageFlags flags) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = 0,
        .flags = flags,
        .pInheritanceInfo = 0,
    };

    pone_vk_check((command_buffer->dispatch->vk_begin_command_buffer)(
        command_buffer->handle, &begin_info));
}

void pone_vk_end_command_buffer(PoneVkCommandBuffer *command_buffer) {
    pone_vk_check((command_buffer->dispatch->vk_end_command_buffer)(
        command_buffer->handle));
}

void pone_vk_cmd_copy_buffer(PoneVkCommandBuffer *command_buffer,
                             VkBuffer src_buffer, VkBuffer dst_buffer,
                             usize region_count, VkBufferCopy *regions) {
    (command_buffer->dispatch->vk_cmd_copy_buffer)(
        command_buffer->handle, src_buffer, dst_buffer, (uint32_t)region_count,
        (const VkBufferCopy *)regions);
}

void pone_vk_cmd_clear_color_image(PoneVkCommandBuffer *command_buffer,
                                   VkImage image, VkImageLayout image_layout,
                                   VkClearColorValue *color, u32 range_count,
                                   VkImageSubresourceRange *ranges) {
    (command_buffer->dispatch->vk_cmd_clear_color_image)(
        command_buffer->handle, image, image_layout, color, range_count,
        ranges);
}

void pone_vk_cmd_copy_buffer_to_image_2(
    PoneVkCommandBuffer *command_buffer,
    VkCopyBufferToImageInfo2 *copy_buffer_to_image_info) {
    (command_buffer->dispatch->vk_cmd_copy_buffer_to_image_2)(
        command_buffer->handle, copy_buffer_to_image_info);
}

void pone_vk_cmd_copy_buffer_2(PoneVkCommandBuffer *command_buffer,
                               VkCopyBufferInfo2 *copy_buffer_info) {
    (command_buffer->dispatch->vk_cmd_copy_buffer_2)(command_buffer->handle,
                                                     copy_buffer_info);
}

void pone_vk_cmd_pipeline_barrier_2(PoneVkCommandBuffer *command_buffer,
                                    VkDependencyInfo *dependency_info) {
    (command_buffer->dispatch->vk_cmd_pipeline_barrier_2)(
        command_buffer->handle, dependency_info);
}

VkDeviceAddress pone_vk_get_buffer_device_address(PoneVkDevice *device,
                                                  VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = 0,
        .buffer = buffer,
    };
    return (device->dispatch->vk_get_buffer_device_address)(device->handle,
                                                            &info);
}

void pone_vk_destroy_buffer(PoneVkDevice *device, VkBuffer buffer) {
    (device->dispatch->vk_destroy_buffer)(device->handle, buffer,
                                          device->allocation_callbacks);
}

void pone_vk_get_buffer_memory_requirements_2(
    PoneVkDevice *device, VkBuffer buffer,
    VkMemoryRequirements2 *memory_requirements) {
    VkBufferMemoryRequirementsInfo2 buffer_memory_requirements_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = 0,
        .buffer = buffer,
    };
    *memory_requirements = (VkMemoryRequirements2){
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = 0,
    };

    (device->dispatch->vk_get_buffer_memory_requirements_2)(
        device->handle, &buffer_memory_requirements_info, memory_requirements);
}

void pone_vk_bind_buffer_memory_2(PoneVkDevice *device, usize bind_info_count,
                                  VkBindBufferMemoryInfo *bind_infos) {
    pone_vk_check((device->dispatch->vk_bind_buffer_memory_2)(
        device->handle, (uint32_t)bind_info_count,
        (const VkBindBufferMemoryInfo *)bind_infos));
}

void pone_vk_allocate_memory(PoneVkDevice *device,
                             VkMemoryAllocateInfo *allocate_info,
                             VkDeviceMemory *device_memory) {
    pone_vk_check((device->dispatch->vk_allocate_memory)(
        device->handle, allocate_info, device->allocation_callbacks,
        device_memory));
}

void pone_vk_free_memory(PoneVkDevice *device, VkDeviceMemory memory) {
    (device->dispatch->vk_free_memory)(device->handle, memory,
                                       device->allocation_callbacks);
}

void pone_vk_map_memory(PoneVkDevice *device, VkDeviceMemory memory,
                        usize offset, usize size, VkMemoryMapFlags flags,
                        void **data) {
    pone_vk_check((device->dispatch->vk_map_memory)(
        device->handle, memory, (VkDeviceSize)offset, (VkDeviceSize)size, flags,
        data));
}

PoneVkSwapchainKhr *
pone_vk_create_swapchain_khr(PoneVkDevice *device,
                             PoneVkSwapchainCreateInfoKhr *create_info,
                             Arena *arena) {
    PoneVkSwapchainKhr *swapchain =
        (PoneVkSwapchainKhr *)arena_alloc(arena, sizeof(PoneVkSwapchainKhr));

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = 0,
        .flags = 0,
        .surface = create_info->surface->surface,
        .minImageCount = create_info->min_image_count,
        .imageFormat = create_info->image_format,
        .imageColorSpace = create_info->image_color_space,
        .imageExtent = create_info->image_extent,
        .imageArrayLayers = create_info->image_array_layers,
        .imageUsage = create_info->image_usage,
        .imageSharingMode = create_info->image_sharing_mode,
        .queueFamilyIndexCount = 1,
        .pQueueFamilyIndices = &create_info->queue_family_index,
        .preTransform = create_info->pre_transform,
        .compositeAlpha = create_info->composite_alpha,
        .presentMode = create_info->present_mode,
        .clipped = create_info->clipped,
        .oldSwapchain = 0,
    };

    VkSwapchainKHR handle;
    pone_vk_check((device->dispatch->vk_create_swapchain_khr)(
        device->handle, &swapchain_create_info, device->allocation_callbacks,
        &handle));

    *swapchain = (PoneVkSwapchainKhr){
        .handle = handle,
        .min_image_count = create_info->min_image_count,
        .images = 0,
        .image_extent = create_info->image_extent,
        .image_format = create_info->image_format,
        .image_color_space = create_info->image_color_space,
        .image_usage = create_info->image_usage,
        .image_sharing_mode = create_info->image_sharing_mode,
        .present_mode = create_info->present_mode,
        .pre_transform = create_info->pre_transform,
        .clipped = create_info->clipped,
    };

    return swapchain;
}

void pone_vk_get_swapchain_images_khr(PoneVkDevice *device,
                                      PoneVkSwapchainKhr *swapchain,
                                      u32 *swapchain_image_count,
                                      Arena *arena) {
    pone_vk_check((device->dispatch->vk_get_swapchain_images_khr)(
        device->handle, swapchain->handle, swapchain_image_count, 0));

    swapchain->images =
        arena_alloc_array(arena, *swapchain_image_count, VkImage);

    pone_vk_check((device->dispatch->vk_get_swapchain_images_khr)(
        device->handle, swapchain->handle, swapchain_image_count,
        swapchain->images));
}

void pone_vk_destroy_swapchain_khr(PoneVkDevice *device,
                                   PoneVkSwapchainKhr *swapchain) {
    (device->dispatch->vk_destroy_swapchain_khr)(
        device->handle, swapchain->handle, device->allocation_callbacks);
}

VkResult
pone_vk_acquire_next_image_khr(PoneVkDevice *device,
                               PoneVkAcquireNextImageInfoKhr *acquire_info,
                               u32 *image_index) {
    VkFence fence = 0;
    if (acquire_info->fence) {
        fence = acquire_info->fence->handle;
    }

    return (device->dispatch->vk_acquire_next_image_khr)(
        device->handle, acquire_info->swapchain->handle, acquire_info->timeout,
        acquire_info->semaphore->handle, fence, image_index);
}

PoneVkImage *pone_vk_create_image(PoneVkDevice *device,
                                  VkImageCreateInfo *create_info,
                                  Arena *arena) {
    PoneVkImage *image = (PoneVkImage *)arena_alloc(arena, sizeof(PoneVkImage));

    VkImage handle;
    pone_vk_check((device->dispatch->vk_create_image)(
        device->handle, create_info, device->allocation_callbacks, &handle));

    u32 queue_family_index = U32_MAX;
    if (create_info->pQueueFamilyIndices) {
        queue_family_index = create_info->pQueueFamilyIndices[0];
    }

    *image = (PoneVkImage){
        .handle = handle,
        .image_type = create_info->imageType,
        .format = create_info->format,
        .extent = create_info->extent,
        .mip_levels = create_info->mipLevels,
        .array_layers = create_info->arrayLayers,
        .samples = create_info->samples,
        .tiling = create_info->tiling,
        .usage = create_info->usage,
        .sharing_mode = create_info->sharingMode,
        .queue_family_index = queue_family_index,
        .initial_layout = create_info->initialLayout,
    };

    return image;
}

void pone2_vk_create_image_view(PoneVkDevice *device,
                                VkImageViewCreateInfo *create_info,
                                VkImageView *image_view) {
    pone_vk_check((device->dispatch->vk_create_image_view)(
        device->handle, create_info, device->allocation_callbacks, image_view));
}

PoneVkImageView *pone_vk_create_image_view(PoneVkDevice *device,
                                           VkImageViewCreateInfo *create_info,
                                           Arena *arena) {
    PoneVkImageView *image_view =
        (PoneVkImageView *)arena_alloc(arena, sizeof(PoneVkImageView));

    VkImageView handle;
    pone_vk_check((device->dispatch->vk_create_image_view)(
        device->handle, create_info, device->allocation_callbacks, &handle));

    *image_view = (PoneVkImageView){
        .handle = handle,
        .view_type = create_info->viewType,
        .format = create_info->format,
        .components = create_info->components,
        .subresource_range = create_info->subresourceRange,
    };

    return image_view;
}

void pone_vk_get_image_memory_requirements_2(
    PoneVkDevice *device, PoneVkImage *image,
    VkMemoryRequirements2 *memory_requirements) {
    VkImageMemoryRequirementsInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = 0,
        .image = image->handle,
    };
    *memory_requirements = (VkMemoryRequirements2){
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = 0,
    };

    (device->dispatch->vk_get_image_memory_requirements_2)(
        device->handle, &info, memory_requirements);
}

void pone_vk_bind_image_memory_2(PoneVkDevice *device, usize bind_info_count,
                                 VkBindImageMemoryInfo *bind_infos) {
    pone_vk_check((device->dispatch->vk_bind_image_memory_2)(
        device->handle, (uint32_t)bind_info_count,
        (const VkBindImageMemoryInfo *)bind_infos));
}

void pone_vk_destroy_image(PoneVkDevice *device, PoneVkImage *image) {
    (device->dispatch->vk_destroy_image)(device->handle, image->handle,
                                         device->allocation_callbacks);
}

void pone_vk_destroy_image_view(PoneVkDevice *device,
                                PoneVkImageView *image_view) {
    (device->dispatch->vk_destroy_image_view)(
        device->handle, image_view->handle, device->allocation_callbacks);
}

void pone_vk_create_sampler(PoneVkDevice *device,
                            VkSamplerCreateInfo *create_info,
                            VkSampler *sampler) {
    pone_vk_check((device->dispatch->vk_create_sampler)(
        device->handle, create_info, device->allocation_callbacks, sampler));
}

void pone_vk_create_buffer(PoneVkDevice *device,
                           VkBufferCreateInfo *create_info, VkBuffer *buffer) {
    pone_vk_check((device->dispatch->vk_create_buffer)(
        device->handle, create_info, device->allocation_callbacks, buffer));
}

void pone_vk_create_fence(PoneVkDevice *device, VkFenceCreateInfo *create_info,
                          PoneVkFence *fence) {
    pone_vk_check((device->dispatch->vk_create_fence)(
        device->handle, create_info, device->allocation_callbacks,
        &fence->handle));
}

static VkFence *_pone_vk_clone_fences_to_vk(u32 fence_count,
                                            PoneVkFence *fences, Arena *arena) {
    VkFence *vk_fences = arena_alloc_array(arena, fence_count, VkFence);
    for (usize i = 0; i < fence_count; i++) {
        vk_fences[i] = fences[i].handle;
    }

    return vk_fences;
}

void pone_vk_wait_for_fences(PoneVkDevice *device, u32 fence_count,
                             PoneVkFence *fences, b8 wait_all, u64 timeout,
                             Arena *arena) {
    usize arena_tmp_begin = arena->offset;
    VkFence *vk_fences =
        _pone_vk_clone_fences_to_vk(fence_count, fences, arena);

    pone_vk_check((device->dispatch->vk_wait_for_fences)(
        device->handle, fence_count, vk_fences, wait_all, timeout));

    arena->offset = arena_tmp_begin;
}

void pone_vk_reset_fences(PoneVkDevice *device, u32 fence_count,
                          PoneVkFence *fences, Arena *arena) {
    usize arena_tmp_begin = arena->offset;
    VkFence *vk_fences =
        _pone_vk_clone_fences_to_vk(fence_count, fences, arena);

    pone_vk_check((device->dispatch->vk_reset_fences)(device->handle,
                                                      fence_count, vk_fences));
    arena->offset = arena_tmp_begin;
}

void pone_vk_create_semaphore(PoneVkDevice *device,
                              VkSemaphoreCreateInfo *create_info,
                              PoneVkSemaphore *semaphore) {
    pone_vk_check((device->dispatch->vk_create_semaphore)(
        device->handle, create_info, device->allocation_callbacks,
        &semaphore->handle));
}

void pone_vk_create_descriptor_pool(PoneVkDevice *device,
                                    VkDescriptorPoolCreateInfo *create_info,
                                    VkDescriptorPool *pool) {
    pone_vk_check((device->dispatch->vk_create_descriptor_pool)(
        device->handle, create_info, device->allocation_callbacks, pool));
}

void pone_vk_create_descriptor_set_layout(
    PoneVkDevice *device, VkDescriptorSetLayoutCreateInfo *create_info,
    VkDescriptorSetLayout *set_layout) {
    pone_vk_check((device->dispatch->vk_create_descriptor_set_layout)(
        device->handle, create_info, device->allocation_callbacks, set_layout));
}

void pone_vk_allocate_descriptor_sets(
    PoneVkDevice *device, VkDescriptorSetAllocateInfo *allocate_info,
    VkDescriptorSet *descriptor_sets) {
    pone_vk_check((device->dispatch->vk_allocate_descriptor_sets)(
        device->handle, allocate_info, descriptor_sets));
}

void pone_vk_create_shader_module(PoneVkDevice *device,
                                  VkShaderModuleCreateInfo *create_info,
                                  VkShaderModule *shader_module) {
    pone_vk_check((device->dispatch->vk_create_shader_module)(device->handle,
                                                              create_info,
                                                              device->allocation_callbacks,
                                                              shader_module));
}
