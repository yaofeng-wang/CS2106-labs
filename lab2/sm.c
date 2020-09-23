/**
 * CS2106 AY 20/21 Semester 1 - Lab 2
 *
 * This file contains function definitions. Your implementation should go in
 * this file.
 */

#define WRITE_END 1
#define READ_END 0

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#include "sm.h"

typedef struct sm_pid {
  int num_pids;
  int * pids;
} sm_pid_t;


int last_service_id;
sm_status_t* records[SM_MAX_SERVICES];
sm_pid_t * pid_records[SM_MAX_SERVICES];

// Use this function to any initialisation if you need to.
void sm_init(void) {
	last_service_id = -1;

	for (int i = 0; i < SM_MAX_SERVICES; i++) {
		records[i] = NULL;
		pid_records[i] = NULL;
	}
}

// Use this function to do any cleanup of resources.
void sm_free(void) {

	for (int i = 0; i <= last_service_id; i++) {
		if (records[i]) {
			free((void *)records[i]->path);
			free(records[i]);
		}
		if (pid_records[i]) {
			free(pid_records[i]->pids);
			free(pid_records[i]);
		} 
	}
}

// Exercise 1a/2: start services
void sm_start(const char *processes[]) {
	last_service_id ++;
	// start /bin/echo hello | /bin/cat | /bin/sha256sum
	// start /bin/echo hello | /bin/cat 
	// start /bin/echo hello | /bin/sha256sum
	// start /bin/echo hello | /bin/diff test.c -
	// start /bin/echo hello 

	// get total num of processes in this service
	int tot_pro = 1; // assume there will at least be 1 process in the service
	int offset = 0;
	while (true) {
		// move offset to the start of the next process
		while (processes[offset]) {
			offset += 1;
		}
		if (processes[offset+1]) {
			offset += 1;
			tot_pro += 1;
		} 
		else {
			break;
		}
	}
	// printf("##(%d) %d\n", getpid(), tot_pro);


	offset = 0;
	int pipefd_old[2];
	int pipefd_new[2];

	int cpid;

	for (int cur_pro = 1; cur_pro <= tot_pro; cur_pro++) {
		// printf("%d: ", offset);
		// printf("%s\n", processes[offset]);
		
		if (pipe(pipefd_new) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (cpid == 0) {
			
			// first process
			if (cur_pro == 1) {
				// printf("(%d)first\n", getpid());
				// printf("(%d)process %d:\n", getpid(), cur_pro);
				// printf("(%d)%s\n", getpid(), processes[offset]);

				fclose(stdin);

				// contains more than 1 process
				if (cur_pro < tot_pro) {

					close(pipefd_new[READ_END]);

					if (dup2(pipefd_new[WRITE_END], STDOUT_FILENO) == -1) {
						perror("dup2");
						exit(EXIT_FAILURE);
					}	
				}
				execv(processes[0], (char * const*) processes);
				printf("execv\n");
				exit(EXIT_FAILURE);
			}
			// last process
			else if (cur_pro == tot_pro) {
				// printf("(%d)last\n", getpid());
				// printf("(%d)process %d:\n", getpid() ,cur_pro);
				// printf("(%d)%s\n", getpid(), processes[offset]);

				close(pipefd_old[WRITE_END]);
				close(pipefd_new[READ_END]);
				close(pipefd_new[WRITE_END]);

				if (dup2(pipefd_old[READ_END], STDIN_FILENO) == -1) {
					perror("dup2");
					exit(EXIT_FAILURE);
				}
				
				int num_args = 2;
				

				while (processes[offset+num_args-1] != NULL) {
					num_args++;
				}
				
				char * args[num_args];
				for (int i = 0; i < num_args; i++) {
					args[i] = (char * const) processes[offset+i];
				}

				execv(processes[offset], args);
				perror("execv\n");
				exit(EXIT_FAILURE);
			}
			//non-terminal process
			else {
				// printf("(%d)non-terminal\n", getpid());
				// printf("(%d)process %d:\n", getpid(), cur_pro);
				// printf("(%d)%s\n", getpid(), processes[offset]);
			

				close(pipefd_old[WRITE_END]);
				close(pipefd_new[READ_END]);

				if (dup2(pipefd_old[READ_END], STDIN_FILENO) == -1) {
					perror("dup");
					exit(EXIT_FAILURE);
				}
				
				if (dup2(pipefd_new[WRITE_END], STDOUT_FILENO) == -1) {
					perror("dup");
					exit(EXIT_FAILURE);
				}
		
				int num_args = 2;
				while (processes[offset+num_args-1] != NULL) {
					num_args++;
				}
				char * args[num_args];
				for (int i = 0; i < num_args; i++) {
					args[i] = (char * const) processes[offset+i];
				}

				execv(processes[offset], args);
				perror("execv\n");
				exit(EXIT_FAILURE);
			}
		}
		else {
			close(pipefd_new[WRITE_END]); 
			// close(pipefd_new[READ_END]);

			pipefd_old[READ_END] = pipefd_new[READ_END];
			pipefd_old[WRITE_END] = pipefd_new[WRITE_END];

			// update service information using the latest processes
			sm_status_t* record = (sm_status_t*) malloc(sizeof(sm_status_t));
			record->pid = cpid;
			char* memory = (char*)malloc(sizeof(char)*1+strlen(processes[offset]));
			strcpy(memory, processes[offset]);
			record->path = memory;
			record->running = true;

			if (records[last_service_id]) {
				free((void *)records[last_service_id]->path);
				free(records[last_service_id]);
				records[last_service_id] = NULL;
			}
			records[last_service_id] = record;

			// update pid_records to include latest pid
			int num_pids = 0;
			int * pids = NULL;
			if (pid_records[last_service_id]) {
				num_pids = pid_records[last_service_id]->num_pids;
				pids = pid_records[last_service_id]->pids;
				free(pid_records[last_service_id]->pids);
				free(pid_records[last_service_id]);
				pid_records[last_service_id] = NULL;
			}
			sm_pid_t* pid_record = (sm_pid_t*) malloc(sizeof(sm_pid_t));
			pid_record->num_pids = num_pids + 1;
			pid_record->pids = malloc(pid_record->num_pids * sizeof(int));
			for (int i=0; i < num_pids; i++) {
				pid_record->pids[i] = pids[i];
			}
			pid_record->pids[num_pids] = cpid;
			pid_records[last_service_id] = pid_record;

			// update offset
			while (processes[offset]) {
				offset += 1;
			}
			if (processes[offset+1]) {
				offset += 1;
			} 
			else {
				break;
			}
		}
	}

}

// Exercise 1b: print service status
size_t sm_status(sm_status_t statuses[]) {

	// update from records to statuses
	for (int i = 0; i <= last_service_id; i++) {
		statuses[i].pid = records[i]->pid;
		statuses[i].path = records[i]->path;
		statuses[i].running = records[i]->running;
	}

	// update running in statuses/records
	int status;
	for (int i = 0; i <= last_service_id; i++) {
		if (statuses[i].running) {
			pid_t return_pid = waitpid(statuses[i].pid, &status, WNOHANG);
			if (return_pid == statuses[i].pid) {
				statuses[i].running = false;
				records[i]->running = false;
			}
		}
	}
	return last_service_id+1;
}

// Exercise 3: stop service, wait on service, and shutdown
void sm_stop(size_t index) {
	int num_pids = pid_records[index]->num_pids;
	int* pids = pid_records[index]->pids;

	int status;
	for (int i = 0; i < num_pids; i++) {
		pid_t return_pid = waitpid(pids[i], &status, WNOHANG);
		if (return_pid != pids[i]) {
			// kill and then wait
			kill(pids[i], 15);
			waitpid(pids[i], &status, 0);
		}
	}
	records[index]->running = false;
}

void sm_wait(size_t index) {
	int num_pids = pid_records[index]->num_pids;
	int* pids = pid_records[index]->pids;

	int status;
	for (int i = 0; i < num_pids; i++) {
		waitpid(pids[i], &status, 0);
	}

	records[index]->running = false;
}

void sm_shutdown(void) {

	// need to loop through all the services because
	// a service is considered to be "Exited"
	// when the last processes in of that service
	// has terminated. However, this means that
	// there may be processes in that services that 
	// have not terminated yet.
	for (int i = 0; i <= last_service_id; i++) {
		sm_stop(i);
	}
}

// Exercise 4: start with output redirection
void sm_startlog(const char *processes[]) {
	last_service_id ++;
	// startlog /bin/echo hello | /bin/cat | /bin/sha256sum
	// startlog /bin/echo hello | /bin/cat 
	// startlog /bin/echo hello | /bin/sha256sum
	// startlog /bin/echo hello | /bin/diff test.c -
	// startlog /bin/echo hello

	// get total num of processes in this service
	int tot_pro = 1; // assume there will at least be 1 process in the service
	int offset = 0;
	while (true) {
		// move offset to the start of the next process
		while (processes[offset]) {
			offset += 1;
		}
		if (processes[offset+1]) {
			offset += 1;
			tot_pro += 1;
		} 
		else {
			break;
		}
	}

	offset = 0;
	int pipefd_old[2];
	int pipefd_new[2];

	int cpid;

	for (int cur_pro = 1; cur_pro <= tot_pro; cur_pro++) {
		// printf("%d: ", offset);
		// printf("%s\n", processes[offset]);
		
		if (pipe(pipefd_new) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}

		cpid = fork();
		if (cpid == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}

		if (cpid == 0) {
			// last process

			if (cur_pro == tot_pro) {
				// printf("(%d)last\n", getpid());
				// printf("(%d)process %d:\n", getpid() ,cur_pro);
				// printf("(%d)(1)%s\n", getpid(), processes[offset]);
				if (cur_pro != 1) {
					close(pipefd_old[WRITE_END]);
					close(pipefd_new[READ_END]);
					close(pipefd_new[WRITE_END]);

					if (dup2(pipefd_old[READ_END], STDIN_FILENO) == -1) {
						perror("dup2");
						exit(EXIT_FAILURE);
					}
				}

				char filename[100];
				strcpy(filename, "service");
				char N[12];
				sprintf(N, "%d", last_service_id);
				strcat(filename, N);
				strcat(filename, ".log");

				int outputfd = open(filename, O_APPEND | O_CREAT | O_WRONLY, 0777);

				if (dup2(outputfd, STDOUT_FILENO) == -1) {
					perror("dup2");
					exit(EXIT_FAILURE);
				}

				if (dup2(outputfd, STDERR_FILENO) == -1) {
					perror("dup2");
					exit(EXIT_FAILURE);
				}
				
				int num_args = 2;
				

				while (processes[offset+num_args-1] != NULL) {
					num_args++;
				}
				
				char * args[num_args];
				for (int i = 0; i < num_args; i++) {
					args[i] = (char * const) processes[offset+i];
				}

				execv(processes[offset], args);
				perror("execv\n");
				exit(EXIT_FAILURE);
			}
			// first process (more than 1 process in service)
			if (cur_pro == 1) {
				// printf("(%d)first\n", getpid());
				// printf("(%d)process %d:\n", getpid(), cur_pro);
				// printf("(%d)(2)%s\n", getpid(), processes[offset]);

				fclose(stdin);
				close(pipefd_new[READ_END]);

				if (dup2(pipefd_new[WRITE_END], STDOUT_FILENO) == -1) {
					perror("dup2");
					exit(EXIT_FAILURE);
				}	
				
				execv(processes[0], (char * const*) processes);
				printf("execv\n");
				exit(EXIT_FAILURE);
			}
			//non-terminal process
			else {
				// printf("(%d)non-terminal\n", getpid());
				// printf("(%d)process %d:\n", getpid(), cur_pro);
				// printf("(%d)(3)%s\n", getpid(), processes[offset]);

				close(pipefd_old[WRITE_END]);
				close(pipefd_new[READ_END]);

				if (dup2(pipefd_old[READ_END], STDIN_FILENO) == -1) {
					perror("dup");
					exit(EXIT_FAILURE);
				}
				
				if (dup2(pipefd_new[WRITE_END], STDOUT_FILENO) == -1) {
					perror("dup");
					exit(EXIT_FAILURE);
				}
		
				int num_args = 2;
				while (processes[offset+num_args-1] != NULL) {
					num_args++;
				}
				char * args[num_args];
				for (int i = 0; i < num_args; i++) {
					args[i] = (char * const) processes[offset+i];
				}

				execv(processes[offset], args);
				perror("execv\n");
				exit(EXIT_FAILURE);
			}
		}
		else {
			close(pipefd_new[WRITE_END]); 
			// close(pipefd_new[READ_END]);

			pipefd_old[READ_END] = pipefd_new[READ_END];
			pipefd_old[WRITE_END] = pipefd_new[WRITE_END];

			// update service information using the latest processes
			sm_status_t* record = (sm_status_t*) malloc(sizeof(sm_status_t));
			record->pid = cpid;
			char* memory = (char*)malloc(sizeof(char)*1+strlen(processes[offset]));
			strcpy(memory, processes[offset]);
			record->path = memory;
			record->running = true;

			if (records[last_service_id]) {
				free((void *)records[last_service_id]->path);
				free(records[last_service_id]);
				records[last_service_id] = NULL;
			}
			records[last_service_id] = record;

			// update pid_records to include latest pid
			int num_pids = 0;
			int * pids = NULL;
			if (pid_records[last_service_id]) {
				num_pids = pid_records[last_service_id]->num_pids;
				pids = pid_records[last_service_id]->pids;
				free(pid_records[last_service_id]->pids);
				free(pid_records[last_service_id]);
				pid_records[last_service_id] = NULL;
			}
			sm_pid_t* pid_record = (sm_pid_t*) malloc(sizeof(sm_pid_t));
			pid_record->num_pids = num_pids + 1;
			pid_record->pids = malloc(pid_record->num_pids * sizeof(int));
			for (int i=0; i < num_pids; i++) {
				pid_record->pids[i] = pids[i];
			}
			pid_record->pids[num_pids] = cpid;
			pid_records[last_service_id] = pid_record;

			// update offset
			while (processes[offset]) {
				offset += 1;
			}
			if (processes[offset+1]) {
				offset += 1;
			} 
			else {
				break;
			}
		}

	}
}

// Exercise 5: show log file
void sm_showlog(size_t index) {
	
	char filename[100];
	strcpy(filename, "service");

	char N[12];
	sprintf(N, "%d", (int) index);
	strcat(filename, N);
	strcat(filename, ".log");

    FILE *file = fopen(filename, "r");
    char currentline[100];

    if (!file) {
    	printf("service has no log file\n");
    	return;
    }

    while (fgets(currentline, sizeof(currentline), file) != NULL) {
        printf("%s",currentline);
    }

    fclose(file);
}


// showlog 0 -> seg fault
// i think somewhere in main.c, it checks if index >= num_services
// created