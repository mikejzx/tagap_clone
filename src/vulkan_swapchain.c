#include "pch.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"

extern struct vulkan_renderer *g_vulkan;

// GPU swapchain support info
struct swapchain_support_info
{
    VkSurfaceCapabilitiesKHR capabilities;

    VkSurfaceFormatKHR *formats;
    u32 format_count;

    VkPresentModeKHR *present_modes;
    u32 present_mode_count;
};

static struct swapchain_support_info 
    query_swapchain_support(VkPhysicalDevice card);

/*
 * Create the swapchain
 */
i32 
vulkan_swapchain_create(struct vulkan_swapchain *swapchain)
{
    // Check swapchain support
    struct swapchain_support_info details = 
        query_swapchain_support(g_vulkan->video_card);

    /*
     * Choose swapchain surface format
     */
    VkSurfaceFormatKHR format = details.formats[0];
    for (i32 i = 0; i < details.format_count; ++i)
    {
        VkSurfaceFormatKHR *fmt = &details.formats[i];
        if (fmt->format == VK_FORMAT_B8G8R8A8_SRGB &&
            fmt->colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            format = *fmt;
            break;
        }
    }

    /*
     * Choose swapchain present mode
     * (Fallback to FIFO mode (Vsync) which is apparently guaranteed)
     */
    VkPresentModeKHR pmode = VK_PRESENT_MODE_FIFO_KHR;
#if 0
    for (i32 i = 0; i < details.present_mode_count; ++i)
    {
        // Prefer mailbox (triple-buffering) mode
        if (details.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            LOG_DBUG("[vulkan] using swapchain mode "
                "VK_PRESENT_MODE_MAILBOX_KHR");
            pmode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
#endif

    /*
     * Choose swapchain extent
     */
    VkExtent2D extent;
    if (details.capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = details.capabilities.currentExtent;
    }
    else
    {
        extent = (VkExtent2D)
        {
            .width = 
                max(details.capabilities.minImageExtent.width, 
                    min(details.capabilities.maxImageExtent.width, WIDTH)),
            .height = 
                max(details.capabilities.minImageExtent.height, 
                    min(details.capabilities.maxImageExtent.height, HEIGHT)),
        };
    }

    // Print out modes
#ifdef DEBUG
    printf("[INFO] [vulkan] supported present modes:\n");
    for (i32 i = 0; i < details.present_mode_count; ++i)
    {
        printf("%s", details.present_modes[i] == pmode 
            ? "  [*]" 
            : "  [ ]");
        switch (details.present_modes[i])
        {
        case VK_PRESENT_MODE_IMMEDIATE_KHR:
            printf(" VK_PRESENT_MODE_IMMEDIATE_KHR\n");
            break;
        case VK_PRESENT_MODE_MAILBOX_KHR:
            printf(" VK_PRESENT_MODE_MAILBOX_KHR\n");
            break;
        case VK_PRESENT_MODE_FIFO_KHR:
            printf(" VK_PRESENT_MODE_FIFO_KHR\n");
            break;
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            printf(" VK_PRESENT_MODE_FIFO_RELAXED_KHR\n");
            break;
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
            printf(" VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR\n");
            break;
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
            printf(" VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR\n");
            break;
        default:
            printf(" (unknown)\n");
            break;
        }
    }
#endif

    // Choose how many images to have in swapchain
    u32 image_count = details.capabilities.minImageCount + 1;
    if (details.capabilities.maxImageCount > 0 &&
        image_count > details.capabilities.maxImageCount)
    {
        // Clamp image count to maximum
        image_count = details.capabilities.maxImageCount;
    }

    // Populate swapchain structure
    VkSwapchainCreateInfoKHR create_info =
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = g_vulkan->surface,
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .preTransform = details.capabilities.currentTransform,
        .presentMode = pmode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    free(details.formats);
    free(details.present_modes);

    u32 queue_family_indices[2] = 
    { 
        g_vulkan->qfams[VKQ_GRAPHICS].index, 
        g_vulkan->qfams[VKQ_PRESENT].index, 
    };
    if (g_vulkan->qfams[VKQ_GRAPHICS].index != 
        g_vulkan->qfams[VKQ_PRESENT].index)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
        LOG_DBUG("[vulkan] swapchain sharing: VK_SHARING_MODE_CONCURRENT");
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
        LOG_DBUG("[vulkan] swapchain sharing: VK_SHARING_MODE_EXCLUSIVE");
    }

    // Create the swapchain
    if (vkCreateSwapchainKHR(g_vulkan->d, 
        &create_info, NULL, &swapchain->handle) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create swapchain");
        return -1;
    }
    swapchain->format = format.format;
    swapchain->extent = extent;

    // Get swapchain images
    u32 schain_image_count;
    vkGetSwapchainImagesKHR(g_vulkan->d,
        swapchain->handle, &schain_image_count, NULL);
    swapchain->image_count = schain_image_count;
    swapchain->images = malloc(schain_image_count * sizeof(VkImage));
    vkGetSwapchainImagesKHR(g_vulkan->d,
        swapchain->handle, &schain_image_count, swapchain->images);

    /*
     * Create image views
     */
    swapchain->imageviews = 
        malloc(swapchain->image_count * sizeof(VkImageView));
    for (i32 i = 0; i < swapchain->image_count; ++i)
    {
        VkImageViewCreateInfo create_info = 
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain->format,
            .components = 
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = 
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        if (vkCreateImageView(g_vulkan->d, 
            &create_info, 
            NULL, 
            &swapchain->imageviews[i]) != VK_SUCCESS)
        {
            LOG_ERROR("[vulkan] failed to create image view");
            return -1;
        }
    }

    return 0;
}

void
vulkan_swapchain_deinit(struct vulkan_swapchain *swapchain)
{
    if (swapchain->imageviews)
    {
        for (i32 i = 0; i < swapchain->image_count; ++i)
        {
            vkDestroyImageView(g_vulkan->d, 
                swapchain->imageviews[i], NULL);
        }
        free(swapchain->imageviews);
    }
    vkDestroySwapchainKHR(g_vulkan->d, swapchain->handle, NULL);
    if (swapchain->images) free(swapchain->images);
}

void
vulkan_swapchain_deinit_framebuffers(struct vulkan_swapchain *swapchain)
{
    if (swapchain->framebuffers)
    {
        for (i32 i = 0; i < swapchain->image_count; ++i)
        {
            vkDestroyFramebuffer(g_vulkan->d, swapchain->framebuffers[i], NULL);
        }
        free(swapchain->framebuffers);
    }
}

i32 
vulkan_swapchain_create_framebuffers(struct vulkan_swapchain *swapchain)
{
    swapchain->framebuffers = 
        malloc(swapchain->image_count * sizeof(VkFramebuffer));

    for (i32 i = 0; i < swapchain->image_count; ++i)
    {
        VkFramebufferCreateInfo fb_info =
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = g_vulkan->render_pass,
            .attachmentCount = 1,
            .pAttachments = &swapchain->imageviews[i],
            .width  = swapchain->extent.width,
            .height = swapchain->extent.height,
            .layers = 1,
        };

        if (vkCreateFramebuffer(g_vulkan->d, &fb_info, NULL,
            &swapchain->framebuffers[i]) != VK_SUCCESS)
        {
            free(swapchain->framebuffers);
            swapchain->framebuffers = NULL;
            LOG_ERROR("[vulkan] failed to create framebuffer");
            return -1;
        }
    }

    return 0;
}

static struct swapchain_support_info 
query_swapchain_support(VkPhysicalDevice card)
{
    struct swapchain_support_info details;

    // Get surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(card, 
        g_vulkan->surface, &details.capabilities);

    // Get supported formats
    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(card,
        g_vulkan->surface, &format_count, NULL);
    if (format_count)
    {
        details.formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
        details.format_count = format_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(card,
            g_vulkan->surface, 
            &format_count, 
            details.formats);
    }
    else
    {
        details.formats = NULL;
        details.format_count = 0;
        LOG_ERROR("[vulkan] no surface formats modes supported on GPU?");
    }

    // Get supported presentation modes
    u32 pmode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(card,
        g_vulkan->surface, &pmode_count, NULL);
    if (pmode_count)
    {
        details.present_modes = malloc(pmode_count * sizeof(VkPresentModeKHR));
        details.present_mode_count = pmode_count;
        vkGetPhysicalDeviceSurfacePresentModesKHR(card,
            g_vulkan->surface, 
            &pmode_count, 
            details.present_modes);
    }
    else
    {
        details.present_modes = NULL;
        details.present_mode_count = 0;
        LOG_ERROR("[vulkan] no present modes supported on GPU?");
    }

    return details;
}

bool 
vulkan_swapchain_check_support(VkPhysicalDevice card)
{
    struct swapchain_support_info details = query_swapchain_support(card);
    return !!details.formats && !!details.present_modes;
}
