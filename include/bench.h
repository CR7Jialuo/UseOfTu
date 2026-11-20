/* bench.h —— 随机图算法基准测试，用于报告中的性能对比。 */
#ifndef BENCH_H
#define BENCH_H

#include <stddef.h>

/* 在 n 个节点的随机校园图上对比 Dijkstra / Floyd / Bellman-Ford / Prim / Kruskal。
 * seed 控制随机数；repeat 为 Dijkstra 重复次数（取平均）。
 * 结果以表格形式打印到 stdout。 */
int bench_run(int n, unsigned int seed, int repeat, char *err, size_t cap);

#endif /* BENCH_H */
