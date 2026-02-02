#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/wait.h>
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

#define LOG_GENERAL_ERROR(msg) \
do { \
    fputs(C_BOLD C_RED "[ERROR] " msg C_RESET "\n", stderr); \
} while(0)

#define LOG_MEMORY_ALLOCATION_FAILED LOG_GENERAL_ERROR("Memory allocation failed.")

char *custom_args[1] = {"--no-clear"};

void handle_sigint(int);
bool fork_exec(char *command, char **args);
void filter_args(int n, char **args, char **file, char **occec_args, char **compilation_args);
char *add_arg(char *dest, char *arg);
int get_mode(char *file);
char **build_compilation_args(char *file, char *compiler, char *compilation_args, unsigned b_args_count, char **base_args);
void free_args(char **args);
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
        LOG_GENERAL_ERROR("No file provided... retry with one.");
        exit(1);
    }

    mode = get_mode(file);

    if(mode == -1) {
        free(compilation_args);
        LOG_GENERAL_ERROR("Only .c and .ml files are supported, the provided fine extension is not valid.");
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
        fputs(C_BOLD C_GREEN "[SUCCESS] All operations went fine." C_RESET "\n", stdout);
    } else if(result == -1) {
        fputs(C_BOLD C_RED "[FAILURE] Something went wrong, check console output." C_RESET "\n", stdout);
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

bool fork_exec(char *command, char **args) {
    pid_t pid = fork();

    if(pid == -1) {
        perror("Fork Failed");
        return false;
    } else if(pid == 0) {
        execvp(command, args);

        perror("Exec failed, exiting");
        _exit(1);
    }

    int process_status;
    waitpid(pid, &process_status,0);

    if (WIFEXITED(process_status) && WEXITSTATUS(process_status) == 0) {
        return true;
    }

    return false;
}

void filter_args(int n, char **args, char **file, char **occec_args, char **compilation_args) {
    for(int i = 1; i < n; i++) {
        if(args[i][0] != '-') {
            if(*file == NULL) {
                *file = args[i];
            } else {
                LOG_GENERAL_ERROR("Passing more than one file is not supported!");
                free(*compilation_args);
                exit(1);
            }
        } else if(args[i][0] == '-' && args[i][1] == 'o' && args[i][2] == '\0') {
            fputs(C_BOLD C_RED "[ERROR] Avoid using the -o flag, output is always named " OUT "." C_RESET "\n", stderr);
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
        LOG_MEMORY_ALLOCATION_FAILED;
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

char **build_compilation_args(char *file, char *compiler, char *compilation_args, unsigned b_args_count, char **base_args) {
    int max_args = 6 + b_args_count;
    char **args = malloc(max_args * sizeof(char *));
    if(args == NULL) {
        return NULL;
    }

    int i = 0;
    args[i++] = strdup(compiler);
    args[i++] = strdup(file);
    args[i++] = strdup("-o");
    args[i++] = strdup(OUT);

    for(unsigned j = 0; j < b_args_count; j++) {
        args[i + j] = strdup(base_args[j]);
    }
    i += b_args_count;

    if(compilation_args != NULL) {
        args[i++] = strdup(compilation_args);
    }

    args[i] = NULL;
    return args;
}

void free_args(char **args) {
    if(args == NULL) return;

    for(unsigned i = 0; args[i] != NULL; i++) {
        free(args[i]);
    }

    free(args);
    return;
}

void rm_compiled_code(void) {
        int clean_response = remove(OUT);
        if(clean_response != 0) {
            LOG_GENERAL_ERROR("Failed to clean " OUT " file after execution");
        }
}

int c_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_c_args[] = {"-Wall", "-Wextra", "-fsanitize=address"};

    char **compilation_command = build_compilation_args(file, C_COMPILER, compilation_args, 3, useful_c_args);
    if(compilation_command == NULL) {
        LOG_MEMORY_ALLOCATION_FAILED; 
        return -1;
    }

    printf(C_CYAN "=> Compiling %s into " OUT C_RESET "\n", file);
    bool compilation_success = fork_exec(C_COMPILER, compilation_command);
    free_args(compilation_command);
    
    if(!compilation_success) {
        LOG_GENERAL_ERROR("Compilation failed.");
        return -1;
    }

    state = 2;

    fputs(C_CYAN "=> Executing " OUT C_RESET "\n", stdout);
    char *execution_command= malloc((strlen(OUT) + 3) * sizeof(char));
    if(execution_command == NULL) { 
        LOG_MEMORY_ALLOCATION_FAILED;
        return -1;
    }

    strcpy(execution_command, "./");
    strcat(execution_command, OUT);

    bool execution_success = fork_exec(execution_command, (char*[]){execution_command, NULL});
    free(execution_command);

    if(!execution_success) {
        LOG_GENERAL_ERROR("Failed to execute " OUT ".");
        return -1;
    }

    if(clean) {
        state = 3;
        fputs(C_CYAN "=> Cleaning " OUT C_RESET "\n", stdout);
        rm_compiled_code();
    }

    return 0;
}

int ocaml_compile_run_clean(char *file, char *compilation_args, bool clean) {
    char *useful_ml_args []= {"placeholder :)"};
    char **compilation_command = build_compilation_args(file, ML_COMPILER, compilation_args, 0, useful_ml_args);
    if(compilation_command == NULL) { 
        LOG_MEMORY_ALLOCATION_FAILED;
        return -1;
    }

    printf(C_CYAN "=> Compiling %s into " OUT C_RESET "\n", file);
    bool compilation_success = fork_exec(ML_COMPILER, compilation_command);
    free_args(compilation_command);

    if(!compilation_success) {
        LOG_GENERAL_ERROR("Compilation failed.");
        return -1;
    }

    state = 2;

    fputs(C_CYAN "=> Executing " OUT C_RESET "\n", stdout);

    bool execution_success = fork_exec(ML_RUN, (char *[]){ML_RUN, OUT, NULL});
    
    if(!execution_success) {
        LOG_GENERAL_ERROR("Failed to execute " OUT ".");
        return -1;
    }

    if(clean) {
        state = 3;

        fputs(C_CYAN "=> Cleaning " OUT C_RESET "\n", stdout);
        rm_compiled_code();

        char *file_name = malloc(strlen(file) + 2);
        if(file_name == NULL) {
            LOG_MEMORY_ALLOCATION_FAILED;
            return -1;
        }
        
        strcpy(file_name, file);
        
        char *dot = strrchr(file_name, '.');

        strcpy(dot, ".cmo");

	    int clean_cmo_response = remove(file_name);
	    if(clean_cmo_response != 0) {
            LOG_GENERAL_ERROR("Failed to clean the .cmo file after execution.");
	    }

        strcpy(dot, ".cmi");

	    int clean_cmi_response = remove(file_name); 
	    if(clean_cmi_response != 0) {
            LOG_GENERAL_ERROR("Failed to clean the .cmi file after execution.");
            return -1;
	    } 

        free(file_name);
    }

    return 0;
}
