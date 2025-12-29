#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <pthread.h>
#include <unistd.h> // for usleep

// OpenCV headers
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>

#include "ObjectDetection.h"
#include "TSLog.h"

// 标签文件路径
#define LABLE_PATH "../test/imagenet_slim_labels.txt"
// 定义摄像头ID
const int DEVICE_ID = 3; 

using namespace std;
using namespace cv;

// ==========================================
// 全局变量区 (共享资源)
// ==========================================
// 1. 共享数据
Mat g_frame;                  // 存放当前最新的摄像头图像
std::vector<int> g_result;    // 存放最新的检测结果
bool g_isRunning = true;      // 控制程序退出的标志

// 2. 读写锁
pthread_rwlock_t img_lock;    // 保护 g_frame
pthread_rwlock_t res_lock;    // 保护 g_result

// 3. 标签数据 (只读，不需要锁)
std::string* labels = new std::string[1001];

// ==========================================
// 线程函数声明
// ==========================================
void* thread_loop_camera(void* arg);
void* thread_loop_doDetect(void* arg);

// 加载标签的辅助函数
bool load_labels() {
    ifstream in(LABLE_PATH);
    if (!in.is_open()) {
        cout << "Error: Could not open label file." << endl;
        return false;
    }
    string label_line;
    int count = 0;
    while (getline(in, label_line)){
        labels[count] = label_line;
        count++;
        if(count >= 1001) break;
    }
    return true;
}

int main(int argc, char **argv) {
    // 1. 初始化标签
    if (!load_labels()) return -1;

    // 2. 初始化读写锁
    pthread_rwlock_init(&img_lock, NULL);
    pthread_rwlock_init(&res_lock, NULL);

    // 3. 创建线程
    pthread_t t_cam, t_det;
    
    // 启动检测线程 (消费者)
    if (pthread_create(&t_det, NULL, thread_loop_doDetect, NULL) != 0) {
        cerr << "Failed to create detection thread" << endl;
        return -1;
    }

    // 启动摄像头显示线程 (生产者 & UI)
    if (pthread_create(&t_cam, NULL, thread_loop_camera, NULL) != 0) {
        cerr << "Failed to create camera thread" << endl;
        return -1;
    }

    // 4. 等待线程结束
    // 通常 UI 线程 (Camera) 结束意味着程序通过 ESC 退出
    pthread_join(t_cam, NULL);
    
    // UI 结束后，通知检测线程退出
    g_isRunning = false;
    pthread_join(t_det, NULL);

    // 5. 释放资源
    pthread_rwlock_destroy(&img_lock);
    pthread_rwlock_destroy(&res_lock);
    delete[] labels;

    cout << "Main thread exited successfully." << endl;
    return 0;
}

// ==========================================
// 线程 1: 摄像头采集与显示 (Thread Loop Camera)
// ==========================================
void* thread_loop_camera(void* arg) {
    VideoCapture cap;
    cap.open(DEVICE_ID);

    if (!cap.isOpened()) {
        cerr << "ERROR! Unable to open camera: " << DEVICE_ID << endl;
        g_isRunning = false; // 通知其他线程退出
        return NULL;
    }

    Mat frame_local;
    std::vector<int> result_local; // 用于绘制的本地结果副本
    
    namedWindow("Object Detection Live (Multi-Thread)", WINDOW_AUTOSIZE);
    cout << "Press ESC to exit." << endl;

    while (g_isRunning) {
        // 1. 获取图像
        cap.read(frame_local);
        if (frame_local.empty()) {
            cerr << "ERROR! blank frame grabbed\n";
            break;
        }

        // 2. 【写操作】将图像更新到全局变量，供算法线程读取
        // 必须加写锁
        pthread_rwlock_wrlock(&img_lock);
        g_frame = frame_local.clone(); // 深拷贝，防止读写冲突
        pthread_rwlock_unlock(&img_lock);

        // 3. 【读操作】获取最新的检测结果
        // 只需要读锁
        pthread_rwlock_rdlock(&res_lock);
        if (!g_result.empty()) {
            result_local = g_result; // 拷贝结果到本地，避免持有锁太久
        }
        pthread_rwlock_unlock(&res_lock);

        // 4. 绘制结果 (使用本地的 frame 和 result)
        // 即使 result 是上一帧算出来的，画在这一帧上也只是会有轻微的位置延迟，
        // 但视频流本身非常流畅。
        if(result_local.size() > 0){
            int labelIndex = result_local[0];
            string labelName = labels[labelIndex];
            
            // 简单绘制
            string text_to_draw = labelName;
            putText(frame_local, text_to_draw, Point(50, 50), 
                    FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0, 0, 255), 2, LINE_8);
        }

        // 5. 显示
        imshow("Object Detection Live (Multi-Thread)", frame_local);

        // 6. 按键检测
        int key = waitKey(30); 
        if (key == 27) { 
            g_isRunning = false; // 修改标志位，通知检测线程结束
            break; 
        }
    }

    cap.release();
    destroyAllWindows();
    return NULL;
}

// ==========================================
// 线程 2: 算法处理 (Thread Loop DoDetect)
// ==========================================
void* thread_loop_doDetect(void* arg) {
    // 1. 初始化检测器 (在算法线程内部初始化，确保上下文正确)
    ObjectDetection* detector = new ObjectDetection();
    detector->init(1); 
    detector->runtime = 1;
    detector->setConfidence(0.5f); 

    Mat frame_to_process;

    while (g_isRunning) {
        bool has_new_frame = false;

        // 2. 【读操作】从全局变量获取待处理图像
        pthread_rwlock_rdlock(&img_lock);
        if (!g_frame.empty()) {
            frame_to_process = g_frame.clone(); // 深拷贝！关键！
            has_new_frame = true;
        }
        pthread_rwlock_unlock(&img_lock);

        if (has_new_frame) {
            // 3. 执行算法 (最耗时的部分，此时不占用任何锁)
            std::vector<int> current_result = detector->doDetect(frame_to_process);

            // 4. 【写操作】更新全局结果
            // 只有在算出结果的一瞬间才加锁
            pthread_rwlock_wrlock(&res_lock);
            g_result = current_result;
            pthread_rwlock_unlock(&res_lock);
            
            // 控制台输出可保留用于调试
            if(current_result.size() > 0) {
                 // cout << "Detected Index: " << current_result[0] << endl;
            }
        } else {
            // 如果还没获取到图像，稍微休息一下避免死循环占用CPU
            usleep(10000); // 10ms
        }
    }

    delete detector;
    return NULL;
}