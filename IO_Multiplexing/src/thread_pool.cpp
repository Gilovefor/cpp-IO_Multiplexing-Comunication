#include "../head/thread_pool.h"

//构造函数
Thread_Pool::Thread_Pool(int numThreads) :stop(false) {	//初始化stop为false
	for (int i = 0; i < numThreads; i++) {		//创造指定数量的线程（numThreads）
		Thread_container.emplace_back([this] {	//Lambda 捕获 this 指针，以便在 Lambda 内部访问类成员
			while (true) {
				std::function<void()> func;		//存储从任务队列中取出的任务
				{
					//临界区，使用锁来保护任务队列，防止数据竞争
					std::unique_lock<std::mutex> lock(mtx);
					//当任务为空或者线程池发出停止信号（stop==true）的时候被唤醒（此时唤醒是为了关闭）
					condition.wait(lock, [this] {return stop || !Task_queue.empty(); });
					if (stop && Task_queue.empty())		//当线程池停止或者任务队列为空的时候，线程结束运行
						return;

					func = std::move(Task_queue.front());	//获取队列里的第一个任务
					Task_queue.pop();						//移除已经取出的任务
				}
				func();		//调用此任务
			}
			});
	}
}

//析构函数
Thread_Pool::~Thread_Pool() {
	{
		std::unique_lock<std::mutex> lock(mtx);	//避免多个线程同时检查stop
		stop = true;							//向线程池发出停止信号
	}
	condition.notify_all();			//唤醒所有线程
	for (std::thread& thread : Thread_container) {
		thread.join();				//等待所有线程完成并且退出
	}
}

  //*******模板函数实现放在头文件中，防止链接器找不到的问题（undefined reference）**********//
//template <typename F, typename... Args>	//第一个参数定义要做的任务，第二个参数提供任务所需的参数
//void Thread_Pool::enqueue(F&& f, Args&&... args) {
//	//func是一个lambda表达式，封装一个任务
//	//std::forward<>实现完美转发，保留原本值是左值还是右值
//	//...展开参数包（相当于一大堆参数压缩发过来，然后再解压）
//	//mutable，使捕获的变量（按值捕获）可以被修改
//	auto func = [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
//		f(args...);
//		};
//	{
//		std::unique_lock<std::mutex>lock(mtx);
//		Task_queue.emplace(std::move(func));	//将这个任务加入到任务队列队尾
//	}
//	condition.notify_one();		//有新任务入队，所以要唤醒一个线程
//}

int Thread_Pool::get_task_count() {
	std::lock_guard<std::mutex> lock(mtx); // 确保线程安全访问任务队列
	return Task_queue.size();              // 返回任务队列的大小
}

void Thread_Pool::thread_safe_print(int task_id, std::thread::id thread_id, const std::string& status) {
	std::lock_guard<std::mutex> lock(cout_mtx);
	std::cout << "任务 " << task_id << " 正在 " << status << " 中，线程 ID: " << thread_id << std::endl;
};

