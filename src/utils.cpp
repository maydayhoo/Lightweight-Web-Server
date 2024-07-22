#include "../include/utils.h"

// 设置非阻塞IO
int setnonblocking(int fd) {

    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option |= O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);

    return old_option;
}

void addsig(int sig, sig_hander handler, bool restart = true) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sa.sa_flags |= (restart ? SA_RESTART: 0);
    sigfillset(&sa.sa_mask);

    assert(sigaction(sig, &sa, NULL) != -1);   
}

// 用于执行在命令行执行命令，并将命令执行的结果返回到程序
std::string _exec_command(const char *cmd) {

    std::unique_ptr<FILE, decltype(&pclose)> __pipe(popen(cmd, "r"), pclose);
    if (!__pipe) {
        throw std::runtime_error("popen() failed.");
    }

    std::string result;
    std::array<char, 128> buffer;
    // fgets 函数每次从文件或输入流中读取一行字符，
    // 并在读取的字符后面自动添加一个空字符 '\0' 以终止字符串。
    // 因此，每次调用 fgets 都会确保返回的字符串是以空字符结尾的。
    while (fgets(buffer.data(), buffer.size(), __pipe.get()) != NULL) {
        result += buffer.data();
    }

    return result;
}