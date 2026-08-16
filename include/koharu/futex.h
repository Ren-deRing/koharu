#pragma once

#include <stdint.h>

int futex_wait(void *uaddr, uint32_t expected);
int futex_wake(void *uaddr, int all);