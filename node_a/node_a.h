#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <grpcpp/grpcpp.h>
#include <yaml-cpp/yaml.h>

#include "accel.grpc.pb.h"
#include "accel.pb.h"
#include "emulator.h"
#include "general.h"

#define CURRENT_MESSAGE_VERSION 1

class NodeA {
public:
    explicit NodeA(const std::string& server_address, int connect_attemps, int connect_delay_ms, std::string api_key);

    bool connect();
    void run(AccelEmulator& emulator, std::atomic<bool>& is_running);

private:
    void write_loop(AccelEmulator& emu, std::atomic<bool>& stop);
    void read_loop(std::atomic<bool>& stop);

    std::unique_ptr<AccelerometerService::Stub> _stub;
    std::unique_ptr<grpc::ClientContext> _context;
    std::unique_ptr<grpc::ClientReaderWriter<AccelPacket, AccelModule>> _stream;
    int _connect_attemps;
    int _connect_delay_ms;
    std::string _api_key;
};

