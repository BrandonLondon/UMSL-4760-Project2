/**
Author: Brandon London
Course: 4760
Prof: Bhatia
Date: 09/30/20
*/

#include <iostream>
#include <unistd.h>
#include <ctime>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/stat.h>
#include <fstream> //for writing to file
#include <cstdlib>
#include <string.h>
#include <stdbool.h>

using namespace std;
//ADDED STRUCT IDK WHY THOUGH
struct shared_memory {
	int count;
	int turn;
	int flags[20];
	char data[20][256];
	int slaveProcessGroup;
};


enum state { idle, want_in, in_cs };


void sigHandler(int);
char* getFormattedTime(); //gets local time


int id; //process id, used by multiple functions

int main(int argc, char ** argv){
	setvbuf(stdout, NULL, _IONBF, 0);
	
	signal(SIGTERM, sigHandler);
	
	int index;

	if(argc < 2){ //If no arguments were supplied, id must not be set
		perror("No argument supplied for id");
		exit(1);
	}
	else{
		id = atoi(argv[1]);
		index = id - 1;//atoi(argv[2]);
	}
	
	srand(time(0) + id); //generate different delays each run

	int N; //number of slave processes master will spawn

	int shmKey = ftok("makefile", 'p');
	int shmSegmentID;
	shared_memory* shm;
	if ((shmSegmentID = shmget(shmKey, sizeof(struct shared_memory), IPC_CREAT | S_IRUSR | S_IWUSR)) < 0) {
		perror("shmget: Failed to allocate shared memory");
		exit(1);
	} else {
		shm = (struct shared_memory*) shmat(shmSegmentID, NULL, 0);
	}
	
	N = shm->count;
	
	printf("%s: Process %d Entering critical section\n", getFormattedTime(), id);
	//sleep for random amount of time (between 0 and 2 seconds);
//	cerr << getFormattedTime() << ": Process " << id << " wants to enter critical section\n";
//TELLS IF ITS A PALINDROME OR NOT
//========================================================================================
	int l = 0;
	int r = strlen(shm->data[index]) - 1;
	bool palin = true;

	while(r > l)
	{
		if(tolower(shm->data[index][l]) != tolower(shm->data[index][r])) {
			palin = false;
			break;
		}
		l++;
		r--;
	}
	
//	printf("id: %d, string: %s, palindrome: %s\n", id, shm->data[id - 1], palin ? "true" : "false");
	
//========================================================================================
	
	int j;
	do{
		shm->flags[id - 1] = want_in;
		j = shm->turn;
		
		while(j != id - 1){
			j = (shm->flags[j] != idle) ? shm->turn : (j + 1) % N ;
		}

		
		shm->flags[id - 1] = in_cs;

		//check that no-one else is in critical section
		for(j = 0; j < N; j++){
			if((j != id - 1) && (shm->flags[j] == in_cs)){
				break;
			}
		}

	} while( (j < N) || ((shm->turn != id-1) && (shm->flags[shm->turn] != idle)) );
	shm->turn = id-1;

	/* Critical section */

//	cerr << getFormattedTime() << ": Process " << id << " in critical section\n";
	
	printf("%s: Process %d in critical section\n", getFormattedTime(), id);
	//sleep for random amount of time (between 0 and 2 seconds);
	sleep(rand() % 3);

//WRITES TO PALIN.OUT AND NONPALIN.OUT
//======================================================================================
	FILE *file = fopen(palin ? "palin.out" : "nopalin.out", "a+");
	if (file == NULL) {
		perror("");
		exit(1);
	}
	fprintf(file, "%s\n", shm->data[id - 1]);
	fclose(file);
	
	file = fopen("output.log", "a+");
	if (file == NULL) {
		perror("");
		exit(1);
	}
	fprintf(file, "%s %d %d %s\n", getFormattedTime(), getpid(), id - 1, shm->data[id - 1]);
	fclose(file);
	
	//exit from critical section;

//	cerr << getFormattedTime() << ": Process " << id << " exiting critical section\n";

	printf("%s: Process %d exiting critical section\n", getFormattedTime(), id);
	//sleep for random amount of time (between 0 and 2 seconds);
	j = (shm->turn + 1) % N;
	
	while(shm->flags[j] == idle){
		j = (j + 1) % N;
	}

	shm->turn = j;

	shm->flags[id-1] = idle;

	return 0;
}

void sigHandler(int signal) {
	if (signal == SIGTERM) {
		printf("In Signal Handler FUnction\n");
		exit(1);
	}
}


char* getFormattedTime(){
	int timeStringLength;
	string timeFormat;

	timeStringLength = 9;
	timeFormat = "%H:%M:%S";

	time_t seconds = time(0);

	struct tm * ltime = localtime(&seconds);
	char *timeString = new char[timeStringLength];

	strftime(timeString, timeStringLength, timeFormat.c_str(), ltime);

	return timeString;
}
