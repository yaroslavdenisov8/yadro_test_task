#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "general.h"
#include "server.h"

#define MAX_SHUTDOWN_TIME_MS 1000

std::unique_ptr<grpc::Server> server;

void sig_handler(int sig_num) {
    std::cout << "Received signal " << sig_num << ". Shutting down...\n";
    if (server) {
        gpr_timespec deadline = gpr_time_from_millis(MAX_SHUTDOWN_TIME_MS, GPR_TIMESPAN);
        server->Shutdown(deadline);
    }
}

void bind_signal() {
    struct sigaction sa;
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
}

int main(int argc, char **argv) {
    spdlog::set_pattern(LOG_PATTERN);
    spdlog::set_level(LOG_LEVEL);

    if (argc < 2) {
        spdlog::error("Usage: {} <port>", argv[0]);
        return 1;
    }

    int port;
    std::string api_key;
    try {
        port = std::stoi(argv[1]);
        YAML::Node config = YAML::LoadFile(CONFIG_FILE);
        api_key = config["api_key"].as<std::string>();
    } catch (const std::exception& e) {
        spdlog::error("Initialization error: {}", e.what());
        return 1;
    }

    bind_signal();

    Service service(api_key);
    grpc::ServerBuilder builder;

    grpc::SslServerCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = read_file(CERT_DIR "ca.crt");
    grpc::SslServerCredentialsOptions::PemKeyCertPair pkcp = {
        read_file(CERT_DIR "server.key"),
        read_file(CERT_DIR "server.crt")
    };
    ssl_opts.pem_key_cert_pairs.push_back(pkcp);
    ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;

    builder.AddListeningPort("0.0.0.0:" + std::to_string(port), grpc::SslServerCredentials(ssl_opts));
    builder.RegisterService(&service);

    server = builder.BuildAndStart();
    if (!server) {
        spdlog::error("Failed to start server on port {}", port);
        return 1;
    }

    spdlog::info("Server listening on port {}", port);

    server->Wait();

    spdlog::info("Server stopped");
    return 0;
}
