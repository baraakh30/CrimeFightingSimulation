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

// Multi-mission management functions
int get_available_members_count(Gang *gang);
int create_new_mission(Gang *gang, SimConfig *config);
void assign_members_to_mission(Gang *gang, Mission *mission, SimConfig *config);
void check_and_execute_ready_missions(Gang *gang, SimConfig *config, int msg_queue_id);
bool execute_mission(Gang *gang, Mission *mission, SimConfig *config, int msg_queue_id);
void complete_mission(Gang *gang, Mission *mission, bool success, SimConfig *config, int msg_queue_id);
void investigate_mission_for_agents(Gang *gang, Mission *mission, SimConfig *config, int msg_queue_id);

// Member and gang management functions
void* gang_member_thread(void *arg);
void process_arrest(Gang *gang, int duration);
void recruit_new_members(Gang *gang, SimConfig *config);
void promote_members(Gang *gang, SimConfig *config);
void execute_agent(Gang *gang, int member_index, int msg_queue_id, SharedState *shared_state);
void member_interaction(Gang *gang, int member_index, int target_index, SimConfig *config);
void update_preparation_levels(Gang *gang, SimConfig *config);
void gang_cleanup(Gang *gang);

#endif /* GANG_H */