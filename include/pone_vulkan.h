#ifndef PONE_VULKAN_H
#define PONE_VULKAN_H

#include "pone_arena.h"
#include "pone_platform.h"
#include "pone_string.h"
#include "pone_types.h"
#include "vulkan/vulkan_core.h"

#if defined(PONE_PLATFORM_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(PONE_PLATFORM_LINUX)
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"

#define pone_vk_check(res) pone_assert((res) == VK_SUCCESS)

struct PoneVkLoader {
    void *so_handle;
    PFN_vkGetInstanceProcAddr vk_get_instance_proc_addr;
    PFN_vkCreateInstance vk_create_instance;
    PFN_vkEnumerateInstanceLayerProperties
        vk_enumerate_instance_layer_properties;
    PFN_vkEnumerateInstanceExtensionProperties
        vk_enumerate_instance_extension_properties;
};

struct PoneVkInstanceDispatch {
    PFN_vkCreateDebugUtilsMessengerEXT vk_create_debug_utils_messenger_ext;
    PFN_vkEnumeratePhysicalDevices vk_enumerate_physical_devices;
    PFN_vkGetPhysicalDeviceProperties2 vk_get_physical_device_properties_2;
    PFN_vkGetPhysicalDeviceFeatures2 vk_get_physical_device_features_2;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties
        vk_get_physical_device_queue_family_properties;
    PFN_vkGetPhysicalDeviceMemoryProperties2
        vk_get_physical_device_memory_properties_2;
    PFN_vkCreateWaylandSurfaceKHR vk_create_wayland_surface_khr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
        vk_get_physical_device_surface_support_khr;
    PFN_vkEnumerateDeviceExtensionProperties
        vk_enumerate_device_extension_properties;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        vk_get_physical_device_surface_capabilities_khr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        vk_get_physical_device_surface_present_modes_khr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        vk_get_physical_device_surface_formats_khr;
    PFN_vkCreateDevice vk_create_device;
    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr;
    PFN_vkDestroyInstance vk_destroy_instance;
    PFN_vkDestroySurfaceKHR vk_destroy_surface_khr;
    PFN_vkDestroyDebugUtilsMessengerEXT vk_destroy_debug_utils_messenger_ext;
};

struct PoneVkInstance {
    PoneVkLoader *loader;
    PoneVkInstanceDispatch *dispatch;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_utils_messenger_ext;
};

PoneVkInstance *pone_vk_create_instance(Arena *arena);

struct PoneVkSurface {
    VkSurfaceKHR surface;
};

PoneVkSurface *pone_vk_create_wayland_surface_khr(PoneVkInstance *instance,
                                                  struct wl_display *wl_display,
                                                  struct wl_surface *wl_surface,
                                                  Arena *arena);
struct PoneVkSurfaceCapabilities {
    VkExtent2D extent;
    u32 image_count;
    VkSurfaceTransformFlagBitsKHR transform;
};

struct PoneVkPhysicalDeviceFeatures {
    b8 buffer_device_address;
    b8 descriptor_indexing;
    b8 synchronization_2;
    b8 dynamic_rendering;
};

struct PoneVkPhysicalDeviceQuery {
    PoneVkInstance *instance;
    PoneVkSurface *surface;
    usize required_extension_count;
    PoneString *required_extension_names;
    VkSurfaceFormatKHR required_surface_format;
    VkPresentModeKHR required_present_mode;
    PoneVkPhysicalDeviceFeatures *required_features;
};

struct PoneVkPhysicalDevice {
    VkPhysicalDevice handle;
    PoneVkInstance *instance;
    VkPhysicalDeviceMemoryProperties2 memory_properties;
};

PoneVkPhysicalDevice *
pone_vk_select_optimal_physical_device(PoneVkPhysicalDeviceQuery *query,
                                       Arena *arena, u32 *queue_family_index);

void pone_vk_physical_device_get_surface_capabilities(
    PoneVkPhysicalDevice *physical_device, PoneVkSurface *surface,
    VkSurfaceCapabilitiesKHR *surface_capabilities);

void pone_vk_physical_device_get_properties(
    PoneVkPhysicalDevice *physical_device,
    VkPhysicalDeviceProperties2 *properties);

struct PoneVkDeviceDispatch {
    PFN_vkDestroyDevice vk_destroy_device;
    PFN_vkDeviceWaitIdle vk_device_wait_idle;
    PFN_vkCreateBuffer vk_create_buffer;
    PFN_vkDestroyBuffer vk_destroy_buffer;
    PFN_vkGetBufferMemoryRequirements2 vk_get_buffer_memory_requirements_2;
    PFN_vkBindBufferMemory2 vk_bind_buffer_memory_2;
    PFN_vkGetBufferDeviceAddress vk_get_buffer_device_address;
    PFN_vkCreateImage vk_create_image;
    PFN_vkCreateImageView vk_create_image_view;
    PFN_vkGetImageMemoryRequirements2 vk_get_image_memory_requirements_2;
    PFN_vkBindImageMemory2 vk_bind_image_memory_2;
    PFN_vkDestroyImage vk_destroy_image;
    PFN_vkDestroyImageView vk_destroy_image_view;
    PFN_vkAllocateMemory vk_allocate_memory;
    PFN_vkFreeMemory vk_free_memory;
    PFN_vkMapMemory vk_map_memory;
    PFN_vkGetDeviceQueue2 vk_get_device_queue_2;
    PFN_vkCreateCommandPool vk_create_command_pool;
    PFN_vkAllocateCommandBuffers vk_allocate_command_buffers;
    PFN_vkCreateSwapchainKHR vk_create_swapchain_khr;
    PFN_vkGetSwapchainImagesKHR vk_get_swapchain_images_khr;
    PFN_vkDestroySwapchainKHR vk_destroy_swapchain_khr;
    PFN_vkAcquireNextImageKHR vk_acquire_next_image_khr;
    PFN_vkCreateFence vk_create_fence;
    PFN_vkWaitForFences vk_wait_for_fences;
    PFN_vkResetFences vk_reset_fences;
    PFN_vkCreateSemaphore vk_create_semaphore;
    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr;
    PFN_vkCreateDescriptorPool vk_create_descriptor_pool;
    PFN_vkCreateDescriptorSetLayout vk_create_descriptor_set_layout;
    PFN_vkAllocateDescriptorSets vk_allocate_descriptor_sets;
};

struct PoneVkCommandBufferDispatch {
    PFN_vkBeginCommandBuffer vk_begin_command_buffer;
    PFN_vkEndCommandBuffer vk_end_command_buffer;
    PFN_vkCmdCopyBuffer vk_cmd_copy_buffer;
    PFN_vkCmdPipelineBarrier2 vk_cmd_pipeline_barrier_2;
    PFN_vkCmdClearColorImage vk_cmd_clear_color_image;
    PFN_vkCmdCopyBufferToImage2 vk_cmd_copy_buffer_to_image_2;
};

struct PoneVkDeviceCreateInfo {
    u32 queue_family_index;
    u32 enabled_extension_count;
    PoneString *enabled_extension_names;
    PoneVkPhysicalDeviceFeatures *features;
};

struct PoneVkDevice {
    VkDevice handle;
    PoneVkDeviceDispatch *dispatch;
    PoneVkCommandBufferDispatch *command_buffer_dispatch;
    VkAllocationCallbacks *allocation_callbacks;
};

PoneVkDevice *pone_vk_create_device(PoneVkPhysicalDevice *physical_device,
                                    PoneVkDeviceCreateInfo *device_create_info,
                                    Arena *arena);
void pone_vk_destroy_device(PoneVkDevice *device);
void pone_vk_device_wait_idle(PoneVkDevice *device);

struct PoneVkSwapchainKhr {
    VkSwapchainKHR handle;
    u32 min_image_count;
    VkImage *images;
    VkExtent2D image_extent;
    VkFormat image_format;
    VkColorSpaceKHR image_color_space;
    VkImageUsageFlags image_usage;
    VkSharingMode image_sharing_mode;
    VkPresentModeKHR present_mode;
    VkSurfaceTransformFlagBitsKHR pre_transform;
    b8 clipped;
};

struct PoneVkSwapchainCreateInfoKhr {
    PoneVkSurface *surface;
    u32 min_image_count;
    VkFormat image_format;
    VkColorSpaceKHR image_color_space;
    VkExtent2D image_extent;
    u32 image_array_layers;
    VkImageUsageFlags image_usage;
    VkSharingMode image_sharing_mode;
    u32 queue_family_index;
    VkSurfaceTransformFlagBitsKHR pre_transform;
    VkCompositeAlphaFlagBitsKHR composite_alpha;
    VkPresentModeKHR present_mode;
    b8 clipped;
};

PoneVkSwapchainKhr *
pone_vk_create_swapchain_khr(PoneVkDevice *device,
                             PoneVkSwapchainCreateInfoKhr *create_info,
                             Arena *arena);

void pone_vk_get_swapchain_images_khr(PoneVkDevice *device,
                                      PoneVkSwapchainKhr *swapchain,
                                      u32 *swapchain_image_count, Arena *arena);
void pone_vk_destroy_swapchain_khr(PoneVkDevice *device,
                                   PoneVkSwapchainKhr *swapchain);

struct PoneVkQueueDispatch {
    PFN_vkQueueSubmit2 vk_queue_submit_2;
    PFN_vkQueueWaitIdle vk_queue_wait_idle;
    PFN_vkQueuePresentKHR vk_queue_present_khr;
};

struct PoneVkQueue {
    VkQueue handle;
    PoneVkQueueDispatch dispatch;
};

void pone_vk_queue_submit_2(PoneVkQueue *queue, usize submit_count,
                            VkSubmitInfo2 *submits, VkFence fence);
void pone_vk_queue_wait_idle(PoneVkQueue *queue);
VkResult pone_vk_queue_present_khr(PoneVkQueue *queue,
                                   VkPresentInfoKHR *present_info);

struct PoneVkCommandPool {
    VkCommandPool handle;
};

void pone_vk_create_command_pool(PoneVkDevice *device,
                                 VkCommandPoolCreateInfo *create_info,
                                 PoneVkCommandPool *pool);

struct PoneVkCommandBuffer {
    VkCommandBuffer handle;
    PoneVkCommandBufferDispatch *dispatch;
};

struct PoneVkCommandBufferAllocateInfo {
    PoneVkCommandPool *command_pool;
    VkCommandBufferLevel level;
    u32 command_buffer_count;
};

void pone_vk_allocate_command_buffers(
    PoneVkDevice *device, PoneVkCommandBufferAllocateInfo *allocate_info,
    Arena *arena, PoneVkCommandBuffer *command_buffers);
void pone_vk_begin_command_buffer(PoneVkCommandBuffer *command_buffer,
                                  VkCommandBufferUsageFlags flags);
void pone_vk_cmd_clear_color_image(PoneVkCommandBuffer *command_buffer,
                                   VkImage image, VkImageLayout image_layout,
                                   VkClearColorValue *color, u32 range_count,
                                   VkImageSubresourceRange *ranges);
void pone_vk_cmd_copy_buffer_to_image_2(
    PoneVkCommandBuffer *command_buffer,
    VkCopyBufferToImageInfo2 *copy_buffer_to_image_info);
void pone_vk_end_command_buffer(PoneVkCommandBuffer *command_buffer);
void pone_vk_cmd_pipeline_barrier_2(PoneVkCommandBuffer *command_buffer,
                                    VkDependencyInfo *dependency_info);

struct PoneVkImage {
    VkImage handle;
    VkImageType image_type;
    VkFormat format;
    VkExtent3D extent;
    u32 mip_levels;
    u32 array_layers;
    VkSampleCountFlagBits samples;
    VkImageTiling tiling;
    VkImageUsageFlags usage;
    VkSharingMode sharing_mode;
    u32 queue_family_index;
    VkImageLayout initial_layout;
};

PoneVkImage *pone_vk_create_image(PoneVkDevice *device,
                                  VkImageCreateInfo *create_info, Arena *arena);

struct PoneVkImageView {
    VkImageView handle;
    VkImageViewType view_type;
    VkFormat format;
    VkComponentMapping components;
    VkImageSubresourceRange subresource_range;
};

PoneVkImageView *pone_vk_create_image_view(PoneVkDevice *device,
                                           VkImageViewCreateInfo *create_info,
                                           Arena *arena);
void pone_vk_destroy_image_view(PoneVkDevice *device,
                                PoneVkImageView *image_view);

struct PoneVkFence {
    VkFence handle;
};

void pone_vk_create_fence(PoneVkDevice *device, VkFenceCreateInfo *create_info,
                          PoneVkFence *fence);
void pone_vk_wait_for_fences(PoneVkDevice *device, u32 fence_count,
                             PoneVkFence *fences, b8 wait_all, u64 timeout,
                             Arena *arena);
void pone_vk_reset_fences(PoneVkDevice *device, u32 fence_count,
                          PoneVkFence *fences, Arena *arena);

struct PoneVkSemaphore {
    VkSemaphore handle;
};

void pone_vk_create_semaphore(PoneVkDevice *device,
                              VkSemaphoreCreateInfo *create_info,
                              PoneVkSemaphore *semaphore);

struct PoneVkAcquireNextImageInfoKhr {
    PoneVkSwapchainKhr *swapchain;
    u64 timeout;
    PoneVkSemaphore *semaphore;
    PoneVkFence *fence;
};

VkResult
pone_vk_acquire_next_image_khr(PoneVkDevice *device,
                               PoneVkAcquireNextImageInfoKhr *acquire_info,
                               u32 *image_index);

void pone_vk_create_descriptor_pool(PoneVkDevice *device,
                                    VkDescriptorPoolCreateInfo *create_info,
                                    VkDescriptorPool *pool);
void pone_vk_create_descriptor_set_layout(
    PoneVkDevice *device, VkDescriptorSetLayoutCreateInfo *create_info,
    VkDescriptorSetLayout *set_layout);
void pone_vk_allocate_descriptor_sets(
    PoneVkDevice *device, VkDescriptorSetAllocateInfo *allocate_info,
    VkDescriptorSet *descriptor_sets);

VkDeviceAddress pone_vk_device_get_buffer_device_address(PoneVkDevice *device,
                                                         VkBuffer buffer);
void pone_vk_create_buffer(PoneVkDevice *device,
                           VkBufferCreateInfo *create_info, VkBuffer *buffer);
void pone_vk_destroy_buffer(PoneVkDevice *device, VkBuffer buffer);
void pone_vk_get_buffer_memory_requirements_2(
    PoneVkDevice *device, VkBuffer buffer,
    VkMemoryRequirements2 *memory_requirements);
void pone_vk_bind_buffer_memory_2(PoneVkDevice *device, usize bind_info_count,
                                  VkBindBufferMemoryInfo *bind_infos);
VkDeviceAddress pone_vk_get_buffer_device_address(PoneVkDevice *device,
                                                  VkBuffer buffer);
void pone_vk_get_image_memory_requirements_2(
    PoneVkDevice *device, PoneVkImage *image,
    VkMemoryRequirements2 *memory_requirements);
void pone_vk_bind_image_memory_2(PoneVkDevice *device, usize bind_info_count,
                                 VkBindImageMemoryInfo *bind_infos);
void pone_vk_destroy_image(PoneVkDevice *device, PoneVkImage *image);
void pone_vk_allocate_memory(PoneVkDevice *device, VkMemoryAllocateInfo *info,
                             VkDeviceMemory *device_memory);
void pone_vk_free_memory(PoneVkDevice *device, VkDeviceMemory memory);
void pone_vk_map_memory(PoneVkDevice *device, VkDeviceMemory memory,
                        usize offset, usize size, VkMemoryMapFlags flags,
                        void **data);
PoneVkQueue *pone_vk_get_device_queue(PoneVkDevice *device,
                                      VkDeviceQueueInfo2 *queue_info,
                                      Arena *arena);
void pone_vk_cmd_copy_buffer(PoneVkCommandBuffer *command_buffer,
                             VkBuffer src_buffer, VkBuffer dst_buffer,
                             usize region_count, VkBufferCopy *regions);

#endif
