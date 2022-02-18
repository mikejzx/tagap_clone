#include "pch.h"
#include "shader.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"

// List of shaders used in the game
struct shader 
g_shader_list[SHADER_COUNT] =
{
    // Default shader used for rendering polygons in the level
    [SHADER_DEFAULT] =
    {
        .name = "default",
        .pconst_size = sizeof(struct push_constants),
        .use_descriptor_sets = true,
        .depth_test = true,

        .vertex_binding_desc = (VkVertexInputBindingDescription)
        {
            .binding = 0,
            .stride = sizeof(struct vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .vertex_attr_desc =
        {
            // #1: vertex position
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(struct vertex, pos),
            },
            // #2: texcoord
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(struct vertex, texcoord),
            },
        },
        .vertex_attr_count = 2,
    },
    // Same as default shader except this does not use depth testing; we use
    // this for entities, etc. to allow for full proper alpha without glitching
    // out because of the depth buffer
    [SHADER_DEFAULT_NO_ZBUFFER] =
    {
        .name = "default",
        .pconst_size = sizeof(struct push_constants),
        .use_descriptor_sets = true,
        .depth_test = false,

        .vertex_binding_desc = (VkVertexInputBindingDescription)
        {
            .binding = 0,
            .stride = sizeof(struct vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .vertex_attr_desc =
        {
            // #1: vertex position
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(struct vertex, pos),
            },
            // #2: texcoord
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(struct vertex, texcoord),
            },
        },
        .vertex_attr_count = 2,
    },
    // Vertex-lit shader, currently used for rendering the '(fade)' texture
    [SHADER_VERTEXLIT] =
    {
        .name = "vertexlit",
        .pconst_size = sizeof(struct push_constants_vl),
        .use_descriptor_sets = false,
        .depth_test = true,

        .vertex_binding_desc = (VkVertexInputBindingDescription)
        {
            .binding = 0,
            .stride = sizeof(struct vertex_vl),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .vertex_attr_desc =
        {
            // #1: vertex position
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(struct vertex_vl, pos),
            },
            // #2: vertex colour
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .offset = offsetof(struct vertex_vl, colour),
            },
        },
        .vertex_attr_count = 2,
    },
    // Particle rendering shader
    [SHADER_PARTICLE] =
    {
        .name = "particle",
        .pconst_size = sizeof(struct push_constants_ptl),
        .use_descriptor_sets = true,
        .depth_test = false,

        .vertex_binding_desc = (VkVertexInputBindingDescription)
        {
            .binding = 0,
            .stride = sizeof(struct vertex_ptl),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .vertex_attr_desc =
        {
            // #1: vertex position
            {
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(struct vertex_ptl, pos),
            },
            // #2: opacity
            {
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32_SFLOAT,
                .offset = offsetof(struct vertex_ptl, opacity),
            },
            // #3: texture index
            {
                .binding = 0,
                .location = 2,
                .format = VK_FORMAT_R32_UINT,
                .offset = offsetof(struct vertex_ptl, tex_index),
            },
        },
        .vertex_attr_count = 3,
    },
};

struct shader_module_set
{
    char name[SHADER_NAME_MAX];
    VkShaderModule vert;
    VkShaderModule frag;
};

static i32 shader_init(struct shader *, struct shader_module_set *);
static VkShaderModule create_shader_module(const char *, bool *);

i32
vulkan_shaders_init_all(void)
{
    i32 status = 0;
    u32 i;

    // Read shader modules first, so we can reuse them if needed
    struct shader_module_set *modules = 
        malloc(sizeof(struct shader_module_set) * SHADER_COUNT);
    u32 shader_module_count = 0;
    for (i = 0; i < SHADER_COUNT; ++i)
    {
        struct shader_module_set *cur_mod;

        for (u32 j = 0; j < shader_module_count; ++j)
        {
            // Check if shader has already been loaded
            if (strcmp(modules[j].name, g_shader_list[i].name) == 0)
            {
                cur_mod = &modules[j];
                goto exists;
            }
        }

        /*
         * Shader not found, let's load it and add it to the list
         */
        char vert_path[256], frag_path[256];
        sprintf(vert_path, "shader/%s.vert.spv", g_shader_list[i].name);
        sprintf(frag_path, "shader/%s.frag.spv", g_shader_list[i].name);

        u32 index = shader_module_count;
        strcpy(modules[index].name, g_shader_list[i].name);

        // Load vertex shader
        bool success;
        modules[index].vert = create_shader_module(vert_path, &success);
        if (!success) continue;

        // Load fragment shader
        modules[index].frag = create_shader_module(frag_path, &success);
        if (!success) continue;

        cur_mod = &modules[index];

        ++shader_module_count;

        // Skip here if already found
    exists:

        // Actually load the shader
        if (shader_init(&g_shader_list[i], cur_mod) < 0)
            status = -1;
    }

    // Free the shader modules
    for (i = 0; i < shader_module_count; ++i)
    {
        vkDestroyShaderModule(g_vulkan->d, modules[i].frag, NULL);
        vkDestroyShaderModule(g_vulkan->d, modules[i].vert, NULL);
    }

    return status;
}

i32
vulkan_shaders_free_all(void)
{
    // Destroy pipelines
    for (u32 i = 0; i < SHADER_COUNT; ++i)
    {
        vkDestroyPipeline(g_vulkan->d, 
            g_shader_list[i].pipeline, 
            NULL);
        vkDestroyPipelineLayout(g_vulkan->d, 
            g_shader_list[i].pipeline_layout, 
            NULL);
    }
    return 0;
}

static i32
shader_init(
    struct shader *s, 
    struct shader_module_set *modules) 
{
    /*
     * Create shader stages
     */
    VkPipelineShaderStageCreateInfo vert_stage_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = modules->vert,
        .pName = "main",
    },
    frag_stage_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = modules->frag,
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
        .pVertexBindingDescriptions = &s->vertex_binding_desc,
        .vertexAttributeDescriptionCount = s->vertex_attr_count,
        .pVertexAttributeDescriptions = s->vertex_attr_desc,
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
        .width = (f32)g_vulkan->swapchain->extent.width,
        .height = (f32)g_vulkan->swapchain->extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    VkRect2D scissor =
    {
        .offset = { 0, 0 },
        .extent = g_vulkan->swapchain->extent,
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
        //.polygonMode = VK_POLYGON_MODE_LINE,
        .lineWidth = 1.0f,
        // We can't enable baceface culling right now because the polygon
        // index calculation means we could get points that are both clockwise
        // and counter clockwise
        //.cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,

        // Flip faces because we render with flipped Y
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        //.frontFace = VK_FRONT_FACE_CLOCKWISE,
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
     * Depth testing
     */
    VkPipelineDepthStencilStateCreateInfo depth_info =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = s->depth_test ? VK_TRUE : VK_FALSE,
        .depthWriteEnable = s->depth_test ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
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
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
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
        .size = s->pconst_size,
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
        .setLayoutCount = !!s->use_descriptor_sets,
        .pSetLayouts = s->use_descriptor_sets 
            ? &g_vulkan->desc_set_layout 
            : NULL,
    };
    if (vkCreatePipelineLayout(g_vulkan->d,
        &pipeline_layout_info, NULL, &s->pipeline_layout) != VK_SUCCESS)
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
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_info,
        .pColorBlendState = &colour_blend_state_info,
        .pDynamicState = NULL,

        .layout = s->pipeline_layout,
        .renderPass = g_vulkan->render_pass,
        .subpass = 0,
    };
    if (vkCreateGraphicsPipelines(g_vulkan->d, VK_NULL_HANDLE, 1,
        &pipeline_info, NULL, &s->pipeline) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create graphics pipeline");
        return -1;
    }

    LOG_INFO("[vulkan] shader '%s' initialised and pipeline created!", s->name);

    return 0;
}

static VkShaderModule
create_shader_module(const char *path, bool *success)
{
    VkShaderModule module;
    *success = false;

    // Load shader from path
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        LOG_ERROR("[vulkan] failed to read shader file '%s'", path);
        return module;
    }

    // Read shader data size
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);
    u32 *buf = malloc(len * sizeof(u32));

    // Read shader data
    size_t n = fread(buf, len, 1, fp);
    (void)n;
    fclose(fp);

    // Create the shader module
    VkShaderModuleCreateInfo create_info =
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = len,
        .pCode = buf,
    };
    if (vkCreateShaderModule(g_vulkan->d,
        &create_info, NULL, &module) != VK_SUCCESS)
    {
        LOG_ERROR("[vulkan] failed to create shader module");
    }

    free(buf);
    *success = true;
    return module;
}
