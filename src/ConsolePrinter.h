#pragma once

#include <format>
#include <string>

namespace ConsolePrinter
{
    void PrintRaw(std::string text);

    template <class... Args>
    void Print(std::format_string<Args...> fmt, Args&&... args)
    {
        PrintRaw(std::format(fmt, std::forward<Args>(args)...));
    }
}
