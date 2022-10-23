/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

typedef struct __Process {
    int pid;
	char cmd[50];
	struct __Process* next;
} Process;

void insert_item(pid_t, char**, Process**);
void delete_item(pid_t, Process**);
void execute_shell_command(struct cmdline*, Process**);
int jobs(char*, Process**);

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);
	
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

		// head of the linked list
		Process* head = NULL;

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

		// execute the program the user asked for in the cmd
		execute_shell_command(l, &head);

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}

	}

}


void execute_shell_command(struct cmdline *line, Process **head) {
	// getting the list of commands
	char ***cmd = line->seq;
	// process id definition
	pid_t pid;
	// bg variable to decide whether we wait the child process or not
	int bg = line->bg;
	int status;
	
	int in;
	// the pipe array
	// pipes are producers consumer style
	int pipe_data[2];
	


	if(!jobs(cmd[0][0], head)) {

		for(int i=0; cmd[i]!=0; i++) {
			if(pipe(pipe_data) == -1)
				perror("Error in pipe");

			// make the fork
			pid = fork();

			switch (pid)
			{
			case -1:
				// in case of erros
				perror("Error in fork");	
				break;
			case 0:
				close(pipe_data[0]);

				// if it is not the last command
				dup2(in, 0);
				if(cmd[i+1]) {
					dup2(pipe_data[1], 1);
				}


				// in case of being the child
				// execute the command
				execvp(cmd[i][0], cmd[i]);

				// if the program reaches this point, it is sign of an error
				// so we exit indicating the failure
				exit(EXIT_FAILURE);

				break;
			default:
				close(pipe_data[1]);
				in = pipe_data[0];

				if(bg) {
					// execute in the background
					insert_item(pid, cmd[i], head);
				}
				else {
					// wait for child process
					if(!waitpid(pid, &status, WUNTRACED))
						perror("Error in waitpid");
				}
				break;
			}
		}


	}

}

int jobs(char* cmd, Process**head) {

	// if the command is = jobs
	if(!strcmp(cmd, "jobs")) {
		Process* it;

		// iterate over the linked list
		for(it=*head;it!=NULL; it=it->next) {
		
			if(waitpid(it->pid, NULL, WNOHANG)!=0) {
				delete_item(it->pid, head);
			}
			else {
				printf("PID : %d => command : %s\n", it->pid, it->cmd);
			}
		}
		return 1;
	}
	return 0;
}


void insert_item(pid_t pid, char** cmd, Process**head) {

	Process* new_malloc = (Process*)malloc(sizeof(Process));
	char aux[50] = "";

	if(*head == NULL) {
		// if the head is NULL
		*head = new_malloc;
	}
	else {
		// if the list is already filled with something	
		Process *it;
		for(it = *head; it->next!=NULL; it = it->next);
		it->next = new_malloc;
	}

	new_malloc->pid = pid;
	new_malloc->next = NULL;
	// string manipulation
	for(int i=0; cmd[i]!=0; i++) {
		strcat(aux, cmd[i]);
		strcat(aux, " ");
	}
	aux[strlen(aux)-1] = '\0';
	strcpy(new_malloc->cmd, aux);

}


void delete_item(pid_t pid, Process**ptr_head) {
	Process *head = *ptr_head;
	//start from the first link
	Process* current = head;
	Process* previous = NULL;

	//navigate through list
	while(current->pid != pid && current->next != NULL) {
		//store reference to current link
		previous = current;
		//move to next link
		current = current->next;
	}

	if(current->pid == pid) {
		//found a match, update the link
		if(current == head) {
			//change first to point to next link
			head = head->next;
		} else {
			//bypass the current link
			previous->next = current->next;
			free(current);
		}
	}

}