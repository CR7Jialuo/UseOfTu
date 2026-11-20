/* gui.h —— 图形界面的状态与接口。
 *
 * 设计原则：GUI 只做“表示层”，所有图算法仍由核心库（campus/graph/algo）完成；
 * 视图变换、命中测试、路径规划都是纯函数式的，便于单独测试与截图回归。
 */
#ifndef GUI_H
#define GUI_H

#include <windows.h>

#include "algo.h"
#include "campus.h"
#include "graph.h"
#include "nav.h"

#define GUI_MAX_SHAPE 320
#define GUI_PANEL_W 312      /* 右侧栏宽度（像素） */
#define GUI_STATUS_H 34      /* 底部状态栏高度 */

/* 一条道路的折线形状（米制），由 shapes.csv 读入 */
typedef struct {
    int a, b;
    int n;
    double x[GUI_MAX_SHAPE];
    double y[GUI_MAX_SHAPE];
} Shape;

typedef struct {
    Campus campus;
    Graph graph;
    Shape *shapes;
    int n_shapes;

    double m_per_px;   /* 底图标定：米/像素 */
    double ox, oy;     /* 底图像素坐标原点（对应世界坐标 (0,0) 的像素） */

    int by_time;       /* 0 距离 / 1 时间 */

    /* 视图：世界坐标(米) -> 屏幕像素 */
    double view_x0;    /* 屏幕左上角对应的世界 x */
    double view_y1;    /* 屏幕左上角对应的世界 y */
    double scale;      /* 每米多少屏幕像素 */
    int cw, ch;        /* 客户区尺寸 */

    /* 交互 */
    int hover;         /* 鼠标悬停的地点 id，0 表示无 */
    int selected;      /* 单击选中的地点 */
    int from_id, to_id;
    int via[8];            /* 巡游途经点（右上角编号显示） */
    int n_via;
    int pick_mode;     /* 0 无，1 正在选起点，2 正在选终点 */
    int mouse_x, mouse_y;
    int dragging;
    POINT drag_pt;
    double drag_x0, drag_y1;

    /* 结果 */
    Path path;
    int has_path;
    double path_ms;    /* 规划耗时（毫秒） */

    /* 提示 */
    char status[256];
    int status_error;

    int show_instructions;
    int anim_segments;      /* 动画：当前已显示的路段数（=0 表示不播放） */
    int anim_playing;
    int show_critical;      /* 是否高亮桥/割点相关道路 */
    int bridges[GUI_MAX_SHAPE];
    int n_bridges;
} Gui;

int  gui_init(Gui *g, const char *data_dir, char *err, size_t cap);
void gui_free(Gui *g);

void gui_fit_view(Gui *g, int cw, int ch);
void gui_world_to_screen(const Gui *g, double x, double y, double *sx, double *sy);
void gui_screen_to_world(const Gui *g, double sx, double sy, double *x, double *y);
void gui_zoom_at(Gui *g, int sx, int sy, double factor);
void gui_pan(Gui *g, int dx, int dy);
int  gui_hit_test(const Gui *g, int sx, int sy);
void gui_center_on(Gui *g, int spot_id, double zoom);

const Shape *gui_shape(const Gui *g, int a, int b);

int  gui_plan(Gui *g);
int  gui_nearest(Gui *g, SpotType type);
int  gui_tour(Gui *g);
void gui_toggle_via(Gui *g, int spot_id);
double gui_path_len(const Gui *g);
double gui_path_time(const Gui *g);
int  gui_build_instructions(const Gui *g, char lines[][200], int max_lines);
const char *gui_spot_name(const Gui *g, int id);

/* 右侧栏按钮命中：返回动作编号（见 render.c 的 ACT_*），未命中返回 0 */
int  gui_panel_hit(const Gui *g, int x, int y);
int  gui_toggle_critical(Gui *g);

/* 渲染层（render.c） */
int  gui_render_init(const char *basemap_path);
void gui_render_shutdown(void);
struct GpGraphics_;  /* GDI+ flat API 的类型，避免头文件互相包含 */
void gui_paint(Gui *g, void *graphics, int w, int h);

#endif /* GUI_H */
