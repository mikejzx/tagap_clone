#ifndef VERTEX_BUFFER_H
#define VERTEX_BUFFER_H

#include "types.h"

/*
 * Main ertex buffer structure
 */
struct vbuffer
{
    // Vulkan vertex buffer and memory handles
    VkBuffer vk_buffer;
    VmaAllocation vma_alloc;
    size_t size;
};

i32 vb_new(struct vbuffer *, const void *, size_t);
i32 vb_new_empty(struct vbuffer *, size_t, bool);
void vb_free(struct vbuffer *);

#endif
