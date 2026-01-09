#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

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

void handle_sigint(int);
void filter_args(int n, char**args, char **file, char **occec_args, char **compilation_args);
char *add_arg(char *dest, char *arg);
int get_mode(char *file);
char *build_compilation_command(char *compiler, char *file, char *compilation_args, char *base_args);
void rm_compiled_code(void);
int c_compile_run_clean(char *file, char *compilation_args, bool clean);
int ocaml_compile_run_clean(char *file, char *compilation_args, bool clean);

// 0 = processing args, 1 = building, 2 = executing, 3 = cleaning
volatile sig_atomic_t state = 0;

int main(int argc, char **argv) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up signal handler");
        return 1;
    }

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

    state = 1;
    int result;
    if(mode == 0) {
        result = c_compile_run_clean(file, compilation_args, clean);
    } else {
        result = ocaml_compile_run_clean(file, compilation_args, clean);
    }

    free(compilation_args);
    if(result == 0) {
        printf("%s%s[SUCCESS] All operations went fine.%s\n", C_GREEN, C_BOLD, C_RESET);
    } else if(result == -1) {
        printf("%s%s[FAILURE] Something went wrong, check console output.%s\n", C_GREEN, C_BOLD, C_RESET);
    }

    return 0;
}

void handle_sigint(int sig) {
    (void)sig; 

    const char msg[] = "\n" C_RED C_BOLD "[TERMINATION] Interrupted! Cleaning up..." C_RESET "\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    
    if(state == 2 || state == 3) {
        unlink(OUT); 
        // will clean .cmo/.cmi file later
    }

    _exit(130); 
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
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
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
    if(compilation_command == NULL) {
        return NULL;
    }

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

void rm_compiled_code(void) {
        int clean_response = remove(OUT);
        if(clean_response != 0) {
            fprintf(stderr, "%s%s[ERROR] Failed to clean the compiled.code file after execution.%s\n", C_RED, C_BOLD, C_RESET);
        }
}

int c_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_c_args = "-Wall -Wextra -fsanitize=address";
    char *compilation_command = build_compilation_command(C_COMPILER, file, compilation_args, useful_c_args);
    if(compilation_command == NULL) { 
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    printf("%s=> Compiling %s into %s%s\n", C_CYAN, file, OUT, C_RESET);
    int compilation_response = system(compilation_command);
    free(compilation_command);
    
    if (WIFSIGNALED(compilation_response) && (WTERMSIG(compilation_response) == SIGINT)) {
        handle_sigint(2);
    } else if(compilation_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Compilation failed%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    state = 2;

    printf("%s=> Executing %s%s\n", C_CYAN, OUT, C_RESET);
    char *execution_command = malloc((strlen(OUT) + 3)*sizeof(char));
    if(execution_command == NULL) { 
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }
    strcpy(execution_command, "./");
    strcat(execution_command, OUT);

    int execution_response = system(execution_command);
    free(execution_command);
    if (WIFSIGNALED(execution_response) && (WTERMSIG(execution_response) == SIGINT)) {
        handle_sigint(2);
    } else if(execution_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Failed to execute the compiled code.%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    if(clean) {
        state = 3;
        printf("%s=> Cleaning %s%s\n", C_CYAN, OUT, C_RESET);
        rm_compiled_code();
    }

    return 0;
}

int ocaml_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_ml_args = "";
    char *compilation_command = build_compilation_command(ML_COMPILER, file, compilation_args, useful_ml_args);
    if(compilation_command == NULL) { 
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    printf("%s=> Compiling %s into %s%s\n", C_CYAN, file, OUT, C_RESET);
    int compilation_response = system(compilation_command);
    free(compilation_command);

    if (WIFSIGNALED(compilation_response) && (WTERMSIG(compilation_response) == SIGINT)) {
        handle_sigint(2);
    } else if(compilation_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Compilation failed%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    state = 2;

    printf("%s=> Executing %s%s\n", C_CYAN, OUT, C_RESET);
    char *execution_command = malloc((strlen(OUT) + strlen(ML_RUN) + 2)*sizeof(char));
    if(execution_command == NULL) { 
        fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }
    strcpy(execution_command, ML_RUN);
    strcat(execution_command, " ");
    strcat(execution_command, OUT);

    int execution_response = system(execution_command);
    free(execution_command);
    if (WIFSIGNALED(execution_response) && (WTERMSIG(execution_response) == SIGINT)) {
        handle_sigint(2);
    } else if(execution_response != 0) {
        fprintf(stderr, "%s%s[ERROR] Failed to execute the compiled code.%s\n", C_RED, C_BOLD, C_RESET);
        return -1;
    }

    if(clean) {
        state = 3;

        printf("%s=> Cleaning %s%s\n", C_CYAN, OUT, C_RESET);
        rm_compiled_code();

	    char *clean_cmo_cmi = malloc(strlen(file)*sizeof(char) + 2);
        if(clean_cmo_cmi == NULL) { 
            fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
            return -1;
        }

	    int file_len = strlen(file);
	    file[file_len - 1] = 'm';
	    file[file_len - 2] = 'c';
	    strcpy(clean_cmo_cmi, file);
	
	    char *clean_cmo = malloc(strlen(file)*sizeof(char) + 2);
        if(clean_cmo == NULL) { 
            fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
            return -1;
        }

	    strcpy(clean_cmo, clean_cmo_cmi);
	    strcat(clean_cmo, "o");

	    int clean_cmo_response = remove(clean_cmo);
	    free(clean_cmo);
	    if(clean_cmo_response != 0) {
            fprintf(stderr, "%s%s[ERROR] Failed to clean the .cmo file after execution.%s\n", C_RED, C_BOLD, C_RESET);
	    }

	    char *clean_cmi = malloc(strlen(file)*sizeof(char) + 2);
        if(clean_cmi == NULL) { 
            fprintf(stderr, "%s%s[ERROR] Some memory allocation failed, please retry...%s\n", C_RED, C_BOLD, C_RESET);
            return -1;
        }
	    strcpy(clean_cmi, clean_cmo_cmi);
	    strcat(clean_cmi, "i");

	    int clean_cmi_response = remove(clean_cmi);
	    free(clean_cmi);
	    if(clean_cmi_response != 0) {
            fprintf(stderr, "%s%s[ERROR] Failed to clean the .cmi file after execution.%s\n", C_RED, C_BOLD, C_RESET);
            return -1;
	    } 

        free(clean_cmo_cmi);
    }

    return 0;
}
