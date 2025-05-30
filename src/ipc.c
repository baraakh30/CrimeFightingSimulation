#include "../include/common.h"
#include "../include/ipc.h"
#include <errno.h>

int init_message_queue(void) {
    int msg_queue_id;
    key_t key = MSG_KEY_BASE;
    
    /* Create the message queue */
    msg_queue_id = msgget(key, IPC_CREAT | 0666);
    if (msg_queue_id == -1) {
        perror("msgget");
        return -1;
    }
    
    log_message("Message queue created with ID: %d", msg_queue_id);
    return msg_queue_id;
}

int create_shared_memory(size_t size) {
    int shm_id;
    key_t key = SHM_KEY_BASE;
    
    /* Create the shared memory segment */
    shm_id = shmget(key, size, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        return -1;
    }
    
    log_message("Shared memory created with ID: %d, size: %zu", shm_id, size);
    return shm_id;
}

void* attach_shared_memory(int shm_id) {
    void *ptr;
    
    /* Attach to the shared memory segment */
    ptr = shmat(shm_id, NULL, 0);
    if (ptr == (void *)-1) {
        perror("shmat");
        return NULL;
    }
    
    return ptr;
}

int detach_shared_memory(void *ptr) {
    /* Detach from the shared memory segment */
    if (shmdt(ptr) == -1) {
        perror("shmdt");
        return -1;
    }
    
    return 0;
}

int remove_shared_memory(int shm_id) {
    /* Remove the shared memory segment */
    if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        return -1;
    }
    
    log_message("Shared memory with ID %d removed", shm_id);
    return 0;
}

int send_message(int msg_queue_id, IpcMessage *message) {
    /* Send the message */
    if (msgsnd(msg_queue_id, message, sizeof(IpcMessage) - sizeof(long), 0) == -1) {
        perror("msgsnd");
        return -1;
    }
    
    return 0;
}

int receive_message(int msg_queue_id, IpcMessage *message, long msg_type, bool no_wait) {
    int flags = no_wait ? IPC_NOWAIT : 0;
    ssize_t result;
    
    /* Receive the message */
    result = msgrcv(msg_queue_id, message, sizeof(IpcMessage) - sizeof(long), 
                  msg_type, flags);
                  
    if (result == -1) {
        if (errno == ENOMSG && no_wait) {
            /* No message available, not an error when no_wait is true */
            return -1;
        }
        perror("msgrcv");
        return -1;
    }
    
    return 0;
}

int send_agent_report(int msg_queue_id, int agent_id, int gang_id, 
                     CrimeTarget target, float confidence, time_t time) {
    IpcMessage message;
    
    /* Fill out the message structure */
    message.mtype = MSG_TYPE_AGENT_REPORT;
    message.data.agent_report.agent_id = agent_id;
    message.data.agent_report.gang_id = gang_id;
    message.data.agent_report.suspected_target = target;
    message.data.agent_report.confidence_level = confidence;
    message.data.agent_report.estimated_execution_time = time;
    
    /* Send the message */
    return send_message(msg_queue_id, &message);
}

int send_police_order(int msg_queue_id, int gang_id, int duration) {
    IpcMessage message;
    
    /* Fill out the message structure */
    message.mtype = MSG_TYPE_POLICE_ORDER;
    message.data.gang_id = gang_id;
    message.data.arrest_duration = duration;
    
    /* Send the message */
    return send_message(msg_queue_id, &message);
}

int send_status_update(int msg_queue_id, SimulationStatus status) {
    IpcMessage message;
    
    /* Fill out the message structure */
    message.mtype = MSG_TYPE_SIMULATION_STATUS;
    message.data.status = status;
    
    /* Send the message */
    return send_message(msg_queue_id, &message);
}

int init_shared_mutex(pthread_mutex_t *mutex) {
    pthread_mutexattr_t attr;
    
    /* Initialize mutex attributes */
    if (pthread_mutexattr_init(&attr) != 0) {
        perror("pthread_mutexattr_init");
        return -1;
    }
    
    /* Set the mutex to be shared between processes */
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        perror("pthread_mutexattr_setpshared");
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
    
    /* Initialize the mutex */
    if (pthread_mutex_init(mutex, &attr) != 0) {
        perror("pthread_mutex_init");
        pthread_mutexattr_destroy(&attr);
        return -1;
    }
    
    /* Destroy the attributes */
    pthread_mutexattr_destroy(&attr);
    
    return 0;
}

int cleanup_shared_mutex(pthread_mutex_t *mutex) {
    /* Destroy the mutex */
    if (pthread_mutex_destroy(mutex) != 0) {
        perror("pthread_mutex_destroy");
        return -1;
    }
    
    return 0;
}

int init_shared_state(SharedState *state) {
    /* Initialize the shared state structure */
    memset(state, 0, sizeof(SharedState));
    
    /* Initialize the mutex */
    if (init_shared_mutex(&state->status_mutex) != 0) {
        return -1;
    }
    
    /* Set initial status */
    state->status = SIM_STATUS_RUNNING;
    state->agent_execution_loss_count = 0;
    return 0;
}

int update_gang_status(SharedState *state, Gang *gang) {
    /* Lock the mutex */
    if (pthread_mutex_lock(&state->status_mutex) != 0) {
        perror("pthread_mutex_lock");
        return -1;
    }
    
    /* Update the gang information */
    memcpy(&state->gangs[gang->id], gang, sizeof(Gang));
    
    /* Unlock the mutex */
    if (pthread_mutex_unlock(&state->status_mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }
    
    return 0;
}

int update_agent_status(SharedState *state, int agent_id, AgentStatus status) {
    /* Lock the mutex */
    if (pthread_mutex_lock(&state->status_mutex) != 0) {
        perror("pthread_mutex_lock");
        return -1;
    }
    
    /* Update the agent status */
    if (agent_id >= 0 && agent_id < MAX_AGENTS) {
        state->agent_statuses[agent_id] = status;
    }
    
    /* Unlock the mutex */
    if (pthread_mutex_unlock(&state->status_mutex) != 0) {
        perror("pthread_mutex_unlock");
        return -1;
    }
    
    return 0;
}

