#include <spdlog/spdlog.h>
#include <cmath>
#include "server.h"


Service::Service(std::string api_key) : _api_key(std::move(api_key)) {}

CallA::CallA(grpc::CallbackServerContext* ctx, Service* srv)
    : _ctx(ctx), _srv(srv) {}

void CallA::Start() {
    spdlog::info("node A connected");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.call_a = this;
    }
    StartRead(&_read_pkt);
}

void CallA::SendModule(const AccelModule& mod) {
    spdlog::info("[S->A] sending module: ts={} val={:.3f}", mod.timestamp(), mod.module());
    _write_mod = mod;
    StartWrite(&_write_mod);
}

void CallA::OnReadDone(bool ok) {
    if (!ok) {
        if (!_finished) {
            _finished = true;
            spdlog::warn("node A read stream closed or network error");
            Finish(grpc::Status::OK);
        }
        return;
    }
    if (_finished) return;

    if (_read_pkt.version() > CURRENT_MESSAGE_VERSION)
        spdlog::warn("recieved partially supporting version {}", _read_pkt.version());
    spdlog::info("[A->S] received packet: ts={} | x={:.3f} y={:.3f} z={:.3f}", _read_pkt.timestamp(), _read_pkt.x(), _read_pkt.y(), _read_pkt.z());

    if (!_srv->IsDuplicate(_read_pkt)) {
        _srv->PushToQueue(_read_pkt);
        _srv->TryTriggerBWrite();
    } else {
        spdlog::warn("[FILTER] duplicate skipped: ts={}", _read_pkt.timestamp());
    }
    StartRead(&_read_pkt);
}

void CallA::OnWriteDone(bool ok) {
    if (!ok) {
        if (!_finished) {
            _finished = true;
            spdlog::warn("node A write failed (client disconnected or buffer overflow)");
            Finish(grpc::Status::OK);
        }
        return;
    }
}

void CallA::OnDone() {
    spdlog::info("node A stream end, freeing resources");
    _finished = true;
    delete this;
}

CallB::CallB(grpc::CallbackServerContext* ctx, Service* srv)
    : _ctx(ctx), _srv(srv), _write_pending(false) {}

void CallB::Start() {
    spdlog::info("node B connected");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.call_b = this;
    }
    _srv->TryTriggerBWrite();
}

void CallB::OnReadDone(bool ok) {
    if (!ok) {
        if (!_finished) {
            _finished = true;
            spdlog::warn("node B read stream closed or network error");
            Finish(grpc::Status::OK);
        }
        return;
    }
    if (_finished) return;

    if (_read_mod.version() > CURRENT_MESSAGE_VERSION)
        spdlog::warn("recieved partially supporting version {}", _read_mod.version());
    spdlog::info("[B->S] received module: ts={} val={:.3f}",
                 _read_mod.timestamp(), _read_mod.module());

    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        if (_srv->_state.call_a) {
            _srv->_state.call_a->SendModule(_read_mod);
        } else {
            spdlog::warn("[BRIDGE] node A disconnected");
        }
    }
    _srv->TryTriggerBWrite();
}

void CallB::OnWriteDone(bool ok) {
    _write_pending = false;
    if (!ok) {
        if (!_finished) {
            _finished = true;
            spdlog::warn("node B write failed");
            Finish(grpc::Status::OK);
        }
        return;
    }
    spdlog::info("[S->B] packet delivered");
    StartRead(&_read_mod);
}

void CallB::OnDone() {
    spdlog::info("node B stream terminated, freeing resources");
    _finished = true;
    delete this;
}

bool Service::CheckApiKey(grpc::CallbackServerContext* context) {
    const auto& metadata = context->client_metadata();
    auto it = metadata.find("x-api-key");
    if (it == metadata.end() || it->second != _api_key)
        return false;
    return true;
}

grpc::ServerBidiReactor<AccelPacket, AccelModule>*
Service::StreamAccelDataA(grpc::CallbackServerContext* context) {
    if (!CheckApiKey(context)) {
        spdlog::error("Security Alert: Unauthorized Node A access attempt!");
        auto* reactor = new CallA(context, this);
        reactor->Finish(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid API Key"));
        return reactor;
    }

    auto* call = new CallA(context, this);
    call->Start();
    return call;
}

grpc::ServerBidiReactor<AccelModule, AccelPacket>*
Service::StreamAccelDataB(grpc::CallbackServerContext* context) {
    if (!CheckApiKey(context)) {
        spdlog::error("Security Alert: Unauthorized Node B access attempt!");
        auto* reactor = new CallB(context, this);
        reactor->Finish(grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid API Key"));
        return reactor;
    }

    auto* call = new CallB(context, this);
    call->Start();
    return call;
}

void Service::PushToQueue(const AccelPacket& pkt) {
    std::lock_guard<std::mutex> lock(_state.mtx);
    _state.packet_queue.push(pkt);
}

void Service::TryTriggerBWrite() {
    std::lock_guard<std::mutex> lock(_state.mtx);

    if (!_state.call_b) {
        spdlog::warn("[BRIDGE] can't forward: B not connected");
        return;
    }
    if (_state.call_b->_write_pending) return;
    if (_state.packet_queue.empty()) return;

    auto pkt = _state.packet_queue.front();
    _state.packet_queue.pop();

    _state.call_b->_write_pkt = pkt;
    _state.call_b->_write_pending = true;
    spdlog::info("[QUEUE] forwarding to B: ts={} queue_size={}", pkt.timestamp(), _state.packet_queue.size());
    _state.call_b->StartWrite(&_state.call_b->_write_pkt);
}

bool Service::IsDuplicate(const AccelPacket& pkt) {
    if (!_state.has_last) {
        _state.last_pkt = pkt;
        _state.has_last = true;
        return false;
    }
    bool dup = std::abs(pkt.x() - _state.last_pkt.x()) < EPS &&
               std::abs(pkt.y() - _state.last_pkt.y()) < EPS &&
               std::abs(pkt.z() - _state.last_pkt.z()) < EPS;
    _state.last_pkt = pkt;
    return dup;
}
