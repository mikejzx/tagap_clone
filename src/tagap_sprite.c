#include "pch.h"
#include "tagap.h"
#include "tagap_sprite.h"

bool
tagap_sprite_load(struct tagap_sprite_info *spr)
{
    if (!spr)
    {
        LOG_INFO("[tagap_sprite] cannot load; invalid sprite");
        return false;
    }

    if (spr->is_loaded)
    {
        //LOG_INFO("[tagap_sprite] sprite already loaded");
        return true;
    }

    // Count sprite frame textures
    char sprpath[256];
    spr->frame_count = -1;
    do
    {
        ++spr->frame_count;
        sprintf(sprpath, "%s/%s_%02d.tga",
            TAGAP_SPRITES_DIR,
            spr->name,
            spr->frame_count);
    } while(access(sprpath, F_OK) == 0);

    if (!spr->frame_count)
    {
        LOG_WARN("[tagap_sprite] no frames in sprite '%s'", spr->name);
        return false;
    }

    // Allocate frames
    spr->frames = calloc(spr->frame_count,
        sizeof(struct tagap_sprite_frame));

    // Load each of the individual frames textures
    for (u32 f = 0; f < spr->frame_count; ++f)
    {
        sprintf(sprpath, "%s/%s_%02d.tga",
            TAGAP_SPRITES_DIR,
            spr->name,
            f);
        spr->frames[f].tex = vulkan_texture_load(sprpath);
        if (spr->frames[f].tex < 0)
        {
            LOG_WARN("[entity] sprite texture '%s' couldn't be loaded",
                sprpath);
            spr->frames[f].tex = 0;
        }
    }

    return true;
}

void
tagap_sprite_free(struct tagap_sprite_info *spr)
{
    if (!spr->is_loaded) return;
    spr->is_loaded = false;

    free(spr->frames);
    spr->frames = NULL;
    spr->frame_count = 0;
}
