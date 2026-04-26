#include "PCH.h"
#include "ConsoleHook.h"
#include "AiClient.h"
#include "ConsolePrinter.h"
#include "Settings.h"

namespace logger = SKSE::log;

namespace
{
    using CompileAndRun_t = void(*)(RE::Script*, RE::ScriptCompiler*,
        RE::COMPILER_NAME, RE::TESObjectREFR*);
    REL::Relocation<CompileAndRun_t> g_original;

    bool StartsWithInsensitive(std::string_view value, std::string_view prefix)
    {
        if (value.size() < prefix.size()) {
            return false;
        }
        for (std::size_t i = 0; i < prefix.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(value[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i]))) {
                return false;
            }
        }
        return true;
    }

    std::string TrimLeft(std::string value)
    {
        const auto it = std::find_if(value.begin(), value.end(),
            [](unsigned char ch) { return !std::isspace(ch); });
        value.erase(value.begin(), it);
        return value;
    }

    // Intercepts all Skyrim console commands before they are executed.
    // Commands beginning with the configured prefix are routed to the AI;
    // all other commands are passed through to the vanilla handler unchanged.
    void HookedCompileAndRun(RE::Script* a_script,
        RE::ScriptCompiler* a_compiler,
        RE::COMPILER_NAME a_name,
        RE::TESObjectREFR* a_targetRef)
    {
        const auto& cfg = Settings::Get();
        const std::string command = (a_script && a_script->text) ? a_script->text : "";

        logger::info("Console command seen: [{}]", command);

        if (!cfg.enabled || !StartsWithInsensitive(command, cfg.prefix)) {
            logger::info("Passing through to vanilla console");
            g_original(a_script, a_compiler, a_name, a_targetRef);
            return;
        }

        std::string userQuery = TrimLeft(command.substr(cfg.prefix.size()));
        if (userQuery.empty()) {
            ConsolePrinter::PrintRaw(cfg.msgEmptyQuery);
            logger::info("Swallowed empty AI command");
            return;
        }

        logger::info("AI command intercepted: [{}]", userQuery);
        ConsolePrinter::PrintRaw(cfg.msgThinking);

        AiClient::SubmitAsync(userQuery, [](AiClient::Result result) {
            const auto& currentCfg = Settings::Get();
            if (result.timedOut) {
                ConsolePrinter::PrintRaw(currentCfg.msgTimeout);
                return;
            }
            if (!result.ok) {
                const std::string message =
                    result.errorMessage.empty()
                    ? currentCfg.msgRequestFailed
                    : (std::string("[AI] ") + result.errorMessage);
                ConsolePrinter::PrintRaw(message);
                return;
            }
            ConsolePrinter::PrintRaw(result.text);
            });

        logger::info("Swallowed AI-prefixed command");
    }
}

namespace ConsoleHook
{
    void Install()
    {
        logger::info("ConsoleHook::Install starting");

        // Call site hook - same approach as CCExtender (CommandPipe.cpp)
        // RELOCATION_ID(52065, 52952) = the containing function in SE/AE
        // OFFSET(0xE2, 0x52) = byte offset to the CALL instruction within it
        // This is a different call site from what DynDOLOD/CCExtender hook,
        // so there are no conflicts.
        REL::Relocation<std::uintptr_t> hookPoint{
            RELOCATION_ID(52065, 52952),
            REL::Relocate(0xE2, 0x52)
        };

        auto& trampoline = SKSE::GetTrampoline();
        g_original = trampoline.write_call<5>(hookPoint.address(), HookedCompileAndRun);

        logger::info("ConsoleHook::Install finished - call site hook installed");
    }
}
