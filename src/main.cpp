#include <unistd.h>
#include "server/webserver.hpp"

int main() {
    /* 端口 ET模式 timeoutMs 优雅退出  */
    /* Mysql配置 */
    /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */    
    WebServer server(1316, 3, 60000, false, 
        3306, "root", "123456", "wxdb",
        10, 5, true, 1, 1024);

    server.StartServer();
}