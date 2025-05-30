#include "../include/main.h"
#include "../include/ipc.h"
#include "../include/visualization.h"


static volatile sig_atomic_t g_shutdown_in_progress = 0;
/* Global variables for signal handling */
static int g_shared_state_id = -1;
static int g_msg_queue_id = -1;

int main(int argc, char *argv[]) {
    SimConfig config;
    char config_file[256] = "config.txt";
    int result;

    /* Initialize random number generator */
    srand(time(NULL));
    
    /* Parse command line arguments */
    if (parse_arguments(argc, argv, config_file, sizeof(config_file)) != 0) {
        print_help(argv[0]);
        return 1;
    }
    
    /* Initialize environment */
    if (initialize_environment(&config, config_file) != 0) {
        log_message("Failed to initialize environment");
        return 1;
    }
    
    /* Register signal handlers */
    register_signal_handlers();
    
    /* Display welcome message */
    display_welcome(&config);
    
    /* Run the simulation */
    result = run_simulation(&config, argc, argv);
    
    return result;
}

int parse_arguments(int argc, char *argv[], char *config_file, size_t config_file_size) {
    // Check if exactly one argument is provided (the config file)
    if (argc != 2) {
        return 1;  // Show help if no config file provided
    }
    
    // Use the provided argument as config file
    safe_strcpy(config_file, argv[1], config_file_size);
    return 0;
}

int initialize_environment(SimConfig *config, const char *config_file) {
    /* Initialize logging */
    log_message("Initializing environment");
    
    /* Initialize random number generator */
    init_random();
    
    /* Read configuration */
    if (load_config(config_file, config) != 0) {
        log_message("Failed to load configuration from %s", config_file);
        return -1;
    }
    
    /* Validate configuration */
    if (!validate_config(config)) {
        log_message("Invalid configuration");
        return -1;
    }
    
    /* Print configuration */
    print_config(config);
    
    return 0;
}


void display_welcome(SimConfig *config) {
    printf("\n=================================================\n");
    printf("   Secret Agent Simulation System\n");
    printf("=================================================\n");
    printf("Gangs: %d\n", config->num_gangs);
    printf("Members per gang: %d-%d\n", config->min_members_per_gang, config->max_members_per_gang);
    printf("Gang ranks: %d\n", config->num_ranks);
    printf("Agent infiltration rate: %.2f\n", config->agent_infiltration_rate);
    printf("=================================================\n\n");
    
    log_message("Simulation starting with %d gangs", config->num_gangs);
}

/* Signal handler for graceful termination */
void signal_handler(int sig) {
    // Prevent multiple calls to signal handler
    if (g_shutdown_in_progress) {
        return;
    }
    
    g_shutdown_in_progress = 1;
    log_message("Received signal %d, shutting down...", sig);
    
    /* First shutdown visualization to prevent X11 errors */
    shutdown_visualization();
    
    /* Tell all child processes to terminate */
    if (g_msg_queue_id != -1) {
        IpcMessage msg;
        msg.mtype = MSG_TYPE_SIMULATION_STATUS;
        msg.data.status = SIM_STATUS_SHUTDOWN;
        
        /* Try to send message but don't block if queue is full */
        msgsnd(g_msg_queue_id, &msg, sizeof(msg.data), IPC_NOWAIT);
    }
    
    /* Update shared state status */
    if (g_shared_state_id != -1) {
        SharedState *state = (SharedState *)attach_shared_memory(g_shared_state_id);
        if (state) {
            pthread_mutex_lock(&state->status_mutex);
            state->status = SIM_STATUS_SHUTDOWN;
            pthread_mutex_unlock(&state->status_mutex);
            
            /* Send termination signals */
            for (int i = 0; i < state->gang_count; i++) {
                pid_t gang_pid = state->gangs[i].process_id;
                if (gang_pid > 0) {
                    kill(gang_pid, SIGTERM);
                }
            }
            
            detach_shared_memory(state);
        }
    }
    
    /* Wait a bit for children to react */
    usleep(500000);  // 500ms
    
    /* Cleanup resources */
    cleanup_ipc_resources(g_shared_state_id, g_msg_queue_id);
    
    exit(0);
}
void register_signal_handlers(void) {
    /* Register for SIGINT (Ctrl+C) */
    signal(SIGINT, signal_handler);
    
    /* Register for SIGTERM */
    signal(SIGTERM, signal_handler);
}
