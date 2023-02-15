// The MIT License (MIT)
//
// Copyright (c) 2016 Trevor Bakker
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

#define MAX_NUM_ARGUMENTS 5 // Mav shell only supports four arguments

char *token[MAX_NUM_ARGUMENTS + 1];

#define HISTORY_SIZE 15
char *history[HISTORY_SIZE] = {NULL};

// Points to the most recent command. Starts off as -1
int hist_ptr = -1;

void free_array(char **arr, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (arr[i] != NULL)
        {
            free(arr[i]);
            arr[i] = NULL;
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

    free(head_ptr);
}

void run_external()
{
}

void run_command_string(char *command_string)
{
    const char *cmd = token[0];

    if (*cmd == '!')
    {
        if (cmd[1] == '!')
        {
            if (hist_ptr == -1)
            {
                puts("Command not in history");
            }
            else
            {
                // This parses the history line into the global variable `token`
                parse_tokens(history[hist_ptr]);

                run_command_string(history[hist_ptr]);
            }
        }
        else
        {
            char *endp;
            unsigned long hist = strtoul(cmd + 1, &endp, 10);

            // If something that is not a digit is found, or `n` is out of
            // bounds, or there isn't any history there yet,
            // That is an invalid command
            if (*endp != '\0' || hist >= HISTORY_SIZE || history[hist] == NULL)
            {
                puts("Command not in history");
            }
            else
            {
                // This parses the history line into the global variable `token`
                parse_tokens(history[hist]);

                run_command_string(history[hist]);
            }
        }
    }
    else
    {
        // Add command to the history
        hist_ptr += 1;
        if (hist_ptr == HISTORY_SIZE)
        {
            hist_ptr = 0;
        }

        if (history[hist_ptr] != NULL)
        {
            free(history[hist_ptr]);
        }
        history[hist_ptr] = strdup(command_string);

        if (!strcmp(cmd, "history"))
        {
            int showpids = 0;
            if (token[1] != NULL && !strcmp(token[1], "-p"))
            {
                showpids = 1;
            }

            // \todo: add showpid functionality

            // If the pointer is at the last element in the list or the list
            // isn't full yet, print the history from the first element to the
            // current one pointed by `hist_ptr`
            if (hist_ptr == HISTORY_SIZE - 1 || history[hist_ptr + 1] == NULL)
            {
                for (int i = 0; i <= hist_ptr; ++i)
                {
                    printf("%d: %s", i, history[i]);
                }
            }
            // else, we print the history from the current one, then loop around
            // the list
            else
            {
                for (int i = hist_ptr + 1, j = 0; j < HISTORY_SIZE; ++i, ++j)
                {
                    if (i == HISTORY_SIZE)
                    {
                        i = 0;
                    }
                    printf("%d: %s", j, history[i]);
                }
            }
        }
        else if (!strcmp(cmd, "cd"))
        {
            if (token[2] != NULL)
            {
                fprintf(stderr, "Too many args for cd command\n");
                return;
            }

            if (token[1] == NULL)
            {
                fprintf(stderr, "Too few args for cd command\n");
                return;
            }

            int err = chdir(token[1]);

            if (err == -1)
            {
                fprintf(stderr, "%s: %s\n", cmd, strerror(errno));
            }
        }
        else
        {
            run_external();
        }
    }
}

int main()
{
    // This will never not be true. execvp needs the last element to be NULL
    token[MAX_NUM_ARGUMENTS] = NULL;

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

        /* Parse input */
        parse_tokens(command_string);

        const char *cmd = token[0];

        if (!strcmp(cmd, "quit") || !strcmp(cmd, "exit"))
        {
            break;
        }

        run_command_string(command_string);
    }

    free(command_string);

    free_array(token, MAX_NUM_ARGUMENTS);
    free_array(history, HISTORY_SIZE);

    return 0;
}
