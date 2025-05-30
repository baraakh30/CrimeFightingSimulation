#ifndef GANG_H
#define GANG_H

#include "common.h"

/* Structure for passing data to gang member threads */
typedef struct {
    int member_index;
    Gang *gang;
    SimConfig *config;
    int msg_queue_id;
    SharedState *shared_state;
} GangMemberArgs;


int gang_init(Gang *gang, int id, int member_count, SimConfig *config);
void gang_process_main(int gang_id, SimConfig *config, int msg_queue_id, int shared_mem_id);
void select_new_target(Gang *gang, SimConfig *config);
void* gang_member_thread(void *arg);
void spread_information(Gang *gang, SimConfig *config, bool is_false);
bool execute_plan(Gang *gang, SimConfig *config, int msg_queue_id);
void process_arrest(Gang *gang, int duration);
void investigate_for_agents(Gang *gang, SimConfig *config, int msg_queue_id);
void recruit_new_members(Gang *gang, SimConfig *config);
void promote_members(Gang *gang, SimConfig *config);
void execute_agent(Gang *gang, int member_index, int msg_queue_id, SharedState *shared_state);
void member_interaction(Gang *gang, int member_index, int target_index, SimConfig *config);
void update_preparation_levels(Gang *gang, SimConfig *config);
void gang_cleanup(Gang *gang);

#endif /* GANG_H */