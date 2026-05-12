#include <csignal>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "node_b.h"

std::atomic<bool> is_running{true};
NodeB* node_ptr = nullptr;

void sig_handler(int sig_num) {
    std::cout << "Received signal " << sig_num << ". Shutting down...\n";
    is_running.store(false);
    if (node_ptr)
        node_ptr->cancel();
}

void bind_signal() {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
}

int main(int argc, char** argv) {
    spdlog::set_pattern(LOG_PATTERN);
    spdlog::set_level(LOG_LEVEL);

    if (argc < 2) {
        spdlog::error("Usage: {} <port>", argv[0]);
        return 1;
    }

    int port;
    try {
        port = std::stoi(argv[1]);
    } catch (const std::exception& e) {
        spdlog::error("Invalid port: {}", argv[1]);
        return 1;
    }

    bind_signal();

    int connect_attempts;
    int connect_delay_ms;
    std::string api_key;
    try {
        YAML::Node config = YAML::LoadFile(CONFIG_FILE);
        connect_attempts = config["connect_attempts"].as<int>();
        connect_delay_ms = config["connect_delay_ms"].as<int>();
        api_key = config["api_key"].as<std::string>();
    } catch (const std::exception& e) {
        std::cerr << "Error loading config: " << e.what() << std::endl;
        return 1;
    }

    std::string server_addr = "localhost:" + std::to_string(port);

    std::cout << "Connecting to address " << server_addr << std::endl;

    NodeB node(server_addr, connect_attempts, connect_delay_ms, api_key);
    node_ptr = &node;
    node.run(is_running);

    return 0;
}
