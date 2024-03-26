#include <yaml-cpp/yaml.h>

#include "server/webserver.hpp"
#include "cfg/ymlconfig.hpp"

int main() {
    YmlConfig ymlConfig;
    ymlConfig.ymlInit();
    WebServer server(ymlConfig);

    server.StartServer();
}