/* gui.c —— 数据加载、视图变换、命中测试、路径规划与转向指引。 */
#include "gui.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "err.h"
#include "platform.h"

#define GUI_PI 3.14159265358979323846

static double now_ms(void)
{
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}

const char *gui_spot_name(const Gui *g, int id)
{
    const Spot *s = campus_spot(&g->campus, id);
    return s ? s->name : "?";
}

/* 从 meta.json 里取 m_per_px 与 px_origin（只认这两个键，避免引入 JSON 库） */
static int load_meta(Gui *g, const char *path, char *err, size_t cap)
{
    FILE *fp = platform_fopen(path, "rb");
    char buf[4096];
    size_t n;
    char *p;

    if (fp == NULL) {
        nav_err_set(err, cap, "缺少 meta.json：%s", path);
        return NAV_ERR_IO;
    }
    n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    p = strstr(buf, "\"m_per_px\"");
    if (p != NULL) {
        p = strchr(p, ':');
        if (p != NULL) {
            g->m_per_px = strtod(p + 1, NULL);
        }
    }
    p = strstr(buf, "\"px_origin\"");
    if (p != NULL) {
        p = strchr(p, '[');
        if (p != NULL) {
            g->ox = strtod(p + 1, &p);
            if (p != NULL && *p == ',') {
                g->oy = strtod(p + 1, NULL);
            }
        }
    }
    if (!(g->m_per_px > 0.0)) {
        nav_err_set(err, cap, "meta.json 里的 m_per_px 无效");
        return NAV_ERR_FMT;
    }
    return NAV_OK;
}

/* shapes.csv: 起点,终点,x1 y1;x2 y2;... */
static int load_shapes(Gui *g, const char *path)
{
    FILE *fp = platform_fopen(path, "rb");
    char line[8192];
    int cap = 512;

    if (fp == NULL) {
        return NAV_ERR_IO; /* 没有形状文件也能跑，退化为直线 */
    }
    g->shapes = (Shape *)calloc((size_t)cap, sizeof(Shape));
    if (g->shapes == NULL) {
        fclose(fp);
        return NAV_ERR_NOMEM;
    }
    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *p = line;
        char *end;
        Shape *sh;
        long a, b;
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        a = strtol(p, &end, 10);
        if (end == p || *end != ',') {
            continue;
        }
        p = end + 1;
        b = strtol(p, &end, 10);
        if (end == p || *end != ',') {
            continue;
        }
        p = end + 1;
        if (g->n_shapes >= cap) {
            break;
        }
        sh = &g->shapes[g->n_shapes];
        sh->a = (int)a;
        sh->b = (int)b;
        sh->n = 0;
        while (*p != '\0' && sh->n < GUI_MAX_SHAPE) {
            char *q;
            double x, y;
            x = strtod(p, &q);
            if (q == p) {
                break;
            }
            p = q;
            y = strtod(p, &q);
            if (q == p) {
                break;
            }
            p = q;
            while (*p == ' ') {
                ++p;
            }
            if (*p == ';') {
                ++p;
            }
            sh->x[sh->n] = x;
            sh->y[sh->n] = y;
            ++sh->n;
        }
        if (sh->n >= 2) {
            ++g->n_shapes;
        }
    }
    fclose(fp);
    return NAV_OK;
}

const Shape *gui_shape(const Gui *g, int a, int b)
{
    int i;
    for (i = 0; i < g->n_shapes; ++i) {
        const Shape *s = &g->shapes[i];
        if ((s->a == a && s->b == b) || (s->a == b && s->b == a)) {
            return s;
        }
    }
    return NULL;
}

int gui_init(Gui *g, const char *data_dir, char *err, size_t cap)
{
    char sp[512], rd[512], meta[512], shapes[512];
    int rc;

    memset(g, 0, sizeof(*g));
    snprintf(sp, sizeof(sp), "%s/spots.csv", data_dir);
    snprintf(rd, sizeof(rd), "%s/roads.csv", data_dir);
    snprintf(meta, sizeof(meta), "%s/meta.json", data_dir);
    snprintf(shapes, sizeof(shapes), "%s/shapes.csv", data_dir);

    rc = campus_load_csv(&g->campus, sp, rd, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    if (load_meta(g, meta, err, cap) != NAV_OK) {
        return NAV_ERR_FMT;
    }
    (void)load_shapes(g, shapes); /* 可选 */

    rc = graph_build(&g->campus, g->by_time, &g->graph, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    g->from_id = 0;
    g->to_id = 0;
    g->scale = 1.0;
    g->show_instructions = 1;
    strcpy(g->status, "就绪：单击地点选中，再点“设为起点/终点”，然后“规划路径”");
    return NAV_OK;
}

void gui_free(Gui *g)
{
    graph_free(&g->graph);
    free(g->shapes);
    g->shapes = NULL;
    g->n_shapes = 0;
}

void gui_world_to_screen(const Gui *g, double x, double y, double *sx, double *sy)
{
    *sx = (x - g->view_x0) * g->scale;
    *sy = (g->view_y1 - y) * g->scale;
}

void gui_screen_to_world(const Gui *g, double sx, double sy, double *x, double *y)
{
    *x = g->view_x0 + sx / g->scale;
    *y = g->view_y1 - sy / g->scale;
}

void gui_fit_view(Gui *g, int cw, int ch)
{
    double minx = 1e18, maxx = -1e18, miny = 1e18, maxy = -1e18;
    double w, h, margin = 40.0;
    int i;

    g->cw = cw;
    g->ch = ch;
    for (i = 0; i < g->campus.n_spots; ++i) {
        const Spot *s = &g->campus.spots[i];
        if (s->type == SPOT_JUNCTION) {
            continue;
        }
        if (s->x < minx) {
            minx = s->x;
        }
        if (s->x > maxx) {
            maxx = s->x;
        }
        if (s->y < miny) {
            miny = s->y;
        }
        if (s->y > maxy) {
            maxy = s->y;
        }
    }
    if (maxx <= minx || maxy <= miny) {
        return;
    }
    w = maxx - minx;
    h = maxy - miny;
    g->scale = (cw - 2 * margin) / w;
    if ((ch - 2 * margin) / h < g->scale) {
        g->scale = (ch - 2 * margin) / h;
    }
    if (g->scale <= 0.0) {
        g->scale = 1.0;
    }
    g->view_x0 = minx - (cw / g->scale - w) / 2.0;
    g->view_y1 = maxy + (ch / g->scale - h) / 2.0;
}

void gui_zoom_at(Gui *g, int sx, int sy, double factor)
{
    double wx, wy, new_scale;

    gui_screen_to_world(g, (double)sx, (double)sy, &wx, &wy);
    new_scale = g->scale * factor;
    if (new_scale < 0.05) {
        new_scale = 0.05;
    }
    if (new_scale > 12.0) {
        new_scale = 12.0;
    }
    g->scale = new_scale;
    g->view_x0 = wx - (double)sx / g->scale;
    g->view_y1 = wy + (double)sy / g->scale;
}

void gui_pan(Gui *g, int dx, int dy)
{
    g->view_x0 -= (double)dx / g->scale;
    g->view_y1 += (double)dy / g->scale;
}

int gui_hit_test(const Gui *g, int sx, int sy)
{
    int i, best = 0;
    double best_d = 16.0 * 16.0;

    for (i = 0; i < g->campus.n_spots; ++i) {
        const Spot *s = &g->campus.spots[i];
        double px, py, d;
        if (s->type == SPOT_JUNCTION) {
            continue;
        }
        gui_world_to_screen(g, s->x, s->y, &px, &py);
        d = (px - sx) * (px - sx) + (py - sy) * (py - sy);
        if (d < best_d) {
            best_d = d;
            best = s->id;
        }
    }
    return best;
}

void gui_center_on(Gui *g, int spot_id, double zoom)
{
    const Spot *s = campus_spot(&g->campus, spot_id);
    if (s == NULL) {
        return;
    }
    if (zoom > 0.0) {
        g->scale = zoom;
    }
    g->view_x0 = s->x - (double)g->cw / (2.0 * g->scale);
    g->view_y1 = s->y + (double)g->ch / (2.0 * g->scale);
}

int gui_plan(Gui *g)
{
    char err[256];
    double t0;
    int rc;

    g->has_path = 0;
    if (g->from_id <= 0 || g->to_id <= 0) {
        snprintf(g->status, sizeof(g->status), "请先设置起点和终点");
        g->status_error = 1;
        return NAV_ERR_ARG;
    }
    t0 = now_ms();
    rc = algo_dijkstra(&g->graph, g->from_id, g->to_id, &g->path, err, sizeof(err));
    g->path_ms = now_ms() - t0;
    if (rc != NAV_OK) {
        snprintf(g->status, sizeof(g->status), "规划失败：%.180s", err);
        g->status_error = 1;
        return rc;
    }
    if (g->path.count == 0) {
        snprintf(g->status, sizeof(g->status), "两点之间没有通路");
        g->status_error = 1;
        return NAV_NOPATH;
    }
    g->has_path = 1;
    g->anim_playing = 1;
    g->anim_segments = 0;
    g->status_error = 0;
    snprintf(g->status, sizeof(g->status), "%s → %s：%.1f 米，约 %.1f 分钟（%.2f ms）",
             gui_spot_name(g, g->from_id), gui_spot_name(g, g->to_id),
             gui_path_len(g), gui_path_time(g), g->path_ms);
    return NAV_OK;
}

/* ---------------- 路径统计与转向指引 ---------------- */

static const Road *gui_road_between(const Gui *g, int a, int b)
{
    int i;
    for (i = 0; i < g->campus.n_roads; ++i) {
        const Road *r = &g->campus.roads[i];
        if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
            return r;
        }
    }
    return NULL;
}

double gui_path_len(const Gui *g)
{
    double total = 0.0;
    int i;
    for (i = 0; i + 1 < g->path.count; ++i) {
        const Road *r = gui_road_between(g, g->path.nodes[i], g->path.nodes[i + 1]);
        if (r != NULL) {
            total += r->len_m;
        }
    }
    return total;
}

double gui_path_time(const Gui *g)
{
    double total = 0.0;
    int i;
    for (i = 0; i + 1 < g->path.count; ++i) {
        const Road *r = gui_road_between(g, g->path.nodes[i], g->path.nodes[i + 1]);
        if (r != NULL) {
            total += r->walk_min;
        }
    }
    return total;
}

static double bearing_of(const Gui *g, int a, int b)
{
    const Spot *sa = campus_spot(&g->campus, a);
    const Spot *sb = campus_spot(&g->campus, b);
    double ang;
    if (sa == NULL || sb == NULL) {
        return 0.0;
    }
    ang = atan2(sb->x - sa->x, sb->y - sa->y) * 180.0 / GUI_PI;
    return ang < 0.0 ? ang + 360.0 : ang;
}

static double angle_delta(double from, double to)
{
    double d = to - from;
    while (d > 180.0) {
        d -= 360.0;
    }
    while (d < -180.0) {
        d += 360.0;
    }
    return d;
}

static const char *compass_of(double b)
{
    static const char *names[8] = { "北", "东北", "东", "东南", "南", "西南", "西", "西北" };
    return names[(int)((b + 22.5) / 45.0) % 8];
}

static const char *turn_of(double d)
{
    double a = fabs(d);
    if (a < 25.0) {
        return "直行";
    }
    if (a < 60.0) {
        return d > 0 ? "稍向右" : "稍向左";
    }
    if (a < 135.0) {
        return d > 0 ? "右转" : "左转";
    }
    return "掉头";
}

static const char *road_name(const Gui *g, int a, int b)
{
    const Road *r = gui_road_between(g, a, b);
    if (r == NULL || r->name[0] == '\0') {
        return "校内道路";
    }
    if (strcmp(r->name, "连接步道") == 0) {
        return "步行连接道";
    }
    return r->name;
}

int gui_build_instructions(const Gui *g, char lines[][200], int max_lines)
{
    int n = g->path.count;
    int s, e, count = 0;
    double prev_b = 0.0;
    int first = 1;

    if (!g->has_path || n < 2 || max_lines <= 0) {
        return 0;
    }
    s = 0;
    while (s < n - 1 && count < max_lines) {
        double len = 0.0;
        double b0 = bearing_of(g, g->path.nodes[s], g->path.nodes[s + 1]);
        const char *road = road_name(g, g->path.nodes[s], g->path.nodes[s + 1]);
        const Road *r0 = gui_road_between(g, g->path.nodes[s], g->path.nodes[s + 1]);

        if (r0 != NULL) {
            len += r0->len_m;
        }
        e = s;
        while (e + 2 < n) {
            const char *r2 = road_name(g, g->path.nodes[e + 1], g->path.nodes[e + 2]);
            double b2 = bearing_of(g, g->path.nodes[e + 1], g->path.nodes[e + 2]);
            const Road *re = gui_road_between(g, g->path.nodes[e + 1], g->path.nodes[e + 2]);
            if (strcmp(r2, road) == 0 && fabs(angle_delta(b0, b2)) < 25.0) {
                if (re != NULL) {
                    len += re->len_m;
                }
                ++e;
            } else {
                break;
            }
        }
        if (first) {
            snprintf(lines[count++], 200, "出发：沿%s向%s行进 %.0f 米", road, compass_of(b0), len);
            first = 0;
        } else {
            double d = angle_delta(prev_b, b0);
            if (strcmp(turn_of(d), "直行") == 0) {
                snprintf(lines[count++], 200, "继续沿%s向%s行进 %.0f 米", road, compass_of(b0), len);
            } else {
                snprintf(lines[count++], 200, "在路口%s，进入%s向%s行进 %.0f 米", turn_of(d), road,
                         compass_of(b0), len);
            }
        }
        prev_b = bearing_of(g, g->path.nodes[e], g->path.nodes[e + 1]);
        s = e + 1;
    }
    if (count < max_lines) {
        snprintf(lines[count++], 200, "到达：%s", gui_spot_name(g, g->path.nodes[n - 1]));
    }
    return count;
}

int gui_nearest(Gui *g, SpotType type)
{
    char err[256];
    int best = 0;

    if (g->from_id <= 0) {
        snprintf(g->status, sizeof(g->status), "请先设置起点");
        g->status_error = 1;
        return NAV_ERR_ARG;
    }
    if (algo_nearest_type(&g->graph, &g->campus, g->from_id, type, &best, &g->path, err,
                          sizeof(err)) != NAV_OK) {
        snprintf(g->status, sizeof(g->status), "查询失败：%.180s", err);
        g->status_error = 1;
        return NAV_ERR_ARG;
    }
    g->to_id = best;
    g->has_path = 1;
    g->anim_playing = 1;
    g->anim_segments = 0;
    g->status_error = 0;
    snprintf(g->status, sizeof(g->status), "最近的%s：%s（%.1f 米）", spot_type_label(type),
             gui_spot_name(g, best), gui_path_len(g));
    return NAV_OK;
}


int gui_toggle_critical(Gui *g)
{
    char err[256];
    int is_ap[NAV_MAX_SPOTS + 1];

    if (g->show_critical) {
        g->show_critical = 0;
        g->n_bridges = 0;
        snprintf(g->status, sizeof(g->status), "已关闭关键道路高亮");
        return 0;
    }
    if (algo_critical(&g->graph, is_ap, g->bridges, &g->n_bridges, err, sizeof(err)) != NAV_OK) {
        snprintf(g->status, sizeof(g->status), "关键道路分析失败：%.180s", err);
        g->status_error = 1;
        return NAV_ERR_ARG;
    }
    g->show_critical = 1;
    {
        int on_path = 0, i;
        for (i = 0; i < g->n_bridges; ++i) {
            const Road *r = campus_road(&g->campus, g->bridges[i]);
            int pi;
            if (r == NULL) {
                continue;
            }
            for (pi = 0; pi + 1 < g->path.count; ++pi) {
                int a = g->path.nodes[pi], b = g->path.nodes[pi + 1];
                if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
                    ++on_path;
                    break;
                }
            }
        }
        if (g->has_path) {
            snprintf(g->status, sizeof(g->status),
                     "关键道路：本路径经过 %d 条桥（路网共 %d 条）", on_path, g->n_bridges);
        } else {
            snprintf(g->status, sizeof(g->status), "关键道路：路网共 %d 条桥，先规划路径可只看路径上的桥",
                     g->n_bridges);
        }
    }
    g->status_error = 0;
    return 0;
}


void gui_toggle_via(Gui *g, int spot_id)
{
    int i;

    for (i = 0; i < g->n_via; ++i) {
        if (g->via[i] == spot_id) {
            int k;
            for (k = i; k + 1 < g->n_via; ++k) {
                g->via[k] = g->via[k + 1];
            }
            --g->n_via;
            snprintf(g->status, sizeof(g->status), "已移除途经点：%s（还剩 %d 个）",
                     gui_spot_name(g, spot_id), g->n_via);
            g->status_error = 0;
            return;
        }
    }
    if (g->n_via >= 8) {
        snprintf(g->status, sizeof(g->status), "最多支持 8 个途经点");
        g->status_error = 1;
        return;
    }
    g->via[g->n_via++] = spot_id;
    snprintf(g->status, sizeof(g->status), "已添加途经点：%s（共 %d 个）",
             gui_spot_name(g, spot_id), g->n_via);
    g->status_error = 0;
}

int gui_tour(Gui *g)
{
    char err[256];
    double t0;
    int rc;
    int order[16];

    if (g->from_id <= 0 || g->to_id <= 0) {
        snprintf(g->status, sizeof(g->status), "巡游需要先设置起点与终点");
        g->status_error = 1;
        return NAV_ERR_ARG;
    }
    t0 = now_ms();
    rc = algo_tsp(&g->graph, g->from_id, g->via, g->n_via, g->to_id, order, &g->path, err,
                  sizeof(err));
    g->path_ms = now_ms() - t0;
    if (rc != NAV_OK) {
        snprintf(g->status, sizeof(g->status), "巡游规划失败：%.180s", err);
        g->status_error = 1;
        return rc;
    }
    g->has_path = 1;
    g->anim_playing = 1;
    g->anim_segments = 0;
    g->status_error = 0;
    snprintf(g->status, sizeof(g->status), "巡游：%s → %d 个途经点 → %s，%.1f 米（%.2f ms）",
             gui_spot_name(g, g->from_id), g->n_via, gui_spot_name(g, g->to_id), gui_path_len(g),
             g->path_ms);
    return NAV_OK;
}
