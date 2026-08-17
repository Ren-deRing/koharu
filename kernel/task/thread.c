#include <koharu/thread.h>
#include <koharu/kmem.h>
#include <string.h>

static struct thread **g_threads = NULL;
static uint32_t g_next_tid       = 0;
static size_t g_threads_cap      = 0;

static int thread_reg_grow(size_t need) {
    size_t new_cap = g_threads_cap ? g_threads_cap * 2 : 16;
    while (new_cap <= need) new_cap *= 2;

    struct thread **new_arr = krealloc(g_threads, sizeof(void *) * new_cap);
    if (!new_arr) return -1;

    memset(&new_arr[g_threads_cap], 0, sizeof(void *) * (new_cap - g_threads_cap));
    g_threads     = new_arr;
    g_threads_cap = new_cap;
    return 0;
}

struct thread *thread_lookup(uint64_t tid) {
    if (!g_threads || tid >= g_threads_cap) return NULL;
    return g_threads[tid];
}

uint32_t thread_alloc_tid(void) {
    return __atomic_fetch_add(&g_next_tid, 1, 5);
}

int thread_register(struct thread *t) {
    if (t->tid >= g_threads_cap) {
        if (thread_reg_grow(t->tid) != 0) return -1;
    }
    g_threads[t->tid] = t;
    return 0;
}

void thread_destroy(struct thread *t) {
    if (!t) return;
    if (t->tid < g_threads_cap)
        g_threads[t->tid] = NULL;
}