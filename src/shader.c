#include "pch.h"
#include "shader.h"
#include "vulkan_renderer.h"
#include "vulkan_swapchain.h"

// List of shaders used in the game
struct shader 
g_shader_list[SHADER_COUNT] =
{
    // Default shader used for rendering the main level stuff
    [SHADER_DEFAULT] =
    {
        .name = "default",
        .pconst_size = sizeof(struct push_constants),
        .use_descriptor_sets = true,

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
                .format = VK_FORMAT_R32G32_SFLOAT,
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
                .format = VK_FORMAT_R32G32_SFLOAT,
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
};

static i32 shader_init(struct shader *);
static VkShaderModule create_shader_module(const char *);

i32
vulkan_shaders_init_all(void)
{
    // Setup shaders
    for (u32 i = 0; i < SHADER_COUNT; ++i)
    {
        if (shader_init(&g_shader_list[i]) < 0)
            return -1;
    }
    return 0;
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
shader_init(struct shader *s)
{
    /* Read shaders and create modules */
    char shader_vert_path[256], shader_frag_path[256];
    sprintf(shader_vert_path, "shader/%s.vert.spv", s->name);
    sprintf(shader_frag_path, "shader/%s.frag.spv", s->name);
    VkShaderModule vert = create_shader_module(shader_vert_path);
    VkShaderModule frag = create_shader_module(shader_frag_path);

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
        .pDepthStencilState = NULL,
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

    vkDestroyShaderModule(g_vulkan->d, frag, NULL);
    vkDestroyShaderModule(g_vulkan->d, vert, NULL);

    return 0;
}

static VkShaderModule
create_shader_module(const char *path)
{
    VkShaderModule module;

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
    return module;
}
