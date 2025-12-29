/**
 * EB2 实验四：实时图像处理系统多线程框架 (伪代码/骨架)
 * * 思路：
 * 1. 主线程或采集线程：负责从摄像头读取 Frame (生产者)
 * 2. 处理线程：负责对 Frame 进行 AI 推理或图像处理 (消费者)
 * 3. 互斥锁/条件变量：保护共享的 Frame 缓冲区
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <unistd.h>

// 模拟共享图像数据
int shared_image_buffer = 0; 
bool is_data_ready = false;
std::mutex mtx;

void capture_thread() {
    while(true) {
        usleep(30000); // 模拟 30ms 获取一帧 (约33fps)
        
        std::lock_guard<std::mutex> lock(mtx);
        shared_image_buffer++; // 模拟写入新图像数据
        is_data_ready = true;
        // 在实际代码中，这里会将 capture.read(frame) 的数据放入队列
        // std::cout << "[Capture] Got frame " << shared_image_buffer << std::endl;
    }
}

void process_thread() {
    int last_processed = 0;
    while(true) {
        int current_data = 0;
        bool has_new = false;
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            if (is_data_ready) {
                current_data = shared_image_buffer;
                has_new = true;
                is_data_ready = false; // 标记已取走
            }
        }
        
        if(has_new) {
            // 模拟耗时的图像处理
            usleep(50000); // 模拟 50ms 处理时间
            std::cout << "[Process] Processed frame " << current_data << std::endl;
        } else {
            usleep(1000); // 避免空转占用CPU
        }
    }
}

int main() {
    std::cout << "Starting EB2 Image Processing Skeleton..." << std::endl;
    
    // 启动线程
    std::thread t1(capture_thread);
    std::thread t2(process_thread);
    
    t1.join();
    t2.join();
    
    return 0;
}
