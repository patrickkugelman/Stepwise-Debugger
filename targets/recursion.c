#include <stdio.h>

static unsigned long factorial(unsigned n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main(void) {
    printf("factorial(5) = %lu\n", factorial(5));
    return 0;
}
