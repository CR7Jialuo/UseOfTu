/* render.c —— GDI+ 绘制：底图、路网、路径、地点、侧栏与状态栏。
 *
 * 所有绘制都走 GpGraphics*，因此窗口（DC）和离屏位图（截图模式）共用同一套代码。
 */
#include "gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#include <gdiplus/gdiplus.h>

/* 布局常量（像素，96 DPI 基准） */


#define PAD 14
#define FONT_UI 15.0f
#define FONT_TITLE 19.0f
#define FONT_SMALL 13.0f

/* 配色：浅色地图风 */
#define C_BG        0xFFFFFFFF
#define C_NAVY      0xFF1F3A5F
#define C_BLUE      0xFF2563EB
#define C_GRAY      0xFF4B5563
#define C_LIGHT     0xFFEEF2F7
#define C_BORDER    0xFFD6DEE8
#define C_ROAD      0xFFC9D3DF
#define C_ROAD_MAIN 0xFFB3BECD
#define C_PATH      0xFFDC2626
#define C_START     0xFF16A34A
#define C_END       0xFFDC2626
#define C_POI       0xFF1F3A5F
#define C_HOVER     0xFFF59E0B
#define C_WHITE     0xFFFFFFFF
#define C_MUTED     0xFF6B7280

static GpImage *s_basemap = NULL;
static GpFontFamily *s_family = NULL;
static GpFont *s_font_ui = NULL;
static GpFont *s_font_title = NULL;
static GpFont *s_font_small = NULL;
static char s_basemap_path[512];

/* UTF-8 -> UTF-16，环形缓冲，供一次绘制内多处调用 */
#define WBUFS 48
static const WCHAR *W(const char *utf8)
{
    static WCHAR bufs[WBUFS][512];
    static int idx = 0;
    WCHAR *out = bufs[idx];
    int n;

    idx = (idx + 1) % WBUFS;
    n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, 512);
    if (n <= 0) {
        out[0] = 0;
    }
    return out;
}

int gui_render_init(const char *basemap_path)
{
    WCHAR wpath[512];

    if (s_family == NULL) {
        GdipCreateFontFamilyFromName(W("微软雅黑"), NULL, &s_family);
        if (s_family == NULL) {
            return -1;
        }
        GdipCreateFont(s_family, FONT_UI, FontStyleRegular, UnitPixel, &s_font_ui);
        GdipCreateFont(s_family, FONT_TITLE, FontStyleBold, UnitPixel, &s_font_title);
        GdipCreateFont(s_family, FONT_SMALL, FontStyleRegular, UnitPixel, &s_font_small);
    }
    if (s_basemap == NULL && basemap_path != NULL && basemap_path[0] != '\0') {
        MultiByteToWideChar(CP_UTF8, 0, basemap_path, -1, wpath, 512);
        if (GdipCreateBitmapFromFile(wpath, (GpBitmap **)&s_basemap) != Ok) {
            s_basemap = NULL;
        }
        snprintf(s_basemap_path, sizeof(s_basemap_path), "%s", basemap_path);
    }
    return 0;
}

void gui_render_shutdown(void)
{
    if (s_basemap != NULL) {
        GdipDisposeImage(s_basemap);
        s_basemap = NULL;
    }
    if (s_font_ui != NULL) {
        GdipDeleteFont(s_font_ui);
        s_font_ui = NULL;
    }
    if (s_font_title != NULL) {
        GdipDeleteFont(s_font_title);
        s_font_title = NULL;
    }
    if (s_font_small != NULL) {
        GdipDeleteFont(s_font_small);
        s_font_small = NULL;
    }
    if (s_family != NULL) {
        GdipDeleteFontFamily(s_family);
        s_family = NULL;
    }
}

/* ---------------- 基础绘制 ---------------- */

static void fill(GpGraphics *gr, int x, int y, int w, int h, ARGB color)
{
    GpSolidFill *b = NULL;
    GdipCreateSolidFill(color, &b);
    GdipFillRectangleI(gr, (GpBrush *)b, x, y, w, h);
    GdipDeleteBrush((GpBrush *)b);
}

static void stroke(GpGraphics *gr, int x0, int y0, int x1, int y1, ARGB color, float width)
{
    GpPen *pen = NULL;
    GdipCreatePen1(color, width, UnitPixel, &pen);
    GdipSetPenLineJoin(pen, LineJoinRound);
    GdipDrawLine(gr, pen, (REAL)x0, (REAL)y0, (REAL)x1, (REAL)y1);
    GdipDeletePen(pen);
}

static void polyline(GpGraphics *gr, const int *xs, const int *ys, int n, ARGB color, float width)
{
    GpPen *pen = NULL;
    Point *pts;
    int i;

    if (n < 2) {
        return;
    }
    pts = (Point *)malloc((size_t)n * sizeof(Point));
    if (pts == NULL) {
        return;
    }
    for (i = 0; i < n; ++i) {
        pts[i].X = xs[i];
        pts[i].Y = ys[i];
    }
    GdipCreatePen1(color, width, UnitPixel, &pen);
    GdipSetPenLineJoin(pen, LineJoinRound);
    GdipDrawLinesI(gr, pen, pts, n);
    GdipDeletePen(pen);
    free(pts);
}

static void text_at(GpGraphics *gr, GpFont *font, int x, int y, int w, int h, const char *s,
                    ARGB color)
{
    GpSolidFill *b = NULL;
    RectF r;

    r.X = (REAL)x;
    r.Y = (REAL)y;
    r.Width = (REAL)(w > 0 ? w : 600);
    r.Height = (REAL)(h > 0 ? h : 24);
    GdipCreateSolidFill(color, &b);
    GdipDrawString(gr, W(s), -1, font, &r, NULL, (GpBrush *)b);
    GdipDeleteBrush((GpBrush *)b);
}

static void dot(GpGraphics *gr, int cx, int cy, int r, ARGB fill_color, ARGB ring_color)
{
    GpSolidFill *b = NULL;
    GpSolidFill *ring = NULL;
    GdipCreateSolidFill(ring_color, &ring);
    GdipFillEllipseI(gr, (GpBrush *)ring, cx - r - 2, cy - r - 2, 2 * (r + 2), 2 * (r + 2));
    GdipDeleteBrush((GpBrush *)ring);
    GdipCreateSolidFill(fill_color, &b);
    GdipFillEllipseI(gr, (GpBrush *)b, cx - r, cy - r, 2 * r, 2 * r);
    GdipDeleteBrush((GpBrush *)b);
}

/* ---------------- 地图 ---------------- */

static void draw_basemap(Gui *g, GpGraphics *gr)
{
    double x0, y0, x1, y1;
    double sx0, sy0, sx1, sy1;
    int wpx = 0, hpx = 0;

    if (s_basemap == NULL) {
        fill(gr, 0, 0, g->cw, g->ch, 0xFFF3F6FA);
        return;
    }
    GdipGetImageWidth(s_basemap, (UINT *)&wpx);
    GdipGetImageHeight(s_basemap, (UINT *)&hpx);

    /* 底图覆盖的世界范围：像素(0,0)=世界(-ox*m, oy*m) */
    x0 = -g->ox * g->m_per_px;
    y1 = g->oy * g->m_per_px;
    x1 = x0 + wpx * g->m_per_px;
    y0 = y1 - hpx * g->m_per_px;

    gui_world_to_screen(g, x0, y1, &sx0, &sy0);
    gui_world_to_screen(g, x1, y0, &sx1, &sy1);
    GdipSetInterpolationMode(gr, InterpolationModeHighQualityBilinear);
    GdipDrawImageRectI(gr, s_basemap, (int)sx0, (int)sy0, (int)(sx1 - sx0), (int)(sy1 - sy0));
}

static void draw_roads(Gui *g, GpGraphics *gr)
{
    int i;
    int xs[GUI_MAX_SHAPE], ys[GUI_MAX_SHAPE];

    for (i = 0; i < g->campus.n_roads; ++i) {
        const Road *r = &g->campus.roads[i];
        const Shape *sh = gui_shape(g, r->from, r->to);
        ARGB color = (r->from > 20 && r->to > 20) ? C_ROAD : C_ROAD_MAIN;

        if (sh != NULL) {
            int k, n = sh->n;
            if (n > GUI_MAX_SHAPE) {
                n = GUI_MAX_SHAPE;
            }
            for (k = 0; k < n; ++k) {
                double px, py;
                gui_world_to_screen(g, sh->x[k], sh->y[k], &px, &py);
                xs[k] = (int)px;
                ys[k] = (int)py;
            }
            polyline(gr, xs, ys, n, color, 3.0f);
        } else {
            const Spot *a = campus_spot(&g->campus, r->from);
            const Spot *b = campus_spot(&g->campus, r->to);
            double ax, ay, bx, by;
            if (a == NULL || b == NULL) {
                continue;
            }
            gui_world_to_screen(g, a->x, a->y, &ax, &ay);
            gui_world_to_screen(g, b->x, b->y, &bx, &by);
            stroke(gr, (int)ax, (int)ay, (int)bx, (int)by, color, 3.0f);
        }
    }
}

static void draw_critical(Gui *g, GpGraphics *gr)
{
    int i;
    int xs[GUI_MAX_SHAPE], ys[GUI_MAX_SHAPE];

    if (!g->show_critical) {
        return;
    }
    for (i = 0; i < g->n_bridges; ++i) {
        const Road *r = campus_road(&g->campus, g->bridges[i]);
        const Shape *sh;
        int n = 0, k;
        int on_path = 0;
        if (r == NULL) {
            continue;
        }
        if (g->has_path) {
            int pi;
            for (pi = 0; pi + 1 < g->path.count; ++pi) {
                int a = g->path.nodes[pi], b = g->path.nodes[pi + 1];
                if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
                    on_path = 1;
                    break;
                }
            }
            if (!on_path) {
                continue;   /* 只标注本次路径上的桥 */
            }
        }
        sh = gui_shape(g, r->from, r->to);
        if (sh != NULL) {
            n = sh->n > GUI_MAX_SHAPE ? GUI_MAX_SHAPE : sh->n;
            for (k = 0; k < n; ++k) {
                double px, py;
                gui_world_to_screen(g, sh->x[k], sh->y[k], &px, &py);
                xs[k] = (int)px;
                ys[k] = (int)py;
            }
        } else {
            const Spot *a = campus_spot(&g->campus, r->from);
            const Spot *b = campus_spot(&g->campus, r->to);
            double ax, ay, bx, by;
            if (a == NULL || b == NULL) {
                continue;
            }
            gui_world_to_screen(g, a->x, a->y, &ax, &ay);
            gui_world_to_screen(g, b->x, b->y, &bx, &by);
            xs[0] = (int)ax; ys[0] = (int)ay;
            xs[1] = (int)bx; ys[1] = (int)by;
            n = 2;
        }
        if (n >= 2) {
            polyline(gr, xs, ys, n, 0xFFF59E0B, on_path ? 8.0f : 4.0f);
        }
    }
}

static void draw_path(Gui *g, GpGraphics *gr)
{
    int i;
    int xs[GUI_MAX_SHAPE], ys[GUI_MAX_SHAPE];

    if (!g->has_path || g->path.count < 2) {
        return;
    }
    for (i = 0; i + 1 < g->path.count; ++i) {
        int a = g->path.nodes[i];
        int b = g->path.nodes[i + 1];
        const Shape *sh = gui_shape(g, a, b);
        const Spot *sa = campus_spot(&g->campus, a);
        const Spot *sb = campus_spot(&g->campus, b);
        int n = 0, k;

        if (sh != NULL) {
            n = sh->n > GUI_MAX_SHAPE ? GUI_MAX_SHAPE : sh->n;
            if (sh->a != a) { /* 反序使用 */
                for (k = 0; k < n; ++k) {
                    double px, py;
                    gui_world_to_screen(g, sh->x[n - 1 - k], sh->y[n - 1 - k], &px, &py);
                    xs[k] = (int)px;
                    ys[k] = (int)py;
                }
            } else {
                for (k = 0; k < n; ++k) {
                    double px, py;
                    gui_world_to_screen(g, sh->x[k], sh->y[k], &px, &py);
                    xs[k] = (int)px;
                    ys[k] = (int)py;
                }
            }
        } else if (sa != NULL && sb != NULL) {
            double ax, ay, bx, by;
            gui_world_to_screen(g, sa->x, sa->y, &ax, &ay);
            gui_world_to_screen(g, sb->x, sb->y, &bx, &by);
            xs[0] = (int)ax;
            ys[0] = (int)ay;
            xs[1] = (int)bx;
            ys[1] = (int)by;
            n = 2;
        }
        if (n >= 2) {
            polyline(gr, xs, ys, n, 0xE6FFFFFF, 11.0f);
            polyline(gr, xs, ys, n, C_PATH, 6.0f);
        }
    }
}

static void draw_spots(Gui *g, GpGraphics *gr)
{
    int i;

    for (i = 0; i < g->campus.n_spots; ++i) {
        const Spot *s = &g->campus.spots[i];
        double px, py;
        int x, y, r = 6;
        ARGB color = C_POI;

        if (s->type == SPOT_JUNCTION) {
            continue;
        }
        gui_world_to_screen(g, s->x, s->y, &px, &py);
        x = (int)px;
        y = (int)py;
        if (s->id == g->from_id) {
            color = C_START;
            r = 9;
        } else if (s->id == g->to_id) {
            color = C_END;
            r = 9;
        } else if (s->id == g->selected) {
            color = C_BLUE;
            r = 8;
        } else if (s->id == g->hover) {
            color = C_HOVER;
            r = 8;
        }
        dot(gr, x, y, r, color, C_WHITE);
        if (s->id == g->from_id || s->id == g->to_id || s->id == g->selected ||
            s->id == g->hover || g->scale > 0.55) {
            text_at(gr, s_font_small, x + 12, y - 10, 220, 20, s->name, 0xFF111827);
        }
    }
}

static void draw_tooltip(Gui *g, GpGraphics *gr)
{
    const Spot *s;
    int x, y, w = 268, h = 86;
    char line1[128], line2[96];

    if (g->hover <= 0) {
        return;
    }
    s = campus_spot(&g->campus, g->hover);
    if (s == NULL) {
        return;
    }
    x = g->mouse_x + 18;
    y = g->mouse_y + 18;
    if (x + w > g->cw - GUI_PANEL_W - 8) {
        x = g->cw - GUI_PANEL_W - w - 8;
    }
    if (y + h > g->ch - GUI_STATUS_H - 8) {
        y = g->ch - GUI_STATUS_H - h - 8;
    }
    fill(gr, x, y, w, h, 0xF2FFFFFF);
    stroke(gr, x, y, x + w, y, C_BORDER, 1.0f);
    stroke(gr, x, y + h, x + w, y + h, C_BORDER, 1.0f);
    stroke(gr, x, y, x, y + h, C_BORDER, 1.0f);
    stroke(gr, x + w, y, x + w, y + h, C_BORDER, 1.0f);

    snprintf(line1, sizeof(line1), "%s（编号 %d）", s->name, s->id);
    snprintf(line2, sizeof(line2), "%s · 地图编号 %02d", spot_type_label(s->type), s->id);
    text_at(gr, s_font_ui, x + 12, y + 10, w - 24, 24, line1, C_NAVY);
    text_at(gr, s_font_small, x + 12, y + 36, w - 24, 20, line2, C_MUTED);
    text_at(gr, s_font_small, x + 12, y + 58, w - 24, 20, s->brief, 0xFF374151);
}

static void draw_scale_bar(Gui *g, GpGraphics *gr)
{
    double meters = 100.0;
    int px = (int)(meters * g->scale);
    int x = PAD, y = g->ch - GUI_STATUS_H - 26;

    if (px < 30) {
        px = (int)(200.0 * g->scale);
        meters = 200.0;
    }
    fill(gr, x, y, px, 5, 0xFF374151);
    fill(gr, x + px, y, px, 5, 0xFFFFFFFF);
    stroke(gr, x + px, y - 1, x + px, y + 6, 0xFF374151, 1.0f);
    text_at(gr, s_font_small, x, y - 22, 60, 18, "0", 0xFF374151);
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f m", meters);
        text_at(gr, s_font_small, x + 2 * px - 20, y - 22, 80, 18, buf, 0xFF374151);
    }
}

/* ---------------- 侧栏 ---------------- */

static void draw_panel(Gui *g, GpGraphics *gr)
{
    int x = g->cw - GUI_PANEL_W;
    int y = 0;
    int h = g->ch - GUI_STATUS_H;
    char buf[256];
    int i;
    char lines[32][200];
    int n_lines;

    fill(gr, x, y, GUI_PANEL_W, h, 0xFFF7F9FC);
    stroke(gr, x, y, x, h, C_BORDER, 1.0f);

    /* 标题 */
    fill(gr, x, 0, GUI_PANEL_W, 64, C_NAVY);
    text_at(gr, s_font_title, x + 18, 18, GUI_PANEL_W - 36, 30, "校园导航 · 苍梧校区", C_WHITE);
    text_at(gr, s_font_small, x + 18, 42, GUI_PANEL_W - 36, 18,
            "Win32 + GDI+ 图形界面", 0xFFC7D2E0);

    /* 起终点卡片 */
    fill(gr, x + 14, 78, GUI_PANEL_W - 28, 118, C_WHITE);
    stroke(gr, x + 14, 78, x + GUI_PANEL_W - 14, 78, C_BORDER, 1.0f);
    text_at(gr, s_font_small, x + 26, 86, 80, 18, "起点", C_MUTED);
    text_at(gr, s_font_ui, x + 70, 84, 200, 22,
            g->from_id > 0 ? gui_spot_name(g, g->from_id) : "（未设置）", C_NAVY);
    text_at(gr, s_font_small, x + 26, 114, 80, 18, "终点", C_MUTED);
    text_at(gr, s_font_ui, x + 70, 112, 200, 22,
            g->to_id > 0 ? gui_spot_name(g, g->to_id) : "（未设置）", C_NAVY);

    /* 口径 */
    text_at(gr, s_font_small, x + 26, 144, 60, 18, "口径", C_MUTED);
    fill(gr, x + 70, 140, 84, 26, g->by_time ? C_LIGHT : C_BLUE);
    text_at(gr, s_font_small, x + 84, 144, 60, 18, "距离", g->by_time ? C_GRAY : C_WHITE);
    fill(gr, x + 158, 140, 84, 26, g->by_time ? C_BLUE : C_LIGHT);
    text_at(gr, s_font_small, x + 172, 144, 60, 18, "时间", g->by_time ? C_WHITE : C_GRAY);

    /* 按钮 */
    fill(gr, x + 14, 208, GUI_PANEL_W - 28, 40, C_BLUE);
    text_at(gr, s_font_ui, x + 14, 217, GUI_PANEL_W - 28, 24, "规划路径（Enter）", C_WHITE);

    fill(gr, x + 14, 256, 130, 34, C_LIGHT);
    text_at(gr, s_font_small, x + 14, 263, 130, 20, "设为起点", C_NAVY);
    fill(gr, x + 162, 256, 130, 34, C_LIGHT);
    text_at(gr, s_font_small, x + 162, 263, 130, 20, "设为终点", C_NAVY);

    fill(gr, x + 14, 298, 130, 34, C_LIGHT);
    text_at(gr, s_font_small, x + 14, 305, 130, 20, "最近食堂", C_NAVY);
    fill(gr, x + 162, 298, 130, 34, C_LIGHT);
    text_at(gr, s_font_small, x + 162, 305, 130, 20, "最近校医院", C_NAVY);

    fill(gr, x + 14, 340, GUI_PANEL_W - 28, 32, g->show_critical ? 0xFFF59E0B : C_LIGHT);
    {
        char crit[80];
        if (g->show_critical) {
            snprintf(crit, sizeof(crit), "关键道路：已开（%d 条桥，点击关）", g->n_bridges);
        } else {
            snprintf(crit, sizeof(crit), "关键道路分析（桥/割点）");
        }
        text_at(gr, s_font_small, x + 26, 346, GUI_PANEL_W - 40, 20, crit,
                g->show_critical ? C_WHITE : C_NAVY);
    }

    /* 转向指引 */
    fill(gr, x + 14, 384, GUI_PANEL_W - 28, 1, C_BORDER);
    text_at(gr, s_font_ui, x + 16, 392, GUI_PANEL_W - 32, 22, "转向指引", C_NAVY);

    {
        int max_lines = (h - 420 - 34) / 22;
        if (max_lines < 1) {
            max_lines = 1;
        }
        if (max_lines > 32) {
            max_lines = 32;
        }
        n_lines = g->show_instructions ? gui_build_instructions(g, lines, max_lines) : 0;
    }
    if (n_lines == 0) {
        text_at(gr, s_font_small, x + 16, 420, GUI_PANEL_W - 32, 40,
                "选好起终点后点“规划路径”，这里会列出逐段指引。", C_MUTED);
    } else {
        int ty = 418;
        for (i = 0; i < n_lines && ty < h - 56; ++i) {
            text_at(gr, s_font_small, x + 16, ty, GUI_PANEL_W - 32, 20, lines[i], 0xFF374151);
            ty += 22;
        }
        if (n_lines >= (h - 420 - 34) / 22 && g->path.count > 3) {
            char more[64];
            snprintf(more, sizeof(more), "指引较多，已截断（共 %d 步）", g->path.count - 1);
            text_at(gr, s_font_small, x + 16, h - 48, GUI_PANEL_W - 32, 18, more, C_MUTED);
        }
    }

    /* 底部信息 */
    snprintf(buf, sizeof(buf), "%d 个地点 · %d 条道路", g->campus.n_spots, g->campus.n_roads);
    text_at(gr, s_font_small, x + 16, h - 26, GUI_PANEL_W - 32, 18, buf, C_MUTED);
}

static void draw_status_bar(Gui *g, GpGraphics *gr)
{
    int y = g->ch - GUI_STATUS_H;
    char buf[512];

    fill(gr, 0, y, g->cw, GUI_STATUS_H, 0xFFF1F4F8);
    stroke(gr, 0, y, g->cw, y, C_BORDER, 1.0f);

    if (g->has_path) {
        snprintf(buf, sizeof(buf), "%.1f 米 · 约 %.1f 分钟 · %d 个节点 · %.2f ms", gui_path_len(g),
                 gui_path_time(g), g->path.count, g->path_ms);
        text_at(gr, s_font_small, PAD, y + 8, 520, 20, buf, 0xFF111827);
    }
    text_at(gr, s_font_small, g->cw - 560, y + 8, 540, 20, g->status,
            g->status_error ? 0xFFB91C1C : C_MUTED);
}

void gui_paint(Gui *g, void *grp, int w, int h)
{
    GpGraphics *gr = (GpGraphics *)grp;
    g->cw = w;
    g->ch = h;
    GdipSetSmoothingMode(gr, SmoothingModeAntiAlias);
    GdipSetTextRenderingHint(gr, TextRenderingHintClearTypeGridFit);
    GdipSetPixelOffsetMode(gr, PixelOffsetModeHalf);

    fill(gr, 0, 0, w, h, C_BG);
    draw_basemap(g, gr);
    draw_roads(g, gr);
    draw_critical(g, gr);
    draw_path(g, gr);
    draw_spots(g, gr);
    draw_scale_bar(g, gr);
    draw_panel(g, gr);
    draw_tooltip(g, gr);
    draw_status_bar(g, gr);
}


/* 侧栏按钮命中测试（与 draw_panel 的排版保持一致） */
int gui_panel_hit(const Gui *g, int x, int y)
{
    int px = g->cw - GUI_PANEL_W;
    int lx = x - px;

    if (x < px) {
        return 0;
    }
    if (y >= 140 && y <= 166) {
        if (lx >= 70 && lx <= 154) return 6;      /* 距离口径 */
        if (lx >= 158 && lx <= 242) return 7;     /* 时间口径 */
    }
    if (lx >= 14 && lx <= GUI_PANEL_W - 14) {
        if (y >= 208 && y <= 248) return 1;       /* 规划 */
        if (y >= 256 && y <= 290) {
            return (lx <= 144) ? 2 : 3;           /* 设为起点 / 终点 */
        }
        if (y >= 298 && y <= 332) {
            return (lx <= 144) ? 4 : 5;           /* 最近食堂 / 校医院 */
        }
        if (y >= 340 && y <= 372) {
            return 8;                             /* 关键道路 */
        }
    }
    return 0;
}
