/* test_main.c —— 校园导航系统自动化测试。
 *
 * 覆盖：
 *   1. 工具层（CRC32、显示宽度）
 *   2. 数据集（内置数据、CSV 读写与错误路径、二进制快照与损坏检测、编辑操作）
 *   3. 图构建（CSR、邻接矩阵、封闭道路）
 *   4. 算法（Dijkstra / Floyd / Bellman-Ford / BFS / 连通分量 / 割点与桥 / MST / TSP / 最近设施）
 *   5. 算法交叉验证（随机图 30 组，全点对比较）
 *   6. 性能冒烟测试
 */
#include "algo.h"
#include "app.h"
#include "bench.h"
#include "campus.h"
#include "crc32.h"
#include "graph.h"
#include "nav.h"
#include "platform.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int g_checks = 0;
static int g_fails = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_fails;                                                           \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)

#define CHECK_INT(actual, expect)                                                \
    do {                                                                         \
        long a_ = (long)(actual);                                                \
        long e_ = (long)(expect);                                                \
        ++g_checks;                                                              \
        if (a_ != e_) {                                                          \
            ++g_fails;                                                           \
            printf("  FAIL %s:%d  %s == %ld, expect %ld\n", __FILE__, __LINE__,  \
                   #actual, a_, e_);                                             \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(actual, expect, eps)                                          \
    do {                                                                         \
        double a_ = (double)(actual);                                            \
        double e_ = (double)(expect);                                            \
        ++g_checks;                                                              \
        if (fabs(a_ - e_) > (eps)) {                                             \
            ++g_fails;                                                           \
            printf("  FAIL %s:%d  %s == %.6f, expect %.6f (eps %.6f)\n",         \
                   __FILE__, __LINE__, #actual, a_, e_, (double)(eps));          \
        }                                                                        \
    } while (0)

#define CHECK_STR(actual, expect)                                                \
    do {                                                                         \
        const char *a_ = (actual);                                               \
        const char *e_ = (expect);                                               \
        ++g_checks;                                                              \
        if (a_ == NULL || strcmp(a_, e_) != 0) {                                 \
            ++g_fails;                                                           \
            printf("  FAIL %s:%d  %s == \"%s\", expect \"%s\"\n", __FILE__,      \
                   __LINE__, #actual, a_ ? a_ : "(null)", e_);                   \
        }                                                                        \
    } while (0)

/* ---------------- 测试辅助 ---------------- */

static unsigned int s_rng = 20261120u;

static unsigned int rnd_next(void)
{
    s_rng ^= s_rng << 13;
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return s_rng;
}

static int rnd_int(int lo, int hi)
{
    if (hi <= lo) {
        return lo;
    }
    return lo + (int)(rnd_next() % (unsigned int)(hi - lo + 1));
}

static const Road *find_road(const Campus *c, int a, int b)
{
    int i;
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
            return r;
        }
    }
    return NULL;
}

static int has_road(const Campus *c, int a, int b)
{
    return find_road(c, a, b) != NULL;
}

/* 链状图：1-2-3-...-n，边长 step。 */
static void make_line(Campus *c, int n, double step)
{
    int i;
    char name[24];
    char err[128];

    campus_init(c);
    snprintf(c->title, sizeof(c->title), "链式测试图");
    c->width_m = 1000.0;
    c->height_m = 800.0;
    for (i = 1; i <= n; ++i) {
        snprintf(name, sizeof(name), "P%d", i);
        campus_add_spot(c, name, SPOT_TEACHING, 50.0 + 40.0 * (double)(i - 1), 100.0, "", NULL, 0);
    }
    for (i = 1; i < n; ++i) {
        campus_add_road(c, i, i + 1, step, step / 78.0, err, sizeof(err));
    }
}

/* 环状图：1-2-...-n-1。 */
static void make_cycle(Campus *c, int n, double step)
{
    char err[128];
    make_line(c, n, step);
    campus_add_road(c, n, 1, step * 2.0, step * 2.0 / 78.0, err, sizeof(err));
}

static void make_random(Campus *c, int n, unsigned int seed)
{
    int i, j, cols, target;
    char name[24];
    char err[128];

    s_rng = seed ? seed : 1u;
    campus_init(c);
    snprintf(c->title, sizeof(c->title), "随机测试图");
    c->width_m = 1000.0;
    c->height_m = 800.0;
    cols = 1;
    while (cols * cols < n) {
        ++cols;
    }
    for (i = 1; i <= n; ++i) {
        double x = 20.0 + 940.0 * (double)((i - 1) % cols) / (double)(cols > 1 ? cols - 1 : 1);
        double y = 20.0 + 740.0 * (double)((i - 1) / cols) / (double)(cols > 1 ? cols - 1 : 1);
        snprintf(name, sizeof(name), "N%d", i);
        campus_add_spot(c, name, SPOT_SCENIC, x, y, "", NULL, 0);
    }
    for (i = 1; i < n; ++i) {
        campus_add_road(c, i, i + 1, (double)rnd_int(10, 90), 1.0, err, sizeof(err));
    }
    target = n * 3;
    for (j = 0; j < target * 6 && c->n_roads < target; ++j) {
        int a = rnd_int(1, n);
        int b = rnd_int(1, n);
        if (a != b && !has_road(c, a, b)) {
            campus_add_road(c, a, b, (double)rnd_int(10, 90), 1.0, err, sizeof(err));
        }
    }
}

static double edge_weight(const Graph *g, int a, int b)
{
    int arc;
    for (arc = g->off[a - 1]; arc < g->off[a]; ++arc) {
        if (g->to[arc] == b) {
            return g->w[arc];
        }
    }
    return NAV_INF;
}

/* 校验路径合法性：相邻节点有边、总权重与边权和一致。 */
static int path_valid(const Graph *g, const Path *p)
{
    int i;
    double sum = 0.0;
    if (p->count < 1) {
        return 0;
    }
    for (i = 0; i < p->count; ++i) {
        if (p->nodes[i] < 1 || p->nodes[i] > g->n) {
            return 0;
        }
    }
    for (i = 0; i + 1 < p->count; ++i) {
        double w = edge_weight(g, p->nodes[i], p->nodes[i + 1]);
        if (w >= NAV_INF / 2.0) {
            return 0;
        }
        sum += w;
    }
    return fabs(sum - p->weight) < 1e-6 * (double)(p->count + 1);
}

static int write_text_file(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    fputs(text, fp);
    fclose(fp);
    return 0;
}

static int read_binary_file(const char *path, unsigned char *buf, size_t cap, size_t *len)
{
    FILE *fp = fopen(path, "rb");
    size_t n;
    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, cap, fp);
    fclose(fp);
    *len = n;
    return 0;
}

static int write_binary_file(const char *path, const unsigned char *buf, size_t len)
{
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return -1;
    }
    if (fwrite(buf, 1, len, fp) != len) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

/* ---------------- 1. 工具层 ---------------- */

static void test_crc_and_display(void)
{
    printf("== 工具层 ==\n");
    /* 标准测试向量："123456789" -> 0xCBF43926 */
    CHECK(nav_crc32("123456789", 9) == 0xCBF43926u);
    CHECK(nav_crc32("", 0) == 0u);
    CHECK(nav_crc32_update(nav_crc32("1234", 4), "56789", 5) == 0xCBF43926u);
    CHECK_INT(nav_display_width("abc"), 3);
    CHECK_INT(nav_display_width("图书馆"), 6);
    CHECK_INT(nav_display_width("A楼"), 3);
    CHECK_INT(nav_display_width(""), 0);
}

/* ---------------- 2. 数据集 ---------------- */

static void test_default_dataset(void)
{
    Campus c;
    printf("== 默认数据集 ==\n");
    CHECK_INT(campus_load_default(&c), NAV_OK);
    CHECK_INT(c.n_spots, 24);
    CHECK(c.n_roads >= 30);
    CHECK_INT(campus_validate(&c, NULL, 0), NAV_OK);
    CHECK_INT(campus_is_connected(&c), 1);
    CHECK(campus_total_road_length(&c) > 5000.0);
    CHECK(campus_total_road_length(&c) < 20000.0);
    CHECK_INT(campus_count_type(&c, SPOT_GATE), 3);
    CHECK_INT(campus_count_type(&c, SPOT_CANTEEN), 2);
    CHECK_INT(campus_count_type(&c, SPOT_DORM), 2);
    CHECK_INT(campus_count_type(&c, SPOT_MEDICAL), 1);
    CHECK(c.spots[0].id == 1);
    CHECK_STR(c.spots[0].name, "东大门");
    CHECK_STR(spot_type_name(SPOT_GATE), "gate");
    CHECK_STR(spot_type_label(SPOT_LIBRARY), "图书馆");
}

static void test_csv_roundtrip(void)
{
    Campus a, b;
    char err[256];
    const char *sp = "build/tmp_spots.csv";
    const char *rd = "build/tmp_roads.csv";
    int i;

    printf("== CSV 读写 ==\n");
    platform_mkdir("build");
    campus_load_default(&a);
    CHECK_INT(campus_save_csv(&a, sp, rd, err, sizeof(err)), NAV_OK);
    CHECK_INT(campus_load_csv(&b, sp, rd, err, sizeof(err)), NAV_OK);
    CHECK_INT(b.n_spots, a.n_spots);
    CHECK_INT(b.n_roads, a.n_roads);
    CHECK_NEAR(campus_total_road_length(&b), campus_total_road_length(&a), 0.05);
    for (i = 0; i < a.n_spots; ++i) {
        CHECK_STR(b.spots[i].name, a.spots[i].name);
        CHECK_INT(b.spots[i].type, a.spots[i].type);
        CHECK_NEAR(b.spots[i].x, a.spots[i].x, 0.05);
        CHECK_NEAR(b.spots[i].y, a.spots[i].y, 0.05);
    }
    CHECK_STR(b.title, a.title);

    /* 带逗号与引号的简介必须原样往返。 */
    {
        Campus c;
        char err2[256];
        campus_init(&c);
        c.width_m = 200.0;
        c.height_m = 200.0;
        CHECK_INT(campus_add_spot(&c, "测试点", SPOT_SERVICE, 10.0, 10.0,
                                  "含逗号, 和\"引号\"的简介", err2, sizeof(err2)),
                  NAV_OK);
        CHECK_INT(campus_save_csv(&c, "build/tmp_q_spots.csv", "build/tmp_q_roads.csv", err2,
                                  sizeof(err2)),
                  NAV_OK);
        CHECK_INT(campus_load_csv(&c, "build/tmp_q_spots.csv", "build/tmp_q_roads.csv", err2,
                                  sizeof(err2)),
                  NAV_OK);
        CHECK_STR(c.spots[0].brief, "含逗号, 和\"引号\"的简介");
    }
}

static void test_csv_errors(void)
{
    Campus c;
    char err[256];
    const char *sp = "build/tmp_bad_spots.csv";
    const char *rd = "build/tmp_bad_roads.csv";

    printf("== CSV 错误路径 ==\n");
    write_text_file(sp, "# only comment\n");
    write_text_file(rd, "");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "地点文件为空") != NULL);

    write_text_file(sp, "1,A,unknown_type,10,10,x\n");
    write_text_file(rd, "");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "类型未知") != NULL);

    write_text_file(sp, "1,A,gate,10,10,x\n3,B,gate,20,20,y\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);

    write_text_file(sp, "1,A,gate,10,10,x\n2,B,gate,20,20,y\n");
    write_text_file(rd, "1,1,99,10,1\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);

    write_text_file(rd, "1,1,2,abc,1\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);

    write_text_file(rd, "1,1,2,-5,1\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);

    write_text_file(rd, "1,1,2,10,1\n2,2,1,12,1\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "重复") != NULL);

    write_text_file(rd, "1,1,1,10,1\n");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "自环") != NULL);

    CHECK_INT(campus_load_csv(&c, "build/no_such_file.csv", rd, err, sizeof(err)), NAV_ERR_IO);
    CHECK(strstr(err, "无法打开") != NULL);

    /* 名称重复 */
    write_text_file(sp, "1,A,gate,10,10,x\n2,A,gate,20,20,y\n");
    write_text_file(rd, "");
    CHECK_INT(campus_load_csv(&c, sp, rd, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "名称重复") != NULL);
}

static void test_binary_roundtrip(void)
{
    Campus a, b;
    char err[256];
    const char *path = "build/tmp_snapshot.nvg";

    printf("== 二进制快照 ==\n");
    campus_load_default(&a);
    a.roads[0].open = 0;
    CHECK_INT(campus_save_bin(&a, path, err, sizeof(err)), NAV_OK);
    CHECK_INT(campus_load_bin(&b, path, err, sizeof(err)), NAV_OK);
    CHECK_INT(b.n_spots, a.n_spots);
    CHECK_INT(b.n_roads, a.n_roads);
    CHECK_INT(b.roads[0].open, 0);
    CHECK_STR(b.title, a.title);
    CHECK_STR(b.spots[3].name, a.spots[3].name);
    CHECK_STR(b.spots[3].brief, a.spots[3].brief);
}

static void test_binary_corruption(void)
{
    Campus a, b;
    char err[256];
    const char *path = "build/tmp_bad_snapshot.nvg";
    static unsigned char buf[65536];
    size_t len = 0;

    printf("== 快照损坏检测 ==\n");
    campus_load_default(&a);
    CHECK_INT(campus_save_bin(&a, path, err, sizeof(err)), NAV_OK);
    CHECK_INT(read_binary_file(path, buf, sizeof(buf), &len), 0);
    CHECK(len > 40);

    /* 翻转中间一个字节 -> CRC 失败 */
    buf[len / 2] ^= 0xFFu;
    CHECK_INT(write_binary_file(path, buf, len), 0);
    CHECK_INT(campus_load_bin(&b, path, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "CRC32") != NULL);

    /* 截断文件 -> 校验失败 */
    CHECK_INT(write_binary_file(path, buf, len - 7), 0);
    CHECK_INT(campus_load_bin(&b, path, err, sizeof(err)), NAV_ERR_FMT);

    /* 错误 magic */
    memset(buf, 0, 48);
    memcpy(buf, "XXXX", 4);
    CHECK_INT(write_binary_file(path, buf, 48), 0);
    CHECK_INT(campus_load_bin(&b, path, err, sizeof(err)), NAV_ERR_FMT);
    CHECK(strstr(err, "magic") != NULL || strstr(err, "CRC") != NULL);

    /* 不存在的文件 */
    CHECK_INT(campus_load_bin(&b, "build/no_such.nvg", err, sizeof(err)), NAV_ERR_IO);
}

static void test_edit_operations(void)
{
    Campus c;
    char err[256];

    printf("== 编辑操作 ==\n");
    make_line(&c, 4, 10.0);
    CHECK_INT(c.n_spots, 4);
    CHECK_INT(c.n_roads, 3);

    /* 删除中间节点：剩余道路端点重新编号 */
    CHECK_INT(campus_remove_spot(&c, 2, err, sizeof(err)), NAV_OK);
    CHECK_INT(c.n_spots, 3);
    CHECK_INT(c.n_roads, 1); /* 1-2 与 2-3 被删除，只剩 3-4 */
    CHECK(c.roads[0].from >= 1 && c.roads[0].from <= c.n_spots);
    CHECK_INT(campus_validate(&c, err, sizeof(err)), NAV_OK);

    /* 新增节点与道路 */
    CHECK_INT(campus_add_spot(&c, "新点", SPOT_LIBRARY, 500.0, 500.0, "", err, sizeof(err)),
              NAV_OK);
    CHECK_INT(c.n_spots, 4);
    CHECK_INT(campus_add_road(&c, 4, 1, 100.0, 2.0, err, sizeof(err)), NAV_OK);
    CHECK_INT(campus_add_road(&c, 1, 4, 100.0, 2.0, err, sizeof(err)), NAV_ERR_ARG);
    CHECK(strstr(err, "已有道路") != NULL);
    CHECK_INT(campus_add_road(&c, 2, 2, 100.0, 2.0, err, sizeof(err)), NAV_ERR_ARG);
    CHECK_INT(campus_add_road(&c, 1, 2, -1.0, 2.0, err, sizeof(err)), NAV_ERR_ARG);
    CHECK_INT(campus_add_road(&c, 1, 2, 10.0, 0.0, err, sizeof(err)), NAV_ERR_ARG);
    CHECK_INT(campus_add_road(&c, 1, 99, 10.0, 1.0, err, sizeof(err)), NAV_ERR_RANGE);

    /* 重名与越界坐标 */
    CHECK_INT(campus_add_spot(&c, "新点", SPOT_GATE, 1.0, 1.0, "", err, sizeof(err)),
              NAV_ERR_ARG);
    CHECK_INT(campus_add_spot(&c, "越界点", SPOT_GATE, 5000.0, 1.0, "", err, sizeof(err)),
              NAV_ERR_RANGE);

    /* 修改道路与封路 */
    CHECK_INT(campus_update_road(&c, 1, 55.0, 1.5, err, sizeof(err)), NAV_OK);
    CHECK_NEAR(c.roads[0].len_m, 55.0, 1e-9);
    CHECK_INT(campus_update_road(&c, 1, 0.0, 1.5, err, sizeof(err)), NAV_ERR_ARG);
    CHECK_INT(campus_update_road(&c, 99, 10.0, 1.0, err, sizeof(err)), NAV_ERR_RANGE);
    CHECK_INT(campus_set_road_open(&c, 1, 0, err, sizeof(err)), NAV_OK);
    CHECK_INT(c.roads[0].open, 0);

    /* 删除道路后编号连续 */
    CHECK_INT(campus_remove_road(&c, 1, err, sizeof(err)), NAV_OK);
    CHECK_INT(campus_validate(&c, err, sizeof(err)), NAV_OK);

    /* 校验器能抓到手工构造的非法数据 */
    {
        Campus bad;
        make_line(&bad, 3, 10.0);
        bad.roads[1].from = bad.roads[1].to;
        CHECK_INT(campus_validate(&bad, err, sizeof(err)), NAV_ERR_FMT);
        make_line(&bad, 3, 10.0);
        bad.roads[0].len_m = -1.0;
        CHECK_INT(campus_validate(&bad, err, sizeof(err)), NAV_ERR_FMT);
        make_line(&bad, 3, 10.0);
        bad.spots[1].id = 7;
        CHECK_INT(campus_validate(&bad, err, sizeof(err)), NAV_ERR_FMT);
    }
}

static void test_find_spot(void)
{
    Campus c;
    int id = 0;

    printf("== 地点查找 ==\n");
    campus_load_default(&c);
    CHECK_INT(campus_find_spot(&c, "4", &id), NAV_OK);
    CHECK_INT(id, 4);
    CHECK_INT(campus_find_spot(&c, "图书馆", &id), NAV_OK);
    CHECK_INT(id, 4);
    CHECK_INT(campus_find_spot(&c, "不存在的点", &id), NAV_ERR_RANGE);
    CHECK_INT(campus_find_spot(&c, "999", &id), NAV_ERR_RANGE);
    CHECK_INT(campus_find_spot(&c, "", &id), NAV_ERR_ARG);
    CHECK(campus_spot(&c, 4) != NULL);
    CHECK(campus_spot(&c, 99) == NULL);
}

/* ---------------- 3. 图构建 ---------------- */

static void test_graph_build(void)
{
    Campus c;
    Graph g;
    char err[256];
    double *mat = NULL;

    printf("== 图构建 ==\n");
    make_line(&c, 5, 10.0);
    CHECK_INT(graph_build(&c, 0, &g, err, sizeof(err)), NAV_OK);
    CHECK_INT(g.n, 5);
    CHECK_INT(g.n_edges, 4);
    CHECK_INT(g.n_arcs, 8);
    CHECK_NEAR(edge_weight(&g, 1, 2), 10.0, 1e-9);
    CHECK_NEAR(edge_weight(&g, 5, 4), 10.0, 1e-9);
    CHECK(edge_weight(&g, 1, 5) >= NAV_INF / 2.0);
    CHECK_INT(graph_adj_matrix(&g, &mat, err, sizeof(err)), NAV_OK);
    CHECK_NEAR(mat[0], 0.0, 1e-9);
    CHECK_NEAR(mat[1], 10.0, 1e-9);
    CHECK(mat[4] >= NAV_INF / 2.0);
    free(mat);
    graph_free(&g);
    CHECK(g.n == 0 && g.off == NULL);

    /* 时间口径 */
    CHECK_INT(graph_build(&c, 1, &g, err, sizeof(err)), NAV_OK);
    CHECK_NEAR(edge_weight(&g, 1, 2), 10.0 / 78.0, 1e-9);
    graph_free(&g);

    /* 封闭道路不参与建图 */
    CHECK_INT(campus_set_road_open(&c, 2, 0, err, sizeof(err)), NAV_OK);
    CHECK_INT(graph_build(&c, 0, &g, err, sizeof(err)), NAV_OK);
    CHECK_INT(g.n_edges, 3);
    CHECK(edge_weight(&g, 2, 3) >= NAV_INF / 2.0);
    graph_free(&g);
}

/* ---------------- 4. 算法 ---------------- */

static void test_dijkstra_basics(void)
{
    Campus c;
    Graph g;
    Path p;
    char err[256];

    printf("== Dijkstra ==\n");
    make_line(&c, 5, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));

    CHECK_INT(algo_dijkstra(&g, 1, 5, &p, err, sizeof(err)), NAV_OK);
    CHECK_INT(p.count, 5);
    CHECK_NEAR(p.weight, 40.0, 1e-9);
    CHECK_INT(p.nodes[0], 1);
    CHECK_INT(p.nodes[4], 5);
    CHECK(path_valid(&g, &p));
    CHECK(p.settled > 0);
    CHECK(p.relaxed > 0);

    /* 起点等于终点 */
    CHECK_INT(algo_dijkstra(&g, 3, 3, &p, err, sizeof(err)), NAV_OK);
    CHECK_INT(p.count, 1);
    CHECK_NEAR(p.weight, 0.0, 1e-9);

    /* 参数越界 */
    CHECK_INT(algo_dijkstra(&g, 0, 3, &p, err, sizeof(err)), NAV_ERR_RANGE);
    CHECK_INT(algo_dijkstra(&g, 1, 99, &p, err, sizeof(err)), NAV_ERR_RANGE);

    /* 不连通 */
    {
        Campus iso;
        Graph gi;
        make_line(&iso, 3, 10.0);
        campus_add_spot(&iso, "孤岛", SPOT_SCENIC, 900.0, 700.0, "", NULL, 0);
        graph_build(&iso, 0, &gi, err, sizeof(err));
        CHECK_INT(algo_dijkstra(&gi, 1, 4, &p, err, sizeof(err)), NAV_OK);
        CHECK_INT(p.count, 0);
        CHECK(p.weight >= NAV_INF / 2.0);
        graph_free(&gi);
    }
    graph_free(&g);

    /* 等权二选一时选择更短的一条 */
    {
        Campus d;
        campus_init(&d);
        d.width_m = 500.0;
        d.height_m = 500.0;
        campus_add_spot(&d, "A", SPOT_GATE, 10, 10, "", NULL, 0);
        campus_add_spot(&d, "B", SPOT_GATE, 100, 10, "", NULL, 0);
        campus_add_spot(&d, "C", SPOT_GATE, 200, 10, "", NULL, 0);
        campus_add_spot(&d, "D", SPOT_GATE, 100, 200, "", NULL, 0);
        campus_add_road(&d, 1, 2, 10.0, 0.1, NULL, 0);
        campus_add_road(&d, 2, 3, 10.0, 0.1, NULL, 0);
        campus_add_road(&d, 1, 4, 5.0, 0.05, NULL, 0);
        campus_add_road(&d, 4, 3, 5.0, 0.05, NULL, 0);
        graph_build(&d, 0, &g, err, sizeof(err));
        algo_dijkstra(&g, 1, 3, &p, err, sizeof(err));
        CHECK_NEAR(p.weight, 10.0, 1e-9);
        CHECK_INT(p.count, 3);
        CHECK_INT(p.nodes[1], 4);
        graph_free(&g);
    }
}

static void test_floyd(void)
{
    Campus c;
    Graph g;
    double *dist = NULL;
    int *next = NULL;
    char err[256];
    int i, j;

    printf("== Floyd-Warshall ==\n");
    make_line(&c, 6, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_floyd(&g, &dist, &next, err, sizeof(err)), NAV_OK);
    CHECK_NEAR(dist[0], 0.0, 1e-9);
    CHECK_NEAR(dist[0 * 6 + 5], 50.0, 1e-9);
    CHECK_NEAR(dist[5 * 6 + 0], 50.0, 1e-9);
    for (i = 0; i < 6; ++i) {
        for (j = 0; j < 6; ++j) {
            Path p;
            algo_floyd_path(&g, dist, next, i + 1, j + 1, &p);
            CHECK_INT(p.count, abs(i - j) + 1);
            CHECK(path_valid(&g, &p));
            CHECK_NEAR(p.weight, dist[i * 6 + j], 1e-9);
        }
    }
    free(dist);
    free(next);
    graph_free(&g);
}

/* 手工构造有向图（用于负权测试；校园道路是无向的，但算法本身按弧松弛）。 */
static int build_directed(Graph *g, int n, const int *from, const int *to, const double *w,
                          int m)
{
    int *deg;
    int i;
    int acc = 0;

    memset(g, 0, sizeof(*g));
    g->n = n;
    g->n_arcs = m;
    g->n_edges = 0;
    g->off = (int *)calloc((size_t)n + 1, sizeof(int));
    g->to = (int *)calloc((size_t)(m > 0 ? m : 1), sizeof(int));
    g->w = (double *)calloc((size_t)(m > 0 ? m : 1), sizeof(double));
    g->road = (int *)calloc((size_t)(m > 0 ? m : 1), sizeof(int));
    deg = (int *)calloc((size_t)n + 1, sizeof(int));
    if (g->off == NULL || g->to == NULL || g->w == NULL || g->road == NULL || deg == NULL) {
        free(deg);
        graph_free(g);
        return -1;
    }
    for (i = 0; i < m; ++i) {
        ++deg[from[i]];
    }
    g->off[0] = 0;
    for (i = 1; i <= n; ++i) {
        g->off[i] = g->off[i - 1] + deg[i];
    }
    for (i = 1; i <= n; ++i) {
        deg[i] = g->off[i - 1];
    }
    for (i = 0; i < m; ++i) {
        int pos = deg[from[i]]++;
        g->to[pos] = to[i];
        g->w[pos] = w[i];
        g->road[pos] = i + 1;
    }
    free(deg);
    (void)acc;
    return 0;
}

static void test_bellman(void)
{
    Campus c;
    Graph g;
    double dist[NAV_MAX_SPOTS + 1];
    int prev[NAV_MAX_SPOTS + 1];
    char err[256];

    printf("== Bellman-Ford ==\n");
    make_line(&c, 4, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_bellman(&g, 1, dist, prev, err, sizeof(err)), NAV_OK);
    CHECK_NEAR(dist[4], 30.0, 1e-9);
    CHECK_INT(prev[4], 3);
    graph_free(&g);

    /* 有向负权：1→2(4), 1→3(5), 2→3(-2)，1→3 最短路为 1-2-3 = 2 */
    {
        const int af[] = { 1, 1, 2 };
        const int at[] = { 2, 3, 3 };
        const double aw[] = { 4.0, 5.0, -2.0 };
        Graph ng;
        CHECK_INT(build_directed(&ng, 3, af, at, aw, 3), 0);
        CHECK_INT(algo_bellman(&ng, 1, dist, prev, err, sizeof(err)), NAV_OK);
        CHECK_NEAR(dist[3], 2.0, 1e-9);
        CHECK_INT(prev[3], 2);
        graph_free(&ng);
    }

    /* 有向负权回路：1→2(1), 2→3(-2), 3→1(0) => 回路权重 -1 */
    {
        const int af[] = { 1, 2, 3 };
        const int at[] = { 2, 3, 1 };
        const double aw[] = { 1.0, -2.0, 0.0 };
        Graph ng;
        CHECK_INT(build_directed(&ng, 3, af, at, aw, 3), 0);
        CHECK_INT(algo_bellman(&ng, 1, dist, prev, err, sizeof(err)), NAV_ERR_NEGCYCLE);
        CHECK(strstr(err, "负权回路") != NULL);
        graph_free(&ng);
    }
}

static void test_bfs(void)
{
    Campus c;
    Graph g;
    Path p;
    char err[256];

    printf("== BFS ==\n");
    make_cycle(&c, 5, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_bfs(&g, 1, 4, &p, err, sizeof(err)), NAV_OK);
    CHECK_INT(p.count, 3); /* 1-5-4 或 1-2-3-4，跳数 2 优先 */
    CHECK_NEAR(p.weight, 2.0, 1e-9);
    CHECK_INT(p.nodes[1], 5);
    graph_free(&g);
}

static void test_components_and_tarjan(void)
{
    Campus c;
    Graph g;
    int comp[NAV_MAX_SPOTS + 1];
    int is_ap[NAV_MAX_SPOTS + 1];
    int bridges[NAV_MAX_ROADS];
    int nb = 0, k, i;
    char err[256];

    printf("== 连通分量 / 割点 / 桥 ==\n");
    make_line(&c, 4, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    k = algo_components(&g, comp, err, sizeof(err));
    CHECK_INT(k, 1);
    CHECK_INT(algo_critical(&g, is_ap, bridges, &nb, err, sizeof(err)), NAV_OK);
    CHECK_INT(nb, 3);
    CHECK_INT(is_ap[1], 0); /* 端点度为 1，删除后不影响其余部分连通性 */
    CHECK_INT(is_ap[2], 1);
    CHECK_INT(is_ap[3], 1);
    graph_free(&g);

    /* 三角形无桥无割点 */
    make_cycle(&c, 3, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_critical(&g, is_ap, bridges, &nb, err, sizeof(err)), NAV_OK);
    CHECK_INT(nb, 0);
    CHECK_INT(is_ap[1] + is_ap[2] + is_ap[3], 0);
    graph_free(&g);

    /* 星形图：中心为割点，所有边为桥 */
    {
        Campus s;
        campus_init(&s);
        s.width_m = 500.0;
        s.height_m = 500.0;
        campus_add_spot(&s, "C", SPOT_GATE, 250, 250, "", NULL, 0);
        for (i = 1; i <= 4; ++i) {
            char nm[16];
            snprintf(nm, sizeof(nm), "L%d", i);
            campus_add_spot(&s, nm, SPOT_SCENIC, 100.0 + 60.0 * (double)i, 380.0, "", NULL, 0);
            campus_add_road(&s, 1, i + 1, 30.0, 0.4, NULL, 0);
        }
        graph_build(&s, 0, &g, err, sizeof(err));
        CHECK_INT(algo_critical(&g, is_ap, bridges, &nb, err, sizeof(err)), NAV_OK);
        CHECK_INT(nb, 4);
        CHECK_INT(is_ap[1], 1);
        graph_free(&g);
    }

    /* 封路造成不连通 */
    make_line(&c, 4, 10.0);
    CHECK_INT(campus_set_road_open(&c, 2, 0, err, sizeof(err)), NAV_OK);
    graph_build(&c, 0, &g, err, sizeof(err));
    k = algo_components(&g, comp, err, sizeof(err));
    CHECK_INT(k, 2);
    CHECK_INT(comp[1], comp[2]);
    CHECK(comp[3] != comp[1]);
    graph_free(&g);

    /* 真实的桥：删掉它必然让路网分裂（交叉验证） */
    {
        Campus d;
        campus_load_default(&d);
        graph_build(&d, 0, &g, err, sizeof(err));
        CHECK_INT(algo_critical(&g, is_ap, bridges, &nb, err, sizeof(err)), NAV_OK);
        for (i = 0; i < nb; ++i) {
            int comp2[NAV_MAX_SPOTS + 1];
            Graph g2;
            Campus d2 = d;
            int kk;
            campus_set_road_open(&d2, bridges[i], 0, err, sizeof(err));
            graph_build(&d2, 0, &g2, err, sizeof(err));
            kk = algo_components(&g2, comp2, err, sizeof(err));
            CHECK(kk > 1);
            graph_free(&g2);
        }
        graph_free(&g);
    }
}

static void test_mst(void)
{
    Campus c;
    Graph g;
    int eu[NAV_MAX_SPOTS], ev[NAV_MAX_SPOTS];
    int cp = 0, ck = 0;
    double tp, tk;
    char err[256];

    printf("== 最小生成树 ==\n");
    make_cycle(&c, 5, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    tp = algo_prim(&g, eu, ev, &cp);
    tk = algo_kruskal(&g, eu, ev, &ck);
    CHECK_INT(cp, 4);
    CHECK_INT(ck, 4);
    CHECK_NEAR(tp, tk, 1e-6);
    CHECK_NEAR(tp, 40.0, 1e-6); /* 环上最小生成树 = 4 条 10 米边 */
    graph_free(&g);

    /* 不连通图 -> -1 */
    {
        Campus d;
        Graph gd;
        make_line(&d, 3, 10.0);
        campus_add_spot(&d, "孤岛", SPOT_SCENIC, 900.0, 700.0, "", NULL, 0);
        graph_build(&d, 0, &gd, err, sizeof(err));
        CHECK(algo_prim(&gd, eu, ev, &cp) < 0.0);
        CHECK(algo_kruskal(&gd, eu, ev, &ck) < 0.0);
        graph_free(&gd);
    }

    /* 校园图：MST 总长不超过所有道路总长 */
    {
        Campus d;
        Campus dd;
        campus_load_default(&dd);
        d = dd;
        graph_build(&d, 0, &g, err, sizeof(err));
        tp = algo_prim(&g, eu, ev, &cp);
        tk = algo_kruskal(&g, eu, ev, &ck);
        CHECK(tp > 0.0);
        CHECK_NEAR(tp, tk, 1e-6);
        CHECK_INT(cp, 23);
        CHECK(tp <= campus_total_road_length(&d) + 1e-6);
        graph_free(&g);
    }
}

/* 暴力枚举 k 个途经点的全排列，求最优顺序代价。 */
static double tsp_bruteforce(const Graph *g, int start, const int *via, int k, int end)
{
    int perm[8];
    int used[8] = { 0 };
    double best = NAV_INF;
    double dist[NAV_MAX_SPOTS + 1];
    int prev[NAV_MAX_SPOTS + 1];
    char err[128];
    double seg[8][8]; /* seg[i][j]: via[i] -> via[j] */
    double from_start[8];
    double to_end[8];
    int i, j;

    for (i = 0; i < k; ++i) {
        algo_dijkstra_table(g, via[i], dist, prev, NULL, NULL, err, sizeof(err));
        for (j = 0; j < k; ++j) {
            seg[i][j] = dist[via[j]];
        }
        to_end[i] = dist[end];
    }
    algo_dijkstra_table(g, start, dist, prev, NULL, NULL, err, sizeof(err));
    for (i = 0; i < k; ++i) {
        from_start[i] = dist[via[i]];
    }
    (void)perm;
    (void)used;
    /* 递归枚举 */
    {
        int stack[8];
        int depth = 0;
        int idx[8];
        for (i = 0; i < k; ++i) {
            idx[i] = -1;
        }
        /* 用简单的深度优先迭代实现（k<=6） */
        while (depth >= 0) {
            int next = idx[depth] + 1;
            if (next >= k) {
                idx[depth] = -1;
                --depth;
                continue;
            }
            idx[depth] = next;
            stack[depth] = next;
            /* 检查重复 */
            {
                int dup = 0, t;
                for (t = 0; t < depth; ++t) {
                    if (stack[t] == next) {
                        dup = 1;
                    }
                }
                if (dup) {
                    continue;
                }
            }
            if (depth == k - 1) {
                double total = from_start[stack[0]];
                int t;
                for (t = 0; t + 1 < k; ++t) {
                    total += seg[stack[t]][stack[t + 1]];
                }
                total += to_end[stack[k - 1]];
                if (total < best) {
                    best = total;
                }
            } else {
                ++depth;
                idx[depth] = -1;
            }
        }
    }
    return best;
}

static void test_tsp(void)
{
    Campus c;
    Graph g;
    Path p;
    int order[NAV_MAX_SPOTS];
    char err[256];

    printf("== 多点巡游 ==\n");
    make_cycle(&c, 7, 10.0);
    graph_build(&c, 0, &g, err, sizeof(err));
    {
        int via[4] = { 3, 5, 6, 7 };
        double brute = tsp_bruteforce(&g, 1, via, 4, 2);
        CHECK_INT(algo_tsp(&g, 1, via, 4, 2, order, &p, err, sizeof(err)), NAV_OK);
        CHECK_NEAR(p.weight, brute, 1e-6);
        CHECK(path_valid(&g, &p));
        CHECK_INT(p.nodes[0], 1);
        CHECK_INT(p.nodes[p.count - 1], 2);
        {
            int i, seen = 0;
            for (i = 0; i < p.count; ++i) {
                if (p.nodes[i] == 3 || p.nodes[i] == 5 || p.nodes[i] == 6 || p.nodes[i] == 7) {
                    ++seen;
                }
            }
            CHECK(seen >= 4); /* 每个途经点至少出现一次 */
        }
    }
    /* k = 0 退化为最短路：环上 1→6 走 1-7-6 更短 */
    {
        Path q;
        CHECK_INT(algo_tsp(&g, 1, NULL, 0, 6, order, &q, err, sizeof(err)), NAV_OK);
        CHECK(path_valid(&g, &q));
        CHECK_INT(q.count, 3);
        CHECK_NEAR(q.weight, 30.0, 1e-6);
    }
    /* 非法参数 */
    {
        int via[1] = { 1 };
        CHECK_INT(algo_tsp(&g, 1, via, 1, 2, order, &p, err, sizeof(err)), NAV_ERR_ARG);
        via[0] = 99;
        CHECK_INT(algo_tsp(&g, 1, via, 1, 2, order, &p, err, sizeof(err)), NAV_ERR_RANGE);
    }
    /* 大规模启发式路径（k=20）也应给出合法路径 */
    {
        Campus big;
        Graph gb;
        int via[20];
        int i;
        make_cycle(&big, 24, 10.0);
        graph_build(&big, 0, &gb, err, sizeof(err));
        for (i = 0; i < 20; ++i) {
            via[i] = 2 + i;
        }
        CHECK_INT(algo_tsp(&gb, 1, via, 20, 24, order, &p, err, sizeof(err)), NAV_OK);
        CHECK(path_valid(&gb, &p));
        graph_free(&gb);
    }
    graph_free(&g);
}

static void test_nearest(void)
{
    Campus c;
    Graph g;
    Path p;
    int best = 0;
    char err[256];

    printf("== 最近设施 ==\n");
    campus_load_default(&c);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_nearest_type(&g, &c, 16, SPOT_CANTEEN, &best, &p, err, sizeof(err)), NAV_OK);
    CHECK_INT(best, 14); /* 宿舍A区 最近的是 学生一食堂 */
    CHECK(path_valid(&g, &p));
    CHECK_INT(p.nodes[0], 16);
    CHECK_INT(p.nodes[p.count - 1], 14);

    /* 自身就是目标类型 */
    best = 0;
    CHECK_INT(algo_nearest_type(&g, &c, 14, SPOT_CANTEEN, &best, &p, err, sizeof(err)), NAV_OK);
    CHECK_INT(best, 14);
    CHECK_INT(p.count, 1);

    /* 边界参数 */
    CHECK_INT(algo_nearest_type(&g, &c, 0, SPOT_CANTEEN, &best, &p, err, sizeof(err)),
              NAV_ERR_RANGE);
    graph_free(&g);
}

/* ---------------- 5. 随机交叉验证 ---------------- */

static void test_cross_validation(void)
{
    int round;
    int mismatches = 0;

    printf("== 随机图交叉验证（30 组）==\n");
    for (round = 0; round < 30; ++round) {
        Campus c;
        Graph g;
        double *fd = NULL;
        int *fn = NULL;
        char err[256];
        int n = 8 + (round % 12);
        int s, t;

        make_random(&c, n, 1000u + (unsigned int)round);
        CHECK_INT(graph_build(&c, round % 2, &g, err, sizeof(err)), NAV_OK);
        CHECK_INT(algo_floyd(&g, &fd, &fn, err, sizeof(err)), NAV_OK);
        for (s = 1; s <= n; ++s) {
            double dd[NAV_MAX_SPOTS + 1];
            int pp[NAV_MAX_SPOTS + 1];
            double bf[NAV_MAX_SPOTS + 1];
            int bfp[NAV_MAX_SPOTS + 1];
            CHECK_INT(algo_dijkstra_table(&g, s, dd, pp, NULL, NULL, err, sizeof(err)), NAV_OK);
            CHECK_INT(algo_bellman(&g, s, bf, bfp, err, sizeof(err)), NAV_OK);
            for (t = 1; t <= n; ++t) {
                double d = fd[(size_t)(s - 1) * (size_t)n + (size_t)(t - 1)];
                if (fabs(d - dd[t]) > 1e-6) {
                    ++mismatches;
                }
                if (fabs(d - bf[t]) > 1e-6) {
                    ++mismatches;
                }
            }
        }
        free(fd);
        free(fn);
        graph_free(&g);
    }
    CHECK_INT(mismatches, 0);
}

static void test_campus_consistency(void)
{
    Campus c;
    Graph g;
    double *fd = NULL;
    int *fn = NULL;
    Path p;
    char err[256];

    printf("== 校园图算法一致性 ==\n");
    campus_load_default(&c);
    graph_build(&c, 0, &g, err, sizeof(err));
    CHECK_INT(algo_floyd(&g, &fd, &fn, err, sizeof(err)), NAV_OK);
    {
        int a, b;
        for (a = 1; a <= c.n_spots; a += 3) {
            for (b = 1; b <= c.n_spots; b += 2) {
                Path q;
                algo_dijkstra(&g, a, b, &p, err, sizeof(err));
                algo_floyd_path(&g, fd, fn, a, b, &q);
                CHECK_NEAR(p.weight, q.weight, 1e-6);
                CHECK(path_valid(&g, &p));
                CHECK(path_valid(&g, &q));
            }
        }
    }
    free(fd);
    free(fn);
    graph_free(&g);
}

/* ---------------- 6. 性能冒烟 ---------------- */

static void test_performance(void)
{
    Campus c;
    Graph g;
    Path p;
    char err[256];
    clock_t t0, t1;
    double ms;

    printf("== 性能冒烟 ==\n");
    make_random(&c, 128, 777u);
    CHECK_INT(graph_build(&c, 0, &g, err, sizeof(err)), NAV_OK);
    t0 = platform_now_ms();
    {
        int rep;
        for (rep = 0; rep < 20; ++rep) {
            CHECK_INT(algo_dijkstra(&g, 1, 128, &p, err, sizeof(err)), NAV_OK);
        }
    }
    t1 = platform_now_ms();
    ms = (t1 - t0) / 20.0;
    printf("  Dijkstra(128 节点): %.4f ms\n", ms);
    CHECK(ms < 50.0);

    t0 = platform_now_ms();
    {
        double *fd = NULL;
        int *fn = NULL;
        CHECK_INT(algo_floyd(&g, &fd, &fn, err, sizeof(err)), NAV_OK);
        free(fd);
        free(fn);
    }
    t1 = platform_now_ms();
    ms = t1 - t0;
    printf("  Floyd(128 节点): %.4f ms\n", ms);
    CHECK(ms < 2000.0);
    graph_free(&g);
}

/* ---------------- 7. 应用集成 ---------------- */

static void test_app(void)
{
    App app;
    char err[256];

    printf("== 应用集成 ==\n");
    if (platform_file_exists("data/campus/spots.csv")) {
        CHECK_INT(app_init(&app, "data/campus", err, sizeof(err)), NAV_OK);
        CHECK_INT(app.campus.n_spots, 24);
        CHECK_INT(app.graph.n, 24);
        CHECK_INT(app.by_time, 0);
        CHECK_INT(app_set_criterion(&app, 1, err, sizeof(err)), NAV_OK);
        CHECK_INT(app.by_time, 1);
        CHECK_NEAR(app.graph.w[0], app.campus.roads[0].walk_min, 1e-9);
        CHECK_INT(app_set_criterion(&app, 0, err, sizeof(err)), NAV_OK);
        app.campus.spots[0].x = 901.0;
        app.dirty = 1;
        CHECK_INT(app_save(&app, err, sizeof(err)), NAV_OK);
        CHECK_INT(app.dirty, 0);
        CHECK_INT(app_reload(&app, err, sizeof(err)), NAV_OK);
        CHECK_NEAR(app.campus.spots[0].x, 901.0, 0.06);
        /* 恢复原始数据，避免污染仓库 */
        campus_load_default(&app.campus);
        CHECK_INT(app_save(&app, err, sizeof(err)), NAV_OK);
        app_free(&app);
    } else {
        printf("  (skip: data/campus 不存在)\n");
    }
}

static void run_group(const char *name, void (*fn)(void))
{
    int before = g_checks;
    int fails_before = g_fails;
    fn();
    printf("[group] %s: %d checks, %d failed\n", name, g_checks - before, g_fails - fails_before);
}

int main(void)
{
    printf("校园导航系统 · 自动化测试\n");
    printf("==========================================\n");

    run_group("tools", test_crc_and_display);
    run_group("dataset", test_default_dataset);
    run_group("csv", test_csv_roundtrip);
    run_group("csv-errors", test_csv_errors);
    run_group("binary", test_binary_roundtrip);
    run_group("binary-corruption", test_binary_corruption);
    run_group("edit", test_edit_operations);
    run_group("lookup", test_find_spot);
    run_group("graph", test_graph_build);
    run_group("dijkstra", test_dijkstra_basics);
    run_group("floyd", test_floyd);
    run_group("bellman", test_bellman);
    run_group("bfs", test_bfs);
    run_group("tarjan", test_components_and_tarjan);
    run_group("mst", test_mst);
    run_group("tsp", test_tsp);
    run_group("nearest", test_nearest);
    run_group("cross-validation", test_cross_validation);
    run_group("consistency", test_campus_consistency);
    run_group("performance", test_performance);
    run_group("app", test_app);

    printf("==========================================\n");
    printf("检查项 %d 个，失败 %d 个\n", g_checks, g_fails);
    if (g_fails == 0) {
        printf("全部通过。\n");
        return 0;
    }
    printf("存在失败项。\n");
    return 1;
}
