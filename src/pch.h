#ifndef TAGAP_PCH_H
#define TAGAP_PCH_H

// Standard libs
#include <assert.h>
#include <locale.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// *nix
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

// Libraries
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#define CGLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cglm/struct.h>
#include <vk_mem_alloc.h>
#include <stb_image.h>
#define AL_API extern
#define ALC_API extern
#include <AL/al.h>
#include <AL/alc.h>

// Local includes
#include "types.h"
#include "log.h"
#include "util.h"

#endif
