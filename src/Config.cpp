#include "LoadBalancer.h"
#include <string>

void LoadBalancer::handleConfigHost(nlohmann::json &value) {
    if (value.is_string())
        m_host = value;
}

void LoadBalancer::handleConfigPort(nlohmann::json &value) {
    if (value.is_number())
        m_port = std::to_string(value.get<int>());
}