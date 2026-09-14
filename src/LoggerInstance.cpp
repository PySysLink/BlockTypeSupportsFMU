#include "LoggerInstance.h"

namespace BlockTypeSupports::BasicFMUSupport
{
    std::shared_ptr<spdlog::logger> LoggerInstance::s_logger = nullptr;
} // namespace BlockTypeSupports::BasicFMUSupport
