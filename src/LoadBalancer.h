#ifndef LOADBALANCER_H
#define LOADBALANCER_H

#include <nlohmann/json.hpp>

struct Agent {
    std::string host;
    std::string port;

    int s;
    double cpuLoad;

    Agent() = default;
    Agent(std::string host, std::string port);
};

class LoadBalancer {
public:
    LoadBalancer();
    ~LoadBalancer();
    void run();
private:
    std::unordered_map<std::string, void (LoadBalancer::*)(nlohmann::json &)> m_configHandlers = {
        {"host", &LoadBalancer::handleConfigHost},
        {"port", &LoadBalancer::handleConfigPort}
    };

    void handleConfigHost(nlohmann::json &value);
    void handleConfigPort(nlohmann::json &value);

    std::string m_host = "127.0.0.1";
    std::string m_port = "8080";
    int m_socket;
    std::unordered_map<std::string, Agent> m_servers;  
};

#endif
