#ifndef VULKAN_RENDERER_H
#define VULKAN_RENDERER_H

#include "types.h"

struct renderer_obj_group;
struct shader;

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
 */

//#define WIDESCREEN
#ifndef WIDESCREEN
#  define WIDTH 800 //1440
#  define HEIGHT 600 //1080
#  define WIDTH_INTERNAL 800
#  define HEIGHT_INTERNAL 600
#else
#  define WIDTH 1920
#  define HEIGHT 1080
#  define WIDTH_INTERNAL 1067
#  define HEIGHT_INTERNAL 600
#endif

#define MAX_TEXTURES 128
// We reserve two textures at the moment
//  0: default 1x1 white texture
#define RESERVED_TEXTURE_COUNT 1
#define TEXINDEX_DEFAULT 0

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
    VkCommandPool cmd_pool;
    VkCommandBuffer *cmd_buffers;
    u32 cmd_buffer_count;

    // Subpass 2 descriptors
    VkDescriptorSetLayout desc_set_layout_sp2;
    VkDescriptorSet *desc_sets_sp2;

    // Textures
    VkSampler sampler;
    struct vulkan_texture
    {
        VkImage image;
        VmaAllocation alloc;
        VkImageView view;
        char name[256];
        u32 w, h;
        VkFormat format;
    } textures[MAX_TEXTURES];
    i32 tex_used;
    VkDescriptorImageInfo *image_desc_infos;
    VkDescriptorImageInfo sampler_desc_info;
    bool in_level;

    // Z-buffer
    VkImage zbuf_image;
    VkImageView zbuf_view;
    VmaAllocation zbuf_alloc;

    struct vulkan_swapchain *swapchain;

    // Lighting
    struct vulkan_texture *light_tex;
    VkFramebuffer *light_framebufs;
    VkRenderPass light_render_pass;
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
i32 vulkan_record_command_buffers(struct renderer_obj_group *, size_t, vec3s *);
i32 vulkan_render_frame(void);

i32 vulkan_texture_load(const char *);

i32 vulkan_level_begin(void);
i32 vulkan_level_end(void);

#endif
