
#include "CacheDefs.h"
#include "CacheFuncs.h"

#include <stdlib.h>
#include <stdio.h>

__always_inline int ProbeAsm(char* pSet, int SetSize, char* pLine)
{
	int Time;
	int j;
	char* pCurrLine = pSet;

	// If nothing to probe, return
	if (SetSize == 0)
		return 0;

	// Go over the linked list and get to its end.
	for (j = 0; j < SetSize - 1; j++)
	{
		pCurrLine = *(char**)pCurrLine;
	}

	// Put the line we are testing at the end of the linked list.
	*(char**)pCurrLine = pLine;
	*(char**)pLine = 0;

	Time = CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS + 1;

	// While the results aren't obvious, keep probing this line.
	while ((Time > CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS &&
			Time < CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS))
	{
		// Now go over all the records in the set for the first time.
		// We do this 2 times to be very very sure all lines have been loaded to the cache.

		// First load all data into registers to avoid unneccesarily touching the stack.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");

		asm volatile ("mov (%r9), %r10;");		// Touch the line we want to test, loading it to the cache.
		asm volatile ("lfence;");
		asm volatile ("2:");
		asm volatile ("mov (%r8), %r8;");		// Move to the next line in the linked list.
		asm volatile ("loop	2b;");

		// This is a copy of the previous lines.
		// We go over all the records for the second time.
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("lfence;");
		asm volatile ("1:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	1b;");

		// Now check if the original line is still in the cache
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the current time in cpu clocks
		asm volatile ("movl %eax, %edi;");		// Save it aside in edi
		asm volatile ("lfence;");
		asm volatile ("mov (%r8), %r8;");		// Touch the last line in the linked list, which is the new line.
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");				// Get the time again.
		asm volatile ("subl %edi, %eax;");		// Calculate how much time has passed.
		asm volatile ("movl %%eax, %0;" : "=r" (Time));  // Save that time to the Time variable
	}

	//printf("Probed %d lines, Took %d nano, %d context switches, %d failures\r\n", SetSize, Time, ContextSwitches, fails - 1);

	*(char**)pCurrLine = 0;
	return (Time >= CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS);
}

int PollAsm(char* pSet, int SetSize)
{
	long int Miss = 0;
	int PollTimeLimit = CACHE_POLL_MIN_SET_MISS_THRESHOLD_IN_TICKS;

	if (SetSize == 0)
		return 0;

	// Read the variables to registers.
	asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
	asm volatile ("movl %0, %%ebx;" : : "r" (PollTimeLimit) : "ebx");
	asm volatile ("mov %0, %%r10;" : : "r" (Miss) : "r10");
	asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");

	// This loop goes over all lines in the set and checks if they are still in the cache.
	asm volatile ("CHECK_LINE:");

	asm volatile ("rdtsc;");			// Get the current time
	asm volatile ("movl %eax, %edi;");	// Save it in edi
	asm volatile ("mov (%r8), %r8;");	// Read the next line into the cache
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");			// Get time again
	asm volatile ("subl %edi, %eax;");	// Check how much time has passed.
	asm volatile ("cmp %eax, %ebx;");	// Is it too much?
	asm volatile ("jnl HIT");			// If not, the line is still in the cache (Cache Hit)
	asm volatile ("MISS:");				// If yes, then this is a cache miss.
	asm volatile ("inc %r10");			// Count how many cache misses we've had so far.
	asm volatile ("HIT:");

	asm volatile ("loop	CHECK_LINE;");

	asm volatile ("mov %%r10, %0;" : "=r" (Miss));	// Save to variable how many misses have we seen.

	return Miss;
}

int ProbeManyTimes(char* pSet, int SetSize, char* pLine)
{
	int Misses = 4;
	int Result = 0;

	// We don't trust probe, because probe code could conflict with the line we are probing.
	// And there could be noise and prefetcher and whatnot.
	// So we probe many times, then check if we've had enough misses/hits to make a decision.
	// Each call to probe asm duplicates its code, promising they do not sit in the same area in the cache.
	// Thus reducing the chance all calls will conflict with the lines.
	// We also add variables to the stack in between calls, to make sure we also use different stack
	// memory in each call.

	// Current probe count is 15.
	// This code was written when the code was much less stable and many probes were needed.
	// We probably don't need these many probe operations anymore.
	// Still, these are cheap operations, so we can allow ourselves.
	while (Misses > 2 && Misses < 5) {
		Misses = 0;
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad1[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad2[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad3[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad4[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad5[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad6[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad7[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad8[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad9[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad10[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad11[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad12[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad13[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		char Pad14[128];
		Result = ProbeAsm(pSet, SetSize, pLine);
		Misses += Result;
		memset(Pad1, 0, sizeof(Pad1));
		memset(Pad2, 0, sizeof(Pad2));
		memset(Pad4, 0, sizeof(Pad4));
		memset(Pad3, 0, sizeof(Pad3));
		memset(Pad5, 0, sizeof(Pad4));
		memset(Pad6, 0, sizeof(Pad3));
		memset(Pad7, 0, sizeof(Pad4));
		memset(Pad8, 0, sizeof(Pad4));
		memset(Pad9, 0, sizeof(Pad4));
		memset(Pad10, 0, sizeof(Pad4));
		memset(Pad11, 0, sizeof(Pad4));
		memset(Pad12, 0, sizeof(Pad4));
		memset(Pad13, 0, sizeof(Pad4));
		memset(Pad14, 0, sizeof(Pad4));
	}

	return Misses >= 5;
}

inline int MeasureTimeAsm(char* pMem)
{
	int Time;
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("movl %eax, %edi;");
	asm volatile ("movl (%0), %%ebx;" : : "r" (pMem) : "ebx");
	asm volatile ("lfence;");
	asm volatile ("rdtsc;");
	asm volatile ("subl %edi, %eax;");
	asm volatile ("movl %%eax, %0;" : "=r" (Time));
	return Time;
}

/*
void TestCacheSpeed(char* pMem, int MemSize)
{
	double elapsedTime;
	int i, j;
	srand (196509312);
	memset(pMem, 0, MemSize);

	for (j = 0; j < 3; j++)
	{
		int RandLine = 1000;
		int Misses = 0;
		int Place = (int)pMem;
		printf("Time %d.\n", j);

		for (i = 0; i < 50; i++)
		{
			if (j == 0)
			{
				RandLine = rand() * 64;
				RandLine %= MemSize;
				*((int*)Place) = (pMem + RandLine);
				Place = pMem + RandLine;// + RandPlace;
			} else
			{
				//Place = *((int*)Place);
			}

			elapsedTime = MeasureTimeAsm(Place);
			printf("First Time: %lf place %x.\n", elapsedTime, Place);

			if (elapsedTime > 100) {
				Misses++;
			}
		}

		printf("Misses %d\n", Misses);
	}
}
*/
