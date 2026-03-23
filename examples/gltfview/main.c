/*
 * glTF Model Viewer - BlazingRenderer / SDL3 + GL
 *
 * Loads a .glb/.gltf file and renders it with an orbit camera.
 * Ported from the Win32/DDraw gltfview to SDL3 + BRender's GL renderer.
 *
 * Usage: gltfview model.glb
 *        Arrow keys: orbit elevation/angle
 *        Space: pause orbit
 *        1/0: zoom in/out
 *        ESC/Q: quit
 *        ALT+ENTER: fullscreen toggle
 */
#include <SDL3/SDL.h>
#include <brender.h>
#include <brsdl3dev.h>
#include <brv1db.h>
#include <fmt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gltf_to_br.h"
#include "stb_image_write.h"

#define WIN_W 1024
#define WIN_H 768
#define SPF   (1.0f / 60.0f)

#define LOG(msg)                  \
    do {                          \
        printf("LOG: %s\n", msg); \
        fflush(stdout);           \
    } while(0)
#define LOGF(fmt, ...)                         \
    do {                                       \
        printf("LOG: " fmt "\n", __VA_ARGS__); \
        fflush(stdout);                        \
    } while(0)

/* Primitive heap for z-buffered renderer */
static uint8_t primitive_heap[1500 * 1024];

/* Scene state */
static br_pixelmap *g_screen;
static br_pixelmap *g_colour_buf;
static br_pixelmap *g_depth_buf;
static br_actor    *g_world;
static br_actor    *g_camera;
static br_actor    *g_scene_root;
static gltf_scene   g_gltf_scene;

/* Orbit camera state */
static float g_orbit_angle     = 0.0f;
static float g_orbit_elevation = 0.3f;
static float g_orbit_dist_mul  = 0.25f;
static float g_orbit_dist_abs = 0.0f;  /* if >0, override orbit distance (absolute units) */
static int   g_center_override = 0;
static float g_center_x = 0, g_center_y = 0, g_center_z = 0;
static int   g_orbit_paused    = 0;
static float g_accum           = 0.0f;

static br_vector3 g_scene_center;
static br_scalar  g_scene_radius;

/* Animation */
static float g_anim_time  = -1.0f;
static float g_anim_speed = 0.17f;

/* VSync (default: on) */
static int g_vsync = 1;

/* Brightness (material ambient ka) */
static float g_brightness = 0.30f;

/* Timeout (seconds, 0 = disabled) */
static float g_timeout = 0.0f;

/* FPS tracking */
static float    g_fps        = 0.0f;
static int      g_fps_frames = 0;
static uint64_t g_fps_start  = 0;

void _BrBeginHook(void)
{
    struct br_device *BR_EXPORT BrDrv1SoftPrimBegin(const char *arguments);
    struct br_device *BR_EXPORT BrDrv1SoftRendBegin(const char *arguments);

    BrDevAddStatic(NULL, BrDrv1SoftPrimBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1SoftRendBegin, NULL);
    BrDevAddStatic(NULL, BrDrv1SDL3Begin, NULL);
}

void _BrEndHook(void)
{
}

/* ------------------------------------------------------------------ */
/* Orbit camera                                                        */
/* ------------------------------------------------------------------ */

static void update_orbit(float dt)
{
    br_scalar cam_x, cam_y, cam_z, dist;

    g_accum += dt;
    while(g_accum >= SPF) {
        if(!g_orbit_paused)
            g_orbit_angle += 1.0f;
        if(g_orbit_angle >= 360.0f)
            g_orbit_angle -= 360.0f;
        g_accum -= SPF;
    }

    dist = (g_orbit_dist_abs > 0.0f) ? g_orbit_dist_abs : g_scene_radius * g_orbit_dist_mul;

    cam_x = g_scene_center.v[0] + dist * (float)cos(g_orbit_elevation) * (float)sin(g_orbit_angle * 3.14159f / 180.0f);
    cam_z = g_scene_center.v[2] + dist * (float)cos(g_orbit_elevation) * (float)cos(g_orbit_angle * 3.14159f / 180.0f);
    cam_y = g_scene_center.v[1] + dist * (float)sin(g_orbit_elevation);

    g_camera->t.type                = BR_TRANSFORM_LOOK_UP;
    g_camera->t.t.look_up.look.v[0] = g_scene_center.v[0] - cam_x;
    g_camera->t.t.look_up.look.v[1] = g_scene_center.v[1] - cam_y;
    g_camera->t.t.look_up.look.v[2] = g_scene_center.v[2] - cam_z;
    g_camera->t.t.look_up.up.v[0]   = BR_SCALAR(0);
    g_camera->t.t.look_up.up.v[1]   = BR_SCALAR(1);
    g_camera->t.t.look_up.up.v[2]   = BR_SCALAR(0);
    g_camera->t.t.look_up.t.v[0]    = cam_x;
    g_camera->t.t.look_up.t.v[1]    = cam_y;
    g_camera->t.t.look_up.t.v[2]    = cam_z;
}

/* ------------------------------------------------------------------ */
/* Scene setup                                                         */
/* ------------------------------------------------------------------ */

static int setup_scene(const char *glb_path)
{
    br_camera *cam_data;
    br_actor  *light;
    br_scalar  dx, dy, dz;

    if(!gltf_load_scene(glb_path, &g_gltf_scene)) {
        LOG("ERROR: failed to load glTF file");
        return 0;
    }

    /* Compute scene center and radius from AABB */
    g_scene_center.v[0] = (g_gltf_scene.bbox_min.v[0] + g_gltf_scene.bbox_max.v[0]) * 0.5f;
    g_scene_center.v[1] = (g_gltf_scene.bbox_min.v[1] + g_gltf_scene.bbox_max.v[1]) * 0.5f;
    g_scene_center.v[2] = (g_gltf_scene.bbox_min.v[2] + g_gltf_scene.bbox_max.v[2]) * 0.5f;

    dx             = g_gltf_scene.bbox_max.v[0] - g_gltf_scene.bbox_min.v[0];
    dy             = g_gltf_scene.bbox_max.v[1] - g_gltf_scene.bbox_min.v[1];
    dz             = g_gltf_scene.bbox_max.v[2] - g_gltf_scene.bbox_min.v[2];
    g_scene_radius = 0.5f * (float)sqrt((double)(dx * dx + dy * dy + dz * dz));

    if(g_scene_radius < 0.01f)
        g_scene_radius = 1.0f;

    if(g_center_override) {
        g_scene_center.v[0] = g_center_x;
        g_scene_center.v[1] = g_center_y;
        g_scene_center.v[2] = g_center_z;
    }

    LOGF("scene center=(%.2f,%.2f,%.2f) radius=%.2f", (double)g_scene_center.v[0], (double)g_scene_center.v[1], (double)g_scene_center.v[2],
         (double)g_scene_radius);

    /* World */
    g_world         = BrActorAllocate(BR_ACTOR_NONE, NULL);
    g_world->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Identity(&g_world->t.t.mat);

    /* Camera */
    g_camera                = BrActorAdd(g_world, BrActorAllocate(BR_ACTOR_CAMERA, NULL));
    cam_data                = (br_camera *)g_camera->type_data;
    cam_data->type          = BR_CAMERA_PERSPECTIVE;
    cam_data->aspect        = BR_DIV(BR_SCALAR(g_colour_buf->width), BR_SCALAR(g_colour_buf->height));
    cam_data->field_of_view = BR_ANGLE_DEG(45.0);

    if(g_orbit_dist_abs > 0.0f) {
        cam_data->hither_z = g_orbit_dist_abs * 0.01f;
        cam_data->yon_z    = g_orbit_dist_abs * 100.0f;
    } else {
        cam_data->hither_z = g_scene_radius * 0.01f;
        cam_data->yon_z    = g_scene_radius * 100.0f;
    }
    if(cam_data->hither_z < 0.01f)
        cam_data->hither_z = 0.01f;

    /* Directional light */
    light         = BrActorAdd(g_world, BrActorAllocate(BR_ACTOR_LIGHT, NULL));
    {
        br_light *ld  = (br_light *)light->type_data;
        ld->type      = BR_LIGHT_DIRECT;
        ld->colour    = BR_COLOUR_RGB(255, 255, 255);
        ld->attenuation_c = BR_SCALAR(1.0);
    }
    light->t.type = BR_TRANSFORM_MATRIX34;
    BrMatrix34Identity(&light->t.t.mat);
    BrLightEnable(light);

    /* Ambient light (fills in faces not reached by the directional light) */
    {
        br_actor *ambient = BrActorAdd(g_world, BrActorAllocate(BR_ACTOR_LIGHT, NULL));
        br_light *la      = (br_light *)ambient->type_data;
        la->type          = BR_LIGHT_AMBIENT;
        la->colour        = BR_COLOUR_RGB(255, 255, 255);
        BrLightEnable(ambient);
    }

    /* Attach the glTF actor tree */
    g_scene_root = BrActorAdd(g_world, g_gltf_scene.root_actor);

    return 1;
}

/* ------------------------------------------------------------------ */
/* Brightness                                                          */
/* ------------------------------------------------------------------ */

static void apply_brightness(void)
{
    for(int i = 0; i < g_gltf_scene.nmaterials; i++) {
        br_material *mat = g_gltf_scene.materials[i];
        mat->ka = BR_UFRACTION(g_brightness);
        BrMaterialUpdate(mat, BR_MATU_ALL);
    }
    LOGF("brightness: ka=%.2f", (double)g_brightness);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    br_error    r;
    br_boolean  is_fullscreen = BR_FALSE;
    SDL_Window *window;
    uint64_t    ticks_last, ticks_now;
    const char *screenshot_path = NULL;
    int         screenshot_frame = 10;
    int         max_frames      = 0;
    int         frame_count     = 0;
    const char *model_path      = NULL;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc)
            screenshot_path = argv[++i];
        else if(strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            max_frames = atoi(argv[++i]);
        else if(strcmp(argv[i], "--animspeed") == 0 && i + 1 < argc)
            g_anim_speed = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--no-vsync") == 0)
            g_vsync = 0;
        else if(strcmp(argv[i], "--brightness") == 0 && i + 1 < argc)
            g_brightness = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--frame") == 0 && i + 1 < argc)
            screenshot_frame = atoi(argv[++i]);
        else if(strcmp(argv[i], "--timeout") == 0 && i + 1 < argc)
            g_timeout = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--dist") == 0 && i + 1 < argc)
            g_orbit_dist_abs = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--dist-mul") == 0 && i + 1 < argc)
            g_orbit_dist_mul = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--elevation") == 0 && i + 1 < argc)
            g_orbit_elevation = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--angle") == 0 && i + 1 < argc)
            g_orbit_angle = (float)atof(argv[++i]);
        else if(strcmp(argv[i], "--center") == 0 && i + 3 < argc) {
            g_center_override = 1;
            g_center_x = (float)atof(argv[++i]);
            g_center_y = (float)atof(argv[++i]);
            g_center_z = (float)atof(argv[++i]);
        }
        else if(argv[i][0] != '-')
            model_path = argv[i];
    }

    if(!model_path) {
        printf("Usage: gltfview [--no-vsync] [--brightness F] [--timeout S]\n"
               "                [--animspeed F] [--screenshot path] [--frames N]\n"
               "                [--dist F] [--dist-mul F] [--center X Y Z]\n"
               "                [--elevation F] [--angle F]\n"
               "                <model.glb|model.gltf>\n");
        return 1;
    }

    LOGF("glTF file: %s", model_path);

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    BrBegin();
    BrLogSetLevel(BR_LOG_DEBUG);

    r = BrDevBeginVar(&g_screen, "SDL3", BRT_WIDTH_I32, WIN_W, BRT_HEIGHT_I32, WIN_H, BR_NULL_TOKEN);
    if(r != BRE_OK) {
        LOG("BrDevBeginVar failed");
        BrEnd();
        return 1;
    }

    /* Create offscreen colour and depth buffers (software path) */
    g_colour_buf = BrPixelmapMatchTyped(g_screen, BR_PMMATCH_OFFSCREEN, BR_PMT_RGB_888);
    if(!g_colour_buf) {
        LOG("BrPixelmapMatchTyped(RGB_888) failed");
        BrPixelmapFree(g_screen);
        BrEnd();
        return 1;
    }

    g_depth_buf = BrPixelmapMatch(g_colour_buf, BR_PMMATCH_DEPTH_16);
    if(!g_depth_buf) {
        LOG("BrPixelmapMatch(DEPTH_16) failed");
        BrPixelmapFree(g_colour_buf);
        BrPixelmapFree(g_screen);
        BrEnd();
        return 1;
    }

    g_colour_buf->origin_x = g_depth_buf->origin_x = g_colour_buf->width >> 1;
    g_colour_buf->origin_y = g_depth_buf->origin_y = g_colour_buf->height >> 1;

    LOGF("screen: %dx%d type=%d", g_screen->width, g_screen->height, g_screen->type);
    LOGF("colour: %dx%d type=%d", g_colour_buf->width, g_colour_buf->height, g_colour_buf->type);
    LOGF("depth:  %dx%d type=%d", g_depth_buf->width, g_depth_buf->height, g_depth_buf->type);

    window = BrSDL3UtilGetWindow(g_screen);
    SDL_SetWindowSurfaceVSync(window, g_vsync);
    LOGF("vsync: %s", g_vsync ? "on" : "off");

    BrRendererBegin(g_colour_buf, NULL, NULL, primitive_heap, sizeof(primitive_heap));

    if(!setup_scene(model_path)) {
        LOG("scene setup failed");
        BrRendererEnd();
        BrPixelmapFree(g_depth_buf);
        BrPixelmapFree(g_colour_buf);
        BrPixelmapFree(g_screen);
        BrEnd();
        return 1;
    }

    apply_brightness();

    LOGF("anim_speed=%.4f, anim_duration=%.3fs", (double)g_anim_speed, (double)g_gltf_scene.anim.duration);
    LOG("scene ready, entering render loop");

    ticks_last = SDL_GetTicksNS();

    for(SDL_Event evt;;) {
        float dt;

        ticks_now  = SDL_GetTicksNS();
        dt         = (float)(ticks_now - ticks_last) / 1e9f;
        ticks_last = ticks_now;
        if(dt > 0.1f)
            dt = 0.1f;

        /* FPS tracking */
        g_fps_frames++;
        if(g_fps_start == 0)
            g_fps_start = ticks_now;
        {
            uint64_t elapsed_ns = ticks_now - g_fps_start;
            float    elapsed_s  = (float)elapsed_ns / 1e9f;
            if(elapsed_s > 0.0f)
                g_fps = (float)g_fps_frames / elapsed_s;
            if(elapsed_s >= 2.0f) {
                LOGF("fps=%.1f  anim_time=%.3f  speed=%.2fx", (double)g_fps, (double)g_anim_time, (double)g_anim_speed);
                g_fps_frames = 0;
                g_fps_start  = ticks_now;
            }
        }

        while(SDL_PollEvent(&evt) > 0) {
            switch(evt.type) {
                case SDL_EVENT_QUIT:
                    goto done;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    /* TODO: resize not supported in software mode yet */
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if(BrSDL3UtilIsAltEnter(&evt.key)) {
                        if(is_fullscreen)
                            SDL_SetWindowFullscreen(window, 0);
                        else
                            SDL_SetWindowFullscreen(window, 1);
                        is_fullscreen = !is_fullscreen;
                        break;
                    }

                    switch(evt.key.scancode) {
                        case SDL_SCANCODE_ESCAPE:
                        case SDL_SCANCODE_Q:
                            goto done;
                        case SDL_SCANCODE_SPACE:
                            g_orbit_paused ^= 1;
                            break;
                        case SDL_SCANCODE_UP:
                            g_orbit_elevation += 0.1f;
                            if(g_orbit_elevation > 1.4f)
                                g_orbit_elevation = 1.4f;
                            break;
                        case SDL_SCANCODE_DOWN:
                            g_orbit_elevation -= 0.1f;
                            if(g_orbit_elevation < -0.5f)
                                g_orbit_elevation = -0.5f;
                            break;
                        case SDL_SCANCODE_LEFT:
                            g_orbit_angle -= 5.0f;
                            if(g_orbit_angle < 0.0f)
                                g_orbit_angle += 360.0f;
                            break;
                        case SDL_SCANCODE_RIGHT:
                            g_orbit_angle += 5.0f;
                            if(g_orbit_angle >= 360.0f)
                                g_orbit_angle -= 360.0f;
                            break;
                        case SDL_SCANCODE_1:
                            g_orbit_dist_mul -= 0.25f;
                            if(g_orbit_dist_mul < 1.0f)
                                g_orbit_dist_mul = 1.0f;
                            break;
                        case SDL_SCANCODE_0:
                            g_orbit_dist_mul += 0.25f;
                            if(g_orbit_dist_mul > 10.0f)
                                g_orbit_dist_mul = 10.0f;
                            break;
                        case SDL_SCANCODE_EQUALS: /* +/= key */
                            g_anim_speed *= 2.0f;
                            if(g_anim_speed > 16.0f)
                                g_anim_speed = 16.0f;
                            LOGF("anim speed: %.2fx", (double)g_anim_speed);
                            break;
                        case SDL_SCANCODE_MINUS:
                            g_anim_speed *= 0.5f;
                            if(g_anim_speed < 0.0625f)
                                g_anim_speed = 0.0625f;
                            LOGF("anim speed: %.2fx", (double)g_anim_speed);
                            break;
                        case SDL_SCANCODE_BACKSPACE:
                            g_anim_speed = 1.0f;
                            LOGF("anim speed: %.2fx (reset)", (double)g_anim_speed);
                            break;
                        case SDL_SCANCODE_V:
                            g_vsync ^= 1;
                            SDL_SetWindowSurfaceVSync(window, g_vsync);
                            LOGF("vsync: %s", g_vsync ? "on" : "off");
                            break;
                        case SDL_SCANCODE_B:
                            g_brightness += 0.05f;
                            if(g_brightness > 1.0f)
                                g_brightness = 1.0f;
                            apply_brightness();
                            break;
                        case SDL_SCANCODE_N:
                            g_brightness -= 0.05f;
                            if(g_brightness < 0.0f)
                                g_brightness = 0.0f;
                            apply_brightness();
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }

        update_orbit(dt);

        if(g_anim_time < 0.0f)
            g_anim_time += dt;
        else
            g_anim_time += dt * g_anim_speed;
        if(g_anim_time >= 0.0f)
            gltf_update_animation(&g_gltf_scene, g_anim_time);

        /* Render */
        BrRendererFrameBegin();
        BrPixelmapFill(g_colour_buf, BR_COLOUR_RGB(40, 40, 50));
        BrPixelmapFill(g_depth_buf, 0xFFFFFFFF);
        BrZbSceneRender(g_world, g_camera, g_colour_buf, g_depth_buf);
        BrRendererFrameEnd();

        /* HUD */
        {
            char       hud_buf[128];
            br_int_16  x0 = -(g_colour_buf->width / 2) + 5;
            br_int_16  y0 = -(g_colour_buf->height / 2) + 5;
            br_uint_16 lh = BrPixelmapTextHeight(g_colour_buf, BrFontProp7x9);

            snprintf(hud_buf, sizeof(hud_buf), "FPS: %.1f  Anim: %.2fx", (double)g_fps, (double)g_anim_speed);
            BrPixelmapText(g_colour_buf, x0, y0, BR_COLOUR_RGB(255, 255, 0), BrFontProp7x9, hud_buf);
            y0 += lh + 2;
            snprintf(hud_buf, sizeof(hud_buf), "ka=%.2f  VSync:%s", (double)g_brightness, g_vsync ? "on" : "off");
            BrPixelmapText(g_colour_buf, x0, y0, BR_COLOUR_RGB(200, 200, 200), BrFontProp7x9, hud_buf);
            y0 += lh + 2;
            BrPixelmapText(g_colour_buf, x0, y0, BR_COLOUR_RGB(255, 255, 255), BrFontProp7x9,
                           "SPACE:Pause  Arrows:Orbit  1/0:Zoom  +/-:Speed  V:VSync  B/N:Bright  Q:Quit");
        }

        BrPixelmapDoubleBuffer(g_screen, g_colour_buf);

        frame_count++;

        /* Pixel probe on first frame */
        if(frame_count == 1) {
            unsigned char *pixels = (unsigned char *)g_colour_buf->pixels;
            LOGF("colour buf: pixels=%p row_bytes=%d", (void *)pixels, g_colour_buf->row_bytes);
            if(pixels) {
                int            cx     = g_colour_buf->width / 2;
                int            cy     = g_colour_buf->height / 2;
                int            stride = g_colour_buf->row_bytes;
                unsigned char *center = pixels + cy * stride + cx * 3;
                unsigned char *corner = pixels;
                LOGF("pixel[0,0]    = (%d,%d,%d)", corner[0], corner[1], corner[2]);
                LOGF("pixel[cx,cy]  = (%d,%d,%d)", center[0], center[1], center[2]);

                /* Count non-background pixels */
                int non_bg = 0;
                for(int y = 0; y < g_colour_buf->height; y++) {
                    unsigned char *row = pixels + y * stride;
                    for(int x = 0; x < g_colour_buf->width; x++) {
                        int off = x * 3;
                        if(row[off] != 50 || row[off + 1] != 40 || row[off + 2] != 40)
                            non_bg++;
                    }
                }
                LOGF("non-background pixels: %d / %d", non_bg, g_colour_buf->width * g_colour_buf->height);
            }
        }

        /* Screenshot: write PNG via stb_image_write */
        if(screenshot_path && frame_count == screenshot_frame) {
            unsigned char *pixels = (unsigned char *)g_colour_buf->pixels;
            if(pixels) {
                int w = g_colour_buf->width, h = g_colour_buf->height;
                int stride = g_colour_buf->row_bytes;
                /* BRender RGB_888 is stored as BGR; swap to RGB for PNG */
                unsigned char *rgb = malloc(w * h * 3);
                if(rgb) {
                    for(int y = 0; y < h; y++) {
                        unsigned char *src = pixels + y * stride;
                        unsigned char *dst = rgb + y * w * 3;
                        for(int x = 0; x < w; x++) {
                            dst[x * 3 + 0] = src[x * 3 + 2];
                            dst[x * 3 + 1] = src[x * 3 + 1];
                            dst[x * 3 + 2] = src[x * 3 + 0];
                        }
                    }
                    if(stbi_write_png(screenshot_path, w, h, 3, rgb, w * 3))
                        LOGF("screenshot saved: %s (PNG)", screenshot_path);
                    else
                        LOGF("screenshot FAILED: %s", screenshot_path);
                    free(rgb);
                }
            }
        }

        if(max_frames > 0 && frame_count >= max_frames)
            goto done;

        if(g_timeout > 0.0f) {
            static float elapsed_total = 0.0f;
            elapsed_total += dt;
            if(elapsed_total >= g_timeout) {
                LOGF("timeout reached (%.1fs)", (double)g_timeout);
                goto done;
            }
        }
    }

done:
    BrRendererEnd();
    BrPixelmapFree(g_depth_buf);
    BrPixelmapFree(g_colour_buf);
    BrPixelmapFree(g_screen);
    BrEnd();

    gltf_free_scene(&g_gltf_scene);
    LOG("done");
    return 0;
}
