/* app.h —— 应用状态：校园数据 + 当前图 + 数据目录。 */
#ifndef APP_H
#define APP_H

#include "nav.h"
#include "campus.h"
#include "graph.h"

#define APP_PATH_MAX 260

typedef struct {
    Campus campus;
    Graph  graph;
    int    by_time;                 /* 当前权重口径：0 距离 / 1 时间 */
    char   data_dir[APP_PATH_MAX];  /* CSV 所在目录 */
    int    dirty;                   /* 是否有未保存修改 */
} App;

/* 初始化：优先按 data_dir 载入 CSV；失败则回退到内置默认数据。 */
int  app_init(App *app, const char *data_dir, char *err, size_t cap);
int  app_rebuild(App *app, char *err, size_t cap);
int  app_reload(App *app, char *err, size_t cap);
int  app_save(App *app, char *err, size_t cap);
void app_free(App *app);
int  app_set_criterion(App *app, int by_time, char *err, size_t cap);

#endif /* APP_H */
