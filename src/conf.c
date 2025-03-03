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

    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';

    return str;
}

void conf_add_key_value(Config *config, const char *key, const char *value) {
    size_t old_size = config->count * sizeof(char *);
    size_t new_size = (config->count + 1) * sizeof(char *);
    config->keys = mem_realloc(config->keys, new_size, old_size);
    config->values = mem_realloc(config->values, new_size, old_size);
    if (!config->keys || !config->values) {
        perror("Out of memory");
        exit(EXIT_FAILURE);
    }

    config->keys[config->count] = mem_malloc(strlen(key) + 1);
    config->values[config->count] = mem_malloc(strlen(value) + 1);
    if (!config->keys[config->count] || !config->values[config->count]) {
        perror("Out of memory");
        exit(EXIT_FAILURE);
    }

    strcpy(config->keys[config->count], key);
    strcpy(config->values[config->count], value);
    config->count ++;
}

void conf_add_key_str(Config *config, const char *key, const char *value) {
    if (strlen(value) + 3 >= MAX_LINE_LENGTH) {
        fprintf(stderr, "Value too long\n");
        exit(EXIT_FAILURE);
    }

    char value_str[MAX_LINE_LENGTH];
    sprintf(value_str, "'%s'", value);
    conf_add_key_value(config, key, value_str);
}

void conf_read_file(Config *config, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Can't open file %s\n", filename);
        exit(EXIT_FAILURE);
    }

    char line_read[MAX_LINE_LENGTH];
    char *current_line = NULL;
    size_t current_line_size = 0;

    while (fgets(line_read, sizeof(line_read), file) != NULL) {
        if (strlen(line_read) == sizeof(line_read) - 1) {
            fprintf(stderr, "Error: Line too long\n");
            exit(EXIT_FAILURE);
        }
        char *line = trim_whitespace(line_read);
        if (*line == '\0' || *line == '#') {
            continue;
        }

        size_t line_len = strlen(line);
        char *new_line = mem_realloc(current_line, current_line_size + line_len + 1, current_line_size);
        if (!new_line) {
            perror("Out of memory");
            exit(EXIT_FAILURE);
        }
        current_line = new_line;
        strcat(current_line, line);
        current_line_size += line_len + 1;

        size_t current_len = strlen(current_line);
        if (current_len > 1 && current_line[current_len - 1] == '\\') {
            current_line[current_len - 1] = ' ';
            continue;
        }

        char *delimiter = strchr(current_line, '=');
        if (delimiter) {
            *delimiter = '\0';
            char *key = mem_malloc(strlen(current_line) + 1);
            char *value = mem_malloc(strlen(delimiter + 1) + 1);
            if (!key || !value) {
                perror("Out of memory");
                exit(EXIT_FAILURE);
            }

            strcpy(key, current_line);
            strcpy(value, delimiter + 1);
            char *key_trim = trim_whitespace(key);
            char *value_trim = trim_whitespace(value);

            conf_add_key_value(config, key_trim, value_trim);

            mem_free(key);
            mem_free(value);
        }

        mem_free(current_line);
        current_line = NULL;
        current_line_size = 0;
    }

    fclose(file);
}

void conf_cleaning(Config *config) {
    if (!config) {
        return;
    }

    for (int i = 0; i < config->count; i++) {
        mem_free(config->keys[i]);
        mem_free(config->values[i]);
    }
    mem_free(config->keys);
    mem_free(config->values);
}

const char *conf_get(const Config *config, const char *key, const char* default_value) {
    if (!config) {
        return default_value;
    }

    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->keys[i], key) == 0) {
            return config->values[i];
        }
    }

    return default_value;
}

void conf_print(const Config *config) {
    if (!config) {
        return;
    }

    for (int i = 0; i < config->count; i++) {
        printf("%s='%s'\n", config->keys[i], config->values[i]);
    }
}
