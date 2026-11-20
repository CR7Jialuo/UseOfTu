/* app.c —— 应用状态管理：数据目录发现、加载、重建图、保存。 */
#include "app.h"
#include "err.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>

static void copy_str(char *dst, size_t cap, const char *src)
{
    size_t len;
    if (dst == NULL || cap == 0) {
        return;
    }
    if (src == NULL) {
        src = "";
    }
    len = strlen(src);
    if (len >= cap) {
        len = cap - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void join_path(char *out, size_t cap, const char *dir, const char *name)
{
    size_t len;
    if (dir == NULL || dir[0] == '\0') {
        copy_str(out, cap, name);
        return;
    }
    len = strlen(dir);
    if (dir[len - 1] == '/' || dir[len - 1] == '\\') {
        snprintf(out, cap, "%s%s", dir, name);
    } else {
        snprintf(out, cap, "%s/%s", dir, name);
    }
}

static int has_csv_pair(const char *dir)
{
    char sp[APP_PATH_MAX * 2];
    char rd[APP_PATH_MAX * 2];
    if (dir == NULL || dir[0] == '\0') {
        return 0;
    }
    join_path(sp, sizeof(sp), dir, "spots.csv");
    join_path(rd, sizeof(rd), dir, "roads.csv");
    return platform_file_exists(sp) && platform_file_exists(rd);
}

static int discover_data_dir(char *out, size_t cap)
{
    char exe[APP_PATH_MAX];
    char tmp[APP_PATH_MAX * 2];

    if (has_csv_pair("data/campus")) {
        copy_str(out, cap, "data/campus");
        return 0;
    }
    if (has_csv_pair("../data/campus")) {
        copy_str(out, cap, "../data/campus");
        return 0;
    }
    if (platform_exe_dir(exe, sizeof(exe)) == 0) {
        join_path(tmp, sizeof(tmp), exe, "data/campus");
        if (has_csv_pair(tmp)) {
            copy_str(out, cap, tmp);
            return 0;
        }
        join_path(tmp, sizeof(tmp), exe, "../data/campus");
        if (has_csv_pair(tmp)) {
            copy_str(out, cap, tmp);
            return 0;
        }
    }
    return -1;
}

int app_rebuild(App *app, char *err, size_t cap)
{
    Graph g;
    int rc;

    if (app == NULL) {
        nav_err_set(err, cap, "应用状态为空");
        return NAV_ERR_ARG;
    }
    rc = graph_build(&app->campus, app->by_time, &g, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    graph_free(&app->graph);
    app->graph = g;
    return NAV_OK;
}

int app_reload(App *app, char *err, size_t cap)
{
    char sp[APP_PATH_MAX * 2];
    char rd[APP_PATH_MAX * 2];
    int rc;

    if (app == NULL) {
        nav_err_set(err, cap, "应用状态为空");
        return NAV_ERR_ARG;
    }
    campus_init(&app->campus);
    if (app->data_dir[0] != '\0' && has_csv_pair(app->data_dir)) {
        join_path(sp, sizeof(sp), app->data_dir, "spots.csv");
        join_path(rd, sizeof(rd), app->data_dir, "roads.csv");
        rc = campus_load_csv(&app->campus, sp, rd, err, cap);
        if (rc != NAV_OK) {
            return rc;
        }
    } else {
        rc = campus_load_default(&app->campus);
        if (rc != NAV_OK) {
            nav_err_set(err, cap, "内置数据集加载失败");
            return rc;
        }
    }
    app->dirty = 0;
    return app_rebuild(app, err, cap);
}

int app_init(App *app, const char *data_dir, char *err, size_t cap)
{
    if (app == NULL) {
        nav_err_set(err, cap, "应用状态为空");
        return NAV_ERR_ARG;
    }
    memset(app, 0, sizeof(*app));
    app->by_time = 0;
    if (data_dir != NULL && data_dir[0] != '\0') {
        copy_str(app->data_dir, sizeof(app->data_dir), data_dir);
    } else {
        char found[APP_PATH_MAX];
        if (discover_data_dir(found, sizeof(found)) == 0) {
            copy_str(app->data_dir, sizeof(app->data_dir), found);
        } else {
            copy_str(app->data_dir, sizeof(app->data_dir), "data/campus");
        }
    }
    return app_reload(app, err, cap);
}

int app_save(App *app, char *err, size_t cap)
{
    char sp[APP_PATH_MAX * 2];
    char rd[APP_PATH_MAX * 2];

    if (app == NULL) {
        nav_err_set(err, cap, "应用状态为空");
        return NAV_ERR_ARG;
    }
    if (app->data_dir[0] == '\0') {
        copy_str(app->data_dir, sizeof(app->data_dir), "data/campus");
    }
    if (platform_mkdir(app->data_dir) != 0 && !has_csv_pair(app->data_dir)) {
        nav_err_set(err, cap, "无法创建数据目录：%s", app->data_dir);
        return NAV_ERR_IO;
    }
    join_path(sp, sizeof(sp), app->data_dir, "spots.csv");
    join_path(rd, sizeof(rd), app->data_dir, "roads.csv");
    if (campus_save_csv(&app->campus, sp, rd, err, cap) != NAV_OK) {
        return NAV_ERR_IO;
    }
    app->dirty = 0;
    return NAV_OK;
}

int app_set_criterion(App *app, int by_time, char *err, size_t cap)
{
    if (by_time != 0 && by_time != 1) {
        nav_err_set(err, cap, "权重口径只能是 0（距离）或 1（时间）");
        return NAV_ERR_ARG;
    }
    app->by_time = by_time;
    return app_rebuild(app, err, cap);
}

void app_free(App *app)
{
    if (app == NULL) {
        return;
    }
    graph_free(&app->graph);
}
