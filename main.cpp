#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <mutex>
#include <thread>

#define TYPE 3

#if TYPE == 2
boost::asio::io_service io_service;

void workerThread() {
    std::cout << "thread start" << std::endl;
    io_service.run();
    std::cout << "thread finish" << std::endl;
}
#endif

#if TYPE == 3
std::mutex mutex;

void workerThread(const std::shared_ptr<boost::asio::io_service>& io_service) {
    {
        std::lock_guard<std::mutex> lock_guard(mutex);

        std::cout << "=================================" << std::endl;
        std::cout << "[" << std::this_thread::get_id() << "] thread start" << std::endl;
        std::cout << "=================================" << std::endl;
    }

    io_service->run();

    {
        std::lock_guard<std::mutex> lock_guard(mutex);
        std::cout << "=================================" << std::endl;
        std::cout << "[" << std::this_thread::get_id() << "] thread finish" << std::endl;
        std::cout << "=================================" << std::endl;
    }
}

void dispatch(int x) {
    std::lock_guard<std::mutex> lock_guard(mutex);
    std::cout << "[" << std::this_thread::get_id() << "] " << __func__ << " x = " << x << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}

void post(int x) {
    std::cout << "start executing post...." << std::endl;
    std::lock_guard<std::mutex> lock_guard(mutex);
    std::cout << "[" << std::this_thread::get_id() << "] " << __func__ << " x = " << x << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "stop executing post...." << std::endl;
}

void post2() {
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    std::cout << "start executing post2...." << std::endl;
    std::lock_guard<std::mutex> lock_guard(mutex);
    std::cout << "[" << std::this_thread::get_id() << "] " << __func__ << " x = " << 1222 << std::endl;
    std::cout << "stop executing post2...." << std::endl;
}

void run(const std::shared_ptr<boost::asio::io_service>& io_service) {
    std::cout << "run executing..." << std::endl;
    for (int x = 0; x < 3; ++x) {
        /// post: 直接把任务加到queue中，等到没有任务在执行的时候开始执行queue中的任务
        io_service->post(std::bind(post, x));
        /// dispatch: 会直接执行任务，但是如果不能立刻执行就会把任务加到queue中(至于什么条件才会加入到队列中不清楚)
        io_service->dispatch(std::bind(dispatch, x));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "finish run executing..." << std::endl;
}
#endif

#if TYPE == 4

std::mutex mutex;

void workerThread(const std::shared_ptr<boost::asio::io_service>& io_service) {
    {
        std::lock_guard<std::mutex> lock_guard(mutex);
        std::cout << "[" << std::this_thread::get_id() << "] thread start" << std::endl;
    }

    io_service->run();

    {
        std::lock_guard<std::mutex> lock_guard(mutex);
        std::cout << "[" << std::this_thread::get_id() << "] thread finish" << std::endl;
    }
}

void printNum(int x) {
    std::cout << "[" << std::this_thread::get_id() << "] x: " << x << std::endl;
}
#endif

int main() {

#if TYPE == 1
    /// io_service:
    ///     1. 任务队列
    ///     2. 任务分发
    boost::asio::io_service io_service;
    /// work: 使得run()一直处于任务待执行的状态而不会退出
    /// 可以使用boost::shared_ptr<boost::asio::io_service::work>...来结束所有任务
    boost::asio::io_service::work work(io_service);

    /// 执行所有待执行的任务，直到没有任务需要处理
    io_service.run();

    /// 可以通过stop结束run的阻塞
    // io_service.stop();

    std::cout << "Yes, run() is returned!" << std::endl;
#endif

#if TYPE == 2
    std::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(io_service));
    for (int i = 1; i < 10; ++i) {
        std::thread t(workerThread);
        t.detach();
    }
    std::cin.get();

    /// stop()会告知io_service所有的任务需要终止，Any tasks you add behind this point will not be executed.
    /// 如果想让它们都执行，可以先调用work.reset()然后再执行io_service.stop()
    io_service.stop();
    std::cout << "io_service stop..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "main exit" << std::endl;
#endif

#if TYPE == 3
    std::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
    std::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(*io_service));

    {
        std::lock_guard<std::mutex> lock_guard(mutex);
        std::cout << "[" << std::this_thread::get_id() << "] This program will exit when all work has finished."
                  << std::endl;
    }

    std::thread t(std::bind(workerThread, io_service));

    io_service->dispatch(std::bind(run, io_service));

    std::this_thread::sleep_for(std::chrono::seconds(6));

    if (t.joinable()) {
        work.reset();
        t.join();
    }
#endif

#if TYPE == 4
    std::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
    std::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(*io_service));
    /// strand:
    ///     strand提供顺序化的事件执行器，如果以"work1->work2->work3"的顺序post，不管有多少个工作线程，io_service依然会以这样的顺序执行任务
    boost::asio::io_service::strand strand(*io_service);

    {
        std::lock_guard<std::mutex> lock_guard(mutex);
        std::cout << "[" << std::this_thread::get_id() << "] This program will exit when all work has finished."
                  << std::endl;
    }

    for (int x = 0; x < 10; ++x) {
        std::thread t(std::bind(workerThread, io_service));
        t.detach();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    strand.post(std::bind(printNum, 11));
    strand.post(std::bind(printNum, 12));
    strand.post(std::bind(printNum, 13));
    strand.post(std::bind(printNum, 14));
    strand.post(std::bind(printNum, 15));

    io_service->post(std::bind(printNum, 1));
    io_service->post(std::bind(printNum, 2));
    io_service->post(std::bind(printNum, 3));
    io_service->post(std::bind(printNum, 4));
    io_service->post(std::bind(printNum, 5));

    work.reset();

    std::this_thread::sleep_for(std::chrono::seconds(5));
#endif

    return 0;
}
