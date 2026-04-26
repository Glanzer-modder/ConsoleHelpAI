#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <SKSE/Logger.h>

#pragma warning(push)
#include <spdlog/sinks/basic_file_sink.h>
#pragma warning(pop)

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

using namespace std::literals;
