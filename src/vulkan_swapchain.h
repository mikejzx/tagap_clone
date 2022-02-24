#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

/*
 * vulkan_swapchain.h
 *
 * Renderer swapchain data
 */

// These theoretically would be in-game options
#define TRIPLE_BUFFERING 0
#define VSYNC 1

struct vulkan_swapchain
{
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;

    VkImage *images;
    u32 image_count;
    VkImageView *imageviews;
    VkFramebuffer *framebuffers;

    struct vulkan_framebuffer_attachment_group
    {
        struct vulkan_framebuffer_attachment
        {
            VkImage image;
            VkImageView view;
            VmaAllocation alloc;
            VkFormat format;
        } colour;
    } *attachments;
};

i32 vulkan_swapchain_create(struct vulkan_swapchain *);
i32 vulkan_swapchain_create_framebuffers(struct vulkan_swapchain *);
void vulkan_swapchain_deinit(struct vulkan_swapchain *);
void vulkan_swapchain_deinit_framebuffers(struct vulkan_swapchain *);
bool vulkan_swapchain_check_support(VkPhysicalDevice);

#endif
