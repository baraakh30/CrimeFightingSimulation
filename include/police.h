#ifndef POLICE_H
#define POLICE_H

#include "common.h"

/* Structure for secret agent management */
typedef struct {
    int id;
    int gang_id;
    int member_id;
    AgentStatus status;
    time_t last_report_time;
    CrimeTarget last_reported_target;
    float confidence_level;
} SecretAgent;

/* Structure to track police intelligence on gangs */
typedef struct {
    int gang_id;
    bool under_surveillance;
    CrimeTarget suspected_target;
    float suspicion_level;
    time_t estimated_execution_time;
    int confirmed_reports;
    int agent_count;
    int agent_ids[MAX_MEMBERS]; /* Agent IDs operating in this gang */
} GangIntelligence;


void police_process_main(SimConfig *config, int msg_queue_id, int shared_mem_id);
void init_intelligence(GangIntelligence *intel, int gang_count);
int infiltrate_gangs(SharedState *shared_state, SecretAgent *agents, GangIntelligence *intel, SimConfig *config, int *agent_count);
bool process_agent_report(AgentReport *report, SecretAgent *agents, GangIntelligence *intel, SimConfig *config, int agent_count);
void take_police_action(int gang_id, int msg_queue_id, SharedState *shared_state, SimConfig *config);
void handle_agent_discovery(int agent_id, SecretAgent *agents, GangIntelligence *intel, SharedState *shared_state, int agent_count);
bool check_end_conditions(SharedState *shared_state, SimConfig *config);
float analyze_gang_patterns(GangIntelligence *intel, SharedState *shared_state, int gang_id);
void review_intelligence(GangIntelligence *intel, SecretAgent *agents, SharedState *shared_state, int gang_count, int agent_count);
void police_cleanup(GangIntelligence *intel);

#endif /* POLICE_H */