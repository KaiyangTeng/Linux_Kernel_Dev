#include <stdio.h>
#include <unistd.h>



int main(void)
{
    // Please tweak the iteration counts to make this calculation run long enough
    FILE*p=fopen("/proc/mp1/status","w");
    if(!p) 
    {
        perror("fail open /proc/mp1/status write");
        return 0;
    }
    pid_t currpid=getpid();
    printf("userapp pid = %d\n", currpid);
    fprintf(p,"%d\n",currpid);
    fclose(p);
    volatile long long unsigned int sum = 0;
    for (int i = 0; i < 100000000; i++) {
        volatile long long unsigned int fac = 1;
        for (int j = 1; j <= 50; j++) {
            fac *= j;
        }
        sum += fac;
    }
    p=fopen("/proc/mp1/status","r");
    if(!p) 
    {
        perror("fail open /proc/mp1/status read");
        return 0;
    }
    char buffer[256];
    while(fgets(buffer,sizeof(buffer),p))
    {
        fputs(buffer,stdout);
    }
    fclose(p);
    return 0;
}
