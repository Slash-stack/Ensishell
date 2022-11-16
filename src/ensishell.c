/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libguile.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <fcntl.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

// this section has been added by zerhounb & notariob

typedef struct jobs_list jobs_list;

struct jobs_list {
    pid_t pid;
    char * cmd;
    time_t begin_process;
    jobs_list * next;
};

jobs_list *process_list = NULL;

int fildes[2];

// end of edited section

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1


void executer(char **cmd, int bg, bool pipe_in, bool pipe_out, char *file_in, char *file_out) {

    // modified by zerhounb & notariob

    pid_t pid = fork();

    /* Erreur lors du fork */
    if (pid == -1) {
        perror("fork: Process Failed !\n");
        return;
    } else if (pid == 0) {

        // gestion des pipe
        if (pipe_in) {
            dup2(fildes[0], STDIN_FILENO);
        }
        if (pipe_out) {
            dup2(fildes[1], STDOUT_FILENO);
        }

        // redirection dans des fichiers sans pipe
        if (file_in && !pipe_out && !pipe_in) {
            int input = open(file_in, O_RDONLY);
            dup2(input, STDIN_FILENO);
            close(input);
        }
        if (file_out && !pipe_out && !pipe_in) {
            int output = open(file_out, O_WRONLY | O_TRUNC | O_CREAT, 0666);
            dup2(output, STDOUT_FILENO);
            close(output);
        }
        // redirection dans des fichiers avec pipe
        if (file_in && pipe_out) {
            int input = open(file_in, O_RDONLY);
            dup2(input, STDIN_FILENO);
            close(input);
        }
        if (file_out && pipe_in) {
            int output = open(file_out, O_WRONLY | O_TRUNC | O_CREAT, 0666);
            dup2(output, STDOUT_FILENO);
            close(output);
        }

        execvp(cmd[0], cmd);
        return;
    }

    close(fildes[1]);

    if (pipe_out) return;

    if (!bg) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        struct timeval begin;
        // il faut ajouter le nouveau processus dans la liste chainée des processus
        // si c'est le premier processus
        if (process_list == NULL) {
            gettimeofday(&begin, NULL);
            process_list = malloc(sizeof(jobs_list));
            process_list->next = NULL;
            process_list->pid = pid;
            process_list->cmd = malloc(strlen(cmd[0]) * sizeof(char));
            process_list->begin_process = begin.tv_sec;
            strcpy(process_list->cmd, cmd[0]);
        } else {
            gettimeofday(&begin, NULL);
            jobs_list *current = process_list;
            // on se positionne à la fin de la liste chaînée
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = malloc(sizeof(jobs_list));
            current->next->next = NULL;
            process_list->begin_process = begin.tv_sec;
            current->next->pid = pid;
            current->next->cmd = malloc(strlen(cmd[0]) * sizeof(char));
            strcpy(current->next->cmd, cmd[0]);
        }
    }
}

void jobs() {

    // modified by zerhounb & notariob

    jobs_list *current = process_list;
    jobs_list *previous = NULL;
    // on parcours la liste des process
    while (current != NULL) {
        int status;
        // si le process est en cours, il faut l'afficher
        if (!waitpid(current->pid, &status, WNOHANG)) {
            printf("[%d] : %s\n", current->pid, current->cmd);
        } else {
            // il faut enlever les process finis
            if (previous == NULL) {
                process_list = current->next;
                //free(current);
            } else {
                previous->next = current->next;
                //free(current);
            }
        }
        previous = current;
        current = current->next;
    }
}

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	//printf("Not implemented yet: can not execute %s\n", line);

    struct cmdline *l = parsecmd(&line);
    int i, j;

    if (l->err) {
        /* Syntax error, read another command */
        printf("error: %s\n", l->err);
    }

    if (l->in) printf("in: %s\n", l->in);
    if (l->out) printf("out: %s\n", l->out);
    if (l->bg) printf("background (&)\n");

    pipe(fildes);
    /* Display each command of the pipe */
    for (i=0; l->seq[i]!=0; i++) {
        char **cmd = l->seq[i];
        printf("seq[%d]: ", i);
        for (j=0; cmd[j]!=0; j++) {
            printf("'%s' ", cmd[j]);
        }
        printf("\n");
        if (strcmp(cmd[0], "jobs") == 0) {
            jobs();
        } else {
            executer(cmd,
                     l->bg,
                     i > 0,
                     l->seq[i+1] != 0,
                     l->in,
                     l->out);
        }
    }

	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

        // place the two file descriptors in filedes
        pipe(fildes);

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                       for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
            if (strcmp(cmd[0], "jobs") == 0) {
                jobs();
            } else {
                executer(cmd,
                         l->bg,
                         i > 0,
                         l->seq[i+1] != 0,
                         l->in,
                         l->out);
            }
		}
	}

}
