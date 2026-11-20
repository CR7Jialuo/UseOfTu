/* ui.c —— 控制台界面实现。 */
#include "ui.h"
#include "bench.h"
#include "err.h"
#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAP_COLS 68
#define MAP_ROWS 30

static const Road *find_road(const Campus *c, int a, int b)
{
    int i;
    if (c == NULL) {
        return NULL;
    }
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
            return r;
        }
    }
    return NULL;
}

double ui_path_length_m(const Campus *c, const Path *p)
{
    double total = 0.0;
    int i;
    if (c == NULL || p == NULL) {
        return 0.0;
    }
    for (i = 0; i + 1 < p->count; ++i) {
        const Road *r = find_road(c, p->nodes[i], p->nodes[i + 1]);
        if (r != NULL) {
            total += r->len_m;
        }
    }
    return total;
}

double ui_path_time_min(const Campus *c, const Path *p)
{
    double total = 0.0;
    int i;
    if (c == NULL || p == NULL) {
        return 0.0;
    }
    for (i = 0; i + 1 < p->count; ++i) {
        const Road *r = find_road(c, p->nodes[i], p->nodes[i + 1]);
        if (r != NULL) {
            total += r->walk_min;
        }
    }
    return total;
}

static void hr(char ch, int n)
{
    int i;
    for (i = 0; i < n; ++i) {
        fputc(ch, stdout);
    }
    fputc('\n', stdout);
}

void ui_banner(const App *app)
{
    const Campus *c = &app->campus;
    printf("\n");
    hr('=', 66);
    printf("  %s 校园导航系统\n", c->title);
    printf("  图的最短路径 · 数据结构课程设计\n");
    hr('=', 66);
    printf("  地点 %d 个    道路 %d 条    当前口径：%s\n", c->n_spots, c->n_roads,
           app->by_time ? "步行时间" : "距离");
    printf("  数据目录：%s\n", app->data_dir[0] ? app->data_dir : "(内置默认数据)");
    printf("  路网总长：%.0f 米   路网连通：%s\n", campus_total_road_length(c),
           campus_is_connected(c) ? "是" : "否");
    hr('=', 66);
}

void ui_print_spots(const Campus *c)
{
    int i, j;
    int shown = 0;
    if (c == NULL) {
        return;
    }
    printf("\n");
    printf("  ");
    nav_print_padded("编号", 6);
    nav_print_padded("名称", 18);
    nav_print_padded("类型", 12);
    nav_print_padded("坐标(米)", 16);
    printf("连接道路\n");
    hr('-', 66);
    for (i = 0; i < c->n_spots; ++i) {
        const Spot *s = &c->spots[i];
        int degree = 0;
        char coord[32];
        char idbuf[16];
        if (s->type == SPOT_JUNCTION) {
            continue; /* 路口只参与最短路，不在地点列表中显示 */
        }
        for (j = 0; j < c->n_roads; ++j) {
            if (c->roads[j].from == s->id || c->roads[j].to == s->id) {
                ++degree;
            }
        }
        snprintf(coord, sizeof(coord), "(%.0f, %.0f)", s->x, s->y);
        snprintf(idbuf, sizeof(idbuf), "%d", s->id);
        printf("  ");
        nav_print_padded(idbuf, 6);
        nav_print_padded(s->name, 18);
        nav_print_padded(spot_type_label(s->type), 12);
        nav_print_padded(coord, 16);
        printf("%d\n", degree);
        ++shown;
    }
    hr('-', 66);
    printf("  共 %d 个地点（另有 %d 个路网路口参与寻路，未列出）。\n", shown,
           c->n_spots - shown);
}

void ui_print_spot_detail(const Campus *c, int id)
{
    const Spot *s = campus_spot(c, id);
    int i;

    if (s == NULL) {
        printf("  地点 %d 不存在。\n", id);
        return;
    }
    printf("\n  [%d] %s（%s）\n", s->id, s->name, spot_type_label(s->type));
    printf("  坐标：%.1f, %.1f 米\n", s->x, s->y);
    printf("  简介：%s\n", s->brief[0] ? s->brief : "（无）");
    printf("  相邻道路：\n");
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        int other;
        if (r->from != s->id && r->to != s->id) {
            continue;
        }
        other = (r->from == s->id) ? r->to : r->from;
        printf("    %2d. %s — %.1f 米 / %.1f 分钟%s\n", r->id, c->spots[other - 1].name,
               r->len_m, r->walk_min, r->open ? "" : "（封闭）");
    }
}

void ui_print_roads(const Campus *c)
{
    int i;
    printf("\n");
    printf("  %-4s %-22s %-22s %10s %10s %s\n", "编号", "起点", "终点", "长度(米)", "时间(分)", "状态");
    hr('-', 80);
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        printf("  %-4d ", r->id);
        nav_print_padded(c->spots[r->from - 1].name, 22);
        nav_print_padded(c->spots[r->to - 1].name, 22);
        printf("%10.1f %10.1f %s\n", r->len_m, r->walk_min, r->open ? "通行" : "封闭");
    }
    hr('-', 80);
}

static char spot_symbol(int id)
{
    if (id >= 1 && id <= 9) {
        return (char)('0' + id);
    }
    if (id >= 10 && id <= 35) {
        return (char)('A' + (id - 10));
    }
    if (id >= 36 && id <= 61) {
        return (char)('a' + (id - 36));
    }
    return '#';
}

static void draw_line(char grid[MAP_ROWS][MAP_COLS + 1], int x0, int y0, int x1, int y1, char ch)
{
    int dx = abs(x1 - x0);
    int dy = -abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        if (y0 >= 0 && y0 < MAP_ROWS && x0 >= 0 && x0 < MAP_COLS) {
            grid[y0][x0] = ch;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

void ui_ascii_map(const App *app)
{
    const Campus *c = &app->campus;
    static char grid[MAP_ROWS][MAP_COLS + 1];
    double m_per_col, m_per_row;
    int i, r, col, closed = 0, dense = 0;

    if (c == NULL || c->n_spots < 1) {
        return;
    }
    m_per_col = c->width_m / (double)(MAP_COLS - 1);
    {
        double need_row = c->height_m / (double)((MAP_ROWS - 1) * 2);
        if (need_row > m_per_col) {
            m_per_col = need_row;
        }
    }
    m_per_row = m_per_col * 2.0;

    for (r = 0; r < MAP_ROWS; ++r) {
        for (col = 0; col < MAP_COLS; ++col) {
            grid[r][col] = ' ';
        }
        grid[r][MAP_COLS] = '\0';
    }

    /* 路网密集时（真实校区）ASCII 图只画地点，避免糊成一片 */
    dense = c->n_roads > 80;
    if (!dense) {
        for (i = 0; i < c->n_roads; ++i) {
            const Road *rd = &c->roads[i];
            int x0 = (int)floor(c->spots[rd->from - 1].x / m_per_col + 0.5);
            int y0 = MAP_ROWS - 1 - (int)floor(c->spots[rd->from - 1].y / m_per_row + 0.5);
            int x1 = (int)floor(c->spots[rd->to - 1].x / m_per_col + 0.5);
            int y1 = MAP_ROWS - 1 - (int)floor(c->spots[rd->to - 1].y / m_per_row + 0.5);
            draw_line(grid, x0, y0, x1, y1, rd->open ? '*' : '+');
        }
    }
    for (i = 0; i < c->n_roads; ++i) {
        if (!c->roads[i].open) {
            ++closed;
        }
    }
    for (i = 0; i < c->n_spots; ++i) {
        const Spot *s = &c->spots[i];
        int x, y;
        if (s->type == SPOT_JUNCTION) {
            continue;
        }
        x = (int)floor(s->x / m_per_col + 0.5);
        y = MAP_ROWS - 1 - (int)floor(s->y / m_per_row + 0.5);
        if (x < 0) {
            x = 0;
        }
        if (x >= MAP_COLS) {
            x = MAP_COLS - 1;
        }
        if (y < 0) {
            y = 0;
        }
        if (y >= MAP_ROWS) {
            y = MAP_ROWS - 1;
        }
        grid[y][x] = spot_symbol(s->id);
    }

    printf("\n  %s · 校园示意地图\n", c->title);
    hr('-', MAP_COLS + 4);
    for (r = 0; r < MAP_ROWS; ++r) {
        printf("  |%s|\n", grid[r]);
    }
    hr('-', MAP_COLS + 4);
    printf("  图例：* 道路   + 封闭道路   数字/字母 = 地点编号\n");
    printf("  比例：1 列 ≈ %.0f 米，1 行 ≈ %.0f 米（示意图，非实测比例尺）\n", m_per_col,
           m_per_row);
    if (closed > 0) {
        printf("  注意：当前有 %d 条道路处于封闭状态，导航已自动绕行。\n", closed);
    }

    /* 图例分三栏打印。 */
    printf("\n");
    for (i = 0; i < APP_PATH_MAX; ++i) {
        break;
    }
    {
        int rows = (c->n_spots + 2) / 3;
        int k;
        for (k = 0; k < rows; ++k) {
            int j;
            printf("  ");
            for (j = 0; j < 3; ++j) {
                int idx = k + j * rows;
                if (idx < c->n_spots) {
                    char item[64];
                    snprintf(item, sizeof(item), "%c %s", spot_symbol(c->spots[idx].id),
                             c->spots[idx].name);
                    nav_print_padded(item, 24);
                }
            }
            printf("\n");
        }
    }
}

static double seg_len_m(const Campus *c, int a, int b)
{
    const Road *r = find_road(c, a, b);
    return r ? r->len_m : 0.0;
}

/* 只打印“地点”，中间路口折叠成“经 N 个路口 / X 米” */
static void print_path_nodes(const Campus *c, const Path *p)
{
    int i;
    int pend_n = 0;
    double pend_m = 0.0;
    int printed = 0;

    for (i = 0; i < p->count; ++i) {
        const Spot *s = &c->spots[p->nodes[i] - 1];
        if (i > 0) {
            pend_m += seg_len_m(c, p->nodes[i - 1], p->nodes[i]);
        }
        if (s->type == SPOT_JUNCTION) {
            ++pend_n;
            continue;
        }
        if (printed > 0) {
            if (pend_n > 0) {
                printf(" —[经 %d 个路口 %.0f 米]→ ", pend_n, pend_m);
            } else {
                printf(" → ");
            }
            if (printed % 2 == 0) {
                printf("\n                ");
            }
        }
        printf("%s(%d)", s->name, s->id);
        pend_n = 0;
        pend_m = 0.0;
        ++printed;
    }
    if (pend_n > 0) {
        printf(" —[经 %d 个路口 %.0f 米]→ ...", pend_n, pend_m);
    }
    printf("\n");
}

static double bearing_deg(const Campus *c, int a, int b)
{
    const Spot *sa = campus_spot(c, a);
    const Spot *sb = campus_spot(c, b);
    double dx, dy, ang;
    if (sa == NULL || sb == NULL) {
        return 0.0;
    }
    dx = sb->x - sa->x;
    dy = sb->y - sa->y;
    ang = atan2(dx, dy) * 180.0 / 3.14159265358979323846;
    if (ang < 0.0) {
        ang += 360.0;
    }
    return ang;
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

static const char *compass_label(double b)
{
    static const char *names[8] = { "北", "东北", "东", "东南", "南", "西南", "西", "西北" };
    int idx = (int)((b + 22.5) / 45.0) % 8;
    return names[idx];
}

static const char *road_name_of(const Campus *c, int a, int b)
{
    const Road *r = find_road(c, a, b);
    if (r == NULL || r->name[0] == '\0') {
        return "校内道路";
    }
    if (strcmp(r->name, "连接步道") == 0) {
        return "步行连接道";
    }
    return r->name;
}

static const char *turn_label(double delta)
{
    double d = fabs(delta);
    if (d < 25.0) {
        return "直行";
    }
    if (d < 60.0) {
        return delta > 0 ? "稍向右" : "稍向左";
    }
    if (d < 135.0) {
        return delta > 0 ? "右转" : "左转";
    }
    return "掉头";
}

/* 把路径翻译成“沿某路向某方向走多少米、在哪左/右转”的指引 */
static void print_turn_instructions(const Campus *c, const Path *p)
{
    int n = p->count;
    int s, e;
    double prev_b = 0.0;
    int first = 1;

    if (n < 2) {
        return;
    }
    printf("  转向指引：\n");
    s = 0;
    while (s < n - 1) {
        double len = seg_len_m(c, p->nodes[s], p->nodes[s + 1]);
        double b_start = bearing_deg(c, p->nodes[s], p->nodes[s + 1]);
        const char *road = road_name_of(c, p->nodes[s], p->nodes[s + 1]);

        e = s;
        while (e + 2 < n) {
            const char *r2 = road_name_of(c, p->nodes[e + 1], p->nodes[e + 2]);
            double b2 = bearing_deg(c, p->nodes[e + 1], p->nodes[e + 2]);
            if (strcmp(r2, road) == 0 && fabs(angle_delta(b_start, b2)) < 25.0) {
                len += seg_len_m(c, p->nodes[e + 1], p->nodes[e + 2]);
                ++e;
            } else {
                break;
            }
        }

        if (first) {
            printf("    出发：沿%s向%s行进 %.0f 米\n", road, compass_label(b_start), len);
            first = 0;
        } else {
            double d = angle_delta(prev_b, b_start);
            if (strcmp(turn_label(d), "直行") == 0) {
                printf("    继续沿%s向%s行进 %.0f 米\n", road, compass_label(b_start), len);
            } else {
                printf("    在路口%s，进入%s向%s行进 %.0f 米\n", turn_label(d), road,
                       compass_label(b_start), len);
            }
        }
        prev_b = bearing_deg(c, p->nodes[e], p->nodes[e + 1]);
        s = e + 1;
    }
    printf("    到达：%s\n", c->spots[p->nodes[n - 1] - 1].name);
}

void ui_print_path(const Campus *c, const Path *p, int by_time)
{
    if (c == NULL || p == NULL) {
        return;
    }
    if (p->count == 0) {
        printf("  两点之间不存在通路（可能被封闭道路切断）。\n");
        return;
    }
    printf("\n  最短路径（按%s）：\n    ", by_time ? "步行时间" : "距离");
    print_path_nodes(c, p);
    print_turn_instructions(c, p);
    printf("  总距离 %.1f 米，预计步行 %.1f 分钟\n", ui_path_length_m(c, p), ui_path_time_min(c, p));
    if (p->settled > 0) {
        printf("  路径经过 %d 个节点、%d 段道路；Dijkstra 出堆 %ld 次，松弛 %ld 次\n", p->count,
               p->count > 0 ? p->count - 1 : 0, p->settled, p->relaxed);
    } else {
        printf("  路径经过 %d 个节点、%d 段道路\n", p->count, p->count > 0 ? p->count - 1 : 0);
    }
}

void ui_print_matrix(const App *app, int limit)
{
    const Campus *c = &app->campus;
    const Graph *g = &app->graph;
    double *dist = NULL;
    int *next = NULL;
    int i, j, n = g->n;
    int show = n;
    char err[256];

    if (limit > 0 && limit < n) {
        show = limit;
    }
    if (algo_floyd(g, &dist, &next, err, sizeof(err)) != NAV_OK) {
        printf("  Floyd 计算失败：%s\n", err);
        return;
    }
    printf("\n  Floyd-Warshall 全源最短路矩阵（%s，单位：%s）\n", "所有点对",
           app->by_time ? "分钟" : "米");
    printf("      ");
    for (j = 0; j < show; ++j) {
        printf("%7d", j + 1);
    }
    printf("\n");
    for (i = 0; i < show; ++i) {
        printf("  %2d  ", i + 1);
        for (j = 0; j < show; ++j) {
            double d = dist[(size_t)i * (size_t)n + (size_t)j];
            if (d >= NAV_INF / 2.0) {
                printf("%7s", "--");
            } else {
                printf("%7.1f", d);
            }
        }
        printf("\n");
    }
    if (show < n) {
        printf("  （仅显示前 %d 个地点；共 %d 个）\n", show, n);
    }
    printf("  行/列编号对应地点：\n");
    for (i = 0; i < show; ++i) {
        printf("    %2d %s%s\n", i + 1, c->spots[i].name, (i % 2 == 1) ? "" : "");
    }
    free(dist);
    free(next);
}

void ui_print_critical(const App *app)
{
    const Campus *c = &app->campus;
    const Graph *g = &app->graph;
    int is_ap[NAV_MAX_SPOTS + 1];
    int bridges[NAV_MAX_ROADS];
    int n_bridges = 0;
    int i, n_ap = 0;
    char err[256];

    if (algo_critical(g, is_ap, bridges, &n_bridges, err, sizeof(err)) != NAV_OK) {
        printf("  关键道路分析失败：%s\n", err);
        return;
    }
    printf("\n  关键道路分析（Tarjan）\n");
    printf("  割点（删除后图不再连通的节点）：\n");
    for (i = 1; i <= g->n; ++i) {
        if (is_ap[i]) {
            ++n_ap;
            printf("    - %s（编号 %d）\n", c->spots[i - 1].name, i);
        }
    }
    if (n_ap == 0) {
        printf("    （无）\n");
    }
    printf("  桥（删除后图不再连通的边）：\n");
    for (i = 0; i < n_bridges; ++i) {
        const Road *r = campus_road(c, bridges[i]);
        if (r != NULL) {
            printf("    - %s — %s（道路 %d，%.1f 米）\n", c->spots[r->from - 1].name,
                   c->spots[r->to - 1].name, r->id, r->len_m);
        }
    }
    if (n_bridges == 0) {
        printf("    （无）\n");
    }
    printf("  结论：割点 %d 个，桥 %d 条。\n", n_ap, n_bridges);
}

void ui_print_mst(const App *app)
{
    const Campus *c = &app->campus;
    const Graph *g = &app->graph;
    int eu[NAV_MAX_SPOTS], ev[NAV_MAX_SPOTS];
    int cnt_p = 0, cnt_k = 0;
    double total_p, total_k;
    int i;

    total_p = algo_prim(g, eu, ev, &cnt_p);
    total_k = algo_kruskal(g, eu, ev, &cnt_k);
    if (total_p < 0.0 || total_k < 0.0) {
        printf("\n  最小生成树：图不连通，无法构造覆盖全部地点的生成树。\n");
        return;
    }
    printf("\n  最小生成树（校园道路布线方案）\n");
    printf("  Prim 与 Kruskal 结果对比：%.1f 米 / %.1f 米（应当相等）\n", total_p, total_k);
    printf("  选中 %d 条道路：\n", cnt_k);
    for (i = 0; i < cnt_k; ++i) {
        const Road *r = find_road(c, eu[i], ev[i]);
        printf("    %2d. %s — %s（%.1f 米）\n", i + 1, c->spots[eu[i] - 1].name,
               c->spots[ev[i] - 1].name, r != NULL ? r->len_m : 0.0);
    }
}

void ui_print_components(const App *app)
{
    const Campus *c = &app->campus;
    int comp[NAV_MAX_SPOTS + 1];
    int k, i, j;

    k = algo_components(&app->graph, comp, NULL, 0);
    if (k < 0) {
        printf("  连通分量计算失败。\n");
        return;
    }
    printf("\n  连通性检查：共 %d 个连通分量\n", k);
    for (i = 1; i <= k; ++i) {
        printf("    分量 %d：", i);
        for (j = 1; j <= app->graph.n; ++j) {
            if (comp[j] == i) {
                printf("%s ", c->spots[j - 1].name);
            }
        }
        printf("\n");
    }
}

/* ---------------- 交互输入 ---------------- */

static int prompt_line(const char *prompt, char *buf, size_t cap)
{
    printf("%s", prompt);
    fflush(stdout);
    return platform_read_line(buf, cap);
}

static int prompt_int(const char *prompt, int *out)
{
    char buf[64];
    char *end = NULL;
    long v;

    if (prompt_line(prompt, buf, sizeof(buf)) < 0) {
        return -1;
    }
    if (buf[0] == '\0') {
        return 2;
    }
    v = strtol(buf, &end, 10);
    if (end == NULL || *end != '\0') {
        return 2;
    }
    *out = (int)v;
    return 0;
}

static int prompt_spot(const Campus *c, const char *prompt, int *id)
{
    char buf[128];
    char err[128];
    for (;;) {
        if (prompt_line(prompt, buf, sizeof(buf)) < 0) {
            return -1;
        }
        if (buf[0] == '\0') {
            return 1;
        }
        if (campus_find_spot(c, buf, id) == NAV_OK) {
            return 0;
        }
        printf("  找不到地点“%s”，请输入编号或准确名称（直接回车取消）。\n", buf);
        nav_err_clear(err, sizeof(err));
    }
}

static int prompt_double(const char *prompt, double *out)
{
    char buf[64];
    char *end = NULL;
    double v;

    if (prompt_line(prompt, buf, sizeof(buf)) < 0) {
        return -1;
    }
    if (buf[0] == '\0') {
        return 1;
    }
    v = strtod(buf, &end);
    if (end == NULL || *end != '\0') {
        return 2;
    }
    *out = v;
    return 0;
}

/* ---------------- 菜单项实现 ---------------- */

static void menu_route(App *app)
{
    int from = 0, to = 0;
    int rc;
    Path p;
    char err[256];

    rc = prompt_spot(&app->campus, "  起点（编号或名称）：", &from);
    if (rc != 0) {
        return;
    }
    rc = prompt_spot(&app->campus, "  终点（编号或名称）：", &to);
    if (rc != 0) {
        return;
    }
    if (algo_dijkstra(&app->graph, from, to, &p, err, sizeof(err)) != NAV_OK) {
        printf("  计算失败：%s\n", err);
        return;
    }
    ui_print_path(&app->campus, &p, app->by_time);

    /* 若存在桥且在路径上，提示封路风险。 */
    {
        int is_ap[NAV_MAX_SPOTS + 1];
        int bridges[NAV_MAX_ROADS];
        int nb = 0, i, j;
        if (algo_critical(&app->graph, is_ap, bridges, &nb, err, sizeof(err)) == NAV_OK && nb > 0) {
            for (i = 0; i + 1 < p.count; ++i) {
                for (j = 0; j < nb; ++j) {
                    const Road *r = campus_road(&app->campus, bridges[j]);
                    if (r == NULL) {
                        continue;
                    }
                    if ((r->from == p.nodes[i] && r->to == p.nodes[i + 1]) ||
                        (r->from == p.nodes[i + 1] && r->to == p.nodes[i])) {
                        printf("  提示：路径中的“%s—%s”是桥，一旦封闭将导致绕行或断路。\n",
                               app->campus.spots[r->from - 1].name,
                               app->campus.spots[r->to - 1].name);
                    }
                }
            }
        }
    }
}

static void menu_tour(App *app)
{
    int start = 0, end = 0;
    int via[NAV_MAX_SPOTS];
    int k = 0, i, rc;
    int order[NAV_MAX_SPOTS];
    char buf[256];
    Path p;
    char err[256];

    rc = prompt_spot(&app->campus, "  出发点：", &start);
    if (rc != 0) {
        return;
    }
    printf("  依次输入途经地点（编号或名称，每行一个，空行结束，最多 %d 个）：\n",
           NAV_MAX_SPOTS - 3);
    for (;;) {
        char msg[64];
        snprintf(msg, sizeof(msg), "    途经点 %d：", k + 1);
        rc = prompt_spot(&app->campus, msg, &via[k]);
        if (rc != 0) {
            break;
        }
        if (k > 0) {
            int dup = 0, j;
            for (j = 0; j < k; ++j) {
                if (via[j] == via[k]) {
                    dup = 1;
                }
            }
            if (dup) {
                printf("    该地点已在列表中，请换一个。\n");
                continue;
            }
        }
        ++k;
        if (k >= NAV_MAX_SPOTS - 3) {
            break;
        }
    }
    if (k == 0) {
        printf("  未输入途经点，已取消。\n");
        return;
    }
    rc = prompt_spot(&app->campus, "  终点：", &end);
    if (rc != 0) {
        return;
    }
    if (algo_tsp(&app->graph, start, via, k, end, order, &p, err, sizeof(err)) != NAV_OK) {
        printf("  巡游计算失败：%s\n", err);
        return;
    }
    if (p.count == 0) {
        printf("  存在无法到达的途经点，巡游无解。\n");
        return;
    }
    printf("\n  推荐游览顺序（%s）：\n    ", k <= 12 ? "Held-Karp 精确解" : "最近邻 + 2-opt 近似解");
    printf("%s", app->campus.spots[start - 1].name);
    for (i = 0; i < k; ++i) {
        printf(" → %s", app->campus.spots[order[i] - 1].name);
    }
    printf(" → %s\n", app->campus.spots[end - 1].name);
    ui_print_path(&app->campus, &p, app->by_time);
    snprintf(buf, sizeof(buf), "%d", k);
    (void)buf;
}

static void menu_near(App *app)
{
    int from = 0, idx = 0, best = 0, rc;
    SpotType type;
    Path p;
    char err[256];

    rc = prompt_spot(&app->campus, "  当前位置：", &from);
    if (rc != 0) {
        return;
    }
    printf("  设施类型：\n");
    for (idx = 1; idx <= SPOT_TYPE_COUNT; ++idx) {
        printf("    %d. %s\n", idx, spot_type_label((SpotType)(idx - 1)));
    }
    rc = prompt_int("  请选择类型编号：", &idx);
    if (rc != 0 || idx < 1 || idx > SPOT_TYPE_COUNT) {
        printf("  输入无效，已取消。\n");
        return;
    }
    type = (SpotType)(idx - 1);
    if (algo_nearest_type(&app->graph, &app->campus, from, type, &best, &p, err, sizeof(err)) !=
        NAV_OK) {
        printf("  查询失败：%s\n", err);
        return;
    }
    printf("\n  最近的“%s”：%s（编号 %d）\n", spot_type_label(type),
           app->campus.spots[best - 1].name, best);
    ui_print_path(&app->campus, &p, app->by_time);
}

static void menu_edit(App *app)
{
    int choice;
    for (;;) {
        char err[256];
        printf("\n  ---- 图编辑 ----\n");
        printf("  1. 新增地点        2. 删除地点\n");
        printf("  3. 新增道路        4. 删除道路\n");
        printf("  5. 修改道路长度/时间  6. 封闭/恢复道路\n");
        printf("  0. 返回\n");
        if (prompt_int("  请选择：", &choice) != 0) {
            return;
        }
        if (choice == 0) {
            return;
        }
        if (choice == 1) {
            char name[NAV_NAME_MAX], brief[NAV_BRIEF_MAX];
            int ti = 1;
            double x = 0.0, y = 0.0;
            if (prompt_line("    名称：", name, sizeof(name)) < 0 || name[0] == '\0') {
                continue;
            }
            for (ti = 1; ti <= SPOT_TYPE_COUNT; ++ti) {
                printf("    %d. %s\n", ti, spot_type_label((SpotType)(ti - 1)));
            }
            if (prompt_int("    类型编号：", &ti) != 0 || ti < 1 || ti > SPOT_TYPE_COUNT) {
                printf("    类型无效，已取消。\n");
                continue;
            }
            if (prompt_double("    横坐标 x（米）：", &x) != 0 ||
                prompt_double("    纵坐标 y（米）：", &y) != 0) {
                printf("    坐标无效，已取消。\n");
                continue;
            }
            if (prompt_line("    简介（可空）：", brief, sizeof(brief)) < 0) {
                brief[0] = '\0';
            }
            if (campus_add_spot(&app->campus, name, (SpotType)(ti - 1), x, y, brief, err,
                                sizeof(err)) != NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    已新增地点“%s”（编号 %d）。\n", name, app->campus.n_spots);
                app->dirty = 1;
                if (app_rebuild(app, err, sizeof(err)) != NAV_OK) {
                    printf("    建图失败：%s\n", err);
                }
            }
        } else if (choice == 2) {
            int id = 0;
            if (prompt_spot(&app->campus, "    要删除的地点：", &id) != 0) {
                continue;
            }
            if (campus_remove_spot(&app->campus, id, err, sizeof(err)) != NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    已删除，并清理了相连道路。\n");
                app->dirty = 1;
                app_rebuild(app, err, sizeof(err));
            }
        } else if (choice == 3) {
            int a = 0, b = 0;
            double len = 0.0, tm = 0.0;
            if (prompt_spot(&app->campus, "    起点：", &a) != 0 ||
                prompt_spot(&app->campus, "    终点：", &b) != 0) {
                continue;
            }
            if (prompt_double("    长度（米）：", &len) != 0 ||
                prompt_double("    步行时间（分钟）：", &tm) != 0) {
                printf("    数值无效，已取消。\n");
                continue;
            }
            if (campus_add_road(&app->campus, a, b, len, tm, err, sizeof(err)) != NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    已新增道路（编号 %d）。\n", app->campus.n_roads);
                app->dirty = 1;
                app_rebuild(app, err, sizeof(err));
            }
        } else if (choice == 4) {
            int id = 0;
            if (prompt_int("    道路编号：", &id) != 0) {
                continue;
            }
            if (campus_remove_road(&app->campus, id, err, sizeof(err)) != NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    已删除道路 %d。\n", id);
                app->dirty = 1;
                app_rebuild(app, err, sizeof(err));
            }
        } else if (choice == 5) {
            const Road *r;
            int id = 0;
            double len = 0.0, tm = 0.0;
            if (prompt_int("    道路编号：", &id) != 0) {
                continue;
            }
            r = campus_road(&app->campus, id);
            if (r == NULL) {
                printf("    道路 %d 不存在。\n", id);
                continue;
            }
            printf("    当前：%.1f 米 / %.1f 分钟\n", r->len_m, r->walk_min);
            if (prompt_double("    新长度（米）：", &len) != 0 ||
                prompt_double("    新时间（分钟）：", &tm) != 0) {
                continue;
            }
            if (campus_update_road(&app->campus, id, len, tm, err, sizeof(err)) != NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    已更新。\n");
                app->dirty = 1;
                app_rebuild(app, err, sizeof(err));
            }
        } else if (choice == 6) {
            const Road *r;
            int id = 0;
            if (prompt_int("    道路编号：", &id) != 0) {
                continue;
            }
            r = campus_road(&app->campus, id);
            if (r == NULL) {
                printf("    道路 %d 不存在。\n", id);
                continue;
            }
            if (campus_set_road_open(&app->campus, id, r->open ? 0 : 1, err, sizeof(err)) !=
                NAV_OK) {
                printf("    失败：%s\n", err);
            } else {
                printf("    道路 %d 现在状态：%s\n", id, r->open ? "封闭" : "通行");
                app->dirty = 1;
                app_rebuild(app, err, sizeof(err));
            }
        } else {
            printf("  无效选项。\n");
        }
    }
}

static void menu_data(App *app)
{
    int choice;
    char err[256];
    for (;;) {
        printf("\n  ---- 数据管理 ----\n");
        printf("  1. 保存为 CSV        2. 重新载入\n");
        printf("  3. 导出二进制快照    4. 从快照导入\n");
        printf("  0. 返回\n");
        if (prompt_int("  请选择：", &choice) != 0) {
            return;
        }
        if (choice == 0) {
            return;
        }
        if (choice == 1) {
            if (app_save(app, err, sizeof(err)) != NAV_OK) {
                printf("  保存失败：%s\n", err);
            } else {
                printf("  已保存到 %s\n", app->data_dir);
            }
        } else if (choice == 2) {
            if (app_reload(app, err, sizeof(err)) != NAV_OK) {
                printf("  载入失败：%s\n", err);
            } else {
                printf("  已重新载入：%d 个地点，%d 条道路。\n", app->campus.n_spots,
                       app->campus.n_roads);
            }
        } else if (choice == 3 || choice == 4) {
            /* 2 倍容量：data_dir 最长 259 字节，再加 "/campus.nvg" 共 271 字节 */
            char path[APP_PATH_MAX * 2];
            if (choice == 3) {
                snprintf(path, sizeof(path), "%s/campus.nvg", app->data_dir);
                if (campus_save_bin(&app->campus, path, err, sizeof(err)) != NAV_OK) {
                    printf("  导出失败：%s\n", err);
                } else {
                    printf("  已导出快照：%s\n", path);
                }
            } else {
                snprintf(path, sizeof(path), "%s/campus.nvg", app->data_dir);
                if (campus_load_bin(&app->campus, path, err, sizeof(err)) != NAV_OK) {
                    printf("  导入失败：%s\n", err);
                } else {
                    printf("  已从快照导入：%d 个地点，%d 条道路。\n", app->campus.n_spots,
                           app->campus.n_roads);
                    app->dirty = 1;
                    app_rebuild(app, err, sizeof(err));
                }
            }
        } else {
            printf("  无效选项。\n");
        }
    }
}

int ui_run_menu(App *app)
{
    int choice;
    for (;;) {
        printf("\n");
        hr('-', 66);
        printf("  1. 校园地图            2. 地点列表 / 详情\n");
        printf("  3. 路径规划            4. 多点巡游\n");
        printf("  5. 最近设施            6. 全源最短路矩阵（Floyd）\n");
        printf("  7. 关键道路分析        8. 最小生成树（道路布线）\n");
        printf("  9. 图编辑              10. 数据管理\n");
        printf("  11. 算法对比实验       12. 切换权重口径（当前：%s）\n",
               app->by_time ? "时间" : "距离");
        printf("  0. 退出\n");
        hr('-', 66);
        if (prompt_int("  请选择：", &choice) != 0) {
            return 0;
        }
        switch (choice) {
        case 0:
            if (app->dirty) {
                printf("  有未保存的修改，退出前请考虑先执行“数据管理 → 保存”。\n");
            }
            return 0;
        case 1:
            ui_ascii_map(app);
            break;
        case 2: {
            int id;
            ui_print_spots(&app->campus);
            if (prompt_spot(&app->campus, "  查看详情（编号或名称，回车跳过）：", &id) == 0) {
                ui_print_spot_detail(&app->campus, id);
            }
            break;
        }
        case 3:
            menu_route(app);
            break;
        case 4:
            menu_tour(app);
            break;
        case 5:
            menu_near(app);
            break;
        case 6:
            ui_print_matrix(app, 22);
            break;
        case 7:
            ui_print_critical(app);
            break;
        case 8:
            ui_print_mst(app);
            break;
        case 9:
            menu_edit(app);
            break;
        case 10:
            menu_data(app);
            break;
        case 11: {
            int n = 120;
            unsigned int seed = 2026u;
            char err[256];
            prompt_int("  节点数（建议 60..200）：", &n);
            if (n < 12 || n > NAV_MAX_SPOTS) {
                n = 120;
            }
            printf("  随机种子（回车用 2026）：");
            {
                char buf[32];
                if (prompt_line("", buf, sizeof(buf)) >= 0 && buf[0] != '\0') {
                    seed = (unsigned int)strtoul(buf, NULL, 10);
                }
            }
            bench_run(n, seed, 32, err, sizeof(err));
            break;
        }
        case 12:
            app_set_criterion(app, app->by_time ? 0 : 1, NULL, 0);
            printf("  已切换到“%s”口径，图已重建。\n", app->by_time ? "步行时间" : "距离");
            break;
        default:
            printf("  无效选项，请重新选择。\n");
            break;
        }
    }
}
