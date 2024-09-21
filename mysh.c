#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <glob.h>


#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef MAX_ARGS
#define MAX_ARGS 200
#endif

#ifndef BUFLENGTH
#define BUFLENGTH 200
#endif

int prevstatus =0;

static int imode;
void execute_c(char **, char*, char*);
char* getpath(char*);
int my_cd(char**);
int my_exit(char**);
int my_pwd(char**);
int my_which(char**);
void my_conditionals(char *tokens[], int conditional_flag, int *prevstatus);

typedef struct
{
    //int in_fd;
    //int out_fd;
    char *in;
    char *out;
} Pipecommand;

void printmysh()
{
    char mysha[7];
    strcpy(mysha, "mysh> ");
    write(STDIN_FILENO, mysha, 7);
}

void read_lines(int fd, void (*use_line)(void *, char *), void *arg)
{
    int buflength = BUFLENGTH;
    char *buf = malloc(BUFLENGTH);
    int pos = 0;
    int bytes;
    int line_start;
    while ((bytes = read(fd, buf + pos, buflength - pos)) > 0)
    {
        line_start = 0;
        int bufend = pos + bytes;
        while (pos < bufend)
        {
            if (buf[pos] == '\n')
            {
                buf[pos] = '\0';
                use_line(arg, buf + line_start);
                line_start = pos + 1;
            }
            pos++;
        }
        // no partial line
        if (line_start == pos)
        {
            pos = 0;
        }
        else if (line_start > 0)
        {
            int segment_length = pos - line_start;
            memmove(buf, buf + line_start, segment_length);
            pos = segment_length;
        }
        else if (bufend == buflength)
        {
            buflength *= 2;
            buf = realloc(buf, buflength);
            continue; //fill in rest of line
        }

        if(imode)
        {
            printmysh();
        }
    }
    // partial line in buffer after EOF
    if (pos > 0) 
    {
        if (pos == buflength)
        {
            buf = realloc(buf, buflength + 1);
        }
        buf[pos] = '\0';
        use_line(arg, buf + line_start);
    }
    free(buf);
}

char* addspaces(char *line) //if need to
{
    int k = 0;
    char *newline = (char *)malloc(strlen(line)*10);
    //char newline[MAX_ARGS];

    for (int i = 0; i < strlen(line); i++)
    {
        if (line[i] != '>' && line[i] != '<' && line[i] != '|')
        {
            newline[k] = line[i];
            k++;
        }
        else
        {
            newline[k] = ' ';
            k++;
            newline[k] = line[i];
            k++;
            newline[k] = ' '; //add spaces, still treated as same delimiter
            k++;
        }
    }
    newline[k++] = '\0';
    return newline;
}

void expand_wildcards(char *token, char **args, int *argc)
{
    glob_t glob_result;
    int ret = glob(token, GLOB_TILDE, NULL, &glob_result);
    //int new_argc = *argc + glob_result.gl_pathc;
    
    if (ret != 0)
    {
        fprintf(stderr, "Error expanding wildcard: %s\n", token);
        exit(EXIT_FAILURE);
    }
    
    int original_argc = *argc;
    
    for (size_t i = 0; i < glob_result.gl_pathc; ++i)
    {
        if (*argc >= MAX_ARGS)
        {
            fprintf(stderr, "Exceeded maximum number of arguments\n");
            exit(EXIT_FAILURE);
        }
        
        // Check if we have enough space in args array
        if ((*argc - original_argc) >= glob_result.gl_pathc)
        {
            // If there's enough space, just copy the pointer
            args[*argc] = glob_result.gl_pathv[i];
        }
        else
        {
            /*if (glob_result.gl_pathv[i] != NULL)
            {
                printf("%s\n", glob_result.gl_pathv[i]);
            }
            //If not enough space, dynamically reallocate
            //args = realloc(args, (*argc + glob_result.gl_pathc - original_argc) * sizeof(char*));
            //if (args == NULL)
            //{
                //fprintf(stderr, "Memory allocation failed\n");
                //exit(EXIT_FAILURE);
            //}*/
            char name[BUFLENGTH];
            strcpy(name, glob_result.gl_pathv[i]);
            args[*argc] = name;
        } 
        (*argc)++;
    }
    
    globfree(&glob_result);
}

//void execute_pipe_command(char **args, int inin_fd, int inout_fd, char **args_pipe, int outin_fd, int outout_fd)
void execute_pipe_command(char **args, char* leftin, char *leftout, char **args_pipe, char *rightin, char *rightout)
{
    int p[2];
    pipe(p); 
    
    pid_t p1 = fork();
    if(p1 < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (p1 == 0)
    {
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        if (leftin != NULL)
        {
            int in_fd = open(leftin, O_RDONLY);
            if (in_fd < 0)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (leftout != NULL)
        {
            int out_fd = open(leftout, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if (out_fd < 0)
            {
                perror("open");
                exit(EXIT_FAILURE);
            }
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        char *pa = getpath(args[0]);
        execv(pa, args);
        exit(EXIT_FAILURE);
    }
    else
    {
        pid_t p2 = fork();
        if (p2 < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (p2 == 0)
        {
            close(p[1]);
            dup2(p[0], STDIN_FILENO);
            if (rightin != NULL)
            {
                int in_fd = open(leftin, O_RDONLY);
                if (in_fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (rightout != NULL)
            {
                int out_fd = open(rightout, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (out_fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            char *pa = getpath(args_pipe[0]);
            execv(pa, args_pipe);
            exit(EXIT_FAILURE);
        }
        else
        {
            close(p[0]);
            close(p[1]);
            wait(NULL);
            wait(NULL);
        }
    }
}

void tokenize(void *st, char *line)
{
    char *args[MAX_ARGS];
    char *args_pipe[MAX_ARGS];
    int argc = 0;
    int argc_pipe = 0; 
    int pipe_found = 0; 
    
    glob_t glob_result;
    glob_t glob_result2;

    int wildcard_flag = 0;
    int wildcard_flag2 = 0; 

    char *newline = addspaces(line); //remove extra spaces
    char *token = strtok(newline, " ");

    if (token == NULL) {
        fprintf(stderr, "Error: Invalid command\n");
        free(newline);
        return;
    }

    char *redirect_filename = NULL;
    //int in_fd = STDIN_FILENO;
    //int out_fd = STDOUT_FILENO;
    char *in_file = NULL;
    char *out_file = NULL;

    int conditional_flag =0;

    Pipecommand left;
    Pipecommand right;
    left.in = NULL;
    left.out = NULL;
    right.in = NULL;
    right.out = NULL;
    //in.in_fd = STDIN_FILENO;
    //in.out_fd = STDOUT_FILENO;
    //out.in_fd = STDIN_FILENO;
    //out.out_fd = STDOUT_FILENO;

    if(strchr(newline, '|') != NULL)
    {
        pipe_found = 1; 
    }

    while (token != NULL && argc < MAX_ARGS -1)
    {
        if(strcmp(token, "|") == 0)
        {
            pipe_found = 1;
            break;
        }
        else if (strchr(token, '*'))
        {
            wildcard_flag = 1; 
            //expand_wildcards(token, args, &argc);
            int ret = glob(token, GLOB_TILDE, NULL, &glob_result);            
            if (ret != 0)
            {
                fprintf(stderr, "Error expanding wildcard: %s\n", token);
                exit(EXIT_FAILURE);
            }
                        
            for (size_t i = 0; i < glob_result.gl_pathc; ++i)
            {
                if (argc >= MAX_ARGS)
                {
                    fprintf(stderr, "Exceeded maximum number of arguments\n");
                    exit(EXIT_FAILURE);
                }
                args[argc++] = glob_result.gl_pathv[i];
            } 
        }
        else if (strcmp(token, "then")==0 || strcmp(token , "else")==0){
            /* if (argc == 0)
            {
                fprintf(stderr, "Error: Invalid conditional command\n");
                free(newline);
                return;
            } */
            if (strcmp(token, "then") == 0)
            {
                conditional_flag=1;
                token = strtok(NULL, " \t\r\n\a"); // Move to the next token
                
                continue;
            }
            else if (strcmp(token, "else") == 0)
            {
                conditional_flag = 2;
                token = strtok(NULL, " \t\r\n\a"); // Move to the next token
                continue;
            }
        }
        else if ((strcmp(token, "<") == 0))
        {
            if(pipe_found)
            {
                redirect_filename = strtok(NULL, " \t\r\n\a");
                left.in = redirect_filename;
            }
            else
            {
                redirect_filename = strtok(NULL, " \t\r\n\a");
                in_file = redirect_filename;
            }
        }
        else if ((strcmp(token, ">") == 0))
        {
            if(pipe_found)
            {
                redirect_filename = strtok(NULL, " ");
                left.out = redirect_filename;
            }
            else
            {
                redirect_filename = strtok(NULL, " ");
                out_file = redirect_filename;
            }
        }
        else
        {
            args[argc++] = token;
        }
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    if (pipe_found)
    {
        token = strtok(NULL, " ");
        /* if (token == NULL) {
            fprintf(stderr, "Error: Invalid pipe command\n");
            free(newline);
            return;
        } */
        while (token != NULL && argc_pipe < MAX_ARGS -1)
        {
            if (strcmp(token, "<") == 0)
            {
                redirect_filename = strtok(NULL, " ");
                right.in = redirect_filename;
            }
            else if (strcmp(token, ">") == 0)
            {
                redirect_filename = strtok(NULL, " ");
                right.out = redirect_filename;
            }
            else if (strchr(token, '*'))
            {
                wildcard_flag2 = 1; 
                int ret2 = glob(token, GLOB_TILDE, NULL, &glob_result2);            
                if (ret2 != 0)
                {
                    fprintf(stderr, "Error expanding wildcard: %s\n", token);
                    exit(EXIT_FAILURE);
                }
                            
                for (size_t i = 0; i < glob_result2.gl_pathc; ++i)
                {
                    if (argc_pipe >= MAX_ARGS)
                    {
                        fprintf(stderr, "Exceeded maximum number of arguments\n");
                        exit(EXIT_FAILURE);
                    }
                    args_pipe[argc_pipe++] = glob_result2.gl_pathv[i];
                } 
            }
            else
            {
                args_pipe[argc_pipe++] = token;
            }
            token = strtok(NULL, " ");
        }
        args_pipe[argc_pipe] = NULL;
        execute_pipe_command(args, left.in, left.out, args_pipe, right.in, right.out);
    }
    else
    {
        if(conditional_flag>0)
        {
            /// function for conditionals. 
            my_conditionals(args, conditional_flag, &prevstatus);
        }
        else {
            execute_c(args, in_file, out_file);
        }
    }

    if (wildcard_flag)
    {
        globfree(&glob_result);
    }
    if (wildcard_flag2)
    {
        globfree(&glob_result2);
    }

    free(newline);
}

void my_conditionals(char *tokens[], int conditional_flag, int *prevstatus)
{
    if (conditional_flag== 1) //"then" detected
    {
        if (*prevstatus ==0) // previous succeeded 
        {
            execute_c(tokens, NULL, NULL);
        }
    }
    else if (conditional_flag == 2) // "else" detected
    {
        if (*prevstatus != 0)
        {
            execute_c(tokens, NULL, NULL);
        }
    }
}

//the following 4 functions are for the built ins
int my_cd(char **arguments)
{
    if(arguments[1] == NULL)
    {
        fprintf(stderr, "Not enough arguments passed\n");
        exit(EXIT_FAILURE);
    }
    if (chdir(arguments[1]) != 0)
    {
        perror("cd");
    }
    return(EXIT_SUCCESS);
}

int my_pwd(char **arguments)
{
    char cwd[2048]; //max buffer length
    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
            printf("%s\n", cwd);
    }
    else
    {
        fprintf(stderr, "getcwd failed\n");
        exit(EXIT_FAILURE);
    }

    return(EXIT_SUCCESS);
}

int my_exit(char **arguments)
{
    for (int i = 1; arguments[i]!= NULL; i++) //print out rest of tokens
    {
        printf("%s ", arguments[i]);
    }
    if (arguments[1]!= NULL) //printed out stuff
    {
            printf("\n");
    }
    if(imode)
    {
        printf("mysh: exiting my shell!!! \n");
    } 
    exit(EXIT_SUCCESS);
}

int my_which(char **arguments)
{
    if((arguments[1] == NULL) || (arguments[2] != NULL))
    {
        fprintf(stderr, "Wrong number of arguments passed\n");
        exit(EXIT_FAILURE);
    }
    if((strcmp(arguments[1], "cd") == 0) || (strcmp(arguments[1], "cd")==0) || (strcmp(arguments[1], "cd")==0) || (strcmp(arguments[1], "cd")==0))
    {
        fprintf(stderr, "Passed name of a built in\n");
        exit(EXIT_FAILURE);
    }

    char *p = getenv("PATH");
    char *token;
    char name[2048];
    int found = 0; 

    if (p == NULL)
    {
        fprintf(stderr, "messed up\n");
        exit(EXIT_FAILURE);
    }
    strncpy(name, p, 2048);
    token = strtok(name, ":");
    while (token != NULL)
    {
        printf("%s/%s", token, arguments[1]);
        token = strtok(NULL, ":");
        found = 1;
    }
    if (found)
    {
        return(EXIT_SUCCESS);
    }
    else
    {
        fprintf(stderr, "Path not found\n");
        return(EXIT_FAILURE);
    }
}

//since we can't use execvp, we must search for the file name
char* getpath(char* arg)
{
    char *pathname;
    char* direct[] = {"/usr/local/bin", "/usr/bin", "/bin", NULL};

    if (strchr(arg, '/') != NULL)
    {
        if (access(arg, X_OK) == 0)
        {
            pathname = (char*)malloc(strlen(arg) + 1);
            strcpy(pathname, arg);
            return pathname;
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        for (int i = 0; direct[i] != NULL; i++)
        {
            pathname = (char*)malloc(100); //can't be more than 100 bytes
            snprintf(pathname, 100, "%s/%s", direct[i], arg);
            if (access(pathname, X_OK) == 0)
            {
                return pathname;
            }
        }
        return NULL;
    }
}
//make child, make a child process to execute the command using fork.
void execute_c(char *tokens[], char* in_file, char* out_file)
{
    if(strcmp(tokens[0], "cd") == 0)
    {
        my_cd(tokens);
    }
    else if (strcmp(tokens[0], "exit") == 0)
    {
        my_exit(tokens);
    }
    else if (strcmp(tokens[0], "pwd") == 0)
    {
        my_pwd(tokens);
    }
    else if (strcmp(tokens[0], "exit") == 0)
    {
        my_which(tokens);
    }
    else
    {
        char* pathname = getpath(tokens[0]);
        if (pathname == NULL)
        {
            fprintf(stderr, "Error: Invalid command '%s'\n", tokens[0]);
            free(pathname); // Add this line to avoid memory leak
            return;
        }
        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            if (in_file != NULL)
            {
                int in_fd = open(in_file, O_RDONLY);
                if (in_fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (out_file != NULL)
            {
                int out_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
                if (out_fd < 0)
                {
                    perror("open");
                    exit(EXIT_FAILURE);
                }
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
 
            char* pathname = getpath(tokens[0]);
            if (pathname != NULL)
            {
                execv(pathname, tokens);
            }
            else
            {
                fprintf(stderr, "couldn't find path\n");
                exit(EXIT_FAILURE);
            }
            //free(pathname);   
        }
        else
        {
            wait(NULL);
            int stat;

            /* do
            {
                //waitpid(pid, &stat, WUNTRACED);
                wait(NULL);
            } while (!WIFEXITED(stat) && !WIFSIGNALED(stat)); */ //terminate if control C or kill 
            
            if (WIFEXITED(stat)){ // update prevstatus with exit status of current command 
                prevstatus = WEXITSTATUS(stat);
            }
            else if (WIFSIGNALED(stat)){
                prevstatus = 1; // will set a non zero status for killed processes
            } 

        
        }

    }
}

int main(int argc, char*argv[])
{
    int fd;
    imode = 0;
    if (argc > 2)
    {
        printf("not doing what your supposed to: must give 1 argument to mysh or none\n");
        return (EXIT_FAILURE);
    }
    if (argc == 1) //interactive mode
    {
        fd = STDIN_FILENO;
        imode = isatty(STDIN_FILENO);
    }
    if (argc == 2) //batch mode
    {
        char *filename = argv[1]; 

        fd = open(filename, O_RDONLY); //open path given as argument
        if (fd < 0)
        {
            printf("failed to open");
            return(EXIT_FAILURE);
        }
        if(isatty(fd))
        {
            imode = 1;
        }
    }

    if(imode) //interactive mode, returned 1
    {
        printf("Welcome to my shell!\n");
        printmysh();
    }

    int n = 0;
    read_lines(fd, tokenize, &n);
    close(fd);

    return EXIT_SUCCESS;
}