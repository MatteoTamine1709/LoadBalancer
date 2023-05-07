#include "./LoadBalancer.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fstream>
#include <unistd.h>
#include <thread>
#include <spdlog/spdlog.h>

Agent::Agent(std::string host, std::string port)
    : host(host), port(port) {
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == -1) {
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host.c_str());
    addr.sin_port = htons(std::stoi(port));

    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("connect() failed: " + std::string(strerror(errno)));
    }

    spdlog::info("Connected to {}:{}", host, port);
}

LoadBalancer::LoadBalancer() {
    std::ifstream configFile("./config.json");
    if (configFile.is_open()) {
        nlohmann::json config;
        configFile >> config;
        for (auto& [key, value] : config.items()) {
            if (m_configHandlers.find(key) != m_configHandlers.end())
                (this->*m_configHandlers[key])(value);
        }
        configFile.close();
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(m_host.c_str());
    addr.sin_port = htons(std::stoi(m_port));

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == -1) {
        throw std::runtime_error("socket() failed: " + std::string(strerror(errno)));
    }

    int optval = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    if (bind(m_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        throw std::runtime_error("bind() failed: " + std::string(strerror(errno)));
    }

    if (listen(m_socket, 10) == -1) {
        throw std::runtime_error("listen() failed: " + std::string(strerror(errno)));
    }

    spdlog::info("Load balancer is running on {}:{}", m_host, m_port);
}

LoadBalancer::~LoadBalancer() {
    close(m_socket);
    for (auto& [key, value] : m_servers) {
        close(value.s);
    }
}

void LoadBalancer::run() {
    while (true) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int clientSocket = accept(m_socket, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if (clientSocket == -1) {
            throw std::runtime_error("accept() failed: " + std::string(strerror(errno)));
        }

        // spdlog::info("Accepted connection from {}:{}", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        std::thread([&]() {
            // while (true) {
                char buffer[1024];
                int readSize = read(clientSocket, buffer, sizeof(buffer));
                if (readSize == -1) {
                    throw std::runtime_error("read() failed: " + std::string(strerror(errno)));
                }
                std::string request(buffer, readSize);
                if (request.find("LOAD_BALANCER_AGENT\n") == 0) {
                    std::istringstream iss(request.substr(request.find("\n") + 1));
                    std::string agentString;
                    std::getline(iss, agentString);
                    if (m_servers.find(agentString) == m_servers.end()) {
                        std::string host = agentString.substr(0, agentString.find(":"));
                        std::string port = agentString.substr(agentString.find(":") + 1);
                        spdlog::flush_on(spdlog::level::info);
                        spdlog::info("New agent {}:{} connected", host, port);
                        m_servers[agentString] = Agent(host, port);
                    }
                    Agent& agent = m_servers[agentString];
                    double cpuLoad;
                    iss >> agent.cpuLoad;
                    spdlog::info("Agent {}:{} has CPU load {}", agent.host, agent.port, agent.cpuLoad);
                } else {
                    std::string agentString;
                    double minCpuLoad = 100;
                    for (auto& [key, value] : m_servers) {
                        if (value.cpuLoad < minCpuLoad) {
                            minCpuLoad = value.cpuLoad;
                            agentString = key;
                        }
                    }
                    Agent& agent = m_servers[agentString];
                    spdlog::info("Redirecting request to agent {}:{} with CPU load {}", agent.host, agent.port, agent.cpuLoad);
                    write(agent.s, request.c_str(), request.size());
                    char buffer[4 * 1024 * 1024];
                    int readSize = read(agent.s, buffer, sizeof(buffer));
                    if (readSize == -1) {
                        throw std::runtime_error("read() failed: " + std::string(strerror(errno)));
                    }
                    std::string response(buffer, readSize);
                    write(clientSocket, response.c_str(), response.size());
                // }

                close(clientSocket);
            }
        }).detach();
    }
}