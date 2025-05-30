#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

int load_config(const char *filename, SimConfig *config);
int parse_config_line(const char *line, char *key, char *value, size_t key_size, size_t value_size);
void set_default_config(SimConfig *config);
void print_config(SimConfig *config);

#endif /* CONFIG_H */