#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <bits/syscall.h>

#define SYS_SEND         2
#define KOHARU_REQ_READY 2
#define SHARED_VA 0x700040000000ULL

int main(int argc, char *argv[], char *envp[]) {
    (void)argc;
    (void)argv;
    (void)envp;

    pthread_mutex_t *mutex = (pthread_mutex_t *)SHARED_VA;

    syscall(SYS_SEND, __koharu_pager_tid, KOHARU_REQ_READY, 0, 0, 0);

    pthread_mutex_lock(mutex);
    pthread_mutex_unlock(mutex);

    void *mapped = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) {
        printf("mmap failed\n");
    } else {
        volatile char *p = (volatile char *)mapped;
        p[0] = 0x41;
        printf("mmap -> %p, byte=%d\n", mapped, (int)p[0]);
    }

    char *buf1 = (char*)malloc(64);
    strcpy(buf1, "shimoe koharu");
    printf("buffer says: '%s'\n", buf1);

    printf("Hello, mlibc!\n");

    return 0;
}
