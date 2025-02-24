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

char csv_delimiter = ',';

Variable variables[MAX_LINE_ITEMS];

char *tokens[MAX_LINE_ITEMS];

void tokenize_line(char *line) {
    int count = 0;
    char *start = line;
    char *end;

    while ((end = strchr(start, csv_delimiter)) != NULL) {
        *end = '\0';
        tokens[count++] = start;
        start = end + 1;
    }
    tokens[count++] = start;

    for (int i = 0; i < count; i++) {
        char *token = tokens[i];
        while (isspace((unsigned char)*token)) token++;
        tokens[i] = token;

        char *end = token + strlen(token) - 1;
        while (end > token && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';
    }
    tokens[count] = NULL;
}

int is_valid_double(const char *str) {
    char *endptr;
    strtod(str, &endptr);
    return *endptr == '\0' && endptr != str;
}

int is_valid_iso_datetime(const char *str) {
    struct tm datetime;
    return strptime(str, "%Y-%m-%dT%H:%M:%S", &datetime) != NULL;
}

void assign_variables_name() {
    int index = 0;
    for (index = 0; tokens[index] != NULL; index++) {
        variables[index].name = tokens[index];
        variables[index].type = VAR_UNKNOWN;
    }
    variables[index].type = VAR_END;
}

int assign_variables_type() {
    int type_changed = 0;

    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[index];
        if (is_valid_double(tokens[index])) {
            if (var->type != VAR_NUMBER) type_changed = 1;
            var->type = VAR_NUMBER;
        } else if (is_valid_iso_datetime(tokens[index])) {
            if (var->type != VAR_DATETIME) type_changed = 1;
            var->type = VAR_DATETIME;
        } else {
            if (var->type != VAR_STRING) type_changed = 1;
            var->type = VAR_STRING;
        }
    }

    return type_changed;
}

void assign_variables_value() {
    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[index];
        switch (var->type) {
            case VAR_NUMBER:
                var->value = atof(tokens[index]);
                break;

            case VAR_STRING:
                var->str = tokens[index];
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

void process_csv(const char *input_filename, const char *output_filename, const char *expr) {

    int total_lines = 0;
    int written_lines = 0;
    int last_progress = -1;
    long processed_size = 0;
    const Instruction *code = NULL; 

    char headder[MAX_LINE_ITEMS];
    char line[MAX_LINE_ITEMS];
    char copy_line[MAX_LINE_ITEMS];

    FILE *inputFile = fopen(input_filename, "rb");
    if (inputFile == NULL) {
        perror("Error opening input file\n");
        return;
    }

    FILE *outputFile = fopen(output_filename, "wb");
    if (outputFile == NULL) {
        perror("Error creating output file\n");
        return;
    }

    fseek(inputFile, 0, SEEK_END);
    long file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    printf(COLOR_CYAN "Processing %s\n" COLOR_RESET, input_filename);

    // Read the header line
    if (fgets(headder, sizeof(headder), inputFile) == NULL) {
        return;
    }
    fwrite(headder, sizeof(char), strlen(headder), outputFile);

    processed_size += strlen(headder);

    tokenize_line(headder);
    assign_variables_name();

    while (fgets(line, sizeof(line), inputFile) != NULL) {
        processed_size += strlen(line);
        total_lines ++;

        strcpy(copy_line, line);
        tokenize_line(line);

        if (total_lines == 1) {
            assign_variables_type();

            code = parse_expression(expr, variables);
            print_code(code, variables);
        }
        assign_variables_value();

        int is_true = execute_code(code, variables);
        if (is_true) {
            fwrite(copy_line, sizeof(char), strlen(copy_line), outputFile);
            written_lines++;
        }

        int progress = (int)((processed_size * 100) / file_size);
        if (progress != last_progress) {
            last_progress = progress;

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
    }
    printf("\n");

    fclose(inputFile);
    fclose(outputFile);

    parse_cleaning(code);

    double pct_written = (double)written_lines * 100 / total_lines;
    printf(
        COLOR_YELLOW "Written %s: %d of %d lines written (%.1f%%)\n" COLOR_RESET, 
        output_filename, written_lines, total_lines, pct_written);
}

int main(int argc, char *argv[]) {
    Config config = {NULL, NULL, 0};
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    const char *expr = NULL;

    if (argc == 2) {
        config = conf_read_file(argv[1]);

        input_dir = conf_get(&config, "source_dir", NULL);
        output_dir = conf_get(&config, "dest_dir", NULL);
        expr = conf_get(&config, "filter_script", NULL);

        csv_delimiter = conf_get(&config, "csv_delimiter", ",")[0];
    }
    else if (argc == 4) {
        input_dir = argv[1];
        output_dir = argv[2];
        expr = argv[3];    
    }

    if (!input_dir || !output_dir || !expr) {
        fprintf(stderr, "Usage: %s <input_directory> <output_directory> <expression>\n", argv[0]);
        return EXIT_FAILURE;
    }

    DIR *dir;
    struct dirent *entry;
    if ((dir = opendir(input_dir)) == NULL) {
        perror("Error opening input directory");
        return EXIT_FAILURE;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".csv") == 0) {
                char input_filename[PATH_MAX];
                snprintf(input_filename, sizeof(input_filename), "%s/%s", input_dir, entry->d_name);

                char output_filename[PATH_MAX];
                snprintf(output_filename, sizeof(output_filename), "%s/%s", output_dir, entry->d_name);

                process_csv(input_filename, output_filename, expr);
            }
        }
    }
    closedir(dir);

    conf_free(&config);
    mem_check();
    
    return EXIT_SUCCESS;
}
