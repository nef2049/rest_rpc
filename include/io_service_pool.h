#ifndef REST_RPC_IO_SERVICE_POOL_H_
#define REST_RPC_IO_SERVICE_POOL_H_

#include <memory>
#include <vector>

#include "use_asio.hpp"

namespace rest_rpc {
namespace rpc_service {

class io_service_pool : private asio::noncopyable {
public:
    explicit io_service_pool(std::size_t pool_size) : next_io_service_(0) {
        if (pool_size == 0)
            throw std::runtime_error("io_service_pool size is 0");

        for (std::size_t i = 0; i < pool_size; ++i) {
            std::shared_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
            std::shared_ptr<boost::asio::io_service::work> work(new boost::asio::io_service::work(*io_service));
            io_services_.push_back(io_service);
            work_.push_back(work);
        }
    }

    void run() {
        std::vector<std::shared_ptr<std::thread>> threads;
        for (std::size_t i = 0; i < io_services_.size(); ++i) {
            threads.emplace_back(std::make_shared<std::thread>(
                [](const std::shared_ptr<boost::asio::io_service>& svr) {
                    svr->run();
                },
                io_services_[i]));
        }

        for (std::size_t i = 0; i < threads.size(); ++i)
            threads[i]->join();
    }

    void stop() {
        for (std::size_t i = 0; i < io_services_.size(); ++i) {
            io_services_[i]->stop();
        }
    }

    boost::asio::io_service& get_io_service() {
        boost::asio::io_service& io_service = *io_services_[next_io_service_];
        ++next_io_service_;
        if (next_io_service_ == io_services_.size())
            next_io_service_ = 0;
        return io_service;
    }

private:
    /// The pool of io_services.
    std::vector<std::shared_ptr<boost::asio::io_service>> io_services_;

    /// The work that keeps the io_services running.
    std::vector<std::shared_ptr<boost::asio::io_service::work>> work_;

    /// The next io_service to use for a connection.
    std::size_t next_io_service_;
};
}  // namespace rpc_service
}  // namespace rest_rpc

#endif  // REST_RPC_IO_SERVICE_POOL_H_