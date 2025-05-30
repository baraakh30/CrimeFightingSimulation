#ifndef MAIN_H
#define MAIN_H

#include "common.h"
#include "config.h"
#include "gang.h"
#include "police.h"
#include "simulation.h"
#include "utils.h"


int parse_arguments(int argc, char *argv[], char *config_file, size_t config_file_size);
int initialize_environment(SimConfig *config, const char *config_file);
void register_signal_handlers(void);
void display_welcome(SimConfig *config);

#endif /* MAIN_H */