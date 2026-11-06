#include <stdio.h>

static void greet(const char *name) {
    printf("hello, %s\n", name);
}

int main(void) {
    greet("world");
    return 0;
}
