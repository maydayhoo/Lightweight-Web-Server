#include <iostream>
#include <memory.h>
#include "../include/http_request_parser.h"
#include "../include/http_response_sender.h"
#include "../include/client_data.h"
#include "../include/process_pool.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/worker.h"

// tell client what content type can server accept
std::vector<std::string> server_support_content_type = {
    "text/html; charset=utf-8"
};

int main(int argc, char **argv) {

    // check input argc
    if (argc <= 1) {
        std::cout << argv[0] << " did't specify the port of server" << endl;
        return -1;
    }

    // generate && bind addr for listenfd
    int port = atoi(argv[1]);
    if (port <= 1023 || port > 65535) {
        std::cout << "port " << port << "is out of bound (1024~65535)" << std::endl;
        return -1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        std::cout << "socket() call failed, and errno: " << errno << endl;
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    bind(listen_fd, (sockaddr *)&server_addr, sizeof(server_addr));

    // set process num
    std::cout << "Please enter n(Integer && >= 1) process number for process_pool: " << std::endl;
    int process_num = -1;
    cin >> process_num;
    
    // set thread num
    std::cout << "Please enter n(Integer && n >= 0) thread num: " << std::endl;
    int thread_num = -1;
    cin >> thread_num;

    // set server support content type
    Http_Response_Sender::set_support_content_type(server_support_content_type);

    // start
    auto p_process_pool = ProcessPool<ClientData>::create(listen_fd, Worker<ClientData>::work, process_num);
    p_process_pool->run(thread_num);
}