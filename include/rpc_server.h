#ifndef REST_RPC_RPC_SERVER_H
#define REST_RPC_RPC_SERVER_H

#include <condition_variable>
#include <mutex>
#include <thread>

#include "connection.h"
#include "io_service_pool.h"
#include "router.h"

namespace rest_rpc {
namespace rpc_service {

using boost::asio::ip::tcp;

class rpc_server : private asio::noncopyable {

public:
    rpc_server(short port, size_t size, size_t timeout_seconds = 15, size_t check_seconds = 10)
            : io_service_pool_(size)
            , acceptor_(io_service_pool_.get_io_service(), tcp::endpoint(tcp::v4(), port))
            , timeout_seconds_(timeout_seconds)
            , check_seconds_(check_seconds) {

        do_accept();
        clean_connections_thread_ = std::make_shared<std::thread>([this] {
            clean_connections();
        });
        clean_subscribes_thread_ = std::make_shared<std::thread>([this] {
            clean_subscribes();
        });
    }

    rpc_server(short port, size_t size, ssl_configure ssl_conf, size_t timeout_seconds = 15, size_t check_seconds = 10)
            : rpc_server(port, size, timeout_seconds, check_seconds) {
#ifdef CINATRA_ENABLE_SSL
        ssl_conf_ = std::move(ssl_conf);
#else
        assert(false);
#endif
    }

    ~rpc_server() {
        {
            std::unique_lock<std::mutex> lock(clean_connections_mutex_);
            stop_clean_connections_ = true;
            clean_connections_cv_.notify_all();
        }
        clean_connections_thread_->join();

        {
            std::unique_lock<std::mutex> lock(clean_subscribes_mutex_);
            stop_clean_subscribes_ = true;
            clean_subscribes_cv_.notify_all();
        }
        clean_subscribes_thread_->join();

        io_service_pool_.stop();
        if (async_run_thread_ && async_run_thread_->joinable()) {
            async_run_thread_->join();
        }
    }

    void async_run() {
        async_run_thread_ = std::make_shared<std::thread>([this] {
            io_service_pool_.run();
        });
    }

    void run() {
        io_service_pool_.run();
    }

    /**
     * register一个普通的function
     * @tparam model sync or async
     * @tparam Function
     * @param name
     * @param f
     */
    template <ExecMode model = ExecMode::sync, typename Function>
    void register_handler(std::string const& name, const Function& f) {
        router::instance().register_handler<model>(name, f);
    }

    /**
     * register一个类的成员function
     * @tparam model sync or async
     * @tparam Function
     * @tparam Self
     * @param name
     * @param f
     * @param self 类对象的地址
     */
    template <ExecMode model = ExecMode::sync, typename Function, typename Self>
    void register_handler(std::string const& name, const Function& f, Self* self) {
        router::instance().register_handler<model>(name, f, self);
    }

    void set_conn_timeout_callback(std::function<void(int64_t)> callback) {
        conn_timeout_callback_ = std::move(callback);
    }

    template <typename T>
    void publish(const std::string& key, T data) {
        publish(key, "", std::move(data));
    }

    template <typename T>
    void publish_by_token(const std::string& key, std::string token, T data) {
        publish(key, std::move(token), std::move(data));
    }

    std::set<std::string> get_token_list() {
        std::unique_lock<std::mutex> lock(clean_subscribes_mutex_);
        return token_list_;
    }

private:
    void do_accept() {
        std::cout << "accepting..." << std::endl;

        connection_.reset(new connection(io_service_pool_.get_io_service(), timeout_seconds_));
        connection_->set_callback([this](std::string key, std::string token, const std::weak_ptr<connection>& conn) {
            std::unique_lock<std::mutex> lock(clean_subscribes_mutex_);
            subscribes_map_.emplace(std::move(key) + token, conn);
            if (!token.empty()) {
                token_list_.emplace(std::move(token));
            }
        });

        acceptor_.async_accept(connection_->socket(), [this](boost::system::error_code ec) {
            std::cout << "accept a new connection" << std::endl;
            if (ec) {
                std::cout << "acceptor error: " << ec.message() << std::endl;
            } else {
#ifdef CINATRA_ENABLE_SSL
                if (!ssl_conf_.cert_file.empty()) {
                    connection_->init_ssl_context(ssl_conf_);
                }
#endif
                connection_->start();
                std::unique_lock<std::mutex> lock(clean_connections_mutex_);
                connection_->set_conn_id(conn_id_);
                connections_map_.emplace(conn_id_++, connection_);
            }

            do_accept();
        });
    }

    void clean_connections() {
        while (!stop_clean_connections_) {
            std::unique_lock<std::mutex> lock(clean_connections_mutex_);
            clean_connections_cv_.wait_for(lock, std::chrono::seconds(check_seconds_));

            for (auto it = connections_map_.cbegin(); it != connections_map_.cend();) {
                if (it->second->has_closed()) {
                    if (conn_timeout_callback_) {
                        conn_timeout_callback_(it->second->conn_id());
                    }
                    it = connections_map_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void clean_subscribes() {
        while (!stop_clean_subscribes_) {
            std::unique_lock<std::mutex> lock(clean_subscribes_mutex_);
            clean_subscribes_cv_.wait_for(lock, std::chrono::seconds(10));

            for (auto it = subscribes_map_.cbegin(); it != subscribes_map_.cend();) {
                auto conn = it->second.lock();
                if (conn == nullptr || conn->has_closed()) {
                    it = subscribes_map_.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    template <typename T>
    void publish(const std::string& key, const std::string& token, T data) {
        decltype(subscribes_map_.equal_range(key)) range;

        {
            std::unique_lock<std::mutex> lock(clean_subscribes_mutex_);
            if (subscribes_map_.empty())
                return;

            // range.first指向第一个实例
            // range.second指向最后一个实例的下一个位置
            range = subscribes_map_.equal_range(key + token);
        }

        std::shared_ptr<std::string> shared_data = get_shared_data<T>(std::move(data));
        for (auto it = range.first; it != range.second; ++it) {
            auto conn = it->second.lock();
            if (conn == nullptr || conn->has_closed()) {
                continue;
            }

            conn->publish(key + token, *shared_data);
        }
    }

    template <typename T>
    typename std::enable_if<std::is_assignable<std::string, T>::value, std::shared_ptr<std::string>>::type
    get_shared_data(std::string data) {
        return std::make_shared<std::string>(std::move(data));
    }

    template <typename T>
    typename std::enable_if<!std::is_assignable<std::string, T>::value, std::shared_ptr<std::string>>::type
    get_shared_data(T data) {
        msgpack_codec codec;
        auto buf = codec.pack(std::move(data));
        return std::make_shared<std::string>(buf.data(), buf.size());
    }

    io_service_pool io_service_pool_;
    tcp::acceptor acceptor_;
    std::shared_ptr<connection> connection_;
    std::shared_ptr<std::thread> async_run_thread_;
    std::size_t timeout_seconds_;

    // id: connection
    std::unordered_map<int64_t, std::shared_ptr<connection>> connections_map_;
    int64_t conn_id_ = 0;
    std::mutex clean_connections_mutex_;
    std::shared_ptr<std::thread> clean_connections_thread_;
    size_t check_seconds_;
    bool stop_clean_connections_ = false;
    std::condition_variable clean_connections_cv_;

    std::function<void(int64_t)> conn_timeout_callback_;
    // key+token: connection
    std::unordered_multimap<std::string, std::weak_ptr<connection>> subscribes_map_;
    // token
    std::set<std::string> token_list_;
    std::mutex clean_subscribes_mutex_;
    std::condition_variable clean_subscribes_cv_;

    std::shared_ptr<std::thread> clean_subscribes_thread_;
    bool stop_clean_subscribes_ = false;

    ssl_configure ssl_conf_;
};

}  // namespace rpc_service
}  // namespace rest_rpc

#endif  // REST_RPC_RPC_SERVER_H