
#include <sys/un.h>

/**
 * Probe Asm gets a linked list of lines (pSet), and a line (pLine)
 * Then it checks if the line conflicts with said Set
 * The set is represented as a linked list where each line points to the next.
 *
 * The function is set as inline to make sure multiple calls will sit in
 * different parts of our memory, preventing contention over the same cache sets.
 * This is important so that if one call will have the code and cache set lines
 * contending for the same sets, at least the next one probably won't.
 *
 * Returns 1 if the line conflicts with the set, and 0 otherwise.
 */
__always_inline int ProbeAsm(char* pSet, int SetSize, char* pLine);

/**
 * Poll Asm goes over a set of lines and checks whether they are all still in the cache
 * Returns how many lines were no longer in the cache.
 */
int PollAsm(char* pSet, int SetSize);

/**
 * ProbeManyTimes probes a certain line to check wether it conflicts with a set.
 * It does so many times to get good statistical assurance, and to ensure most
 * of the calls did not have their code conflict with the lines we are testing against.
 *
 * Returns 1 for if the line conflicts with the set, and 0 otherwise.
 */
int ProbeManyTimes(char* pSet, int SetSize, char* pLine);

/**
 * Debug function for measuring time difference when loading a line to the cache.
 *
 * Returns the time in cpu clocks.
 */
__always_inline int MeasureTimeAsm(char* pMem);

/**
 * Debug function for testing cache speeds.
 * Requires a buffer of memory on which to test.
 */
//void TestCacheSpeed(char* pMem, int MemSize);
