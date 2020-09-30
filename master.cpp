/*
 * Author: Brandon Lodnon
 * Date: 09/28/2020
 * Prof: Bhatia
 * Course: 4760
 */
#include <sys/time.h>
#include <unistd.h>
#include <fstream> 
#include <iostream>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdlib>
#include <string>
#include <cstring>

using namespace std;

struct shared_memory {
	int count;
	int turn;
	int flags[20];
	char data[20][256];
	int slaveProcessGroup;
};

void trySpawnChild(int count); //spawns child if less than 20 processes are in the system
void spawn(int count); //helper function of spawnChild for code simplicity
void sigHandler(int SIG_CODE); //Signal handler for master to handle Ctrl+C interrupt
void timerSignalHandler(int); //Signal handler for master to handle time out
void releaseMemory(); //Releases all shared memory allocated

void parentInterrupt(int seconds);
void timer(int seconds);

const int MAX_NUM_OF_PROCESSES_IN_SYSTEM = 20;
int currentConcurrentProcessesInSystem = 0;
int maxTotalProcessesInSystem = 4;
int maxConcurrentProcessesInSystem = 2;
int durationBeforeTermination; // How long should master run? set in main
int shmKey = ftok("makefile", 'p');
int shmSegmentID;
struct shared_memory* shm;

int status = 0; //used for wait status in spawnChild function

int startTime; //will hold time right before forking starts, used in main and timer signal handler


int main(int argc, char** argv){
	signal(SIGINT, sigHandler);
	
	if ((shmSegmentID = shmget(shmKey, sizeof(struct shared_memory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0) {
		perror("TESTING");
		exit(1);
	} else {
		shm = (struct shared_memory*) shmat(shmSegmentID, NULL, 0);
	}

	int c;// for getopt
	while((c = getopt(argc, argv, "hn:s:t:")) != -1) {
		switch(c){
			case 'h':
				
				cout<< "HOW TO RUN: " << endl;
				cout<< "./master -h for help command" << endl;
				cout<< "./master [-n x] [-s x] [-t time] infile" << endl;
				cout<< "PARAMETERS: " << endl;
				cout<< "-h Describes how to project should be run and then terminate." << endl;
				cout<< "-n x Indicate the maximum total of child processes master will ever create. (Default 4)" << endl;
				cout<< "-s x Indicate the number of children allowed to exist in the system at the same time (Default 2)" << endl;
				cout<< "-t time The time in seconds after which the processes will terminate, even if it has not finished (Default 100)" << endl;
				cout<< "infile Input file containing strings to be tested" << endl;
				exit(0);
			break;
			
			case 'n'://Max Processes
				maxTotalProcessesInSystem = atoi(optarg);
				if((maxTotalProcessesInSystem <= 0) || maxTotalProcessesInSystem > MAX_NUM_OF_PROCESSES_IN_SYSTEM){
					perror("MaxProcesses: Cannot be Zero or greater than 20");
					exit(1);
				}
			break;
			case 's':
				maxConcurrentProcessesInSystem = atoi(optarg);
				if(maxConcurrentProcessesInSystem <= 0){
					perror("spawnBurst: Cannot be negative or zero");
					exit(1);
				}		
			break;
			case 't':
				durationBeforeTermination = atoi(optarg);
				if(durationBeforeTermination <= 0) {
					perror("durationBeforeTermination: Master cannot have a negative or zero duration");
					exit(1);
				}
			break;
			
			default:
				perror("NOT VALID PARAM: Use -h for Help");
				exit(1);
			break;
						
				
		}
	}
	
	parentInterrupt(durationBeforeTermination);
//============================================================================================================
	int i = 0;
	FILE *fp = fopen(argv[optind], "r");
	if(fp == 0)
	{
		perror("fopen: File not found");
		exit(1);
	}
	char line[256];
	while(fgets(line, sizeof(line), fp) != NULL) {
		line[strlen(line) - 1] = '\0';
		//HOW do i attached shared memory segment
		strcpy(shm->data[i], line);
		i++;
	}
	
	// Set count to number of lines in intput file
	//shm->count = i;
	
	int count = 0;
	
	if(i < maxTotalProcessesInSystem){
		maxTotalProcessesInSystem = i;
	}
	
	if (maxTotalProcessesInSystem < maxConcurrentProcessesInSystem) {
		maxConcurrentProcessesInSystem = maxTotalProcessesInSystem;
	}
	
	shm->count = maxTotalProcessesInSystem;
	
	while(count < maxConcurrentProcessesInSystem){
//		printf("count: %d concurrentProcesses: %d \n", count, currentConcurrentProcessesInSystem);
	 	trySpawnChild(count++);
	 }

	 //wait for all child processes to finish or time to run out, then free up memory and close

	while(currentConcurrentProcessesInSystem > 0){
	 	wait(NULL);
	 	--currentConcurrentProcessesInSystem;
//	 	cout << currentConcurrentProcessesInSystem << " processes in system.\n";
//		printf("\ncount %d\n", count);
		trySpawnChild(count++);
	}
	
	releaseMemory();

	return 0;
}

void trySpawnChild(int count){
	if((currentConcurrentProcessesInSystem < maxConcurrentProcessesInSystem) && (count < maxTotalProcessesInSystem)){
			spawn(count);
	}
}

void spawn(int count){
	++currentConcurrentProcessesInSystem;
	if(fork() == 0){
//		cout << currentConcurrentProcessesInSystem << " processes in system.\n";
	 	if(count == 1) shm->slaveProcessGroup = getpid();
	 	setpgid(0, shm->slaveProcessGroup);
		char buf[256];
		sprintf(buf, "%d", count + 1);
		execl("./palin", "palin", buf, (char*) NULL);
		exit(0);
	 }
}

void sigHandler(int signal){
	killpg(shm->slaveProcessGroup, SIGTERM);
	int status;
	while (wait(&status) > 0) {
		if (WIFEXITED(status)) printf("OK: Child exited with exit status: %d\n", WEXITSTATUS(status));
		else printf("ERROR: Child has not terminated correctly\n");
	}
	releaseMemory();
	cout << "Exiting master process" << endl;
	exit(0);
}

void releaseMemory() {
	shmdt(shm);
	shmctl(shmSegmentID, IPC_RMID, NULL);
}

void parentInterrupt(int seconds)
{
	timer(seconds);

	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = &sigHandler;
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGALRM, &sa, NULL) == -1)
	{
		perror("ERROR");
	}
}


void timer(int seconds)
{
	//Timers decrement from it_value to zero, generate a signal, and reset to it_interval.
	//	//A timer which is set to zero (it_value is zero or the timer expires and it_interval is zero) stops.
	struct itimerval value;
	value.it_value.tv_sec = seconds;
	value.it_value.tv_usec = 0;
	value.it_value.tv_usec = 0;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_usec = 0;
	if(setitimer(ITIMER_REAL, &value, NULL) == -1)
	{
		perror("ERROR");
	}
}
