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

#define MAX_VARIABLES   (1024)
#define MAX_LINE_LENGTH (1024 * 10)

int variables_base = 0;
Variable variables[MAX_VARIABLES];

const char *tokens[MAX_VARIABLES];

const char *input_csv_delimiter = ",";

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
        if (count + 2 >= MAX_VARIABLES) {
            fprintf(stderr, "Too many tokens\n");
            exit(EXIT_FAILURE);
        }

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

void var_print(const Variable *var) {
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
            printf("Unknown variable type: %d !?!\n", var->type);
            break;
    }
}

void var_print_all() {
    printf("---- Variables ---------\n");
    for (int i = 0; variables[i].type != VAR_END; i++) {
        const Variable *var = &variables[i];
        printf("%2d  ", i);
        var_print(var);
    }
    printf("------------------------\n\n");
}

void var_cleaning(bool all) {
    if (all) {
        for (int idx = 0; idx < variables_base; idx++) {
            Variable *var = &variables[idx];
            if (var->type == VAR_STRING && var->is_dynamic) {
                mem_free((void *) var->str);
                var->type = VAR_UNKNOWN;
                var->str = NULL;
                var->is_dynamic = false;
            }
        }
    }

    for (Variable *var = &variables[variables_base]; var->type != VAR_END; var++) {
        if (var->type == VAR_STRING && var->is_dynamic) {
            mem_free((void *) var->str);
            var->type = VAR_UNKNOWN;
            var->str = NULL;
            var->is_dynamic = false;
        }
    }

}

const char *var_get_str(const char *name, const char *default_value) {
    for (int i = 0; variables[i].type != VAR_END; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].str;
        }
    }
    return default_value;
}

void assign_variables_config(Config *config) {
    int idx = 0;
    while (idx < config->count) {
        if (idx + 2 >= MAX_VARIABLES) {
            fprintf(stderr, "Too many variables\n");
            exit(EXIT_FAILURE);
        }

        const char *name = config->keys[idx];
        const char *expr = config->values[idx];

        Variable *var = &variables[idx];
        var->type = VAR_END;

        const Variable *code = parse_expression(expr, variables);
        Variable exec_var = execute_code_datatype(code, variables);

        var->name = name;
        var->type = exec_var.type;
        var->is_dynamic = false;

        switch (exec_var.type) {
            case VAR_NUMBER:
                var->value = exec_var.value;
                break;

            case VAR_STRING:
                var->str = (char *) mem_malloc(strlen(exec_var.str) + 1);
                var->is_dynamic = true;
                if (var->str == NULL) {
                    fprintf(stderr, "Out of memory\n");
                    exit(EXIT_FAILURE);
                }
                strcpy((char *)var->str, (char *)exec_var.str);
                if (exec_var.is_dynamic) {
                    mem_free((void *) exec_var.str);
                    exec_var.str = NULL;
                    exec_var.is_dynamic = false;
                }
                break;

            case VAR_DATETIME:
                var->datetime = exec_var.datetime; // FIXME: Copy??
                break;

            default:
                fprintf(stderr, "Unknown variable type %d\n", exec_var.type);
                exit(EXIT_FAILURE);
        }

        parse_cleaning(code);

        idx ++;
    }

    variables_base = idx;
    variables[variables_base].type = VAR_END;
}

void assign_variables_name() {
    int idx = 0;
    while (tokens[idx] != NULL) {
        if (variables_base + idx + 2 >= MAX_VARIABLES) {
            fprintf(stderr, "Too many variables\n");
            exit(EXIT_FAILURE);
        }

        Variable *var = &variables[variables_base + idx];
        var->name = tokens[idx];
        var->type = VAR_UNKNOWN;
        var->is_dynamic = false;

        idx ++;
    }
    variables[variables_base + idx].type = VAR_END;
}

void assign_variables_type() {
    for (int idx = 0; tokens[idx] != NULL; idx++) {
        Variable *var = &variables[variables_base + idx];
        var->is_dynamic = false;

        if (is_valid_double(tokens[idx])) {
            var->type = VAR_NUMBER;
        }
        else if (is_valid_iso_datetime(tokens[idx])) {
            var->type = VAR_DATETIME;
        }
        else {
            var->type = VAR_STRING;
        }
    }
}

void assign_variables_value() {
    var_cleaning(false);
    for (int idx = 0; tokens[idx] != NULL; idx++) {
        Variable *var = &variables[variables_base + idx];
        var->is_dynamic = false;
        switch (var->type) {
            case VAR_NUMBER:
                var->value = atof(tokens[idx]);
                break;

            case VAR_STRING:
                var->str = tokens[idx];
                break;

            case VAR_DATETIME:
                strptime(tokens[idx], DATE_FORMAT, &var->datetime);
                break;

            default:
                fprintf(stderr, "Unknown variable type %d!?\n", var->type);
                exit(EXIT_FAILURE);
                break;
        }
    }
}

void  update_progress_bar(long processed_size, long file_size, long *last_progress) {
    int progress = (int)((processed_size * 100) / file_size);
    if (progress > 100) progress = 100;

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

    const Variable *input_code = NULL; 
    int output_code_count = 0;
    const Variable *output_code[MAX_VARIABLES];
    const char *output_delimiter = NULL;
    char line[MAX_LINE_LENGTH];

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

    printf(COLOR_CYAN "Processing %s\n" COLOR_RESET, input_filename);

    fseek(inputFile, 0, SEEK_END);
    long file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    char *output_fields_copy = NULL;
    const char *output_fields = var_get_str("output_fields", NULL);
    if (output_fields) {
        output_fields_copy = (void *) mem_malloc(strlen(output_fields) + 1);
        if (output_fields_copy == NULL) {
            fprintf(stderr, "Out of memory\n");
            exit(EXIT_FAILURE);
        }

        strcpy(output_fields_copy, output_fields);      
    }

    while (fgets(line, sizeof(line), inputFile) != NULL) {
        if (strlen(line) == sizeof(line) - 1) {
            fprintf(stderr, "Error: Line too long\n");
            exit(EXIT_FAILURE);
        }

        total_lines ++;
        processed_size += strlen(line);
        update_progress_bar(processed_size, file_size, &last_progress);

        if (total_lines == 1) {
            char headder[MAX_LINE_LENGTH];
            strcpy(headder, line);

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
            continue;
        }

        char copy_line[MAX_LINE_LENGTH];
        strcpy(copy_line, line);
        tokenize_line(line, input_csv_delimiter);

        if (total_lines == 2) {
            assign_variables_type();
            assign_variables_value();

            input_code = parse_expression(expr, variables);

            if (output_fields_copy) {
                output_delimiter = var_get_str("output_csv_delimiter", input_csv_delimiter);

                tokenize_line(output_fields_copy, output_delimiter);
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
        if (!is_true) continue;

        written_lines ++;
 
        if (!output_code_count) {
            fwrite(copy_line, sizeof(char), strlen(copy_line), outputFile);
            continue;
        }

        char output_line[MAX_LINE_LENGTH];
        char *output_line_ptr = output_line;
        output_line[0] = '\0';

        for (int index = 0; index < output_code_count; index++) {
            const Variable res = execute_code_datatype(output_code[index], variables);

            switch (res.type) {
                case VAR_NUMBER:
                    sprintf(output_line_ptr, "%f", res.value);
                    break;

                case VAR_STRING:
                    sprintf(output_line_ptr, "%s", res.str);
                    if (res.type == VAR_STRING && res.is_dynamic) mem_free((void *) res.str);
                    break;

                case VAR_DATETIME:
                    {
                        char buffer[20];
                        strftime(buffer, sizeof(buffer), DATE_FORMAT, &res.datetime);
                        sprintf(output_line_ptr, "%s", buffer);
                    }
                    break;

                default:
                    fprintf(stderr, "Unknown variable type %d!\n", res.type);
                    exit(EXIT_FAILURE);
            }

            output_line_ptr = output_line + strlen(output_line);
            if (index < output_code_count - 1) {
                sprintf(output_line_ptr, "%s", output_delimiter);
                output_line_ptr = output_line + strlen(output_line);
            }
        }
        fwrite(output_line, sizeof(char), strlen(output_line), outputFile);
        fwrite("\n", sizeof(char), strlen("\n"), outputFile);
    }

    double pct_written = (double)written_lines * 100 / total_lines;
    printf(
        COLOR_YELLOW "\nWritten %s: %d of %d lines written (%.1f%%)\n" COLOR_RESET, 
        output_filename, written_lines, total_lines, pct_written
    );

    fclose(inputFile);
    fclose(outputFile);

    mem_free((void *) output_fields_copy);

    for (int index=0; index < output_code_count; index++) {
        parse_cleaning(output_code[index]);
    }
    parse_cleaning(input_code);
    var_cleaning(false);
}

int main(int argc, char *argv[]) {
    Config config = {
        .keys = NULL, 
        .values = NULL, 
        .count = 0
    };
    variables_base = 0;
    variables[variables_base].type = VAR_END;
    
    const char *home_dir_env = getenv("HOME");
    if (home_dir_env != NULL) {
        if (strlen(home_dir_env) + 1 >= PATH_MAX) {
            fprintf(stderr, "HOME way too long!");
            return EXIT_FAILURE;
        }
        conf_add_key_str(&config, "home_dir", home_dir_env);
    }

    if (argc == 2) {
        conf_read_file(&config, argv[1]);
    }
    else if (argc == 4) {
        conf_add_key_str(&config, "source_dir", argv[1]);
        conf_add_key_str(&config, "dest_dir", argv[2]);
        conf_add_key_str(&config, "input_script", argv[3]);
    }
    assign_variables_config(&config);

    const char *input_dir = var_get_str("source_dir", NULL);
    const char *output_dir = var_get_str("dest_dir", NULL);
    const char *expr = var_get_str("input_script", NULL);
    input_csv_delimiter = var_get_str("input_csv_delimiter", ",");

    if (!input_dir || !output_dir || !expr) {
        fprintf(stderr, "Usage: %s <conf file> | <input_directory> <output_directory> <expression>\n", argv[0]);
        return EXIT_FAILURE;
    }

    DIR *dir = opendir(input_dir);
    if (dir == NULL) {
        fprintf(stderr, "Error opening input directory: \"%s\"\n", input_dir);
        return EXIT_FAILURE;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG) {
            continue;
        }

        const char *ext = strrchr(entry->d_name, '.');
        if (ext && strcmp(ext, ".csv") == 0) {
            char input_filename[PATH_MAX],
                 output_filename[PATH_MAX];
            snprintf(input_filename, sizeof(input_filename), "%s/%s", input_dir, entry->d_name);
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
