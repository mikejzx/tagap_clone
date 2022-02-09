#include "pch.h"
#include "vulkan_renderer.h"
#include "index_buffer.h"

i32
ib_new(struct ibuffer *ib, const void *indices, size_t size)
{
    memset(ib, 0, sizeof(struct ibuffer));

    /*
     * Create staging buffer for transfer of index data
     */
    //LOG_DBUG("[vulkan] creating staging buffer");
    VkBuffer staging_buf;
    VmaAllocation staging_buf_alloc;
    if (vulkan_create_buffer(
        (VkDeviceSize)size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf, &staging_buf_alloc) < 0)
    {
        LOG_ERROR("[ibuffer] failed to create staging buffer");
        return -1;
    }

    /*
     * Map index data into staging buffer
     */
    void *data;
    vmaMapMemory(g_vulkan->vma, staging_buf_alloc, &data);
    memcpy(data, indices, size);
    vmaUnmapMemory(g_vulkan->vma, staging_buf_alloc);

    /*
     * Create index buffer.
     * We specify that it's used as destination of transfer, and as an index
     * buffer
     */
    //LOG_DBUG("[vulkan] creating index buffer");
    if (vulkan_create_buffer(
        (VkDeviceSize)size,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &ib->vk_buffer, &ib->vma_alloc) < 0)
    {
        LOG_ERROR("[ibuffer] failed to create index buffer");
        goto fail;
    }

    // Copy staging buffer to index buffer
    if (vulkan_copy_buffer(staging_buf, ib->vk_buffer, size) < 0)
    {
        LOG_ERROR("[ibuffer] failed to copy staging buffer to index buffer");
        goto fail;
    }

    // Free the staging buffer as we no longer need it
    vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);

    ib->size = size;

    return 0;

fail:
    vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);
    return -1;
}

void
ib_free(struct ibuffer *ib)
{
    vmaDestroyBuffer(g_vulkan->vma, ib->vk_buffer, ib->vma_alloc);
}
