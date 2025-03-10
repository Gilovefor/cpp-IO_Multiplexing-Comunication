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

template <typename F, typename... Args>	//��һ����������Ҫ�������񣬵ڶ��������ṩ��������Ĳ���
void Thread_Pool::enqueue(F&& f, Args&&... args) {
	//func��һ��lambda���ʽ����װһ������
	//std::forward<>ʵ������ת��������ԭ��ֵ����ֵ������ֵ
	//...չ�����������൱��һ��Ѳ���ѹ����������Ȼ���ٽ�ѹ��
	//mutable��ʹ����ı�������ֵ���񣩿��Ա��޸�
	auto func = [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
		f(args...);
		};
	{
		std::unique_lock<std::mutex>lock(mtx);
		Task_queue.emplace(std::move(func));	//�����������뵽������ж�β
	}
	condition.notify_one();		//����������ӣ�����Ҫ����һ���߳�
}


#endif
