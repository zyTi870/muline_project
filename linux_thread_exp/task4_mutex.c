#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

int number = 0;
pthread_mutex_t lock; // 定义互斥锁

void* worker(void* arg) {
    char* thread_name = (char*)arg;
    int i;
    for(i = 0; i < 1000; i++) {
        
        // 加锁：进入临界区
        pthread_mutex_lock(&lock);
        
        int count = number;
        count++;
        usleep(10); 
        number = count;
        
        printf("[%s] Current Number: %d\n", thread_name, number);
        
        // 解锁：退出临界区
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    
    // 初始化锁
    pthread_mutex_init(&lock, NULL);
    
    printf("Starting Mutex Experiment (With Lock)...\n");
    
    pthread_create(&t1, NULL, worker, "Thread_1");
    pthread_create(&t2, NULL, worker, "Thread_2");
    
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    
    // 销毁锁
    pthread_mutex_destroy(&lock);
    
    printf("------------------------------------------------\n");
    printf("Expected Result: 2000\n");
    printf("Actual Result:   %d\n", number);
    printf("Analysis: With Mutex, operations are atomic, ensuring data integrity.\n");
    
    return 0;
}
