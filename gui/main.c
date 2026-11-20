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
    ACT_ABOUT
};

static void do_action(HWND hwnd, int act)
{
    switch (act) {
    case ACT_PLAN:
        gui_plan(&g_gui);
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
        g_gui.hover = gui_hit_test(&g_gui, x, y);
        if (g_gui.hover != old_hover) {
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
        }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == 0x9000) {
            DestroyWindow(hwnd);
        } else {
            do_action(hwnd, (int)LOWORD(wp));
        }
        return 0;
    case WM_TIMER:
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

static HMENU build_menu(void)
{
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU path = CreatePopupMenu();
    HMENU help = CreatePopupMenu();

    AppendMenuW(file, MF_STRING, ACT_FIT, W("适应窗口(&F)\tF5"));
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, 0x9000, W("退出(&X)"));
    AppendMenuW(view, MF_STRING, ACT_NEAR_CANTEEN, W("最近食堂"));
    AppendMenuW(view, MF_STRING, ACT_NEAR_MEDICAL, W("最近校医院"));
    AppendMenuW(path, MF_STRING, ACT_PLAN, W("规划路径(&R)\tEnter"));
    AppendMenuW(path, MF_STRING, ACT_SWAP, W("交换起终点"));
    AppendMenuW(path, MF_STRING, ACT_CLEAR, W("清除路径\tEsc"));
    AppendMenuW(help, MF_STRING, ACT_ABOUT, W("关于(&A)"));

    AppendMenuW(bar, MF_POPUP, (UINT_PTR)file, W("文件(&F)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)view, W("查询(&Q)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)path, W("路径(&P)"));
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)help, W("帮助(&H)"));
    return bar;
}

/* ---------------- 截图模式 ---------------- */

static int run_screenshot(int argc, char **argv, const char *out, int route_from, int route_to)
{
    GpBitmap *bmp = NULL;
    GpGraphics *gr = NULL;
    int w = 1400, h = 900;
    int i;

    (void)argc;
    (void)argv;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            int tw = 0, th = 0;
            if (sscanf(argv[++i], "%dx%d", &tw, &th) == 2) {
                w = tw;
                h = th;
            }
        }
    }
    if (gui_init(&g_gui, g_data_dir, (char[256]){0}, 256) != NAV_OK) {
        fprintf(stderr, "init failed\n");
        return 2;
    }
    gui_fit_view(&g_gui, w, h);
    if (route_from > 0 && route_to > 0) {
        g_gui.from_id = route_from;
        g_gui.to_id = route_to;
        gui_plan(&g_gui);
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
    int selftest = 0;
    int i;

    (void)prev;
    (void)show;
    (void)cmdline;

    {
        int argc = 0;
        char **argv = platform_utf8_args(&argc);
        strcpy(g_data_dir, "data/campus_real");
        for (i = 1; i < argc; ++i) {
            if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) {
                strcpy(g_data_dir, argv[++i]);
            } else if (strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
                shot = argv[++i];
            } else if (strcmp(argv[i], "--selftest") == 0) {
                selftest = 1;
            } else if (strcmp(argv[i], "--route") == 0 && i + 2 < argc) {
                route_from = atoi(argv[++i]);
                route_to = atoi(argv[++i]);
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
        char base[512];
        snprintf(base, sizeof(base), "%s/basemap.png", g_data_dir);
        gui_render_init(base);
        rc = run_screenshot(0, NULL, shot, route_from, route_to);
        gui_render_shutdown();
        GdiplusShutdown(token);
        return rc;
    }

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"CampusNavGuiClass";
    if (!RegisterClassExW(&wc)) {
        return 1;
    }
    hwnd = CreateWindowExW(0, wc.lpszClassName, W("校园导航系统"),
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1440, 940, NULL,
                           build_menu(), inst, NULL);
    if (hwnd == NULL) {
        return 1;
    }

    {
        char base[512];
        snprintf(base, sizeof(base), "%s/basemap.png", g_data_dir);
        gui_render_init(base);
    }
    if (gui_init(&g_gui, g_data_dir, (char[256]){0}, 256) != NAV_OK) {
        MessageBoxW(hwnd, W("数据加载失败：请检查 data 目录"),
                    W("错误"), MB_OK | MB_ICONERROR);
        return 2;
    }
    {
        RECT rc;
        GetClientRect(hwnd, &rc);
        gui_fit_view(&g_gui, rc.right, rc.bottom);
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
