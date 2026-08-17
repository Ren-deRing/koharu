#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/mman.h>

int main(void) {
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
    
    printf("futex ok\n");

    void *mapped = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mapped == MAP_FAILED) {
        printf("mmap failed\n");
    } else {
        volatile char *p = (volatile char *)mapped;
        p[0] = 0x41;
        printf("mmap -> %p, byte=%d\n", mapped, (int)p[0]);
    }

    char *buf = malloc(64);
    strcpy(buf, "shimoe koharu");
    printf("buffer says: '%s'\n", buf);

    printf("Hello, mlibc!\n");
    return 0;
}
