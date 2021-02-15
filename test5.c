#include "sut.h"
#include <stdio.h>
#include <string.h>

void hello1() {
    int i;
    char *str;
    sut_open("0.0.0.0", 32414);
    for (i = 0; i < 10; i++) {
	str = sut_read();
	if (strlen(str) != 0)
	    printf("I am SUT-One, message from server: %s\n", str);
	else
	    printf("ERROR!, empty message received \n");
	sut_yield();
    }
    sut_exit();
}

void hello2() {
    int i;
    for (i = 0; i < 5; i++) {
	printf("Hello world!, this is SUT-Two \n");
	sut_yield();
    }
    sut_exit();
}

void hello3() {
    int i;
    for (i = 0; i < 5; i++) {
	printf("Hello world!, this is SUT-Three \n");
	sut_yield();
    }
    sut_exit();
}

int main() {
    sut_init();
    sut_create(hello1);
    sut_create(hello2);
    sut_create(hello3);
    sut_shutdown();
}
