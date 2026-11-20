/* nav.h —— 校园导航系统公共类型定义。
 *
 * 设计约束：
 *   1. 全系统统一使用 UTF-8 源文件与 UTF-8 控制台输出；
 *   2. 核心库不调用 exit()，错误通过 (char *err, size_t cap) 回传；
 *   3. 地点编号从 1 连续编号到 n_spots，删除地点时统一重排，保证编号即下标。
 */
#ifndef NAV_H
#define NAV_H

#include <stddef.h>

#define NAV_MAX_SPOTS  128   /* 最大地点数 */
#define NAV_MAX_ROADS  1024  /* 最大道路数 */
#define NAV_NAME_MAX   64    /* 地点/道路名称最大字节数（UTF-8） */
#define NAV_BRIEF_MAX  160   /* 简介最大字节数 */

#define NAV_INF  1.0e18      /* 表示无穷大的距离值 */

/* 返回码约定：0 成功；负值为错误；NAV_NOPATH 表示两点不连通。 */
#define NAV_OK         0
#define NAV_ERR_ARG   (-1)
#define NAV_ERR_NOMEM (-2)
#define NAV_ERR_IO    (-3)
#define NAV_ERR_FMT   (-4)
#define NAV_ERR_RANGE (-5)
#define NAV_ERR_NEGCYCLE (-6)
#define NAV_NOPATH    (-7)

/* 地点类型：驱动"最近设施"查询与地图着色。 */
typedef enum {
    SPOT_GATE = 0,
    SPOT_TEACHING,
    SPOT_LIBRARY,
    SPOT_ADMIN,
    SPOT_DORM,
    SPOT_CANTEEN,
    SPOT_SPORT,
    SPOT_MEDICAL,
    SPOT_SERVICE,
    SPOT_SCENIC,
    SPOT_TYPE_COUNT
} SpotType;

typedef struct {
    int      id;                 /* 1..n */
    char     name[NAV_NAME_MAX];
    SpotType type;
    double   x, y;               /* 校园局部坐标，单位米 */
    char     brief[NAV_BRIEF_MAX];
} Spot;

typedef struct {
    int    id;                   /* 1..m */
    int    from, to;             /* 端点地点编号 */
    double len_m;                /* 道路长度，米 */
    double walk_min;             /* 步行时间，分钟 */
    int    open;                 /* 1 通行，0 临时封闭 */
    char   name[NAV_NAME_MAX];   /* 道路名称，可为空串 */
} Road;

typedef struct {
    char   title[NAV_NAME_MAX];
    double width_m, height_m;    /* 地图绘制范围 */
    Spot   spots[NAV_MAX_SPOTS];
    int    n_spots;
    Road   roads[NAV_MAX_ROADS];
    int    n_roads;
} Campus;

/* 类型名与枚举互转（用于 CSV 读写和菜单显示）。 */
const char *spot_type_name(SpotType t);
const char *spot_type_label(SpotType t);        /* 中文名，用于界面显示 */
int spot_type_from_name(const char *s, SpotType *out);
int spot_type_from_index(int idx, SpotType *out);

#endif /* NAV_H */
