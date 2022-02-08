#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include "types.h"
#include "renderer.h"

/*
 * vulkan_renderer.h
 *
 * Backend rendering code.  The aim is to keep this renderer relatively
 * straightforward to use and work with.  This is my first time even looking at
 * the Vulkan API so things may not be done in the best way possible.
 *
 * For now a few things are not supported but may be added in future:
 * + Swapchain rebuilding (e.g. on window resizes, etc.)
 * + Selection of best GPU, at the moment we just select the first GPU that
 *   meets all requirements.
 * + Proper shader management; at the moment the file paths are hard-coded in
 *   the renderer
 */

#define WIDTH 1280 //800
#define HEIGHT 720 //600

#define MAX_TEXTURES 256

enum vulkan_queue_id
{
    VKQ_GRAPHICS,
    VKQ_PRESENT,
    VKQ_COUNT,
};

struct queue_family
{
    VkQueue queue;
    bool has_index;
    u32 index;
};

struct vulkan_renderer
{
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice video_card;
    VkDevice d;
    VmaAllocator vma;

    struct queue_family qfams[VKQ_COUNT];

    VkRenderPass render_pass;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet *desc_sets;
    VkDescriptorPool desc_pool;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline; // Graphics pipeline
    VkCommandPool cmd_pool;
    VkCommandBuffer *cmd_buffers;
    u32 cmd_buffer_count;

    // Textures
    VkSampler sampler;
    struct vulkan_texture
    {
        VkImage image;
        VmaAllocation alloc;
        VkImageView view;
        char name[256];
        u32 w, h;
    } textures[MAX_TEXTURES];
    i32 tex_used;
};

extern struct vulkan_renderer *g_vulkan;

void vulkan_renderer_init_state(void);
i32 vulkan_renderer_init(SDL_Window *);
void vulkan_renderer_deinit(void);
void vulkan_renderer_wait_for_idle(void);
i32 vulkan_create_buffer(VkDeviceSize,
    VkBufferUsageFlags,
    VmaMemoryUsage,
    VkMemoryPropertyFlags,
    VkBuffer *,
    VmaAllocation *);
i32 vulkan_copy_buffer(VkBuffer, VkBuffer, size_t);

i32 vulkan_render_frame_pre(void);
i32 vulkan_record_command_buffers(struct renderable *, size_t, vec3s);
i32 vulkan_render_frame(void);

i32 vulkan_texture_load(const char *);

i32 vulkan_level_begin(void);
i32 vulkan_level_end(void);

#endif
