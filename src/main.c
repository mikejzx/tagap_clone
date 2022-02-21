#include "pch.h"
#include "renderer.h"
#include "tagap.h"

struct tagap g_state;

static i32 foreach_in_dir(const char *, i32(*)(const char *));

i32
main (i32 argc, char **argv)
{
    (void)argc;
    (void)argv;

    setlocale(LC_NUMERIC, "");

    LOG_INFO("Starting TAGAP ...");

    LOG_INFO("TAGAP data directory: '%s'", TAGAP_DATA_DIR);
    LOG_INFO("Additional data directory: '%s'", TAGAP_DATA_MOD_DIR);

    // Set initial state
    memset(&g_state, 0, sizeof(struct tagap));
    g_state.type = GAME_STATE_BOOT;
    vulkan_renderer_init_state();

    level_init();

    //strcpy(g_state.l.map_path, TAGAP_SCRIPT_DIR "/maps/Level_1-Bb.map");
    strcpy(g_state.l.map_path, TAGAP_SCRIPT_DIR "/maps/Level_1-A.map");

    // Set up SDL window
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        LOG_ERROR("failed to initialise SDL");
        return -1;
    }
    SDL_Window *win_handle = SDL_CreateWindow(
        "TAGAP Clone",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        WIDTH, HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
        | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win_handle)
    {
        LOG_ERROR("failed to create window handle.  Perhaps libsdl2 was "
            "not built with required features?");
        goto game_quit;
    }

    // Initialise renderer
    if (renderer_init(win_handle) < 0) goto game_quit;

    SDL_Event event;
    for (;;)
    {
        g_state.now = NOW_NS();
        u64 frame_delta = g_state.now - g_state.last_frame;
        g_state.last_frame = g_state.now;
        g_state.dt = (f64)frame_delta / NS_PER_SECOND;

        g_state.mouse_scroll = 0;

        // Poll inputs
        static bool buttons[4] = { 0, 0, 0, 0 };
        while (SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            // Temporary camera scrolling controls
            case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym)
                {
                case SDLK_h: buttons[0] = 1; break;
                case SDLK_j: buttons[1] = 1; break;
                case SDLK_k: buttons[2] = 1; break;
                case SDLK_l: buttons[3] = 1; break;
                default: break;
                }
            } break;
            case SDL_KEYUP:
            {
                switch(event.key.keysym.sym)
                {
                case SDLK_h: buttons[0] = 0; break;
                case SDLK_j: buttons[1] = 0; break;
                case SDLK_k: buttons[2] = 0; break;
                case SDLK_l: buttons[3] = 0; break;
                default: break;
                }
            } break;

            // Mouse scrollwheel
            case SDL_MOUSEWHEEL:
            {
                g_state.mouse_scroll = event.wheel.y;
            } break;

            case SDL_QUIT:
                goto game_quit;
            default:
                break;
            }
        }
        static const f32 CAM_MOVE_SPEED = 10.0f;
        vec3s cam_add = GLMS_VEC3_ZERO_INIT;
        for (u32 i = 0; i < 4; ++i)
        {
            f32 v = (f32)buttons[i];
            switch (i)
            {
            case 0: cam_add.x -= v * CAM_MOVE_SPEED; break;
            case 1: cam_add.y += v * CAM_MOVE_SPEED; break;
            case 2: cam_add.y -= v * CAM_MOVE_SPEED; break;
            case 3: cam_add.x += v * CAM_MOVE_SPEED; break;
            }
        }
        g_state.cam_pos = glms_vec3_add(g_state.cam_pos, cam_add);

        // Get inputs
        g_state.kb_state = SDL_GetKeyboardState(NULL);
        g_state.m_state = SDL_GetMouseState(&g_state.mouse_x, &g_state.mouse_y);

        // Main state machine loop
        switch (g_state.type)
        {
        case GAME_STATE_BOOT:
            // Do any pre-game initialisations
            LOG_INFO("Running boot state ...");

            // Load the main game scripts
            foreach_in_dir(TAGAP_SCRIPT_DIR "/game", tagap_script_run);

            // Boot to menu state
            // TODO...

            // For now we just immediately load the first level of the game
            tagap_set_state(GAME_STATE_LEVEL_LOAD);
            break;
        case GAME_STATE_MENU:
            // TODO: menu loop
            break;
        case GAME_STATE_LEVEL_LOAD:
            // Reset current level state
            level_reset();
            vulkan_level_begin();

            // Load the level that is in the current level state
            if (level_load(g_state.l.map_path) < 0)
            {
                // Failed to load, goto menu
                tagap_set_state(GAME_STATE_MENU);
                break;
            }

            // Put something on the damn screen
            level_submit_to_renderer();

            level_spawn_entities();

            vulkan_level_end();
            tagap_set_state(GAME_STATE_LEVEL);
            break;
        case GAME_STATE_LEVEL:
            // Update the level
            level_update();

            // Update status line
            if (g_state.now - g_state.last_sec > NS_PER_SECOND)
            {
                g_state.last_sec = g_state.now;
                printf("status: %d fps, %.3f delta, %d draw cmds %d tex %d tmpe"
                    "    \r",
                    (i32)floor(1.0d / g_state.dt),
                    g_state.dt,
                    g_state.draw_calls,
                    g_vulkan->tex_used,
                    g_map->tmp_entity_count);
                fflush(stdout);
            }

            //SDL_Delay(100);
            break;
        }

        // Render this frame
        if (g_state.type == GAME_STATE_LEVEL)
        {
            renderer_render(&g_state.cam_pos);
        }
    }
game_quit:

    level_deinit();
    renderer_deinit();

    // Deinitialise SDL
    if (win_handle) SDL_DestroyWindow(win_handle);
    SDL_Quit();

    return 0;
}

static i32
foreach_in_dir(const char *path, i32(*func)(const char *))
{
    struct dirent *dp;
    DIR *dfd;

    if (!(dfd = opendir(path)))
    {
        LOG_ERROR("[foreach_in_dir] cannot open directory '%s'", path);
        return -1;
    }

    char filename[512];
    for (; (dp = readdir(dfd)) != NULL;)
    {
        struct stat stbuf;
        sprintf(filename, "%s/%s", path, dp->d_name);
        if (stat(filename, &stbuf) == -1)
        {
            LOG_WARN("[foreach_in_dir] cannot stat file '%s'", filename);
            continue;
        }

        if ((stbuf.st_mode & S_IFMT) == S_IFDIR)
        {
            // Skip directories
            continue;
        }

        // Run function on file
        func(filename);
    }
    return 0;
}
