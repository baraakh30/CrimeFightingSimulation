#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

/* Shared memory key definitions */
#define SHM_KEY_BASE 9000
#define MSG_KEY_BASE 8000

/* Maximum values for various elements */
#define MAX_GANGS 20
#define MAX_MEMBERS 50
#define MAX_RANKS 10
#define MAX_AGENTS 50

/* Message types for inter-process communication */
#define MSG_TYPE_GANG_REPORT 1
#define MSG_TYPE_POLICE_ORDER 2
#define MSG_TYPE_AGENT_REPORT 3
#define MSG_TYPE_SIMULATION_STATUS 4

typedef struct SharedState SharedState;

/* Crime targets enumeration */
typedef enum
{
    TARGET_BANK_ROBBERY,
    TARGET_JEWELRY_ROBBERY,
    TARGET_DRUG_TRAFFICKING,
    TARGET_ART_THEFT,
    TARGET_KIDNAPPING,
    TARGET_BLACKMAIL,
    TARGET_ARM_TRAFFICKING,
    TARGET_COUNT
} CrimeTarget;

/* Gang member status */
typedef enum
{
    MEMBER_STATUS_ACTIVE,
    MEMBER_STATUS_ARRESTED,
    MEMBER_STATUS_DEAD,
    MEMBER_STATUS_EXECUTED
} MemberStatus;

/* Agent status */
typedef enum
{
    AGENT_STATUS_ACTIVE,
    AGENT_STATUS_UNCOVERED,
    AGENT_STATUS_DEAD
} AgentStatus;

/* Simulation status */
typedef enum
{
    SIM_STATUS_RUNNING,
    SIM_STATUS_POLICE_WIN,
    SIM_STATUS_GANGS_WIN,
    SIM_STATUS_AGENTS_LOST,
    SIM_STATUS_SHUTDOWN
} SimulationStatus;

/* Structure to represent configuration parameters loaded from file */
typedef struct
{
    int num_gangs;
    int min_members_per_gang;
    int max_members_per_gang;
    int num_ranks;
    float agent_infiltration_rate;
    int preparation_time_min;
    int preparation_time_max;
    float false_info_probability;
    float mission_success_rate_base;
    float mission_kill_probability;
    float agent_suspicion_threshold;
    float police_confirmation_threshold;
    int prison_time;
    int police_thwart_win_count;
    int gang_success_win_count;
    int agent_execution_loss_count;
    int info_spread_delay;
    float member_knowledge_transfer_rate;
    float member_knowledge_rank_factor;
    float member_knowledge_lucky_chance;
    float agent_knowledge_gain;

    float base_preparation_increment;
    float rank_preparation_bonus;
    float min_preparation_required_base;
    float min_preparation_difficulty_factor;
    float agent_report_knowledge_reset;

    float promotion_base_chance;
    float promotion_rank_factor;
    float agent_base_suspicion;
    float knowledge_anomaly_suspicion;
    int min_agent_report_time;

    float target_difficulty_base;
    float target_difficulty_scaling;
    float agent_initial_knowledge_threshold;
    float agent_knowledge_report_threshold;
    float info_spread_base_value;
    float info_spread_rank_factor;
    float agent_discovery_threshold;
    float preparation_knowledge_factor;
    float preparation_rank_factor;
    int max_agents_per_gang;
} SimConfig;

/* Structure for a gang member */
typedef struct
{
    int id;
    int gang_id;
    int rank;
    bool is_agent;
    int agent_id; /* -1 if not an agent */
    MemberStatus status;
    float preparation_level;
    float knowledge_level; /* How much correct info they have about current plan */
    time_t release_time;   /* When arrested, this indicates release time */
    pthread_t thread_id;
} GangMember;

/* Structure for a gang */
typedef struct
{
    int id;
    int member_count;
    CrimeTarget current_target;
    int target_preparation_time;
    float required_preparation_level;
    bool plan_in_progress;
    bool plan_disrupted;
    int successful_missions;
    int failed_missions;
    pid_t process_id;
    sem_t *member_semaphore; /* For synchronizing member threads */
    GangMember members[MAX_MEMBERS];
    SharedState *shared_state;
} Gang;

/* Structure for a secret agent report */
typedef struct
{
    int agent_id;
    int gang_id;
    CrimeTarget suspected_target;
    float confidence_level;
    time_t estimated_execution_time;
} AgentReport;

/* Message structure for inter-process communication */
typedef struct
{
    long mtype;
    union
    {
        AgentReport agent_report;
        int gang_id;
        CrimeTarget target;
        SimulationStatus status;
        int arrest_duration;
    } data;
} IpcMessage;

/* Global shared memory structure for visualization and coordination */
struct SharedState
{
    SimulationStatus status;
    int gang_count;
    int total_thwarted_plans;
    int total_successful_plans;
    int total_executed_agents;
    Gang gangs[MAX_GANGS];
    int agent_count;
    AgentStatus agent_statuses[MAX_AGENTS];
    pthread_mutex_t status_mutex;
    int agent_execution_loss_count;
};

/* Function prototypes for utility functions */
void log_message(const char *format, ...);
const char *get_target_name(CrimeTarget target);
float rand_float(void);
int rand_range(int min, int max);

#endif /* COMMON_H */