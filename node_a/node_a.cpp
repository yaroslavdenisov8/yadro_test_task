#include <fstream>
#include <filesystem>
#include <spdlog/spdlog.h>

#include "node_a.h"

NodeA::NodeA(const std::string& server_address, int connect_attemps, int connect_delay_ms, std::string api_key)
: _connect_attemps(connect_attemps), _connect_delay_ms(connect_delay_ms), _api_key(std::move(api_key))
{
    std::filesystem::create_directories(RESULT_DIR);

    grpc::SslCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = read_file(CERT_DIR "ca.crt");
    ssl_opts.pem_private_key = read_file(CERT_DIR "client.key");
    ssl_opts.pem_cert_chain = read_file(CERT_DIR "client.crt");

    auto channel = grpc::CreateChannel(server_address, grpc::SslCredentials(ssl_opts));
    _stub = AccelerometerService::NewStub(channel);

    spdlog::info("node A initialized, target: {}", server_address);
}

bool NodeA::connect() {
    _context = std::make_unique<grpc::ClientContext>();
    _context->AddMetadata("x-api-key", _api_key);

    _stream = _stub->StreamAccelDataA(_context.get());
    if (!_stream) {
        spdlog::error("failed to create gRPC stream to server");
        return false;
    }
    spdlog::info("node A created server stream");
    return true;
}

void NodeA::run(AccelEmulator& emulator, std::atomic<bool>& is_running) {
    for (int attempt = 0; attempt < _connect_attemps && is_running.load(); ++attempt) {
        if (connect()) {
            std::thread writer([this, &emulator, &is_running]() { write_loop(emulator, is_running); });
            std::thread reader([this, &is_running]() { read_loop(is_running); });

            reader.join();
            writer.join();

            grpc::Status status = _stream->Finish();
            if (status.ok())
                spdlog::info("node A stream closed OK");
            else
                spdlog::warn("node A stream error: {}", status.error_message());
        } else
            spdlog::warn("node A connection failed (retry {}/{})", attempt + 1, _connect_attemps);

        if (is_running.load()) {
            spdlog::info("node A waiting {}ms before retry...", _connect_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(_connect_delay_ms));
        }
    }
}

void NodeA::write_loop(AccelEmulator& emu, std::atomic<bool>& is_running) {
    while (is_running.load()) {
        AccelPacket pkt = emu.generate();
        pkt.set_version(CURRENT_MESSAGE_VERSION);

        if (!_stream->Write(pkt)) {
            spdlog::warn("node A write failed: server likely closed connection");
            break;
        }

        spdlog::info("[A->S] sent packet: ts={} x={:.3f} y={:.3f} z={:.3f}", pkt.timestamp(), pkt.x(), pkt.y(), pkt.z());
        emu.wait_for_next_sample();
    }
    _stream->WritesDone();
    spdlog::info("node A finished writing");
}

void NodeA::read_loop(std::atomic<bool>& is_running) {
    AccelModule mod;
    std::ofstream log_file(RESULT_DIR RESULT_FILE, std::ios::app);

    while (is_running.load() && _stream->Read(&mod)) {
        if (mod.version() > CURRENT_MESSAGE_VERSION)
            spdlog::warn("recieved partially supporting version {}", mod.version());
        spdlog::info("[S->A] received module: ts={} val={:.3f}", mod.timestamp(), mod.module());

        if (log_file.is_open())
            log_file << mod.timestamp() << " " << mod.module() << std::endl;
    }

    spdlog::warn("node A read loop ended: stream closed by server");
    is_running.store(false);
}
