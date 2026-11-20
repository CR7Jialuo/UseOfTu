/* campus.c —— 校园数据集：校验、CSV 读写、二进制快照、编辑操作。
 *
 * CSV 解析要点：
 *   - 跳过 '#' 注释行与空行；
 *   - 支持双引号包裹字段，字段内 "" 表示一个引号字符；
 *   - 数字字段用 strtod/strtol 解析并检查整串消费，拒绝 "12abc"。
 */
#include "campus.h"
#include "crc32.h"
#include "err.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CRC_TAIL 4

static const struct {
    const char *name;
    const char *label;
} k_type_names[SPOT_TYPE_COUNT] = {
    { "gate",      "校门" },
    { "teaching",  "教学楼" },
    { "library",   "图书馆" },
    { "admin",     "行政楼" },
    { "dorm",      "宿舍" },
    { "canteen",   "食堂" },
    { "sport",     "体育设施" },
    { "medical",   "医疗" },
    { "service",   "服务设施" },
    { "scenic",    "景观" }
};

const char *spot_type_name(SpotType t)
{
    if ((int)t < 0 || t >= SPOT_TYPE_COUNT) {
        return "unknown";
    }
    return k_type_names[t].name;
}

const char *spot_type_label(SpotType t)
{
    if ((int)t < 0 || t >= SPOT_TYPE_COUNT) {
        return "未知";
    }
    return k_type_names[t].label;
}

int spot_type_from_name(const char *s, SpotType *out)
{
    int i;
    if (s == NULL || out == NULL) {
        return NAV_ERR_ARG;
    }
    for (i = 0; i < SPOT_TYPE_COUNT; ++i) {
        if (strcmp(s, k_type_names[i].name) == 0) {
            *out = (SpotType)i;
            return NAV_OK;
        }
    }
    return NAV_ERR_ARG;
}

int spot_type_from_index(int idx, SpotType *out)
{
    if (out == NULL || idx < 1 || idx > SPOT_TYPE_COUNT) {
        return NAV_ERR_RANGE;
    }
    *out = (SpotType)(idx - 1);
    return NAV_OK;
}

void campus_init(Campus *c)
{
    if (c == NULL) {
        return;
    }
    memset(c, 0, sizeof(*c));
    strcpy(c->title, "校园导航");
    c->width_m = 1000.0;
    c->height_m = 800.0;
}

const Spot *campus_spot(const Campus *c, int id)
{
    if (c == NULL || id < 1 || id > c->n_spots) {
        return NULL;
    }
    return &c->spots[id - 1];
}

const Road *campus_road(const Campus *c, int id)
{
    if (c == NULL || id < 1 || id > c->n_roads) {
        return NULL;
    }
    return &c->roads[id - 1];
}

double campus_total_road_length(const Campus *c)
{
    double total = 0.0;
    int i;
    if (c == NULL) {
        return 0.0;
    }
    for (i = 0; i < c->n_roads; ++i) {
        total += c->roads[i].len_m;
    }
    return total;
}

int campus_count_type(const Campus *c, SpotType type)
{
    int i, n = 0;
    if (c == NULL) {
        return 0;
    }
    for (i = 0; i < c->n_spots; ++i) {
        if (c->spots[i].type == type) {
            ++n;
        }
    }
    return n;
}

static int has_edge(const Campus *c, int a, int b, int skip_road)
{
    int i;
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        if (r->id == skip_road) {
            continue;
        }
        if ((r->from == a && r->to == b) || (r->from == b && r->to == a)) {
            return 1;
        }
    }
    return 0;
}

int campus_validate(const Campus *c, char *err, size_t cap)
{
    int i, j;

    if (c == NULL) {
        nav_err_set(err, cap, "数据集指针为空");
        return NAV_ERR_ARG;
    }
    if (c->n_spots < 1) {
        nav_err_set(err, cap, "地点数为 0，至少需要 1 个地点");
        return NAV_ERR_FMT;
    }
    if (c->n_spots > NAV_MAX_SPOTS) {
        nav_err_set(err, cap, "地点数 %d 超过上限 %d", c->n_spots, NAV_MAX_SPOTS);
        return NAV_ERR_RANGE;
    }
    if (c->n_roads > NAV_MAX_ROADS) {
        nav_err_set(err, cap, "道路数 %d 超过上限 %d", c->n_roads, NAV_MAX_ROADS);
        return NAV_ERR_RANGE;
    }
    if (c->width_m <= 0.0 || c->height_m <= 0.0) {
        nav_err_set(err, cap, "绘制范围必须为正数");
        return NAV_ERR_FMT;
    }
    if (strlen(c->title) >= NAV_NAME_MAX) {
        nav_err_set(err, cap, "标题过长");
        return NAV_ERR_FMT;
    }
    for (i = 0; i < c->n_spots; ++i) {
        const Spot *s = &c->spots[i];
        if (s->id != i + 1) {
            nav_err_set(err, cap, "第 %d 个地点编号为 %d，应为 %d", i + 1, s->id, i + 1);
            return NAV_ERR_FMT;
        }
        if (s->name[0] == '\0') {
            nav_err_set(err, cap, "地点 %d 名称为空", s->id);
            return NAV_ERR_FMT;
        }
        if (strlen(s->name) >= NAV_NAME_MAX || strlen(s->brief) >= NAV_BRIEF_MAX) {
            nav_err_set(err, cap, "地点 %d 的名称或简介超出长度上限", s->id);
            return NAV_ERR_FMT;
        }
        if ((int)s->type < 0 || s->type >= SPOT_TYPE_COUNT) {
            nav_err_set(err, cap, "地点 %d 类型非法", s->id);
            return NAV_ERR_FMT;
        }
        if (s->x < 0.0 || s->x > c->width_m || s->y < 0.0 || s->y > c->height_m) {
            nav_err_set(err, cap, "地点 %s 坐标 (%.1f, %.1f) 超出绘制范围", s->name, s->x, s->y);
            return NAV_ERR_FMT;
        }
        for (j = 0; j < i; ++j) {
            if (strcmp(c->spots[j].name, s->name) == 0) {
                nav_err_set(err, cap, "地点名称重复：%s", s->name);
                return NAV_ERR_FMT;
            }
        }
    }
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        if (r->id != i + 1) {
            nav_err_set(err, cap, "第 %d 条道路编号为 %d，应为 %d", i + 1, r->id, i + 1);
            return NAV_ERR_FMT;
        }
        if (r->from < 1 || r->from > c->n_spots || r->to < 1 || r->to > c->n_spots) {
            nav_err_set(err, cap, "道路 %d 端点编号越界", r->id);
            return NAV_ERR_FMT;
        }
        if (r->from == r->to) {
            nav_err_set(err, cap, "道路 %d 是自环", r->id);
            return NAV_ERR_FMT;
        }
        if (!(r->len_m > 0.0) || r->len_m > 1.0e6) {
            nav_err_set(err, cap, "道路 %d 长度非法：%.3f", r->id, r->len_m);
            return NAV_ERR_FMT;
        }
        if (!(r->walk_min > 0.0) || r->walk_min > 1.0e5) {
            nav_err_set(err, cap, "道路 %d 步行时间非法：%.3f", r->id, r->walk_min);
            return NAV_ERR_FMT;
        }
        if (r->open != 0 && r->open != 1) {
            nav_err_set(err, cap, "道路 %d 通行标志非法", r->id);
            return NAV_ERR_FMT;
        }
        if (has_edge(c, r->from, r->to, r->id)) {
            nav_err_set(err, cap, "道路 %d 与已有道路重复（%s - %s）", r->id,
                        c->spots[r->from - 1].name, c->spots[r->to - 1].name);
            return NAV_ERR_FMT;
        }
    }
    return NAV_OK;
}

int campus_is_connected(const Campus *c)
{
    int i, head, tail;
    int seen[NAV_MAX_SPOTS + 1];
    int queue[NAV_MAX_SPOTS + 1];

    if (c == NULL || c->n_spots < 1) {
        return 0;
    }
    memset(seen, 0, sizeof(seen));
    head = 0;
    tail = 0;
    queue[tail++] = 1;
    seen[1] = 1;
    while (head < tail) {
        int cur = queue[head++];
        for (i = 0; i < c->n_roads; ++i) {
            const Road *r = &c->roads[i];
            int nxt = -1;
            if (!r->open) {
                continue;
            }
            if (r->from == cur) {
                nxt = r->to;
            } else if (r->to == cur) {
                nxt = r->from;
            }
            if (nxt > 0 && !seen[nxt]) {
                seen[nxt] = 1;
                queue[tail++] = nxt;
            }
        }
    }
    for (i = 1; i <= c->n_spots; ++i) {
        if (!seen[i]) {
            return 0;
        }
    }
    return 1;
}

/* ---------------- CSV 读写 ---------------- */

static int csv_split(char *line, char *fields[], int max_fields)
{
    int n = 0;
    char *p = line;

    if (line == NULL || fields == NULL || max_fields <= 0) {
        return 0;
    }
    while (n < max_fields) {
        char *start = p;
        char *next;
        int had_comma;

        if (*start == '"') {
            char *src = start + 1;
            char *dst = start;
            for (;;) {
                if (*src == '\0') {
                    break;
                }
                if (*src == '"') {
                    if (src[1] == '"') {
                        *dst++ = '"';
                        src += 2;
                        continue;
                    }
                    ++src;
                    break;
                }
                *dst++ = *src++;
            }
            *dst = '\0';
            while (*src != '\0' && *src != ',') {
                ++src;
            }
            had_comma = (*src == ',');
            next = had_comma ? src + 1 : src;
            fields[n++] = start;
        } else {
            char *end = start;
            char *trim;
            while (*end != '\0' && *end != ',') {
                ++end;
            }
            had_comma = (*end == ',');
            next = had_comma ? end + 1 : end;
            trim = end;
            while (trim > start && (trim[-1] == ' ' || trim[-1] == '\t')) {
                --trim;
            }
            *trim = '\0';
            fields[n++] = start;
        }
        p = next;
        if (!had_comma) {
            break;
        }
    }
    return n;
}

static int parse_int(const char *s, long *out)
{
    char *end = NULL;
    long v;
    if (s == NULL || *s == '\0') {
        return NAV_ERR_FMT;
    }
    v = strtol(s, &end, 10);
    if (end == NULL || *end != '\0') {
        return NAV_ERR_FMT;
    }
    *out = v;
    return NAV_OK;
}

static int parse_double(const char *s, double *out)
{
    char *end = NULL;
    double v;
    if (s == NULL || *s == '\0') {
        return NAV_ERR_FMT;
    }
    v = strtod(s, &end);
    if (end == NULL || *end != '\0') {
        return NAV_ERR_FMT;
    }
    if (v != v) { /* NaN */
        return NAV_ERR_FMT;
    }
    *out = v;
    return NAV_OK;
}

static int copy_field(char *dst, size_t cap, const char *src)
{
    size_t len;
    if (src == NULL) {
        src = "";
    }
    len = strlen(src);
    if (len >= cap) {
        return NAV_ERR_FMT;
    }
    memcpy(dst, src, len + 1);
    return NAV_OK;
}

/* 读一行；返回 1 成功，0 到文件尾，负值错误。 */
static int read_line(FILE *fp, char *buf, size_t cap, char *err, size_t errcap)
{
    size_t len;
    if (fgets(buf, (int)cap, fp) == NULL) {
        if (ferror(fp)) {
            nav_err_set(err, errcap, "读取文件失败");
            return NAV_ERR_IO;
        }
        return 0;
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] != '\n' && !feof(fp)) {
        nav_err_set(err, errcap, "单行长度超过 %u 字节", (unsigned)cap);
        return NAV_ERR_FMT;
    }
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
        buf[--len] = '\0';
    }
    return 1;
}

static int load_spots(Campus *c, const char *path, char *err, size_t cap)
{
    FILE *fp;
    char line[1024];
    int rc;
    int line_no = 0;

    fp = platform_fopen(path, "rb");
    if (fp == NULL) {
        nav_err_set(err, cap, "无法打开地点文件：%s", path);
        return NAV_ERR_IO;
    }
    c->n_spots = 0;
    while ((rc = read_line(fp, line, sizeof(line), err, cap)) == 1) {
        char *fields[8];
        int nf;
        long id;
        Spot s;

        ++line_no;
        if (line_no == 1 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, strlen(line + 3) + 1);
        }
        if (strncmp(line, "# title:", 8) == 0) {
            const char *t = line + 8;
            while (*t == ' ' || *t == '\t') {
                ++t;
            }
            if (copy_field(c->title, sizeof(c->title), t) != NAV_OK) {
                nav_err_set(err, cap, "地点文件第 %d 行标题过长", line_no);
                fclose(fp);
                return NAV_ERR_FMT;
            }
            continue;
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        nf = csv_split(line, fields, 8);
        if (nf != 6) {
            nav_err_set(err, cap, "地点文件第 %d 行列数为 %d，应为 6", line_no, nf);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (parse_int(fields[0], &id) != NAV_OK || id < 1 || id > NAV_MAX_SPOTS) {
            nav_err_set(err, cap, "地点文件第 %d 行编号非法：%s", line_no, fields[0]);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (c->n_spots >= NAV_MAX_SPOTS) {
            nav_err_set(err, cap, "地点数超过上限 %d", NAV_MAX_SPOTS);
            fclose(fp);
            return NAV_ERR_RANGE;
        }
        memset(&s, 0, sizeof(s));
        s.id = (int)id;
        if (copy_field(s.name, sizeof(s.name), fields[1]) != NAV_OK) {
            nav_err_set(err, cap, "地点文件第 %d 行名称过长", line_no);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (spot_type_from_name(fields[2], &s.type) != NAV_OK) {
            nav_err_set(err, cap, "地点文件第 %d 行类型未知：%s", line_no, fields[2]);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (parse_double(fields[3], &s.x) != NAV_OK || parse_double(fields[4], &s.y) != NAV_OK) {
            nav_err_set(err, cap, "地点文件第 %d 行坐标非法", line_no);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (copy_field(s.brief, sizeof(s.brief), fields[5]) != NAV_OK) {
            nav_err_set(err, cap, "地点文件第 %d 行简介过长", line_no);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (s.id != c->n_spots + 1) {
            nav_err_set(err, cap, "地点文件第 %d 行编号 %d 不连续，应为 %d", line_no, s.id,
                        c->n_spots + 1);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        c->spots[c->n_spots++] = s;
    }
    fclose(fp);
    if (rc < 0) {
        return rc;
    }
    if (c->n_spots == 0) {
        nav_err_set(err, cap, "地点文件为空：%s", path);
        return NAV_ERR_FMT;
    }
    return NAV_OK;
}

static int load_roads(Campus *c, const char *path, char *err, size_t cap)
{
    FILE *fp;
    char line[1024];
    int rc;
    int line_no = 0;

    fp = platform_fopen(path, "rb");
    if (fp == NULL) {
        nav_err_set(err, cap, "无法打开道路文件：%s", path);
        return NAV_ERR_IO;
    }
    c->n_roads = 0;
    while ((rc = read_line(fp, line, sizeof(line), err, cap)) == 1) {
        char *fields[8];
        int nf;
        long id, a, b;
        Road r;

        ++line_no;
        if (line_no == 1 && (unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, strlen(line + 3) + 1);
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        nf = csv_split(line, fields, 8);
        if (nf != 5) {
            nav_err_set(err, cap, "道路文件第 %d 行列数为 %d，应为 5", line_no, nf);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (parse_int(fields[0], &id) != NAV_OK || id < 1 || id > NAV_MAX_ROADS) {
            nav_err_set(err, cap, "道路文件第 %d 行编号非法：%s", line_no, fields[0]);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (c->n_roads >= NAV_MAX_ROADS) {
            nav_err_set(err, cap, "道路数超过上限 %d", NAV_MAX_ROADS);
            fclose(fp);
            return NAV_ERR_RANGE;
        }
        memset(&r, 0, sizeof(r));
        r.id = (int)id;
        r.open = 1;
        if (parse_int(fields[1], &a) != NAV_OK || parse_int(fields[2], &b) != NAV_OK) {
            nav_err_set(err, cap, "道路文件第 %d 行端点非法", line_no);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        r.from = (int)a;
        r.to = (int)b;
        if (parse_double(fields[3], &r.len_m) != NAV_OK ||
            parse_double(fields[4], &r.walk_min) != NAV_OK) {
            nav_err_set(err, cap, "道路文件第 %d 行长度或时间非法", line_no);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        if (r.id != c->n_roads + 1) {
            nav_err_set(err, cap, "道路文件第 %d 行编号 %d 不连续，应为 %d", line_no, r.id,
                        c->n_roads + 1);
            fclose(fp);
            return NAV_ERR_FMT;
        }
        c->roads[c->n_roads++] = r;
    }
    fclose(fp);
    return (rc < 0) ? rc : NAV_OK;
}

int campus_load_csv(Campus *c, const char *spots_path, const char *roads_path, char *err,
                    size_t cap)
{
    Campus tmp;
    int rc;

    if (c == NULL || spots_path == NULL || roads_path == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    campus_init(&tmp);
    snprintf(tmp.title, sizeof(tmp.title), "校园导航数据集");
    tmp.width_m = 1000.0;
    tmp.height_m = 800.0;

    rc = load_spots(&tmp, spots_path, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    rc = load_roads(&tmp, roads_path, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    /* 依据实际坐标扩展绘制范围，保证 ASCII 地图不越界。 */
    {
        int i;
        double maxx = 0.0, maxy = 0.0;
        for (i = 0; i < tmp.n_spots; ++i) {
            if (tmp.spots[i].x > maxx) {
                maxx = tmp.spots[i].x;
            }
            if (tmp.spots[i].y > maxy) {
                maxy = tmp.spots[i].y;
            }
        }
        tmp.width_m = maxx + 30.0;
        tmp.height_m = maxy + 30.0;
    }
    rc = campus_validate(&tmp, err, cap);
    if (rc != NAV_OK) {
        return rc;
    }
    *c = tmp;
    return NAV_OK;
}

static void csv_write_field(FILE *fp, const char *s)
{
    int need_quote = 0;
    const char *p;

    if (s == NULL) {
        s = "";
    }
    for (p = s; *p != '\0'; ++p) {
        if (*p == ',' || *p == '"') {
            need_quote = 1;
            break;
        }
    }
    if (!need_quote) {
        fputs(s, fp);
        return;
    }
    fputc('"', fp);
    for (p = s; *p != '\0'; ++p) {
        if (*p == '"') {
            fputs("\"\"", fp);
        } else {
            fputc(*p, fp);
        }
    }
    fputc('"', fp);
}

int campus_save_csv(const Campus *c, const char *spots_path, const char *roads_path, char *err,
                    size_t cap)
{
    FILE *fp;
    int i;

    if (c == NULL || spots_path == NULL || roads_path == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (campus_validate(c, err, cap) != NAV_OK) {
        return NAV_ERR_FMT;
    }

    fp = platform_fopen(spots_path, "wb");
    if (fp == NULL) {
        nav_err_set(err, cap, "无法写入地点文件：%s", spots_path);
        return NAV_ERR_IO;
    }
    fprintf(fp, "# title: %s\n", c->title);
    fprintf(fp, "# %s 地点表（由校园导航系统导出）\n", c->title);
    fputs("# 字段：编号,名称,类型,横坐标(米),纵坐标(米),简介\n", fp);
    for (i = 0; i < c->n_spots; ++i) {
        const Spot *s = &c->spots[i];
        fprintf(fp, "%d,", s->id);
        csv_write_field(fp, s->name);
        fputc(',', fp);
        fputs(spot_type_name(s->type), fp);
        fprintf(fp, ",%.1f,%.1f,", s->x, s->y);
        csv_write_field(fp, s->brief);
        fputc('\n', fp);
    }
    fclose(fp);

    fp = platform_fopen(roads_path, "wb");
    if (fp == NULL) {
        nav_err_set(err, cap, "无法写入道路文件：%s", roads_path);
        return NAV_ERR_IO;
    }
    fprintf(fp, "# %s 道路表（由校园导航系统导出）\n", c->title);
    fputs("# 字段：编号,起点编号,终点编号,长度(米),步行时间(分钟)\n", fp);
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        fprintf(fp, "%d,%d,%d,%.1f,%.1f\n", r->id, r->from, r->to, r->len_m, r->walk_min);
    }
    fclose(fp);
    return NAV_OK;
}

/* ---------------- 二进制快照 ---------------- */

static void put_u16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFFu);
    p[1] = (unsigned char)((v >> 8) & 0xFFu);
}

static void put_u32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v & 0xFFul);
    p[1] = (unsigned char)((v >> 8) & 0xFFul);
    p[2] = (unsigned char)((v >> 16) & 0xFFul);
    p[3] = (unsigned char)((v >> 24) & 0xFFul);
}

static void put_f64(unsigned char *p, double v)
{
    unsigned long long u = 0;
    memcpy(&u, &v, sizeof(u));
    put_u32(p, (unsigned long)(u & 0xFFFFFFFFull));
    put_u32(p + 4, (unsigned long)((u >> 32) & 0xFFFFFFFFull));
}

static void get_u16(const unsigned char *p, unsigned int *v)
{
    *v = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static void get_u32(const unsigned char *p, unsigned long *v)
{
    *v = (unsigned long)p[0] | ((unsigned long)p[1] << 8) | ((unsigned long)p[2] << 16) |
         ((unsigned long)p[3] << 24);
}

static void get_f64(const unsigned char *p, double *v)
{
    unsigned long lo = 0, hi = 0;
    unsigned long long u;
    get_u32(p, &lo);
    get_u32(p + 4, &hi);
    u = (unsigned long long)lo | ((unsigned long long)hi << 32);
    memcpy(v, &u, sizeof(*v));
}

#define SNAPSHOT_VERSION 1u
#define SNAPSHOT_MAGIC0 'N'
#define SNAPSHOT_MAGIC1 'V'
#define SNAPSHOT_MAGIC2 'G'
#define SNAPSHOT_MAGIC3 '1'

static size_t snapshot_size(const Campus *c)
{
    size_t n = 32;
    int i;
    n += 2 + strlen(c->title);
    for (i = 0; i < c->n_spots; ++i) {
        n += 4 + 2 + 8 + 8 + 2 + strlen(c->spots[i].name) + 2 + strlen(c->spots[i].brief);
    }
    for (i = 0; i < c->n_roads; ++i) {
        n += 4 + 4 + 4 + 8 + 8 + 1;
    }
    return n + CRC_TAIL;
}

static unsigned char *snapshot_write_header(unsigned char *p, const Campus *c)
{
    unsigned int title_len = (unsigned int)strlen(c->title);
    memcpy(p, "NVG1", 4);
    p += 4;
    put_u16(p, SNAPSHOT_VERSION);
    p += 2;
    put_u16(p, 0);
    p += 2;
    put_u32(p, (unsigned long)c->n_spots);
    p += 4;
    put_u32(p, (unsigned long)c->n_roads);
    p += 4;
    put_f64(p, c->width_m);
    p += 8;
    put_f64(p, c->height_m);
    p += 8;
    put_u16(p, title_len);
    p += 2;
    memcpy(p, c->title, title_len);
    return p + title_len;
}

int campus_save_bin(const Campus *c, const char *path, char *err, size_t cap)
{
    size_t total, body;
    unsigned char *buf;
    unsigned char *p;
    FILE *fp;
    int i;
    uint32_t crc;

    if (c == NULL || path == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    if (campus_validate(c, err, cap) != NAV_OK) {
        return NAV_ERR_FMT;
    }
    total = snapshot_size(c);
    if (total > 64u * 1024u * 1024u) {
        nav_err_set(err, cap, "快照过大：%u 字节", (unsigned)total);
        return NAV_ERR_RANGE;
    }
    buf = (unsigned char *)malloc(total);
    if (buf == NULL) {
        nav_err_set(err, cap, "内存不足（快照 %u 字节）", (unsigned)total);
        return NAV_ERR_NOMEM;
    }
    p = snapshot_write_header(buf, c);
    for (i = 0; i < c->n_spots; ++i) {
        const Spot *s = &c->spots[i];
        unsigned int nlen = (unsigned int)strlen(s->name);
        unsigned int blen = (unsigned int)strlen(s->brief);
        put_u32(p, (unsigned long)s->id);
        p += 4;
        put_u16(p, (unsigned int)s->type);
        p += 2;
        put_f64(p, s->x);
        p += 8;
        put_f64(p, s->y);
        p += 8;
        put_u16(p, nlen);
        p += 2;
        memcpy(p, s->name, nlen);
        p += nlen;
        put_u16(p, blen);
        p += 2;
        memcpy(p, s->brief, blen);
        p += blen;
    }
    for (i = 0; i < c->n_roads; ++i) {
        const Road *r = &c->roads[i];
        put_u32(p, (unsigned long)r->id);
        p += 4;
        put_u32(p, (unsigned long)r->from);
        p += 4;
        put_u32(p, (unsigned long)r->to);
        p += 4;
        put_f64(p, r->len_m);
        p += 8;
        put_f64(p, r->walk_min);
        p += 8;
        *p++ = (unsigned char)(r->open ? 1 : 0);
    }
    body = (size_t)(p - buf);
    crc = nav_crc32(buf, body);
    put_u32(buf + body, (unsigned long)crc);

    fp = platform_fopen(path, "wb");
    if (fp == NULL) {
        free(buf);
        nav_err_set(err, cap, "无法写入快照：%s", path);
        return NAV_ERR_IO;
    }
    if (fwrite(buf, 1, body + CRC_TAIL, fp) != body + CRC_TAIL) {
        fclose(fp);
        free(buf);
        nav_err_set(err, cap, "写入快照失败：%s", path);
        return NAV_ERR_IO;
    }
    fclose(fp);
    free(buf);
    return NAV_OK;
}

static unsigned char *read_whole_file(const char *path, size_t *size, char *err, size_t cap)
{
    FILE *fp;
    long len;
    unsigned char *buf;

    fp = platform_fopen(path, "rb");
    if (fp == NULL) {
        nav_err_set(err, cap, "无法打开快照：%s", path);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        nav_err_set(err, cap, "快照不可寻址：%s", path);
        return NULL;
    }
    len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        nav_err_set(err, cap, "无法获取快照长度：%s", path);
        return NULL;
    }
    if (len < 36 || len > 64L * 1024L * 1024L) {
        fclose(fp);
        nav_err_set(err, cap, "快照长度异常：%ld 字节", len);
        return NULL;
    }
    rewind(fp);
    buf = (unsigned char *)malloc((size_t)len);
    if (buf == NULL) {
        fclose(fp);
        nav_err_set(err, cap, "内存不足（快照 %ld 字节）", len);
        return NULL;
    }
    if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        free(buf);
        fclose(fp);
        nav_err_set(err, cap, "读取快照失败：%s", path);
        return NULL;
    }
    fclose(fp);
    *size = (size_t)len;
    return buf;
}

int campus_load_bin(Campus *c, const char *path, char *err, size_t cap)
{
    unsigned char *buf;
    size_t size = 0;
    size_t body;
    unsigned char *p;
    unsigned char *end;
    unsigned int ver = 0, nspots = 0, nroads = 0, title_len = 0;
    unsigned long v32 = 0;
    unsigned long crc_stored = 0;
    uint32_t crc_calc;
    Campus tmp;
    int i;

    if (c == NULL || path == NULL) {
        nav_err_set(err, cap, "参数为空");
        return NAV_ERR_ARG;
    }
    buf = read_whole_file(path, &size, err, cap);
    if (buf == NULL) {
        return NAV_ERR_IO;
    }
    body = size - CRC_TAIL;
    get_u32(buf + body, &crc_stored);
    crc_calc = nav_crc32(buf, body);
    if ((unsigned long)crc_calc != crc_stored) {
        free(buf);
        nav_err_set(err, cap, "快照校验失败（CRC32 0x%08lX != 0x%08lX），文件可能被截断或篡改",
                    (unsigned long)crc_calc, crc_stored);
        return NAV_ERR_FMT;
    }
    if (memcmp(buf, "NVG1", 4) != 0) {
        free(buf);
        nav_err_set(err, cap, "文件头不是校园导航快照（magic 应为 NVG1）");
        return NAV_ERR_FMT;
    }
    p = buf + 4;
    end = buf + body;
    get_u16(p, &ver);
    p += 2;
    if (ver != SNAPSHOT_VERSION) {
        free(buf);
        nav_err_set(err, cap, "快照版本 %u 不受支持（当前支持 %u）", ver, SNAPSHOT_VERSION);
        return NAV_ERR_FMT;
    }
    p += 2; /* flags */
    get_u32(p, &v32);
    nspots = (unsigned int)v32;
    p += 4;
    get_u32(p, &v32);
    nroads = (unsigned int)v32;
    p += 4;
    if (nspots < 1 || nspots > NAV_MAX_SPOTS) {
        free(buf);
        nav_err_set(err, cap, "快照地点数非法：%u", nspots);
        return NAV_ERR_FMT;
    }
    if (nroads > NAV_MAX_ROADS) {
        free(buf);
        nav_err_set(err, cap, "快照道路数非法：%u", nroads);
        return NAV_ERR_FMT;
    }
    campus_init(&tmp);
    get_f64(p, &tmp.width_m);
    p += 8;
    get_f64(p, &tmp.height_m);
    p += 8;
    get_u16(p, &title_len);
    p += 2;
    if (title_len >= NAV_NAME_MAX || p + title_len > end) {
        free(buf);
        nav_err_set(err, cap, "快照标题长度非法：%u", title_len);
        return NAV_ERR_FMT;
    }
    memcpy(tmp.title, p, title_len);
    tmp.title[title_len] = '\0';
    p += title_len;

    for (i = 0; i < (int)nspots; ++i) {
        Spot s;
        unsigned int type = 0, nlen = 0, blen = 0;
        memset(&s, 0, sizeof(s));
        if (p + 24 > end) {
            free(buf);
            nav_err_set(err, cap, "快照在读取第 %d 个地点时截断", i + 1);
            return NAV_ERR_FMT;
        }
        get_u32(p, &v32);
        s.id = (int)v32;
        p += 4;
        get_u16(p, &type);
        s.type = (SpotType)type;
        p += 2;
        get_f64(p, &s.x);
        p += 8;
        get_f64(p, &s.y);
        p += 8;
        get_u16(p, &nlen);
        p += 2;
        if (nlen >= NAV_NAME_MAX || p + nlen + 2 > end) {
            free(buf);
            nav_err_set(err, cap, "快照地点 %d 名称长度非法", i + 1);
            return NAV_ERR_FMT;
        }
        memcpy(s.name, p, nlen);
        s.name[nlen] = '\0';
        p += nlen;
        get_u16(p, &blen);
        p += 2;
        if (blen >= NAV_BRIEF_MAX || p + blen > end) {
            free(buf);
            nav_err_set(err, cap, "快照地点 %d 简介长度非法", i + 1);
            return NAV_ERR_FMT;
        }
        memcpy(s.brief, p, blen);
        s.brief[blen] = '\0';
        p += blen;
        tmp.spots[i] = s;
    }
    tmp.n_spots = (int)nspots;

    for (i = 0; i < (int)nroads; ++i) {
        Road r;
        memset(&r, 0, sizeof(r));
        if (p + 29 > end) {
            free(buf);
            nav_err_set(err, cap, "快照在读取第 %d 条道路时截断", i + 1);
            return NAV_ERR_FMT;
        }
        get_u32(p, &v32);
        r.id = (int)v32;
        p += 4;
        get_u32(p, &v32);
        r.from = (int)v32;
        p += 4;
        get_u32(p, &v32);
        r.to = (int)v32;
        p += 4;
        get_f64(p, &r.len_m);
        p += 8;
        get_f64(p, &r.walk_min);
        p += 8;
        r.open = (*p++ != 0) ? 1 : 0;
        tmp.roads[i] = r;
    }
    tmp.n_roads = (int)nroads;

    if (p != end) {
        int extra = (int)(end - p);
        free(buf);
        nav_err_set(err, cap, "快照尾部有多余的 %d 字节", extra);
        return NAV_ERR_FMT;
    }
    free(buf);
    if (campus_validate(&tmp, err, cap) != NAV_OK) {
        return NAV_ERR_FMT;
    }
    *c = tmp;
    return NAV_OK;
}

/* ---------------- 编辑操作 ---------------- */

int campus_add_spot(Campus *c, const char *name, SpotType type, double x, double y,
                    const char *brief, char *err, size_t cap)
{
    Spot *s;
    int i;

    if (c == NULL || name == NULL || name[0] == '\0') {
        nav_err_set(err, cap, "地点名称不能为空");
        return NAV_ERR_ARG;
    }
    if (c->n_spots >= NAV_MAX_SPOTS) {
        nav_err_set(err, cap, "地点数已达上限 %d", NAV_MAX_SPOTS);
        return NAV_ERR_RANGE;
    }
    if (strlen(name) >= NAV_NAME_MAX) {
        nav_err_set(err, cap, "地点名称过长（上限 %d 字节）", NAV_NAME_MAX - 1);
        return NAV_ERR_FMT;
    }
    if (brief != NULL && strlen(brief) >= NAV_BRIEF_MAX) {
        nav_err_set(err, cap, "地点简介过长（上限 %d 字节）", NAV_BRIEF_MAX - 1);
        return NAV_ERR_FMT;
    }
    for (i = 0; i < c->n_spots; ++i) {
        if (strcmp(c->spots[i].name, name) == 0) {
            nav_err_set(err, cap, "地点名称已存在：%s", name);
            return NAV_ERR_ARG;
        }
    }
    if (x < 0.0 || x > c->width_m || y < 0.0 || y > c->height_m) {
        nav_err_set(err, cap, "坐标 (%.1f, %.1f) 超出校园范围 (0..%.0f, 0..%.0f)", x, y,
                    c->width_m, c->height_m);
        return NAV_ERR_RANGE;
    }
    s = &c->spots[c->n_spots];
    memset(s, 0, sizeof(*s));
    s->id = c->n_spots + 1;
    strcpy(s->name, name);
    s->type = type;
    s->x = x;
    s->y = y;
    if (brief != NULL) {
        strcpy(s->brief, brief);
    }
    ++c->n_spots;
    return NAV_OK;
}

int campus_remove_spot(Campus *c, int id, char *err, size_t cap)
{
    int i, w;

    if (c == NULL || id < 1 || id > c->n_spots) {
        nav_err_set(err, cap, "地点编号 %d 不存在", id);
        return NAV_ERR_RANGE;
    }
    if (c->n_spots == 1) {
        nav_err_set(err, cap, "至少需要保留 1 个地点");
        return NAV_ERR_ARG;
    }
    for (i = id; i < c->n_spots; ++i) {
        c->spots[i - 1] = c->spots[i];
    }
    --c->n_spots;
    for (i = 0; i < c->n_spots; ++i) {
        c->spots[i].id = i + 1;
    }

    /* 删除与该地点相连的道路，并重排端点编号。 */
    w = 0;
    for (i = 0; i < c->n_roads; ++i) {
        Road r = c->roads[i];
        if (r.from == id || r.to == id) {
            continue;
        }
        if (r.from > id) {
            --r.from;
        }
        if (r.to > id) {
            --r.to;
        }
        c->roads[w++] = r;
    }
    c->n_roads = w;
    for (i = 0; i < c->n_roads; ++i) {
        c->roads[i].id = i + 1;
    }
    return NAV_OK;
}

int campus_add_road(Campus *c, int a, int b, double len_m, double walk_min, char *err,
                    size_t cap)
{
    Road *r;

    if (c == NULL) {
        nav_err_set(err, cap, "数据集指针为空");
        return NAV_ERR_ARG;
    }
    if (a < 1 || a > c->n_spots || b < 1 || b > c->n_spots) {
        nav_err_set(err, cap, "端点编号越界（有效范围 1..%d）", c->n_spots);
        return NAV_ERR_RANGE;
    }
    if (a == b) {
        nav_err_set(err, cap, "不能添加自环道路（%d - %d）", a, b);
        return NAV_ERR_ARG;
    }
    if (!(len_m > 0.0)) {
        nav_err_set(err, cap, "道路长度必须为正数");
        return NAV_ERR_ARG;
    }
    if (!(walk_min > 0.0)) {
        nav_err_set(err, cap, "步行时间必须为正数");
        return NAV_ERR_ARG;
    }
    if (c->n_roads >= NAV_MAX_ROADS) {
        nav_err_set(err, cap, "道路数已达上限 %d", NAV_MAX_ROADS);
        return NAV_ERR_RANGE;
    }
    if (has_edge(c, a, b, -1)) {
        nav_err_set(err, cap, "地点 %s 与 %s 之间已有道路", c->spots[a - 1].name,
                    c->spots[b - 1].name);
        return NAV_ERR_ARG;
    }
    r = &c->roads[c->n_roads];
    memset(r, 0, sizeof(*r));
    r->id = c->n_roads + 1;
    r->from = a;
    r->to = b;
    r->len_m = len_m;
    r->walk_min = walk_min;
    r->open = 1;
    ++c->n_roads;
    return NAV_OK;
}

int campus_remove_road(Campus *c, int id, char *err, size_t cap)
{
    int i;

    if (c == NULL || id < 1 || id > c->n_roads) {
        nav_err_set(err, cap, "道路编号 %d 不存在", id);
        return NAV_ERR_RANGE;
    }
    for (i = id; i < c->n_roads; ++i) {
        c->roads[i - 1] = c->roads[i];
    }
    --c->n_roads;
    for (i = 0; i < c->n_roads; ++i) {
        c->roads[i].id = i + 1;
    }
    return NAV_OK;
}

int campus_update_road(Campus *c, int id, double len_m, double walk_min, char *err, size_t cap)
{
    if (c == NULL || id < 1 || id > c->n_roads) {
        nav_err_set(err, cap, "道路编号 %d 不存在", id);
        return NAV_ERR_RANGE;
    }
    if (!(len_m > 0.0)) {
        nav_err_set(err, cap, "道路长度必须为正数");
        return NAV_ERR_ARG;
    }
    if (!(walk_min > 0.0)) {
        nav_err_set(err, cap, "步行时间必须为正数");
        return NAV_ERR_ARG;
    }
    c->roads[id - 1].len_m = len_m;
    c->roads[id - 1].walk_min = walk_min;
    return NAV_OK;
}

int campus_set_road_open(Campus *c, int id, int open, char *err, size_t cap)
{
    if (c == NULL || id < 1 || id > c->n_roads) {
        nav_err_set(err, cap, "道路编号 %d 不存在", id);
        return NAV_ERR_RANGE;
    }
    c->roads[id - 1].open = open ? 1 : 0;
    return NAV_OK;
}

int campus_find_spot(const Campus *c, const char *key, int *id_out)
{
    int i;
    long v;

    if (c == NULL || key == NULL || id_out == NULL || key[0] == '\0') {
        return NAV_ERR_ARG;
    }
    if (parse_int(key, &v) == NAV_OK) {
        if (v >= 1 && v <= c->n_spots) {
            *id_out = (int)v;
            return NAV_OK;
        }
        return NAV_ERR_RANGE;
    }
    for (i = 0; i < c->n_spots; ++i) {
        if (strcmp(c->spots[i].name, key) == 0) {
            *id_out = c->spots[i].id;
            return NAV_OK;
        }
    }
    return NAV_ERR_RANGE;
}
