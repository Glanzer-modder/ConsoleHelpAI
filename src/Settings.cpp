#include "PCH.h"
#include "Settings.h"
#include <Windows.h>

namespace logger = SKSE::log;

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    Settings::Config g_config{};

    std::filesystem::path GetIniPath()
    {
        // Assumes the DLL lives beside the ini in Data/SKSE/Plugins.
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), modulePath, MAX_PATH) == 0) {
            return L"ConsoleHelpAI.ini";
        }

        return std::filesystem::path(modulePath).parent_path() / L"ConsoleHelpAI.ini";
    }

    std::string Trim(std::string value)
    {
        auto notSpace = [](unsigned char c) { return !std::isspace(c); };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    std::string ReadString(const wchar_t* section, const wchar_t* key, const std::string& fallback, const std::filesystem::path& iniPath)
    {
        wchar_t buffer[4096]{};
        std::wstring fallbackW(fallback.begin(), fallback.end());
        GetPrivateProfileStringW(section, key, fallbackW.c_str(), buffer, static_cast<DWORD>(std::size(buffer)), iniPath.c_str());
        std::wstring resultW(buffer);
        return std::string(resultW.begin(), resultW.end());
    }

    std::uint32_t ReadUInt(const wchar_t* section, const wchar_t* key, std::uint32_t fallback, const std::filesystem::path& iniPath)
    {
        return GetPrivateProfileIntW(section, key, fallback, iniPath.c_str());
    }

    bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback, const std::filesystem::path& iniPath)
    {
        const std::string value = Trim(ReadString(section, key, fallback ? "true" : "false", iniPath));
        if (value == "true" || value == "1") return true;
        if (value == "false" || value == "0") return false;
        return fallback;
    }
}

namespace Settings
{
    void Load()
    {
        const auto iniPath = GetIniPath();

        logger::info("Settings::Load() called, ini path: {}", iniPath.string());

        g_config.enabled = ReadBool(L"General", L"Enabled", g_config.enabled, iniPath);
        g_config.prefix = Trim(ReadString(L"General", L"Prefix", g_config.prefix, iniPath));
        g_config.timeoutMs = ReadUInt(L"General", L"TimeoutMs", g_config.timeoutMs, iniPath);
        g_config.maxResponseChars = ReadUInt(L"General", L"MaxResponseChars", static_cast<std::uint32_t>(g_config.maxResponseChars), iniPath);
        g_config.logLevel = Trim(ReadString(L"General", L"LogLevel", g_config.logLevel, iniPath));

        g_config.systemPrompt = ReadString(L"Prompt", L"SystemPrompt", g_config.systemPrompt, iniPath);
        g_config.userPromptTemplate = ReadString(L"Prompt", L"UserTemplate", g_config.userPromptTemplate, iniPath);
        g_config.temperature = static_cast<float>(ReadUInt(L"Tuning", L"Temperature",
            static_cast<std::uint32_t>(g_config.temperature * 100.0f), iniPath)) / 100.0f;
        g_config.maxTokens = ReadUInt(L"Tuning", L"MaxTokens", g_config.maxTokens, iniPath);
        g_config.supportsReasoning = ReadBool(L"Tuning", L"SupportsReasoning", g_config.supportsReasoning, iniPath);

        g_config.providerName = Trim(ReadString(L"Provider", L"Name", g_config.providerName, iniPath));
        g_config.providerUrl = Trim(ReadString(L"Provider", L"Url", g_config.providerUrl, iniPath));
        g_config.providerModel = Trim(ReadString(L"Provider", L"Model", g_config.providerModel, iniPath));
        g_config.apiKeyEnvVar = Trim(ReadString(L"Provider", L"ApiKeyEnvVar", g_config.apiKeyEnvVar, iniPath));

        g_config.msgThinking = ReadString(L"Messages", L"Thinking", g_config.msgThinking, iniPath);
        g_config.msgTimeout = ReadString(L"Messages", L"Timeout", g_config.msgTimeout, iniPath);
        g_config.msgEmptyQuery = ReadString(L"Messages", L"EmptyQuery", g_config.msgEmptyQuery, iniPath);
        g_config.msgRequestFailed = ReadString(L"Messages", L"RequestFailed", g_config.msgRequestFailed, iniPath);

        logger::info("=== ConsoleHelpAI Settings from the ConsoleHelpAI.ini file ===");
        logger::info("[General]");
        logger::info("  Enabled        = {}", g_config.enabled);
        logger::info("  Prefix         = [{}]", g_config.prefix);
        logger::info("  TimeoutMs      = {}", g_config.timeoutMs);
        logger::info("  MaxResponseChars = {}", g_config.maxResponseChars);
        logger::info("  LogLevel       = {}", g_config.logLevel);
        logger::info("[Prompt]");
        logger::info("  SystemPrompt   = {}", g_config.systemPrompt);
        logger::info("  UserTemplate   = {}", g_config.userPromptTemplate);
        logger::info("[Tuning]");
        logger::info("  Temperature    = {:.2f}", g_config.temperature);
        logger::info("  MaxTokens      = {}", g_config.maxTokens);
        logger::info("  SupportsReasoning = {}", g_config.supportsReasoning);
        logger::info("[Provider]");
        logger::info("  Name           = {}", g_config.providerName);
        logger::info("  Url            = {}", g_config.providerUrl);
        logger::info("  Model          = {}", g_config.providerModel);
        logger::info("  ApiKeyEnvVar   = {}", g_config.apiKeyEnvVar);
        logger::info("[Messages]");
        logger::info("  Thinking       = {}", g_config.msgThinking);
        logger::info("  Timeout        = {}", g_config.msgTimeout);
        logger::info("  EmptyQuery     = {}", g_config.msgEmptyQuery);
        logger::info("  RequestFailed  = {}", g_config.msgRequestFailed);
        logger::info("==============================");
    }

    const Config& Get()
    {
        return g_config;
    }
}
