#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include "common.h"
#include <GL/glut.h>

/* Visualization constants */
#define VIZ_REFRESH_RATE 30  /* Refresh rate in ms */
#define MAX_TEXT_LENGTH 256
#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800
#define COLOR_BACKGROUND   0.1f, 0.1f, 0.1f
#define COLOR_GANG_BG      0.2f, 0.2f, 0.3f
#define COLOR_POLICE_BG    0.2f, 0.3f, 0.2f
#define COLOR_STATUS_BG    0.3f, 0.2f, 0.2f
#define COLOR_TEXT         1.0f, 1.0f, 1.0f
#define COLOR_TITLE        1.0f, 0.8f, 0.2f
#define COLOR_AGENT        0.2f, 0.6f, 1.0f
#define COLOR_MEMBER       0.7f, 0.7f, 0.7f
#define COLOR_SUCCESS      0.2f, 0.8f, 0.2f
#define COLOR_FAILURE      0.8f, 0.2f, 0.2f
#define COLOR_WARNING      0.8f, 0.8f, 0.2f
#define COLOR_SELECTED     1.0f, 0.5f, 0.0f

#define FONT_TITLE         GLUT_BITMAP_HELVETICA_18
#define FONT_NORMAL        GLUT_BITMAP_HELVETICA_12
#define FONT_SMALL         GLUT_BITMAP_HELVETICA_10


int init_visualization(int argc, char **argv, SharedState *shared_state, SimConfig *config);

int setup_visualization(const char *window_title);

void display_callback(void);


void idle_callback(void);


void keyboard_callback(unsigned char key, int x, int y);

void special_callback(int key, int x, int y);
void mouse_callback(int button, int state, int x, int y);
void timer_callback(int value);
void reshape_callback(int width, int height);
void render_string(float x, float y, void *font, const char *text);
void render_rectangle(float x, float y, float width, float height, float r, float g, float b);
void render_gang_box(float x, float y, Gang *gang);
void render_police_box(float x, float y, SharedState *shared_state);
void render_statistics(float x, float y, SharedState *shared_state, SimConfig *config);
void render_member_icon(float x, float y, float size, GangMember *member);
void render_progress_bar(float x, float y, float width, float height, float progress, float r, float g, float b);
void render_target_info(float x, float y, CrimeTarget target);
void render_status_message(SimulationStatus status);
void shutdown_visualization(void);
#endif /* VISUALIZATION_H */