/* bench.c —— 随机校园图上的算法基准测试（用于报告性能对比）。
 *
 * 随机图生成：n 个节点放在网格上，先连成一条链保证连通，再随机补边到约 3n 条，
 * 权重取欧氏距离（米）与 78 米/分钟的步行时间。
 */
#include "bench.h"
#include "algo.h"
#include "campus.h"
#include "err.h"
#include "graph.h"
#include "nav.h"
#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static unsigned int g_seed = 1u;

static unsigned int rnd_next(void)
{
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}

static int rnd_range(int lo, int hi)
{
    if (hi <= lo) {
        return lo;
    }
    return lo + (int)(rnd_next() % (unsigned int)(hi - lo + 1));
}

static double now_ms(void)
{
    return platform_now_ms();
}

static int has_road(const Campus *c, int a, int b)
{
    int i;
    for (i = 0; i < c->n_roads; ++i) {
        if ((c->roads[i].from == a && c->roads[i].to == b) ||
            (c->roads[i].from == b && c->roads[i].to == a)) {
            return 1;
        }
    }
    return 0;
}

static int add_road_auto(Campus *c, int a, int b)
{
    char err[128];
    double dx, dy, len;

    if (a == b || has_road(c, a, b)) {
        return 0;
    }
    if (c->n_roads >= NAV_MAX_ROADS) {
        return -1;
    }
    dx = c->spots[a - 1].x - c->spots[b - 1].x;
    dy = c->spots[a - 1].y - c->spots[b - 1].y;
    len = sqrt(dx * dx + dy * dy) * 1.12;
    if (len < 1.0) {
        len = 1.0;
    }
    return campus_add_road(c, a, b, len, len / 78.0, err, sizeof(err));
}

static int make_random_campus(Campus *c, int n)
{
    int i, cols;
    char name[32];

    campus_init(c);
    c->width_m = 1000.0;
    c->height_m = 800.0;
    snprintf(c->title, sizeof(c->title), "随机测试校园");
    cols = 1;
    while (cols * cols < n) {
        ++cols;
    }
    for (i = 1; i <= n; ++i) {
        double x = 20.0 + 940.0 * (double)((i - 1) % cols) / (double)(cols > 1 ? cols - 1 : 1);
        double y = 20.0 + 740.0 * (double)((i - 1) / cols) / (double)(cols > 1 ? cols - 1 : 1);
        snprintf(name, sizeof(name), "N%d", i);
        if (campus_add_spot(c, name, (i % 3 == 0) ? SPOT_DORM : SPOT_TEACHING, x, y, "", NULL,
                            0) != NAV_OK) {
            return -1;
        }
    }
    for (i = 1; i < n; ++i) {
        if (add_road_auto(c, i, i + 1) != 0) {
            return -1;
        }
    }
    {
        int target = n * 3;
        int guard = 0;
        while (c->n_roads < target && guard++ < target * 20) {
            int a = rnd_range(1, n);
            int b = rnd_range(1, n);
            add_road_auto(c, a, b);
        }
    }
    return 0;
}

int bench_run(int n, unsigned int seed, int repeat, char *err, size_t cap)
{
    Campus c;
    Graph g;
    double t0, t1;
    double build_ms, dijkstra_ms, floyd_ms, bellman_ms, prim_ms, kruskal_ms;
    int i;
    Path p;
    double *dist = NULL;
    int *next = NULL;
    double bf_dist[NAV_MAX_SPOTS + 1];
    int bf_prev[NAV_MAX_SPOTS + 1];
    int eu[NAV_MAX_SPOTS], ev[NAV_MAX_SPOTS], cnt = 0;
    double mst_total;
    char e[256];

    if (n < 4 || n > NAV_MAX_SPOTS) {
        snprintf(err, cap, "节点数需在 4..%d 之间", NAV_MAX_SPOTS);
        return NAV_ERR_RANGE;
    }
    if (repeat < 1) {
        repeat = 1;
    }
    g_seed = seed ? seed : 1u;
    printf("\n  随机图基准：n=%d, seed=%u, Dijkstra 重复 %d 次\n", n, seed, repeat);
    printf("  %-22s %14s %14s\n", "阶段/算法", "耗时(ms)", "结果");
    printf("  ------------------------------------------------------------------\n");

    t0 = now_ms();
    if (make_random_campus(&c, n) != 0) {
        snprintf(err, cap, "随机图生成失败（n=%d）", n);
        return NAV_ERR_NOMEM;
    }
    if (graph_build(&c, 0, &g, e, sizeof(e)) != NAV_OK) {
        snprintf(err, cap, "随机图建图失败：%s", e);
        return NAV_ERR_NOMEM;
    }
    t1 = now_ms();
    build_ms = t1 - t0;
    printf("  %-22s %14.3f %14s\n", "建图", build_ms, "-");

    t0 = now_ms();
    for (i = 0; i < repeat; ++i) {
        if (algo_dijkstra(&g, 1, n, &p, e, sizeof(e)) != NAV_OK) {
            snprintf(err, cap, "Dijkstra 失败：%s", e);
            graph_free(&g);
            return NAV_ERR_ARG;
        }
    }
    t1 = now_ms();
    dijkstra_ms = (t1 - t0) / (double)repeat;
    printf("  %-22s %14.4f %14.1f\n", "Dijkstra (1→n)", dijkstra_ms, p.weight);

    t0 = now_ms();
    if (algo_floyd(&g, &dist, &next, e, sizeof(e)) != NAV_OK) {
        snprintf(err, cap, "Floyd 失败：%s", e);
        graph_free(&g);
        return NAV_ERR_ARG;
    }
    t1 = now_ms();
    floyd_ms = t1 - t0;
    printf("  %-22s %14.4f %14.1f\n", "Floyd-Warshall", floyd_ms, dist[(size_t)(n - 1)]);

    t0 = now_ms();
    if (algo_bellman(&g, 1, bf_dist, bf_prev, e, sizeof(e)) != NAV_OK) {
        snprintf(err, cap, "Bellman-Ford 失败：%s", e);
        free(dist);
        free(next);
        graph_free(&g);
        return NAV_ERR_ARG;
    }
    t1 = now_ms();
    bellman_ms = t1 - t0;
    printf("  %-22s %14.4f %14.1f\n", "Bellman-Ford", bellman_ms, bf_dist[n]);

    t0 = now_ms();
    mst_total = algo_prim(&g, eu, ev, &cnt);
    t1 = now_ms();
    prim_ms = t1 - t0;
    printf("  %-22s %14.4f %14.1f\n", "Prim (MST)", prim_ms, mst_total);

    t0 = now_ms();
    mst_total = algo_kruskal(&g, eu, ev, &cnt);
    t1 = now_ms();
    kruskal_ms = t1 - t0;
    printf("  %-22s %14.4f %14.1f\n", "Kruskal (MST)", kruskal_ms, mst_total);
    printf("  ------------------------------------------------------------------\n");
    printf("  图规模：%d 个节点，%d 条道路\n", g.n, g.n_edges);

    free(dist);
    free(next);
    graph_free(&g);
    return NAV_OK;
}
