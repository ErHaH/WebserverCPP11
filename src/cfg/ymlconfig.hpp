#ifndef YMLCONFIG_HPP
#define YMLCONFIG_HPP

#include <string.h>
#include <string>
#include <yaml-cpp/yaml.h>
#include <iostream>

struct YmlConfig {
    int serverPort;
    int trigMode;
    int timeOutMs;
    bool optLinger;
    int connPoolNum;
    int threadNum;
    bool openLog;
    int logLevel;
    int logQueSize;
    int sqlPort;
    std::unique_ptr<std::string> sqlUser;
    std::unique_ptr<std::string> sqlPwd;
    std::unique_ptr<std::string> dbName;

    void ymlInit();
};

void YmlConfig::ymlInit() {
    try {
        YAML::Node yamlFile = YAML::LoadFile("properties.yml");
        //server配置
        serverPort = yamlFile["server"]["port"].as<int>();
        trigMode = yamlFile["server"]["trigMode"].as<int>();
        timeOutMs = yamlFile["server"]["timeOutMs"].as<int>();
        optLinger = yamlFile["server"]["optLinger"].as<std::string>() == "true" ? true : false;
        connPoolNum = yamlFile["server"]["connPoolNum"].as<int>();
        threadNum = yamlFile["server"]["threadNum"].as<int>();
        openLog = yamlFile["server"]["openLog"].as<std::string>() == "true" ? true : false;
        logLevel = yamlFile["server"]["logLevel"].as<int>();
        logQueSize = yamlFile["server"]["logQueSize"].as<int>();
        //mysql配置
        sqlPort = yamlFile["mysql"]["sqlPort"].as<int>();
        sqlUser = std::make_unique<std::string>(yamlFile["mysql"]["sqlUser"].as<std::string>()); ;
        sqlPwd = std::make_unique<std::string>(yamlFile["mysql"]["sqlPwd"].as<std::string>());
        dbName = std::make_unique<std::string>(yamlFile["mysql"]["dbName"].as<std::string>());

    } catch(const std::exception& e) {
        std::cerr << e.what() << " -- above is a yaml exception\n";
    }
}


#endif