#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "sched.h"
#include <stdio.h>
#include <netinet/in.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <gmp.h>
#include <signal.h>
//#include <gnutls/gnutls.h>
//#include <nettle/nettle-types.h>


#include "EvictionSet.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define DEMO 0

LinesBuffer Buffer;

Cache_Mapping EvictionSets;
Cache_Statistics CacheStats;
Cache_Statistics OrderdStats;

Cache_Statistics FirstStatistics;
Cache_Statistics SecondStatistics;
Cache_Statistics DiffStatistics1, DiffStatistics2;

int exitflag; //exit flag when in the noise loop

char* RsaKeys[1];
void sighandler(int sig);

int main()
{
//	char Input[100];
//	int SetsToMonitor[SETS_IN_CACHE][2];
//	int SetsToMonitorNum;
//	int Set;
	int i;
//	int ByteInKey;
	int input_int;
	int SuspectedSets[SETS_IN_CACHE];
	int numOfSuspects;
	exitflag = 0;
	memset(&Buffer, 0, sizeof(Buffer));
	signal(SIGINT, sighandler);

	printf("---------------------------------- Start mapping the cache ----------------------------------\n");
	if (DEMO){
		printf("Press any key.\r\n");
		scanf("%*c");
	}

	int SetsFound = CreateEvictionSetFromBuff(&Buffer, &EvictionSets);
	printf("\nTotal Sets found: %d\r\n", SetsFound);
	while(1){
		printf("--------------------------------------------Menu---------------------------------------------\n");
		printf("1)Get stats\n2)Find suspicious sets\n3)Rumble suspects\n4)Exit\n");
		printf("Please enter your choice\n");
		scanf("%d",&input_int);
		fflush(stdin);
		if (input_int == 1)
		{
			GetMemoryStatistics(&EvictionSets, &CacheStats);
			SortStatistics(&CacheStats,&OrderdStats);
			for(i = 0 ; OrderdStats.SetStats[SetsFound - i -1].num > 1100 ; i++){
				unsigned int SetNum = (unsigned int)OrderdStats.SetStats[SetsFound - i -1].mean;
				printf("%d) Set %d, Num %ld, mean %lf, var %lf, offset %lx\r\n",i, SetNum, (long int)CacheStats.SetStats[SetNum].num,
				CacheStats.SetStats[SetNum].mean, CacheStats.SetStats[SetNum].variance, CacheStats.SetStats[SetNum].offset);
			}
		}
		else if(input_int == 4)
			exit(0);
		else if(input_int == 2){
			numOfSuspects = findsuspectsWithPatterns(&EvictionSets,SuspectedSets,SetsFound);
			printf("number of high activity sets: %d\n",numOfSuspects);

		}
		else if(input_int == 3){
			printf("Making noise\nctrl + c to stop\n");
			noiseSuspectedSets(&EvictionSets,SuspectedSets,numOfSuspects);
		}
		else
			continue;
	}
	GetMemoryStatistics(&EvictionSets, &CacheStats);

	WriteStatisticsToFile(&CacheStats, "statistics.txt", 1);
	return 0;
}


void sighandler(int sig){
	signal(sig,SIG_IGN);
	if(exitflag == 1){
		exitflag = 0;
		signal(SIGINT,sighandler);
	}
	else
		exit(0);
}
