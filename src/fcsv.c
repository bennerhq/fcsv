/**
 * This program reads a CSV file and filters out rows that match a 
 * given expression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <ctype.h>

#include "../hdr/exec.h"
#include "../hdr/expr.h"

#define COLOR_RESET "\033[0m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"

#define MAX_LINE_ITEMS  1024

#define CSV_SEPERATOR   ','
#define DATE_FORMAT     "%Y-%m-%dT%H:%M:%S"

const Instruction *code; 

Variable variables[MAX_LINE_ITEMS];
int variables_size = 0;

char *tokens[MAX_LINE_ITEMS];
int tokens_pos[MAX_LINE_ITEMS];

int find_char_tokens_pos(const char *str, char ch, int *tokens_pos) {
    int count = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == ch) {
            tokens_pos[count++] = i;
        }
    }
    return count;
}

void tokenize_line(char *line) {
    int count = find_char_tokens_pos(line, CSV_SEPERATOR, tokens_pos);
    int start = 0;
    for (int i = 0; i <= count; i++) {
        int end = (i == count) ? strlen(line) : tokens_pos[i];
        line[end] = '\0';
        tokens[i] = &line[start];
        start = end + 1;
    }
}

void assign_variables_name() {
    for (int index = 0; tokens[index] != NULL; index++) {
        variables[index].name = tokens[index];
        variables[index].type = VAR_UNKNOWN;
    }
}

int is_valid_double(const char *str) {
    char *endptr;
    strtod(str, &endptr);
    return *endptr == '\0' && endptr != str;
}

int is_valid_iso_datetime(const char *str) {
    struct tm tm;
    return strptime(str, "%Y-%m-%dT%H:%M:%S", &tm) != NULL;
}

void assign_variables_type() {
    for (int index = 0; tokens[index] != NULL; index++) {
        if (is_valid_double(tokens[index])) {
            variables[index].type = VAR_NUMBER;
        } else if (is_valid_iso_datetime(tokens[index])) {
            variables[index].type = VAR_DATE;
        } else {
            variables[index].type = VAR_STRING;
        }
    }
}

void assign_variables_value() {
    for (int index = 0; tokens[index] != NULL; index++) {
        Variable *var = &variables[index];
        switch (var->type) {
            case VAR_NUMBER:
                var->value = atof(tokens[index]);
                break;

            case VAR_STRING:
                var->string = tokens[index];
                break;

            case VAR_DATE:
                strptime(tokens[index], DATE_FORMAT, &var->tm);
                break;

            case VAR_UNKNOWN:
            case VAR_END:
                break;
        }
    }
}

void print_variables() {
    printf("Variables:\n");
    for (int index = 0; tokens[index] != NULL; index ++) {
        Variable *var = &variables[index];
        printf("\t%d: %s = ", index, var->name);
        if (var->type == VAR_NUMBER) {
            printf("%f\n", var->value);
        } else {
            printf("%s\n", var->string);
        }
    }
}

void process_csv(const char *input_filename, const char *output_filename, const char *expr) {
    int total_lines = 0;
    int written_lines = 0;
    int last_progress = -1;
    long processed_size = 0;

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

    if (code == NULL) {
        code = parse_expression(expr, variables);

        print_code(code);
    }

    while (fgets(line, sizeof(line), inputFile) != NULL) {
        processed_size += strlen(line);
        total_lines ++;

        strcpy(copy_line, line);

        tokenize_line(line);
        if (total_lines == 1) {
            assign_variables_type();
        }
        assign_variables_value();

        int use = execute_code(code, variables);
        if (use) {
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

    double pct_written = (double)written_lines * 100 / total_lines;
    printf(
        COLOR_YELLOW "Written %s: %d of %d lines written (%.1f%%)\n" COLOR_RESET, 
        output_filename, written_lines, total_lines, pct_written);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input_directory> <output_directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_dir = argv[1];
    const char *output_dir = argv[2];
    const char *expr = argv[3];

    DIR *dir;
    struct dirent *entry;

    if ((dir = opendir(input_dir)) == NULL) {
        perror("Error opening input directory");
        return EXIT_FAILURE;
    }

    code = NULL;

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

    return EXIT_SUCCESS;
}
