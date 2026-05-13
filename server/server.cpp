#include <spdlog/spdlog.h>
#include <cmath>
#include "server.h"

extern std::atomic<bool> is_running;

CallA::CallA(grpc::CallbackServerContext* ctx, Service* srv)
    : _ctx(ctx), _srv(srv), _cancelled(false) {}

void CallA::Cancel() {
    _cancelled = true;
    Finish(grpc::Status::OK); 
}

void CallA::Start() {
    spdlog::info("node A connected");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.call_a = this;
    }
    if (!_cancelled) StartRead(&_read_pkt);
}

void CallA::SendModule(const AccelModule& mod) {
    spdlog::info("[S->A] sending module: ts={} val={:.3f}", mod.timestamp(), mod.module());
    _write_mod = mod;
    StartWrite(&_write_mod);
}

void CallA::OnReadDone(bool ok) {
    if (!ok || _finished || _cancelled || !is_running.load()) {
        if (!_finished) { _finished = true; spdlog::warn("node A read stream closed or network error"); Finish(grpc::Status::OK); }
        return;
    }

    if (_read_pkt.version() > CURRENT_MESSAGE_VERSION)
        spdlog::warn("recieved partially supporting version {}", _read_pkt.version());
    spdlog::info("[A->S] received packet: ts={} | x={:.3f} y={:.3f} z={:.3f}", _read_pkt.timestamp(), _read_pkt.x(), _read_pkt.y(), _read_pkt.z());

    if (!_srv->IsDuplicate(_read_pkt)) {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.latest_pkt = _read_pkt;
        _srv->_state.has_latest = true;
    } else {
        spdlog::warn("[FILTER] duplicate skipped: ts={}", _read_pkt.timestamp());
    }
    _srv->TryTriggerBWrite();
    StartRead(&_read_pkt);
}

void CallA::OnWriteDone(bool ok) {
    if (!ok || _finished || _cancelled || !is_running.load()) {
        if (!_finished) { _finished = true; spdlog::warn("node A write failed (client disconnected or buffer overflow)"); Finish(grpc::Status::OK); }
        return;
    }
}

void CallA::OnDone() {
    spdlog::info("node A stream end, freeing resources");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        if (_srv->_state.call_a == this) _srv->_state.call_a = nullptr;
    }
    _finished = true;
    delete this;
}

CallB::CallB(grpc::CallbackServerContext* ctx, Service* srv)
    : _ctx(ctx), _srv(srv), _cancelled(false) {}

void CallB::Cancel() {
    _cancelled = true;
    Finish(grpc::Status::OK);
}

void CallB::Start() {
    spdlog::info("node B connected");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.call_b = this;
    }
    if (!_cancelled) _srv->TryTriggerBWrite();
}

void CallB::OnReadDone(bool ok) {
    if (!ok || _finished || _cancelled || !is_running.load()) {
        if (!_finished) { _finished = true; spdlog::warn("node B read stream closed or network error"); Finish(grpc::Status::OK); }
        return;
    }

    if (_read_mod.version() > CURRENT_MESSAGE_VERSION)
        spdlog::warn("recieved partially supporting version {}", _read_mod.version());
    spdlog::info("[B->S] received module: ts={} val={:.3f}", _read_mod.timestamp(), _read_mod.module());

    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        if (_srv->_state.call_a) _srv->_state.call_a->SendModule(_read_mod);
        else spdlog::warn("[BRIDGE] node A disconnected");
    }
    _srv->TryTriggerBWrite();
}

void CallB::OnWriteDone(bool ok) {
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        _srv->_state.b_is_busy = false;
        if (!ok || _finished || _cancelled || !is_running.load()) {
            if (!_finished) { _finished = true; spdlog::warn("node B write failed or shutdown"); Finish(grpc::Status::OK); }
            return;
        }
    }
    spdlog::info("[S->B] packet delivered");
    StartRead(&_read_mod);
}

void CallB::OnDone() {
    spdlog::info("node B stream terminated, freeing resources");
    {
        std::lock_guard<std::mutex> lock(_srv->_state.mtx);
        if (_srv->_state.call_b == this) _srv->_state.call_b = nullptr;
    }
    _finished = true;
    delete this;
}

Service::Service(std::string api_key) : _api_key(std::move(api_key)) {}

bool Service::CheckApiKey(grpc::CallbackServerContext* context) {
    const auto& metadata = context->client_metadata();
    auto it = metadata.find("x-api-key");
    return (it != metadata.end() && it->second == _api_key);
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

void Service::TryTriggerBWrite() {
    if (!is_running.load()) return;

    CallB* target = nullptr;
    AccelPacket pkt;
    bool should_write = false;

    {
        std::lock_guard<std::mutex> lock(_state.mtx);
        if (!_state.call_b || _state.b_is_busy || !_state.has_latest) return;

        pkt = _state.latest_pkt;
        _state.has_latest = false;
        _state.b_is_busy = true;
        target = _state.call_b;
        should_write = true;
    }
    if (should_write) {
        target->_write_pkt = pkt;
        spdlog::info("[BRIDGE] forwarding to B: ts={}", pkt.timestamp());
        target->StartWrite(&target->_write_pkt);
    }
}

bool Service::IsDuplicate(const AccelPacket& pkt) {
    std::lock_guard<std::mutex> lock(_state.mtx);
    if (!_state.has_latest) return false;
    const auto& last = _state.latest_pkt;
    return std::abs(pkt.x() - last.x()) < EPS &&
           std::abs(pkt.y() - last.y()) < EPS &&
           std::abs(pkt.z() - last.z()) < EPS;
}

void Service::Shutdown() {
    CallA* a = nullptr;
    CallB* b = nullptr;
    {
        std::lock_guard<std::mutex> lock(_state.mtx);
        a = _state.call_a;
        b = _state.call_b;
    }
    if (a) a->Cancel();
    if (b) b->Cancel();
}
