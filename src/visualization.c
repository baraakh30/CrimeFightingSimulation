#include "../include/visualization.h"
#include "../include/utils.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global state pointer for visualization access
static SharedState *g_shared_state = NULL;
static SimConfig *g_config = NULL;
static int g_window_id = 0;
static int g_window_width = WINDOW_WIDTH;
static int g_window_height = WINDOW_HEIGHT;
static volatile sig_atomic_t g_viz_shutdown_requested = 0;
static int g_viz_initialized = 0;

int init_visualization(int argc, char **argv, SharedState *shared_state, SimConfig *config) {
    g_shared_state = shared_state;
    g_config = config;
    
    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    
    int result = setup_visualization("Secret Agent Simulation");
    if (result == 0) {
        g_viz_initialized = 1;
    }
    return result;
}
void request_visualization_shutdown() {
    g_viz_shutdown_requested = 1;
    
    if (glutGetWindow() ) {
        glutDestroyWindow(glutGetWindow());
    }
}
int setup_visualization(const char *window_title) {
    // Create window
    g_window_id = glutCreateWindow(window_title);
    if (g_window_id == 0) {
        log_message("Failed to create visualization window");
        return -1;
    }
    
    // Set background color
    glClearColor(COLOR_BACKGROUND, 1.0f);
    
    // Register callbacks
    glutDisplayFunc(display_callback);
    glutIdleFunc(idle_callback);
    glutKeyboardFunc(keyboard_callback);
    glutSpecialFunc(special_callback);
    glutMouseFunc(mouse_callback);
    glutReshapeFunc(reshape_callback);
    
    // Start timer for periodic refresh
    glutTimerFunc(VIZ_REFRESH_RATE, timer_callback, 0);
    
    return 0;
}

void display_callback(void) {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Reset modelview matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    if (!g_shared_state) {
        render_string(10, 20, FONT_TITLE, "Shared state not initialized");
        glutSwapBuffers();
        return;
    }
    
    // Lock the shared state mutex for reading
    pthread_mutex_lock(&g_shared_state->status_mutex);
    
    // Render gangs information
    float y_offset = 50;
    glColor3f(COLOR_TITLE);
    render_string(50, 30, FONT_TITLE, "Criminal Gangs");
    
    for (int i = 0; i < g_shared_state->gang_count; i++) {
        render_gang_box(50, y_offset, &g_shared_state->gangs[i]);
        y_offset += 120;
        
        // Start new column if we're running out of space
        if (y_offset > g_window_height - 150) {
            y_offset = 50;
            glutSwapBuffers();
        }
    }
    
    // Render police information
    render_police_box(g_window_width - 350, 50, g_shared_state);
    
    // Render statistics
    render_statistics(g_window_width - 350, g_window_height - 200, g_shared_state, g_config);
    
    // Render status message
    render_status_message(g_shared_state->status);
    
    // Unlock the mutex
    pthread_mutex_unlock(&g_shared_state->status_mutex);
    
    // Swap buffers
    glutSwapBuffers();
}

void idle_callback(void) {
    // Do nothing in idle, we'll use timer callback instead
}

void keyboard_callback(unsigned char key, int x, int y) {
    switch (key) {
        case 'q':
        case 'Q':
        case 27: // ESC
            glutDestroyWindow(g_window_id);
            shutdown_visualization();
            exit(0);
            break;
    }
}

void special_callback(int key, int x, int y) {
    // Handle special keys if needed
}

void mouse_callback(int button, int state, int x, int y) {
    // Handle mouse events if needed
}

void timer_callback(int value) {
    // Check if shutdown was requested
    if (g_viz_shutdown_requested) {
        if (glutGetWindow()) {
            glutDestroyWindow(glutGetWindow());
        }
        g_viz_initialized = 0;
        return;
    }
    
    // Request display refresh
    glutPostRedisplay();
    
    // Reset timer
    glutTimerFunc(VIZ_REFRESH_RATE, timer_callback, 0);
}


void reshape_callback(int width, int height) {
    g_window_width = width;
    g_window_height = height;
    
    // Adjust viewport
    glViewport(0, 0, width, height);
    
    // Set up orthographic projection
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, width, height, 0);
    
    // Reset modelview
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void render_string(float x, float y, void *font, const char *text) {
    glRasterPos2f(x, y);
    
    for (const char *c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

void render_rectangle(float x, float y, float width, float height, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
    
    // Add a border
    glColor3f(0.5f, 0.5f, 0.5f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void render_gang_box(float x, float y, Gang *gang) {
    char buffer[MAX_TEXT_LENGTH];
    
    // Draw background
    render_rectangle(x, y, 300, 100, COLOR_GANG_BG);
    
    // Draw gang title
    glColor3f(COLOR_TITLE);
    snprintf(buffer, MAX_TEXT_LENGTH, "Gang #%d", gang->id);
    render_string(x + 10, y + 20, FONT_TITLE, buffer);
    
    // Draw active missions info
    glColor3f(COLOR_TEXT);
    snprintf(buffer, MAX_TEXT_LENGTH, "Active Missions: %d/%d", gang->active_mission_count, MAX_CONCURRENT_MISSIONS);
    render_string(x + 10, y + 40, FONT_NORMAL, buffer);
    
    // Show details of first active mission (if any)
    Mission *first_mission = NULL;
    for (int i = 0; i < MAX_CONCURRENT_MISSIONS; i++) {
        if (gang->missions[i].mission_id != -1 && gang->missions[i].in_progress) {
            first_mission = &gang->missions[i];
            break;
        }
    }
    
    if (first_mission) {
        snprintf(buffer, MAX_TEXT_LENGTH, "Next Target: %s", get_target_name(first_mission->target));
        render_string(x + 10, y + 55, FONT_NORMAL, buffer);
        
        snprintf(buffer, MAX_TEXT_LENGTH, "Prep Required: %d%% (%d members)", 
                 (int)(first_mission->required_preparation_level * 100),
                 first_mission->assigned_count);
        render_string(x + 10, y + 70, FONT_NORMAL, buffer);
    } else {
        snprintf(buffer, MAX_TEXT_LENGTH, "No active missions");
        render_string(x + 10, y + 55, FONT_NORMAL, buffer);
    }
    
    // Draw member count
    snprintf(buffer, MAX_TEXT_LENGTH, "Members: %d", gang->member_count);
    render_string(x + 10, y + 85, FONT_NORMAL, buffer);
    
    // Draw mission stats
    snprintf(buffer, MAX_TEXT_LENGTH, "Success: %d / Fails: %d", 
             gang->successful_missions, gang->failed_missions);
    render_string(x + 10, y + 100, FONT_NORMAL, buffer);
    
    // Draw member icons
    float member_x = x + 200;
    float member_y = y + 30;
    float icon_size = 10;
    int row_count = 0;
    
    for (int i = 0; i < gang->member_count; i++) {
        render_member_icon(member_x, member_y, icon_size, &gang->members[i]);
        member_x += icon_size + 5;
        row_count++;
        
        // Wrap to next row after 8 icons
        if (row_count >= 8) {
            row_count = 0;
            member_x = x + 200;
            member_y += icon_size + 5;
        }
    }
}

void render_police_box(float x, float y, SharedState *shared_state) {
    char buffer[MAX_TEXT_LENGTH];
    
    // Draw background
    render_rectangle(x, y, 300, 120, COLOR_POLICE_BG);
    
    // Draw title
    glColor3f(COLOR_TITLE);
    render_string(x + 10, y + 20, FONT_TITLE, "Police Department");
    
    // Count active agents - only count up to agent_count instead of MAX_AGENTS
    int active_agents = 0;
    int dead_agents = 0;
    int uncovered_agents = 0;
    
    for (int i = 0; i < shared_state->agent_count; i++) {
        if (shared_state->agent_statuses[i] == AGENT_STATUS_ACTIVE) {
            active_agents++;
        } else if (shared_state->agent_statuses[i] == AGENT_STATUS_DEAD) {
            dead_agents++;
        } else if (shared_state->agent_statuses[i] == AGENT_STATUS_UNCOVERED) {
            uncovered_agents++;
        }
    }
    
    // Draw agent stats
    glColor3f(COLOR_TEXT);
    snprintf(buffer, MAX_TEXT_LENGTH, "Active Agents: %d", active_agents);
    render_string(x + 10, y + 45, FONT_NORMAL, buffer);
    
    snprintf(buffer, MAX_TEXT_LENGTH, "Uncovered Agents: %d", uncovered_agents);
    render_string(x + 10, y + 60, FONT_NORMAL, buffer);
    
    snprintf(buffer, MAX_TEXT_LENGTH, "Dead Agents: %d", dead_agents);
    render_string(x + 10, y + 75, FONT_NORMAL, buffer);
    
    // Draw thwarted/failed mission stats
    snprintf(buffer, MAX_TEXT_LENGTH, "Thwarted Plans: %d", shared_state->total_thwarted_plans);
    render_string(x + 10, y + 90, FONT_NORMAL, buffer);
    
    snprintf(buffer, MAX_TEXT_LENGTH, "Successful Gang Plans: %d", shared_state->total_successful_plans);
    render_string(x + 10, y + 105, FONT_NORMAL, buffer);
}

void render_statistics(float x, float y, SharedState *shared_state, SimConfig *config) {
    char buffer[MAX_TEXT_LENGTH];
    
    // Draw background
    render_rectangle(x, y, 300, 150, 0.2f, 0.2f, 0.2f);
    
    // Draw title
    glColor3f(COLOR_TITLE);
    render_string(x + 10, y + 20, FONT_TITLE, "Simulation Statistics");
    
    // Draw stats
    glColor3f(COLOR_TEXT);
    
    // Show win conditions
    snprintf(buffer, MAX_TEXT_LENGTH, "Police win at: %d thwarted plans", 
             config->police_thwart_win_count);
    render_string(x + 10, y + 45, FONT_NORMAL, buffer);
    
    snprintf(buffer, MAX_TEXT_LENGTH, "Gangs win at: %d successful plans", 
             config->gang_success_win_count);
    render_string(x + 10, y + 60, FONT_NORMAL, buffer);
    
    snprintf(buffer, MAX_TEXT_LENGTH, "Agents lose at: %d executed agents", 
             shared_state->agent_execution_loss_count);

    render_string(x + 10, y + 75, FONT_NORMAL, buffer);
    
    // Show current progress
    float police_progress = (float)shared_state->total_thwarted_plans / config->police_thwart_win_count;
    float gang_progress = (float)shared_state->total_successful_plans / config->gang_success_win_count;
    float agent_loss_progress = (float)shared_state->total_executed_agents / shared_state->agent_execution_loss_count;
    
    if (police_progress > 1.0f) police_progress = 1.0f;
    if (gang_progress > 1.0f) gang_progress = 1.0f;
    if (agent_loss_progress > 1.0f) agent_loss_progress = 1.0f;
    
    // Render progress bars
    render_string(x + 10, y + 95, FONT_NORMAL, "Police Progress:");
    render_progress_bar(x + 120, y + 95, 160, 10, police_progress, 0.2f, 0.6f, 1.0f);
    
    render_string(x + 10, y + 115, FONT_NORMAL, "Gang Progress:");
    render_progress_bar(x + 120, y + 115, 160, 10, gang_progress, 0.8f, 0.2f, 0.2f);
    
    render_string(x + 10, y + 135, FONT_NORMAL, "Agent Losses:");
    render_progress_bar(x + 120, y + 135, 160, 10, agent_loss_progress, 0.8f, 0.8f, 0.2f);
}

void render_member_icon(float x, float y, float size, GangMember *member) {
    // Choose color based on status
    switch (member->status) {
        case MEMBER_STATUS_ACTIVE:
            if (member->is_agent) {
                glColor3f(COLOR_AGENT); // Secret agent color (but only we can see it)
            } else {
                glColor3f(COLOR_MEMBER); // Regular member
            }
            break;
        case MEMBER_STATUS_ARRESTED:
            glColor3f(COLOR_WARNING); // Arrested
            break;
        case MEMBER_STATUS_DEAD:
            glColor3f(COLOR_FAILURE); // Dead
            break;
        case MEMBER_STATUS_EXECUTED:
            glColor3f(COLOR_FAILURE); // Executed agent
            break;
    }
    
    // Draw member icon (circle)
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y); // center
    
    // Draw a circle with 8 segments
    for (int i = 0; i <= 8; i++) {
        float angle = 2.0f * M_PI * i / 8;
        glVertex2f(x + size * cos(angle), y + size * sin(angle));
    }
    
    glEnd();
    
    // Add border
    glColor3f(0.0f, 0.0f, 0.0f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 8; i++) {
        float angle = 2.0f * M_PI * i / 8;
        glVertex2f(x + size * cos(angle), y + size * sin(angle));
    }
    glEnd();
    
    // Draw rank indicator (small dot inside for higher ranks)
    if (member->rank > 0) {
        // Higher rank = brighter indicator
        float brightness = 0.5f + (member->rank / 10.0f) * 0.5f;
        glColor3f(brightness, brightness, brightness);
        
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y); // center
        
        // Smaller inner circle, size based on rank
        float inner_size = size * 0.3f * (member->rank / 5.0f);
        for (int i = 0; i <= 8; i++) {
            float angle = 2.0f * M_PI * i / 8;
            glVertex2f(x + inner_size * cos(angle), y + inner_size * sin(angle));
        }
        
        glEnd();
    }
}

void render_progress_bar(float x, float y, float width, float height, 
                        float progress, float r, float g, float b) {
    // Background
    render_rectangle(x, y, width, height, 0.3f, 0.3f, 0.3f);
    
    // Progress
    render_rectangle(x, y, width * progress, height, r, g, b);
    
    // Border
    glColor3f(0.7f, 0.7f, 0.7f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);
    glEnd();
}

void render_target_info(float x, float y, CrimeTarget target) {
    char buffer[MAX_TEXT_LENGTH];
    
    // Draw target name
    glColor3f(COLOR_TEXT);
    snprintf(buffer, MAX_TEXT_LENGTH, "Target: %s", get_target_name(target));
    render_string(x, y, FONT_NORMAL, buffer);
}

void render_status_message(SimulationStatus status) {
    char buffer[MAX_TEXT_LENGTH];
    
    // Draw status box at bottom of screen
    render_rectangle(10, g_window_height - 40, g_window_width - 20, 30, COLOR_STATUS_BG);
    
    // Set status message based on simulation state
    switch (status) {
        case SIM_STATUS_RUNNING:
            snprintf(buffer, MAX_TEXT_LENGTH, "Simulation Running");
            glColor3f(COLOR_TEXT);
            break;
        case SIM_STATUS_POLICE_WIN:
            snprintf(buffer, MAX_TEXT_LENGTH, "SIMULATION ENDED: Police successfully thwarted enough criminal plans!");
            glColor3f(COLOR_SUCCESS);
            break;
        case SIM_STATUS_GANGS_WIN:
            snprintf(buffer, MAX_TEXT_LENGTH, "SIMULATION ENDED: Criminal gangs have succeeded too many times!");
            glColor3f(COLOR_FAILURE);
            break;
        case SIM_STATUS_AGENTS_LOST:
            snprintf(buffer, MAX_TEXT_LENGTH, "SIMULATION ENDED: Too many secret agents have been discovered and executed!");
            glColor3f(COLOR_WARNING);
            break;
        default:
            snprintf(buffer, MAX_TEXT_LENGTH, "Unknown Status");
            glColor3f(COLOR_TEXT);
            break;
    }
    
    // Center text in status bar
    float text_width = 0;
    for (const char *c = buffer; *c != '\0'; c++) {
        text_width += glutBitmapWidth(FONT_TITLE, *c);
    }
    
    float x = (g_window_width - text_width) / 2;
    render_string(x, g_window_height - 25, FONT_TITLE, buffer);
}

void shutdown_visualization(void) {
    if (g_viz_initialized) {
        g_viz_shutdown_requested = 1;
        // Give a little time for the visualization thread to clean up
        usleep(100000);  // 100ms
    }
}