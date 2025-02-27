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

/**
 * This program reads a CSV file and filters out rows that match a 
 * given expression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "../hdr/dmalloc.h"
#include "../hdr/conf.h"
#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define COLOR_RESET     "\033[0m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_CYAN      "\033[36m"

#define MAX_LINE_ITEMS  1024

const char *tokens[MAX_LINE_ITEMS];

const char *input_csv_delimiter = ",";

int variables_count = 0;
int variables_base = 0;
Variable variables[MAX_LINE_ITEMS];

int is_valid_double(const char *str) {
    char *endptr;
    strtod(str, &endptr);
    return *endptr == '\0' && endptr != str;
}

int is_valid_iso_datetime(const char *str) {
    struct tm datetime;
    return strptime(str, "%Y-%m-%dT%H:%M:%S", &datetime) != NULL;
}

void tokenize_line(const char *line, const char *delimiter) {
    if (!line) {
        tokens[0] = NULL;
        return;
    }

    int count = 0;
    const char *start = line;
    char *end;

    while ((end = strstr(start, delimiter)) != NULL) {
        *end = '\0';
        tokens[count++] = start;
        start = end + strlen(delimiter);
    }
    tokens[count++] = start;

    for (int i = 0; i < count; i++) {
        const char *token = tokens[i];
        while (isspace((unsigned char)*token)) token++;
        tokens[i] = token;

        char *end = (char *)token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
    }
    tokens[count] = NULL;
}

void var_print(Variable *var) {
    printf("%s=", var->name);
    switch (var->type) {
        case VAR_NUMBER:
            printf("%f\n", var->value);
            break;

        case VAR_STRING:
            printf("'%s'\n", var->str);
            break;

        case VAR_DATETIME:
            {
                char buffer[20];
                strftime(buffer, sizeof(buffer), DATE_FORMAT, &var->datetime);
                printf("%s\n", buffer);
            }
            break;
        default:
            printf("Unknown type: %d\n", var->type);
            break;
    }
}

void var_print_all() {
    for (int i = 0; i < variables_count; i++) {
        Variable *var = &variables[i];
        var_print(var);
    }
}

const char *var_get_str(const char *name, const char *default_value) {
    for (int i = 0; i < variables_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].str;
        }
    }
    return default_value;
}

double var_get_number(const char *name) {
    for (int i = 0; i < variables_count; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return 0;
}

void var_push(Variable *var) {
    if (variables_count >= MAX_LINE_ITEMS) {
        fprintf(stderr, "Too many variables\n");
        exit(EXIT_FAILURE);
    }

    Variable *new_var = &variables[variables_count ++];

    new_var->type = var->type;
    new_var->name = var->name;
    new_var->is_dynamic = false;

    switch (var->type) {
        case VAR_NUMBER:
            new_var->value = var->value;
            break;

        case VAR_STRING:
            new_var->is_dynamic = true;
            new_var->str = (char *) mem_malloc(strlen(var->str) + 1);
            if (new_var->str == NULL) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
            }

            strcpy((char *)new_var->str, (char *)var->str);
            break;

        case VAR_DATETIME:
            new_var->datetime = var->datetime; // FIXME: Copy??
            break;

        default:
            fprintf(stderr, "Unknown variable type %d\n", var->type);
            exit(EXIT_FAILURE);
    }
}

void var_cleaning(bool all) {
    for (int i = all ? 0 :variables_base; i < variables_count; i++) {
        Variable *var = &variables[i];
        if (var->is_dynamic) {
            mem_free((void *) var->str);
            var->str = NULL;
        }
    }
}

void assign_variables_config(Config *config) {
    variables_count = 0;
    for (int i = 0; i < config->count; i++) {
        variables[variables_count].type = VAR_END;

        const Instruction *code = parse_expression(config->values[i], variables);
        Variable *var = execute_code_datatype(code, variables);

        var->name = config->keys[i];
        var_push(var);

        parse_cleaning(code);
    }

    variables[variables_count].type = VAR_END;
    variables_base = variables_count;
}

void assign_variables_name() {
    variables_count = variables_base;
    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[variables_count ++];
        var->name = tokens[index];
        var->type = VAR_UNKNOWN;
        var->is_dynamic = false;
    }
    variables[variables_count].type = VAR_END;
}

int assign_variables_type() {
    int type_changed = 0;

    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[variables_base + index];
        if (is_valid_double(tokens[index])) {
            if (var->type != VAR_NUMBER) type_changed = 1;
            var->type = VAR_NUMBER;
        } 
        else if (is_valid_iso_datetime(tokens[index])) {
            if (var->type != VAR_DATETIME) type_changed = 1;
            var->type = VAR_DATETIME;
        } 
        else {
            if (var->type != VAR_STRING) type_changed = 1;
            var->type = VAR_STRING;
        }
    }

    return type_changed;
}

void assign_variables_value() {
    var_cleaning(false);
    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[variables_base + index];
        switch (var->type) {
            case VAR_NUMBER:
                var->value = atof(tokens[index]);
                break;

            case VAR_STRING:
                var->str = tokens[index];
                var->is_dynamic = false;
                break;

            case VAR_DATETIME:
                strptime(tokens[index], DATE_FORMAT, &var->datetime);
                break;

            case VAR_UNKNOWN:
            case VAR_END:
                break;
        }
    }
}

void  update_progress_bar(long processed_size, long file_size, long *last_progress) {
    int progress = (int)((processed_size * 100) / file_size);
    if (progress == *last_progress) {
        return;
    }
    *last_progress = progress;

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int terminal_width = w.ws_col;
    int bar_width = terminal_width * 0.5;

    char progress_bar[bar_width + 1];
    int bar_length = (progress * bar_width) / 100;
    memset(progress_bar, '=', bar_length);
    memset(progress_bar + bar_length, ' ', bar_width - bar_length);
    progress_bar[bar_width] = '\0';
    printf("\r" COLOR_GREEN "[%-*s] %d%%" COLOR_RESET, bar_width, progress_bar, progress);
    fflush(stdout);
}

void process_csv(const char *input_filename, const char *output_filename, const char *expr) {
    int total_lines = 0;
    int written_lines = 0;
    long last_progress = -1;
    long processed_size = 0;

    const Instruction *input_code = NULL; 

    int output_code_count = 0;
    const Instruction *output_code[MAX_LINE_ITEMS];

    const char *output_delimiter = NULL;

    char headder[MAX_LINE_ITEMS];
    char line[MAX_LINE_ITEMS];
    char copy_line[MAX_LINE_ITEMS];

    FILE *inputFile = fopen(input_filename, "rb");
    if (inputFile == NULL) {
        fprintf(stderr, "Error opening input file: '%s'\n", input_filename);
        exit(EXIT_FAILURE);
    }

    FILE *outputFile = fopen(output_filename, "wb");
    if (outputFile == NULL) {
        fprintf(stderr, "Error creating output file: '%s'\n", output_filename);
        exit(EXIT_FAILURE);
    }

    fseek(inputFile, 0, SEEK_END);
    long file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    printf(COLOR_CYAN "Processing %s\n" COLOR_RESET, input_filename);

    // Read the header line
    if (fgets(headder, sizeof(headder), inputFile) == NULL) {
        fprintf(stderr, "Can't read input file: '%s'\n", output_filename);
        exit(EXIT_FAILURE);
    }
    if (strlen(headder) == sizeof(headder) - 1) {
        fprintf(stderr, "Error: Line too long\n");
        exit(EXIT_FAILURE);
    }

    const char *output_headder = var_get_str("output_headder", NULL);
    if (output_headder) {
        fwrite(output_headder, sizeof(char), strlen(output_headder), outputFile);
        fwrite("\n", sizeof(char), strlen("\n"), outputFile);
    }
    else {
        fwrite(headder, sizeof(char), strlen(headder), outputFile);
    }
    processed_size += strlen(headder);

    tokenize_line(headder, input_csv_delimiter);
    assign_variables_name();

    while (fgets(line, sizeof(line), inputFile) != NULL) {
        if (strlen(line) == sizeof(line) - 1) {
            fprintf(stderr, "Error: Line too long\n");
            exit(EXIT_FAILURE);
        }

        processed_size += strlen(line);
        total_lines ++;

        strcpy(copy_line, line);
        tokenize_line(line, input_csv_delimiter);

        if (total_lines == 1) {
            assign_variables_type();
            assign_variables_value();

            input_code = parse_expression(expr, variables);

            const char *output_fields = var_get_str("output_fields", NULL);
            if (output_fields) {
                output_delimiter = var_get_str("output_csv_delimiter", input_csv_delimiter);

                tokenize_line(output_fields, output_delimiter);
                output_code_count = 0;
                for (int index = 0; tokens[index] != NULL; index++) {
                    output_code[output_code_count++] = parse_expression(tokens[index], variables);
                }
            }
        }
        else {
            assign_variables_value();
        }

        bool is_true = execute_code(input_code, variables) != 0;
        if (is_true) {
            if (output_code_count) {
                char output_line[MAX_LINE_ITEMS];
                char *output_line_ptr = output_line;
                output_line[0] = '\0';

                for (int index = 0; index < output_code_count; index++) {
                    Variable *res = execute_code_datatype(output_code[index], variables);

                    switch (res->type) {
                        case VAR_NUMBER:
                            sprintf(output_line_ptr, "%f", res->value);
                            break;

                        case VAR_STRING:
                            sprintf(output_line_ptr, "%s", res->str);
                            if (res->is_dynamic) mem_free((void *) res->str);
                            break;

                        case VAR_DATETIME:
                            {
                                char buffer[20];
                                strftime(buffer, sizeof(buffer), DATE_FORMAT, &res->datetime);
                                sprintf(output_line_ptr, "%s", buffer);
                            }
                            break;

                        default:
                            fprintf(stderr, "Unknown variable type %d\n", res->type);
                            exit(EXIT_FAILURE);
                    }

                    output_line_ptr += strlen(output_line_ptr);
                    if (index < output_code_count - 1) {
                        sprintf(output_line_ptr, "%s", output_delimiter);
                        output_line_ptr += strlen(output_line_ptr);
                    }
                }

                fwrite(output_line, sizeof(char), strlen(output_line), outputFile);
                fwrite("\n", sizeof(char), strlen("\n"), outputFile);
            }
            else {
                fwrite(copy_line, sizeof(char), strlen(copy_line), outputFile);
            }

            written_lines++;    
        }

        update_progress_bar(processed_size, file_size, &last_progress);
    }

    double pct_written = (double)written_lines * 100 / total_lines;
    printf(
        COLOR_YELLOW "\nWritten %s: %d of %d lines written (%.1f%%)\n" COLOR_RESET, 
        output_filename, written_lines, total_lines, pct_written
    );

    fclose(inputFile);
    fclose(outputFile);

    for (int index=0; index < output_code_count; index++) {
        parse_cleaning(output_code[index]);
    }
    parse_cleaning(input_code);
    var_cleaning(false);
}

int main(int argc, char *argv[]) {
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    const char *expr = NULL;
    Config config = {NULL, NULL, 0};

    if (argc == 2) {
        config = conf_read_file(argv[1]);

        assign_variables_config(&config);

        input_dir = var_get_str("source_dir", NULL);
        output_dir = var_get_str("dest_dir", NULL);
        expr = var_get_str("input_script", NULL);
        input_csv_delimiter = var_get_str("input_csv_delimiter", ",");
    }
    else if (argc == 4) {
        input_dir = argv[1];
        output_dir = argv[2];
        expr = argv[3];
    }

    if (!input_dir || !output_dir || !expr) {
        fprintf(stderr, "Usage: %s <conf file> | <input_directory> <output_directory> <expression>\n", argv[0]);
        return EXIT_FAILURE;
    }

    DIR *dir = opendir(input_dir);
    if (dir == NULL) {
        fprintf(stderr, "Error opening input directory: '%s'\n", input_dir);
        return EXIT_FAILURE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".csv") == 0) {
            char input_filename[PATH_MAX];
            snprintf(input_filename, sizeof(input_filename), "%s/%s", input_dir, entry->d_name);

            char output_filename[PATH_MAX];
            snprintf(output_filename, sizeof(output_filename), "%s/%s", output_dir, entry->d_name);

            process_csv(input_filename, output_filename, expr);
        }
    }
    closedir(dir);

    conf_cleaning(&config);
    var_cleaning(true);
    mem_cleaning();
    
    return EXIT_SUCCESS;
}
