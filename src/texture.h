#ifndef TEXTURE_H
#define TEXTURE_H

#include "types.h"

struct texture
{
    VkImage vk_image;
    VmaAllocation vma_alloc;
    VkImageView vk_imageview;
    VkSampler vk_sampler;

    u32 w, h, bpp;
};

i32 texture_load(const char *, struct texture *);
void texture_free(struct texture *);

#endif
