#pragma once

#include <functional>
#include <string>

namespace AiClient
{
    struct Result
    {
        bool ok{ false };
        bool timedOut{ false };
        std::string text;
        std::string errorMessage;
    };

    using Callback = std::function<void(Result)>;

    void SubmitAsync(const std::string& userQuery, Callback callback);
}
