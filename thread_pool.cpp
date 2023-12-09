#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>
#include <future>
#include <atomic>
#include <queue>
#include <list>

using namespace std;

class thread_pool
{
public:

	thread_pool(int num_threads) {
		threads.reserve(num_threads);
		for (int i = 0; i < num_threads; ++i) {
			threads.emplace_back(&thread_pool::run, this);
		}
	}

	template <typename Func, typename ...Args>
	long long int add_task(const Func& task_func, Args... args) {

		// получаем значение индекса для новой задачи
		lock_guard<mutex> q_lock(q_mtx);
		q.emplace(async(launch::deferred, task_func, args...), last_idx++);

		// делаем notify_one, чтобы проснулся один спящий поток (если такой есть)
		// в методе run
		q_cv.notify_one();
		return last_idx - 1;
	}

	void Wait(long long int task_id) {
		unique_lock<mutex> lock(completed_task_ids_mtx);

		// ожидаем вызова notify в функции run (сработает после завершения задачи)
		completed_task_ids_cv.wait(lock, [this, task_id]()->bool {
			return completed_task_ids.find(task_id) != completed_task_ids.end();
		});
	}

	void Wait_all() {
		unique_lock<mutex> lock(q_mtx);

		// ожидаем вызова notify в функции run (сработает после завершения задачи)
		completed_task_ids_cv.wait(lock, [this]()->bool {
			lock_guard<mutex> task_lock(completed_task_ids_mtx);
			return q.empty() && last_idx - 1 == completed_task_ids.size();
		});
	}

	bool calculated(long long int task_id) {
		lock_guard<mutex> lock(completed_task_ids_mtx);
		if (completed_task_ids.find(task_id) != completed_task_ids.end()) {
			return true;
		}
		return false;
	}

	~thread_pool() {
		quite = true;
		Wait_all();
		for (int i = 0; i < threads.size(); ++i) {
			q_cv.notify_all();
			threads[i].join();
		}
	}

private:

	void run() {
		while (!quite) {
			unique_lock<mutex> lock(q_mtx);

			// если есть задачи, то берём задачу, иначе - засыпаем
			// если мы зашли в деструктор, то quite будет true и мы не будем 
			// ждать завершения всех задач и выйдем из цикла
			q_cv.wait(lock, [this]()->bool { return !q.empty() || quite; });

			if (!q.empty()) {
				auto elem = move(q.front());
				q.pop();
				lock.unlock();

				// вычисляем объект типа future (вычисляем функцию) 
				elem.first.get();

				lock_guard<mutex> lock(completed_task_ids_mtx);

				// добавляем номер выполненой задачи в список завершённых
				completed_task_ids.insert(elem.second);

				// делаем notify, чтобы разбудить потоки
				completed_task_ids_cv.notify_all();
			}
		}
	}

	// очередь задач - хранит функцию(задачу), которую нужно исполнить и номер задачи
	queue<pair<future<void>, long long int>> q;

	mutex q_mtx;
	condition_variable q_cv;

	// очередь задач - хранит функцию(задачу), которую нужно исполнить и номер задачи
	unordered_set<long long int> completed_task_ids;

	mutex completed_task_ids_mtx;
	condition_variable completed_task_ids_cv;

	vector<thread> threads;

	// флаг завершения работы thread_pool
	atomic<bool> quite = false;

	// переменная хранящая id который будет выдан следующей задаче
	atomic<long long int> last_idx = 1;

};

void sum(int& ans, std::vector<int>& arr) {
	for (int i = 0; i < arr.size(); ++i) {
		ans += arr[i];
	}
}

int main() {

	thread_pool tp(3);
	std::vector<int> s1 = { 1, 2, 3 };
	int ans1 = 0;

	std::vector<int> s2 = { 4, 5 };
	int ans2 = 0;

	std::vector<int> s3 = { 8, 9, 10 };
	int ans3 = 0;

	// добавляем в thread_pool выполняться 3 задачи
	auto id1 = tp.add_task(sum, ref(ans1), ref(s1));
	auto id2 = tp.add_task(sum, ref(ans2), ref(s2));
	auto id3 = tp.add_task(sum, ref(ans3), ref(s3));

	tp.Wait_all();
	cout << ans1 << endl;
	cout << ans2 << endl;
	cout << ans3 << endl;
	return 0;
}