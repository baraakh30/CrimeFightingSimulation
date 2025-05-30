#include "../include/simulation.h"
#include "../include/ipc.h"
#include "../include/visualization.h"
#include "../include/utils.h"
#include "../include/config.h"
#include "../include/gang.h"
#include "../include/police.h"

#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

// Global variables for signal handling
static int g_shared_state_id = -1;
static int g_msg_queue_id = -1;
static pid_t *g_gang_pids = NULL;
static pid_t g_police_pid = -1;
static int g_gang_count = 0;
static pthread_t g_viz_thread = 0;
static pthread_t g_monitor_thread = 0;
static volatile sig_atomic_t g_shutdown_flag = 0;


int simulation_init(SimConfig *config, const char *config_file) {
    if (!config || !config_file) {
        log_message("Invalid parameters for simulation initialization");
        return -1;
    }
    
    // Load configuration from file
    if (load_config(config_file, config) != 0) {
        log_message("Failed to load configuration from %s", config_file);
        return -1;
    }
    
    // Validate configuration
    if (!validate_config(config)) {
        log_message("Invalid configuration parameters");
        return -1;
    }
    
    // Initialize random number generator
    init_random();
    
    log_message("Simulation initialized with %d gangs", config->num_gangs);
    return 0;
}

int run_simulation(SimConfig *config, int argc, char **argv) {
    int shared_state_id, msg_queue_id;
    SharedState *shared_state;
    int status;
    
    log_message("Starting secret agent simulation");
    
    // Create IPC resources
    if (create_ipc_resources(&shared_state_id, &msg_queue_id, config) != 0) {
        log_message("Failed to create IPC resources");
        return -1;
    }
    
    // Save global references for signal handler
    g_shared_state_id = shared_state_id;
    g_msg_queue_id = msg_queue_id;
    
    // Attach to shared memory
    shared_state = (SharedState *)attach_shared_memory(shared_state_id);
    if (!shared_state) {
        log_message("Failed to attach to shared memory");
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    // Initialize shared state
    if (init_shared_state(shared_state) != 0) {
        log_message("Failed to initialize shared state");
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    // Set initial simulation status
    shared_state->status = SIM_STATUS_RUNNING;
    shared_state->gang_count = config->num_gangs;
    shared_state->agent_execution_loss_count = config->agent_execution_loss_count;
    // Create visualization thread
    VisualizationThreadArgs *viz_args = malloc(sizeof(VisualizationThreadArgs));
    if (!viz_args) {
        log_message("Failed to allocate memory for visualization thread arguments");
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    viz_args->shared_state = shared_state;
    viz_args->config = config;
    viz_args->argc = argc;
    viz_args->argv = argv;
    
    if (pthread_create(&g_viz_thread, NULL, visualization_thread, viz_args) != 0) {
        log_message("Failed to create visualization thread");
        free(viz_args);
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    // Create monitor thread
    if (pthread_create(&g_monitor_thread, NULL, simulation_monitor_thread, shared_state) != 0) {
        log_message("Failed to create monitor thread");
        // Cancel visualization thread
        pthread_cancel(g_viz_thread);
        pthread_join(g_viz_thread, NULL);
        free(viz_args);
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    // Spawn gang processes
    g_gang_count = spawn_gang_processes(shared_state, config, msg_queue_id, shared_state_id);
    if (g_gang_count <= 0) {
        log_message("Failed to spawn gang processes");
        pthread_cancel(g_viz_thread);
        pthread_cancel(g_monitor_thread);
        pthread_join(g_viz_thread, NULL);
        pthread_join(g_monitor_thread, NULL);
        free(viz_args);
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    // Spawn police process
    g_police_pid = spawn_police_process(config, msg_queue_id, shared_state_id);
    if (g_police_pid <= 0) {
        log_message("Failed to spawn police process");
        // Kill gang processes
        for (int i = 0; i < g_gang_count; i++) {
            if (g_gang_pids[i] > 0) {
                kill(g_gang_pids[i], SIGTERM);
            }
        }
        pthread_cancel(g_viz_thread);
        pthread_cancel(g_monitor_thread);
        pthread_join(g_viz_thread, NULL);
        pthread_join(g_monitor_thread, NULL);
        free(viz_args);
        free(g_gang_pids);
        g_gang_pids = NULL;
        detach_shared_memory(shared_state);
        cleanup_ipc_resources(shared_state_id, msg_queue_id);
        return -1;
    }
    
    log_message("Simulation started with %d gangs and police process", g_gang_count);
    
    // Wait for all child processes to finish
    while (!g_shutdown_flag) {
        pid_t terminated_pid = waitpid(-1, &status, 0);
        if (terminated_pid == -1) {
            if (errno == EINTR) {
                // Interrupted by signal, check if we should shut down
                if (g_shutdown_flag) {
                    break;
                }
                continue;
            } else {
                // No more child processes
                break;
            }
        }
        
        if (terminated_pid == g_police_pid) {
            log_message("Police process terminated with status %d", status);
            g_shutdown_flag = 1;
        } else {
            // Check if it was a gang process
            for (int i = 0; i < g_gang_count; i++) {
                if (terminated_pid == g_gang_pids[i]) {
                    log_message("Gang %d process terminated with status %d", i, status);
                    g_gang_pids[i] = -1;
                    break;
                }
            }
        }
        
        // Check if all gang processes have terminated
        bool all_gangs_terminated = true;
        for (int i = 0; i < g_gang_count; i++) {
            if (g_gang_pids[i] > 0) {
                all_gangs_terminated = false;
                break;
            }
        }
        
        if (all_gangs_terminated && g_police_pid <= 0) {
            g_shutdown_flag = 1;
        }
    }
    
    // Wait for visualization thread to finish
    pthread_join(g_viz_thread, NULL);
    pthread_join(g_monitor_thread, NULL);
    free(viz_args);
    
    // Cleanup
    log_message("Simulation ended");
    detach_shared_memory(shared_state);
    cleanup_ipc_resources(shared_state_id, msg_queue_id);
    free(g_gang_pids);
    g_gang_pids = NULL;
    
    return 0;
}

int create_ipc_resources(int *shared_state_id, int *msg_queue_id, SimConfig *config) {
    size_t shared_mem_size = sizeof(SharedState);
    
    // Create shared memory segment
    *shared_state_id = create_shared_memory(shared_mem_size);
    if (*shared_state_id == -1) {
        log_message("Failed to create shared memory segment");
        return -1;
    }
    
    // Create message queue
    *msg_queue_id = init_message_queue();
    if (*msg_queue_id == -1) {
        log_message("Failed to create message queue");
        remove_shared_memory(*shared_state_id);
        return -1;
    }
    
    return 0;
}

int spawn_gang_processes(SharedState *shared_state, SimConfig *config, int msg_queue_id, int shared_state_id) {
    int gang_count = config->num_gangs;
    
    // Allocate array to store gang process IDs
    g_gang_pids = (pid_t *)malloc(gang_count * sizeof(pid_t));
    if (!g_gang_pids) {
        log_message("Failed to allocate memory for gang PIDs");
        return -1;
    }
    
    // Initialize gang PIDs to invalid value
    for (int i = 0; i < gang_count; i++) {
        g_gang_pids[i] = -1;
    }
    
    // Create gang processes
    for (int i = 0; i < gang_count; i++) {
        // Determine number of members for this gang
        int member_count = rand_range(config->min_members_per_gang, config->max_members_per_gang);
        
        // Initialize gang structure in shared memory
        gang_init(&shared_state->gangs[i], i, member_count, config);
        
        // Fork gang process
        pid_t pid = fork();
        if (pid < 0) {
            log_message("Failed to fork gang process %d", i);
            // Kill already created gang processes
            for (int j = 0; j < i; j++) {
                if (g_gang_pids[j] > 0) {
                    kill(g_gang_pids[j], SIGTERM);
                }
            }
            return -1;
        } else if (pid == 0) {
            // Child process - gang
            gang_process_main(i, config, msg_queue_id, shared_state_id);
            exit(0);
        } else {
            // Parent process
            g_gang_pids[i] = pid;
            log_message("Created gang %d with PID %d and %d members", 
                       i, pid, member_count);
        }
    }
    
    return gang_count;
}

pid_t spawn_police_process(SimConfig *config, int msg_queue_id, int shared_state_id) {
    pid_t pid = fork();
    
    if (pid < 0) {
        log_message("Failed to fork police process");
        return -1;
    } else if (pid == 0) {
        // Child process - police
        police_process_main(config, msg_queue_id, shared_state_id);
        exit(0);
    } else {
        // Parent process
        log_message("Created police process with PID %d", pid);
        return pid;
    }
}

void *visualization_thread(void *args) {
    VisualizationThreadArgs *viz_args = (VisualizationThreadArgs *)args;
    
    // Set cancellation state
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    
    // Initialize visualization
    if (init_visualization(viz_args->argc, viz_args->argv, viz_args->shared_state, viz_args->config) != 0) {
        log_message("Failed to initialize visualization");
        return NULL;
    }
    
    // Enter GLUT main loop
    glutMainLoop();
    
    return NULL;
}

void *simulation_monitor_thread(void *args) {
    SharedState *shared_state = (SharedState *)args;
    SimulationStatus prev_status = SIM_STATUS_RUNNING;
    
    while (!g_shutdown_flag) {
        // Lock mutex to read shared state
        pthread_mutex_lock(&shared_state->status_mutex);
        
        // Check if simulation status has changed
        if (shared_state->status != prev_status) {
            log_message("Simulation status changed to: %s", 
                       simulation_status_to_string(shared_state->status));
            prev_status = shared_state->status;
            
            // If simulation ended, initiate shutdown
            if (shared_state->status != SIM_STATUS_RUNNING) {
                g_shutdown_flag = 1;
            }
        }
        
        pthread_mutex_unlock(&shared_state->status_mutex);
        
        // Sleep briefly
        usleep(500000); // 500ms
    }
    
    // Initiate shutdown if not already done
    if (!g_shutdown_flag) {
        shutdown_simulation(g_shared_state_id, g_msg_queue_id);
    }
    
    return NULL;
}

void shutdown_simulation(int shared_state_id, int msg_queue_id) {
    log_message("Shutting down simulation...");
    g_shutdown_flag = 1;
    
    // First, request visualization shutdown before accessing shared memory
    // This prevents X11 errors when terminating
    if (g_viz_thread) {
        shutdown_visualization();
    }
    
    // Attach to shared memory to get process IDs
    SharedState *shared_state = (SharedState *)attach_shared_memory(shared_state_id);
    if (!shared_state) {
        log_message("Error: Cannot attach to shared memory during shutdown");
    } else {
        // Update simulation status to terminate
        pthread_mutex_lock(&shared_state->status_mutex);
        shared_state->status = SIM_STATUS_SHUTDOWN;
        pthread_mutex_unlock(&shared_state->status_mutex);
        
        // Send shutdown message to all processes
        IpcMessage msg;
        msg.mtype = MSG_TYPE_SIMULATION_STATUS;
        msg.data.status = SIM_STATUS_SHUTDOWN;
        
        if (send_message(msg_queue_id, &msg) != 0) {
            log_message("Failed to send shutdown message");
        }
        
        // Detach from shared memory after use
        detach_shared_memory(shared_state);
    }

    // Drain the message queue of any pending messages to prevent processing during shutdown
    IpcMessage msg;
    while (receive_message(msg_queue_id, &msg, 0, true) == 0) {
        // Just drain the queue, don't process
    }

    // Give processes time to terminate
    usleep(500000); // 500ms
    
    // Force kill any remaining processes
    if (g_gang_pids) {
        for (int i = 0; i < g_gang_count; i++) {
            if (g_gang_pids[i] > 0) {
                if (kill(g_gang_pids[i], 0) == 0) {
                    log_message("Sending SIGTERM to gang %d (PID %d)", i, g_gang_pids[i]);
                    kill(g_gang_pids[i], SIGTERM);
                }
            }
        }
    }
    
    if (g_police_pid > 0 && kill(g_police_pid, 0) == 0) {
        log_message("Sending SIGTERM to police (PID %d)", g_police_pid);
        kill(g_police_pid, SIGTERM);
    }
    
    // Give processes time to terminate after SIGTERM
    usleep(500000); // 500ms
    
    // Force kill with SIGKILL if needed
    if (g_gang_pids) {
        for (int i = 0; i < g_gang_count; i++) {
            if (g_gang_pids[i] > 0 && kill(g_gang_pids[i], 0) == 0) {
                log_message("Force killing gang %d (PID %d)", i, g_gang_pids[i]);
                kill(g_gang_pids[i], SIGKILL);
            }
        }
    }
    
    if (g_police_pid > 0 && kill(g_police_pid, 0) == 0) {
        log_message("Force killing police (PID %d)", g_police_pid);
        kill(g_police_pid, SIGKILL);
    }
    
    // Cleanup monitor thread
    if (g_monitor_thread) {
        pthread_cancel(g_monitor_thread);
        pthread_join(g_monitor_thread, NULL);
    }
    
    // Cleanup visualization thread last
    if (g_viz_thread) {
        pthread_cancel(g_viz_thread);
        pthread_join(g_viz_thread, NULL);
    }
    
    // Finally clean up IPC resources
    cleanup_ipc_resources(shared_state_id, msg_queue_id);
}

// Improve the cleanup_ipc_resources function:
void cleanup_ipc_resources(int shared_state_id, int msg_queue_id) {
    // Remove message queue
    if (msg_queue_id != -1) {
        struct msqid_ds queue_info;
        if (msgctl(msg_queue_id, IPC_STAT, &queue_info) == 0) {
            if (msgctl(msg_queue_id, IPC_RMID, NULL) != 0) {
                log_message("Failed to remove message queue: %s", strerror(errno));
            } else {
                log_message("Message queue removed");
            }
        }
    }
    
    // Remove shared memory
    if (shared_state_id != -1) {
        struct shmid_ds shm_info;
        if (shmctl(shared_state_id, IPC_STAT, &shm_info) == 0) {
            if (remove_shared_memory(shared_state_id) != 0) {
                log_message("Failed to remove shared memory: %s", strerror(errno));
            } else {
                log_message("Shared memory removed");
            }
        }
    }
    
    // Cleanup semaphores that might have been left open
    for (int i = 0; i < MAX_GANGS; i++) {
        char sem_name[32];
        snprintf(sem_name, sizeof(sem_name), "/gang_sem_%d", i);
        sem_unlink(sem_name);
    }
}