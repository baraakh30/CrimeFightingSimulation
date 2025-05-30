#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


void set_default_config(SimConfig *config) {
    config->num_gangs = 3;
    config->min_members_per_gang = 5;
    config->max_members_per_gang = 10;
    config->num_ranks = 5;
    config->agent_infiltration_rate = 0.3;
    config->preparation_time_min = 10;
    config->preparation_time_max = 30;
    config->false_info_probability = 0.2;
    config->mission_success_rate_base = 0.7;
    config->mission_kill_probability = 0.1;
    config->agent_suspicion_threshold = 0.7;
    config->police_confirmation_threshold = 0.6;
    config->prison_time = 10;
    config->police_thwart_win_count = 10;
    config->gang_success_win_count = 10;
    config->agent_execution_loss_count = 5;
    config->info_spread_delay = 1;
    config->member_knowledge_transfer_rate = 0.05f;
    config->member_knowledge_rank_factor = 0.1f;
    config->member_knowledge_lucky_chance = 0.2f;
    config->base_preparation_increment = 0.05f;
    config->rank_preparation_bonus = 0.1f;
    config->min_preparation_required_base = 0.6f;
    config->min_preparation_difficulty_factor = 0.3f;
    config->agent_report_knowledge_reset = 0.2f;
    config->promotion_base_chance = 0.2f;
    config->promotion_rank_factor = 0.2f;
    config->agent_base_suspicion = 0.2f;
    config->knowledge_anomaly_suspicion = 0.15f;
    config->min_agent_report_time = 3;
    config->target_difficulty_base = 0.5f;
    config->target_difficulty_scaling = 0.5f;
    config->info_spread_base_value = 0.05f;
    config->info_spread_rank_factor = 0.25f;
    config->preparation_knowledge_factor = 0.5f;
    config->preparation_rank_factor = 0.02f;
    config->agent_initial_knowledge_threshold = 0.1f;
    config->agent_knowledge_report_threshold = 0.7f;
    config->agent_discovery_threshold = 0.7f;
    config->agent_knowledge_gain = 0.03f;
    config->max_agents_per_gang = 2;
}


static char* trim(char *str) {
    char *end;
    
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0)  // All spaces?
        return str;
    
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator character
    end[1] = '\0';
    
    return str;
}


 int parse_config_line(const char *line, char *key, char *value, size_t key_size, size_t value_size) {
    const char *separator;
    size_t key_len, value_len;
    
    // Skip comments and empty lines
    if (line[0] == '#' || line[0] == '\0' || isspace(line[0]))
        return 0;
    
    // Find the separator (=)
    separator = strchr(line, '=');
    if (!separator)
        return 0;
    
    // Calculate lengths
    key_len = separator - line;
    value_len = strlen(separator + 1);
    
    // Check buffer sizes
    if (key_len >= key_size || value_len >= value_size)
        return 0;
    
    // Copy key and value
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    strcpy(value, separator + 1);
    
    // Trim whitespace
    trim(key);
    trim(value);
    
    return 1;
}


int load_config(const char *filename, SimConfig *config) {
    FILE *file;
    char line[256];
    char key[128];
    char value[128];
    
    // Set default values first
    set_default_config(config);
    
    // Open the configuration file
    file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open configuration file");
        return -1;
    }
    
    // Read and parse each line
    while (fgets(line, sizeof(line), file)) {
        if (parse_config_line(line, key, value, sizeof(key), sizeof(value))) {
            if (strcmp(key, "num_gangs") == 0) {
                config->num_gangs = atoi(value);
            } else if (strcmp(key, "min_members_per_gang") == 0) {
                config->min_members_per_gang = atoi(value);
            } else if (strcmp(key, "max_members_per_gang") == 0) {
                config->max_members_per_gang = atoi(value);
            } else if (strcmp(key, "num_ranks") == 0) {
                config->num_ranks = atoi(value);
            } else if (strcmp(key, "agent_infiltration_rate") == 0) {
                config->agent_infiltration_rate = atof(value);
            } else if (strcmp(key, "preparation_time_min") == 0) {
                config->preparation_time_min = atoi(value);
            } else if (strcmp(key, "preparation_time_max") == 0) {
                config->preparation_time_max = atoi(value);
            } else if (strcmp(key, "false_info_probability") == 0) {
                config->false_info_probability = atof(value);
            } else if (strcmp(key, "mission_success_rate_base") == 0) {
                config->mission_success_rate_base = atof(value);
            } else if (strcmp(key, "mission_kill_probability") == 0) {
                config->mission_kill_probability = atof(value);
            } else if (strcmp(key, "agent_suspicion_threshold") == 0) {
                config->agent_suspicion_threshold = atof(value);
            } else if (strcmp(key, "police_confirmation_threshold") == 0) {
                config->police_confirmation_threshold = atof(value);
            } else if (strcmp(key, "prison_time") == 0) {
                config->prison_time = atoi(value);
            } else if (strcmp(key, "police_thwart_win_count") == 0) {
                config->police_thwart_win_count = atoi(value);
            } else if (strcmp(key, "gang_success_win_count") == 0) {
                config->gang_success_win_count = atoi(value);
            } else if (strcmp(key, "agent_execution_loss_count") == 0) {
                config->agent_execution_loss_count = atoi(value);
            } else if (strcmp(key, "info_spread_delay") == 0) {
                config->info_spread_delay = atoi(value);
            } 
            else if (strcmp(key, "member_knowledge_transfer_rate") == 0) {
                config->member_knowledge_transfer_rate = atof(value);
            } else if (strcmp(key, "member_knowledge_rank_factor") == 0) {
                config->member_knowledge_rank_factor = atof(value);
            } else if (strcmp(key, "member_knowledge_lucky_chance") == 0) {
                config->member_knowledge_lucky_chance = atof(value);
            } else if (strcmp(key, "base_preparation_increment") == 0) {
                config->base_preparation_increment = atof(value);
            } else if (strcmp(key, "rank_preparation_bonus") == 0) {
                config->rank_preparation_bonus = atof(value);
            } else if (strcmp(key, "min_preparation_required_base") == 0) {
                config->min_preparation_required_base = atof(value);
            } else if (strcmp(key, "min_preparation_difficulty_factor") == 0) {
                config->min_preparation_difficulty_factor = atof(value);
            } else if (strcmp(key, "promotion_base_chance") == 0) {
                config->promotion_base_chance = atof(value);
            } else if (strcmp(key, "promotion_rank_factor") == 0) {
                config->promotion_rank_factor = atof(value);
            } else if (strcmp(key, "target_difficulty_base") == 0) {
                config->target_difficulty_base = atof(value);
            } else if (strcmp(key, "target_difficulty_scaling") == 0) {
                config->target_difficulty_scaling = atof(value);
            } else if (strcmp(key, "info_spread_base_value") == 0) {
                config->info_spread_base_value = atof(value);
            } else if (strcmp(key, "info_spread_rank_factor") == 0) {
                config->info_spread_rank_factor = atof(value);
            } else if (strcmp(key, "preparation_knowledge_factor") == 0) {
                config->preparation_knowledge_factor = atof(value);
            } else if (strcmp(key, "preparation_rank_factor") == 0) {
                config->preparation_rank_factor = atof(value);
            } else if (strcmp(key, "agent_knowledge_gain") == 0) {
                config->agent_knowledge_gain = atof(value);
            } else if (strcmp(key, "agent_report_knowledge_reset") == 0) {
                config->agent_report_knowledge_reset = atof(value);
            } else if (strcmp(key, "agent_base_suspicion") == 0) {
                config->agent_base_suspicion = atof(value);
            } else if (strcmp(key, "knowledge_anomaly_suspicion") == 0) {
                config->knowledge_anomaly_suspicion = atof(value);
            } else if (strcmp(key, "min_agent_report_time") == 0) {
                config->min_agent_report_time = atoi(value);
            } else if (strcmp(key, "agent_initial_knowledge_threshold") == 0) {
                config->agent_initial_knowledge_threshold = atof(value);
            } else if (strcmp(key, "agent_knowledge_report_threshold") == 0) {
                config->agent_knowledge_report_threshold = atof(value);
            } else if (strcmp(key, "agent_discovery_threshold") == 0) {
                config->agent_discovery_threshold = atof(value);
            }
            else if (strcmp(key, "max_agents_per_gang") == 0) {
                config->max_agents_per_gang = atoi(value);
            }
            // Ignore unknown keys
        }
    }
    
    fclose(file);
    return 0;
}

void print_config(SimConfig *config) {
    printf("Simulation Configuration:\n");
    printf("------------------------\n");
    printf("Number of gangs: %d\n", config->num_gangs);
    printf("Members per gang: %d-%d\n", config->min_members_per_gang, config->max_members_per_gang);
    printf("Number of ranks: %d\n", config->num_ranks);
    printf("Agent infiltration rate: %.2f\n", config->agent_infiltration_rate);
    printf("Preparation time range: %d-%d seconds\n", 
           config->preparation_time_min, config->preparation_time_max);
    printf("False information probability: %.2f\n", config->false_info_probability);
    printf("Mission base success rate: %.2f\n", config->mission_success_rate_base);
    printf("Mission kill probability: %.2f\n", config->mission_kill_probability);
    printf("Agent suspicion threshold: %.2f\n", config->agent_suspicion_threshold);
    printf("Police confirmation threshold: %.2f\n", config->police_confirmation_threshold);
    printf("Prison time: %d seconds\n", config->prison_time);
    printf("Win conditions:\n");
    printf("  - Police win after %d thwarted plans\n", config->police_thwart_win_count);
    printf("  - Gangs win after %d successful plans\n", config->gang_success_win_count);
    printf("  - Gangs win after %d executed agents\n", config->agent_execution_loss_count);
    printf("Information spread delay: %d seconds\n", config->info_spread_delay);
    printf("Member knowledge transfer rate: %.2f\n", config->member_knowledge_transfer_rate);
    printf("Member knowledge rank factor: %.2f\n", config->member_knowledge_rank_factor);
    printf("Member knowledge lucky chance: %.2f\n", config->member_knowledge_lucky_chance);
    printf("Base preparation increment: %.2f\n", config->base_preparation_increment);
    printf("Rank preparation bonus: %.2f\n", config->rank_preparation_bonus);
    printf("Minimum preparation required base: %.2f\n", config->min_preparation_required_base);
    printf("Minimum preparation difficulty factor: %.2f\n", config->min_preparation_difficulty_factor);
    printf("Promotion base chance: %.2f\n", config->promotion_base_chance);
    printf("Promotion rank factor: %.2f\n", config->promotion_rank_factor);
    printf("Target difficulty base: %.2f\n", config->target_difficulty_base);
    printf("Target difficulty scaling: %.2f\n", config->target_difficulty_scaling);
    printf("Information spread base value: %.2f\n", config->info_spread_base_value);
    printf("Information spread rank factor: %.2f\n", config->info_spread_rank_factor);
    printf("Preparation knowledge factor: %.2f\n", config->preparation_knowledge_factor);
    printf("Preparation rank factor: %.2f\n", config->preparation_rank_factor);
    printf("Agent knowledge gain: %.2f\n", config->agent_knowledge_gain);
    printf("Agent report knowledge reset: %.2f\n", config->agent_report_knowledge_reset);
    printf("Agent base suspicion: %.2f\n", config->agent_base_suspicion);
    printf("Knowledge anomaly suspicion: %.2f\n", config->knowledge_anomaly_suspicion);
    printf("Minimum agent report time: %d\n", config->min_agent_report_time);
    printf("Agent initial knowledge threshold: %.2f\n", config->agent_initial_knowledge_threshold);
    printf("Agent knowledge report threshold: %.2f\n", config->agent_knowledge_report_threshold);
    printf("Agent discovery threshold: %.2f\n", config->agent_discovery_threshold);
    printf("Maximum Agents Per Gang: %d\n", config->max_agents_per_gang);

    printf("------------------------\n");
}

