#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void thread(void) {
    int i;
    for(i = 0; i < 3; i++) {
        printf("This is a pthread.\n");
        sleep(1); // 添加少量延时以便观察切换，否则可能瞬间执行完
    }
}

int main(void) {
    pthread_t id;
    int i, ret;
    ret = pthread_create(&id, NULL, (void *)thread, NULL);
    if(0 != ret) {
        printf("Create pthread error!\n");
        exit(1);
    }
    for(i = 0; i < 3; i++) {
        printf("This is the main process.\n");
        sleep(1);
    }
    pthread_join(id, NULL);
    return 0;
}
