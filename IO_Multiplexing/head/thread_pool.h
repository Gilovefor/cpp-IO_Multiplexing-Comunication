#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <utility>

class Thread_Pool {
private:
    std::queue<std::function<void()>> Task_queue;
    std::vector<std::thread> Thread_container;
    std::mutex mtx;
    std::mutex cout_mtx;
    std::condition_variable condition;
    bool stop;

public:
    Thread_Pool(int numThreads);
    ~Thread_Pool();

    template <typename F, typename... Args>
    void enqueue(F&& f, Args&&... args);

    int get_task_count();  

    void thread_safe_print(int task_id, std::thread::id thread_id, const std::string& status);
};

template <typename F, typename... Args>	//第一个参数定义要做的任务，第二个参数提供任务所需的参数
void Thread_Pool::enqueue(F&& f, Args&&... args) {
	//func是一个lambda表达式，封装一个任务
	//std::forward<>实现完美转发，保留原本值是左值还是右值
	//...展开参数包（相当于一大堆参数压缩发过来，然后再解压）
	//mutable，使捕获的变量（按值捕获）可以被修改
	auto func = [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
		f(args...);
		};
	{
		std::unique_lock<std::mutex>lock(mtx);
		Task_queue.emplace(std::move(func));	//将这个任务加入到任务队列队尾
	}
	condition.notify_one();		//有新任务入队，所以要唤醒一个线程
}


#endif
