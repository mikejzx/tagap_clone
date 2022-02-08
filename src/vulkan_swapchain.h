#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

/*
 * vulkan_swapchain.h
 *
 * Renderer swapchain data
 */

struct vulkan_swapchain
{
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;

    VkImage *images;
    u32 image_count;
    VkImageView *imageviews;
    VkFramebuffer *framebuffers;
};

i32 vulkan_swapchain_create(struct vulkan_swapchain *);
i32 vulkan_swapchain_create_framebuffers(struct vulkan_swapchain *);
void vulkan_swapchain_deinit(struct vulkan_swapchain *);
void vulkan_swapchain_deinit_framebuffers(struct vulkan_swapchain *);
bool vulkan_swapchain_check_support(VkPhysicalDevice);

#endif
