/* main.c —— Win64 图形界面入口：窗口、消息循环、交互与截图模式。
 *
 * 用法：
 *   navgui.exe                                  打开窗口
 *   navgui.exe --data data/campus_real          指定数据目录
 *   navgui.exe --shot out.png [--route 1 5]     离屏渲染一帧并保存 PNG（便于回归检查）
 */
#include "gui.h"
#include "platform.h"

#include <windowsx.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdiplus/gdiplus.h>

/* UTF-8 -> UTF-16（环形缓冲，供菜单/消息框等一次性使用） */
#define WBUFS 16
static const WCHAR *W(const char *utf8)
{
    static WCHAR bufs[WBUFS][512];
    static int idx = 0;
    WCHAR *out = bufs[idx];
    idx = (idx + 1) % WBUFS;
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, 512) <= 0) {
        out[0] = 0;
    }
    return out;
}

static Gui g_gui;
static char g_data_dir[512];
static GpBitmap *s_back = NULL;
static GpGraphics *s_back_gr = NULL;

/* ---------------- 离屏位图与 PNG 输出 ---------------- */

static int png_encoder_clsid(CLSID *out)
{
    UINT num = 0, size = 0, i;
    ImageCodecInfo *info;

    if (GdipGetImageEncodersSize(&num, &size) != Ok || size == 0) {
        return -1;
    }
    info = (ImageCodecInfo *)malloc(size);
    if (info == NULL) {
        return -1;
    }
    if (GdipGetImageEncoders(num, size, info) != Ok) {
        free(info);
        return -1;
    }
    for (i = 0; i < num; ++i) {
        if (wcscmp(info[i].MimeType, L"image/png") == 0) {
            *out = info[i].Clsid;
            free(info);
            return 0;
        }
    }
    free(info);
    return -1;
}

static int save_png(GpBitmap *bmp, const char *utf8_path)
{
    CLSID clsid;
    WCHAR path[512];

    if (png_encoder_clsid(&clsid) != 0) {
        return -1;
    }
    MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, path, 512);
    return GdipSaveImageToFile((GpImage *)bmp, path, &clsid, NULL) == Ok ? 0 : -1;
}

static int ensure_backbuffer(int w, int h)
{
    if (w <= 0 || h <= 0) {
        return -1;
    }
    if (s_back != NULL) {
        UINT bw = 0, bh = 0;
        GdipGetImageWidth((GpImage *)s_back, &bw);
        GdipGetImageHeight((GpImage *)s_back, &bh);
        if ((int)bw == w && (int)bh == h) {
            return 0;
        }
        GdipDeleteGraphics(s_back_gr);
        s_back_gr = NULL;
        GdipDisposeImage((GpImage *)s_back);
        s_back = NULL;
    }
    if (GdipCreateBitmapFromScan0(w, h, 0, PixelFormat32bppPARGB, NULL, &s_back) != Ok) {
        return -1;
    }
    if (GdipGetImageGraphicsContext((GpImage *)s_back, &s_back_gr) != Ok) {
        return -1;
    }
    return 0;
}

/* ---------------- 动作 ---------------- */

enum {
    ACT_NONE = 0,
    ACT_PLAN = 1,
    ACT_SET_FROM,
    ACT_SET_TO,
    ACT_NEAR_CANTEEN,
    ACT_NEAR_MEDICAL,
    ACT_CRIT,
    ACT_FIT,
    ACT_SWAP,
    ACT_CLEAR,
    ACT_CRITICAL,
    ACT_TOUR,
    ACT_ZOOM_IN,
    ACT_ZOOM_OUT,
    ACT_ABOUT
};

static void do_action(HWND hwnd, int act)
{
    switch (act) {
    case ACT_PLAN:
        if (gui_plan(&g_gui) == NAV_OK) {
            SetTimer(hwnd, 2, 45, NULL);   /* 路径逐段出现的动画 */
        }
        break;
    case ACT_SET_FROM:
        if (g_gui.selected > 0) {
            g_gui.from_id = g_gui.selected;
            snprintf(g_gui.status, sizeof(g_gui.status), "起点设为：%s",
                     gui_spot_name(&g_gui, g_gui.selected));
            g_gui.status_error = 0;
        }
        break;
    case ACT_SET_TO:
        if (g_gui.selected > 0) {
            g_gui.to_id = g_gui.selected;
            snprintf(g_gui.status, sizeof(g_gui.status), "终点设为：%s",
                     gui_spot_name(&g_gui, g_gui.selected));
            g_gui.status_error = 0;
        }
        break;
    case ACT_NEAR_CANTEEN:
        gui_nearest(&g_gui, SPOT_CANTEEN);
        break;
    case ACT_NEAR_MEDICAL:
        gui_nearest(&g_gui, SPOT_MEDICAL);
        break;
    case ACT_SWAP: {
        int t = g_gui.from_id;
        g_gui.from_id = g_gui.to_id;
        g_gui.to_id = t;
        break;
    }
    case ACT_CLEAR:
        g_gui.has_path = 0;
        g_gui.path.count = 0;
        strcpy(g_gui.status, "已清除路径");
        g_gui.status_error = 0;
        break;
    case ACT_FIT: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        gui_fit_view(&g_gui, rc.right, rc.bottom);
        break;
    }
    case ACT_CRITICAL:
        gui_toggle_critical(&g_gui);
        break;
    case ACT_TOUR:
        if (gui_tour(&g_gui) == NAV_OK) {
            SetTimer(hwnd, 2, 45, NULL);
        }
        break;
    case ACT_ZOOM_IN:
        gui_zoom_at(&g_gui, g_gui.cw / 2, g_gui.ch / 2, 1.25);
        break;
    case ACT_ZOOM_OUT:
        gui_zoom_at(&g_gui, g_gui.cw / 2, g_gui.ch / 2, 1.0 / 1.25);
        break;
    case ACT_ABOUT:
        MessageBoxW(hwnd,
                    W("教室导航（图的最短路径）\n\n"
                      "Win32 + GDI+ 图形界面\n"
                      "核心算法与命令行版共用同一套库\n\n"
                      "数据：江苏海洋大学苍梧校区真实路网\n"
                      "353 个节点（20 个地点 + 333 个路口）· 409 条道路"),
                    W("关于"), MB_OK | MB_ICONINFORMATION);
        break;
    default:
        break;
    }
    InvalidateRect(hwnd, NULL, FALSE);
}

/* ---------------- 窗口过程 ---------------- */

static void paint_window(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GpGraphics *gr = NULL;

    GetClientRect(hwnd, &rc);
    if (ensure_backbuffer(rc.right, rc.bottom) == 0) {
        gui_paint(&g_gui, s_back_gr, rc.right, rc.bottom);
        GdipCreateFromHDC(hdc, &gr);
        if (gr != NULL) {
            GdipSetInterpolationMode(gr, InterpolationModeNearestNeighbor);
            GdipDrawImageI(gr, (GpImage *)s_back, 0, 0);
            GdipDeleteGraphics(gr);
        }
    }
    EndPaint(hwnd, &ps);
}

static void export_view(HWND hwnd);

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        paint_window(hwnd);
        return 0;
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED) {
            g_gui.cw = LOWORD(lp);
            g_gui.ch = HIWORD(lp);
            ensure_backbuffer(g_gui.cw, g_gui.ch);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int old_hover = g_gui.hover;
        if (g_gui.dragging) {
            gui_pan(&g_gui, g_gui.drag_pt.x - x, g_gui.drag_pt.y - y);
            g_gui.drag_pt.x = x;
            g_gui.drag_pt.y = y;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        g_gui.mouse_x = x;
        g_gui.mouse_y = y;
        g_gui.hover = gui_hit_test(&g_gui, x, y);
        if (g_gui.hover != old_hover || g_gui.hover > 0) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int act = gui_panel_hit(&g_gui, x, y);
        if (act != ACT_NONE) {
            do_action(hwnd, act);
            return 0;
        }
        if (x < g_gui.cw - GUI_PANEL_W) {
            int hit = gui_hit_test(&g_gui, x, y);
            if (hit > 0) {
                g_gui.selected = hit;
                snprintf(g_gui.status, sizeof(g_gui.status), "选中：%s（编号 %d）",
                         gui_spot_name(&g_gui, hit), hit);
                g_gui.status_error = 0;
            } else {
                g_gui.dragging = 1;
                g_gui.drag_pt.x = x;
                g_gui.drag_pt.y = y;
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        g_gui.dragging = 0;
        return 0;
    case WM_RBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
        int hit = gui_hit_test(&g_gui, x, y);
        if (hit > 0) {
            gui_toggle_via(&g_gui, hit);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT pt;
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        pt.x = GET_X_LPARAM(lp);
        pt.y = GET_Y_LPARAM(lp);
        ScreenToClient(hwnd, &pt);
        gui_zoom_at(&g_gui, pt.x, pt.y, delta > 0 ? 1.15 : 1.0 / 1.15);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_RETURN) {
            do_action(hwnd, ACT_PLAN);
        } else if (wp == VK_F5) {
            do_action(hwnd, ACT_FIT);
        } else if (wp == VK_ESCAPE) {
            do_action(hwnd, ACT_CLEAR);
        } else if (wp == '1') {
            do_action(hwnd, ACT_SET_FROM);
        } else if (wp == '2') {
            do_action(hwnd, ACT_SET_TO);
        } else if (wp == VK_ADD || wp == VK_OEM_PLUS) {
            do_action(hwnd, ACT_ZOOM_IN);
        } else if (wp == VK_SUBTRACT || wp == VK_OEM_MINUS) {
            do_action(hwnd, ACT_ZOOM_OUT);
        } else if (wp == 'T') {
            do_action(hwnd, ACT_TOUR);
        } else if (wp == VK_HOME) {
            do_action(hwnd, ACT_FIT);
        } else if (wp == VK_LEFT) {
            gui_pan(&g_gui, 60, 0);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (wp == VK_RIGHT) {
            gui_pan(&g_gui, -60, 0);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (wp == VK_UP) {
            gui_pan(&g_gui, 0, 60);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (wp == VK_DOWN) {
            gui_pan(&g_gui, 0, -60);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (wp == VK_SPACE && g_gui.has_path) {
            g_gui.anim_playing = 1;
            g_gui.anim_segments = 0;
            SetTimer(hwnd, 2, 45, NULL);
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == 0x9002) {
            MessageBoxW(hwnd,
                        W("鼠标：滚轮缩放 · 按住拖动平移 · 单击选点\n"
                          "键盘：1 设为起点 · 2 设为终点 · Enter 规划\n"
                          "      + / - 缩放 · 方向键平移 · Home 适应窗口\n"
                          "      空格 重播路径动画 · Esc 清除路径\n"
                          "菜单：文件可导出当前视图为 PNG"),
                        W("快捷键说明"), MB_OK | MB_ICONINFORMATION);
        } else if (LOWORD(wp) == 0x9001) {
            export_view(hwnd);
        } else if (LOWORD(wp) == 0x9000) {
            DestroyWindow(hwnd);
        } else {
            do_action(hwnd, (int)LOWORD(wp));
        }
        return 0;
    case WM_TIMER:
        if (wp == 2) {
            if (!g_gui.anim_playing) {
                KillTimer(hwnd, 2);
                return 0;
            }
            ++g_gui.anim_segments;
            if (g_gui.anim_segments > g_gui.path.count) {
                g_gui.anim_playing = 0;
                g_gui.anim_segments = 0;
                KillTimer(hwnd, 2);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        KillTimer(hwnd, 1);
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

static void export_view(HWND hwnd)
{
    RECT rc;
    char path[MAX_PATH];
    char msg[MAX_PATH + 64];
    SYSTEMTIME st;

    GetClientRect(hwnd, &rc);
    if (ensure_backbuffer(rc.right, rc.bottom) != 0) {
        MessageBoxW(hwnd, W("创建离屏位图失败"), W("导出失败"), MB_OK | MB_ICONERROR);
        return;
    }
    gui_paint(&g_gui, s_back_gr, rc.right, rc.bottom);
    GetLocalTime(&st);
    snprintf(path, sizeof(path), "campus-nav-%04d%02d%02d-%02d%02d%02d.png", st.wYear,
             st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    if (save_png(s_back, path) != 0) {
        MessageBoxW(hwnd, W("保存 PNG 失败"), W("导出失败"), MB_OK | MB_ICONERROR);
        return;
    }
    snprintf(msg, sizeof(msg), "已保存：%s", path);
    MessageBoxW(hwnd, W(msg), W("导出当前视图"), MB_OK | MB_ICONINFORMATION);
}

static HMENU build_menu(void)
{
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU path = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, ACT_FIT, W("适应窗口(&F)\tF5"));
    AppendMenuW(file, MF_STRING, 0x9001, W("导出当前视图(PNG)"));
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, 0x9000, W("退出(&X)"));
    AppendMenuW(view, MF_STRING, ACT_ZOOM_IN, W("放大\t+"));
    AppendMenuW(view, MF_STRING, ACT_ZOOM_OUT, W("缩小\t-"));
    AppendMenuW(view, MF_SEPARATOR, 0, NULL);
    AppendMenuW(view, MF_STRING, ACT_CRITICAL, W("关键道路高亮"));
    AppendMenuW(view, MF_SEPARATOR, 0, NULL);
    AppendMenuW(view, MF_STRING, ACT_NEAR_CANTEEN, W("最近食堂"));
    AppendMenuW(view, MF_STRING, ACT_NEAR_MEDICAL, W("最近校医院"));
    AppendMenuW(path, MF_STRING, ACT_PLAN, W("规划路径(&R)\tEnter"));
    AppendMenuW(path, MF_STRING, ACT_TOUR, W("多点巡游\tCtrl+T"));
    AppendMenuW(path, MF_STRING, ACT_SWAP, W("交换起终点"));
    AppendMenuW(path, MF_STRING, ACT_CLEAR, W("清除路径\tEsc"));
    AppendMenuW(help, MF_STRING, 0x9002, W("快捷键说明(&K)"));
    AppendMenuW(help, MF_SEPARATOR, 0, NULL);
    AppendMenuW(help, MF_STRING, ACT_ABOUT, W("关于(&A)"));

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, W("文件(&F)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, W("查询(&Q)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)path, W("路径(&P)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, W("帮助(&H)"));
    return bar;
}



/* 数据目录解析：优先命令行；否则依次尝试 当前目录 / exe 同级 / exe 上一级 */
static int resolve_data_dir(char *out, size_t cap, const char *cli_dir)
{
    static const char *names[] = { "data/campus_real", "data\\campus_real" };
    char exe[MAX_PATH];
    int i;

    if (cli_dir != NULL && cli_dir[0] != '\0') {
        snprintf(out, cap, "%s", cli_dir);
        return 0;
    }
    for (i = 0; i < 2; ++i) {
        FILE *fp;
        char probe[MAX_PATH * 2];
        snprintf(probe, sizeof(probe), "%s/spots.csv", names[i]);
        fp = fopen(probe, "rb");
        if (fp != NULL) {
            fclose(fp);
            snprintf(out, cap, "%s", names[i]);
            return 0;
        }
    }
    if (GetModuleFileNameA(NULL, exe, MAX_PATH) > 0) {
        char *slash = strrchr(exe, '\\');
        if (slash != NULL) {
            *slash = '\0';
        }
        {
            char probe[MAX_PATH * 2];
            snprintf(probe, sizeof(probe), "%s/data/campus_real/spots.csv", exe);
            if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES) {
                snprintf(out, cap, "%s/data/campus_real", exe);
                return 0;
            }
            snprintf(probe, sizeof(probe), "%s/../data/campus_real/spots.csv", exe);
            if (GetFileAttributesA(probe) != INVALID_FILE_ATTRIBUTES) {
                snprintf(out, cap, "%s/../data/campus_real", exe);
                return 0;
            }
        }
    }
    snprintf(out, cap, "%s", names[0]);
    return -1;
}

/* ---------------- 截图模式 ---------------- */

static int run_screenshot(int argc, char **argv, const char *out, int route_from, int route_to,
                          int hover_id, int critical, int w, int h, const char *via_csv)
{
    GpBitmap *bmp = NULL;
    GpGraphics *gr = NULL;
    int i;
    (void)argc;
    (void)argv;
    (void)i;
    {
        char err[256] = { 0 };
        if (gui_init(&g_gui, g_data_dir, err, sizeof(err)) != NAV_OK) {
            fprintf(stderr, "init failed: dir=%s err=%s\n", g_data_dir, err);
            return 2;
        }
    }
    gui_fit_view(&g_gui, w, h);
    if (critical) {
        gui_toggle_critical(&g_gui);
    }
    if (hover_id > 0) {
        const Spot *hs = campus_spot(&g_gui.campus, hover_id);
        g_gui.hover = hover_id;
        if (hs != NULL) {
            double px, py;
            gui_world_to_screen(&g_gui, hs->x, hs->y, &px, &py);
            g_gui.mouse_x = (int)px + 6;
            g_gui.mouse_y = (int)py + 6;
        }
    }
    if (route_from > 0 && route_to > 0) {
        g_gui.from_id = route_from;
        g_gui.to_id = route_to;
        if (via_csv != NULL && via_csv[0] != '\0') {
            const char *p = via_csv;
            while (*p != '\0' && g_gui.n_via < 8) {
                int id = atoi(p);
                if (id > 0) {
                    g_gui.via[g_gui.n_via++] = id;
                }
                while (*p != '\0' && *p != ',') {
                    ++p;
                }
                if (*p == ',') {
                    ++p;
                }
            }
            gui_tour(&g_gui);
        } else {
            gui_plan(&g_gui);
        }
        g_gui.anim_playing = 0;   /* 截图不要动画，直接画完整路径 */
        g_gui.anim_segments = 0;
    }
    if (GdipCreateBitmapFromScan0(w, h, 0, PixelFormat32bppPARGB, NULL, &bmp) != Ok) {
        return 3;
    }
    if (GdipGetImageGraphicsContext((GpImage *)bmp, &gr) != Ok) {
        return 3;
    }
    gui_paint(&g_gui, gr, w, h);
    GdipDeleteGraphics(gr);
    if (save_png(bmp, out) != 0) {
        fprintf(stderr, "save failed: %s\n", out);
        return 4;
    }
    GdipDisposeImage((GpImage *)bmp);
    printf("screenshot saved: %s (%dx%d)\n", out, w, h);
    gui_free(&g_gui);
    return 0;
}

/* ---------------- 入口 ---------------- */

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    GdiplusStartupInput input;
    ULONG_PTR token = 0;
    WNDCLASSEXW wc;
    HWND hwnd;
    MSG msg;
    const char *shot = NULL;
    int route_from = 0, route_to = 0;
    int hover_id = 0, critical = 0;
    int open_id = 0;
    const char *via_csv = NULL;
    int size_w = 1400, size_h = 900;
    int selftest = 0;
    int i;

    (void)prev;
    (void)show;
    (void)cmdline;

    {
        int argc = 0;
        char **argv = platform_utf8_args(&argc);
        const char *cli_data = NULL;
        for (i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
                cli_data = argv[++i];
            } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
                shot = argv[++i];
            } else if (strcmp(argv[i], "--open") == 0 && i + 1 < argc) {
                open_id = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--via") == 0 && i + 1 < argc) {
                via_csv = argv[++i];
            } else if (strcmp(argv[i], "--hover") == 0 && i + 1 < argc) {
                hover_id = atoi(argv[++i]);
            } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
                int tw = 0, th = 0;
                if (sscanf(argv[++i], "%dx%d", &tw, &th) == 2 && tw > 400 && th > 300) {
                    size_w = tw;
                    size_h = th;
                }
            } else if (strcmp(argv[i], "--critical") == 0) {
                critical = 1;
            } else if (strcmp(argv[i], "--selftest") == 0) {
                selftest = 1;
            } else if (strcmp(argv[i], "--route") == 0 && i + 2 < argc) {
                route_from = atoi(argv[++i]);
                route_to = atoi(argv[++i]);
            }
        }
        resolve_data_dir(g_data_dir, sizeof(g_data_dir), cli_data);
    }

    /* 高 DPI 感知：避免系统拉伸导致模糊（Win10 1803+ 才有该 API） */
    {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (user32 != NULL) {
            typedef BOOL(WINAPI * SetCtxFn)(HANDLE);
            SetCtxFn set_ctx = (SetCtxFn)(void *)GetProcAddress(user32,
                                                               "SetProcessDpiAwarenessContext");
            if (set_ctx != NULL) {
                set_ctx((HANDLE)-4); /* PER_MONITOR_AWARE_V2 */
            } else {
                SetProcessDPIAware();
            }
        }
    }

    input.GdiplusVersion = 1;
    input.DebugEventCallback = NULL;
    input.SuppressBackgroundThread = FALSE;
    input.SuppressExternalCodecs = FALSE;
    if (GdiplusStartup(&token, &input, NULL) != Ok) {
        return 1;
    }
    gui_render_init(NULL);

    if (shot != NULL) {
        int rc;
        char base[1024];
        snprintf(base, sizeof(base), "%s/basemap.png", g_data_dir);
        gui_render_init(base);
        rc = run_screenshot(0, NULL, shot, route_from, route_to, hover_id, critical, size_w,
                            size_h, via_csv);
        gui_render_shutdown();
        GdiplusShutdown(token);
        return rc;
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"CampusNavGuiClass";
    if (!RegisterClassExW(&wc)) {
        return 1;
    }
    {
        int dpi = 96;
        int ww = 1440;
        int wh = 940;
        HDC sdc = GetDC(NULL);
        if (sdc != NULL) {
            dpi = GetDeviceCaps(sdc, LOGPIXELSX);
            ReleaseDC(NULL, sdc);
        }
        if (dpi >= 120) {
            ww = 1440 * dpi / 96;
            wh = 940 * dpi / 96;
        }
        hwnd = CreateWindowExW(0, wc.lpszClassName, W("校园导航系统"),
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, ww, wh, NULL,
                               build_menu(), inst, NULL);
    }
    if (hwnd == NULL) {
        return 1;
    }
    {
        HICON icon = LoadIconW(inst, MAKEINTRESOURCEW(1));
        if (icon != NULL) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);
        }
    }

    {
        char base[1024];
        snprintf(base, sizeof(base), "%s/basemap.png", g_data_dir);
        gui_render_init(base);
    }
    {
        char err[256] = { 0 };
        if (gui_init(&g_gui, g_data_dir, err, sizeof(err)) != NAV_OK) {
            char msg[1280];
            snprintf(msg, sizeof(msg),
                     "数据目录：%.200s\n错误：%.200s\n\n请确认该目录下存在 spots.csv / roads.csv / "
                     "meta.json 与 basemap.png。",
                     g_data_dir, err);
            MessageBoxW(hwnd, W(msg), W("数据加载失败"), MB_OK | MB_ICONERROR);
            return 2;
        }
    }
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        g_gui.cw = rc.right;
        g_gui.ch = rc.bottom;
        gui_fit_view(&g_gui, rc.right, rc.bottom);
        if (open_id > 0 && campus_spot(&g_gui.campus, open_id) != NULL) {
            g_gui.selected = open_id;
            gui_center_on(&g_gui, open_id, 1.6);
            snprintf(g_gui.status, sizeof(g_gui.status), "已定位：%s（编号 %d）",
                     gui_spot_name(&g_gui, open_id), open_id);
        }
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    if (selftest) {
        SetTimer(hwnd, 1, 800, NULL);
    }

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    gui_free(&g_gui);
    gui_render_shutdown();
    GdiplusShutdown(token);
    return 0;
}
