#include "memory"
#include "router.h"
#include "rpc_server.h"

struct dummy {
    int add(std::weak_ptr<rest_rpc::rpc_service::connection> conn, int a, int b) {
        return a + b;
    }
};

int main() {

    rest_rpc::rpc_service::rpc_server server(9000, std::thread::hardware_concurrency());

    dummy d;
    server.register_handler<rest_rpc::ExecMode::sync>("add", &dummy::add, &d);

    server.run();

    return 0;
}