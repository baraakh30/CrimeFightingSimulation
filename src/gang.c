#include "../include/gang.h"
#include "../include/ipc.h"

#include <fcntl.h>
static volatile sig_atomic_t gang_shutdown_requested = 0;
void gang_signal_handler(int sig) {
    gang_shutdown_requested = 1;
}
int gang_init(Gang *gang, int id, int member_count, SimConfig *config) {
    if (!gang || member_count <= 0 || member_count > MAX_MEMBERS) {
        return -1;
    }

    memset(gang, 0, sizeof(Gang));
    gang->id = id;
    gang->member_count = member_count;
    gang->successful_missions = 0;
    gang->failed_missions = 0;
    gang->plan_in_progress = false;
    gang->plan_disrupted = false;
    
    // Initialize members
    for (int i = 0; i < member_count; i++) {
        gang->members[i].id = i;
        gang->members[i].gang_id = id;
        gang->members[i].rank = 0; // Start at lowest rank
        gang->members[i].is_agent = false;
        gang->members[i].agent_id = -1;
        gang->members[i].status = MEMBER_STATUS_ACTIVE;
        gang->members[i].preparation_level = 0.0f;
        gang->members[i].knowledge_level = 0.0f;
        gang->members[i].release_time = 0;
    }
    
    // Create member semaphore
    char sem_name[32];
    snprintf(sem_name, sizeof(sem_name), "/gang_sem_%d", id);
    gang->member_semaphore = sem_open(sem_name, O_CREAT, 0644, 1);
    if (gang->member_semaphore == SEM_FAILED) {
        perror("sem_open failed");
        return -1;
    }
    
    return 0;
}

void gang_process_main(int gang_id, SimConfig *config, int msg_queue_id, int shared_mem_id) {
        signal(SIGTERM, gang_signal_handler);
    signal(SIGINT, gang_signal_handler);
    // Attach to shared memory
    SharedState *shared_state = (SharedState *)attach_shared_memory(shared_mem_id);
    if (!shared_state) {
        log_message("Gang %d: Failed to attach to shared memory", gang_id);
        exit(EXIT_FAILURE);
    }
    
    // Initialize gang
    Gang *gang = &shared_state->gangs[gang_id];
    gang->process_id = getpid();
    gang->shared_state = shared_state;
    log_message("Gang %d: Process started (PID: %d)", gang_id, gang->process_id);
    
    // Create gang member threads
    pthread_t member_threads[MAX_MEMBERS];
    GangMemberArgs *member_args = malloc(sizeof(GangMemberArgs) * gang->member_count);
    
    if (!member_args) {
        log_message("Gang %d: Failed to allocate memory for member thread arguments", gang_id);
        detach_shared_memory(shared_state);
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < gang->member_count; i++) {
        member_args[i].member_index = i;
        member_args[i].gang = gang;
        member_args[i].config = config;
        member_args[i].msg_queue_id = msg_queue_id;
        member_args[i].shared_state = shared_state;
        
        if (pthread_create(&member_threads[i], NULL, gang_member_thread, &member_args[i]) != 0) {
            log_message("Gang %d: Failed to create member thread %d", gang_id, i);
            continue;
        }
        
        gang->members[i].thread_id = member_threads[i];
    }
    
    // Main gang process loop
    IpcMessage message;
    bool running = true;
    
    // Select initial target
    select_new_target(gang, config);
    
    while (running && !gang_shutdown_requested) {
        // Check for messages from police
        if (receive_message(msg_queue_id, &message, MSG_TYPE_POLICE_ORDER, true) == 0) {
            if (message.data.gang_id == gang_id) {
                log_message("Gang %d: Received arrest order for %d days", gang_id, message.data.arrest_duration);
                process_arrest(gang, message.data.arrest_duration);
                gang->plan_disrupted = true;
            }
        }
                // Check simulation status
        if (shared_state->status != SIM_STATUS_RUNNING || gang_shutdown_requested) {
            running = false;
            log_message("Gang %d: Detected shutdown request", gang_id);
            break;
        }
        // Update shared state
        pthread_mutex_lock(&shared_state->status_mutex);
        memcpy(&shared_state->gangs[gang_id], gang, sizeof(Gang));
        pthread_mutex_unlock(&shared_state->status_mutex);
        
        // Check simulation status
        if (shared_state->status != SIM_STATUS_RUNNING) {
            running = false;
            continue;
        }
        
        // Main gang logic
        if (!gang->plan_in_progress) {
            // Start a new plan
            select_new_target(gang, config);
            gang->plan_in_progress = true;
            
            // Spread information about the plan
            spread_information(gang, config, false);
            
            // Occasionally spread false information
            if (rand_float() < config->false_info_probability) {
                spread_information(gang, config, true);
            }
            
            log_message("Gang %d: New target selected: %s", gang_id, get_target_name(gang->current_target));
        } else {
            // Check if preparation is complete
            bool all_prepared = true;
            for (int i = 0; i < gang->member_count; i++) {
                if (gang->members[i].status == MEMBER_STATUS_ACTIVE &&
                    gang->members[i].preparation_level < gang->required_preparation_level) {

                    all_prepared = false;
                    break;
                }
            }
            if (all_prepared && !gang->plan_disrupted) {
                // Execute the plan
                bool success = execute_plan(gang, config, msg_queue_id);
                if (success) {
                    gang->successful_missions++;
                    log_message("Gang %d: Mission successful! Total successful: %d", 
                               gang_id, gang->successful_missions);
                } else {
                    gang->failed_missions++;
                    log_message("Gang %d: Mission failed! Total failures: %d", 
                               gang_id, gang->failed_missions);
                }
                
                // Update statistics
                pthread_mutex_lock(&shared_state->status_mutex);
                if (success) {
                    shared_state->total_successful_plans++;
                }
                pthread_mutex_unlock(&shared_state->status_mutex);
                
                // Check if we should investigate for leaked information
                if (!success) {
                    investigate_for_agents(gang, config, msg_queue_id);
                }
                
                // Replace any dead members
                recruit_new_members(gang, config);
                
                // Promote members occasionally
                if (rand_float() <config->promotion_base_chance) {
                    promote_members(gang, config);
                }
                
                // Reset for next mission
                gang->plan_in_progress = false;
                gang->plan_disrupted = false;
            }
        }
        
        // Sleep to avoid consuming too much CPU
        usleep(100000); // 100ms
    }
    
    // Cleanup and exit
    log_message("Gang %d: Shutting down", gang_id);
    
    // Cancel all member threads
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].thread_id) {
            pthread_cancel(gang->members[i].thread_id);
        }
    }
    
    // Join all threads
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].thread_id) {
            pthread_join(gang->members[i].thread_id, NULL);
        }
    }
    
    gang_cleanup(gang);
    free(member_args);  // Don't forget to free allocated memory
    detach_shared_memory(shared_state);
    exit(EXIT_SUCCESS);
}

void select_new_target(Gang *gang, SimConfig *config) {
    // Pick a random target
    gang->current_target = rand() % TARGET_COUNT;
    
    // Set preparation time within configured range
    gang->target_preparation_time = rand_range(config->preparation_time_min, config->preparation_time_max);
    
    // Set required preparation level (scaled based on target difficulty)
    float target_difficulty = config->target_difficulty_base + 
    ((float)gang->current_target / TARGET_COUNT) * config->target_difficulty_scaling;
    gang->required_preparation_level = config->min_preparation_required_base + 
    rand_float() * config->min_preparation_difficulty_factor * target_difficulty;
    
    // Reset preparation levels
    for (int i = 0; i < gang->member_count; i++) {
        gang->members[i].preparation_level = 0.0f;
        gang->members[i].knowledge_level = 0.0f;
    }
}

void* gang_member_thread(void *arg) {
    GangMemberArgs *args = (GangMemberArgs *)arg;
    Gang *gang = args->gang;
    int member_index = args->member_index;
    SimConfig *config = args->config;
    int msg_queue_id = args->msg_queue_id;
    SharedState *shared_state = args->shared_state;
    GangMember *member = &gang->members[member_index];
    
    log_message("Gang %d, Member %d: Thread started", gang->id, member_index);
    
    while (1) {
        // Skip processing if member is arrested or dead
        if (member->status != MEMBER_STATUS_ACTIVE) {
            // If arrested, check if it's time to release
            if (member->status == MEMBER_STATUS_ARRESTED && time(NULL) >= member->release_time) {
                member->status = MEMBER_STATUS_ACTIVE;
                member->preparation_level = 0.0f;
                log_message("Gang %d, Member %d: Released from prison", gang->id, member_index);
            } else {
                // Sleep and continue
                usleep(500000); // 500ms
                continue;
            }
        }
        
        // Check if simulation is still running
        if (shared_state->status != SIM_STATUS_RUNNING) {
            break;
        }
        
        // Main member activities
        if (gang->plan_in_progress && !gang->plan_disrupted) {
            // Improve preparation level
            float prep_increment = config->base_preparation_increment + 
            (config->rank_preparation_bonus * member->rank / (float)config->num_ranks);

            member->preparation_level += prep_increment;
            if (member->preparation_level > 1.0f) {
                member->preparation_level = 1.0f;
            }
            
            // Secret agent activities (reporting to police)
            if (member->is_agent) {
                float suspicion_threshold = config->agent_suspicion_threshold;
                            // Add delay before agents can report
                static time_t first_knowledge_time = 0;
            if (member->knowledge_level > config->agent_initial_knowledge_threshold && 
                first_knowledge_time == 0) {
                first_knowledge_time = time(NULL);
            }
                               
                // Require at least some time to pass before reporting
                time_t min_time_before_report = config->min_agent_report_time;
                // Report to police when confidence is high enough AND enough time has passed
                if (member->knowledge_level > suspicion_threshold && 
                    time(NULL) - first_knowledge_time >= min_time_before_report) {
                    send_agent_report(msg_queue_id, member->agent_id, gang->id, 
                                     gang->current_target, member->knowledge_level, 
                                     time(NULL) + gang->target_preparation_time);
                    log_message("Gang %d, Agent %d: Reporting to police with confidence %.2f", 
                               gang->id, member->agent_id, member->knowledge_level);
                    
                    // Reset knowledge level to avoid constant reporting
                    member->knowledge_level *= config->agent_report_knowledge_reset;

                }
            }
            
            // Interact with other members to exchange information
            int target_index = rand() % gang->member_count;
            if (target_index != member_index && gang->members[target_index].status == MEMBER_STATUS_ACTIVE) {
                member_interaction(gang, member_index, target_index, config);
            }
        }
        
        // Random sleep to simulate varied activities
        usleep(rand_range(100000, 300000)); // 100-300ms
    }
    
    log_message("Gang %d, Member %d: Thread exiting", gang->id, member_index);
    return NULL;
}

void spread_information(Gang *gang, SimConfig *config, bool is_false) {
    CrimeTarget real_target = gang->current_target;
    
    // If spreading false information, select a different target
    if (is_false) {
        CrimeTarget false_target;
        do {
            false_target = rand() % TARGET_COUNT;
        } while (false_target == real_target);
        
        gang->current_target = false_target;
    }
    
    // Spread information based on rank
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status != MEMBER_STATUS_ACTIVE) {
            continue;
        }
        
        // Higher ranks get more accurate information
        float accuracy = (float)gang->members[i].rank / config->num_ranks;
        
        // If not spreading false info, or if member rank is high
        if (!is_false || rand_float() > accuracy) {
gang->members[i].knowledge_level += config->info_spread_base_value + 
                                   config->info_spread_rank_factor * accuracy;
            
            // Cap knowledge level
            if (gang->members[i].knowledge_level > 1.0f) {
                gang->members[i].knowledge_level = 1.0f;
            }
        }
    }
    
    // Reset target if we were spreading false info
    if (is_false) {
        gang->current_target = real_target;
    }
    
    // Simulate delay in information spreading
    usleep(config->info_spread_delay * 1000);
}

bool execute_plan(Gang *gang, SimConfig *config, int msg_queue_id) {
    log_message("Gang %d: Executing plan for target: %s", gang->id, get_target_name(gang->current_target));
    
    // Calculate success probability based on preparation and configuration
    float avg_preparation = 0.0f;
    int active_members = 0;
    
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_ACTIVE) {
            avg_preparation += gang->members[i].preparation_level;
            active_members++;
        }
    }
    
    if (active_members > 0) {
        avg_preparation /= active_members;
    } else {
        return false; // No active members, automatic failure
    }
    
    // Final success probability
    float success_prob = config->mission_success_rate_base * avg_preparation;
    
    // Check for mission success
    bool success = (rand_float() < success_prob);
    
    // Handle casualties
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_ACTIVE) {
            // Check if member dies during mission
            if (rand_float() < config->mission_kill_probability) {
                gang->members[i].status = MEMBER_STATUS_DEAD;
                log_message("Gang %d, Member %d: Died during mission", gang->id, i);
                
                // Handle agent death
                if (gang->members[i].is_agent) {
                    sem_wait(gang->member_semaphore);
                    update_agent_status(gang->shared_state, gang->members[i].agent_id, AGENT_STATUS_DEAD);
                    sem_post(gang->member_semaphore);
                }
            }
        }
    }
    
    return success;
}

void process_arrest(Gang *gang, int duration) {
    time_t release_time = time(NULL) + duration;
    
    // Arrest all active members
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_ACTIVE) {
            gang->members[i].status = MEMBER_STATUS_ARRESTED;
            gang->members[i].release_time = release_time;
            gang->members[i].preparation_level = 0.0f;
            log_message("Gang %d, Member %d: Arrested for %d seconds", gang->id, i, duration);
        }
    }
    
    gang->plan_disrupted = true;
}

void investigate_for_agents(Gang *gang, SimConfig *config, int msg_queue_id) {
    log_message("Gang %d: Starting internal investigation", gang->id);
    
    // Investigation is more thorough after failed missions
    // Start with higher ranks as they are more likely to have inside information
    for (int r = config->num_ranks - 1; r >= 0; r--) {
        for (int i = 0; i < gang->member_count; i++) {
            if (gang->members[i].status != MEMBER_STATUS_ACTIVE || gang->members[i].rank != r) {
                continue;
            }
            
            // Calculate suspicion level for this member
            float suspicion = 0.0f;
            
            // Agents might slip up when interacting or during missions
            if (gang->members[i].is_agent) {
                suspicion += config->agent_base_suspicion;  // Base chance for mistakes
                
                // Knowledge anomalies increase suspicion
                if (gang->members[i].knowledge_level < 0.5f * r / (float)config->num_ranks) {
                    suspicion += config->knowledge_anomaly_suspicion; // Unusually low knowledge for rank
                }
            }
            
            // Random factor in investigations
            suspicion += rand_float();
            // Check if agent is caught
            if (suspicion > config->agent_discovery_threshold && gang->members[i].is_agent) {
                log_message("Gang %d: Secret agent %d discovered!", gang->id, gang->members[i].agent_id);
                execute_agent(gang, i, msg_queue_id, gang->shared_state);
            }
        }
    }
}

void recruit_new_members(Gang *gang, SimConfig *config) {
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_DEAD || 
            gang->members[i].status == MEMBER_STATUS_EXECUTED) {
            
            // Recruit new member to replace
            gang->members[i].status = MEMBER_STATUS_ACTIVE;
            gang->members[i].rank = 0; // Start at lowest rank
            gang->members[i].is_agent = false; // New recruits aren't agents
            gang->members[i].agent_id = -1;
            gang->members[i].preparation_level = 0.0f;
            gang->members[i].knowledge_level = 0.0f;
            
            log_message("Gang %d: Recruited new member to replace %d", gang->id, i);
        }
    }
}

void promote_members(Gang *gang, SimConfig *config) {
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_ACTIVE && 
            gang->members[i].rank < config->num_ranks - 1) {
            
            // Chance of promotion increases with lower rank
float promotion_chance = config->promotion_base_chance * 
    (1.0f - (gang->members[i].rank / (float)config->num_ranks));            
            if (rand_float() < promotion_chance) {
                gang->members[i].rank++;
                log_message("Gang %d, Member %d: Promoted to rank %d", 
                           gang->id, i, gang->members[i].rank);
            }
        }
    }
}

void execute_agent(Gang *gang, int member_index, int msg_queue_id, SharedState *shared_state) {
    GangMember *member = &gang->members[member_index];
    
    if (!member->is_agent) {
        return; // Not an agent
    }
    
    log_message("Gang %d: Executing agent %d (member %d)", gang->id, member->agent_id, member_index);
    
    sem_wait(gang->member_semaphore);
    shared_state->agent_statuses[member->agent_id] = AGENT_STATUS_UNCOVERED;
    shared_state->total_executed_agents++;
    sem_post(gang->member_semaphore);
    update_agent_status(shared_state, member->agent_id, AGENT_STATUS_UNCOVERED);
    // Mark member as executed
    member->status = MEMBER_STATUS_EXECUTED;
    member->is_agent = false;
    member->agent_id = -1;
}

void member_interaction(Gang *gang, int member_index, int target_index, SimConfig *config) {
    GangMember *member = &gang->members[member_index];
    GangMember *target = &gang->members[target_index];
    
    // Only interact with active members
    if (target->status != MEMBER_STATUS_ACTIVE) {
        return;
    }
    
    // Knowledge flow is based on rank difference
    // Higher ranks give information to lower ranks
    if (member->rank >= target->rank) {
        // Knowledge flows from higher rank to lower rank
        float knowledge_transfer = config->member_knowledge_transfer_rate + 
    config->member_knowledge_rank_factor * (member->knowledge_level - target->knowledge_level);
        if (knowledge_transfer > 0) {
            target->knowledge_level += knowledge_transfer;
            if (target->knowledge_level > 1.0f) {
                target->knowledge_level = 1.0f;
            }
        }
    } else {
        // Lower ranks can sometimes get information from higher ranks if lucky
        if (rand_float() < config->member_knowledge_lucky_chance) {
float knowledge_transfer = config->member_knowledge_transfer_rate * 
    (target->knowledge_level - member->knowledge_level);
            if (knowledge_transfer > 0) {
                member->knowledge_level += knowledge_transfer;
                if (member->knowledge_level > 1.0f) {
                    member->knowledge_level = 1.0f;
                }
            }
        }
    }
    
    // Agents gain extra knowledge through interactions
    if (member->is_agent) {
        member->knowledge_level += config->agent_knowledge_gain;
        if (member->knowledge_level > 1.0f) {
            member->knowledge_level = 1.0f;
        }
    }
}

void update_preparation_levels(Gang *gang, SimConfig *config) {
    for (int i = 0; i < gang->member_count; i++) {
        if (gang->members[i].status == MEMBER_STATUS_ACTIVE) {
            // Preparation increases faster for members with higher rank and knowledge
            float prep_increment = config->base_preparation_increment + 
                                config->preparation_rank_factor * gang->members[i].rank / (float)config->num_ranks;
            prep_increment *= (config->preparation_knowledge_factor + 
                            config->preparation_knowledge_factor * gang->members[i].knowledge_level);




            gang->members[i].preparation_level += prep_increment;
            if (gang->members[i].preparation_level > 1.0f) {
                gang->members[i].preparation_level = 1.0f;
            }
        }
    }
}

void gang_cleanup(Gang *gang) {
    if (gang->member_semaphore != SEM_FAILED) {
        sem_close(gang->member_semaphore);
        
        char sem_name[32];
        snprintf(sem_name, sizeof(sem_name), "/gang_sem_%d", gang->id);
        sem_unlink(sem_name);
    }
}