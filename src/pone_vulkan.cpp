#include "pone_vulkan.h"
#include "pone_assert.h"

#define pone_vk_get_device_proc_addr(device, fp, var, fn)                      \
    var = (PFN_##fp)(fn)((device), #fp);                                       \
    pone_assert(var)

static void
pone_vk_command_buffer_dispatch_init(PoneVkCommandBufferDispatch *dispatch, VkDevice device,
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
}

static void
pone_vk_device_dispatch_init(PoneVkDeviceDispatch *dispatch, VkDevice device,
                             PFN_vkGetDeviceProcAddr vk_get_device_proc_addr) {
    pone_vk_get_device_proc_addr(device, vkDestroyDevice,
                                 dispatch->vk_destroy_device,
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
    pone_vk_get_device_proc_addr(device, vkAllocateMemory,
                                 dispatch->vk_allocate_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkFreeMemory,
                                 dispatch->vk_free_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkMapMemory,
                                 dispatch->vk_map_memory,
                                 vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device, vkGetDeviceQueue2,
                                 dispatch->vk_get_device_queue_2,
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
    
    dispatch->vk_get_device_proc_addr = vk_get_device_proc_addr;
}

static void
pone_vk_device_init(PoneVkDevice *device, VkDevice inner,
                    VkAllocationCallbacks *allocation_callbacks,
                    PFN_vkGetDeviceProcAddr vk_get_device_proc_addr) {
    device->device = inner;
    device->allocation_callbacks = allocation_callbacks;
    pone_vk_device_dispatch_init(&device->dispatch, inner,
                                 vk_get_device_proc_addr);
    pone_vk_command_buffer_dispatch_init(&device->command_buffer_dispatch, inner,
                                         vk_get_device_proc_addr);
}

static void
pone_vk_queue_dispatch_init(PoneVkQueueDispatch *dispatch, PoneVkDevice *device) {
    pone_vk_get_device_proc_addr(device->device, vkQueueSubmit2,
                                 dispatch->vk_queue_submit_2,
                                 device->dispatch.vk_get_device_proc_addr);
    pone_vk_get_device_proc_addr(device->device, vkQueueWaitIdle,
                                 dispatch->vk_queue_wait_idle,
                                 device->dispatch.vk_get_device_proc_addr);
}

PoneVkDevice *
pone_vk_create_device(PFN_vkCreateDevice vk_create_device,
                      VkPhysicalDevice physical_device,
                      VkDeviceCreateInfo *device_create_info,
                      VkAllocationCallbacks *allocation_callbacks, Arena *arena,
                      PFN_vkGetDeviceProcAddr vk_get_device_proc_addr) {
    VkDevice device;
    pone_vk_check((vk_create_device)(physical_device, device_create_info,
                                     allocation_callbacks, &device));
    PoneVkDevice *result =
        (PoneVkDevice *)arena_alloc(arena, sizeof(PoneVkDevice));
    pone_vk_device_init(result, device, allocation_callbacks,
                        vk_get_device_proc_addr);

    return result;
}

void pone_vk_destroy_device(PoneVkDevice *device) {
    (device->dispatch.vk_destroy_device)(device->device,
                                         device->allocation_callbacks);
}

PoneVkQueue *
pone_vk_get_device_queue_2(PoneVkDevice *device, VkDeviceQueueInfo2 *queue_info,
                           Arena *arena) {
    PoneVkQueue *queue = (PoneVkQueue *)arena_alloc(arena, sizeof(PoneVkQueue));
    (device->dispatch.vk_get_device_queue_2)(device->device,
                                             (const VkDeviceQueueInfo2 *)queue_info,
                                             &queue->queue);
    pone_assert(queue->queue);
    pone_vk_queue_dispatch_init(&queue->dispatch, device);

    return queue;
}

void pone_vk_queue_submit_2(PoneVkQueue *queue,
                            usize submit_count, VkSubmitInfo2 *submits,
                            VkFence fence) {
    pone_vk_check((queue->dispatch.vk_queue_submit_2)(queue->queue, (uint32_t)submit_count,
                                                      (const VkSubmitInfo2 *)submits, fence));
}

void pone_vk_queue_wait_idle(PoneVkQueue *queue) {
    pone_vk_check((queue->dispatch.vk_queue_wait_idle)(queue->queue));
}

void
pone_vk_allocate_command_buffers(PoneVkDevice *device,
                                 VkCommandBufferAllocateInfo *allocate_info,
                                 PoneVkCommandBuffer *command_buffers,
                                 Arena *arena) {
    usize begin_scratch_offset = arena->offset;
    usize command_buffer_count = (usize)allocate_info->commandBufferCount;
    VkCommandBuffer *inner_buffers =
        arena_alloc_array(arena, command_buffer_count, VkCommandBuffer);
    pone_vk_check(
        (device->dispatch.vk_allocate_command_buffers)(device->device,
                                                       (const VkCommandBufferAllocateInfo *)allocate_info,
                                                       inner_buffers));
    for (usize command_buffer_index = 0;
         command_buffer_index < command_buffer_count;
         ++command_buffer_index) {
        PoneVkCommandBuffer *cmd = command_buffers + command_buffer_index;
        cmd->command_buffer = inner_buffers[command_buffer_index];
        cmd->dispatch = &device->command_buffer_dispatch;
    }
    
    arena->offset = begin_scratch_offset;
}

void
pone_vk_begin_command_buffer(PoneVkCommandBuffer *command_buffer,
                             VkCommandBufferBeginInfo *begin_info) {
    pone_vk_check(
        (command_buffer->dispatch->vk_begin_command_buffer)(command_buffer->command_buffer,
                                                           (const VkCommandBufferBeginInfo *)
                                                           begin_info));
                                                                     
}

void pone_vk_end_command_buffer(PoneVkCommandBuffer *command_buffer) {
    pone_vk_check(
        (command_buffer->dispatch->vk_end_command_buffer)(command_buffer->command_buffer));
}

void
pone_vk_cmd_copy_buffer(PoneVkCommandBuffer *command_buffer,
                        VkBuffer src_buffer,
                        VkBuffer dst_buffer,
                        usize region_count,
                        VkBufferCopy *regions) {
    (command_buffer->dispatch->vk_cmd_copy_buffer)(command_buffer->command_buffer,
                                                   src_buffer,
                                                   dst_buffer,
                                                   (uint32_t)region_count,
                                                   (const VkBufferCopy *)regions);
}

VkDeviceAddress
pone_vk_get_buffer_device_address(PoneVkDevice *device,
                                  VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = 0,
        .buffer = buffer,
    };
    return (device->dispatch.vk_get_buffer_device_address)(device->device,
                                                           &info);
}

VkBuffer pone_vk_create_buffer(PoneVkDevice *device, usize size,
                               VkBufferUsageFlags usage) {
    VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = 0,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = 0,
    };

    VkBuffer buffer;
    pone_vk_check((device->dispatch.vk_create_buffer)(
        device->device, &info, device->allocation_callbacks, &buffer));

    return buffer;
}

void pone_vk_destroy_buffer(PoneVkDevice *device, VkBuffer buffer) {
    (device->dispatch.vk_destroy_buffer)(device->device, buffer, device->allocation_callbacks);
}

VkMemoryRequirements2
pone_vk_get_buffer_memory_requirements_2(PoneVkDevice *device, VkBuffer buffer) {
    VkBufferMemoryRequirementsInfo2 buffer_memory_requirements_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = 0,
        .buffer = buffer,
    };
    VkMemoryRequirements2 memory_requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = 0,
    };

    (device->dispatch.vk_get_buffer_memory_requirements_2)(
        device->device, &buffer_memory_requirements_info, &memory_requirements);

    return memory_requirements;
}

void
pone_vk_bind_buffer_memory_2(PoneVkDevice *device, usize bind_info_count,
                             VkBindBufferMemoryInfo *bind_infos) {
    pone_vk_check(
        (device->dispatch.vk_bind_buffer_memory_2)(device->device, (uint32_t)bind_info_count,
                                                   (const VkBindBufferMemoryInfo *)bind_infos));
    
}

VkDeviceMemory pone_vk_allocate_memory(PoneVkDevice *device,
                                       PoneVkMemoryAllocateInfo *info) {
    VkMemoryAllocateInfo vk_memory_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = info->p_next,
        .allocationSize = info->allocation_size,
        .memoryTypeIndex = info->memory_type_index,
    };

    VkDeviceMemory device_memory;
    pone_vk_check((device->dispatch.vk_allocate_memory)(
        device->device, &vk_memory_allocate_info, device->allocation_callbacks,
        &device_memory));

    return device_memory;
}

void pone_vk_free_memory(PoneVkDevice *device, VkDeviceMemory memory) {
    (device->dispatch.vk_free_memory)(device->device, memory, device->allocation_callbacks);
}

void pone_vk_map_memory(PoneVkDevice *device,
                        VkDeviceMemory memory,
                        usize offset,
                        usize size,
                        VkMemoryMapFlags flags,
                        void **data) {
    pone_vk_check(
        (device->dispatch.vk_map_memory)(device->device, memory, (VkDeviceSize)offset,
                                         (VkDeviceSize)size, flags, data));
}

VkSwapchainKHR pone_vk_create_swapchain_khr(PoneVkDevice *device,
                                            VkSwapchainCreateInfoKHR *info) {
    VkSwapchainKHR swapchain;
    pone_vk_check((device->dispatch.vk_create_swapchain_khr)(
        device->device, info, device->allocation_callbacks, &swapchain));

    return swapchain;
}

void pone_vk_get_swapchain_images_khr(PoneVkDevice *device,
                                      VkSwapchainKHR swapchain,
                                      VkImage **swapchain_images,
                                      u32 *swapchain_image_count,
                                      Arena *arena) {
    pone_assert(swapchain_images && swapchain_image_count);
    pone_vk_check((device->dispatch.vk_get_swapchain_images_khr)(
        device->device, swapchain, swapchain_image_count, 0));

    *swapchain_images =
        arena_alloc_array(arena, *swapchain_image_count, VkImage);
    pone_vk_check((device->dispatch.vk_get_swapchain_images_khr)(
        device->device, swapchain, swapchain_image_count, *swapchain_images));
}

void pone_vk_create_image(PoneVkDevice *device, VkImageCreateInfo *info,
                          PoneVkImage *image) {
    VkImage inner;
    pone_vk_check((device->dispatch.vk_create_image)(
        device->device, info, device->allocation_callbacks, &inner));

    image->extent = info->extent;
    image->format = info->format;
    image->image = inner;
}

void pone_vk_create_image_view(PoneVkDevice *device,
                               VkImageViewCreateInfo *info,
                               VkImageView *image_view) {
    pone_vk_check((device->dispatch.vk_create_image_view)(
        device->device, info, device->allocation_callbacks, image_view));
}

VkMemoryRequirements2
pone_vk_get_image_memory_requirements_2(PoneVkDevice *device,
                                        PoneVkImage *image) {
    VkImageMemoryRequirementsInfo2 info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2,
        .pNext = 0,
        .image = image->image,
    };
    VkMemoryRequirements2 memory_requirements = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2,
        .pNext = 0,
    };

    (device->dispatch.vk_get_image_memory_requirements_2)(device->device, &info,
                                                          &memory_requirements);

    return memory_requirements;
}

void
pone_vk_bind_image_memory_2(PoneVkDevice *device, usize bind_info_count,
                            VkBindImageMemoryInfo *bind_infos) {
    pone_vk_check(
        (device->dispatch.vk_bind_image_memory_2)(device->device, (uint32_t)bind_info_count,
                                                  (const VkBindImageMemoryInfo *)bind_infos));
}

void pone_vk_destroy_image(PoneVkDevice *device, PoneVkImage *image) {
    (device->dispatch.vk_destroy_image)(device->device, image->image,
                                        device->allocation_callbacks);
}
