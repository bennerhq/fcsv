/**
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 43):
 *
 * GitHub Co-pilot and <jens@bennerhq.com> wrote this file.  As long as you 
 * retain this notice you can do whatever you want with this stuff. If we meet 
 * some day, and you think this stuff is worth it, you can buy me a beer in 
 * return.   
 *
 * /benner
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../hdr/dmalloc.h"
#include "../hdr/conf.h"

#define MAX_LINE_LENGTH     1024

char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

Config conf_read_file(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    Config config = {NULL, NULL, 0};
    char line_read[MAX_LINE_LENGTH];
    char *current_line = NULL;
    size_t current_line_size = 0;

    while (fgets(line_read, sizeof(line_read), file) != NULL) {
        char *line = line_read;
        while (isspace(*line)) {
            line++;
        }
        if (*line == '\0' || *line == '#') {
            continue;
        }
        size_t len = strlen(line);

        int multi_lines = 0;
        if (len > 1 && line[len - 2] == '\\') {
            line[len - 2] = ' ';
            multi_lines = 1;
        }

        current_line_size += strlen(line);
        current_line = mem_realloc(current_line, current_line_size + 1, current_line_size);
        if (! current_line) {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }
        strcat(current_line, line);

        if (multi_lines) {
            continue;
        }

        char *delimiter = strchr(current_line, '=');
        if (delimiter) {
            *delimiter = '\0';
            char *key = mem_alloc(strlen(current_line) + 1);
            char *value = mem_alloc(strlen(delimiter + 1) + 1);
            if (!key || !value) {
                perror("Error allocating memory");
                exit(EXIT_FAILURE);
            }

            config.keys = mem_realloc(config.keys, (config.count + 1) * sizeof(char *), config.count * sizeof(char *));
            config.values = mem_realloc(config.values, (config.count + 1) * sizeof(char *), config.count * sizeof(char *));
            if (!config.keys || !config.values) {
                perror("Error allocating memory");
                exit(EXIT_FAILURE);
            }

            strcpy(key, current_line);
            strcpy(value, delimiter + 1);
            char *key_trim = trim_whitespace(key);
            char *value_trim = trim_whitespace(value);

            config.keys[config.count] = mem_alloc(strlen(key_trim) + 1);
            config.values[config.count] = mem_alloc(strlen(value_trim) + 1);
            if (!config.keys[config.count] || !config.values[config.count]) {
                perror("Error allocating memory");
                exit(EXIT_FAILURE);
            }

            strcpy(config.keys[config.count], key_trim);
            strcpy(config.values[config.count], value_trim);
            config.count++;

            mem_free(key);
            mem_free(value);
        }

        mem_free(current_line);
        current_line = NULL;
        current_line_size = 0;
    }

    fclose(file);
    return config;
}

void conf_free(Config *config) {
    if (!config) {
        return;
    }

    for (size_t i = 0; i < config->count; i++) {
        mem_free(config->keys[i]);
        mem_free(config->values[i]);
    }
    mem_free(config->keys);
    mem_free(config->values);
}

void conf_print(const Config *config) {
    for (size_t i = 0; i < config->count; i++) {
        printf("%s='%s'\n", config->keys[i], config->values[i]);
    }
}

char *conf_get(const Config *config, const char *key) {
    for (size_t i = 0; i < config->count; i++) {
        if (strcmp(config->keys[i], key) == 0) {
            printf("Found: '%s'\n", config->values[i]);
            return config->values[i];
        }
    }

    return NULL;
}
