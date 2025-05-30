#ifndef UTILS_H
#define UTILS_H

#include "common.h"
#include <stdarg.h>


void log_message(const char *format, ...);
const char* get_target_name(CrimeTarget target);
float rand_float(void);
int rand_range(int min, int max);
sem_t* create_named_semaphore(int id, int initial_value);
int calculate_time_delay(int base_time, float preparation_level);
void format_timestamp(char *buffer, size_t size);
char* safe_strcpy(char *dest, const char *src, size_t dest_size);
int validate_config(SimConfig *config);
void print_help(const char *program_name);
const char* member_status_to_string(MemberStatus status);
const char* agent_status_to_string(AgentStatus status);
const char* simulation_status_to_string(SimulationStatus status);
void init_random(void);
void random_sleep(int min_ms, int max_ms);
void generate_member_name(int gang_id, int member_id, char *buffer, size_t size);

#endif /* UTILS_H */