#include "PCH.h"
#include "ConsolePrinter.h"

namespace ConsolePrinter
{
    void PrintRaw(std::string text)
    {
        SKSE::GetTaskInterface()->AddTask([text = std::move(text)]() {
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print(text.c_str());
            }
        });
    }
}
