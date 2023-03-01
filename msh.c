// The MIT License (MIT)
//
// Copyright (c) 2023 Ikechukwu Ofili
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens
                           // so we need to define what delimits our tokens.
                           // In this case  white space
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 10 // Mav shell only supports four arguments

#define HISTORY_SIZE 15

char *token[MAX_NUM_ARGUMENTS + 1];

// Points to the most recent command in history. Starts off as -1
int hist_ptr = -1;

struct command
{
    char *cmd;
    pid_t pid;
};

// Zero-initialized
struct command history[HISTORY_SIZE] = {0};

void free_array(char **arr, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (arr[i] != NULL)
        {
            free(arr[i]);
        }
    }
}

/* Parse input*/
void parse_tokens(const char *command_string)
{
    // Clean up the old values in token
    free_array(token, MAX_NUM_ARGUMENTS);

    int token_count = 0;
    // Pointer to point to the token parsed by strsep
    char *argument_ptr = NULL;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;

    // Tokenize the input strings with whitespace used as the delimiter
    while (((argument_ptr = strsep(&working_string, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
        token[token_count] = strndup(argument_ptr, MAX_COMMAND_SIZE);
        if (strlen(token[token_count]) == 0)
        {
            free(token[token_count]);
            token[token_count] = NULL;
        }
        token_count++;
    }

    // Set all args from the last arg parsed to the end of the token array
    // This helps so that I don't get a double free error if I run a cmd that
    // has 7 args then run one that has 3 args then called free_array on token
    for (; token_count < MAX_NUM_ARGUMENTS; ++token_count)
    {
        token[token_count] = NULL;
    }    

    free(head_ptr);
}

// Returns the pid of the child process that was spawned
pid_t run_external()
{
    pid_t pid = fork();

    if (pid == -1)
    {
        // We can't really do anything but exit
        perror("fork: fatal error");
        exit(EXIT_FAILURE);
    }
    if (pid == 0)
    {
        execvp(token[0], token);

        /* Execution will get here only if failed */

        // If command not found, print that, else print the specific error
        if (errno == ENOENT)
            fprintf(stderr, "%s: Command not found.\n", token[0]);
        else
            perror(token[0]);

        // If you don't exit you will basically have two shells now lol
        exit(EXIT_FAILURE);
    }
    else
    {
        // You can check the status of the child here for some fancy stuff
        int status;
        waitpid(pid, &status, 0);
    }

    //return the pid
    return pid;
}

void print_history(bool showpid)
{
    // Choose to showpid or not
    if (showpid)
    {
        // If the pointer is at the last element in the list or the list
        // isn't full yet, print the history from the first element to the
        // current one pointed by `hist_ptr`
        if (hist_ptr == HISTORY_SIZE - 1 || history[hist_ptr + 1].cmd == NULL)
        {
            for (int i = 0; i <= hist_ptr; ++i)
                printf("[%2d] [%d] %s", i, history[i].pid, history[i].cmd);
        }
        // else, we print the history from the current one, then loop around
        // the list
        else
        {
            for (int i = hist_ptr + 1, j = 0; j < HISTORY_SIZE; ++i, ++j)
            {
                if (i == HISTORY_SIZE)
                    i = 0;
                printf("[%2d] [%d] %s", j, history[i].pid, history[i].cmd);
            }
        }
    }
    else
    {
        // Same logic as above
        if (hist_ptr == HISTORY_SIZE - 1 || history[hist_ptr + 1].cmd == NULL)
        {
            for (int i = 0; i <= hist_ptr; ++i)
                printf("[%2d] %s", i, history[i].cmd);
        }
        else
        {
            for (int i = hist_ptr + 1, j = 0; j < HISTORY_SIZE; ++i, ++j)
            {
                if (i == HISTORY_SIZE)
                    i = 0;
                printf("[%2d] %s", j, history[i].cmd);
            }
        }
    }
}

void run_command_string(char *command_string)
{
    char *cmd = token[0];

    if (*cmd == '!')
        if (*(cmd + 1) == '!')
        {
            if (hist_ptr == -1)
                puts("Command not in history");
            else
            {
                // Run the most recent command
                parse_tokens(history[hist_ptr].cmd);
                run_command_string(history[hist_ptr].cmd);
            }
        }
        else
        {
            char *endp;
            unsigned long hist = strtoul(cmd + 1, &endp, 10);

            // If something that is not a digit is found, or `n` is out of
            // bounds, that is an invalid command and we report this
            if (*endp != '\0' || hist >= HISTORY_SIZE)
            {
                fprintf(stderr, "Invalid reference: %s\n", cmd + 1);
                return;
            }

            /* Do some math to calculate the real index */

            // hist_ptr always points to the end of the list so hist_ptr + 1 is
            // actually the first command in history. Offset that number by
            // hist and we got our real history index.
            // **NOTE** we don't need to do that if hist_ptr points to the
            // bottom of the history array or the history array has less than
            // HISTORY_SIZE elements
            if (hist_ptr != HISTORY_SIZE - 1 && history[hist_ptr + 1].cmd != NULL)
            {
                printf("here!!\n");
                hist = hist + hist_ptr + 1;

                if (hist >= HISTORY_SIZE)
                    hist -= HISTORY_SIZE;
            }
            
            if (history[hist].cmd == NULL)
                fputs("Command not in history\n", stderr);
            else
            {
                // Run the command at the specified history number
                parse_tokens(history[hist].cmd);
                run_command_string(history[hist].cmd);
            }
        }
    else
    {
        /* Add command to the history if it is not a `!` command */

        hist_ptr += 1;
        if (hist_ptr == HISTORY_SIZE)
            hist_ptr = 0;

        // Save the previous cmd so we can delete it later. We can't delete it
        // now because it might be the same string pointed to by command_string
        char *prev = history[hist_ptr].cmd;

        history[hist_ptr].cmd = strdup(command_string);

        // Do this in case history needs to see its own pid
        history[hist_ptr].pid = -1;

        // We have wrapped around so we free the history that used to be here
        if (prev != NULL)
            free(prev);

        if (!strcmp(cmd, "history"))
        {
            // Print the history, pass if you want to show the pids or not
            print_history(token[1] != NULL && !strcmp(token[1], "-p"));
        }
        else if (!strcmp(cmd, "cd"))
        {
            if (token[2] != NULL)
            {
                fprintf(stderr, "Too many args for cd command\n");
                return;
            }

            char *dir = token[1];
            if (dir == NULL)
            {
                // Try to cd to the user's home directory. We do this through
                // the environment variable "HOME"
                // There is a better way to get home directory but if some major
                // shells use this, who am I to not do the same
                dir = getenv("HOME");
            }

            int err = chdir(dir);
            if (err == -1)
                fprintf(stderr, "cd: %s\n", strerror(errno));
        }
        else
        {
            history[hist_ptr].pid = run_external();
        }
    }
}

int main()
{
    char *command_string = (char *)malloc(MAX_COMMAND_SIZE);

    while (1)
    {
        // Print out the msh prompt
        printf("msh> ");

        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while (!fgets(command_string, MAX_COMMAND_SIZE, stdin))
            ;

        // Ignore blank lines
        if (*command_string == '\n')
        {
            continue;
        }

        parse_tokens(command_string);

        const char *cmd = token[0];

        // Quit if command is 'quit' or 'exit'
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
        {
            break;
        }

        // If line is not blank, parse it, and run the parsed command
        run_command_string(command_string);
    }

    free(command_string);
    free_array(token, MAX_NUM_ARGUMENTS);

    for (uint i = 0; i < HISTORY_SIZE; ++i)
    {
        if (history[i].cmd != NULL)
            free(history[i].cmd);
    }

    return 0;
}