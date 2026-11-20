/* ui.h —— 控制台界面：ASCII 校园地图、表格、路径展示、交互菜单。 */
#ifndef UI_H
#define UI_H

#include "algo.h"
#include "app.h"

void ui_banner(const App *app);
void ui_print_spots(const Campus *c);
void ui_print_spot_detail(const Campus *c, int id);
void ui_print_roads(const Campus *c);
void ui_ascii_map(const App *app);
void ui_print_path(const Campus *c, const Path *p, int by_time);
void ui_print_matrix(const App *app, int limit);
void ui_print_critical(const App *app);
void ui_print_mst(const App *app);
void ui_print_components(const App *app);

/* 路径折算：米 / 分钟（与当前权重口径无关，两个指标都算）。 */
double ui_path_length_m(const Campus *c, const Path *p);
double ui_path_time_min(const Campus *c, const Path *p);

/* 交互式主菜单，返回 0 正常退出。 */
int ui_run_menu(App *app);

#endif /* UI_H */
