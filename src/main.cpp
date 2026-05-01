#include "PCH.h"
#include "Plugin.h"
#include "ConsoleHook.h"
#include "Settings.h"

namespace logger = SKSE::log;

namespace
{
    // Declares plugin metadata to SKSE.
    // UsesAddressLibrary: required since we use REL::Relocation for hook addresses.
    // HasNoStructUse: we don't use SKSE's struct layout assumptions.
    constexpr SKSE::PluginVersionData GetPluginVersion()
    {
        SKSE::PluginVersionData v{};
        v.PluginName(Plugin::NAME);
        v.PluginVersion(1);
        v.AuthorName("Glanzer");
        v.UsesAddressLibrary(true);
        v.HasNoStructUse(true);
        return v;
    }

    void InitializeLogging()
    {
        // Determine the correct documents folder name based on which
        // platform DLLs are present, matching the approach used by
        // MediaKeysFix to avoid Address Library lookup issues on AE.
        PWSTR buf{ nullptr };
        ::SHGetKnownFolderPath(::FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &buf);
        std::unique_ptr<wchar_t, decltype(&::CoTaskMemFree)> documentsPath{ buf, ::CoTaskMemFree };

        if (!documentsPath) {
            SKSE::stl::report_and_fail("Unable to find documents folder.");
        }

        std::filesystem::path path{ documentsPath.get() };
        path /= "My Games"sv;

        if SKYRIM_REL_VR_CONSTEXPR(REL::Module::IsVR()) {
            path /= "Skyrim VR"sv;
        }
        else if (std::filesystem::exists("steam_api64.dll"sv)) {
            path /= "Skyrim Special Edition"sv;
        }
        else if (std::filesystem::exists("Galaxy64.dll"sv)) {
            path /= "Skyrim Special Edition GOG"sv;
        }
        else if (std::filesystem::exists("eossdk-win64-shipping.dll"sv)) {
            path /= "Skyrim Special Edition EPIC"sv;
        }
        else {
            path /= "Skyrim Special Edition"sv;  // safe fallback
        }

        path /= "SKSE"sv;
        path /= std::format("{}.log", Plugin::NAME);

        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    }

    void MessageHandler(SKSE::MessagingInterface::Message* msg)
    {
        if (!msg) {
            return;
        }

        if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
            Settings::Load();
            ConsoleHook::Install();
            logger::info("{} {} installed.", Plugin::NAME, Plugin::VERSION);
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version{ GetPluginVersion() };

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(
    const SKSE::QueryInterface* a_skse,
    SKSE::PluginInfo* a_info)
{
    if (!a_skse || !a_info) {
        return false;
    }

    // Prevent loading in the Creation Kit
    if (a_skse->IsEditor()) {
        logger::critical("Loaded in editor, marking as incompatible.");
        return false;
    }

    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = Plugin::NAME;
    a_info->version = 1;

    return true;
}

extern "C" __declspec(dllexport) bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
    InitializeLogging();
    logger::info("Loading {} {}", Plugin::NAME, Plugin::VERSION);

    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1 << 10);

    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Failed to acquire SKSE messaging interface.");
        return false;
    }

    if (!messaging->RegisterListener(MessageHandler)) {
        logger::critical("Failed to register SKSE message listener.");
        return false;
    }

    return true;
}
