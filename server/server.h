#pragma once
#include <grpcpp/grpcpp.h>
#include <memory>
#include <mutex>
#include <yaml-cpp/yaml.h>
#include "accel.grpc.pb.h"
#include "accel.pb.h"

#define CURRENT_MESSAGE_VERSION 1
#define EPS 1e-3f

class Service;
class CallA;
class CallB;

struct ServiceState {
    std::mutex mtx;
    CallA* call_a = nullptr;
    CallB* call_b = nullptr;

    AccelPacket latest_pkt;
    bool has_latest = false;
    bool b_is_busy = false;
};

class CallA : public grpc::ServerBidiReactor<AccelPacket, AccelModule> {
public:
    CallA(grpc::CallbackServerContext* ctx, Service* srv);
    void Start();
    void SendModule(const AccelModule& mod);
    void Cancel();

private:
    friend class Service;
    void OnReadDone(bool ok) override;
    void OnWriteDone(bool ok) override;
    void OnDone() override;

    grpc::CallbackServerContext* _ctx;
    Service* _srv;
    AccelPacket _read_pkt;
    AccelModule _write_mod;
    bool _finished = false;
    bool _cancelled = false;
};

class CallB : public grpc::ServerBidiReactor<AccelModule, AccelPacket> {
public:
    CallB(grpc::CallbackServerContext* ctx, Service* srv);
    void Start();
    void Cancel();

private:
    friend class Service;
    void OnReadDone(bool ok) override;
    void OnWriteDone(bool ok) override;
    void OnDone() override;

    grpc::CallbackServerContext* _ctx;
    Service* _srv;
    AccelModule _read_mod;
    AccelPacket _write_pkt;
    bool _finished = false;
    bool _cancelled = false;
};

class Service : public AccelerometerService::CallbackService {
public:
    explicit Service(std::string api_key);
    grpc::ServerBidiReactor<AccelPacket, AccelModule>* StreamAccelDataA(grpc::CallbackServerContext* context) override;
    grpc::ServerBidiReactor<AccelModule, AccelPacket>* StreamAccelDataB(grpc::CallbackServerContext* context) override;

    void TryTriggerBWrite();
    bool IsDuplicate(const AccelPacket& pkt);
    bool CheckApiKey(grpc::CallbackServerContext* context);
    void Shutdown();

private:
    friend class CallA;
    friend class CallB;
    ServiceState _state;
    std::string _api_key;
};
