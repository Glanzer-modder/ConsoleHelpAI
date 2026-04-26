#pragma once

#include <string>
#include <cstdint>

namespace Settings
{
    struct Config
    {
        bool enabled{ true };
        std::string prefix{ "ai:" };
        std::uint32_t timeoutMs{ 10000 };
        std::size_t maxResponseChars{ 600 };
        std::string logLevel{ "info" };
        float temperature{ 0.2f };
        std::uint32_t maxTokens{ 0 };  // 0 = auto-calculate from maxResponseChars / 4
        bool supportsReasoning{ false };  // set true for reasoning models (e.g. gpt-5.5 via OpenRouter)

        std::string systemPrompt{
            "You are a Skyrim SE expert on names, places, and console commands. "
            "Be concise and direct. For console commands, give the command first, then a brief one-sentence explanation. "
            "Never use introductory phrases. "
            "Never provide specific location editor IDs for the coc command - instead tell the user to look up "
            "the exact ID at https://en.uesp.net/wiki/Skyrim:Places or https://skyrimcommands.com. "
            "For weather IDs and formIDs also direct the user to https://skyrimcommands.com rather than guessing."
        };
        std::string userPromptTemplate{ "{query}" };

        std::string providerName{ "Gemini" };
        std::string providerUrl{ "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions" };
        std::string providerModel{ "gemini-2.5-flash-lite" };
        std::string apiKeyEnvVar{ "SKYRIM_AI_API_KEY" };

        std::string msgThinking{ "[AI] Thinking..." };
        std::string msgTimeout{ "[AI] Request timed out." };
        std::string msgEmptyQuery{ "[AI] Empty query." };
        std::string msgRequestFailed{ "[AI] Request failed." };
    };

    void Load();
    const Config& Get();
}
