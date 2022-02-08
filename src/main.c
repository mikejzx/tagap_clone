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

    state_level_init();

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
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
    if (!win_handle)
    {
        LOG_ERROR("failed to create window handle.  Perhaps libsdl2 was "
            "not built with required features?");
        goto game_quit;
    }

    // Initialise renderer
    if (renderer_init(win_handle) < 0) goto game_quit;

    static vec3s cam_pos = (vec3s)GLMS_VEC3_ZERO_INIT;

    SDL_Event event;
    for (;;)
    {
        // Poll inputs
        static const f32 CAM_MOVE_SPEED = 50.0f;
        vec3s cam_add = GLMS_VEC3_ZERO_INIT;
        while (SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            case SDL_KEYDOWN:
            {
                switch(event.key.keysym.sym)
                {
                case SDLK_h:
                    cam_add.x += CAM_MOVE_SPEED;
                    break;
                case SDLK_j:
                    cam_add.y -= CAM_MOVE_SPEED;
                    break;
                case SDLK_k:
                    cam_add.y += CAM_MOVE_SPEED;
                    break;
                case SDLK_l:
                    cam_add.x -= CAM_MOVE_SPEED;
                    break;
                default:
                    break;
                }
            } break;
            case SDL_QUIT:
                goto game_quit;
            default:
                break;
            }
        }
        cam_pos = glms_vec3_add(cam_pos, cam_add);

        // Main state machine loop
        switch (g_state.type)
        {
        case GAME_STATE_BOOT:
            // Do any pre-game initialisations
            LOG_INFO("Running boot state ...");

            // Load the main game scripts
            //foreach_in_dir(TAGAP_SCRIPT_DIR "/game", tagap_script_run);

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
            state_level_reset();
            vulkan_level_begin();

            // Load the level that is in the current level state
            if (level_load(g_state.l.map_path) < 0)
            {
                // Failed to load, goto menu
                tagap_set_state(GAME_STATE_MENU);
                break;
            }

            // Put something on the damn screen
            state_level_submit_to_renderer();

            // Generate line geometry in a single vertex buffer
            // TODO

            vulkan_level_end();
            tagap_set_state(GAME_STATE_LEVEL);
            break;
        case GAME_STATE_LEVEL:
            // TODO: actual game level loop

            // Update entities
            // TODO

            // Render level
            // TODO
            break;
        }

        // Render this frame
        renderer_render(cam_pos);
    }
game_quit:

    state_level_deinit();
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
