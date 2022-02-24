#include "pch.h"
#include "vulkan_renderer.h"
#include "vertex_buffer.h"

i32
vb_new(struct vbuffer *vb, const void *vertices, size_t size)
{
    memset(vb, 0, sizeof(struct vbuffer));

    /*
     * Create staging buffer for transfer of vertex data
     */
    //LOG_DBUG("[vulkan] creating staging buffer");
    VkBuffer staging_buf;
    VmaAllocation staging_buf_alloc;
    if (vulkan_create_buffer(
        (VkDeviceSize)size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf, &staging_buf_alloc) < 0)
    {
        LOG_ERROR("[vbuffer] failed to create staging buffer");
        return -1;
    }

    /*
     * Map vertex data into staging buffer
     */
    void *data;
    vmaMapMemory(g_vulkan->vma, staging_buf_alloc, &data);
    memcpy(data, vertices, size);
    vmaUnmapMemory(g_vulkan->vma, staging_buf_alloc);

    /*
     * Create vertex buffer.
     * We specify that it's used as destination of transfer, and as a vertex
     * buffer
     */
    //LOG_DBUG("[vulkan] creating vertex buffer");
    if (vulkan_create_buffer(
        (VkDeviceSize)size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &vb->vk_buffer, &vb->vma_alloc) < 0)
    {
        LOG_ERROR("[vbuffer] failed to create vertex buffer");
        goto fail;
    }

    // Copy staging buffer to vertex buffer
    if (vulkan_copy_buffer(staging_buf, vb->vk_buffer, size) < 0)
    {
        LOG_ERROR("[vbuffer] failed to copy staging buffer to vertex buffer");
        goto fail;
    }

    // Free the staging buffer as we no longer need it
    vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);

    vb->size = size;

    return 0;

fail:
    vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);
    return -1;
}

/* Create empty vertex buffer */
i32
vb_new_empty(struct vbuffer *vb, size_t size, bool direct_map)
{
    memset(vb, 0, sizeof(struct vbuffer));

    VkBufferUsageFlags usage = 0;
    VkMemoryPropertyFlagBits flags = 0;
    VmaAllocationCreateFlagBits alloc_flags = 0;
    if (direct_map)
    {
        // Buffer will be mapped and written to directly
        usage = 0;
        alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT |
            VMA_ALLOCATION_CREATE_MAPPED_BIT;
        flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    else
    {
        // Buffer will be used as a transfer destination
        usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        alloc_flags = 0;
        flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }


    if (vulkan_create_buffer(
        (VkDeviceSize)size,
        usage | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        alloc_flags,
        flags,
        &vb->vk_buffer, &vb->vma_alloc) < 0)
    {
        LOG_ERROR("[vbuffer] failed to create vertex buffer");
        return -1;
    }

    vb->size = size;

    return 0;
}

void
vb_free(struct vbuffer *vb)
{
    vmaDestroyBuffer(g_vulkan->vma, vb->vk_buffer, vb->vma_alloc);
}
