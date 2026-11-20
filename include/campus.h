/* campus.h —— 校园数据集（地点 + 道路）的建模、校验、持久化与编辑接口。
 *
 * 数据文件：
 *   spots.csv   地点表：编号,名称,类型,横坐标,纵坐标,简介
 *   roads.csv   道路表：编号,起点,终点,长度,步行时间
 *   *.nvg       二进制快照：magic("NVG1") + 版本 + 计数 + 字段 + CRC32
 */
#ifndef CAMPUS_H
#define CAMPUS_H

#include "nav.h"

/* 初始化空数据集。 */
void campus_init(Campus *c);

/* 载入内置默认校园（江苏海洋大学苍梧校区示意数据，见 src/default_campus.c）。 */
int campus_load_default(Campus *c);

/* 校验：编号连续性、端点合法性、长度正数、无自环与重复边、容量上限。 */
int campus_validate(const Campus *c, char *err, size_t cap);

/* CSV 读写。路径不存在或格式错误时返回负值并写入 err。 */
int campus_load_csv(Campus *c, const char *spots_path, const char *roads_path,
                    char *err, size_t cap);
int campus_save_csv(const Campus *c, const char *spots_path, const char *roads_path,
                    char *err, size_t cap);

/* 二进制快照读写（含 CRC32 完整性校验）。 */
int campus_save_bin(const Campus *c, const char *path, char *err, size_t cap);
int campus_load_bin(Campus *c, const char *path, char *err, size_t cap);

/* 编辑操作。成功返回 0；失败返回负值并写 err。 */
int campus_add_spot(Campus *c, const char *name, SpotType type, double x, double y,
                    const char *brief, char *err, size_t cap);
int campus_remove_spot(Campus *c, int id, char *err, size_t cap);
int campus_add_road(Campus *c, int a, int b, double len_m, double walk_min,
                    char *err, size_t cap);
int campus_remove_road(Campus *c, int id, char *err, size_t cap);
int campus_update_road(Campus *c, int id, double len_m, double walk_min,
                       char *err, size_t cap);
int campus_set_road_open(Campus *c, int id, int open, char *err, size_t cap);

/* 按编号（"12"）或名称（"图书馆"）查找地点；找不到返回负值。 */
int campus_find_spot(const Campus *c, const char *key, int *id_out);
const Spot *campus_spot(const Campus *c, int id);
const Road *campus_road(const Campus *c, int id);

/* 统计与查询辅助。 */
double campus_total_road_length(const Campus *c);
int    campus_is_connected(const Campus *c);
int    campus_count_type(const Campus *c, SpotType type);

#endif /* CAMPUS_H */
