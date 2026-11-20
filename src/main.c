/* main.c —— 校园导航系统入口：子命令 + 交互菜单。
 *
 * 用法：
 *   nav                      交互菜单
 *   nav map                  打印 ASCII 校园地图
 *   nav list | roads | show <地点>
 *   nav route <起点> <终点> [--by time|dist] [--json]
 *   nav tour <起点> <终点> <途经1> [途经2 ...]
 *   nav near <位置> <设施类型或编号>
 *   nav allpairs [--limit N]
 *   nav critical | mst | components | check
 *   nav bench [--nodes N] [--seed S] [--repeat R]
 *   nav save | export [目录] | import <快照.nvg>
 *   nav version | help
 *
 * 全局选项：--data <目录>（CSV 数据目录）、--time（按步行时间计算）
 */
#include "app.h"
#include "bench.h"
#include "err.h"
#include "platform.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAV_VERSION "1.0.0"

static void usage(void)
{
    printf("校园导航系统 %s（图的最短路径 · C11）\n\n", NAV_VERSION);
    printf("用法：nav [--data 目录] [--time] <命令> [参数]\n\n");
    printf("命令：\n");
    printf("  menu                     交互菜单（默认）\n");
    printf("  map                      打印 ASCII 校园地图\n");
    printf("  list                     地点列表\n");
    printf("  roads                    道路列表\n");
    printf("  show <地点>              地点详情（编号或名称）\n");
    printf("  route <起点> <终点>      最短路径（--by time|dist，--json）\n");
    printf("  tour <起点> <终点> <途经...>  多点巡游，顺序自动优化\n");
    printf("  near <位置> <类型>       最近设施（类型名或编号）\n");
    printf("  allpairs [--limit N]     Floyd 全源最短路矩阵\n");
    printf("  critical                 割点与桥分析\n");
    printf("  mst                      最小生成树（Prim 与 Kruskal 对比）\n");
    printf("  components               连通分量\n");
    printf("  check                    数据集校验与连通性检查\n");
    printf("  bench [--nodes N] [--seed S] [--repeat R]  算法基准测试\n");
    printf("  save                     保存当前数据到 CSV\n");
    printf("  export <目录>            另存 CSV 到指定目录\n");
    printf("  import <文件.nvg>        导入二进制快照\n");
    printf("  version                  版本信息\n");
    printf("  help                     本帮助\n\n");
    printf("退出码：0 成功，1 无通路，2 参数或数据错误。\n");
}

static int resolve_spot_key(const Campus *c, const char *key, int *id, char *err, size_t cap)
{
    if (key == NULL || key[0] == '\0') {
        nav_err_set(err, cap, "地点参数为空");
        return NAV_ERR_ARG;
    }
    if (campus_find_spot(c, key, id) != NAV_OK) {
        nav_err_set(err, cap, "找不到地点“%s”（可输入编号或完整名称）", key);
        return NAV_ERR_ARG;
    }
    return NAV_OK;
}

static void print_path_json(const Campus *c, const Path *p, int by_time)
{
    int i;
    printf("{\"criterion\":\"%s\",\"weight\":%.1f,\"distance_m\":%.1f,\"minutes\":%.1f,\"count\":%d,\"nodes\":[",
           by_time ? "time" : "dist", p->weight, ui_path_length_m(c, p), ui_path_time_min(c, p),
           p->count);
    for (i = 0; i < p->count; ++i) {
        printf("%s%d", (i > 0) ? "," : "", p->nodes[i]);
    }
    printf("],\"names\":[");
    for (i = 0; i < p->count; ++i) {
        printf("%s\"%s\"", (i > 0) ? "," : "", c->spots[p->nodes[i] - 1].name);
    }
    printf("]}\n");
}

static int cmd_check(App *app)
{
    char err[512];
    int rc;
    int comp[NAV_MAX_SPOTS + 1];
    int k;

    rc = campus_validate(&app->campus, err, sizeof(err));
    if (rc != NAV_OK) {
        printf("数据集校验：失败 —— %s\n", err);
        return 2;
    }
    printf("数据集校验：通过（%d 个地点，%d 条道路）\n", app->campus.n_spots,
           app->campus.n_roads);
    printf("标题：%s\n", app->campus.title);
    printf("路网总长：%.1f 米\n", campus_total_road_length(&app->campus));
    k = algo_components(&app->graph, comp, err, sizeof(err));
    printf("连通分量：%d 个 %s\n", k, (k == 1) ? "（路网连通）" : "（存在不可达区域）");
    {
        int is_ap[NAV_MAX_SPOTS + 1];
        int bridges[NAV_MAX_ROADS];
        int nb = 0, n_ap = 0, i;
        if (algo_critical(&app->graph, is_ap, bridges, &nb, err, sizeof(err)) == NAV_OK) {
            for (i = 1; i <= app->graph.n; ++i) {
                if (is_ap[i]) {
                    ++n_ap;
                }
            }
            printf("割点 %d 个，桥 %d 条\n", n_ap, nb);
        }
    }
    return (k == 1) ? 0 : 1;
}

static int cmd_route(App *app, int argc, char **argv, int by_time_flag)
{
    int from = 0, to = 0, i;
    int json = 0;
    int by_time = by_time_flag;
    const char *from_key = NULL;
    const char *to_key = NULL;
    Path p;
    char err[512];

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
        } else if (strcmp(argv[i], "--by") == 0 && i + 1 < argc) {
            ++i;
            if (strcmp(argv[i], "time") == 0 || strcmp(argv[i], "1") == 0) {
                by_time = 1;
            } else if (strcmp(argv[i], "dist") == 0 || strcmp(argv[i], "0") == 0) {
                by_time = 0;
            } else {
                printf("--by 只接受 time 或 dist\n");
                return 2;
            }
        } else if (from_key == NULL) {
            from_key = argv[i];
        } else if (to_key == NULL) {
            to_key = argv[i];
        } else {
            printf("route 参数过多\n");
            return 2;
        }
    }
    if (from_key == NULL || to_key == NULL) {
        printf("用法：nav route <起点> <终点> [--by time|dist] [--json]\n");
        return 2;
    }
    if (by_time != app->by_time && app_set_criterion(app, by_time, err, sizeof(err)) != NAV_OK) {
        printf("切换口径失败：%s\n", err);
        return 2;
    }
    if (resolve_spot_key(&app->campus, from_key, &from, err, sizeof(err)) != NAV_OK ||
        resolve_spot_key(&app->campus, to_key, &to, err, sizeof(err)) != NAV_OK) {
        printf("%s\n", err);
        return 2;
    }
    if (algo_dijkstra(&app->graph, from, to, &p, err, sizeof(err)) != NAV_OK) {
        printf("计算失败：%s\n", err);
        return 2;
    }
    if (p.count == 0) {
        if (json) {
            printf("{\"error\":\"no path\"}\n");
        } else {
            printf("%s 到 %s 之间没有通路。\n", app->campus.spots[from - 1].name,
                   app->campus.spots[to - 1].name);
        }
        return 1;
    }
    if (json) {
        print_path_json(&app->campus, &p, by_time);
    } else {
        printf("%s（%s）→ %s（%s）\n", app->campus.spots[from - 1].name,
               app->campus.spots[from - 1].brief, app->campus.spots[to - 1].name,
               app->campus.spots[to - 1].brief);
        ui_print_path(&app->campus, &p, by_time);
    }
    return 0;
}

static int cmd_tour(App *app, int argc, char **argv)
{
    int start = 0, end = 0, i;
    int via[NAV_MAX_SPOTS];
    int order[NAV_MAX_SPOTS];
    int k = 0;
    Path p;
    char err[512];

    if (argc < 3) {
        printf("用法：nav tour <起点> <终点> <途经1> [途经2 ...]\n");
        return 2;
    }
    if (resolve_spot_key(&app->campus, argv[0], &start, err, sizeof(err)) != NAV_OK ||
        resolve_spot_key(&app->campus, argv[1], &end, err, sizeof(err)) != NAV_OK) {
        printf("%s\n", err);
        return 2;
    }
    for (i = 2; i < argc && k < NAV_MAX_SPOTS - 3; ++i) {
        int id;
        if (resolve_spot_key(&app->campus, argv[i], &id, err, sizeof(err)) != NAV_OK) {
            printf("%s\n", err);
            return 2;
        }
        if (id == start || id == end) {
            printf("途经点不能与起点或终点相同：%s\n", argv[i]);
            return 2;
        }
        via[k++] = id;
    }
    if (k == 0) {
        printf("至少需要一个途经点\n");
        return 2;
    }
    if (algo_tsp(&app->graph, start, via, k, end, order, &p, err, sizeof(err)) != NAV_OK) {
        printf("巡游计算失败：%s\n", err);
        return 2;
    }
    if (p.count == 0) {
        printf("存在不可达的途经点，巡游无解。\n");
        return 1;
    }
    printf("游览顺序（%s）：", k <= 12 ? "Held-Karp 精确解" : "最近邻 + 2-opt 近似解");
    printf("%s", app->campus.spots[start - 1].name);
    for (i = 0; i < k; ++i) {
        printf(" → %s", app->campus.spots[order[i] - 1].name);
    }
    printf(" → %s\n", app->campus.spots[end - 1].name);
    ui_print_path(&app->campus, &p, app->by_time);
    return 0;
}

static int cmd_near(App *app, int argc, char **argv)
{
    int from = 0, best = 0;
    SpotType type;
    Path p;
    char err[512];

    if (argc < 2) {
        printf("用法：nav near <位置> <设施类型名或编号>\n");
        return 2;
    }
    if (resolve_spot_key(&app->campus, argv[0], &from, err, sizeof(err)) != NAV_OK) {
        printf("%s\n", err);
        return 2;
    }
    if (spot_type_from_name(argv[1], &type) != NAV_OK) {
        int idx = atoi(argv[1]);
        if (spot_type_from_index(idx, &type) != NAV_OK) {
            printf("未知设施类型“%s”（可用：gate/teaching/library/admin/dorm/canteen/sport/"
                   "medical/service/scenic 或 1..10）\n",
                   argv[1]);
            return 2;
        }
    }
    if (algo_nearest_type(&app->graph, &app->campus, from, type, &best, &p, err, sizeof(err)) !=
        NAV_OK) {
        printf("查询失败：%s\n", err);
        return 2;
    }
    printf("最近的“%s”：%s（编号 %d）\n", spot_type_label(type),
           app->campus.spots[best - 1].name, best);
    ui_print_path(&app->campus, &p, app->by_time);
    return 0;
}

static int cmd_bench(int argc, char **argv)
{
    int nodes = 120, repeat = 32;
    unsigned int seed = 2026u;
    int i;
    char err[512];

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "--nodes") == 0 && i + 1 < argc) {
            nodes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (unsigned int)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--repeat") == 0 && i + 1 < argc) {
            repeat = atoi(argv[++i]);
        }
    }
    if (bench_run(nodes, seed, repeat, err, sizeof(err)) != NAV_OK) {
        printf("基准测试失败：%s\n", err);
        return 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    const char *data_dir = NULL;
    const char *cmd;
    int by_time = 0;
    int argi = 1;
    int rc = 0;
    char err[512];
    App app;

    platform_init_console();
    {
        int fixed_argc = 0;
        char **fixed_argv = platform_utf8_args(&fixed_argc);
        if (fixed_argv != NULL) {
            argc = fixed_argc;
            argv = fixed_argv;
        }
    }
    while (argi < argc && argv[argi][0] == '-' && argv[argi][1] == '-') {
        if (strcmp(argv[argi], "--data") == 0 && argi + 1 < argc) {
            data_dir = argv[argi + 1];
            argi += 2;
        } else if (strcmp(argv[argi], "--time") == 0) {
            by_time = 1;
            ++argi;
        } else if (strcmp(argv[argi], "--help") == 0 || strcmp(argv[argi], "-h") == 0) {
            usage();
            return 0;
        } else if (strcmp(argv[argi], "--version") == 0) {
            printf("校园导航系统 %s\n", NAV_VERSION);
            return 0;
        } else {
            break;
        }
    }
    cmd = (argi < argc) ? argv[argi] : "menu";
    ++argi;

    if (strcmp(cmd, "help") == 0) {
        usage();
        return 0;
    }
    if (strcmp(cmd, "version") == 0) {
        printf("校园导航系统 %s（C11，图的最短路径课程设计）\n", NAV_VERSION);
        return 0;
    }

    if (app_init(&app, data_dir, err, sizeof(err)) != NAV_OK) {
        fprintf(stderr, "初始化失败：%s\n", err);
        return 2;
    }
    if (by_time != app.by_time &&
        app_set_criterion(&app, by_time, err, sizeof(err)) != NAV_OK) {
        fprintf(stderr, "切换权重口径失败：%s\n", err);
        app_free(&app);
        return 2;
    }

    if (strcmp(cmd, "menu") == 0) {
        ui_banner(&app);
        rc = ui_run_menu(&app);
    } else if (strcmp(cmd, "map") == 0) {
        ui_ascii_map(&app);
    } else if (strcmp(cmd, "list") == 0) {
        ui_print_spots(&app.campus);
    } else if (strcmp(cmd, "roads") == 0) {
        ui_print_roads(&app.campus);
    } else if (strcmp(cmd, "show") == 0) {
        int id;
        if (argi >= argc) {
            printf("用法：nav show <地点>\n");
            rc = 2;
        } else if (resolve_spot_key(&app.campus, argv[argi], &id, err, sizeof(err)) != NAV_OK) {
            printf("%s\n", err);
            rc = 2;
        } else {
            ui_print_spots(&app.campus);
            ui_print_spot_detail(&app.campus, id);
        }
    } else if (strcmp(cmd, "route") == 0) {
        rc = cmd_route(&app, argc - argi, argv + argi, by_time);
    } else if (strcmp(cmd, "tour") == 0) {
        rc = cmd_tour(&app, argc - argi, argv + argi);
    } else if (strcmp(cmd, "near") == 0) {
        rc = cmd_near(&app, argc - argi, argv + argi);
    } else if (strcmp(cmd, "allpairs") == 0) {
        int limit = 22;
        int i;
        for (i = argi; i < argc; ++i) {
            if (strcmp(argv[i], "--limit") == 0 && i + 1 < argc) {
                limit = atoi(argv[++i]);
            }
        }
        ui_print_matrix(&app, limit);
    } else if (strcmp(cmd, "critical") == 0) {
        ui_print_critical(&app);
    } else if (strcmp(cmd, "mst") == 0) {
        ui_print_mst(&app);
    } else if (strcmp(cmd, "components") == 0) {
        ui_print_components(&app);
    } else if (strcmp(cmd, "check") == 0) {
        rc = cmd_check(&app);
    } else if (strcmp(cmd, "bench") == 0) {
        rc = cmd_bench(argc - argi, argv + argi);
    } else if (strcmp(cmd, "save") == 0) {
        if (app_save(&app, err, sizeof(err)) != NAV_OK) {
            printf("保存失败：%s\n", err);
            rc = 2;
        } else {
            printf("已保存到 %s\n", app.data_dir);
        }
    } else if (strcmp(cmd, "export") == 0) {
        char sp[APP_PATH_MAX * 2], rd[APP_PATH_MAX * 2];
        const char *dir = (argi < argc) ? argv[argi] : "data/campus";
        if (platform_mkdir(dir) != 0) {
            printf("无法创建目录：%s\n", dir);
            rc = 2;
        } else {
            snprintf(sp, sizeof(sp), "%s/spots.csv", dir);
            snprintf(rd, sizeof(rd), "%s/roads.csv", dir);
            if (campus_save_csv(&app.campus, sp, rd, err, sizeof(err)) != NAV_OK) {
                printf("导出失败：%s\n", err);
                rc = 2;
            } else {
                printf("已导出：%s\n", dir);
            }
        }
    } else if (strcmp(cmd, "import") == 0) {
        if (argi >= argc) {
            printf("用法：nav import <快照.nvg>\n");
            rc = 2;
        } else if (campus_load_bin(&app.campus, argv[argi], err, sizeof(err)) != NAV_OK) {
            printf("导入失败：%s\n", err);
            rc = 2;
        } else if (app_rebuild(&app, err, sizeof(err)) != NAV_OK) {
            printf("重建图失败：%s\n", err);
            rc = 2;
        } else {
            printf("已导入：%d 个地点，%d 条道路\n", app.campus.n_spots, app.campus.n_roads);
        }
    } else {
        printf("未知命令：%s\n\n", cmd);
        usage();
        rc = 2;
    }

    app_free(&app);
    return rc;
}
