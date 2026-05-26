/*
 * cluster.c — 4-way flood fill connected components on 16×16 ToF grid.
 *
 * Algorithm:
 *   Scan grid cell by cell. When an unvisited occupied cell is found,
 *   flood-fill using a queue. Bounding box for centroid + average confidence.
 *
 * Memory: 256 bool visited + max 256 ints in BFS queue.
 * Stack usage: ~300 bytes. Safe for ESP32-S3 FreeRTOS tasks.
 */

#include "cluster.h"
#include <string.h>

/* BFS queue — bounded, no dynamic alloc */
typedef struct {
    uint8_t row;
    uint8_t col;
} queue_elem_t;

static queue_elem_t bfs_queue[TOF_GRID_SIZE * TOF_GRID_SIZE];
static uint16_t     q_head, q_tail;

static inline void q_push(uint8_t r, uint8_t c) {
    bfs_queue[q_tail].row = r;
    bfs_queue[q_tail].col = c;
    q_tail++;
}

static inline bool q_pop(uint8_t *r, uint8_t *c) {
    if (q_head >= q_tail) return false;
    *r = bfs_queue[q_head].row;
    *c = bfs_queue[q_head].col;
    q_head++;
    return true;
}

static inline bool is_occupied(uint16_t dist_mm) {
    return (dist_mm > 0 && dist_mm < CEILING_HEIGHT_MM);
}

cluster_result_t find_clusters_4way(
    const uint16_t grid[TOF_GRID_SIZE][TOF_GRID_SIZE],
    const uint8_t confidence[TOF_GRID_SIZE][TOF_GRID_SIZE])
{
    bool visited[TOF_GRID_SIZE][TOF_GRID_SIZE];
    memset(visited, 0, sizeof(visited));

    cluster_result_t result;
    memset(&result, 0, sizeof(result));

    /* Cell size in meters */
    const float cell_w = 4.2f / TOF_GRID_SIZE;  /* 0.2625 m */
    const float cell_h = 3.0f / TOF_GRID_SIZE;  /* 0.1875 m */

    for (uint8_t i = 0; i < TOF_GRID_SIZE; i++) {
        for (uint8_t j = 0; j < TOF_GRID_SIZE; j++) {

            if (visited[i][j] || !is_occupied(grid[i][j]))
                continue;

            /* Start new cluster — flood fill */
            q_head = 0;
            q_tail = 0;
            q_push(i, j);
            visited[i][j] = true;

            float sum_x = 0, sum_y = 0;
            uint32_t sum_conf = 0;
            uint8_t cell_count = 0;

            while (q_pop(&i, &j)) {
                /* Skip out-of-bounds or visited */
                /* Centroid: cell center position */
                float cx = ((float)j - 7.5f) * cell_w;  /* j=0..15, center at j-7.5 */
                float cy = ((float)i - 7.5f) * cell_h;

                sum_x += cx;
                sum_y += cy;
                sum_conf += confidence[i][j];
                cell_count++;

                /* Neighbors: up, down, left, right */
                const int8_t dirs[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};
                for (int d = 0; d < 4; d++) {
                    int8_t ni = (int8_t)i + dirs[d][0];
                    int8_t nj = (int8_t)j + dirs[d][1];
                    if (ni >= 0 && ni < TOF_GRID_SIZE &&
                        nj >= 0 && nj < TOF_GRID_SIZE &&
                        !visited[ni][nj] && is_occupied(grid[ni][nj]))
                    {
                        visited[ni][nj] = true;
                        q_push((uint8_t)ni, (uint8_t)nj);
                    }
                }
            }

            if (cell_count >= MIN_CLUSTER_CELLS && result.count < MAX_CLUSTERS) {
                cluster_t *c = &result.clusters[result.count];
                c->x_world_m = sum_x / (float)cell_count;
                c->y_world_m = sum_y / (float)cell_count;
                c->cells = cell_count;
                c->avg_conf = (uint8_t)(sum_conf / cell_count);
                result.count++;
            }
        }
    }

    return result;
}
