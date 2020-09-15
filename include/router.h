#ifndef REST_RPC_ROUTER_H_
#define REST_RPC_ROUTER_H_

#include <functional>

#include "codec.h"
#include "const_vars.h"
#include "meta_util.hpp"
#include "use_asio.hpp"

namespace rest_rpc {

// clang-format off
enum ExecMode {
    sync,
    async
};
// clang-format on

namespace rpc_service {

class connection;

class router {

private:
    router() = default;

public:
    router(const router&) = delete;
    router(router&&) = delete;

    static router& instance() {
        static router instance;
        return instance;
    }

    // ==================== register and remove ====================
    template <ExecMode model, typename Function>
    void register_handler(std::string const& name, Function f) {
        return register_nonmember_func<model>(name, std::move(f));
    }

    template <ExecMode model, typename Function, typename Self>
    void register_handler(std::string const& name, const Function& f, Self* self) {
        return register_member_func<model>(name, f, self);
    }

    void remove_handler(std::string const& name) {
        this->map_invokers_.erase(name);
    }
    // ==================== register and remove ====================

    // ==================== HANDLE ====================
    template <typename T>
    void route(const char* data, std::size_t size, std::weak_ptr<T> conn) {
        auto conn_sp = conn.lock();
        if (!conn_sp) {
            return;
        }

        auto req_id = conn_sp->request_id();
        std::string result;
        try {
            msgpack_codec codec;
            auto p = codec.unpack<std::tuple<std::string>>(data, size);
            auto& func_name = std::get<0>(p);
            auto it = map_invokers_.find(func_name);
            if (it == map_invokers_.end()) {
                result = codec.pack_args_str(result_code::FAIL, "unknown function: " + func_name);
                conn_sp->response(req_id, std::move(result));
                return;
            }

            ExecMode model;
            // 这里调用apply或者apply_member
            it->second(conn, data, size, result, model);

            if (model == ExecMode::sync) {
                // sync就直接返回结果
                if (result.size() >= MAX_BUF_LEN) {
                    result = codec.pack_args_str(
                        result_code::FAIL, "the response result is out of range: more than 10M " + func_name);
                }
                conn_sp->response(req_id, std::move(result));
            } else {
                // async没有写
                // ...
            }
        } catch (const std::exception& ex) {
            msgpack_codec codec;
            result = codec.pack_args_str(result_code::FAIL, ex.what());
            conn_sp->response(req_id, std::move(result));
        }
    }
    // ==================== HANDLE ====================

private:
    /**
     * call的helper
     * @tparam F
     * @tparam I
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param tup
     * @param ptr
     * @return
     */
    template <typename F, size_t... I, typename Arg, typename... Args>
    static typename std::result_of<F(std::weak_ptr<connection>, Args...)>::type call_helper(
        const F& f,
        const std::index_sequence<I...>&,
        std::tuple<Arg, Args...> tup,
        std::weak_ptr<connection> ptr) {
        return f(ptr, std::move(std::get<I + 1>(tup))...);
    }

    /**
     * 这里才是执行普通函数的地点，执行的普通函数返回值为void
     * @tparam F
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param ptr
     * @param result
     * @param tp
     * @return
     */
    template <typename F, typename Arg, typename... Args>
    static typename std::enable_if<
        std::is_void<typename std::result_of<F(std::weak_ptr<connection>, Args...)>::type>::value>::type
    call(const F& f, std::weak_ptr<connection> ptr, std::string& result, std::tuple<Arg, Args...> tp) {
        call_helper(f, std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), ptr);
        // codec pack
        result = msgpack_codec::pack_args_str(result_code::OK);
    }

    /**
     * 这里才是执行普通函数的地点，执行的普通函数返回值不为void
     * @tparam F std::function<...>
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param ptr
     * @param result
     * @param tp
     * @return
     */
    template <typename F, typename Arg, typename... Args>
    static typename std::enable_if<
        !std::is_void<typename std::result_of<F(std::weak_ptr<connection>, Args...)>::type>::value>::type
    call(const F& f, std::weak_ptr<connection> ptr, std::string& result, std::tuple<Arg, Args...> tp) {
        auto r = call_helper(f, std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), ptr);
        msgpack_codec codec;
        // codec pack
        result = msgpack_codec::pack_args_str(result_code::OK, r);
    }

    /**
     * call_member的helper
     * @tparam F
     * @tparam Self
     * @tparam Indexes
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param self
     * @param tup
     * @param ptr
     * @return
     */
    template <typename F, typename Self, size_t... Indexes, typename Arg, typename... Args>
    static typename std::result_of<F(Self, std::weak_ptr<connection>, Args...)>::type call_member_helper(
        const F& f,
        Self* self,
        const std::index_sequence<Indexes...>&,
        std::tuple<Arg, Args...> tup,
        std::weak_ptr<connection> ptr = std::shared_ptr<connection>{nullptr}) {
        // d.dummy::test(...)
        // d.*&dummy::test(...)
        // (&d)->dummy::test(...)
        // (&d)->*&dummy::test(...)
        return (*self.*f)(ptr, std::move(std::get<Indexes + 1>(tup))...);
    }

    /**
     * 这里才是执行类函数的地点，执行的类函数返回值为void
     * @tparam F
     * @tparam Self
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param self
     * @param ptr
     * @param result
     * @param tp
     * @return
     */
    template <typename F, typename Self, typename Arg, typename... Args>
    static typename std::enable_if<
        std::is_void<typename std::result_of<F(Self, std::weak_ptr<connection>, Args...)>::type>::value>::type
    call_member(
        const F& f,
        Self* self,
        std::weak_ptr<connection> ptr,
        std::string& result,
        std::tuple<Arg, Args...> tp) {
        call_member_helper(f, self, typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), ptr);
        // codec pack
        result = msgpack_codec::pack_args_str(result_code::OK);
    }

    /**
     * 这里才是执行类函数的地点，执行的类函数返回值不为void(注意这里std::enable_if<...>::type仍然为void)
     * @tparam F
     * @tparam Self
     * @tparam Arg
     * @tparam Args
     * @param f
     * @param self
     * @param ptr
     * @param result
     * @param tp
     * @return
     */
    template <typename F, typename Self, typename Arg, typename... Args>
    static typename std::enable_if<
        !std::is_void<typename std::result_of<F(Self, std::weak_ptr<connection>, Args...)>::type>::value>::type
    call_member(
        const F& f,
        Self* self,
        std::weak_ptr<connection> ptr,
        std::string& result,
        std::tuple<Arg, Args...> tp) {
        auto r = call_member_helper(f, self, typename std::make_index_sequence<sizeof...(Args)>{}, std::move(tp), ptr);
        // codec pack
        result = msgpack_codec::pack_args_str(result_code::OK, r);
    }

    template <typename Function>
    struct Invoker {
        /**
         * 执行一个普通的函数
         * @tparam model
         * @param func
         * @param conn
         * @param data
         * @param size
         * @param result
         * @param exe_model
         */
        template <ExecMode model>
        static inline void apply(
            const Function& func,
            std::weak_ptr<connection> conn,
            const char* data,
            size_t size,
            std::string& result,
            ExecMode& exe_model) {
            using args_tuple = typename function_traits<Function>::args_tuple_2nd;
            exe_model = ExecMode::sync;
            msgpack_codec codec;
            try {
                // codec unpack
                auto tp = codec.unpack<args_tuple>(data, size);
                // execute
                call(func, conn, result, std::move(tp));
                exe_model = model;
            } catch (std::invalid_argument& e) {
                result = codec.pack_args_str(result_code::FAIL, e.what());
            } catch (const std::exception& e) {
                result = codec.pack_args_str(result_code::FAIL, e.what());
            }
        }

        /**
         * 执行一个类的成员函数
         * @tparam model
         * @tparam Self
         * @param func
         * @param self
         * @param conn
         * @param data
         * @param size
         * @param result
         * @param exe_model
         */
        template <ExecMode model, typename Self>
        static inline void apply_member(
            const Function& func,
            Self* self,
            std::weak_ptr<connection> conn,
            const char* data,
            size_t size,
            std::string& result,
            ExecMode& exe_model) {
            using args_tuple = typename function_traits<Function>::args_tuple_2nd;
            exe_model = ExecMode::sync;
            msgpack_codec codec;
            try {
                // codec unpack
                auto tp = codec.unpack<args_tuple>(data, size);
                // execute
                call_member(func, self, conn, result, std::move(tp));
                exe_model = model;
            } catch (std::invalid_argument& e) {
                result = codec.pack_args_str(result_code::FAIL, e.what());
            } catch (const std::exception& e) {
                result = codec.pack_args_str(result_code::FAIL, e.what());
            }
        }
    };

    /**
     * 将普通函数添加到map_invokers_中
     * @param name
     * @param f
     */
    template <ExecMode model, typename Function>
    void register_nonmember_func(std::string const& name, Function f) {
        this->map_invokers_[name] = {std::bind(
            &Invoker<Function>::template apply<model>,
            std::move(f),
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5)};
    }

    /**
     * 将类函数添加到map_invokers_中
     * @param name
     * @param f
     * @param self 类对象的地址
     */
    template <ExecMode model, typename Function, typename Self>
    void register_member_func(const std::string& name, const Function& f, Self* self) {
        this->map_invokers_[name] = {std::bind(
            &Invoker<Function>::template apply_member<model, Self>,
            f,
            self,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4,
            std::placeholders::_5)};
    }

    // clang-format off
    std::unordered_map<std::string, std::function<void(std::weak_ptr<connection>, const char*, size_t, std::string&, ExecMode& model)>> map_invokers_;
    // clang-format on
};

}  // namespace rpc_service
}  // namespace rest_rpc

#endif  // REST_RPC_ROUTER_H_
