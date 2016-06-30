
#if 0
int ProbeAsm__COUNTER__(char* pSet, int SetSize, char* pLine, int FindMisses)
{
	int Time;
	int j;
	int MaxLoops = 15;
	int CacheMisses = 0;
	int CacheHits = 0;
	int tmp;

	printf("Probing\r\n", SetSize);
	if (SetSize == 0)
		return 0;

	for (j = 0; j < MaxLoops; j++) {
		// First of all, touch the record.
		tmp = *pLine;
		Time = 1000000;

		/*
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");
		asm volatile ("movl %eax, %edi;");
		asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
		asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
		asm volatile ("L1:");
		asm volatile ("mov (%r8), %r8;");
		asm volatile ("loop	L1;");
		asm volatile ("lfence;");
		asm volatile ("rdtsc;");
		asm volatile ("subl %edi, %eax;");
		asm volatile ("movl %%eax, %0;" : "=r" (Time));
		*/

		// While the results aren't obvious, keep probing this line.
		while (Time > CACHE_POLL_CONTEXT_SWITCH_IN_TICKS ||
				(Time > CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS &&
				Time < CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS))
		{
			usleep(1);

			// Now go over all the records in the set, then touch the original record again and check timing.
			asm volatile ("lfence;");
			asm volatile ("movl %0, %%ecx;" : : "r" (SetSize) : "ecx");
			asm volatile ("mov %0, %%r8;" : : "r" (pSet) : "r8");
			asm volatile ("L2:");
			asm volatile ("mov (%r8), %r8;");
			asm volatile ("loop	L2;");
			asm volatile ("mov %0, %%r9;" : : "r" (pLine) : "r9");
			asm volatile ("add %r8, %r9;");
			asm volatile ("lfence;");
			asm volatile ("rdtsc;");
			asm volatile ("movl %eax, %edi;");
			asm volatile ("mov (%r9), %r9;");
			asm volatile ("lfence;");
			asm volatile ("rdtsc;");
			asm volatile ("subl %edi, %eax;");
			asm volatile ("movl %%eax, %0;" : "=r" (Time));
		}

		printf("Probed %d lines, Took %d nano\r\n", SetSize, Time);

		CacheMisses += (Time >= CACHE_POLL_MIN_MISS_THRESHOLD_IN_TICKS);
		CacheHits += (Time <= CACHE_POLL_MAX_HIT_THRESHOLD_IN_TICKS);


		if (!FindMisses && CacheHits > 0)
			return 0;
/*
		else if (FindMisses && CacheMisses > 0)
			return 1;
*/
		if (CacheMisses > (MaxLoops / 2) || CacheHits > (MaxLoops / 2))
			return (CacheMisses > (MaxLoops / 2));

	}

	//return CacheMisses > (MaxLoops / 2);
	return 1;
}
#endif
