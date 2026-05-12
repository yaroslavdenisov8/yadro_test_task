#include <spdlog/spdlog.h>

#include "node_b.h"

NodeB::NodeB(const std::string& server_address, int connect_attemps, int connect_delay_ms, std::string api_key)
: _connect_attemps(connect_attemps), _connect_delay_ms(connect_delay_ms), _api_key(std::move(api_key))
{
    grpc::SslCredentialsOptions ssl_opts;
    ssl_opts.pem_root_certs = read_file(CERT_DIR "ca.crt");
    ssl_opts.pem_private_key = read_file(CERT_DIR "client.key");
    ssl_opts.pem_cert_chain = read_file(CERT_DIR "client.crt");

    auto channel = grpc::CreateChannel(server_address, grpc::SslCredentials(ssl_opts));
    _stub = AccelerometerService::NewStub(channel);
    spdlog::info("node B initialized, target: {}", server_address);
}

bool NodeB::connect() {
    _context = std::make_unique<grpc::ClientContext>();
    _context->AddMetadata("x-api-key", _api_key);

    _stream = _stub->StreamAccelDataB(_context.get());
    if (!_stream) {
        spdlog::error("failed to create gRPC stream to server");
        return false;
    }
    spdlog::info("node B created server stream");
    return true;
}

void NodeB::run(std::atomic<bool>& is_running) {
    for (int attempt = 0; attempt < _connect_attemps && is_running.load(); ++attempt) {
        if (connect()) {
            AccelPacket pkt;
            AccelModule mod;

            while (is_running.load() && _stream->Read(&pkt)) {
                if (pkt.version() > CURRENT_MESSAGE_VERSION)
                    spdlog::warn("recieved partially supporting version {}", pkt.version());
                spdlog::info("[S->B] received packet: ts={} x={:.3f} y={:.3f} z={:.3f}", pkt.timestamp(), pkt.x(), pkt.y(), pkt.z());

                mod = response(pkt);
                spdlog::info("[B] computed module: ts={} val={:.3f}", mod.timestamp(), mod.module());

                if (!_stream->Write(mod)) {
                    spdlog::warn("node B write failed: server likely closed connection");
                    break;
                }
                spdlog::info("[B->S] sent module: ts={} val={:.3f}", mod.timestamp(), mod.module());
            }

            spdlog::warn("node B read loop ended: stream closed by server");
            _stream->WritesDone();

            grpc::Status status = _stream->Finish();
            if (status.ok())
                spdlog::info("node B stream closed OK");
            else
                spdlog::warn("node B stream error: {}", status.error_message());
        } else
            spdlog::warn("node A connection failed (retry {}/{})", attempt + 1, _connect_attemps);

        if (is_running.load()) {
            spdlog::info("node A waiting {}ms before retry...", _connect_delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(_connect_delay_ms));
        }
    }
}

AccelModule NodeB::response(const AccelPacket &pkt) {
    AccelModule mod;
    float sum = std::pow(pkt.x(), 2) + std::pow(pkt.y(), 2) + std::pow(pkt.z(), 2);
    float module = std::sqrt(sum);
    mod.set_module(module);
    mod.set_timestamp(pkt.timestamp());
    mod.set_version(CURRENT_MESSAGE_VERSION);
    return mod;
}

void NodeB::cancel() {
    if (_context) _context->TryCancel();
}
