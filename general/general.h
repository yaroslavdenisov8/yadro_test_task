#pragma once

#include <fstream>
#include <spdlog/spdlog.h>

#define CONFIG_FILE "config.yaml"
#define RESULT_DIR "./accel/"
#define RESULT_FILE "module.log"
#define CERT_DIR "build/certs/"
#define LOG_PATTERN "[%H:%M:%S.%e] [%^%l%$] %v"
#define LOG_LEVEL spdlog::level::info
#define EPS 1e-3f

std::string read_file(const std::string& path);

