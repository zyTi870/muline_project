#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void* thread_func(void* arg) {
    int i;
    for(i = 0; i < 1000; i++) {
        printf("B: This is the child thread (Count %d)\n", i);
    }
    return NULL;
}

int main(void) {
    pthread_t id;
    int i, ret;
    ret = pthread_create(&id, NULL, thread_func, NULL);
    if(0 != ret) {
        perror("Create pthread error");
        exit(1);
    }
    for(i = 0; i < 1000; i++) {
        printf("A: This is the main process (Count %d)\n", i);
    }
    pthread_join(id, NULL);
    return 0;
}
