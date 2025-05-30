#ifndef SIMULATION_H
#define SIMULATION_H

#include "common.h"
#include <GL/glut.h>

/* Visualization element positions and sizes */

#define GANG_DISPLAY_X 50
#define GANG_DISPLAY_Y 50
#define GANG_DISPLAY_WIDTH 200
#define GANG_DISPLAY_HEIGHT 100
#define POLICE_DISPLAY_X 900
#define POLICE_DISPLAY_Y 50
#define POLICE_DISPLAY_WIDTH 250
#define POLICE_DISPLAY_HEIGHT 150
#define STATUS_DISPLAY_X 50
#define STATUS_DISPLAY_Y 700
#define STATUS_DISPLAY_WIDTH 1100
#define STATUS_DISPLAY_HEIGHT 50

/* OpenGL colors */
typedef struct {
    float r;
    float g;
    float b;
} Color;

/* Structure for visualization thread data */
typedef struct {
    SharedState *shared_state;
    SimConfig *config;
    int argc;
    char **argv;
} VisualizationThreadArgs;


int simulation_init(SimConfig *config, const char *config_file);
int run_simulation(SimConfig *config, int argc, char **argv);
int create_ipc_resources(int *shared_state_id, int *msg_queue_id, SimConfig *config);
int spawn_gang_processes(SharedState *shared_state, SimConfig *config, int msg_queue_id, int shared_state_id);
pid_t spawn_police_process(SimConfig *config, int msg_queue_id, int shared_state_id);
void *visualization_thread(void *args);
void shutdown_simulation(int shared_state_id, int msg_queue_id);
void cleanup_ipc_resources(int shared_state_id, int msg_queue_id);
void *simulation_monitor_thread(void *args);

#endif /* SIMULATION_H */