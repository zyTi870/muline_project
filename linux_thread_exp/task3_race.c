#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

int number = 0; // 全局共享变量

void* worker(void* arg) {
    char* thread_name = (char*)arg;
    int i;
    // 每个线程尝试增加1000次，总共应为2000
    for(i = 0; i < 1000; i++) {
        // 实验要求的人为制造混乱代码段
        int count = number;
        count++;
        usleep(10); // 微秒级延时，强制让出CPU，增大冲突概率
        number = count;
        
        printf("[%s] Current Number: %d\n", thread_name, number);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    
    printf("Starting Race Condition Experiment (No Lock)...\n");
    
    pthread_create(&t1, NULL, worker, "Thread_1");
    pthread_create(&t2, NULL, worker, "Thread_2");
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    printf("------------------------------------------------\n");
    printf("Expected Result: 2000\n");
    printf("Actual Result:   %d\n", number);
    printf("Analysis: Without locks, context switches during read-modify-write causes data loss.\n");
    
    return 0;
}
