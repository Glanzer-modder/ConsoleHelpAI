#include "PCH.h"
#include "AiClient.h"
#include "Settings.h"

#include <Windows.h>
#include <winhttp.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace logger = SKSE::log;

namespace
{
    // Read an environment variable safely
    std::string ReadEnvVar(const std::string& name)
    {
        char* value = nullptr;
        std::size_t len = 0;
        if (_dupenv_s(&value, &len, name.c_str()) != 0 || value == nullptr) {
            return {};
        }
        std::string result(value);
        free(value);
        return result;
    }

    // Returns the API key path next to the DLL
    std::filesystem::path GetKeyFilePath()
    {
        wchar_t modulePath[MAX_PATH]{};
        if (GetModuleFileNameW(
            reinterpret_cast<HMODULE>(&__ImageBase),
            modulePath, MAX_PATH) == 0) {
            return L"ConsoleHelpAI_Key.ini";
        }
        return std::filesystem::path(modulePath).parent_path()
            / L"ConsoleHelpAI_Key.ini";
    }

    // Reads the API key using a two-step fallback chain:
    // 1. Check ConsoleHelpAI_Key.ini for a non-blank ApiKey value
    // 2. Fall back to the environment variable named in cfg.apiKeyEnvVar
    // Logs which source the key came from without exposing the key value.
    std::string ReadApiKey(const Settings::Config& cfg)
    {
        // Step 1 - check the key file
        const auto keyFilePath = GetKeyFilePath();
        wchar_t buffer[512]{};
        GetPrivateProfileStringW(
            L"ApiKey",
            L"ApiKey",
            L"",
            buffer,
            static_cast<DWORD>(std::size(buffer)),
            keyFilePath.c_str());

        // Trim whitespace from the result
        std::wstring resultW(buffer);
        const auto notSpace = [](wchar_t c) { return !std::iswspace(c); };
        resultW.erase(resultW.begin(),
            std::find_if(resultW.begin(), resultW.end(), notSpace));
        resultW.erase(
            std::find_if(resultW.rbegin(), resultW.rend(), notSpace).base(),
            resultW.end());

        if (!resultW.empty()) {
            const std::string key(resultW.begin(), resultW.end());
            logger::info("API key source: key file ({})",
                keyFilePath.string());
            return key;
        }

        // Step 2 - fall back to environment variable
        const std::string key = ReadEnvVar(cfg.apiKeyEnvVar);
        if (!key.empty()) {
            logger::info("API key source: environment variable ({})",
                cfg.apiKeyEnvVar);
            return key;
        }

        // Neither source had a key
        return {};
    }

    // Escape a string for safe inclusion in a JSON value
    std::string JsonEscape(const std::string& input)
    {
        std::string output;
        output.reserve(input.size());
        for (unsigned char c : input) {
            switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n";  break;
            case '\r': output += "\\r";  break;
            case '\t': output += "\\t";  break;
            default:
                if (c < 0x20) {
                    // Control characters
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    output += buf;
                }
                else {
                    output += static_cast<char>(c);
                }
                break;
            }
        }
        return output;
    }

    // Build the JSON request body for the chat completions API
    // Compatible with Groq, Google Gemini, SambaNova, and OpenRouter
    std::string BuildRequestBody(const std::string& userQuery)
    {
        const auto& cfg = Settings::Get();

        // Build user message from template
        std::string userContent = cfg.userPromptTemplate;
        const auto pos = userContent.find("{query}");
        if (pos != std::string::npos) {
            userContent.replace(pos, 7, userQuery);
        }
        else {
            userContent += userQuery;
        }

        // Format temperature as string with 2 decimal places
        char tempStr[16];
        snprintf(tempStr, sizeof(tempStr), "%.2f", cfg.temperature);

        // Use MaxTokens if set, otherwise auto-calculate from MaxResponseChars / 4
        const auto maxTokens = cfg.maxTokens > 0
            ? cfg.maxTokens
            : static_cast<std::uint32_t>(std::max(std::size_t(1), cfg.maxResponseChars / 4));

        return std::string("{")
            + "\"model\":\"" + JsonEscape(cfg.providerModel) + "\","
            + "\"max_tokens\":" + std::to_string(maxTokens) + ","
            + "\"temperature\":" + tempStr + ","
            + (cfg.supportsReasoning ? "\"reasoning\":{\"exclude\":true}," : "")
            + "\"messages\":["
            + "{\"role\":\"system\",\"content\":\"" + JsonEscape(cfg.systemPrompt) + "\"},"
            + "{\"role\":\"user\",\"content\":\"" + JsonEscape(userContent) + "\"}"
            + "]}";
    }

    // Parse the response text from a chat completions JSON response
    // Looks for: "content":"<text>"
    std::string ParseResponseText(const std::string& json)
    {
        // Find "content":"
        const std::string marker = "\"content\":\"";
        const auto pos = json.find(marker);
        if (pos == std::string::npos) {
            // Check if content is explicitly null - reasoning models do this
            // when the token budget is exhausted before producing a response
            if (json.find("\"content\":null") != std::string::npos) {
                logger::warn("ParseResponseText: content is null - "
                    "model may have exhausted token budget during reasoning. "
                    "Try increasing MaxTokens in ConsoleHelpAI.ini.");
                return {};
            }
            logger::warn("ParseResponseText: could not find content marker");
            return {};
        }

        const auto start = pos + marker.size();
        std::string result;
        result.reserve(256);

        // Walk forward, handling JSON escape sequences
        for (std::size_t i = start; i < json.size(); ++i) {
            const char c = json[i];
            if (c == '"') {
                break;  // end of string value
            }
            if (c == '\\' && i + 1 < json.size()) {
                ++i;
                switch (json[i]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case '/':  result += '/';  break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                case 'u':
                    // Skip unicode escapes - just skip 4 hex digits
                    i += 4;
                    break;
                default:
                    result += json[i];
                    break;
                }
            }
            else {
                result += c;
            }
        }

        return result;
    }

    // Split a URL into host and path components
    bool ParseUrl(const std::string& url,
        std::wstring& outHost,
        std::wstring& outPath,
        bool& outHttps,
        INTERNET_PORT& outPort)
    {
        // Determine scheme
        std::string remainder;
        if (url.substr(0, 8) == "https://") {
            outHttps = true;
            outPort = INTERNET_DEFAULT_HTTPS_PORT;
            remainder = url.substr(8);
        }
        else if (url.substr(0, 7) == "http://") {
            outHttps = false;
            outPort = INTERNET_DEFAULT_HTTP_PORT;
            remainder = url.substr(7);
        }
        else {
            logger::error("ParseUrl: unrecognized scheme in '{}'", url);
            return false;
        }

        // Split host and path
        const auto slash = remainder.find('/');
        std::string host, path;
        if (slash == std::string::npos) {
            host = remainder;
            path = "/";
        }
        else {
            host = remainder.substr(0, slash);
            path = remainder.substr(slash);
        }

        // Convert to wide strings for WinHTTP
        outHost = std::wstring(host.begin(), host.end());
        outPath = std::wstring(path.begin(), path.end());
        return true;
    }

    AiClient::Result DoRequest(const std::string& userQuery)
    {
        const auto& cfg = Settings::Get();

        // Read API key
        const auto apiKey = ReadApiKey(cfg);
        if (apiKey.empty()) {
            logger::error("DoRequest: no API key found in key file or env var '{}'",
                cfg.apiKeyEnvVar);
            return { false, false, {},
                "API key not found. Add it to ConsoleHelpAI_Key.ini or set the "
                + cfg.apiKeyEnvVar + " environment variable." };
        }

        // Parse the provider URL
        std::wstring host, path;
        bool useHttps = true;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        if (!ParseUrl(cfg.providerUrl, host, path, useHttps, port)) {
            return { false, false, {}, "Invalid provider URL." };
        }

        const std::string body = BuildRequestBody(userQuery);
        logger::info("DoRequest: POST to {} (body {} bytes)", cfg.providerUrl, body.size());

        // Build Authorization header
        const std::string authHeader = "Authorization: Bearer " + apiKey;
        const std::wstring headers =
            L"Content-Type: application/json\r\n" +
            std::wstring(authHeader.begin(), authHeader.end()) + L"\r\n";

        // WinHTTP session
        HINTERNET hSession = WinHttpOpen(
            L"ConsoleHelpAI/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!hSession) {
            logger::error("DoRequest: WinHttpOpen failed ({})", GetLastError());
            return { false, false, {}, "WinHTTP session failed." };
        }

        // Set timeouts (connect, send, receive all use timeoutMs)
        const DWORD timeout = static_cast<DWORD>(cfg.timeoutMs);
        WinHttpSetTimeouts(hSession,
            static_cast<int>(timeout),  // resolve
            static_cast<int>(timeout),  // connect
            static_cast<int>(timeout),  // send
            static_cast<int>(timeout)); // receive

        // Connect
        HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
        if (!hConnect) {
            logger::error("DoRequest: WinHttpConnect failed ({})", GetLastError());
            WinHttpCloseHandle(hSession);
            return { false, false, {}, "WinHTTP connect failed." };
        }

        // Open request
        const DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect,
            L"POST",
            path.c_str(),
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            flags);
        if (!hRequest) {
            logger::error("DoRequest: WinHttpOpenRequest failed ({})", GetLastError());
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { false, false, {}, "WinHTTP open request failed." };
        }

        // Send request
        const BOOL sent = WinHttpSendRequest(
            hRequest,
            headers.c_str(),
            static_cast<DWORD>(-1L),
            const_cast<char*>(body.c_str()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0);

        if (!sent) {
            const DWORD err = GetLastError();
            const bool timedOut = (err == ERROR_WINHTTP_TIMEOUT);
            logger::error("DoRequest: WinHttpSendRequest failed ({})", err);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { false, timedOut, {}, "Request send failed." };
        }

        // Receive response
        if (!WinHttpReceiveResponse(hRequest, nullptr)) {
            const DWORD err = GetLastError();
            const bool timedOut = (err == ERROR_WINHTTP_TIMEOUT);
            logger::error("DoRequest: WinHttpReceiveResponse failed ({})", err);
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return { false, timedOut, {}, "No response received." };
        }

        // Check HTTP status code
        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX);

        logger::info("DoRequest: HTTP status {}", statusCode);

        // Read response body
        std::string responseBody;
        DWORD bytesAvailable = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            std::string chunk(bytesAvailable, '\0');
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest, chunk.data(),
                bytesAvailable, &bytesRead)) {
                responseBody.append(chunk.data(), bytesRead);
            }
        }

        // Clean up handles
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        logger::info("DoRequest: received {} bytes", responseBody.size());

        if (statusCode != 200) {
            logger::error("DoRequest: non-200 status. Body: {}", responseBody);

            if (statusCode == 429) {
                return { false, false, {},
                    "Rate limit reached - you have sent too many requests. Please wait a moment and try again. (HTTP 429)" };
            }
            if (statusCode == 401) {
                return { false, false, {},
                    "Authentication failed - your API key may be invalid or missing. (HTTP 401)" };
            }
            if (statusCode == 503) {
                return { false, false, {},
                    "AI service is temporarily unavailable. Please try again shortly. (HTTP 503)" };
            }

            return { false, false, {}, "Request failed (HTTP " +
                std::to_string(statusCode) + "). Check the ConsoleHelpAI.log for more information." };
        }

        // Parse the response
        std::string text = ParseResponseText(responseBody);
        if (text.empty()) {
            logger::error("DoRequest: empty content in response. Body: {}", responseBody);
            return { false, false, {}, "Empty response from AI." };
        }

        // Trim to max length
        if (text.size() > cfg.maxResponseChars) {
            text.resize(cfg.maxResponseChars);
            // Don't cut mid-word
            const auto lastSpace = text.rfind(' ');
            if (lastSpace != std::string::npos && lastSpace > cfg.maxResponseChars / 2) {
                text.resize(lastSpace);
            }
        }

        logger::info("DoRequest: response text: [{}]", text);
        return { true, false, std::move(text), {} };
    }
}

namespace AiClient
{
    void SubmitAsync(const std::string& userQuery, Callback callback)
    {
        std::thread([userQuery, callback = std::move(callback)]() mutable {
            Result result = DoRequest(userQuery);
            SKSE::GetTaskInterface()->AddTask(
                [result = std::move(result), callback = std::move(callback)]() mutable {
                    callback(std::move(result));
                });
            }).detach();
    }
}
