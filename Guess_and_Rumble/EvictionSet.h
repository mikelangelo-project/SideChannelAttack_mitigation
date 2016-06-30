
#ifndef EVICTION_SET_H
#define EVICTION_SET_H

#include "CacheDefs.h"
#include <stdint.h>

#define BITS_IN_CHAR 8

typedef struct Cache_Line_Node {
	struct Cache_Line_Node* pNext;
	Cache_Line* pLine;
}Cache_Line_Node;

extern int NUMBER_OF_KEYS_TO_SAMPLE;
extern int exitflag;

/**
 * This function gets a buffer of type LinesBuffer, and uses its lines to
 * map a cache eviction set.
 * I.E. A set of memory lines, sorted into sets, which will allow to rewrite the
 * entire cache.
 *
 * The found cache sets will be saved to the given Cache_Mapping pointer.
 *
 * Returns the number of cache sets which were found.
 */
int CreateEvictionSetFromBuff(LinesBuffer* pBuffer, Cache_Mapping* pEvictionSets);

/**
 * Same as CreateEvictionSetFromBuff, but this time takes a pointer to a chunk of memory
 * and its size, instead of a struct.
 */
int CreateEvictionSetFromMem(char* Memory, uint32_t Size, Cache_Mapping* pCacheSets);

/**
 * This function is not neccesary for the attack, but it is usefull for getting some
 * statistics over the cache when analyzing the cache behaviour of a new library.
 *
 * Gets a cache mapping, and fills the found statistics into the give Cache_Statistics
 * pointer.
 */
void GetMemoryStatistics(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats);

/**
 * Helper function for when we want to save our statistics to a file for later review.
 */
void WriteStatisticsToFile(Cache_Statistics* pCacheStats, char* pFileName, int Sorted);

/**
 * Sorts a set of statistics in a Cache_Statistics struct.
 *
 * Returns the sorted list in the StatisticsToSort pointer.
 */
void SortStatistics(Cache_Statistics* BaseStatistics, Cache_Statistics* StatisticsToSort);
/**
 * this function try to find the attacked sets,
 * Gets mapped cache, a pointer to array of the suspected sets number.
 * return number of suspected sets
 */
int findSuspects(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound);
/**
 * run in loop until user exit and read the first byte in the first line in each one of the suspected sets.
 */
void noiseSuspectedSets(Cache_Mapping* pEvictionSets, int* suspectsSets , int suspectsNum);
/**
 * This function gets a setnum and a array of Suspects and return 1 if the set is in the list, else 0.
 */
int IsSuspectSet(int setnum,int * suspectsSets);
/*
* This function gets a list of sets and a cache mapping, and fills the found statistics for the given list into the give Cache_Statistics
* pointer.
*/
void GetMemorySatisticsFromListOfSets(Cache_Mapping* pEvictionSets, Cache_Statistics* pCacheStats,int* SetsToTest,int NumOfSamplesPerSet);
/**
 * This function try to find sets in use for the decryption process by running usage statistics on the cache sets.
 * at first we have to run the statistics before runing the decryption process and after that we'll run the decryption and try the statistics again.
 * the sets with the highest deltas between the two processes, are the suspects.
 * this function returns the number of suspects found.
 */
int findsuspectsWithPatterns(Cache_Mapping* pEvictionSets, int* suspectsSets, int SetsFound);

#endif
