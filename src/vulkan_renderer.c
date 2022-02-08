#include "pch.h"
#include "tagap.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"
#include "vertex_buffer.h"
#include "index_buffer.h"

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
static i32 vulkan_create_graphics_pipeline(void);
static i32 vulkan_create_command_pool(void);
static i32 vulkan_create_sync_objects(void);
static i32 vulkan_create_command_buffers(void);

struct push_constants
{
    mat4s mvp;
};

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

    (void)((status = vulkan_create_instance(handle)) < 0 ||
    (status = vulkan_create_surface(handle)) < 0 ||
    (status = vulkan_get_physical_device()) < 0 ||
    (status = vulkan_create_logical_device()) < 0 ||
    (status = vulkan_create_allocator()) < 0 ||
    (status = vulkan_swapchain_create(swapchain)) < 0 ||
    (status = vulkan_create_render_pass()) < 0 ||
    (status = vulkan_create_graphics_pipeline()) < 0 ||
    (status = vulkan_swapchain_create_framebuffers(swapchain)) < 0 ||
    (status = vulkan_create_command_pool() < 0) ||
    (status = vulkan_create_sync_objects() < 0) ||
    (status = vulkan_create_command_buffers() < 0));
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
    vkDestroyPipeline(g_vulkan->d, g_vulkan->pipeline, NULL);
    vkDestroyPipelineLayout(g_vulkan->d, g_vulkan->pipeline_layout, NULL);
    vkDestroyRenderPass(g_vulkan->d, g_vulkan->render_pass, NULL);
    vulkan_swapchain_deinit(swapchain);
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

#ifdef DEBUG
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
#endif

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

#ifdef DEBUG
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
#endif

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
    // Colour buffer attachment description
    VkAttachmentDescription colour_attachment =
    {
        .format = swapchain->format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        // May use DONT_CARE if we can guarantee drawing over the entire screen
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        // Not using stencil buffer
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
        // Refers to our one and only subpass
        .dstSubpass = 0,
        
        // Operations to wait on, we wait for swap chain to finish reading from
        // the image
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,

        // Wait on the writing of colour attachment
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
        NULL, &g_vulkan->render_pass) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create render pass");
        return -1;
    }

    return 0;
}

VkShaderModule
create_shader_module(const u32 *buf, size_t len)
{
    VkShaderModuleCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = len,
        .pCode = buf,
    };

    VkShaderModule module;
    if (vkCreateShaderModule(g_vulkan->d, 
        &create_info, NULL, &module) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create shader module");
    }
    return module;
}

/*
 * Create the graphics pipeline
 */
static i32
vulkan_create_graphics_pipeline(void)
{
    /*
     * Read shaders
     * TODO: better shader management
     */
#define SHADER_VERT_PATH "shader/default.vert.spv"
#define SHADER_FRAG_PATH "shader/default.frag.spv"
    // Load vertex shader
    FILE *fp = fopen(SHADER_VERT_PATH, "rb");
    if (!fp)
    {
        LOG_ERROR("[vulkan] failed to read vertex shader file '%s'",
            SHADER_VERT_PATH);
        return -1;
    }

    // Read vertex shader size
    fseek(fp, 0, SEEK_END);
    size_t vert_len = ftell(fp);
    rewind(fp);
    u32 *vert_data = malloc(vert_len * sizeof(u32));

    // Read vertex shader data
    size_t n = fread(vert_data, vert_len, 1, fp);
    fclose(fp);

    // Load fragment shader
    fp = fopen(SHADER_FRAG_PATH, "rb");
    if (!fp)
    {
        LOG_ERROR("[vulkan] failed to read fragment shader file '%s'",
            SHADER_FRAG_PATH);
        free(vert_data);
        return -1;
    }

    // Read fragment shader size
    fseek(fp, 0, SEEK_END);
    size_t frag_len = ftell(fp);
    rewind(fp);
    u32 *frag_data = malloc(frag_len * sizeof(u32));

    n = fread(frag_data, frag_len, 1, fp);
    (void)n;
    fclose(fp);

    VkShaderModule vert = create_shader_module(vert_data, vert_len);
    VkShaderModule frag = create_shader_module(frag_data, frag_len);

    free(vert_data);
    free(frag_data);

    /*
     * Create shader stages
     */
    VkPipelineShaderStageCreateInfo vert_stage_info = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert,
        .pName = "main",
    },
    frag_stage_info = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag,
        .pName = "main",
    };
    const VkPipelineShaderStageCreateInfo shader_stages[] = 
    {
        vert_stage_info, frag_stage_info
    };

    /*
     * Vertex input
     */
    VkPipelineVertexInputStateCreateInfo vertex_input_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &VERTEX_BINDING_DESC,
        .vertexAttributeDescriptionCount = VERTEX_ATTR_COUNT,
        .pVertexAttributeDescriptions = VERTEX_ATTR_DESC,
    };

    /*
     * Input assembly
     */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    /*
     * Viewport state
     */
    VkViewport viewport =
    {
        .x = 0.0f,
        .y = 0.0f,
        .width = (f32)swapchain->extent.width,
        .height = (f32)swapchain->extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor = 
    {
        .offset = { 0, 0 },
        .extent = swapchain->extent,
    };
    VkPipelineViewportStateCreateInfo viewport_state_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    /*
     * Rasteriser
     */
    VkPipelineRasterizationStateCreateInfo rasteriser_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        // Can modify this for wireframe mode
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,

        // Flip faces because we render with flipped Y
        //.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, 
        .frontFace = VK_FRONT_FACE_CLOCKWISE, 
    };

    /*
     * Multisampler
     */
    VkPipelineMultisampleStateCreateInfo multisample_info = 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    /*
     * Colour blending
     */
    VkPipelineColorBlendAttachmentState colour_blend_attachment =
    {
        .colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT | 
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo colour_blend_state_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colour_blend_attachment,
    };

    /*
     * Push constants
     */
    const VkPushConstantRange push_consts =
    {
        .offset = 0,
        .size = sizeof(struct push_constants),
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };

    /*
     * Pipeline layout
     */
    VkPipelineLayoutCreateInfo pipeline_layout_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges = &push_consts,
        .pushConstantRangeCount = 1,
    };
    if (vkCreatePipelineLayout(g_vulkan->d, 
        &pipeline_layout_info, NULL, &g_vulkan->pipeline_layout) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create pipeline layout");
        return -1;
    }

    /*
     * Finally create the damn pipeline
     */
    VkGraphicsPipelineCreateInfo pipeline_info = 
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = sizeof(shader_stages) / 
            sizeof(VkPipelineShaderStageCreateInfo),
        .pStages = shader_stages,

        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasteriser_info,
        // Can we use NULL here?
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = NULL,
        .pColorBlendState = &colour_blend_state_info,
        .pDynamicState = NULL,
        
        .layout = g_vulkan->pipeline_layout,
        .renderPass = g_vulkan->render_pass,
        .subpass = 0,
    };
    if (vkCreateGraphicsPipelines(g_vulkan->d, VK_NULL_HANDLE, 1,
        &pipeline_info, NULL, &g_vulkan->pipeline) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create graphics pipeline");
        return -1;
    }

    vkDestroyShaderModule(g_vulkan->d, frag, NULL);
    vkDestroyShaderModule(g_vulkan->d, vert, NULL);

    LOG_INFO("[vulkan] graphics pipeline created!");

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

/*
 * Record into current command buffer
 */
i32
vulkan_record_command_buffers(
    struct renderable *objs, 
    size_t obj_count)
{
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

    // Configure render pass
    static const VkClearValue clear_colour = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
    VkRenderPassBeginInfo render_pass_info = 
    {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = g_vulkan->render_pass,
        .framebuffer = swapchain->framebuffers[cur_image_index],
        .renderArea =
        {
            .offset = { 0, 0 },
            .extent = swapchain->extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear_colour,
    };

    // Begin the render pass
    vkCmdBeginRenderPass(cbuf, &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(cbuf, 
        VK_PIPELINE_BIND_POINT_GRAPHICS, g_vulkan->pipeline);

    static const VkDeviceSize offset = 0;
    for (i32 o = 0; o < obj_count; ++o)
    {
        // Bind vertex and index buffers
        vkCmdBindVertexBuffers(cbuf, 
            0, 
            1, 
            &objs[o].vb.vk_buffer, 
            &offset);
        vkCmdBindIndexBuffer(cbuf, 
            objs[o].ib.vk_buffer, 
            0, 
            VK_INDEX_TYPE_UINT16);

        // Put MVP in push constants
        mat4s m_m = (mat4s)GLMS_MAT4_IDENTITY_INIT;
        m_m = glms_translate(m_m, 
            (vec3s){ objs[o].pos.x, objs[o].pos.y, 0.0f });
        if (objs[o].rot != 0.0f)
        {
            m_m = glms_rotate_z(m_m, glm_rad(objs[o].rot));
        }
        static mat4s m_v = (mat4s)GLMS_MAT4_IDENTITY_INIT;
        mat4s m_p = glms_ortho(
            0.0f, (f32)swapchain->extent.width, 
            0.0f, (f32)swapchain->extent.height,
            -1.0f, 1.0f);
        const struct push_constants pconsts =
        {
            .mvp = glms_mat4_mul(m_p, glms_mat4_mul(m_v, m_m)),
        };
        vkCmdPushConstants(cbuf,
            g_vulkan->pipeline_layout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(struct push_constants),
            &pconsts);

        // Draw!
        vkCmdDrawIndexed(cbuf, 
            objs[o].ib.size / sizeof(u16), 
            1, 0, 0, 0);
    }

    // End render pass and end command buffer recording
    vkCmdEndRenderPass(cbuf);
    if (vkEndCommandBuffer(cbuf) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to record command buffer");
        return -1;
    }
    return 0;
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
        goto fail;
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
        goto fail;
    }

    // Set buffers to copy
    const VkBufferCopy copy_region = 
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = (VkDeviceSize)size,
    };
    vkCmdCopyBuffer(cmdbuf, a, b, 1, &copy_region);

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
