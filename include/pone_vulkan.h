#ifndef PONE_VULKAN_H
#define PONE_VULKAN_H

#include "pone_arena.h"
#include "pone_types.h"
#include "pone_platform.h"

#if defined(PONE_PLATFORM_WIN64)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(PONE_PLATFORM_LINUX)
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#define VK_NO_PROTOTYPES
#include "vulkan/vulkan.h"


#define pone_vk_check(res) pone_assert((res) == VK_SUCCESS)

struct PoneVkDeviceDispatch {
    PFN_vkDestroyDevice vk_destroy_device;
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
    PFN_vkAllocateMemory vk_allocate_memory;
    PFN_vkFreeMemory vk_free_memory;
    PFN_vkMapMemory vk_map_memory;
    PFN_vkGetDeviceQueue2 vk_get_device_queue_2;
    PFN_vkAllocateCommandBuffers vk_allocate_command_buffers;
    PFN_vkCreateSwapchainKHR vk_create_swapchain_khr;
    PFN_vkGetSwapchainImagesKHR vk_get_swapchain_images_khr;
    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr;
};

struct PoneVkCommandBufferDispatch {
    PFN_vkBeginCommandBuffer vk_begin_command_buffer;
    PFN_vkEndCommandBuffer vk_end_command_buffer;
    PFN_vkCmdCopyBuffer vk_cmd_copy_buffer;
    PFN_vkCmdPipelineBarrier2 vk_cmd_pipeline_barrier_2;
};

struct PoneVkDevice {
    VkDevice device;
    PoneVkDeviceDispatch dispatch;
    PoneVkCommandBufferDispatch command_buffer_dispatch;
    VkAllocationCallbacks *allocation_callbacks;
};

struct PoneVkQueueDispatch {
    PFN_vkQueueSubmit2 vk_queue_submit_2;
    PFN_vkQueueWaitIdle vk_queue_wait_idle;
};

struct PoneVkQueue {
    VkQueue queue;
    PoneVkQueueDispatch dispatch;
};

struct PoneVkCommandBuffer {
    VkCommandBuffer command_buffer;
    PoneVkCommandBufferDispatch *dispatch;
};

struct PoneVkImage {
    VkExtent3D extent;
    VkFormat format;
    VkImage image;
};

struct PoneVkMemoryAllocateInfo {
    void *p_next;
    usize allocation_size;
    u32 memory_type_index;
};

PoneVkDevice *
pone_vk_create_device(PFN_vkCreateDevice vk_create_device,
                      VkPhysicalDevice physical_device,
                      VkDeviceCreateInfo *device_create_info,
                      VkAllocationCallbacks *allocation_callbacks, Arena *arena,
                      PFN_vkGetDeviceProcAddr vk_get_device_proc_addr);
void pone_vk_destroy_device(PoneVkDevice *device);
VkDeviceAddress pone_vk_device_get_buffer_device_address(PoneVkDevice *device,
                                                         VkBuffer buffer);
VkBuffer pone_vk_create_buffer(PoneVkDevice *device, usize size,
                               VkBufferUsageFlags usage);
void pone_vk_destroy_buffer(PoneVkDevice *device, VkBuffer buffer);
VkMemoryRequirements2
pone_vk_get_buffer_memory_requirements_2(PoneVkDevice *device,
                                                VkBuffer buffer);
void
pone_vk_bind_buffer_memory_2(PoneVkDevice *device, usize bind_info_count,
                             VkBindBufferMemoryInfo *bind_infos);
VkDeviceAddress
pone_vk_get_buffer_device_address(PoneVkDevice *device, VkBuffer buffer);
void pone_vk_create_image(PoneVkDevice *device, VkImageCreateInfo *info,
                          PoneVkImage *image);
void pone_vk_create_image_view(PoneVkDevice *device,
                               VkImageViewCreateInfo *info,
                               VkImageView *image_view);
VkMemoryRequirements2
pone_vk_get_image_memory_requirements_2(PoneVkDevice *device,
                                        PoneVkImage *image);
void
pone_vk_bind_image_memory_2(PoneVkDevice *device, usize bind_info_count,
                            VkBindImageMemoryInfo *bind_infos);
void pone_vk_destroy_image(PoneVkDevice *device, PoneVkImage *image);
VkDeviceMemory pone_vk_allocate_memory(PoneVkDevice *device,
                                       PoneVkMemoryAllocateInfo *info);
void pone_vk_free_memory(PoneVkDevice *device, VkDeviceMemory memory);
void pone_vk_map_memory(PoneVkDevice *device,
                        VkDeviceMemory memory,
                        usize offset,
                        usize size,
                        VkMemoryMapFlags flags,
                        void **data);
PoneVkQueue *pone_vk_get_device_queue_2(PoneVkDevice *device, VkDeviceQueueInfo2 *queue_info,
                                        Arena *arena);
void pone_vk_queue_submit_2(PoneVkQueue *queue,
                            usize submit_count, VkSubmitInfo2 *submits,
                            VkFence fence);
void pone_vk_queue_wait_idle(PoneVkQueue *queue);
void pone_vk_allocate_command_buffers(PoneVkDevice *device,
                                      VkCommandBufferAllocateInfo *allocate_info,
                                      PoneVkCommandBuffer *command_buffers,
                                      Arena *arena);
void pone_vk_begin_command_buffer(PoneVkCommandBuffer *command_buffer,
                                  VkCommandBufferBeginInfo *begin_info);
void pone_vk_end_command_buffer(PoneVkCommandBuffer *command_buffer);
void pone_vk_cmd_copy_buffer(PoneVkCommandBuffer *command_buffer,
                             VkBuffer src_buffer,
                             VkBuffer dst_buffer,
                             usize region_count,
                             VkBufferCopy *regions);
VkSwapchainKHR pone_vk_create_swapchain_khr(PoneVkDevice *device,
                                            VkSwapchainCreateInfoKHR *info);
void pone_vk_get_swapchain_images_khr(PoneVkDevice *device,
                                      VkSwapchainKHR swapchain,
                                      VkImage **swapchain_images,
                                      u32 *swapchain_image_count, Arena *arena);

#endif
