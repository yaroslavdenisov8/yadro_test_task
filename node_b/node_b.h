#include <string>
#include <iostream>
#include <fstream>
#include <grpcpp/grpcpp.h>
#include <yaml-cpp/yaml.h>

#include "general.h"
#include "accel.grpc.pb.h"
#include "accel.pb.h"

#define CURRENT_MESSAGE_VERSION 1

class NodeB {
public:
    explicit NodeB(const std::string& server_address, int connect_attemps, int connect_delay_ms, std::string api_key);
    bool connect();
    void run(std::atomic<bool>& is_running);
    void cancel();

private:
    AccelModule response(const AccelPacket &pkt);

    std::unique_ptr<AccelerometerService::Stub> _stub;
    std::unique_ptr<grpc::ClientContext> _context;
    std::unique_ptr<grpc::ClientReaderWriter<AccelModule, AccelPacket>> _stream;
    int _connect_attemps;
    int _connect_delay_ms;
    std::string _api_key;
};

