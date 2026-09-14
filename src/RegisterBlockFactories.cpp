#include <PySysLinkBase/IBlockFactory.h>
#include "BlockFactoryFMU.h"
#include "spdlog/spdlog.h"
#include <iostream>
#include "LoggerInstance.h"

extern "C" void RegisterBlockFactories(std::map<std::string, std::shared_ptr<PySysLinkBase::IBlockFactory>>& registry, std::map<std::string, PySysLinkBase::ConfigurationValue> pluginConfiguration) {
    std::cout << "Call to RegisterBlockFactories" << std::endl;
    registry["BasicFMU"] = std::make_shared<BlockTypeSupports::BasicFMUSupport::BlockFactoryFMU>(pluginConfiguration);
    std::cout << "End of function" << std::endl;
}

extern "C" void RegisterSpdlogLogger(std::shared_ptr<spdlog::logger> logger) {
    BlockTypeSupports::BasicFMUSupport::LoggerInstance::SetLogger(logger);
    BlockTypeSupports::BasicFMUSupport::LoggerInstance::GetLogger()->debug("Logger from plugin BlockTypeSupportsBasicFMU!");
}
