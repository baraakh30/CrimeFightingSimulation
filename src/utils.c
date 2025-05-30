#include "../include/utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// Target name strings
static const char* target_names[] = {
    "Bank Robbery",
    "Jewelry Shop Robbery",
    "Drug Trafficking",
    "Art Theft",
    "Kidnapping",
    "Blackmail",
    "Arm Trafficking"
};

// Status strings
static const char* member_status_strings[] = {
    "Active",
    "Arrested",
    "Dead",
    "Executed"
};

static const char* agent_status_strings[] = {
    "Active",
    "Uncovered",
    "Dead"
};

static const char* simulation_status_strings[] = {
    "Running",
    "Police Win",
    "Gangs Win",
    "Agents Lost"
};

// Initialize random number generator
void init_random(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((unsigned int)(tv.tv_sec * 1000 + tv.tv_usec / 1000));
}

// Generate a random float between 0.0 and 1.0
float rand_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

// Generate a random integer in the specified range (inclusive)
int rand_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

// Log a message with timestamp
void log_message(const char *format, ...) {
    char buffer[256];
    char timestamp[32];
    va_list args;
    
    format_timestamp(timestamp, sizeof(timestamp));
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    printf("[%s] %s\n", timestamp, buffer);
    fflush(stdout);
}

// Format current timestamp
void format_timestamp(char *buffer, size_t size) {
    time_t now;
    struct tm *tm_info;
    
    time(&now);
    tm_info = localtime(&now);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Get string name for crime target enum
const char* get_target_name(CrimeTarget target) {
    if (target >= 0 && target < TARGET_COUNT) {
        return target_names[target];
    }
    return "Unknown";
}

// Create a named semaphore
sem_t* create_named_semaphore(int id, int initial_value) {
    char name[32];
    snprintf(name, sizeof(name), "/gang_sem_%d", id);
    
    sem_t *sem = sem_open(name, O_CREAT | O_EXCL, 0644, initial_value);
    if (sem == SEM_FAILED) {
        // Try to unlink and recreate
        sem_unlink(name);
        sem = sem_open(name, O_CREAT, 0644, initial_value);
    }
    
    return (sem == SEM_FAILED) ? NULL : sem;
}

// Calculate time delay based on preparation level
int calculate_time_delay(int base_time, float preparation_level) {
    // Lower preparation means longer time
    float factor = 1.0 - preparation_level;
    return (int)(base_time * (1.0 + factor));
}

// Safely copy a string with bounds checking
char* safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (dest == NULL || src == NULL || dest_size == 0) {
        return dest;
    }
    
    size_t to_copy = strlen(src);
    if (to_copy >= dest_size) {
        to_copy = dest_size - 1;
    }
    
    memcpy(dest, src, to_copy);
    dest[to_copy] = '\0';
    
    return dest;
}

// Validate configuration parameters
int validate_config(SimConfig *config) {
    if (config->num_gangs <= 0 || config->num_gangs > MAX_GANGS) {
        log_message("Invalid number of gangs: %d (max: %d)", config->num_gangs, MAX_GANGS);
        return 0;
    }
    
    if (config->min_members_per_gang <= 0 || config->max_members_per_gang > MAX_MEMBERS) {
        log_message("Invalid member range: %d-%d (max: %d)", 
                   config->min_members_per_gang, config->max_members_per_gang, MAX_MEMBERS);
        return 0;
    }
    
    if (config->num_ranks <= 0 || config->num_ranks > MAX_RANKS) {
        log_message("Invalid number of ranks: %d (max: %d)", config->num_ranks, MAX_RANKS);
        return 0;
    }
    
    if (config->agent_infiltration_rate < 0.0 || config->agent_infiltration_rate > 1.0) {
        log_message("Invalid agent infiltration rate: %.2f (should be 0.0-1.0)", 
                   config->agent_infiltration_rate);
        return 0;
    }
    
    return 1;
}

// Print help information
void print_help(const char *program_name) {
    printf("Usage: %s [config_file]\n", program_name);
    printf("\n");
    printf("If config isnt valid, the program will use default values\n");
}

// Convert member status to string
const char* member_status_to_string(MemberStatus status) {
    if (status >= 0 && status <= MEMBER_STATUS_EXECUTED) {
        return member_status_strings[status];
    }
    return "Unknown";
}

// Convert agent status to string
const char* agent_status_to_string(AgentStatus status) {
    if (status >= 0 && status <= AGENT_STATUS_DEAD) {
        return agent_status_strings[status];
    }
    return "Unknown";
}

// Convert simulation status to string
const char* simulation_status_to_string(SimulationStatus status) {
    if (status >= 0 && status <= SIM_STATUS_AGENTS_LOST) {
        return simulation_status_strings[status];
    }
    return "Unknown";
}

// Sleep for a random time within a range
void random_sleep(int min_ms, int max_ms) {
    int sleep_time = rand_range(min_ms, max_ms);
    usleep(sleep_time * 1000);
}

// Generate a member name for visualization
void generate_member_name(int gang_id, int member_id, char *buffer, size_t size) {
    snprintf(buffer, size, "G%d-M%d", gang_id, member_id);
}