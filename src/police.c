#include "../include/police.h"
#include "../include/ipc.h"
#include "../include/utils.h"
static volatile sig_atomic_t police_shutdown_requested = 0;
void police_signal_handler(int sig)
{
    police_shutdown_requested = 1;
}

void police_process_main(SimConfig *config, int msg_queue_id, int shared_mem_id)
{
    signal(SIGTERM, police_signal_handler);
    signal(SIGINT, police_signal_handler);
    SharedState *shared_state = (SharedState *)attach_shared_memory(shared_mem_id);
    if (!shared_state)
    {
        log_message("Police: Failed to attach to shared memory");
        exit(EXIT_FAILURE);
    }

    SecretAgent agents[MAX_AGENTS];
    GangIntelligence intel[MAX_GANGS];
    int agent_count = 0;
    IpcMessage message;
    bool simulation_running = true;

    memset(agents, 0, sizeof(agents));
    init_intelligence(intel, shared_state->gang_count);

    log_message("Police: Process started");

    // Attempt to infiltrate gangs with secret agents
    int infiltrated = infiltrate_gangs(shared_state, agents, intel, config, &agent_count);
    log_message("Police: Infiltrated %d agents into gangs", infiltrated);

    // Main police loop
    while (simulation_running && !police_shutdown_requested)
    {
        if (police_shutdown_requested)
        {
            log_message("Police: Received direct termination signal");
            simulation_running = false;
            break;
        }
        // Check for messages from agents
        if (!police_shutdown_requested && 
            receive_message(msg_queue_id, &message, MSG_TYPE_AGENT_REPORT, true) == 0) {
            
            // Only process if we're not shutting down
            if (!police_shutdown_requested) {
                if (process_agent_report(&message.data.agent_report, agents, intel, 
                                        config, agent_count)) {
                    // Take action against the gang if confidence is high enough
                    take_police_action(message.data.agent_report.gang_id, 
                                     msg_queue_id, shared_state, config);
                }
            }
        }

        // Check for simulation status updates
        if (receive_message(msg_queue_id, &message, MSG_TYPE_SIMULATION_STATUS, true) == 0)
        {
            if (message.data.status != SIM_STATUS_RUNNING)
            {
                log_message("Police: Received shutdown signal, status=%d", message.data.status);
                simulation_running = false;
            }
        }

        // Add a safety check for external termination
        if (shared_state->status != SIM_STATUS_RUNNING)
        {
            log_message("Police: Detected simulation status change to %d", shared_state->status);
            simulation_running = false;
        }

        // Periodically review intelligence
        review_intelligence(intel, agents, shared_state, shared_state->gang_count, agent_count);

        // Check if ending conditions are met
        if (check_end_conditions(shared_state, config))
        {
            simulation_running = false;
        }

        // Sleep a bit to avoid using 100% CPU
        usleep(100000); // 100ms
    }

    log_message("Police: Process shutting down");
    police_cleanup(intel);
    detach_shared_memory(shared_state);
}

void init_intelligence(GangIntelligence *intel, int gang_count)
{
    for (int i = 0; i < gang_count; i++)
    {
        intel[i].gang_id = i;
        intel[i].under_surveillance = false;
        intel[i].suspected_target = rand() % TARGET_COUNT;
        intel[i].suspicion_level = 0.0;
        intel[i].estimated_execution_time = 0;
        intel[i].confirmed_reports = 0;
        intel[i].agent_count = 0;
        memset(intel[i].agent_ids, -1, sizeof(intel[i].agent_ids));
    }
}

int infiltrate_gangs(SharedState *shared_state, SecretAgent *agents, GangIntelligence *intel,
                     SimConfig *config, int *agent_count)
{
    int infiltrated = 0;
    int current_agent_id = 0;

    pthread_mutex_lock(&shared_state->status_mutex);

    // Try to infiltrate each gang based on success rate
    for (int gang_id = 0; gang_id < shared_state->gang_count; gang_id++)
    {
        Gang *gang = &shared_state->gangs[gang_id];
        int gang_agent_count = 0;

        // Attempt to place agents based on infiltration rate
        for (int member_id = 0; member_id < gang->member_count; member_id++)
        {
            if (rand_float() < config->agent_infiltration_rate)
            {
                // Make sure we haven't hit the global agent limit
                if (*agent_count >= MAX_AGENTS)
                {
                    break;
                }
                
                // Make sure we haven't hit the per-gang agent limit
                if (gang_agent_count >= config->max_agents_per_gang)
                {
                    log_message("Police: Gang %d already has maximum number of agents (%d)", 
                               gang_id, config->max_agents_per_gang);
                    break;
                }

                // Check if this member is already infiltrated
                if (gang->members[member_id].is_agent)
                {
                    continue;
                }

                // Infiltrate this member
                gang->members[member_id].is_agent = true;
                gang->members[member_id].agent_id = current_agent_id;

                // Add to agent tracking
                agents[current_agent_id].id = current_agent_id;
                agents[current_agent_id].gang_id = gang_id;
                agents[current_agent_id].member_id = member_id;
                agents[current_agent_id].status = AGENT_STATUS_ACTIVE;
                agents[current_agent_id].last_report_time = time(NULL);
                agents[current_agent_id].last_reported_target = TARGET_COUNT; // Invalid target means no report yet
                agents[current_agent_id].confidence_level = 0.0;

                // Update intel
                intel[gang_id].agent_ids[intel[gang_id].agent_count] = current_agent_id;
                intel[gang_id].agent_count++;

                // Update global agent status
                shared_state->agent_statuses[current_agent_id] = AGENT_STATUS_ACTIVE;

                current_agent_id++;
                (*agent_count)++;
                infiltrated++;
                gang_agent_count++;
            }
        }
    }

    // Update the global agent count
    shared_state->agent_count = *agent_count;
    if (*agent_count > 0 && *agent_count < config->agent_execution_loss_count) {
        // Update local config copy
        config->agent_execution_loss_count = *agent_count;
        
        // Update shared state's config copy
        shared_state->agent_execution_loss_count = *agent_count;
        
        log_message("Police: Adjusting agent_execution_loss_count to %d to match actual agent count", 
                  *agent_count);
    }

    pthread_mutex_unlock(&shared_state->status_mutex);

    return infiltrated;
}

bool process_agent_report(AgentReport *report, SecretAgent *agents, GangIntelligence *intel,
                          SimConfig *config, int agent_count)
{
    int agent_id = report->agent_id;
    int gang_id = report->gang_id;
    bool should_act = false;

    // Validate agent ID
    if (agent_id < 0 || agent_id >= agent_count)
    {
        // During normal operation, this is an error
        // During shutdown, we'll just silently ignore
        if (!police_shutdown_requested)
        {
            log_message("Police: Invalid agent ID in report: %d", agent_id);
        }
        return false;
    }

    // Update agent data
    agents[agent_id].last_report_time = time(NULL);
    agents[agent_id].last_reported_target = report->suspected_target;
    agents[agent_id].confidence_level = report->confidence_level;

    // Update intelligence on this gang
    GangIntelligence *gang_intel = &intel[gang_id];

    // If confidence is high enough, update our suspicion
    if (report->confidence_level > gang_intel->suspicion_level)
    {
        gang_intel->suspected_target = report->suspected_target;
        gang_intel->suspicion_level = report->confidence_level;
        gang_intel->estimated_execution_time = report->estimated_execution_time;
    }

    // Count this as a confirmed report
    gang_intel->confirmed_reports++;

    // Place gang under surveillance if not already
    if (!gang_intel->under_surveillance && gang_intel->suspicion_level > 0.3)
    {
        gang_intel->under_surveillance = true;
        log_message("Police: Gang %d now under surveillance, suspected target: %s",
                    gang_id, get_target_name(gang_intel->suspected_target));
    }

    // Store when we first received a report for this gang
    static time_t first_report_times[MAX_GANGS];
    if (gang_intel->confirmed_reports == 1)
    {
        first_report_times[gang_id] = time(NULL);
    }

    // Decide if we should act based on suspicion level, confirmation threshold, and delay
    time_t min_investigation_time = 5; // At least 5 seconds before acting

    if (gang_intel->suspicion_level > config->police_confirmation_threshold &&
        gang_intel->confirmed_reports >= gang_intel->agent_count &&
        time(NULL) - first_report_times[gang_id] >= min_investigation_time)
    {
        should_act = true;
        log_message("Police: Sufficient evidence to act against gang %d", gang_id);
        // Reset confirmed reports counter
        gang_intel->confirmed_reports = 0;
    }

    return should_act;
}

void take_police_action(int gang_id, int msg_queue_id, SharedState *shared_state, SimConfig *config)
{
    log_message("Police: Taking action against gang %d", gang_id);

    // Send arrest order to the gang
    send_police_order(msg_queue_id, gang_id, config->prison_time);

    // Update statistics
    pthread_mutex_lock(&shared_state->status_mutex);
    shared_state->total_thwarted_plans++;
    pthread_mutex_unlock(&shared_state->status_mutex);
}

void handle_agent_discovery(int agent_id, SecretAgent *agents, GangIntelligence *intel,
                            SharedState *shared_state, int agent_count)
{
    if (agent_id < 0 || agent_id >= agent_count)
    {
        return;
    }

    int gang_id = agents[agent_id].gang_id;

    log_message("Police: Agent %d in gang %d has been discovered", agent_id, gang_id);

    // Update agent status
    agents[agent_id].status = AGENT_STATUS_UNCOVERED;

    // Update shared state
    pthread_mutex_lock(&shared_state->status_mutex);
    shared_state->agent_statuses[agent_id] = AGENT_STATUS_UNCOVERED;
    shared_state->total_executed_agents++;
    pthread_mutex_unlock(&shared_state->status_mutex);

    // Remove from intelligence array
    GangIntelligence *gang_intel = &intel[gang_id];
    for (int i = 0; i < gang_intel->agent_count; i++)
    {
        if (gang_intel->agent_ids[i] == agent_id)
        {
            // Shift remaining agents
            for (int j = i; j < gang_intel->agent_count - 1; j++)
            {
                gang_intel->agent_ids[j] = gang_intel->agent_ids[j + 1];
            }
            gang_intel->agent_count--;
            break;
        }
    }
}

bool check_end_conditions(SharedState *shared_state, SimConfig *config)
{
    pthread_mutex_lock(&shared_state->status_mutex);

    bool should_end = false;
    SimulationStatus status = shared_state->status;
    
    // Get the adjusted agent_execution_loss_count from shared memory
    int adjusted_agent_loss_count = shared_state->agent_execution_loss_count > 0 ? 
                                   shared_state->agent_execution_loss_count : 
                                   config->agent_execution_loss_count;

    // Police win condition
    if (shared_state->total_thwarted_plans >= config->police_thwart_win_count &&
        status == SIM_STATUS_RUNNING)
    {
        shared_state->status = SIM_STATUS_POLICE_WIN;
        should_end = true;
        log_message("Police: Win condition met - %d plans thwarted", shared_state->total_thwarted_plans);
    }

    // Gangs win condition
    if (shared_state->total_successful_plans >= config->gang_success_win_count &&
        status == SIM_STATUS_RUNNING)
    {
        shared_state->status = SIM_STATUS_GANGS_WIN;
        should_end = true;
        log_message("Police: Loss condition met - gangs successful %d times",
                    shared_state->total_successful_plans);
    }

    // Agents lost condition - USE THE ADJUSTED VALUE
    if (shared_state->total_executed_agents >= adjusted_agent_loss_count &&
        status == SIM_STATUS_RUNNING)
    {
        shared_state->status = SIM_STATUS_AGENTS_LOST;
        should_end = true;
        log_message("Police: Loss condition met - %d agents executed out of %d limit",
                    shared_state->total_executed_agents, adjusted_agent_loss_count);
    }

    // If status changed, send update
    if (should_end)
    {
        pthread_mutex_unlock(&shared_state->status_mutex);
        return true;
    }

    pthread_mutex_unlock(&shared_state->status_mutex);
    return false;
}

float analyze_gang_patterns(GangIntelligence *intel, SharedState *shared_state, int gang_id)
{
    // Simple analysis - just add some randomness to current suspicion level
    float suspicion = intel[gang_id].suspicion_level;
    float variance = rand_float() * 0.2 - 0.1; // -0.1 to 0.1

    suspicion += variance;
    if (suspicion < 0.0)
        suspicion = 0.0;
    if (suspicion > 1.0)
        suspicion = 1.0;

    return suspicion;
}

void review_intelligence(GangIntelligence *intel, SecretAgent *agents,
                         SharedState *shared_state, int gang_count, int agent_count)
{
    static time_t last_review = 0;
    time_t now = time(NULL);

    // Only review every 5 seconds
    if (now - last_review < 5)
    {
        return;
    }

    last_review = now;

    for (int gang_id = 0; gang_id < gang_count; gang_id++)
    {
        if (intel[gang_id].under_surveillance)
        {
            // Analyze patterns to adjust suspicion
            intel[gang_id].suspicion_level = analyze_gang_patterns(intel, shared_state, gang_id);

            // Check for overdue targets
            if (intel[gang_id].estimated_execution_time > 0 &&
                now > intel[gang_id].estimated_execution_time + 300)
            { // 5 minutes grace period
                // Target time has passed, reduce suspicion
                intel[gang_id].suspicion_level *= 0.5;
                intel[gang_id].estimated_execution_time = 0;

                if (intel[gang_id].suspicion_level < 0.3)
                {
                    intel[gang_id].under_surveillance = false;
                    log_message("Police: Surveillance on gang %d terminated - target time passed", gang_id);
                }
            }
        }
    }
}

void police_cleanup(GangIntelligence *intel)
{
    // Nothing to clean up in this implementation
    (void)intel; // Avoid unused parameter warning
}