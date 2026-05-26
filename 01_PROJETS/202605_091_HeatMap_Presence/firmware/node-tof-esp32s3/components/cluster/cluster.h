#pragma once
#include <stdint.h>
#include <stdbool.h>

#define TOF_GRID_SIZE    16
#define MAX_CLUSTERS     5
#define CEILING_HEIGHT_MM 3000
#define MIN_CLUSTER_CELLS 2

typedef struct {
    float   x_world_m;     /* centroid X in world coords */
    float   y_world_m;     /* centroid Y in world coords */
    uint8_t cells;          /* number of occupied cells */
    uint8_t avg_conf;       /* average confidence 0-15 */
} cluster_t;

typedef struct {
    uint8_t     count;
    cluster_t   clusters[MAX_CLUSTERS];
} cluster_result_t;

/*
 * find_clusters_4way — flood fill 4-connected on 16×16 grid.
 *
 * grid[i][j] = distance_mm from ToF sensor at cell (i,j).
 * A cell is "occupied" if 0 < distance < ceiling_height_mm.
 * Returns connected components with ≥ MIN_CLUSTER_CELLS cells.
 *
 * Local coordinate system: sensor at (0,0), looking down.
 * FOV 67.9°(H) × 52.8°(V), at 3m height → coverage ~4.2m × 3.0m.
 * Cell (i,j) → world coords:
 *   x_world = (j - 8.0) * (4.2/16)  — center of cell in meters
 *   y_world = (i - 8.0) * (3.0/16)
 */
cluster_result_t find_clusters_4way(const uint16_t grid[TOF_GRID_SIZE][TOF_GRID_SIZE],
                                    const uint8_t confidence[TOF_GRID_SIZE][TOF_GRID_SIZE]);
