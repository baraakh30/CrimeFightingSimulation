#ifndef IPC_H
#define IPC_H

#include "common.h"


int init_message_queue(void);
int create_shared_memory(size_t size);
void* attach_shared_memory(int shm_id);
int detach_shared_memory(void *ptr);
int remove_shared_memory(int shm_id);
int send_message(int msg_queue_id, IpcMessage *message);
int receive_message(int msg_queue_id, IpcMessage *message, long msg_type, bool no_wait);
int send_agent_report(int msg_queue_id, int agent_id, int gang_id, CrimeTarget target, float confidence, time_t time);

int send_police_order(int msg_queue_id, int gang_id, int duration);
int send_status_update(int msg_queue_id, SimulationStatus status);
int init_shared_mutex(pthread_mutex_t *mutex);
int cleanup_shared_mutex(pthread_mutex_t *mutex);
int init_shared_state(SharedState *state);
int update_gang_status(SharedState *state, Gang *gang);
int update_agent_status(SharedState *state, int agent_id, AgentStatus status);

#endif /* IPC_H */