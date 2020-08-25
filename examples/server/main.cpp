#include "memory"
#include "rpc_server.h"

struct dummy {
    int add(rest_rpc::rpc_service::rpc_conn conn, int a, int b) {
        return a + b;
    }
};

int main() {

    rest_rpc::rpc_service::rpc_server server(9000, std::thread::hardware_concurrency());

    dummy d;
    server.register_handler("add", &dummy::add, &d);

    server.run();

    return 0;
}