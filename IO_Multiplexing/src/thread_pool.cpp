#include "../head/thread_pool.h"

//���캯��
Thread_Pool::Thread_Pool(int numThreads) :stop(false) {	//��ʼ��stopΪfalse
	for (int i = 0; i < numThreads; i++) {		//����ָ���������̣߳�numThreads��
		Thread_container.emplace_back([this] {	//Lambda ���� this ָ�룬�Ա��� Lambda �ڲ��������Ա
			while (true) {
				std::function<void()> func;		//�洢�����������ȡ��������
				{
					//�ٽ�����ʹ����������������У���ֹ���ݾ���
					std::unique_lock<std::mutex> lock(mtx);
					//������Ϊ�ջ����̳߳ط���ֹͣ�źţ�stop==true����ʱ�򱻻��ѣ���ʱ������Ϊ�˹رգ�
					condition.wait(lock, [this] {return stop || !Task_queue.empty(); });
					if (stop && Task_queue.empty())		//���̳߳�ֹͣ�����������Ϊ�յ�ʱ���߳̽�������
						return;

					func = std::move(Task_queue.front());	//��ȡ������ĵ�һ������
					Task_queue.pop();						//�Ƴ��Ѿ�ȡ��������
				}
				func();		//���ô�����
			}
			});
	}
}

//��������
Thread_Pool::~Thread_Pool() {
	{
		std::unique_lock<std::mutex> lock(mtx);	//�������߳�ͬʱ���stop
		stop = true;							//���̳߳ط���ֹͣ�ź�
	}
	condition.notify_all();			//���������߳�
	for (std::thread& thread : Thread_container) {
		thread.join();				//�ȴ������߳���ɲ����˳�
	}
}

  //*******ģ�庯��ʵ�ַ���ͷ�ļ��У���ֹ�������Ҳ��������⣨undefined reference��**********//
//template <typename F, typename... Args>	//��һ����������Ҫ�������񣬵ڶ��������ṩ��������Ĳ���
//void Thread_Pool::enqueue(F&& f, Args&&... args) {
//	//func��һ��lambda���ʽ����װһ������
//	//std::forward<>ʵ������ת��������ԭ��ֵ����ֵ������ֵ
//	//...չ�����������൱��һ��Ѳ���ѹ����������Ȼ���ٽ�ѹ��
//	//mutable��ʹ����ı�������ֵ���񣩿��Ա��޸�
//	auto func = [f = std::forward<F>(f), ...args = std::forward<Args>(args)]() mutable {
//		f(args...);
//		};
//	{
//		std::unique_lock<std::mutex>lock(mtx);
//		Task_queue.emplace(std::move(func));	//�����������뵽������ж�β
//	}
//	condition.notify_one();		//����������ӣ�����Ҫ����һ���߳�
//}

int Thread_Pool::get_task_count() {
	std::lock_guard<std::mutex> lock(mtx); // ȷ���̰߳�ȫ�����������
	return Task_queue.size();              // ����������еĴ�С
}

void Thread_Pool::thread_safe_print(int task_id, std::thread::id thread_id, const std::string& status) {
	std::lock_guard<std::mutex> lock(cout_mtx);
	std::cout << "���� " << task_id << " ���� " << status << " �У��߳� ID: " << thread_id << std::endl;
};

