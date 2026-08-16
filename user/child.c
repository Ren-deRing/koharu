#include <stdio.h>
#include <stdint.h>
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

    // The pager holds this shared mutex until it is ready to serve our next
    // heap request. Blocking on it goes through mlibc -> SYS_FUTEX_WAIT.
    pthread_mutex_lock(mutex);
    pthread_mutex_unlock(mutex);

    printf("Hello, mlibc!\n");

    return 0;
}
