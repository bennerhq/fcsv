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

const Instruction *code; 

#define MAX_LINE_ITEMS 1024
Variable variables[MAX_LINE_ITEMS];

int filter_dynamic(char *line) {
    char *token;
    double var_values[MAX_LINE_ITEMS];
    int index = 0;

    char line_copy[MAX_LINE_ITEMS];
    strncpy(line_copy, line, MAX_LINE_ITEMS);
    line_copy[MAX_LINE_ITEMS - 1] = '\0';

    token = strtok(line_copy, ",");
    while (token != NULL) {
        var_values[index++] = atoi(token);
        token = strtok(NULL, ",");
    }

    return execute_code(code, variables);
}

void process_csv(const char *input_filename, const char *output_filename, const char *expr) {
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

    printf(COLOR_CYAN "Processing %s\n" COLOR_RESET, input_filename);

    // Read the header line
    char headder_line[MAX_LINE_ITEMS];
    if (fgets(headder_line, sizeof(headder_line), inputFile) != NULL) {
        fwrite(headder_line, sizeof(char), strlen(headder_line), outputFile);

        // Tokenize header into global variable 
        char *token = strtok(headder_line, ",");
        int index = 0;
        while (token != NULL) {
            if (index >= MAX_LINE_ITEMS) {
                fprintf(stderr, "Error: Too many columns in CSV file\n");
                exit(EXIT_FAILURE);
            }
            Variable *var = &variables[index];
            index ++;

            var->name = token;
            var->type = VAR_NUMBER;
            var->value = 0;

            token = strtok(NULL, ",");
        }
        variables[index].type = VAR_END;
    }

    if (code == NULL) {
        code = parse_expression(expr, variables);

        print_code(code);
    }

    // Read and process each line
    char line[MAX_LINE_ITEMS];
    int total_lines = 0;
    int written_lines = 0;
    int last_progress = -1;

    fseek(inputFile, 0, SEEK_END);
    long file_size = ftell(inputFile);
    fseek(inputFile, 0, SEEK_SET);

    long processed_size = 0;
    while (fgets(line, sizeof(line), inputFile) != NULL) {
        if (processed_size == 0) {
            char *element_token = strtok(line, ",");
            int element_index = 0;
            while (element_token != NULL) {
                if (isdigit(element_token[0])) {
                    variables[element_index].type = VAR_NUMBER;
                } else if (isalpha(element_token[0])) {
                    variables[element_index].type = VAR_STRING;
                } else {
                    variables[element_index].type = VAR_UNKNOWN;
                }
                element_index++;
                element_token = strtok(NULL, ",");
            }
        }

        total_lines++;
        processed_size += strlen(line);

        int okay = filter_dynamic(line);
        if (okay) {
            fwrite(line, sizeof(char), strlen(line), outputFile);
            written_lines++;    
        }

        // Print progress bar
        int progress = (int)((processed_size * 100) / file_size);
        if (progress != last_progress) {
            last_progress = progress;

            // Get terminal width
            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int terminal_width = w.ws_col;
            int bar_width = terminal_width * 0.5; // Leave space for percentage display

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
    printf(COLOR_YELLOW "Written %s: %d of %d lines written (%.1f%%)\n" COLOR_RESET, output_filename, written_lines, total_lines, pct_written);
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
            char input_filename[PATH_MAX];
            snprintf(input_filename, sizeof(input_filename), "%s/%s", input_dir, entry->d_name);

            char output_filename[PATH_MAX];
            snprintf(output_filename, sizeof(output_filename), "%s/%s", output_dir, entry->d_name);

            process_csv(input_filename, output_filename, expr);
        }
    }

    closedir(dir);

    return EXIT_SUCCESS;
}
