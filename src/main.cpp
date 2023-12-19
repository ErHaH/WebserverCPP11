#include "server/webserver.hpp"

int main() {
    WebServer server(9999, 0, 6000, false, 
        3306, "wx", "123456", "wxdb",
        10, 5, false, 0, 0);

    server.StartServer();
}