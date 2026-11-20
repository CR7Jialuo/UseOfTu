/* algo.c —— 校园导航系统的图算法实现。
 *
 * 已实现：
 *   - Dijkstra（二叉堆优化，单源最短路，非负权）
 *   - Floyd-Warshall（全源最短路 + next 矩阵重建路径）
 *   - Bellman-Ford（支持负权，可检测负权回路）
 *   - BFS（最少路段数意义下的最短路）
 *   - 连通分量、Tarjan 割点与桥
 *   - Prim / Kruskal 最小生成树
 *   - 多点巡游：Held-Karp 精确解（k<=12）+ 最近邻 2-opt 启发式
 *   - 最近设施查询
 */
#include "algo.h"
#include "err.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NAV_EPS 1.0e-9

void path_clear(Path *p)
{
    if (p == NULL) {
        return;
    }
    p->count = 0;
    p->weight = NAV_INF;
    p->settled = 0;
    p->relaxed = 0;
}

/* ---------------- 二叉最小堆 ---------------- */

typedef struct {
    double key;
    int node;
} HeapNode;

typedef struct {
    HeapNode *a;
    int n;
    int cap;
} Heap;

static int heap_init(Heap *h, int cap)
{
    h->n = 0;
    h->cap = cap > 0 ? cap : 1;
    h->a = (HeapNode *)malloc((size_t)h->cap * sizeof(HeapNode));
    return (h->a != NULL) ? 0 : -1;
}

static void heap_free(Heap *h)
{
    free(h->a);
    h->a = NULL;
    h->n = 0;
    h->cap = 0;
}

static void heap_push(Heap *h, double key, int node)
{
    int i;
    if (h->n >= h->cap) {
        return; /* 容量按弧数预留，理论上不会发生 */
    }
    i = h->n++;
    h->a[i].key = key;
    h->a[i].node = node;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->a[parent].key <= h->a[i].key) {
            break;
        }
        {
            HeapNode tmp = h->a[parent];
            h->a[parent] = h->a[i];
            h->a[i] = tmp;
        }
        i = parent;
    }
}

static int heap_pop(Heap *h, double *key, int *node)
{
    int i = 0;
    if (h->n == 0) {
        return 0;
    }
    *key = h->a[0].key;
    *node = h->a[0].node;
    h->a[0] = h->a[--h->n];
    for (;;) {
        int l = 2 * i + 1;
        int r = l + 1;
        int smallest = i;
        if (l < h->n && h->a[l].key < h->a[smallest].key) {
            smallest = l;
        }
        if (r < h->n && h->a[r].key < h->a[smallest].key) {
            smallest = r;
        }
        if (smallest == i) {
            break;
        }
        {
            HeapNode tmp = h->a[i];
            h->a[i] = h->a[smallest];
            h->a[smallest] = tmp;
        }
        i = smallest;
    }
    return 1;
}

/* ---------------- 路径重建 ---------------- */

static void path_from_prev(const Graph *g, const int *prev, const double *dist, int src, int dst,
                           Path *out)
{
    int stack[NAV_MAX_SPOTS];
    int top = 0;
    int cur = dst;

    /* 保留调用方写入的算法统计量（settled/relaxed），只重置路径本身。 */
    out->count = 0;
    out->weight = NAV_INF;
    if (dst < 1 || dst > g->n || dist[dst] >= NAV_INF / 2.0) {
        return;
    }
    while (cur != 0 && top < NAV_MAX_SPOTS) {
        stack[top++] = cur;
        if (cur == src) {
            break;
        }
        cur = prev[cur];
    }
    if (top == 0 || stack[top - 1] != src) {
        return;
    }
    out->count = top;
    {
        int i;
        for (i = 0; i < top; ++i) {
            out->nodes[i] = stack[top - 1 - i];
        }
    }
    out->weight = dist[dst];
}

/* ---------------- Dijkstra ---------------- */

static int dijkstra_run(const Graph *g, int src, int dst, double *dist, int *prev,
                        long *settled_out, long *relaxed_out)
{
    Heap heap;
    int i;
    long settled = 0, relaxed = 0;
    int stop_at = (dst >= 1 && dst <= g->n) ? dst : 0;

    for (i = 0; i <= g->n; ++i) {
        dist[i] = NAV_INF;
        prev[i] = 0;
    }
    if (heap_init(&heap, g->n_arcs + 2) != 0) {
        return NAV_ERR_NOMEM;
    }
    dist[src] = 0.0;
    heap_push(&heap, 0.0, src);

    for (;;) {
        double key;
        int u;
        int arc;
        if (!heap_pop(&heap, &key, &u)) {
            break;
        }
        if (key > dist[u] + NAV_EPS) {
            continue; /* 过期堆项 */
        }
        ++settled;
        if (stop_at != 0 && u == stop_at) {
            break;
        }
        for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
            int v = g->to[arc];
            double nd = dist[u] + g->w[arc];
            ++relaxed;
            if (nd < dist[v] - NAV_EPS) {
                dist[v] = nd;
                prev[v] = u;
                heap_push(&heap, nd, v);
            }
        }
    }
    heap_free(&heap);
    if (settled_out != NULL) {
        *settled_out = settled;
    }
    if (relaxed_out != NULL) {
        *relaxed_out = relaxed;
    }
    return NAV_OK;
}

int algo_dijkstra_table(const Graph *g, int src, double *dist, int *prev, long *settled,
                        long *relaxed, char *err, size_t cap)
{
    if (g == NULL || dist == NULL || prev == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (src < 1 || src > g->n) {
        nav_err_set(err, cap, "起点编号 %d 越界（1..%d）", src, g->n);
        return NAV_ERR_RANGE;
    }
    return dijkstra_run(g, src, 0, dist, prev, settled, relaxed);
}

int algo_dijkstra(const Graph *g, int src, int dst, Path *out, char *err, size_t cap)
{
    double dist[NAV_MAX_SPOTS + 1];
    int prev[NAV_MAX_SPOTS + 1];
    int rc;

    if (g == NULL || out == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (src < 1 || src > g->n || dst < 1 || dst > g->n) {
        nav_err_set(err, cap, "起点或终点编号越界（1..%d）", g->n);
        return NAV_ERR_RANGE;
    }
    path_clear(out);
    rc = dijkstra_run(g, src, dst, dist, prev, &out->settled, &out->relaxed);
    if (rc != NAV_OK) {
        nav_err_set(err, cap, "Dijkstra 执行失败");
        return rc;
    }
    path_from_prev(g, prev, dist, src, dst, out);
    return NAV_OK;
}

/* ---------------- Floyd-Warshall ---------------- */

int algo_floyd(const Graph *g, double **dist, int **next, char *err, size_t cap)
{
    int n, i, j, k;
    double *d;
    int *nx;

    if (g == NULL || dist == NULL || next == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    n = g->n;
    d = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    nx = (int *)calloc((size_t)n * (size_t)n, sizeof(int));
    if (d == NULL || nx == NULL) {
        free(d);
        free(nx);
        nav_err_set(err, cap, "内存不足（Floyd 矩阵 %d x %d）", n, n);
        return NAV_ERR_NOMEM;
    }
    for (i = 0; i < n; ++i) {
        for (j = 0; j < n; ++j) {
            d[(size_t)i * (size_t)n + (size_t)j] = (i == j) ? 0.0 : NAV_INF;
        }
    }
    for (i = 1; i <= n; ++i) {
        int arc;
        for (arc = g->off[i - 1]; arc < g->off[i]; ++arc) {
            int v = g->to[arc];
            size_t idx = (size_t)(i - 1) * (size_t)n + (size_t)(v - 1);
            if (g->w[arc] < d[idx]) {
                d[idx] = g->w[arc];
                nx[idx] = v;
            }
        }
    }
    for (k = 0; k < n; ++k) {
        for (i = 0; i < n; ++i) {
            double dik = d[(size_t)i * (size_t)n + (size_t)k];
            size_t row_i = (size_t)i * (size_t)n;
            size_t row_k = (size_t)k * (size_t)n;
            if (dik >= NAV_INF / 2.0) {
                continue;
            }
            for (j = 0; j < n; ++j) {
                double alt = dik + d[row_k + (size_t)j];
                if (alt < d[row_i + (size_t)j] - NAV_EPS) {
                    d[row_i + (size_t)j] = alt;
                    /* 路径 i..k..j 的第一跳，与 i..k 的第一跳相同 */
                    nx[row_i + (size_t)j] = nx[row_i + (size_t)k];
                }
            }
        }
    }
    *dist = d;
    *next = nx;
    return NAV_OK;
}

int algo_floyd_path(const Graph *g, const double *dist, const int *next, int src, int dst,
                    Path *out)
{
    int n, cur, guard;
    size_t base;

    if (g == NULL || dist == NULL || next == NULL || out == NULL) {
        return NAV_ERR_ARG;
    }
    n = g->n;
    path_clear(out);
    if (src < 1 || src > n || dst < 1 || dst > n) {
        return NAV_ERR_RANGE;
    }
    base = (size_t)(src - 1) * (size_t)n;
    if (dist[base + (size_t)(dst - 1)] >= NAV_INF / 2.0) {
        return NAV_OK; /* 不连通 */
    }
    out->nodes[0] = src;
    out->count = 1;
    out->weight = dist[base + (size_t)(dst - 1)];
    cur = src;
    for (guard = 0; guard < n; ++guard) {
        int nxt;
        if (cur == dst) {
            return NAV_OK;
        }
        nxt = next[(size_t)(cur - 1) * (size_t)n + (size_t)(dst - 1)];
        if (nxt < 1 || nxt > n) {
            out->count = 0;
            out->weight = NAV_INF;
            return NAV_ERR_FMT;
        }
        out->nodes[out->count++] = nxt;
        cur = nxt;
    }
    out->count = 0;
    out->weight = NAV_INF;
    return NAV_ERR_FMT;
}

/* ---------------- Bellman-Ford ---------------- */

int algo_bellman(const Graph *g, int src, double *dist, int *prev, char *err, size_t cap)
{
    int i, u, pass;

    if (g == NULL || dist == NULL || prev == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (src < 1 || src > g->n) {
        nav_err_set(err, cap, "起点编号 %d 越界（1..%d）", src, g->n);
        return NAV_ERR_RANGE;
    }
    for (i = 0; i <= g->n; ++i) {
        dist[i] = NAV_INF;
        prev[i] = 0;
    }
    dist[src] = 0.0;

    for (pass = 0; pass < g->n - 1; ++pass) {
        int changed = 0;
        for (u = 1; u <= g->n; ++u) {
            int arc;
            if (dist[u] >= NAV_INF / 2.0) {
                continue;
            }
            for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
                int v = g->to[arc];
                double nd = dist[u] + g->w[arc];
                if (nd < dist[v] - NAV_EPS) {
                    dist[v] = nd;
                    prev[v] = u;
                    changed = 1;
                }
            }
        }
        if (!changed) {
            break;
        }
    }
    for (u = 1; u <= g->n; ++u) {
        int arc;
        if (dist[u] >= NAV_INF / 2.0) {
            continue;
        }
        for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
            int v = g->to[arc];
            if (dist[u] + g->w[arc] < dist[v] - NAV_EPS) {
                nav_err_set(err, cap, "图中存在负权回路，最短路径无定义");
                return NAV_ERR_NEGCYCLE;
            }
        }
    }
    return NAV_OK;
}

/* ---------------- BFS ---------------- */

int algo_bfs(const Graph *g, int src, int dst, Path *out, char *err, size_t cap)
{
    int *dist;
    int *prev;
    int *queue;
    int head = 0, tail = 0;
    int i;

    if (g == NULL || out == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (src < 1 || src > g->n || dst < 1 || dst > g->n) {
        nav_err_set(err, cap, "起点或终点编号越界（1..%d）", g->n);
        return NAV_ERR_RANGE;
    }
    dist = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    prev = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    queue = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    if (dist == NULL || prev == NULL || queue == NULL) {
        free(dist);
        free(prev);
        free(queue);
        nav_err_set(err, cap, "内存不足（BFS）");
        return NAV_ERR_NOMEM;
    }
    for (i = 0; i <= g->n; ++i) {
        dist[i] = -1;
        prev[i] = 0;
    }
    dist[src] = 0;
    queue[tail++] = src;
    while (head < tail) {
        int u = queue[head++];
        int arc;
        for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
            int v = g->to[arc];
            if (dist[v] < 0) {
                dist[v] = dist[u] + 1;
                prev[v] = u;
                queue[tail++] = v;
            }
        }
    }
    path_clear(out);
    if (dist[dst] >= 0) {
        int stack[NAV_MAX_SPOTS];
        int top = 0;
        int cur = dst;
        while (cur != 0 && top < NAV_MAX_SPOTS) {
            stack[top++] = cur;
            if (cur == src) {
                break;
            }
            cur = prev[cur];
        }
        if (top > 0 && stack[top - 1] == src) {
            out->count = top;
            for (i = 0; i < top; ++i) {
                out->nodes[i] = stack[top - 1 - i];
            }
            out->weight = (double)dist[dst];
        }
    }
    free(dist);
    free(prev);
    free(queue);
    return NAV_OK;
}

/* ---------------- 连通分量 ---------------- */

int algo_components(const Graph *g, int *comp, char *err, size_t cap)
{
    int i, k = 0;
    int *queue;

    if (g == NULL || comp == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    queue = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    if (queue == NULL) {
        nav_err_set(err, cap, "内存不足（连通分量）");
        return NAV_ERR_NOMEM;
    }
    for (i = 0; i <= g->n; ++i) {
        comp[i] = 0;
    }
    for (i = 1; i <= g->n; ++i) {
        if (comp[i] != 0) {
            continue;
        }
        {
            int head = 0, tail = 0;
            ++k;
            comp[i] = k;
            queue[tail++] = i;
            while (head < tail) {
                int u = queue[head++];
                int arc;
                for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
                    int v = g->to[arc];
                    if (comp[v] == 0) {
                        comp[v] = k;
                        queue[tail++] = v;
                    }
                }
            }
        }
    }
    free(queue);
    return k;
}

/* ---------------- Tarjan：割点与桥 ---------------- */

typedef struct {
    const Graph *g;
    int *disc;
    int *low;
    int *is_ap;
    int *bridge;
    int n_bridges;
    int timer;
    int n;
} TarjanCtx;

static void tarjan_dfs(TarjanCtx *ctx, int u, int parent_node, int parent_road)
{
    int arc;
    int children = 0;

    ctx->disc[u] = ctx->low[u] = ++ctx->timer;
    for (arc = ctx->g->off[u - 1]; arc < ctx->g->off[u]; ++arc) {
        int v = ctx->g->to[arc];
        int road = ctx->g->road[arc];
        if (road == parent_road) {
            continue;
        }
        if (ctx->disc[v] == 0) {
            ++children;
            tarjan_dfs(ctx, v, u, road);
            if (ctx->low[v] < ctx->low[u]) {
                ctx->low[u] = ctx->low[v];
            }
            if (ctx->low[v] > ctx->disc[u]) {
                ctx->bridge[ctx->n_bridges++] = road;
            }
            if (parent_node != 0 && ctx->low[v] >= ctx->disc[u]) {
                ctx->is_ap[u] = 1;
            }
        } else if (ctx->disc[v] < ctx->low[u]) {
            ctx->low[u] = ctx->disc[v];
        }
    }
    if (parent_node == 0 && children > 1) {
        ctx->is_ap[u] = 1;
    }
}

int algo_critical(const Graph *g, int *is_ap, int *bridge_roads, int *n_bridges, char *err,
                  size_t cap)
{
    TarjanCtx ctx;
    int i;

    if (g == NULL || is_ap == NULL || bridge_roads == NULL || n_bridges == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.g = g;
    ctx.n = g->n;
    ctx.disc = (int *)calloc((size_t)g->n + 1, sizeof(int));
    ctx.low = (int *)calloc((size_t)g->n + 1, sizeof(int));
    ctx.is_ap = is_ap;
    ctx.bridge = bridge_roads;
    if (ctx.disc == NULL || ctx.low == NULL) {
        free(ctx.disc);
        free(ctx.low);
        nav_err_set(err, cap, "内存不足（Tarjan）");
        return NAV_ERR_NOMEM;
    }
    for (i = 0; i <= g->n; ++i) {
        is_ap[i] = 0;
    }
    for (i = 1; i <= g->n; ++i) {
        if (ctx.disc[i] == 0) {
            tarjan_dfs(&ctx, i, 0, 0);
        }
    }
    *n_bridges = ctx.n_bridges;
    free(ctx.disc);
    free(ctx.low);
    return NAV_OK;
}

/* ---------------- 最小生成树 ---------------- */

double algo_prim(const Graph *g, int *eu, int *ev, int *cnt)
{
    double *best;
    int *from;
    int *used;
    int i, iter;
    double total = 0.0;
    int count = 0;

    if (g == NULL || eu == NULL || ev == NULL || cnt == NULL || g->n < 1) {
        return -1.0;
    }
    *cnt = 0;
    if (g->n == 1) {
        return 0.0;
    }
    best = (double *)malloc(((size_t)g->n + 1) * sizeof(double));
    from = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    used = (int *)calloc((size_t)g->n + 1, sizeof(int));
    if (best == NULL || from == NULL || used == NULL) {
        free(best);
        free(from);
        free(used);
        return -1.0;
    }
    for (i = 0; i <= g->n; ++i) {
        best[i] = NAV_INF;
        from[i] = 0;
    }
    best[1] = 0.0;
    for (iter = 0; iter < g->n; ++iter) {
        int u = 0;
        int arc;
        double bw = NAV_INF;
        for (i = 1; i <= g->n; ++i) {
            if (!used[i] && best[i] < bw) {
                bw = best[i];
                u = i;
            }
        }
        if (u == 0) {
            break; /* 不连通 */
        }
        used[u] = 1;
        if (from[u] != 0) {
            total += bw;
            eu[count] = from[u];
            ev[count] = u;
            ++count;
        }
        for (arc = g->off[u - 1]; arc < g->off[u]; ++arc) {
            int v = g->to[arc];
            if (!used[v] && g->w[arc] < best[v]) {
                best[v] = g->w[arc];
                from[v] = u;
            }
        }
    }
    free(best);
    free(from);
    free(used);
    if (count != g->n - 1) {
        return -1.0;
    }
    *cnt = count;
    return total;
}

typedef struct {
    double w;
    int a, b;
    int id;
} SortEdge;

static int cmp_edge(const void *pa, const void *pb)
{
    const SortEdge *x = (const SortEdge *)pa;
    const SortEdge *y = (const SortEdge *)pb;
    if (x->w < y->w) {
        return -1;
    }
    if (x->w > y->w) {
        return 1;
    }
    return 0;
}

static int uf_find(int *parent, int x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

double algo_kruskal(const Graph *g, int *eu, int *ev, int *cnt)
{
    SortEdge *edges;
    int *parent;
    int i;
    int count = 0;
    double total = 0.0;

    if (g == NULL || eu == NULL || ev == NULL || cnt == NULL || g->n < 1) {
        return -1.0;
    }
    *cnt = 0;
    if (g->n == 1) {
        return 0.0;
    }
    edges = (SortEdge *)malloc((size_t)(g->n_edges > 0 ? g->n_edges : 1) * sizeof(SortEdge));
    parent = (int *)malloc(((size_t)g->n + 1) * sizeof(int));
    if (edges == NULL || parent == NULL) {
        free(edges);
        free(parent);
        return -1.0;
    }
    for (i = 0; i < g->n_edges; ++i) {
        edges[i].w = g->ew[i];
        edges[i].a = g->eu[i];
        edges[i].b = g->ev[i];
        edges[i].id = g->eid[i];
    }
    qsort(edges, (size_t)g->n_edges, sizeof(SortEdge), cmp_edge);
    for (i = 1; i <= g->n; ++i) {
        parent[i] = i;
    }
    for (i = 0; i < g->n_edges && count < g->n - 1; ++i) {
        int ra = uf_find(parent, edges[i].a);
        int rb = uf_find(parent, edges[i].b);
        if (ra == rb) {
            continue;
        }
        parent[ra] = rb;
        eu[count] = edges[i].a;
        ev[count] = edges[i].b;
        ++count;
        total += edges[i].w;
    }
    free(edges);
    free(parent);
    if (count != g->n - 1) {
        return -1.0;
    }
    *cnt = count;
    return total;
}

/* ---------------- 多点巡游（TSP 变体） ---------------- */

#define TSP_EXACT_LIMIT 12

typedef struct {
    int n;          /* 终端数 m = k + 2 */
    double *dist;   /* m*m */
    int *prev_all;  /* m*(n+1) */
    const Graph *g;
} TspCost;

static void tsp_cost_free(TspCost *c)
{
    free(c->dist);
    free(c->prev_all);
}

static int tsp_cost_build(const Graph *g, const int *term, int m, TspCost *c, char *err,
                          size_t cap)
{
    int i, t;
    double *dist;

    c->g = g;
    c->n = m;
    c->dist = (double *)malloc((size_t)m * (size_t)m * sizeof(double));
    c->prev_all = (int *)malloc((size_t)m * ((size_t)g->n + 1) * sizeof(int));
    dist = (double *)malloc(((size_t)g->n + 1) * sizeof(double));
    if (c->dist == NULL || c->prev_all == NULL || dist == NULL) {
        free(dist);
        tsp_cost_free(c);
        nav_err_set(err, cap, "内存不足（巡游代价矩阵）");
        return NAV_ERR_NOMEM;
    }
    for (t = 0; t < m; ++t) {
        int *prev = c->prev_all + (size_t)t * ((size_t)g->n + 1);
        if (algo_dijkstra_table(g, term[t], dist, prev, NULL, NULL, err, cap) != NAV_OK) {
            free(dist);
            tsp_cost_free(c);
            return NAV_ERR_ARG;
        }
        for (i = 0; i < m; ++i) {
            c->dist[(size_t)t * (size_t)m + (size_t)i] = dist[term[i]];
        }
    }
    free(dist);
    return NAV_OK;
}

static double tsp_segment_cost(const TspCost *c, int a, int b)
{
    return c->dist[(size_t)a * (size_t)c->n + (size_t)b];
}

/* 精确解：Held-Karp 动态规划，k 为途经点数。 */
static int tsp_exact(const TspCost *c, int k, int *order)
{
    int total_masks = 1 << k;
    double *dp;
    int *parent;
    int mask, i, j;
    double best = NAV_INF;
    int best_last = -1;

    dp = (double *)malloc((size_t)total_masks * (size_t)k * sizeof(double));
    parent = (int *)malloc((size_t)total_masks * (size_t)k * sizeof(int));
    if (dp == NULL || parent == NULL) {
        free(dp);
        free(parent);
        return NAV_ERR_NOMEM;
    }
    for (mask = 0; mask < total_masks; ++mask) {
        for (i = 0; i < k; ++i) {
            dp[(size_t)mask * (size_t)k + (size_t)i] = NAV_INF;
            parent[(size_t)mask * (size_t)k + (size_t)i] = -1;
        }
    }
    for (i = 0; i < k; ++i) {
        dp[(size_t)(1 << i) * (size_t)k + (size_t)i] = tsp_segment_cost(c, 0, i + 1);
    }
    for (mask = 1; mask < total_masks; ++mask) {
        for (i = 0; i < k; ++i) {
            double base;
            if (!(mask & (1 << i))) {
                continue;
            }
            base = dp[(size_t)mask * (size_t)k + (size_t)i];
            if (base >= NAV_INF / 2.0) {
                continue;
            }
            for (j = 0; j < k; ++j) {
                int next_mask;
                double cand;
                if (mask & (1 << j)) {
                    continue;
                }
                next_mask = mask | (1 << j);
                cand = base + tsp_segment_cost(c, i + 1, j + 1);
                if (cand < dp[(size_t)next_mask * (size_t)k + (size_t)j] - NAV_EPS) {
                    dp[(size_t)next_mask * (size_t)k + (size_t)j] = cand;
                    parent[(size_t)next_mask * (size_t)k + (size_t)j] = i;
                }
            }
        }
    }
    for (i = 0; i < k; ++i) {
        double cand = dp[(size_t)(total_masks - 1) * (size_t)k + (size_t)i] +
                      tsp_segment_cost(c, i + 1, c->n - 1);
        if (cand < best) {
            best = cand;
            best_last = i;
        }
    }
    if (best_last < 0) {
        free(dp);
        free(parent);
        return NAV_ERR_ARG;
    }
    {
        int mask2 = total_masks - 1;
        int cur = best_last;
        int pos = k - 1;
        while (cur >= 0 && pos >= 0) {
            order[pos--] = cur;
            {
                int prev = parent[(size_t)mask2 * (size_t)k + (size_t)cur];
                mask2 &= ~(1 << cur);
                cur = prev;
            }
        }
        if (pos != -1) {
            free(dp);
            free(parent);
            return NAV_ERR_FMT;
        }
    }
    free(dp);
    free(parent);
    return NAV_OK;
}

/* 启发式：最近邻构造 + 2-opt 改进（order 中为 via 下标 0..k-1）。 */
static void tsp_heuristic(const TspCost *c, int k, int *order)
{
    int *visited;
    int i, pos;

    visited = (int *)calloc((size_t)k, sizeof(int));
    if (visited == NULL) {
        for (i = 0; i < k; ++i) {
            order[i] = i;
        }
        return;
    }
    {
        int cur = 0;
        for (pos = 0; pos < k; ++pos) {
            int best_idx = -1;
            double best_w = NAV_INF;
            for (i = 0; i < k; ++i) {
                double w;
                if (visited[i]) {
                    continue;
                }
                w = tsp_segment_cost(c, cur, i + 1);
                if (w < best_w) {
                    best_w = w;
                    best_idx = i;
                }
            }
            if (best_idx < 0) {
                break;
            }
            visited[best_idx] = 1;
            order[pos] = best_idx;
            cur = best_idx + 1;
        }
    }
    free(visited);

    /* 2-opt：固定首尾，反转中间若干段。 */
    if (k >= 3) {
        int improved = 1;
        int guard = 0;
        while (improved && guard++ < 64) {
            int a, b;
            improved = 0;
            for (a = 0; a < k - 1; ++a) {
                for (b = a + 1; b < k; ++b) {
                    int prev_a = (a == 0) ? 0 : order[a - 1] + 1;
                    int next_b = (b == k - 1) ? c->n - 1 : order[b + 1] + 1;
                    double old_w = tsp_segment_cost(c, prev_a, order[a] + 1) +
                                   tsp_segment_cost(c, order[b] + 1, next_b);
                    double new_w = tsp_segment_cost(c, prev_a, order[b] + 1) +
                                   tsp_segment_cost(c, order[a] + 1, next_b);
                    if (new_w < old_w - NAV_EPS) {
                        int x = a, y = b;
                        while (x < y) {
                            int tmp = order[x];
                            order[x] = order[y];
                            order[y] = tmp;
                            ++x;
                            --y;
                        }
                        improved = 1;
                    }
                }
            }
        }
    }
}

static void path_append(Path *dst, const Path *seg, int skip_first)
{
    int start = skip_first ? 1 : 0;
    int i;
    for (i = start; i < seg->count; ++i) {
        if (dst->count < NAV_MAX_SPOTS) {
            dst->nodes[dst->count++] = seg->nodes[i];
        }
    }
}

int algo_tsp(const Graph *g, int start, const int *via, int k, int end, int *order, Path *out,
             char *err, size_t cap)
{
    TspCost cost;
    int *term;
    int m, i;
    int *local_order = NULL;
    double total = 0.0;
    Path seg;

    if (g == NULL || out == NULL || order == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (k < 0 || k > NAV_MAX_SPOTS - 2) {
        nav_err_set(err, cap, "途经点个数 %d 非法", k);
        return NAV_ERR_ARG;
    }
    if (start < 1 || start > g->n || end < 1 || end > g->n) {
        nav_err_set(err, cap, "起点或终点编号越界（1..%d）", g->n);
        return NAV_ERR_RANGE;
    }
    for (i = 0; i < k; ++i) {
        if (via[i] < 1 || via[i] > g->n) {
            nav_err_set(err, cap, "途经点编号 %d 越界", via[i]);
            return NAV_ERR_RANGE;
        }
        if (via[i] == start || via[i] == end) {
            nav_err_set(err, cap, "途经点不能与起点或终点相同");
            return NAV_ERR_ARG;
        }
    }
    if (k == 0) {
        path_clear(out);
        return algo_dijkstra(g, start, end, out, err, cap);
    }

    m = k + 2;
    term = (int *)malloc((size_t)m * sizeof(int));
    local_order = (int *)malloc((size_t)k * sizeof(int));
    if (term == NULL || local_order == NULL) {
        free(term);
        free(local_order);
        nav_err_set(err, cap, "内存不足（巡游）");
        return NAV_ERR_NOMEM;
    }
    term[0] = start;
    for (i = 0; i < k; ++i) {
        term[i + 1] = via[i];
    }
    term[m - 1] = end;

    if (tsp_cost_build(g, term, m, &cost, err, cap) != NAV_OK) {
        free(term);
        free(local_order);
        return NAV_ERR_ARG;
    }
    if (k <= TSP_EXACT_LIMIT) {
        if (tsp_exact(&cost, k, local_order) != NAV_OK) {
            free(term);
            free(local_order);
            tsp_cost_free(&cost);
            nav_err_set(err, cap, "巡游精确求解失败（内存不足或图不连通）");
            return NAV_ERR_NOMEM;
        }
    } else {
        tsp_heuristic(&cost, k, local_order);
    }

    path_clear(out);
    for (i = 0; i < k; ++i) {
        order[i] = term[local_order[i] + 1];
    }

    /* 拼接各段最短路。 */
    {
        int cur = 0;
        for (i = 0; i < k; ++i) {
            int nxt = local_order[i] + 1;
            if (algo_dijkstra(g, term[cur], term[nxt], &seg, err, cap) != NAV_OK) {
                free(term);
                free(local_order);
                tsp_cost_free(&cost);
                return NAV_ERR_ARG;
            }
            if (seg.count == 0) {
                free(term);
                free(local_order);
                tsp_cost_free(&cost);
                path_clear(out);
                return NAV_OK; /* 不连通 */
            }
            path_append(out, &seg, i > 0);
            total += seg.weight;
            cur = nxt;
        }
        if (algo_dijkstra(g, term[cur], term[m - 1], &seg, err, cap) != NAV_OK) {
            free(term);
            free(local_order);
            tsp_cost_free(&cost);
            return NAV_ERR_ARG;
        }
        if (seg.count == 0) {
            free(term);
            free(local_order);
            tsp_cost_free(&cost);
            path_clear(out);
            return NAV_OK;
        }
        path_append(out, &seg, 1);
        total += seg.weight;
    }
    out->weight = total;
    free(term);
    free(local_order);
    tsp_cost_free(&cost);
    return NAV_OK;
}

/* ---------------- 最近设施 ---------------- */

int algo_nearest_type(const Graph *g, const Campus *c, int from, SpotType type, int *best_spot,
                      Path *out, char *err, size_t cap)
{
    double dist[NAV_MAX_SPOTS + 1];
    int prev[NAV_MAX_SPOTS + 1];
    int i;
    int best = 0;
    double best_w = NAV_INF;

    if (g == NULL || c == NULL || best_spot == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (from < 1 || from > g->n) {
        nav_err_set(err, cap, "起点编号 %d 越界（1..%d）", from, g->n);
        return NAV_ERR_RANGE;
    }
    if (algo_dijkstra_table(g, from, dist, prev, NULL, NULL, err, cap) != NAV_OK) {
        return NAV_ERR_ARG;
    }
    for (i = 1; i <= c->n_spots; ++i) {
        if (c->spots[i - 1].type != type) {
            continue;
        }
        if (i == from) {
            best = i;
            best_w = 0.0;
            break;
        }
        if (dist[i] < best_w) {
            best_w = dist[i];
            best = i;
        }
    }
    if (best == 0) {
        nav_err_set(err, cap, "校园内没有类型为“%s”的地点", spot_type_label(type));
        return NAV_ERR_ARG;
    }
    *best_spot = best;
    if (out != NULL) {
        path_clear(out);
        path_from_prev(g, prev, dist, from, best, out);
        if (out->count > 0) {
            out->weight = best_w;
        }
    }
    return NAV_OK;
}
