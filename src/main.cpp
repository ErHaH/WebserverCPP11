#include <unistd.h>
#include "server/webserver.hpp"

int main() {
    WebServer server(1316, 3, 60000, false, 
        3306, "root", "123456", "wxdb",
        10, 5, false, 1, 1024);
        
    server.StartServer();
}