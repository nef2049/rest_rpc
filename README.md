# rest_rpc
It's so easy to love RPC

## subscribe and publish

- client1 subscribe

client1.subscribe("key2", "048a796c8a3c6a6b7bd1223bf2c8cee05232e927b521984ba417cb2fca6df9d1", [](...){});

- client2 subscribe same or different key-token 

client1.subscribe("key2", "048a796c8a3c6a6b7bd1223bf2c8cee05232e927b521984ba417cb2fca6df9d1", [](...){});

- server

1. 存储token "048a796c8a3c6a6b7bd1223bf2c8cee05232e927b521984ba417cb2fca6df9d1"
2. 存储key-token: connection1(client1保持的连接)
3. 存储key-token: connection2(client2保持的连接)

- server register publish

server.register_handler("publish_by_token", [&server](rpc_conn conn, std::string key, std::string token, std::string val){});
具体的回调行为是找到key-token对应的所有的clients
然后针对每个client都是response(0, std::move(result), request_type::subscribe);

- client call

当clientX call<>("publish_by_token", key, token, data)时候
server会调用publish_by_token对应的回调函数，也就是所有subscribe key-token的clients都会收到data这个数据

- Summary

整个流程其实就是完成了一个client向另外的一个或多个clients发送数据的功能

## Others

To be continue...