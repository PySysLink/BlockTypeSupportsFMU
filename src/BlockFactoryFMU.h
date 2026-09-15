#ifndef BLOCK_TYPE_SUPPORTS_FMU_BLOCK_FACTORY_H
#define BLOCK_TYPE_SUPPORTS_FMU_BLOCK_FACTORY_H

#include <PySysLinkBase/ConfigurationValue.h>
#include <PySysLinkBase/IBlockEventsHandler.h>
#include <PySysLinkBase/IBlockFactory.h>
#include <PySysLinkBase/ConfigurationValue.h>

#include <memory>
#include <mutex>
#include <map>
#include <stdexcept>
#include <string>

#include "FMUDefinition.h"
#include "SimulationBlockFMU.h"

namespace BlockTypeSupports::BasicFMUSupport {

class BlockFactoryFMU : public PySysLinkBase::IBlockFactory {
public:
    explicit BlockFactoryFMU(
        const std::map<std::string, PySysLinkBase::ConfigurationValue>&)
    {}

    std::shared_ptr<PySysLinkBase::ISimulationBlock> CreateBlock(
        std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration,
        std::shared_ptr<PySysLinkBase::IBlockEventsHandler> eventHandler) override 
    {
        const auto pathIt = blockConfiguration.find("FMUPath");
        if (pathIt == blockConfiguration.end()) {
            throw std::invalid_argument(
                "BasicFMUSupport requires block configuration key FMUPath");
        }

        const auto fmuPath =
            PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<std::string>(
                "FMUPath", blockConfiguration);
        std::shared_ptr<const FMUDefinition> definition;
        {
            std::lock_guard<std::mutex> lock(definitionsMutex_);
            const auto definitionIt = definitions_.find(fmuPath);
            if (definitionIt != definitions_.end()) {
                definition = definitionIt->second;
            } else {
                definition = std::make_shared<FMUDefinition>(fmuPath);
                definitions_.emplace(fmuPath, definition);
            }
        }

        return std::make_shared<SimulationBlockFMU>(
            std::move(definition), std::move(blockConfiguration),
            std::move(eventHandler));
    }

private:
    std::mutex definitionsMutex_;
    std::map<std::string, std::shared_ptr<const FMUDefinition>> definitions_;
};

}  // namespace BlockTypeSupports::BasicFMUSupport

#endif