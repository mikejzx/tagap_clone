#include "pch.h"
#include "index_buffer.h"
#include "renderer.h"
#include "tagap.h"
#include "tagap_theme.h"
#include "vertex_buffer.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"
#include "shader.h"

#define MAX_FRAMES_IN_FLIGHT 2

#ifdef DEBUG
#  define VALIDATION_LAYERS_ENABLED 1
#else
#  define VALIDATION_LAYERS_ENABLED 0
#endif

struct vulkan_renderer *g_vulkan;

// All the data that can stay in this file only
static struct vulkan_swapchain *swapchain = NULL;
static const char *DEVICE_EXTENSIONS[] =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
static VkSemaphore
    semaphore_img_available[MAX_FRAMES_IN_FLIGHT],
    semaphore_render_finish[MAX_FRAMES_IN_FLIGHT];
static VkFence
    in_flight_fences[MAX_FRAMES_IN_FLIGHT],
    *in_flight_images = NULL;

// Validation layer stuff for Debug mode
static bool check_validation_support(void);
static const char *VALIDATION_LAYERS[] =
{
    "VK_LAYER_KHRONOS_validation",
    NULL // Null-terminator
};
static const i32 VALIDATION_LAYER_COUNT =
    sizeof(VALIDATION_LAYERS) / sizeof(const char *) - 1;

static size_t cur_frame = 0;
static u32 cur_image_index = 0;

static i32 vulkan_create_instance(SDL_Window *handle);
static i32 vulkan_create_surface(SDL_Window *handle);
static i32 vulkan_get_physical_device(void);
static i32 vulkan_create_logical_device(void);
static i32 vulkan_create_allocator(void);
static i32 vulkan_create_render_pass(void);
static i32 vulkan_create_light_render_pass(void);
static i32 vulkan_create_descriptor_set_layout(void);
static i32 vulkan_create_command_pool(void);
static i32 vulkan_create_zbuffer();
static i32 vulkan_create_sync_objects(void);
static i32 vulkan_create_command_buffers(void);
static i32 vulkan_create_descriptor_pool(void);
static i32 vulkan_setup_textures(void);
static i32 vulkan_texture_create(u8 *, i32, i32,
    VkDeviceSize, VkImageUsageFlagBits, VkFormat, struct vulkan_texture *);
static i32 vulkan_rewrite_descriptors(void);

static VkCommandBuffer vulkan_begin_oneshot_cmd(void);
static i32 vulkan_end_oneshot_cmd(VkCommandBuffer);

static inline bool
is_qfam_complete(struct queue_family *qfam, u32 count)
{
    for (u32 i = 0; i < count; ++i)
    {
        if (qfam[i].has_index) continue;
        return false;
    }
    return true;
}

void
vulkan_renderer_init_state(void) { g_vulkan = &g_state.vulkan; }

i32
vulkan_renderer_init(SDL_Window *handle)
{
    i32 status;

    swapchain = calloc(1, sizeof(struct vulkan_swapchain));
    g_vulkan->swapchain = swapchain;

    (void)((status = vulkan_create_instance(handle)) < 0 ||
    (status = vulkan_create_surface(handle)) < 0 ||
    (status = vulkan_get_physical_device()) < 0 ||
    (status = vulkan_create_logical_device()) < 0 ||
    (status = vulkan_create_allocator()) < 0 ||
    (status = vulkan_swapchain_create(swapchain)) < 0 ||
    (status = vulkan_create_render_pass()) < 0 ||
    (status = vulkan_create_light_render_pass()) < 0 ||
    (status = vulkan_create_descriptor_set_layout()) < 0 ||
    (status = vulkan_shaders_init_all()) < 0 ||
    (status = vulkan_create_zbuffer()) < 0 ||
    (status = vulkan_swapchain_create_framebuffers(swapchain)) < 0 ||
    (status = vulkan_create_command_pool()) < 0 ||
    (status = vulkan_create_sync_objects()) < 0 ||
    (status = vulkan_create_descriptor_pool()) < 0 ||
    (status = vulkan_setup_textures()) < 0 ||
    (status = vulkan_update_sp2_descriptors()) < 0 ||
    (status = vulkan_rewrite_descriptors()) < 0 ||
    (status = vulkan_create_command_buffers()) < 0);
    return status;
}

void
vulkan_renderer_wait_for_idle(void)
{
    vkDeviceWaitIdle(g_vulkan->d);
}

void
vulkan_renderer_deinit(void)
{
    LOG_INFO("[vulkan] cleanup");
    vulkan_renderer_wait_for_idle();

    // Destroy the global sampler
    vkDestroySampler(g_vulkan->d, g_vulkan->sampler, NULL);
    if (g_vulkan->image_desc_infos) free(g_vulkan->image_desc_infos);

    // Destroy image views and images
    for (u32 i = 0; i < MAX_TEXTURES; ++i)
    {
        vkDestroyImageView(g_vulkan->d, g_vulkan->textures[i].view, NULL);
        if (i < g_vulkan->tex_used)
        {
            vmaDestroyImage(g_vulkan->vma,
                g_vulkan->textures[i].image,
                g_vulkan->textures[i].alloc);
        }
    }

    // Cleanup lightmap images, framebuffers, etc.
    if (g_vulkan->light_framebufs)
    {
        for (u32 i = 0; i < swapchain->image_count; ++i)
        {
            vkDestroyFramebuffer(g_vulkan->d,
                g_vulkan->light_framebufs[i], NULL);
            vkDestroyImageView(g_vulkan->d, g_vulkan->light_tex[i].view, NULL);
            vmaDestroyImage(g_vulkan->vma,
                g_vulkan->light_tex[i].image,
                g_vulkan->light_tex[i].alloc);
        }
    }
    if (g_vulkan->light_framebufs) free(g_vulkan->light_framebufs);
    if (g_vulkan->light_tex) free(g_vulkan->light_tex);

    // Cleanup Z-buffer
    vkDestroyImageView(g_vulkan->d, g_vulkan->zbuf_view, NULL);
    vmaDestroyImage(g_vulkan->vma, g_vulkan->zbuf_image, g_vulkan->zbuf_alloc);

    if (in_flight_images) free(in_flight_images);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (semaphore_img_available[i])
        {
            vkDestroySemaphore(g_vulkan->d, semaphore_img_available[i], NULL);
        }
        if (semaphore_render_finish[i])
        {
            vkDestroySemaphore(g_vulkan->d, semaphore_render_finish[i], NULL);
        }
        if (in_flight_fences[i])
        {
            vkDestroyFence(g_vulkan->d, in_flight_fences[i], NULL);
        }
    }
    vkDestroyCommandPool(g_vulkan->d, g_vulkan->cmd_pool, NULL);
    vulkan_swapchain_deinit_framebuffers(swapchain);
    vulkan_shaders_free_all();
    vkDestroyDescriptorSetLayout(g_vulkan->d,
        g_vulkan->desc_set_layout_sp2, NULL);
    vkDestroyDescriptorSetLayout(g_vulkan->d,
        g_vulkan->desc_set_layout, NULL);
    vkDestroyRenderPass(g_vulkan->d, g_vulkan->light_render_pass, NULL);
    vkDestroyRenderPass(g_vulkan->d, g_vulkan->render_pass, NULL);
    vulkan_swapchain_deinit(swapchain);
    vkDestroyDescriptorPool(g_vulkan->d, g_vulkan->desc_pool, NULL);
    if (g_vulkan->desc_sets) free(g_vulkan->desc_sets);
    if (g_vulkan->desc_sets_sp2) free(g_vulkan->desc_sets_sp2);
    vmaDestroyAllocator(g_vulkan->vma);
    vkDestroyDevice(g_vulkan->d, NULL);
    vkDestroySurfaceKHR(g_vulkan->instance, g_vulkan->surface, NULL);
    vkDestroyInstance(g_vulkan->instance, NULL);

    free(swapchain);
}

/*
 * Create Vulkan instance
 */
static i32
vulkan_create_instance(SDL_Window *handle)
{
    // Application info
    const VkApplicationInfo app =
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = NULL,
        .pApplicationName = "TAGAP",
        .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
        .pEngineName = "TAGAP Engine Clone",
        .engineVersion = VK_MAKE_VERSION(0, 0, 1),
        .apiVersion = VK_API_VERSION_1_0,
    };

    // Get extensions
    u32 ext_count;
    if (SDL_Vulkan_GetInstanceExtensions(
        handle, &ext_count, NULL) != SDL_TRUE)
    {
        LOG_ERROR("[vulkan] failed to get instance extensions");
        return -1;
    }
    const char **extensions = malloc(sizeof(const char *) * ext_count);
    if (SDL_Vulkan_GetInstanceExtensions(
        handle, &ext_count, extensions) != SDL_TRUE)
    {
        LOG_ERROR("[vulkan] failed to read instance extensions");
        return -1;
    }

    // Instance info
    VkInstanceCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .pApplicationInfo = &app,
        .enabledExtensionCount = ext_count,
        .ppEnabledExtensionNames = extensions,
    };

    // Enable validation layers (in DEBUG mode)
    if (VALIDATION_LAYERS_ENABLED && !check_validation_support())
    {
        LOG_WARN("[vulkan] validation layers requested but not supported");
        create_info.ppEnabledLayerNames = NULL;
        create_info.enabledLayerCount = 0;
    }
    else
    {
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
        create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
    }

    // Create instance
    VkResult result = vkCreateInstance(&create_info, NULL, &g_vulkan->instance);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create instance");
        free(extensions);
        return -1;
    }

//#ifdef DEBUG
    // Enumerate and print extensions in debug mode
    ext_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
    VkExtensionProperties *ext =
        malloc(ext_count * sizeof(VkExtensionProperties));
    vkEnumerateInstanceExtensionProperties(NULL, &ext_count, ext);
    printf("[INFO] [vulkan] supported extensions:\n");
    for (i32 i = 0; i < ext_count; ++i)
    {
        printf("  * %s\n", ext[i].extensionName);
    }
    free(ext);
//#endif

    LOG_INFO("[vulkan] library initialised, instance created");

    free(extensions);

    return 0;
}

static i32
vulkan_create_surface(SDL_Window *handle)
{
    if (SDL_Vulkan_CreateSurface(handle,
        g_vulkan->instance,
        &g_vulkan->surface) != SDL_TRUE)
    {
        LOG_ERROR("[vulkan] failed to create window surface");
        return -1;
    }
    return 0;
}

static bool
check_validation_support(void)
{
    u32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);
    VkLayerProperties *layers =
        malloc(layer_count * sizeof(VkLayerProperties));
    vkEnumerateInstanceLayerProperties(&layer_count, layers);

    bool status = true;
    for (const char **layer_name = VALIDATION_LAYERS;
        *layer_name != NULL;
        ++layer_name)
    {
        bool layer_found = false;
        for (i32 i = 0; i < layer_count; ++i)
        {
            VkLayerProperties *layer_props = &layers[i];
            if (strcmp(*layer_name, layer_props->layerName) != 0) continue;

            layer_found = true;
            break;
        }

        if (!layer_found)
        {
            status = false;
            LOG_WARN("[vulkan] no support for layer '%s'", *layer_name);
            break;
        }
    }

    free(layers);

    if (status)
    {
        LOG_INFO("[vulkan] good validation support");
    }

    return status;
}

static void
find_queue_families(VkPhysicalDevice card,
    struct queue_family *qfams, u32 qfam_count)
{
    memset(qfams, 0, qfam_count * sizeof(struct queue_family));

    // Get queue family properties from card
    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(card, &count, NULL);
    VkQueueFamilyProperties *families =
        malloc(count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(card, &count, families);

    for (i32 i = 0; i < count; ++i)
    {
        struct VkQueueFamilyProperties *qfp = &families[i];

        // Check for graphics support
        if (qfp->queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            qfams[VKQ_GRAPHICS].has_index = true;
            qfams[VKQ_GRAPHICS].index = i;
        }

        // Check for presentation support
        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(card, i,
            g_vulkan->surface, &present_support);
        if (present_support)
        {
            qfams[VKQ_PRESENT].has_index = true;
            qfams[VKQ_PRESENT].index = i;
        }

        if (is_qfam_complete(qfams, count)) break;
    }

    free(families);
}

static bool
check_device_extension_support(VkPhysicalDevice card)
{
    // Get extensions
    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(card,
        NULL, &extension_count, NULL);
    VkExtensionProperties *available_exts =
        malloc(extension_count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(card,
        NULL, &extension_count, available_exts);

    // Make sure that all extensions we need are satisfied
    i32 required_ext_count = sizeof(DEVICE_EXTENSIONS) / sizeof(const char *);
    i32 *required_exts = calloc(required_ext_count, sizeof(i32));
    for (i32 i = 0; i < extension_count; ++i)
    {
        VkExtensionProperties *ext = &available_exts[i];
        for (i32 r = 0; r < required_ext_count; ++r)
        {
            if (strcmp(DEVICE_EXTENSIONS[r], ext->extensionName) != 0) continue;

            LOG_DBUG("[vulkan] extension is supported: %s", ext->extensionName);
            required_exts[r] = 1;
            break;
        }
    }

    // Check that all extensions satisfied
    bool satisfied = true;
    for (i32 r = 0; r < required_ext_count; ++r)
    {
        if (!required_exts[r])
        {
            satisfied = false;
            LOG_WARN("[vulkan] extensions not all satisfied");
            break;
        }
    }

    if (satisfied)
    {
        LOG_INFO("[vulkan] all extensions satisfied");
    }

    free(required_exts);
    free(available_exts);

    return satisfied;
}

/*
 * Get physical graphics device
 */
static i32
vulkan_get_physical_device(void)
{
    g_vulkan->video_card = VK_NULL_HANDLE;

    u32 gpu_count = 0;
    vkEnumeratePhysicalDevices(g_vulkan->instance, &gpu_count, NULL);
    if (gpu_count <= 0)
    {
        LOG_ERROR("[vulkan] failed to find any GPUs with Vulkan support!  "
            "Upgrade your damned PC!");
        return -1;
    }
    VkPhysicalDevice *gpus = malloc(gpu_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(g_vulkan->instance, &gpu_count, gpus);

    for (i32 i = 0; i < gpu_count; ++i)
    {
        struct queue_family qfams[VKQ_COUNT];
        find_queue_families(gpus[i], qfams, VKQ_COUNT);
        bool suitable =
            is_qfam_complete(qfams, VKQ_COUNT) &&
            check_device_extension_support(gpus[i]) &&
            vulkan_swapchain_check_support(gpus[i]);
        if (!suitable) continue;

        // Just use the first suitable device
        // TODO: select best video card for rendering, e.g. discrete GPU
        g_vulkan->video_card = gpus[i];
        memcpy(g_vulkan->qfams, qfams, VKQ_COUNT * sizeof(struct queue_family));
        break;
    }

//#ifdef DEBUG
    // Print the devices (and highlight device in use) in debug mode
    printf("[INFO] [vulkan] detected video cards:\n");
    for (i32 i = 0; i < gpu_count; ++i)
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(gpus[i], &props);
        printf("  %s %s" LOG_ESC_RESET "\n",
            gpus[i] == g_vulkan->video_card ?
                "\033[1;32m[*]" : "[ ]",
            props.deviceName);
        break;
    }
//#endif

    free(gpus);

    if (g_vulkan->video_card == VK_NULL_HANDLE)
    {
        LOG_ERROR("[vulkan] failed to find suitable video card!");
        return -1;
    }
    return 0;
}

/*
 * Create the logical device
 */
static i32
vulkan_create_logical_device(void)
{
    // Seems to work well with the two queues we have (which are usually not
    // unique to each other).
    i32 unique_queue_count = 0;
    i32 unique_queues[VKQ_COUNT];
    for (i32 i = 0; i < VKQ_COUNT; ++i)
    {
        for (i32 j = 0; j < unique_queue_count; ++j)
        {
            if (unique_queues[j] == g_vulkan->qfams[i].index)
            {
                goto not_unique;
            }
        }
        unique_queues[unique_queue_count++] = g_vulkan->qfams[i].index;
    not_unique:
        continue;
    }
    LOG_DBUG("[vulkan] %d unique queues", unique_queue_count);
    VkDeviceQueueCreateInfo *queue_create_infos =
        calloc(unique_queue_count, sizeof(VkDeviceQueueCreateInfo));
    f32 queue_priority = 1.0f;
    for (i32 i = 0; i < unique_queue_count; ++i)
    {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo)
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_queues[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    const VkPhysicalDeviceFeatures features = { 0 };
    VkDeviceCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = unique_queue_count,
        .pEnabledFeatures = &features,

        // Enable extensions
        .enabledExtensionCount =
            sizeof(DEVICE_EXTENSIONS) / sizeof(const char *),
        .ppEnabledExtensionNames = DEVICE_EXTENSIONS,
    };

    // Enable validation layers
    if (VALIDATION_LAYERS_ENABLED)
    {
        create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
        create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
    }
    else
    {
        create_info.ppEnabledLayerNames = NULL;
        create_info.enabledLayerCount = 0;
    }

    // Create logical device
    if (vkCreateDevice(g_vulkan->video_card,
        &create_info, NULL, &g_vulkan->d) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create logical device");
        free(queue_create_infos);
        return -1;
    }
    LOG_INFO("[vulkan] created logical device");
    free(queue_create_infos);

    // Get queue handles
    vkGetDeviceQueue(g_vulkan->d,
        g_vulkan->qfams[VKQ_GRAPHICS].index, 0,
        &g_vulkan->qfams[VKQ_GRAPHICS].queue);
    vkGetDeviceQueue(g_vulkan->d,
        g_vulkan->qfams[VKQ_PRESENT].index, 0,
        &g_vulkan->qfams[VKQ_PRESENT].queue);

    return 0;
}

/*
 * Create the allocator
 */
static i32
vulkan_create_allocator(void)
{
    const VmaAllocatorCreateInfo alloc_info =
    {
        .vulkanApiVersion = VK_API_VERSION_1_0,
        .physicalDevice = g_vulkan->video_card,
        .device = g_vulkan->d,
        .instance = g_vulkan->instance,
    };
    if (vmaCreateAllocator(&alloc_info, &g_vulkan->vma) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create VMA allocator");
        return -1;
    }
    return 0;
}

/*
 * Create render pass
 */
static i32
vulkan_create_render_pass(void)
{
    VkAttachmentDescription attachments[3];
    VkSubpassDescription subpass_desc[2];

    // Custom image colour attachment
    attachments[2] = (VkAttachmentDescription)
    {
        .format = swapchain->format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference ref_colour =
    {
        .attachment = 2,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    // Swapchain colour buffer attachment description
    attachments[0] = (VkAttachmentDescription)
    {
        .format = swapchain->format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference ref_colour_swapchain =
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    // Swapchain Z-buffer attachment description
    attachments[1] = (VkAttachmentDescription)
    {
        .format = VK_FORMAT_D32_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference ref_depth_swapchain =
    {
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    // Subpass 1: input attachment
    subpass_desc[0] = (VkSubpassDescription)
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ref_colour,
        .pDepthStencilAttachment = &ref_depth_swapchain,
    };

    VkAttachmentReference input_references[1] =
    {
        { 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
    };

    // Subpass 2: input attachment read, swapchain colour attachment write
    subpass_desc[1] = (VkSubpassDescription)
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &ref_colour_swapchain,
        .pDepthStencilAttachment = NULL,
        .inputAttachmentCount = sizeof(input_references) /
            sizeof(VkAttachmentReference),
        .pInputAttachments = input_references,
    };

    const VkSubpassDependency deps[] =
    {
        {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = 1,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
    };

    /*
     * Create the render pass
     */
    VkRenderPassCreateInfo render_pass_info =
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = sizeof(attachments) /
            sizeof(VkAttachmentDescription),
        .pAttachments = attachments,
        .subpassCount = sizeof(subpass_desc) / sizeof(VkSubpassDescription),
        .pSubpasses = subpass_desc,
        .dependencyCount = sizeof(deps) / sizeof(VkSubpassDependency),
        .pDependencies = deps,
    };
    if (vkCreateRenderPass(g_vulkan->d, &render_pass_info,
        NULL, &g_vulkan->render_pass) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create render pass");
        return -1;
    }

    return 0;
}

/*
 * Create lightmap render pass
 */
static i32
vulkan_create_light_render_pass(void)
{
    // Colour buffer attachment description
    VkAttachmentDescription colour_attachment =
    {
        .format = VK_FORMAT_R16G16B16A16_SFLOAT,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    VkAttachmentReference colour_attachment_ref =
    {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    // Subpass
    VkSubpassDescription subpass =
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colour_attachment_ref,
    };

    /*
     * Create subpass dependency
     */
    const VkSubpassDependency dep =
    {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    /*
     * Create the render pass
     */
    VkRenderPassCreateInfo render_pass_info =
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colour_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dep,
    };
    if (vkCreateRenderPass(g_vulkan->d, &render_pass_info,
        NULL, &g_vulkan->light_render_pass) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create light render pass");
        return -1;
    }

    return 0;
}

static i32
vulkan_create_descriptor_set_layout(void)
{
    /*
     * Subpass 1 descriptor set layout; we bind 128 texture samplers for use in
     * level rendering
     */
    // The global texture sampler binding
    static const VkDescriptorSetLayoutBinding layout_bindings[] =
    {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 1,
            .pImmutableSamplers = NULL,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = MAX_TEXTURES,
            .pImmutableSamplers = NULL,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    const VkDescriptorSetLayoutCreateInfo layout_info =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(layout_bindings) /
            sizeof(VkDescriptorSetLayoutBinding),
        .pBindings = layout_bindings,
    };
    if (vkCreateDescriptorSetLayout(g_vulkan->d,
        &layout_info, NULL, &g_vulkan->desc_set_layout) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create descriptor "
            "set layout for subpass 1");
        return -1;
    }

    /*
     * Subpass 2 descriptor set layout
     * Simply allows for reading the colour attachment from subpass 1
     */
    static const VkDescriptorSetLayoutBinding layout_bindings_sp2[] =
    {
        {
            // Input colour attachment/G-buffer (from subpass 1)
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .pImmutableSamplers = NULL,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            // Lightmap texture
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .pImmutableSamplers = NULL,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            // Environment (e.g. rain) texture
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .pImmutableSamplers = NULL,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    const VkDescriptorSetLayoutCreateInfo layout_info_sp2 =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = sizeof(layout_bindings_sp2) /
            sizeof(VkDescriptorSetLayoutBinding),
        .pBindings = layout_bindings_sp2,
    };
    if (vkCreateDescriptorSetLayout(g_vulkan->d,
        &layout_info_sp2, NULL, &g_vulkan->desc_set_layout_sp2) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create descriptor "
            "set layout for subpass 2");
        return -1;
    }

    return 0;
}

/*
 * Create the command pool
 */
static i32
vulkan_create_command_pool(void)
{
    const VkCommandPoolCreateInfo pool_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = g_vulkan->qfams[VKQ_GRAPHICS].index,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    };
    if (vkCreateCommandPool(g_vulkan->d, &pool_info, NULL,
        &g_vulkan->cmd_pool) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create command pool");
        return -1;
    }
    return 0;
}

/*
 * Create the command buffers
 */
static i32
vulkan_create_command_buffers(void)
{
    g_vulkan->cmd_buffer_count = swapchain->image_count;
    g_vulkan->cmd_buffers =
        malloc(g_vulkan->cmd_buffer_count * sizeof(VkCommandBuffer));

    const VkCommandBufferAllocateInfo alloc_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = g_vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = g_vulkan->cmd_buffer_count,
    };

    if (vkAllocateCommandBuffers(g_vulkan->d, &alloc_info,
        g_vulkan->cmd_buffers) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to allocate command buffers");
        return -1;
    }

    return 0;
}

static void vulkan_record_obj_command_buffer(
    VkCommandBuffer,
    struct renderable *,
    struct shader *,
    enum shader_type,
    vec3s *);

static bool vulkan_check_should_cull_obj(struct renderable *, vec3s *);

/*
 * Record into current command buffer
 */
i32
vulkan_record_command_buffers(
    struct renderer_obj_group *objgrps,
    size_t objgrp_count,
    vec3s *cam_pos)
{
#ifdef DEBUG
    assert(objgrp_count == SHADER_COUNT);
#endif
    VkCommandBuffer cbuf = g_vulkan->cmd_buffers[cur_image_index];

    // Reset the command buffer
    vkResetCommandBuffer(g_vulkan->cmd_buffers[cur_image_index], 0);

    // Begin recording
    static const VkCommandBufferBeginInfo begin_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
    };
    if (vkBeginCommandBuffer(cbuf, &begin_info) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to begin recording command buffers");
        return -1;
    }

    // Reset draw count
    g_state.draw_calls = 0;

    /* Configure render pass 1 (light render) */
    static const VkClearValue clear_colours_p1[] =
    {
        // Lightmap buffer clear colour
        {{{ 0.0f, 0.0f, 0.0f, 1.0f }}},
    };
    VkRenderPassBeginInfo render_pass_info =
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = g_vulkan->light_render_pass,
        .framebuffer = g_vulkan->light_framebufs[cur_image_index],
        .renderArea =
        {
            .offset = { 0, 0 },
            .extent = swapchain->extent,
        },
        .clearValueCount = sizeof(clear_colours_p1) / sizeof(VkClearValue),
        .pClearValues = clear_colours_p1,
    };

    /*
     * Pass 1: lighting render
     */
    vkCmdBeginRenderPass(cbuf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the light shader
    vkCmdBindPipeline(cbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_shader_list[SHADER_LIGHT].pipeline);

    do
    {
        struct renderable *objs = objgrps[SHADER_LIGHT].objs;
        u32 obj_count = objgrps[SHADER_LIGHT].obj_count;
        if (obj_count == 0) break;

        // Render each light
        for (u32 o = 0; o < obj_count; ++o)
        {
            // Skip hidden lights
            if (objs[o].flags & RENDERABLE_HIDDEN_BIT) continue;

            // Skip lights with no indices
            if (objs[o].ib.index_count == 0) continue;

            // Cull lights that have bounds outside the viewport
        #ifndef NO_CULLING
            if (vulkan_check_should_cull_obj(&objs[o], cam_pos))
            {
                continue;
            }
        #endif

            // Render the light
            vulkan_record_obj_command_buffer(cbuf,
                &objs[o],
                &g_shader_list[SHADER_LIGHT],
                SHADER_LIGHT,
                cam_pos);
        }
    } while(0);

    // End the light render pass
    vkCmdEndRenderPass(cbuf);

    /*
     * Configure render pass 2 (main render)
     */
    static const VkClearValue clear_colours_p2[] =
    {
        // Swapchain colour buffer clear colour
        {{{ 0.0f, 0.0f, 0.0f, 1.0f }}},
        // Depth clear value
        {{{ 1.0f, 0.0f }}},
        // Colour attachment clear colour
        {{{ 0.0f, 0.0f, 0.0f, 1.0f }}},
    };
    render_pass_info = (VkRenderPassBeginInfo)
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = g_vulkan->render_pass,
        .framebuffer = swapchain->framebuffers[cur_image_index],
        .renderArea =
        {
            .offset = { 0, 0 },
            .extent = swapchain->extent,
        },
        .clearValueCount = sizeof(clear_colours_p2) / sizeof(VkClearValue),
        .pClearValues = clear_colours_p2,
    };

    // Begin the level render pass
    vkCmdBeginRenderPass(cbuf, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    /*
     * Pass 2: level G-buffer render (Subpass 1)
     */
    // Iterate over each of the groups (i.e. objects with different shaders)
    for (u32 g = 0; g < SHADER_COUNT; ++g)
    {
        u32 shader_id = SHADER_RENDER_ORDER[g];

        // We don't use the subpass 2 shader, as we're still in subpass 1...
        // Also don't render lights twice
        if (shader_id == SHADER_SCREENSUBPASS ||
            shader_id == SHADER_LIGHT ) continue;

        struct renderable *objs = objgrps[shader_id].objs;
        u32 obj_count = objgrps[shader_id].obj_count;

        // If Skip if there's no objects to render in this group
        if (obj_count == 0) continue;

        // Bind the graphics pipeline for this shader
        vkCmdBindPipeline(cbuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS, g_shader_list[shader_id].pipeline);

        // Render each object
        for (i32 o = 0; o < obj_count; ++o)
        {
            // Skip hidden objects
            if (objs[o].flags & RENDERABLE_HIDDEN_BIT) continue;

            // Skip objects with no indices
            if (objs[o].ib.index_count == 0) continue;

            // Cull objects that have bounds outside the viewport
            // Extremely effective at more than doubling the FPS
        #ifndef NO_CULLING
            if (vulkan_check_should_cull_obj(&objs[o], cam_pos))
            {
                continue;
            }
        #endif

            // Render the object
            vulkan_record_obj_command_buffer(cbuf,
                &objs[o], &g_shader_list[shader_id], shader_id, cam_pos);
        }
    }

    /*
     * Pass 2: Subpass 2: we read colour attachments here and compose G-buffer
     *         to swapchain
     */
    struct shader *shad_sp2 = &g_shader_list[SHADER_SCREENSUBPASS];
    vkCmdNextSubpass(cbuf, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shad_sp2->pipeline);
    // Push constants for shading, etc.
    f32 dim =
        theme_get_darkness_value(g_map->theme->darkness[THEME_STATE_BASE]);
    struct push_constants_sp2 sp2_pconsts =
    {
        .shade_colour = (vec4s)
        {
             g_map->theme->colours
                 [THEME_AFFECT_WORLD][THEME_STATE_BASE].x * dim,
             g_map->theme->colours
                 [THEME_AFFECT_WORLD][THEME_STATE_BASE].y * dim,
             g_map->theme->colours
                 [THEME_AFFECT_WORLD][THEME_STATE_BASE].z * dim,
             1.0f,
        },
        .env.texcoord_mul = g_map->theme_env_tex.dilation,
        .env.texcoord_offset = g_map->theme_env_tex.offset,
    };
    vkCmdPushConstants(cbuf,
        shad_sp2->pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(struct push_constants_sp2),
        &sp2_pconsts);
    vkCmdBindDescriptorSets(cbuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shad_sp2->pipeline_layout,
        0, 1,
        &g_vulkan->desc_sets_sp2[cur_image_index],
        0, NULL);
    vkCmdDraw(cbuf, 3, 1, 0, 0);

    // End render pass and end command buffer recording
    vkCmdEndRenderPass(cbuf);
    if (vkEndCommandBuffer(cbuf) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to record command buffer");
        return -1;
    }
    return 0;
}

// Record to command buffer for a single object
static void
vulkan_record_obj_command_buffer(
    VkCommandBuffer cbuf,
    struct renderable *obj,
    struct shader *s,
    enum shader_type shader_id,
    vec3s *cam_pos)
{
    static const VkDeviceSize offset = 0;

    // Bind vertex and index buffers
    vkCmdBindVertexBuffers(cbuf,
        0,
        1,
        &obj->vb.vk_buffer,
        &offset);
    vkCmdBindIndexBuffer(cbuf,
        obj->ib.vk_buffer,
        0,
        IB_VKTYPE);

    // Put MVP in push constants
    f32 flip_sign = -((f32)!!(obj->flags &
        RENDERABLE_FLIPPED_BIT) * 2.0f - 1.0f);
    mat4s m_m = (mat4s)GLMS_MAT4_IDENTITY_INIT;

    // Apply object position
    m_m = glms_translate(m_m, (vec3s)
    {
        obj->pos.x + obj->offset.x * flip_sign,
        obj->pos.y - obj->offset.y,
        0.0f
    });
    // Apply object scale
    if (obj->flags & RENDERABLE_SCALED_BIT)
    {
        m_m = glms_scale(m_m, (vec3s)
        {
            obj->scale, obj->scale, 1.0f
        });
    }

    m_m.raw[0][0] *= flip_sign;
    m_m.raw[1][1] *= -1.0f;
    if (obj->rot != 0.0f)
    {
        // Apply object rotation
        m_m = glms_rotate_z(m_m, glm_rad(obj->rot));
    }
    if (obj->flags & RENDERABLE_TEX_SCALE_BIT)
    {
        // Scale the object by texture size
        m_m = glms_scale(m_m, (vec3s)
        {
            (f32)g_vulkan->textures[obj->tex].w,
            (f32)g_vulkan->textures[obj->tex].h,
            1.0f,
        });
    }
    mat4s m_v = (mat4s)GLMS_MAT4_IDENTITY_INIT;

    // Apply camera position
    m_v = glms_translate(m_v,
        (vec3s){ -cam_pos->x, -cam_pos->y, 0.0f });

    // Projection matrix
    mat4s m_p = glms_ortho(
        0.0f, WIDTH_INTERNAL,
        0.0f, HEIGHT_INTERNAL,
        -225.0f, 225.0f);

    // A bit dodgey but works
    size_t pconst_size = s->pconst_size;
    void *pconsts = alloca(pconst_size);
    if (shader_id == SHADER_DEFAULT || shader_id == SHADER_DEFAULT_NO_ZBUFFER)
    {
        // Shading multiplier
        f32 dim = (obj->flags & RENDERABLE_SHADED_BIT) ? 0.60f : 1.0f;
        vec4s shading =
            (obj->flags & RENDERABLE_EXTRA_SHADING_BIT)
            ? obj->extra_shading
            : GLMS_VEC4_ONE;
        shading.x *= dim;
        shading.y *= dim;
        shading.z *= dim;

        struct push_constants p =
        {
            .mvp = glms_mat4_mul(m_p, glms_mat4_mul(m_v, m_m)),
            .shading = shading,
            .tex_offset = obj->tex_offset,
            .tex_index = obj->tex,
        };
        memcpy(pconsts, &p, pconst_size);
    }
    else if (shader_id == SHADER_VERTEXLIT || shader_id == SHADER_PARTICLE)
    {
        struct push_constants_vl p =
        {
            .mvp = glms_mat4_mul(m_p, glms_mat4_mul(m_v, m_m)),
        };
        memcpy(pconsts, &p, pconst_size);
    }
    else if (shader_id == SHADER_LIGHT)
    {
        struct push_constants_light p =
        {
            .mvp = glms_mat4_mul(m_p, glms_mat4_mul(m_v, m_m)),
            .colour = obj->light_colour,
            .tex_index = obj->tex,
        };
        memcpy(pconsts, &p, pconst_size);
    }

    // Push constants
    vkCmdPushConstants(cbuf,
        s->pipeline_layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        pconst_size,
        pconsts);

    // Bind descriptor sets
    if (s->use_descriptor_sets)
    {
        // This is a regular shader, so bind the normal descriptor set
        vkCmdBindDescriptorSets(cbuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            s->pipeline_layout,
            0, 1,
            &g_vulkan->desc_sets[cur_image_index],
            0, NULL);
    }

    // Draw!
    vkCmdDrawIndexed(cbuf,
        obj->ib.index_count,
        1, 0, 0, 0);
    ++g_state.draw_calls;
}

/* Check whether object should be culled */
static bool
vulkan_check_should_cull_obj(struct renderable *o, vec3s *cam_pos)
{
    if (o->flags & RENDERABLE_NO_CULL_BIT)
    {
        return false;
    }

    struct bounds
    {
        vec2s min, max;
    };
    const struct bounds viewport_bounds =
    {
        .min = (vec2s)
        {{
            cam_pos->x,
            -cam_pos->y - HEIGHT_INTERNAL,
        }},
        .max = (vec2s)
        {{
            cam_pos->x + WIDTH_INTERNAL,
            -cam_pos->y,
        }},
    };
    struct bounds obj_bounds;
    if (!(o->flags & RENDERABLE_TEX_SCALE_BIT))
    {
        obj_bounds = (struct bounds)
        {
            .min = (vec2s)
            {{
                o->pos.x + o->bounds.min.x,
                -o->pos.y + o->bounds.min.y
            }},
            .max = (vec2s)
            {{
                o->pos.x + o->bounds.max.x,
                -o->pos.y + o->bounds.max.y
            }},
        };
    }
    else
    {
        // Using texture size as bounds
        f32 tw = (f32)g_vulkan->textures[o->tex].w / 2.0f,
            th = (f32)g_vulkan->textures[o->tex].h / 2.0f;
        obj_bounds = (struct bounds)
        {
            .min = (vec2s)
            {{
                o->pos.x - tw,
                -o->pos.y - tw
            }},
            .max = (vec2s)
            {{
                o->pos.x + tw,
                -o->pos.y + th
            }},
        };
    }
    bool in_aabb =
        obj_bounds.min.x < viewport_bounds.max.x &&
        obj_bounds.max.x > viewport_bounds.min.x &&
        obj_bounds.min.y < viewport_bounds.max.y &&
        obj_bounds.max.y > viewport_bounds.min.y;
    if (!in_aabb)
    {
        // Outside of viewport, so don't render it
        return true;
    }
    return false;
}


static i32
vulkan_create_sync_objects(void)
{
    VkSemaphoreCreateInfo semaphore_info =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info =
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        // Need to start signalled so that vkWaitForFences doesn't wait forever
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    in_flight_images = calloc(swapchain->image_count, sizeof(VkFence));

    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        if (vkCreateSemaphore(g_vulkan->d, &semaphore_info, NULL,
                &semaphore_img_available[i]) != VK_SUCCESS ||
            vkCreateSemaphore(g_vulkan->d, &semaphore_info, NULL,
                &semaphore_render_finish[i]) != VK_SUCCESS ||
            vkCreateFence(g_vulkan->d, &fence_info, NULL,
                &in_flight_fences[i]) != VK_SUCCESS)
        {
            LOG_ERROR("[vulkan] failed to create synchronisation objects "
                "for a frame!");
            return -1;
        }
    }
    return 0;
}

i32
vulkan_render_frame_pre(void)
{
    /*
     * Wait for frame to finish
     */
    vkWaitForFences(g_vulkan->d, 1,
        &in_flight_fences[cur_frame], VK_TRUE, UINT64_MAX);

    /*
     * Acquire an image from the swap chain
     */
    vkAcquireNextImageKHR(g_vulkan->d,
        swapchain->handle,
        UINT64_MAX,
        semaphore_img_available[cur_frame],
        VK_NULL_HANDLE,
        &cur_image_index);

    // Check if previous frame is using this image
    // (i.e. there's a fence to wait on)
    if (in_flight_images[cur_image_index] != VK_NULL_HANDLE)
    {
        vkWaitForFences(g_vulkan->d, 1,
            &in_flight_images[cur_image_index], VK_TRUE, UINT64_MAX);
    }
    in_flight_images[cur_image_index] =
        in_flight_fences[cur_frame];

    vkResetFences(g_vulkan->d, 1, &in_flight_fences[cur_frame]);

    // Record commands
    // ... done via vulkan_record_command_buffers
    return 0;
}


i32
vulkan_render_frame(void)
{
    /*
     * Submit the command buffer
     */
    VkSemaphore signal_semaphores[] =
    {
        semaphore_render_finish[cur_frame],
    };
    const VkPipelineStageFlags wait_stages[] =
    {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };
    const VkSubmitInfo submit_info =
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &semaphore_img_available[cur_frame],
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &g_vulkan->cmd_buffers[cur_image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };
    if (vkQueueSubmit(
        g_vulkan->qfams[VKQ_GRAPHICS].queue,
        1,
        &submit_info,
        in_flight_fences[cur_frame]) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to submit draw command buffer!");
        return -1;
    }

    /*
     * Present image to the swap chain!
     */
    const VkPresentInfoKHR present_info =
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->handle,
        .pImageIndices = &cur_image_index,
    };
    vkQueuePresentKHR(
        g_vulkan->qfams[VKQ_PRESENT].queue,
        &present_info);

    cur_frame = (cur_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    return 0;
}

static VkCommandBuffer
vulkan_begin_oneshot_cmd(void)
{
    // TODO: use temporary command pool for these short-lived command buffers?
    const VkCommandBufferAllocateInfo alloc_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = g_vulkan->cmd_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer cmdbuf = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(g_vulkan->d, &alloc_info,
        &cmdbuf) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to allocate command buffer");
        return VK_NULL_HANDLE;
    }
    // Record command buffer
    const VkCommandBufferBeginInfo begin_info =
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    if (vkBeginCommandBuffer(cmdbuf, &begin_info) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to begin recording command buffer");
        return VK_NULL_HANDLE;
    }

    return cmdbuf;
}

static i32
vulkan_end_oneshot_cmd(VkCommandBuffer cmdbuf)
{
    // End recording
    if (vkEndCommandBuffer(cmdbuf) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to record command buffer");
        goto fail;
    }

    // Submit the command
    const VkSubmitInfo submit_info =
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdbuf,
    };
    if (vkQueueSubmit(
        g_vulkan->qfams[VKQ_GRAPHICS].queue,
        1,
        &submit_info,
        VK_NULL_HANDLE) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to submit buffer transfer command buffer!");
        goto fail;
    }

    // Wait for command to complete
    vkQueueWaitIdle(g_vulkan->qfams[VKQ_GRAPHICS].queue);

    // Free the staging buffer as we no longer need it
    vkFreeCommandBuffers(g_vulkan->d, g_vulkan->cmd_pool, 1, &cmdbuf);
    return 0;
fail:
    vkFreeCommandBuffers(g_vulkan->d, g_vulkan->cmd_pool, 1, &cmdbuf);
    return -1;
}

static i32
vulkan_create_descriptor_pool()
{
    VkDescriptorPoolSize pool_sizes[] =
    {
        {
            // Sampler for all textures
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = swapchain->image_count,
        },
        {
            // Subpass 1: textures used in level, etc.
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = swapchain->image_count * MAX_TEXTURES,
        },
        {
            // Subpass 2: G-buffer attachment
            .type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = swapchain->image_count,
        },
        {
            // Subpass 2: "lightmap" texture
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = swapchain->image_count * 2,
        },
    };
    VkDescriptorPoolCreateInfo pool_info =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize),
        .pPoolSizes = pool_sizes,
        .maxSets = swapchain->image_count * 2,
    };
    if (vkCreateDescriptorPool(g_vulkan->d,
        &pool_info, NULL, &g_vulkan->desc_pool) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create descriptor pool");
        return -1;
    }

    return 0;
}

static i32
vulkan_setup_textures(void)
{
    // Create the global texture sampler
    const VkSamplerCreateInfo sampler_info =
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR, //VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_LINEAR, //VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1,
        .unnormalizedCoordinates = VK_FALSE,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
        .mipLodBias = 0.0f,
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    if (vkCreateSampler(g_vulkan->d,
        &sampler_info, NULL, &g_vulkan->sampler) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create texture sampler");
        return -1;
    }

    g_vulkan->sampler_desc_info = (VkDescriptorImageInfo)
    {
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .sampler = g_vulkan->sampler,
    };

    // Create descriptor sets
    VkDescriptorSetLayout *layouts =
        malloc(swapchain->image_count * sizeof(VkDescriptorSetLayout));
    for (u32 i = 0; i < swapchain->image_count; ++i)
    {
        layouts[i] = g_vulkan->desc_set_layout;
    }
    const VkDescriptorSetAllocateInfo alloc_info =
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = g_vulkan->desc_pool,
        .descriptorSetCount = swapchain->image_count,
        .pSetLayouts = layouts,
    };

    // Allocate descriptor sets
    g_vulkan->desc_sets =
        malloc(swapchain->image_count * sizeof(VkDescriptorSet));
    if (vkAllocateDescriptorSets(g_vulkan->d,
        &alloc_info, g_vulkan->desc_sets) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to allocate descriptor sets");
        free(layouts);
        return -1;
    }
    free(layouts);

    // Create the default 1x1 white texture
    g_vulkan->tex_used = 0;
    u8 default_tex[] = { 0xff, 0xff, 0xff, 0xff };
    if (vulkan_texture_create(default_tex, 1, 1, 1 * 1 * 4, 0, 0, NULL) != 0)
    {
        LOG_ERROR("[vulkan] failed to create default texture");
        return -1;
    }

    g_vulkan->light_tex =
        malloc(sizeof(struct vulkan_texture) * swapchain->image_count);
    g_vulkan->light_framebufs =
        malloc(sizeof(VkFramebuffer) * swapchain->image_count);
    u32 lightmap_w = swapchain->extent.width,
        lightmap_h = swapchain->extent.height;
    for (i32 i = 0; i < swapchain->image_count; ++i)
    {
        /*
         * Create the "lightmap" textures
         */
        if (vulkan_texture_create(NULL,
            lightmap_w,
            lightmap_h, 0,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            &g_vulkan->light_tex[i]) < 0)
        {
            LOG_ERROR("[vulkan] failed to create lightmap texture");
            return -1;
        }

        /*
         * Create lightmap framebuffers
         */
        VkFramebufferCreateInfo fb_info =
        {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = g_vulkan->light_render_pass,
            .attachmentCount = 1,
            .pAttachments = &g_vulkan->light_tex[i].view,
            .width  = lightmap_w,
            .height = lightmap_h,
            .layers = 1,
        };

        if (vkCreateFramebuffer(g_vulkan->d, &fb_info, NULL,
            &g_vulkan->light_framebufs[i]) != VK_SUCCESS)
        {
            LOG_ERROR("[vulkan] failed to lightmap framebuffer");
            return -1;
        }
    }

    return 0;
}

i32
vulkan_create_buffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage usage_vma,
    VkMemoryPropertyFlags props,
    VkBuffer *buffer,
    VmaAllocation *allocation)
{
    const VkBufferCreateInfo buffer_info =
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    // Use VulkanMemoryAllocator to allocate for us
    const VmaAllocationCreateInfo vma_alloc_info =
    {
        .usage = usage_vma,
        .requiredFlags = props,
    };
    if (vmaCreateBuffer(g_vulkan->vma,
        &buffer_info,
        &vma_alloc_info,
        buffer,
        allocation, NULL) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create buffer");
        return -1;
    }

    return 0;
}

i32
vulkan_copy_buffer(VkBuffer a, VkBuffer b, size_t size)
{
    VkCommandBuffer cmdbuf;
    if ((cmdbuf = vulkan_begin_oneshot_cmd()) == VK_NULL_HANDLE) return -1;

    // Set buffers to copy
    const VkBufferCopy copy_region =
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = (VkDeviceSize)size,
    };
    vkCmdCopyBuffer(cmdbuf, a, b, 1, &copy_region);

    if (vulkan_end_oneshot_cmd(cmdbuf) < 0) return -1;

    return 0;
}

static i32
vulkan_copy_buffer_to_image(VkBuffer buffer, VkImage img, u32 w, u32 h)
{
    VkCommandBuffer cmdbuf;
    if ((cmdbuf = vulkan_begin_oneshot_cmd()) == VK_NULL_HANDLE) return -1;

    const VkBufferImageCopy rgn =
    {
        // Tightly packed
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        // Which parts we want to copy
        .imageSubresource =
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = { 0, 0, 0 },
        .imageExtent =
        {
            w, h, 1
        },
    };

    vkCmdCopyBufferToImage(cmdbuf,
        buffer,
        img,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &rgn);

    if (vulkan_end_oneshot_cmd(cmdbuf) < 0) return -1;
    return 0;
}

static i32
transition_image_layout(VkImage img, VkFormat fmt,
    VkImageLayout layout_old, VkImageLayout layout_new)
{
    VkCommandBuffer cmdbuf;
    if ((cmdbuf = vulkan_begin_oneshot_cmd()) == VK_NULL_HANDLE) return -1;

    VkImageMemoryBarrier barrier =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = layout_old,
        .newLayout = layout_new,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = img,
        .subresourceRange =
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkPipelineStageFlags src_stage, dst_stage;
    // undefined --> transfer destination: transfer writes don't need to wait
    //                                     for anything
    if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED &&
        layout_new == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (layout_old == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (layout_old == VK_IMAGE_LAYOUT_UNDEFINED &&
        layout_new == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        LOG_ERROR("[vulkan] unsupported layout transition");
        vulkan_end_oneshot_cmd(cmdbuf);
        return -1;
    }

    vkCmdPipelineBarrier(cmdbuf,
        src_stage,
        dst_stage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier);

    if (vulkan_end_oneshot_cmd(cmdbuf) < 0) return -1;
    return 0;
}

static i32
vulkan_texture_create(u8 *pixels, i32 w, i32 h,
    VkDeviceSize size,
    VkImageUsageFlagBits usage,
    VkFormat format,
    struct vulkan_texture *tex_out)
{
    bool transfer = false;
    VkBuffer staging_buf;
    VmaAllocation staging_buf_alloc;

    struct vulkan_texture *tex;
    i32 tex_index;
    if (tex_out == NULL)
    {
        // Check that we don't exceed maximum textures
        if (g_vulkan->tex_used + 1 >= MAX_TEXTURES)
        {
            LOG_ERROR("[vulkan] texture capacity (%d) exceeded",
                g_vulkan->tex_used);
            return -1;
        }
        tex_index = g_vulkan->tex_used;
        tex = &g_vulkan->textures[tex_index];
        ++g_vulkan->tex_used;
    }
    else
    {
        // A custom output location was specified
        tex = tex_out;
        tex_index = 0;
    }

    // Reset name and set dimensions
    tex->name[0] = '\0';
    tex->w = (u32)w;
    tex->h = (u32)h;
    if (format == VK_FORMAT_UNDEFINED)
    {
        tex->format = VK_FORMAT_R8G8B8A8_SRGB;
    }
    else
    {
        tex->format = format;
    }

    // If we pass NULL for pixels then don't transfer
    if (!pixels) goto skip_tex_transfer;

    transfer = true;

    // Create staging buffer
    if (vulkan_create_buffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_ONLY,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buf,
        &staging_buf_alloc) < 0)
    {
        LOG_ERROR("[vulkan] failed to create staging buffer!");
        return -1;
    }

    // Copy pixel data
    void *data;
    vmaMapMemory(g_vulkan->vma, staging_buf_alloc, &data);
    memcpy(data, pixels, size);
    vmaUnmapMemory(g_vulkan->vma, staging_buf_alloc);
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

skip_tex_transfer:

    const VkImageCreateInfo image_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent =
        {
            .width = w,
            .height = h,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = tex->format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0,
    };
    const VmaAllocationCreateInfo image_alloc_info =
    {
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    if (vmaCreateImage(g_vulkan->vma,
        &image_info,
        &image_alloc_info,
        &tex->image,
        &tex->alloc, NULL) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create image");
        goto fail;
    }

    // Transition image layout, and copy buffer
    if (transfer)
    {
        transition_image_layout(tex->image, tex->format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        vulkan_copy_buffer_to_image(staging_buf, tex->image, w, h);

        // Prepare image for shader access
        transition_image_layout(tex->image, tex->format,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Destroy the staging buffer
        vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);
    }
    else
    {
        transition_image_layout(tex->image, tex->format,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Create imageview
    VkImageViewCreateInfo view_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = tex->image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = tex->format,
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
        &view_info,
        NULL,
        &tex->view) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create image view for texture");
        goto fail;
    }

    // Re-write descriptors if needed
    if (g_vulkan->in_level)
    {
        vulkan_rewrite_descriptors();
        LOG_DBUG("[vulkan] rewrite texture descriptors");
    }

    return tex_index;
fail:
    if (transfer)
    {
        vmaDestroyBuffer(g_vulkan->vma, staging_buf, staging_buf_alloc);
    }
    return -1;
}

i32
vulkan_texture_load(const char *path)
{
    // Check if texture already is loaded (probably pretty slow)
    for (u32 i = 1; i < g_vulkan->tex_used; ++i)
    {
        struct vulkan_texture *tex = &g_vulkan->textures[i];
        if (strcmp(tex->name, path) == 0)
        {
            return i;
        }
    }

    // Load texture data
    i32 w, h, ch;
    stbi_uc *pixels = stbi_load(path,
        &w, &h, &ch, STBI_rgb_alpha);
    VkDeviceSize size = w * h * 4;
    if (!pixels)
    {
        LOG_ERROR("[stb_image] failed to load texture '%s'",
            path);
        return -1;
    }

    //LOG_DBUG("[stb_image] loaded image '%s'", path);

    // Create texture
    i32 status = vulkan_texture_create(pixels, w, h, size, 0, 0, NULL);

    stbi_image_free(pixels);

    if (status > 0)
    {
        strcpy(g_vulkan->textures[status].name, path);
    }

    return status;
}

i32
vulkan_level_begin(void)
{
    // Free up all the textures (except reserved)
    g_vulkan->in_level = false;
    for (u32 i = RESERVED_TEXTURE_COUNT; i < g_vulkan->tex_used; ++i)
    {
        vkDestroyImageView(g_vulkan->d, g_vulkan->textures[i].view, NULL);
        vmaDestroyImage(g_vulkan->vma,
            g_vulkan->textures[i].image,
            g_vulkan->textures[i].alloc);
    }
    g_vulkan->tex_used = RESERVED_TEXTURE_COUNT;

    return 0;
}

i32
vulkan_level_end(void)
{
    i32 status = vulkan_rewrite_descriptors();
    g_vulkan->in_level = true;
    return status;
}

i32
vulkan_update_sp2_descriptors(void)
{
    if (!g_vulkan->desc_sets_sp2)
    {
        // Create descriptor sets
        VkDescriptorSetLayout *layouts =
            malloc(swapchain->image_count * sizeof(VkDescriptorSetLayout));
        for (u32 i = 0; i < swapchain->image_count; ++i)
        {
            layouts[i] = g_vulkan->desc_set_layout_sp2;
        }
        const VkDescriptorSetAllocateInfo alloc_info =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = g_vulkan->desc_pool,
            .descriptorSetCount = swapchain->image_count,
            .pSetLayouts = layouts,
        };

        // Allocate descriptor sets
        g_vulkan->desc_sets_sp2 =
            malloc(swapchain->image_count * sizeof(VkDescriptorSet));
        if (vkAllocateDescriptorSets(g_vulkan->d,
            &alloc_info, g_vulkan->desc_sets_sp2) != VK_SUCCESS)
        {
            LOG_ERROR("[vulkan] failed to allocate descriptor sets for subpass 2");
            free(layouts);
            return -1;
        }
        free(layouts);
    }

    for (u32 i = 0; i < swapchain->image_count; ++i)
    {
        const VkDescriptorImageInfo
        info_gbuffer =
        {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = swapchain->attachments[i].colour.view,
            .sampler = VK_NULL_HANDLE,
        },
        info_lightmap =
        {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = g_vulkan->light_tex[i].view,
            .sampler = g_vulkan->sampler,
        },
        info_envtex =
        {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = g_vulkan->textures[g_vulkan->env_tex_index].view,
            .sampler = g_vulkan->sampler,
        };

        const VkWriteDescriptorSet set_writes[] =
        {
            {
                // G-buffer
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = g_vulkan->desc_sets_sp2[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
                .descriptorCount = 1,
                .pImageInfo = &info_gbuffer,
            },
            {
                // "Lightmap"
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = g_vulkan->desc_sets_sp2[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &info_lightmap,
            },
            {
                // Environment (rain) texture
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = g_vulkan->desc_sets_sp2[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &info_envtex,
            },
        };

        vkUpdateDescriptorSets(g_vulkan->d, sizeof(set_writes) /
            sizeof(VkWriteDescriptorSet), set_writes, 0, NULL);
    }
    return 0;
}

static i32
vulkan_rewrite_descriptors(void)
{
    if (!g_vulkan->image_desc_infos)
    {
        g_vulkan->image_desc_infos =
            malloc(MAX_TEXTURES * sizeof(VkDescriptorImageInfo));
    }

    // Set all image infos
    for (u32 i = 0; i < MAX_TEXTURES; ++i)
    {
        // Just reset the non-used infos to the default texture at index 0
        i32 index = i;
        if (index >= g_vulkan->tex_used) index = 0;

        g_vulkan->image_desc_infos[i] = (VkDescriptorImageInfo)
        {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = g_vulkan->textures[index].view,
            .sampler = VK_NULL_HANDLE,
        };
    }

    // Update descriptor sets
    for (u32 i = 0; i < swapchain->image_count; ++i)
    {
        const VkWriteDescriptorSet set_writes[] =
        {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = g_vulkan->desc_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &g_vulkan->sampler_desc_info,
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = g_vulkan->desc_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = MAX_TEXTURES,
                .pImageInfo = g_vulkan->image_desc_infos,
            },
        };

        vkUpdateDescriptorSets(g_vulkan->d,
            sizeof(set_writes) / sizeof(VkWriteDescriptorSet),
            set_writes,
            0, NULL);
    }
    return 0;
}

/* Create the depth buffer image */
static i32
vulkan_create_zbuffer(void)
{
    VkFormat fmt = VK_FORMAT_D32_SFLOAT;

    i32 w = swapchain->extent.width,
        h = swapchain->extent.height;

    // Create image
    const VkImageCreateInfo image_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent =
        {
            .width = w,
            .height = h,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = fmt,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .flags = 0,
    };
    const VmaAllocationCreateInfo image_alloc_info =
    {
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    if (vmaCreateImage(g_vulkan->vma,
        &image_info,
        &image_alloc_info,
        &g_vulkan->zbuf_image,
        &g_vulkan->zbuf_alloc, NULL) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create Z-buffer image");
        goto fail;
    }

    // Create imageview
    VkImageViewCreateInfo view_info =
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = g_vulkan->zbuf_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = fmt,
        .subresourceRange =
        {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    if (vkCreateImageView(g_vulkan->d,
        &view_info,
        NULL,
        &g_vulkan->zbuf_view) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create image view for Z-buffer");
        goto fail;
    }

    return 0;
fail:
    return -1;
}
