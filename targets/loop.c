#include <stdio.h>

static int total = 0;

static void accumulate(int value) {
    total += value;
}

int main(void) {
    for (int i = 0; i < 10; i++) {
        accumulate(i);
    }
    printf("total = %d\n", total);
    return 0;
}
