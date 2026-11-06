#include <stdio.h>

static void store(int *slot, int value) {
    *slot = value;
}

static void forward(int *slot) {
    store(slot, 42);
}

int main(void) {
    int *slot = NULL;
    puts("about to crash");
    fflush(stdout);
    forward(slot);
    return 0;
}
