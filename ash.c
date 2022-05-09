/* This is a program that produces a simple linux shell, user can execute normal commands as well as some of the built in
    commands such as changing direcotry, history, pipeline, jobs, ctrl z, bg, fg and kill.

    Author: Zimo Zou
    UPI: zzou702

    TO RUN: gcc -o ash ash.c
            ./ash


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#define MAX_CMD_INPUT 256
#define MAX_ARG_INPUT 16
#define HISTORY_NUMBER 10
#define PATH_MAX 50

// global variable declaration
char cmd[MAX_CMD_INPUT];       // storing the typed in command
char *argv[MAX_ARG_INPUT];     // argument to be passed into execvp()
char *argv1[MAX_ARG_INPUT];    // reserved for pipe
char *history[HISTORY_NUMBER]; // storing the history of the commands
int history_number_exec;       // 0 for false and 1 for true;
pid_t cpid;                    // child process id
int amper = 0;                 // background process
int piping = 0;                // pipe line
int job_number = 1;            // current job number to be assigned
int signal_tracker = 0;        // tracking the signal ctrl z, process in the signal handler should only be excuted once for every loop/command
pid_t fg_pid = 0;              // process id for the restarted foreground

// functions declaration
void start_shell();
void get_cmd();
void convert_cmd();
void execute();
int exit_if_inputted();
void change_directory();
void check_history();
void store_history();
void convert_history();
void pipe_execute();
void convert_pipe_command();
char get_job_status(int process_ID);
void print_current_job();
void add_jobs_linkedlist();
void print_finished_jobs();
void remove_finished_jobs();
void print_all_jobs();
void store_max_job_ID();
void signal_handler(int signum);
void foreground();
void background();
void kill_process();
void remove_specified_job(int job_pid);

// jobs linked list using struct
struct Job
{
    int job_id;
    int pid;
    char *job_command;
    struct Job *next;
};

// global linked list
struct Job *head_job;

int main(void)
{
    // initialising the global linked list for the head of the linked list
    head_job = malloc(sizeof(struct Job));
    head_job->job_id = 0;
    head_job->job_command = NULL;
    head_job->pid = 0;
    head_job->next = NULL;

    signal(SIGTSTP, signal_handler); // signal handler for ctrl + z

    // starting the shell(ash)
    start_shell();

    return 0;
}

void start_shell(void)
{

    // runs until user manually quits
    while (1)
    {

        print_finished_jobs();  // checking current job statuses, printing them out
        remove_finished_jobs(); // removing finished jobs
        store_max_job_ID();     // setting the job id to be assigned to the next job

        printf("ash> "); // printing the prompt

        // resetting the history number
        history_number_exec = 0;

        // resetting the cmd
        memset(cmd, '\0', MAX_CMD_INPUT);
        // resetting the signal tracker
        signal_tracker = 0;
        get_cmd(); // getting the command from the user

        // bypassing empty commands
        if (!strcmp(cmd, ""))
        {
            continue;
        }

        // putting the input command into arguments for execvp()
        convert_cmd();

        // exiting the shell
        if (exit_if_inputted())
        {
            break;
        }

        // resetting the signal tracker
        // signal_tracker = 0;

        // checking for the jobs built in command
        if (!strcasecmp(argv[0], "jobs"))
        {
            print_all_jobs();
            continue;
        }

        // fg, bg and kill
        if (!strcmp(argv[0], "fg"))
        {
            foreground();
            continue;
        }
        else if (!strcmp(argv[0], "bg"))
        {
            background();
            continue;
        }
        else if (!strcmp(argv[0], "kill"))
        {
            kill_process();
            continue;
        }

        // checking for the presence of "|", for the need of piping
        int j = 0;
        while (argv[j] != NULL)
        {
            if (!strcmp(argv[j], "|")) // "|" is present, there is a need for piping
            {
                piping = 1;
                break;
            }
            else
            {
                piping = 0;
            }
            j++;
        }

        // if piping required ("|" is present), convert the commands into 2
        if (piping == 1)
        {
            convert_pipe_command();
        }

        // check for the need of built in change directory commands
        if (!strcmp(argv[0], "cd"))
        {

            change_directory();
            store_history(); // storing the command inputted into history
            continue;
        }

        // checking for history built in command
        if (!strcmp(argv[0], "history") || !strcmp(argv[0], "h"))
        {

            check_history();
            continue;
        }

        store_history();

        // executing the command
        execute();

        // resetting the cpid
        cpid = 0;
    }
}

// taking the input command
void get_cmd(void)
{
    fgets(cmd, MAX_CMD_INPUT, stdin); // getting the user command from the input stream stdin

    // removing the new line character (trailing new line)
    if ((strlen(cmd) > 0) && (cmd[strlen(cmd) - 1] == '\n'))
    {
        cmd[strlen(cmd) - 1] = '\0';
    }
}

// converting the command inputted so it is in a format that can be executed
void convert_cmd(void)
{
    // emptying the arguments
    int j = 0;
    while (argv[j] != NULL)
    {
        argv[j] = NULL;
        j++;
    }

    // resetting the ampersand
    amper = 0;

    char *token; // token to go through the string

    token = strtok(strdup(cmd), " "); // separate by the space character

    int i = 0;

    // getting each section of the command separate by space and storing them
    while (token != NULL)
    {

        argv[i] = token;
        i++;

        token = strtok(NULL, " ");
    }

    // checking for ampersand for bg jobs
    if (!strcmp("&", argv[i - 1]))
    {
        amper = 1;
        argv[i - 1] = NULL;
    }
}

// ocnverting the pipe line command where "|" is present
void convert_pipe_command(void)
{
    char *buffer[MAX_ARG_INPUT]; // buffer to store the command

    // initialise everything in the buffer to NULL
    for (int i = 0; i < MAX_ARG_INPUT; i++)
    {
        buffer[i] = NULL;
    }

    int x = 0;

    // copying everything in the command into the buffer
    while (argv[x] != NULL)
    {
        buffer[x] = argv[x];
        x++;
    }

    // emptying the arguments argv1
    int j = 0;
    while (argv1[j] != NULL)
    {
        argv1[j] = NULL;
        j++;
    }

    j = 0;

    // emptying the arguments argv
    while (argv[j] != NULL)
    {
        argv[j] = NULL;
        j++;
    }

    // moving the commands before "|" into argv1, command 1
    int i = 0;
    while (strcmp(buffer[i], "|"))
    {
        argv1[i] = buffer[i];
        i++;
    }

    i++; // going past the index position of "|"

    // moving the commands after "|" into argv, command 2
    j = 0;
    while (buffer[i] != NULL)
    {
        argv[j] = buffer[i];
        i++;
        j++;
    }
}

// exiting the shell
int exit_if_inputted(void)
{
    // returning the value 1 to exit the program if "exit" has been entered by the user
    if (!strcasecmp("exit", argv[0]))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void change_directory(void)
{
    // change to home directory if no directory is inputted
    if (argv[1] == NULL)
    {
        chdir("/home");
    }
    else if ((!strcmp(argv[1], "..")) && (argv[2] == NULL))
    {                       // change to parent directory
        char cwd[PATH_MAX]; // current working directory
        char parent[200];   // parent directory

        // append /.. to the current directory to get the parent directory
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            snprintf(parent, sizeof(parent), "%s/..", cwd);
            chdir(parent); // change to parent directory
        }
        else
        { // error checking
            perror("error getting current working directory");
            exit(EXIT_FAILURE);
        }
    }
    else if (argv[2] == NULL)
    { // change to the directory inputted
        chdir(argv[1]);
    }
}

void check_history(void)
{
    if (argv[1] == NULL)
    { // if no number was inputted after the history command, only displaying the history
        store_history();

        // displaying all the history
        for (int i = 0; i < HISTORY_NUMBER; i++)
        {
            if (history[i] == NULL)
            {
                break;
            }
            printf("\t%d: %s\n", i + 1, history[i]);
        }
    }
    else
    { // if a number is inputted after the history command
        history_number_exec = 1;

        store_history();
        convert_history(); // converting the command in the history to a format that can be executed
        execute();
    }
}

// converting the command in the history to a format that can be executed
void convert_history(void)
{
    char *buffer = history[atoi(argv[1]) - 1]; // buffer storing the command that will be modified by the strtok()

    // emptying the arguments
    int j = 0;
    while (argv[j] != NULL)
    {
        argv[j] = NULL;
        j++;
    }

    // resetting the ampersand
    amper = 0;

    char *token;

    token = strtok(buffer, " "); // separating the command string by space

    int i = 0;

    // storing in an array that will be executed
    while (token != NULL)
    {
        argv[i] = token;
        i++;

        token = strtok(NULL, " ");
    }

    // checking for ampersand
    if (!strcmp("&", argv[i - 1]))
    {
        amper = 1;
        argv[i - 1] = NULL;
    }
}

// storing the command in history
void store_history(void)
{
    // checking if the command history followed by a number is put in
    if (history_number_exec == 0) // history or h is not followed by a nuber
    {
        // going through the 10 history store
        for (int i = 0; i < 10; i++)
        {
            // no more history is stored and the total history is not 10
            if (history[i] == NULL)
            {
                history[i] = strdup(cmd); // storing the command at that position
                break;
            }

            // all 10 slots for storing history has been assign to some command
            if ((i == 9) && (history[i] != NULL))
            {
                // shifting every command in the history array to the left by one, dumping the 1st/oldest history
                for (int i = 0; i < 9; i++)
                {
                    history[i] = strdup(history[i + 1]);
                }

                // sotring the history at the 10th place
                history[9] = strdup(cmd);
            }
        }
    }
    else if (history_number_exec == 1) // history followed by number is inputted
    {
        // going through all of history
        for (int i = 0; i < 10; i++)
        {
            // no more history is stored
            if (history[i] == NULL)
            {
                history[i] = strdup(history[atoi(argv[1]) - 1]); // storing the command executed at the history number
                break;
            }

            // all 10 slots are used to store some command
            if ((i == 9) && (history[i] != NULL))
            {
                // shifting to left by one
                for (int i = 0; i < 9; i++)
                {
                    history[i] = strdup(history[i + 1]);
                }

                history[9] = history[atoi(argv[1])]; // storing the command executed at the history number
            }
        }
    }
}

// adding jobs to the linked list
void add_jobs_linkedlist(void)
{
    struct Job *new_job = malloc(sizeof(struct Job)); // initialising the new job

    new_job->job_id = job_number;       // current job id is allocated
    new_job->job_command = strdup(cmd); // storing the command of the job
    new_job->pid = cpid;                // the id of the child process
    new_job->next = head_job;           // pointing to the current head of the linked list

    head_job = new_job; // changing the new job to be the head of the linked list

    job_number++; // incrementing the current job number

    struct Job *node = head_job;
}

// get the status of the job identifed by its process id
char get_job_status(int process_ID)
{

    int status;
    char pidtext[10];
    char procfilename[100];
    FILE *procfile;

    struct Job *node = head_job;

    // go through the linked list
    while (node)
    {
        // the job we are trying to locate in the linked list
        if ((node->pid == process_ID) && (node->pid != 0))
        {
            // using the example taught in tutorial
            sprintf(pidtext, "%d", node->pid); // appending the process id
            strcpy(procfilename, "/proc/");    // the file where we get the status of the process
            strcat(procfilename, pidtext);     // appedning the process id to the file name
            strcat(procfilename, "/stat");

            procfile = fopen(procfilename, "r"); // reading the file

            // error checking
            if (procfile == NULL)
            {
                perror("Failed to open file.");
                exit(EXIT_FAILURE);
            }

            // going through the file until we reach ")" character, where the char following is the status
            do
            {
                status = fgetc(procfile);
            } while (status != ')');

            fgetc(procfile);
            status = fgetc(procfile); // getting the next character

            fclose(procfile); // closing the file
            return status;
        }

        node = node->next; // incrementing through the linked list
    }

    // if status is not found
    perror("Could not obtain the status of the job");
    exit(EXIT_FAILURE);
}

// printing the current job, head of the linked list
void print_current_job(void)
{
    printf("[%d]\t%d\n", head_job->job_id, head_job->pid);
}

// printing the finished jobs
void print_finished_jobs(void)
{
    // go through the linked list
    struct Job *node = head_job;
    while (node)
    {
        // until we reach teh last element of the linked list
        if (node->pid != 0)
        {
            // if the job status is Z which is zombie / done
            if (get_job_status(node->pid) == 'Z')
            {
                printf("[%d]\t<Done>\t%s\n", node->job_id, node->job_command);
            }
        }
        node = node->next; // incrementing
    }
}

// removing the finished jobs from the linked list
void remove_finished_jobs(void)
{
    struct Job *node = head_job;

    // if head node is to be deleted
    while (node)
    {
        // head node is not the last element in the linked list
        if (head_job->pid != 0)
        {
            //  head job is finished
            if (get_job_status(node->pid) == 'Z')
            {
                head_job = node->next; // setting the head of the linked list to the next job
                node = head_job;       // incrementing the current node
            }
            else // head job is not finished
            {
                break;
            }
        }
        else // head node is the last element in the linked lsit
        {
            break;
        }
    }

    // current node is the last element in the linked list where all of its attributes are NULL
    if (node->next == NULL)
    {
        return;
    }

    // temp node to be the next node in the linked list from the current node
    struct Job *temp = node->next;

    // going through the linked list
    while (node)
    {
        // if the next element is not the last element of the linked list
        if (temp->pid != 0)
        {
            // if the next node / job is finished
            if (get_job_status(temp->pid) == 'Z')
            {
                // connect the current node to the next next node, removing the next node from the linked list
                node->next = temp->next;
                temp = node->next;
                continue;
            }
            else // if the next job is not finished
            {
                temp = temp->next; // increment the next node
            }
        }
        node = node->next; // increment the current node
    }
}

void remove_specified_job(int job_pid)
{
    struct Job *node = head_job;
    struct Job *temp = node->next;

    // if head job is to be removed
    if (head_job->pid == job_pid)
    {
        head_job = head_job->next;
        return;
    }

    // going through the linked list
    while (node)
    {
        // if the next element is not the last element of the linked list
        if (temp->pid != 0)
        {
            // if the next node is the node we want to remove
            if (temp->pid == job_pid)
            {
                // connect the current node to the next next node, removing the next node from the linked list
                node->next = temp->next;
                temp = node->next;
                continue;
            }
            else // if the job is not found
            {
                temp = temp->next; // increment the next node
            }
        }
        node = node->next; // increment the current node
    }
}

// printing all the jobs
void print_all_jobs(void)
{
    struct Job *node = head_job;

    // go through the linked list
    while (node)
    {
        if (node->pid != 0) // false if there is no current jobs
        {
            if (get_job_status(node->pid) == 'Z') // zombie / finished
            {
                printf("[%d]\t<Done>\t%s\n", node->job_id, node->job_command);
            }
            else if (get_job_status(node->pid) == 'R') // running / runnable
            {
                printf("[%d]\t<Running>\t%s\n", node->job_id, node->job_command);
            }
            else if (get_job_status(node->pid) == 'S') // sleeping
            {
                printf("[%d]\t<Sleeping>\t%s\n", node->job_id, node->job_command);
            }
            else if (get_job_status(node->pid) == 'D') // waiting
            {
                printf("[%d]\t<Waiting>\t%s\n", node->job_id, node->job_command);
            }
            else if (get_job_status(node->pid) == 'T') // stopped
            {
                printf("[%d]\t<Stopped>\t%s\n", node->job_id, node->job_command);
            }
        }
        node = node->next; // increment
    }
}

// storing the job ID to be allocated to the next job in the global variable job_number
void store_max_job_ID(void)
{
    struct Job *node = head_job;

    int max_jid = 0; // temporary storage of the max job ID in the linked list

    // if there is no current jobs in the linked list i.e. all values in head_job is NULL
    if (head_job->pid == 0)
    {
        job_number = 1; // next job would be assign a value of 1
        return;
    }

    // going through the linked list
    while (node)
    {
        if (node->pid != 0)
        {
            // storing the biggest job ID in the linked list
            if (node->job_id > max_jid)
            {
                max_jid = node->job_id;
            }
        }
        node = node->next; // increment
    }

    // the next job would be allocated an ID that is one bigger than the current biggest job ID
    job_number = max_jid + 1;
}

// executin the command inputted
void execute(void)
{
    // child process 1
    cpid = fork();

    // error checking
    if (cpid == -1)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    // child process 1
    if (cpid == 0)
    {
        //"|" has been inputted for the pipeline, if no need for pipeline, execute the command inputted
        if (piping == 1)
        {
            // pipe
            int pipefd[2];
            pid_t pipe_pid;

            // error checking
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // child process 2 (pipe_pid) for the pipeline
            pipe_pid = fork();

            // error checking
            if (pipe_pid == -1)
            {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            // child process 2
            if (pipe_pid == 0)
            {
                // closing the standard output stream for command 1, directing the output of command 1 to be the input of command 2
                close(STDOUT_FILENO);
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
                close(pipefd[0]);

                // executing the command 1 (command before "|")
                execvp(argv1[0], argv1);
            }
            else
            {
                // closing the standard input stream for command 2, directing the input of command 2 to be the output of command 2
                close(STDIN_FILENO);
                dup2(pipefd[0], STDIN_FILENO);
                close(pipefd[0]);
                close(pipefd[1]);
            }
        }

        execvp(argv[0], argv); // executing the command, or command 2 if piping is required
    }
    else // parent process
    {

        // if the command is to be executed in the foregroun, wait for the child process
        if (amper == 0)
        {
            waitpid(cpid, NULL, WUNTRACED);
        }
        else // background process, not waiting for the child process
        {
            // background jobs to be added to the linked list
            add_jobs_linkedlist();
            print_current_job(); // printing the current job to the prompt
        }
    }
}

void signal_handler(int signum)
{
    // killing the child process if ctrl + z is entered
    if ((signum == SIGTSTP)) // ctrl z is pressed for the first time for this command
    {
        if (cpid != 0)
        {
            kill(cpid, SIGSTOP); // stopping the child process

            // adding the stopped commands to the jobs linked list
            if (strcasecmp(cmd, "jobs") && strcasecmp(cmd, ""))
            {
                add_jobs_linkedlist();
            }

            sleep(0.3);
        }
        else if (fg_pid != 0)
        {                          // stopping a restarted job
            kill(fg_pid, SIGSTOP); // killing the restarted foreground process
            fg_pid = 0;            // resetting the foreground process id since it has been stopped
        }
    }
}

// to start a job in the foreground i.e. terminal
void foreground(void)
{
    int jnum; // job id or job number
    struct Job *node = head_job;

    // fg inputted without and number following, continue the last stopped job
    if (argv[1] == NULL)
    {
        while (node)
        {
            if (node->pid != 0)
            {
                // last stopped jobs
                if (get_job_status(node->pid) == 'T')
                {
                    kill(node->pid, SIGCONT);
                    fg_pid = node->pid;

                    int status;
                    pid_t return_pid = waitpid(node->pid, &status, WNOHANG);

                    // inspiration taken from tutorial 3 waitpid
                    do
                    {
                        return_pid = waitpid(cpid, &status, WUNTRACED | WCONTINUED);
                        if (return_pid == -1)
                        {
                            perror("waitpid");
                            exit(EXIT_FAILURE);
                        }

                        if (WIFEXITED(status))
                        {
                            remove_specified_job(node->pid); // removing the job from the jobs list since it is now in the foreground
                        }
                        else if (WIFSTOPPED(status))
                        {
                            break;
                            ;
                        }
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

                    return; // only starting the last stopped job
                }
            }
            node = node->next;
        }
    }
    else
    { // fg with specified job
        jnum = atoi(argv[1]);

        while (node)
        {
            if (node->pid != 0)
            {
                if ((node->job_id == jnum) && (get_job_status(node->pid) == 'T'))
                {

                    kill(node->pid, SIGCONT);
                    fg_pid = node->pid;

                    int status;
                    pid_t return_pid = waitpid(node->pid, &status, WNOHANG);

                    // inspiration taken from tutorial 3 waitpid
                    do
                    {
                        return_pid = waitpid(cpid, &status, WUNTRACED | WCONTINUED);

                        // error checking
                        if (return_pid == -1)
                        {
                            perror("waitpid");
                            exit(EXIT_FAILURE);
                        }

                        // if the program has exited
                        if (WIFEXITED(status))
                        {
                            remove_specified_job(node->pid); // removing the job from the jobs list since it is now in the foreground
                        }
                        else if (WIFSTOPPED(status)) // if the program has been stopped
                        {
                            break;
                            ;
                        }
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

                    return;
                }
            }
            node = node->next;
        }
    }
}

void background(void)
{
    int jnum; // job id or job number
    struct Job *node = head_job;

    // bg inputted without and number following, continue the last stopped job
    if (argv[1] == NULL)
    {
        while (node)
        {
            if (node->pid != 0)
            {
                // last stopped jobs
                if (get_job_status(node->pid) == 'T')
                {
                    kill(node->pid, SIGCONT); //starting the process without waiting

                    break; // only starting the last stopped job
                }
            }
            node = node->next;
        }
    }
    else
    { // bg with specified job
        jnum = atoi(argv[1]);

        while (node)
        {
            if (node->pid != 0)
            {
                if ((node->job_id == jnum) && (get_job_status(node->pid) == 'T'))
                {
                    kill(node->pid, SIGCONT);

                    return;
                }
            }
            node = node->next;
        }
    }
}

void kill_process(void)
{
    int jnum; // job id or job number
    struct Job *node = head_job;

    // kill inputted without and number following, continue the last stopped job
    if (argv[1] == NULL)
    {
        while (node)
        {
            if (node->pid != 0)
            {
                // last stopped jobs
                if (get_job_status(node->pid) == 'T')
                {
                    kill(node->pid, SIGKILL);
                    remove_specified_job(node->pid); // removing the job from the jobs list since it is now killed

                    break; // only starting the last stopped job
                }
            }
            node = node->next;
        }
    }
    else
    { // kill with specified job
        jnum = atoi(argv[1]);

        while (node)
        {
            if (node->pid != 0)
            {
                if ((node->job_id == jnum) && (get_job_status(node->pid) == 'T'))
                {
                    kill(node->pid, SIGKILL);
                    remove_specified_job(node->pid); // removing the job from the jobs list since it is now killed

                    return;
                }
            }
            node = node->next;
        }
    }
}
