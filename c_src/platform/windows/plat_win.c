/* plat_win.c - Omega Race C port: the modern Windows backend.
 *
 * Win32 + OpenGL 3.3 core built on the framework files vendored into
 * this directory: raw input (key[] scancode buffer), XAudio2
 * mixer, the shader beam renderer, DirectInput joystick, DPI-aware window
 * with ALT+ENTER borderless fullscreen, ini config. Implements
 * platform/omega_platform.h; the core neither knows nor cares.
 *
 * Pacing (FRAME_PACING_NOTES.md):
 *  - the game paces itself at the hardware's floating rate through
 *    omega_app_step; vsync defaults OFF ([main] vsync in omega_win.ini:
 *    0 off / 1 on / 2 adaptive) because a vsynced flip quantizes 21 ms
 *    frames to 16.7/33.3 and reads as judder.
 *  - timeBeginPeriod(1) so the 1 ms frame wait is real.
 *  - a zero-length segment is a dot: the beam renderer turns the
 *    degenerate segment's lone endpoint into a round end-cap disc, so
 *    dots ride the same shader path as lines (no shot sprites - the
 *    machine draws only lines and dots).
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../omega_platform.h"
#include "framework.h"     /* Windows.h, glew, log */
#include "sys_gl.h"
#include "rawinput.h"
#include "joystick.h"
#include "mixer.h"
#include "vector_draw.h"
#include "mat4.h"
#include "ini.h"
#include "colordefs.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

extern void omega_hw_nvram_save(void);   /* app_loop.c: flush on exit */

/* ---- phosphor -------------------------------------------------------
 * A vector monitor's phosphor is still glowing from earlier frames when
 * the beam comes round again, so the screen shows a superposition of
 * the last few frames at falling brightness, not one crisp frame. The
 * DVG's frame rate floats with the display list (43-52 fps measured),
 * so decay MUST be a function of a frame's age in milliseconds - never
 * of how many frames back it is, which is what makes a fixed
 * "previous frame at 45%" look wrong as the rate moves.
 *
 * [vector] phosphor_ms in omega_win.ini is the decay time constant: a
 * frame is drawn at exp(-age_ms / phosphor_ms) of the intensity the DVG
 * gave it. 0 turns the path off entirely (one frame, full brightness -
 * exactly the behaviour before this existed).
 *
 * This is a TUNING KNOB, not a measured constant. The V2000's
 * persistence at normal brightness is short, so small values are the
 * honest starting point: at the ~21 ms frame period, 12 ms leaves the
 * previous frame at 17%, 8 ms at 7%, 20 ms at 35%.
 *
 * Capture happens in plat_video_line (the DVG walker's own sink) and
 * the whole history is replayed into the beam batch at present time,
 * which is what the renderer's retained-batch contract is for. */
#define PH_FRAMES  8            /* history depth                        */
#define PH_SEGS    2048         /* segments captured per frame          */
#define PH_FLOOR   0.02         /* drop a frame once it fades below this */

typedef struct { float x0, y0, x1, y1; int z; } ph_seg;
typedef struct { ph_seg seg[PH_SEGS]; int n; double t; int used; } ph_frame;

static ph_frame ph_buf[PH_FRAMES];
static int      ph_cur;
static double   ph_tau;         /* ms; 0 = phosphor off                 */
static int      ph_dropped;     /* segments lost to PH_SEGS, logged once */

/* ------------------------------------------------------------------ */
/* framework externs (framework.h declares these; the host defines)    */
/* ------------------------------------------------------------------ */

static HWND hWnd;
static int  g_quit;

int SCREEN_W = 1024;      /* window client area, physical pixels */
int SCREEN_H = 768;
int DESIGN_W = 1024;      /* the design rect ViewOrthoScaled letterboxes */
int DESIGN_H = 768;

HWND win_get_window(void) { return hWnd; }

static void msg_box(const char* title, const char* message)
{
    MessageBoxA(NULL, message, title, MB_ICONEXCLAMATION | MB_OK);
}

static void set_window_title(const char* title) { SetWindowTextA(hWnd, title); }

/* ------------------------------------------------------------------ */
/* DPI awareness                                                       */
/* ------------------------------------------------------------------ */

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif
#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

static void enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI* SetProcessDpiAwarenessContext_t)(DPI_AWARENESS_CONTEXT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetProcessDpiAwarenessContext_t set_ctx = (SetProcessDpiAwarenessContext_t)
            GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (set_ctx && set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            return;
    }
    SetProcessDPIAware();
}

static UINT window_dpi(HWND hwnd)
{
    typedef UINT(WINAPI* GetDpiForWindow_t)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    UINT dpi = 0;
    if (user32) {
        GetDpiForWindow_t get_dpi = (GetDpiForWindow_t)
            GetProcAddress(user32, "GetDpiForWindow");
        if (get_dpi) dpi = get_dpi(hwnd);
    }
    if (dpi == 0) {
        HDC hdc = GetDC(NULL);
        if (hdc) {
            dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(NULL, hdc);
        }
    }
    return dpi ? dpi : USER_DEFAULT_SCREEN_DPI;
}

static void size_window_for_dpi(HWND hwnd, UINT dpi, DWORD style)
{
    typedef BOOL(WINAPI* AdjustWindowRectExForDpi_t)(LPRECT, DWORD, BOOL, DWORD, UINT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    AdjustWindowRectExForDpi_t adjust_for_dpi = NULL;
    RECT wr;

    wr.left = 0;
    wr.top = 0;
    wr.right = MulDiv(DESIGN_W, (int)dpi, USER_DEFAULT_SCREEN_DPI);
    wr.bottom = MulDiv(DESIGN_H, (int)dpi, USER_DEFAULT_SCREEN_DPI);

    if (user32)
        adjust_for_dpi = (AdjustWindowRectExForDpi_t)
            GetProcAddress(user32, "AdjustWindowRectExForDpi");
    if (adjust_for_dpi)
        adjust_for_dpi(&wr, style, FALSE, 0, dpi);
    else
        AdjustWindowRect(&wr, style, FALSE);

    SetWindowPos(hwnd, NULL, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/* ------------------------------------------------------------------ */
/* borderless fullscreen, ALT+ENTER                                    */
/* ------------------------------------------------------------------ */

static int      is_fullscreen;
static RECT     windowed_rect;
static LONG_PTR windowed_style;
static LONG_PTR windowed_exstyle;

int IsFullscreen(void) { return is_fullscreen; }

void ToggleFullscreen(void)
{
    if (is_fullscreen) {
        SetWindowLongPtr(hWnd, GWL_STYLE, windowed_style);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, windowed_exstyle);
        SetWindowPos(hWnd, HWND_NOTOPMOST,
                     windowed_rect.left, windowed_rect.top,
                     windowed_rect.right - windowed_rect.left,
                     windowed_rect.bottom - windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 0;
    } else {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        GetWindowRect(hWnd, &windowed_rect);
        windowed_style = GetWindowLongPtr(hWnd, GWL_STYLE);
        windowed_exstyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW | WS_EX_TOPMOST);
        SetWindowPos(hWnd, HWND_TOPMOST,
                     mi.rcMonitor.left, mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        is_fullscreen = 1;
    }
}

/* ------------------------------------------------------------------ */
/* resolution-independent beam width                                   */
/* ------------------------------------------------------------------ */

/* The beam renderer takes its width and AA feather in DESIGN units (the
 * beam projection's 1023 units span the letterboxed viewport), so a
 * fixed design width gets FAT at small windows and thin at big ones.  In
 * this backend the two ini keys the renderer reads, [vector] linewidth
 * and line_smoothing, therefore mean the beam width in PIXELS AT THE DEFAULT 1024-WIDE
 * WINDOW, scaling in proportion with the picture from there, and the AA
 * feather in PHYSICAL PIXELS on any screen (defaults 2.5 and 1.25) - see
 * update_beam_width() for both conversions.  (Same rule in every port
 * that shares this backend - the Space Duel port established it,
 * 2026-09-03.)  At the default 1024-wide window one design unit is
 * about one pixel, so the old numbers look the same there. */
static float beam_px = 2.5f;             /* [vector] linewidth, pixels      */
static float beam_feather_px = 1.25f;    /* [vector] line_smoothing, pixels */

static void update_beam_width(void)
{
    double vw;

    /* WIDTH IS PROPORTIONAL (2026-09-03, user's choice after trying
     * constant-pixel): the beam scales with the picture, so a window and
     * fullscreen look the same relative to the drawing.  [vector]
     * linewidth is calibrated as pixels at the default DESIGN_W-wide
     * (1024) window: one design unit of the beam projection's 1023-unit
     * span is 1023 / 1024 of a pixel there, so linewidth=2.5 draws 2.5 px
     * at that size and 2.5 * (viewport width / 1024) px on a bigger
     * screen. */
    beam_set_linewidth((float)(beam_px * 1023.0 / (double)DESIGN_W));

    /* THE FEATHER IS PHYSICAL PIXELS: anti-aliasing is a property of the
     * pixel grid, not of the drawing, so [vector] line_smoothing means
     * that many pixels of edge ramp on ANY screen - converted from the
     * letterboxed viewport's actual pixel width (ViewOrthoScaled's fit),
     * and redone on every WM_SIZE, fullscreen toggles included.  (Scaling
     * the feather with the picture, as the first proportional version
     * did, made every setting look fully anti-aliased in fullscreen: a
     * 0.8 ramp became 2 px on a 2560-wide panel.) */
    vw = (double)SCREEN_W;
    if ((double)SCREEN_H * DESIGN_W < vw * DESIGN_H)
        vw = (double)SCREEN_H * DESIGN_W / (double)DESIGN_H;
    if (vw < 1.0) vw = 1.0;
    beam_set_smoothing((float)(beam_feather_px * 1023.0 / vw));
}

/* ------------------------------------------------------------------ */
/* window procedure                                                    */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            SCREEN_W = LOWORD(lParam);
            SCREEN_H = HIWORD(lParam);
            ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);
            update_beam_width();
        }
        return 0;

    case WM_DPICHANGED: {
        const RECT* suggested = (const RECT*)lParam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_SYSKEYDOWN:
        /* ALT+ENTER; lParam bit 29 is the recorded ALT context flag. */
        if (wParam == VK_RETURN && (lParam & (1 << 29))) {
            ToggleFullscreen();
            key[KEY_ENTER] = 0;    /* raw input saw the keystroke too */
            return 0;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_ERASEBKGND:
        return 1;                  /* GL owns every pixel */

    case WM_SYSCOMMAND:
        /* Thrust is bound to ALT, and DefWindowProc turns a lone ALT
         * release into SC_KEYMENU - the modal window-menu loop, which
         * blocks the game loop until the next input (reads as a
         * multi-second freeze). No menu exists; eat it. */
        if ((wParam & 0xFFF0) == SC_KEYMENU) return 0;
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_SETCURSOR:
        /* Fullscreen is the cabinet: no cursor over the game. Windowed
         * mode (and the frame edges) keep the arrow for resizing. */
        if (is_fullscreen && LOWORD(lParam) == HTCLIENT) {
            SetCursor(NULL);
            return 1;
        }
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_INPUT:
        RawInput_ProcessInput(hwnd, wParam, lParam);
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_DEVICECHANGE:
        joystick_device_change();
        return DefWindowProc(hwnd, message, wParam, lParam);

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { PostQuitMessage(0); return 0; }
        return 0;

    default:
        return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

/* ------------------------------------------------------------------ */
/* lifecycle                                                           */
/* ------------------------------------------------------------------ */

static float beam_proj[16];

/* [sound] ay8910 and ay_volume, read in plat_init: whether the core runs
 * its AY-3-8910 sound-board port (streamed PCM) instead of the sample map,
 * and the stream's volume when it does. See the "audio" block below and
 * omega_platform.h's PCM-stream comment. */
static int ay_mode = 1;         /* [sound] ay8910, default 1 (the AYs);
                                   0 = the recorded sample set           */
static int ay_volume = 100;     /* [sound] ay_volume, percent 0..100     */

int plat_init(void)
{
    WNDCLASS wc;
    DWORD dwStyle;
    RECT wr = { 0, 0, 1024, 768 };
    int swap_mode;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    enable_dpi_awareness();

    memset(&wc, 0, sizeof wc);
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"OmegaRaceWin";
    if (!RegisterClass(&wc)) {
        msg_box("Omega Race", "Failed to register the window class.");
        return 1;
    }

    dwStyle = WS_CAPTION | WS_POPUPWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS
            | WS_CLIPCHILDREN | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    AdjustWindowRect(&wr, dwStyle, FALSE);
    hWnd = CreateWindow(L"OmegaRaceWin", L"Omega Race (C port)", dwStyle,
                        0, 0, wr.right - wr.left, wr.bottom - wr.top,
                        NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        msg_box("Omega Race", "Failed to create the main window.");
        return 1;
    }

    log_open("omega_win.log");
    log_set_level(LOG_LEVEL_INFO);
    LOG_INFO("omega_win backend starting");

    size_window_for_dpi(hWnd, window_dpi(hWnd), dwStyle);

    /* Settings. beam_init reads [vector] from the same file. */
    set_config_file("omega_win.ini");
    /* vsync OFF by default: the game paces itself at the hardware's
     * floating rate (~46.5 fps play) and a vsynced flip quantizes that
     * to the panel's refresh, which reads as judder
     * (FRAME_PACING_NOTES.md). */
    swap_mode = get_config_int("main", "vsync", 0);
    set_config_int("main", "vsync", swap_mode);

    /* [sound] ay8910: nonzero (the default) runs the core's AY-3-8910
     * sound-board port (streamed PCM); 0 selects the recorded sample set
     * instead. If the stream cannot be opened the core falls back to the
     * samples by itself. ay_volume: the stream's volume, 0..100 percent. */
    ay_mode = get_config_int("sound", "ay8910", 1) != 0;
    set_config_int("sound", "ay8910", ay_mode);
    ay_volume = get_config_int("sound", "ay_volume", 100);
    if (ay_volume < 0)   ay_volume = 0;
    if (ay_volume > 100) ay_volume = 100;
    set_config_int("sound", "ay_volume", ay_volume);

    /* Phosphor decay time constant, ms. See the block above plat_video_
     * begin: a tuning knob, defaulted short because the V2000's
     * persistence at normal brightness is short. 0 = off. */
    ph_tau = (double)get_config_float("vector", "phosphor_ms", 12.0f);
    set_config_float("vector", "phosphor_ms", (float)ph_tau);

    if (FAILED(RawInput_Initialize(hWnd))) {
        msg_box("Omega Race",
            "Failed to register for raw input; keyboard and mouse will not work.");
        LOG_ERROR("RawInput_Initialize failed");
    }

    install_joystick();          /* zero sticks is fine */

    if (mixer_init() != 0)
        LOG_ERROR("mixer_init failed; continuing without audio");

    if (!CreateGLContext()) {
        msg_box("Omega Race", "Failed to create an OpenGL context.");
        return 1;
    }
    SetSwapMode(swap_mode);
    ViewOrthoScaled(SCREEN_W, SCREEN_H, DESIGN_W, DESIGN_H);

    if (!beam_init()) {
        msg_box("Omega Race",
            "Beam renderer failed to build (needs OpenGL 3.3) - see omega_win.log.");
        return 1;
    }
    beam_set_color_mode(0);      /* B/W game: sorted alpha-over */

    /* DVG space is 0..1023 on both axes, y up, squeezed into the 4:3
     * design rect. */
    mat4_ortho(beam_proj, 0.0f, 1023.0f, 0.0f, 1023.0f, -1.0f, 1.0f);

    /* Pixel-based beam width (see update_beam_width above).  beam_init has
     * just read the same two keys as design units and applied them; read
     * them again here as pixels and re-apply, so the ini's numbers are the
     * ones on screen at any window size. */
    beam_px = get_config_float("vector", "linewidth", 2.5f);
    beam_feather_px = get_config_float("vector", "line_smoothing", 1.25f);
    if (beam_px < 0.1f) beam_px = 0.1f;
    if (beam_feather_px < 0.0f) beam_feather_px = 0.0f;
    set_config_float("vector", "linewidth", beam_px);
    set_config_float("vector", "line_smoothing", beam_feather_px);
    update_beam_width();
    LOG_INFO("beam: %.2f px wide at a 1024-wide window (proportional), %.2f px feather on any screen ([vector] linewidth / line_smoothing)",
             beam_px, beam_feather_px);

    /* 1 ms scheduler resolution: without it Sleep(1) rounds up to the
     * 15.6 ms quantum and the pacing loop repays the debt by firing
     * frames early - which reads as judder. */
    timeBeginPeriod(1);

    set_window_title("Omega Race (C port) - Left/Right turn, Space or Ctrl fire, "
                     "Up or Alt thrust, 5 coin, 1 start, F2 test, Esc quit");
    ShowWindow(hWnd, SW_SHOW);
    return 0;
}

void plat_shutdown(void)
{
    timeEndPeriod(1);
    beam_shutdown();
    remove_joystick();
    plat_audio_close();     /* stream voice first: mixer_end tears down g_xa2 */
    mixer_end();
    DeleteGLContext();
    LOG_INFO("omega_win backend closing");
    log_close();
    if (IsWindow(hWnd)) DestroyWindow(hWnd);
}

/* ------------------------------------------------------------------ */
/* video: the beam segment sink                                        */
/* ------------------------------------------------------------------ */

void plat_video_begin(void)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ph_cur = (ph_cur + 1) % PH_FRAMES;
    ph_buf[ph_cur].n = 0;
    ph_buf[ph_cur].used = 1;
}

/* Line sink for the DVG walker. Intensity z is the PROM state machine's
 * 0..7; (z<<4)|0x0f is the GUI's established brightness mapping. A
 * zero-length segment is a dot (shot sparks): the beam renderer gives
 * the degenerate segment's lone endpoint a round end-cap disc, so dots
 * ride the same shader path as lines - the machine draws only lines
 * and dots.
 * Corner brightening needs no endpoint dots here: coincident endpoints
 * become round joins automatically. */
void plat_video_line(float x0, float y0, float x1, float y1, int z)
{
    ph_frame* f = &ph_buf[ph_cur];
    if (f->n < PH_SEGS) {
        ph_seg* s = &f->seg[f->n++];
        s->x0 = x0; s->y0 = y0; s->x1 = x1; s->y1 = y1; s->z = z;
    } else if (!ph_dropped) {
        ph_dropped = 1;
        LOG_WARN("display list exceeded %d segments; phosphor history "
                 "is truncating frames", PH_SEGS);
    }
}

/* Replay the phosphor history newest-last, each frame scaled by its own
 * decay, then draw the batch. beam_set_color_mode(0) sorts alpha-over so
 * a bright fresh beam occludes the dim ghost underneath it, which is the
 * right compositing for a B/W tube. */
void plat_video_present(void)
{
    double now = plat_now_ms();
    int i;

    ph_buf[ph_cur].t = now;

    beam_clear();
    for (i = 0; i < PH_FRAMES; i++) {
        /* oldest first: start one past the newest and wrap */
        ph_frame* f = &ph_buf[(ph_cur + 1 + i) % PH_FRAMES];
        double fac;
        int k;

        if (!f->used) continue;
        if (f == &ph_buf[ph_cur]) fac = 1.0;
        else if (ph_tau <= 0.0)   continue;  /* phosphor off */
        else {
            fac = exp(-(now - f->t) / ph_tau);
            if (fac < PH_FLOOR) continue;
        }
        for (k = 0; k < f->n; k++) {
            const ph_seg* s = &f->seg[k];
            int intens = (int)((double)((s->z << 4) | 0x0f) * fac + 0.5);
            if (intens <= 0) continue;
            beam_add_line(s->x0, s->y0, s->x1, s->y1, intens, RGB_WHITE);
        }
    }

    beam_draw_all(beam_proj);
    glSwap();
}

/* ------------------------------------------------------------------ */
/* input                                                               */
/* ------------------------------------------------------------------ */

void plat_input_poll(plat_inputs* in)
{
    int mdx, mdy;

    memset(in, 0, sizeof *in);

    /* Fire and thrust each answer to two keys (Alt grabs the window
     * menu on Windows, so Up/Space are first-class). */
    in->fire   = (key[KEY_LCONTROL] || key[KEY_SPACE]) ? 1 : 0;
    in->thrust = (key[KEY_ALT] || key[KEY_UP]) ? 1 : 0;
    in->coin1  = key[KEY_5] ? 1 : 0;
    in->coin2  = key[KEY_6] ? 1 : 0;
    in->start1 = key[KEY_1] ? 1 : 0;
    in->start2 = key[KEY_2] ? 1 : 0;
    /* 9 is the cabinet test-switch line, F2 the diagnostics affordance
     * (edge-detected in the core). */
    in->test   = (key[KEY_9] || key[KEY_F2]) ? 1 : 0;
    in->diag   = key[KEY_F2] ? 1 : 0;
    in->quit   = key[KEY_ESC] ? 1 : 0;

    /* Spinner: positive delta is the KEY_RIGHT direction, and mouse
     * right matches it (mouse right = +1) so the ship turns the way
     * the mouse moves. get_mouse_mickeys drains the raw-input
     * accumulators. */
    get_mouse_mickeys(&mdx, &mdy);
    (void)mdy;
    if (mdx > 0) in->spinner_delta += 1;
    if (mdx < 0) in->spinner_delta -= 1;
    if (key[KEY_LEFT])  in->spinner_delta -= 1;
    if (key[KEY_RIGHT]) in->spinner_delta += 1;

    /* Joystick: stick left/right spins, button 1 fires, 2 thrusts,
     * 8 coins, 9 starts (an Xbox pad's Back/Start land there). Inert
     * with no device. */
    poll_joystick();
    if (num_joysticks > 0 && joystick_is_connected(0)) {
        if (joy_left)  in->spinner_delta -= 1;
        if (joy_right) in->spinner_delta += 1;
        if (joy[0].num_buttons > 0 && joy[0].button[0].b) in->fire   = 1;
        if (joy[0].num_buttons > 1 && joy[0].button[1].b) in->thrust = 1;
        if (joy[0].num_buttons > 7 && joy[0].button[7].b) in->coin1  = 1;
        if (joy[0].num_buttons > 8 && joy[0].button[8].b) in->start1 = 1;
    }
}

uint8_t plat_dsw_c4(void) { return 0x5f; }
uint8_t plat_dsw_c6(void) { return 0xbf; }

void plat_leds_out(uint8_t led_shadow) { (void)led_shadow; }

/* ------------------------------------------------------------------ */
/* audio: sample playback, and the AY-3-8910 PCM stream                */
/* ------------------------------------------------------------------ */

/* Two sound paths share this backend's mixer:
 *  - the AY-3-8910 sound-board port (sound_board.c + ay8910.c, core) - the
 *    default, [sound] ay8910=1: plat_sound_use_ay() tells the core to
 *    render both chips to one mono 16-bit block per 244 Hz tick and push
 *    it through plat_audio_push; mixer.c's stream voice (XAudio2) plays
 *    the blocks back to back.
 *  - samples, [sound] ay8910=0 (and the fallback if the stream will not
 *    open): sound_samples.c (core) maps command bytes to hex-named wavs,
 *    played on the mixer channels below. */

int plat_sound_use_ay(void) { return ay_mode; }

int plat_audio_open(int sample_rate)
{
    if (stream_open(sample_rate, 1) != 0)
        return -1;
    stream_set_volume(mixer_percent_to_byte(ay_volume));
    LOG_INFO("AY-3-8910 stream open at %d Hz, volume %d%%", sample_rate, ay_volume);
    return 0;
}

void plat_audio_push(const int16_t* pcm, int frames)
{
    stream_push(pcm, frames);
}

void plat_audio_close(void)
{
    stream_close();
}

/* sample number -> wav member of samples\omegrace.zip (a MAME-style
 * sample set). The number IS the port-0x14 command byte and the members
 * are named in hex by that byte: 1..4, 7..f, 10..16. Commands 5 and 6
 * have no file (the sound CPU's own harmony voices; the main CPU never
 * sends them - see disasm/SOUND_NOTES.md). */
#define SAMPLES_ZIP "samples\\omegrace.zip"

static const char* sample_file(int num)
{
    static char name[16];

    if (num < 0x01 || num > 0x16 || num == 0x05 || num == 0x06) return NULL;
    sprintf(name, "%x.wav", num);
    return name;
}

static int wav_tried[0x20];
static int wav_num[0x20];        /* mixer sample number, -1 = missing */

void plat_sample_start(int channel, int sample, int loop)
{
    if (channel < 0 || channel >= MIXER_MAX_CHANNELS) return;
    if (sample < 0 || sample >= 0x20) return;
    if (!wav_tried[sample]) {
        const char* fn = sample_file(sample);
        wav_num[sample] = fn ? load_sample(SAMPLES_ZIP, fn) : -1;
        wav_tried[sample] = 1;
    }
    if (wav_num[sample] < 0) return;
    sample_start(channel, wav_num[sample], loop);
}

void plat_sample_stop(int channel)
{
    if (channel < 0 || channel >= MIXER_MAX_CHANNELS) return;
    sample_stop(channel);
}

/* ------------------------------------------------------------------ */
/* time                                                                */
/* ------------------------------------------------------------------ */

double plat_now_ms(void)
{
    LARGE_INTEGER cnt;
    static LARGE_INTEGER freq;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart * 1000.0 / (double)freq.QuadPart;
}

void plat_sleep_ms(int ms) { Sleep((DWORD)ms); }

/* ------------------------------------------------------------------ */
/* NVRAM: the core's blob, stored as omega_c.nv (format unchanged)     */
/* ------------------------------------------------------------------ */

int plat_nvram_read(void* buf, unsigned len)
{
    size_t got;
    FILE* f = fopen("omega_c.nv", "rb");
    if (!f) return 1;
    got = fread(buf, 1, len, f);
    fclose(f);
    return got == len ? 0 : 1;
}

int plat_nvram_write(const void* buf, unsigned len)
{
    FILE* f = fopen("omega_c.nv", "wb");
    if (!f) return 1;
    fwrite(buf, 1, len, f);
    fclose(f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* status + main loop                                                  */
/* ------------------------------------------------------------------ */

void plat_status_text(const char* s)
{
    char buf[240];
    snprintf(buf, sizeof buf,
             "Omega Race (C port)  %s  - "
             "Left/Right turn, Space or Ctrl fire, Up or Alt thrust, "
             "5 coin, 1 start, Esc quit", s);
    set_window_title(buf);

    /* AY mode: the stream's health, about once a second, to omega_win.log.
     * "starved" is the number that matters - each one is an audible gap. */
    if (ay_mode) {
        char st[160];
        stream_stats(st, sizeof st);
        LOG_INFO("%s", st);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int iCmdShow)
{
    MSG msg;
    (void)hInstance; (void)hPrevInstance; (void)lpCmdLine; (void)iCmdShow;

    if (plat_init()) return 1;
    omega_app_init();

    while (!g_quit) {
        /* Drain the queue - raw input arrives at the mouse's report rate
         * and would starve the loop if handled one message per pass. */
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { g_quit = 1; break; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (g_quit) break;

        {
            double wait = omega_app_step(plat_now_ms());
            if (wait > 3.0) plat_sleep_ms(1);   /* 1 ms sleep, then spin */
        }
    }

    omega_hw_nvram_save();
    plat_shutdown();
    return 0;
}
