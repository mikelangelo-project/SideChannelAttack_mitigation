Cache_SCA_Mitigate : CacheFuncs.o EvictionSet.o main.o
        cc -o Cache_SCA_Mitigate CacheFuncs.o EvictionSet.o main.o -l ssl -l gnutls -l gmp -l crypto

main.o : main.c
        cc -c main.c

EvictionSet.o : EvictionSet.c
        cc -c EvictionSet.c

CacheFuncs.o : CacheFuncs.c
        cc -c CacheFuncs.c




