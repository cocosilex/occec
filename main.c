#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define C_COMPILER "clang"
#define ML_COMPILER "ocamlc"
#define ML_RUN "ocamlrun"

#define OUT "compiled.code"

#define C_RESET "\033[0m"
#define C_BOLD "\033[1m"

#define C_GREEN "\033[32m"
#define C_CYAN "\033[36m"
#define C_RED "\033[31m"

char *custom_args[1] = {"--no-clear"};

void filter_args(int n, char**args, char **file, char **occec_args, char **compilation_args);
char *add_arg(char *dest, char *arg);
int get_mode(char *file);
char *build_compilation_command(char *compiler, char *file, char *compilation_args, char *base_args);
void c_compile_run_clean(char *file, char *compilation_args, bool clean);
void ocaml_compile_run_clean(char *file, char *compilation_args, bool clean);

int main(int argc, char **argv) {
    bool clean = true;
    int mode = 0; // 0 for C, 1 for Ocaml

    char *file = NULL; // mandatory, will crash if not provided!
    char *occec_args = NULL; // if one or more custom_args are provided
    char *compilation_args = NULL; // if the user want to provide some custom compilation args
    filter_args(argc, argv, &file, &occec_args, &compilation_args);

    if(file == NULL) {
        fprintf(stderr, "%s%s[ERROR] No file provided... please retry with one.%s\n", C_RED, C_BOLD, C_RESET);
        exit(1);
    }

    mode = get_mode(file);

    if(mode == -1) {
        free(compilation_args);
        fprintf(stderr, "%s%s[ERROR] Only .c and .ml extensions are supported, the file provided is not valid.%s\n", C_RED, C_BOLD, C_RESET);
        exit(0);
    }

    if(occec_args != NULL) {
        clean = false;
    }

    if(mode == 0) {
        c_compile_run_clean(file, compilation_args, clean);
    } else {
        //printf("[INFO] Ocaml is not implemented for now.\n");
        ocaml_compile_run_clean(file, compilation_args, clean);
    }

    free(compilation_args);
    return 0;
}

void filter_args(int n, char **args, char **file, char **occec_args, char **compilation_args) {
    for(int i = 1; i < n; i++) {
        if(args[i][0] != '-') {
            if(*file == NULL) {
                *file = args[i];
            } else {
                fprintf(stderr, "%s%s[ERROR] Passing more than one file is not supported!%s\n", C_RED, C_BOLD, C_RESET);
                free(*compilation_args);
                exit(1);
            }
        } else if(args[i][0] == '-' && args[i][1] == 'o' && args[i][2] == '\0') {
            fprintf(stderr, "%s%s[ERROR] Avoid using the -o flag, output is always named %s%s\n", C_RED, C_BOLD, OUT, C_RESET);
            free(*compilation_args);
            exit(1);
        } else {
            if(strcmp(args[i], custom_args[0]) == 0) {
                *occec_args = custom_args[0];
            } else {
                *compilation_args = add_arg(*compilation_args, args[i]);
                if(*compilation_args == NULL) {
                    free(*compilation_args);
                    exit(1);
                }
            }
        }
    }
}

char *add_arg(char *dest, char *arg) {
    unsigned long current_len = (dest == NULL) ? 0 : strlen(dest);
    unsigned long extension = strlen(arg);
    unsigned long new_len = current_len + extension + (current_len > 0 ? 1 : 0) + 1;

    char *new_dest = realloc(dest, new_len);
    if(new_dest == NULL) {
        free(new_dest);
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed (it's very sad :/), retry!%s\n", C_RED, C_BOLD, C_RESET);
        return NULL;
    }

    if (current_len > 0) {
        strcat(new_dest, " "); 
    } else {
        new_dest[0] = '\0'; 
    }
    strcat(new_dest, arg);

    return new_dest;
}

int get_mode(char *file) {
    unsigned long file_name_len = strlen(file);
    if (file_name_len < 2) return -1;

    if(file[file_name_len - 1] == 'c' && file[file_name_len - 2] == '.') {
        return 0;
    } 
    
    if (file_name_len < 3) return -1;
    if(file[file_name_len - 1] == 'l' && file[file_name_len - 2] == 'm' && file[file_name_len - 3] == '.') {
        return 1;
    }
    return -1;
}

char *build_compilation_command(char *compiler, char *file, char *compilation_args, char *base_args) {
    unsigned long total_len = strlen(compiler) + strlen(file) + strlen(base_args) + strlen(OUT)+ 4;
    
    char *output_arg = " -o ";
    total_len += 4;    

    if(compilation_args != NULL) {
        total_len += strlen(compilation_args) + 1;
    }

    char *compilation_command = malloc(total_len*sizeof(char));
    strcpy(compilation_command, compiler);
    strcat(compilation_command, " ");
    strcat(compilation_command, file);
    strcat(compilation_command, output_arg);
    strcat(compilation_command, OUT);
    strcat(compilation_command, " ");
    strcat(compilation_command, base_args);
    strcat(compilation_command, " ");
    if(compilation_args != NULL) {
        strcat(compilation_command, compilation_args);
    }
    return compilation_command;
}

void c_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_c_args = "-Wall -Wextra -fsanitize=address";
    char *compilation_command = build_compilation_command(C_COMPILER, file, compilation_args, useful_c_args);

    printf("%s=> Compiling %s into %s%s\n", C_CYAN, file, OUT, C_RESET);
    int compilation_response = system(compilation_command);
    free(compilation_command);

    if(compilation_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Compilation failed%s\n", C_RED, C_BOLD, C_RESET);
        return;
    }

    printf("%s=> Executing %s%s\n", C_CYAN, OUT, C_RESET);
    char *execution_command = malloc((strlen(OUT) + 3)*sizeof(char));
    strcpy(execution_command, "./");
    strcat(execution_command, OUT);

    int execution_response = system(execution_command);
    free(execution_command);
    if(execution_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Failed to execute the compiled code.%s\n", C_RED, C_BOLD, C_RESET);
        return;
    }

    if(clean) {
        printf("%s=> Cleaning %s%s\n", C_CYAN, OUT, C_RESET);
        char *clean_command = malloc((strlen(OUT) + 4)*sizeof(char));
        strcpy(clean_command, "rm ");
        strcat(clean_command, OUT);

        int clean_response = system(clean_command);
        free(clean_command);
        if(clean_response != 0) {
            fprintf(stderr, "%s%s[ERROR] Failed to clean the file after execution.%s\n", C_RED, C_BOLD, C_RESET);
        }
    }

    printf("%s%s[SUCCESS] All operations went fine.%s\n", C_GREEN, C_BOLD, C_RESET);
}

void ocaml_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_c_args = "";
    char *compilation_command = build_compilation_command(ML_COMPILER, file, compilation_args, useful_c_args);

    printf("%s=> Compiling %s into %s%s\n", C_CYAN, file, OUT, C_RESET);
    int compilation_response = system(compilation_command);
    free(compilation_command);

    if(compilation_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Compilation failed%s\n", C_RED, C_BOLD, C_RESET);
        return;
    }

    printf("%s=> Executing %s%s\n", C_CYAN, OUT, C_RESET);
    char *execution_command = malloc((strlen(OUT) + strlen(ML_RUN) + 2)*sizeof(char));
    strcpy(execution_command, ML_RUN);
    strcat(execution_command, " ");
    strcat(execution_command, OUT);

    int execution_response = system(execution_command);
    free(execution_command);
    if(execution_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Failed to execute the compiled code.%s\n", C_RED, C_BOLD, C_RESET);
        return;
    }

    if(clean) {
        // this is intended to no clean .cmi and .cmo files for now, will eventually be changes later
        printf("%s=> Cleaning %s%s\n", C_CYAN, OUT, C_RESET);
        char *clean_command = malloc((strlen(OUT) + 4)*sizeof(char));
        strcpy(clean_command, "rm ");
        strcat(clean_command, OUT);

        int clean_response = system(clean_command);
        free(clean_command);
        if(clean_response != 0) {
            fprintf(stderr, "%s%s[ERROR] Failed to clean the file after execution.%s\n", C_RED, C_BOLD, C_RESET);
        }
    }

    printf("%s%s[SUCCESS] All operations went fine.%s\n", C_GREEN, C_BOLD, C_RESET);
}