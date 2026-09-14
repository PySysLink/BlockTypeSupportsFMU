#ifndef BLOCK_TYPE_SUPPORTS_FMU_BLOCK_FACTORY_H
#define BLOCK_TYPE_SUPPORTS_FMU_BLOCK_FACTORY_H

#include <PySysLinkBase/ConfigurationValue.h>
#include <PySysLinkBase/IBlockEventsHandler.h>
#include <PySysLinkBase/IBlockFactory.h>

#include <memory>
#include <stdexcept>
#include <string>

#include "FMUDefinition.h"
#include "SimulationBlockFMU.h"

namespace BlockTypeSupports::BasicFMUSupport {

class BlockFactoryFMU : public PySysLinkBase::IBlockFactory {
public:
    explicit BlockFactoryFMU(
        const std::map<std::string, PySysLinkBase::ConfigurationValue>& configuration) {
        const auto it = configuration.find("FMUPath");
        if (it == configuration.end()) {
            throw std::invalid_argument(
                "BasicFMUSupport requires plugin configuration key FMUPath");
        }
        definition_ = std::make_shared<FMUDefinition>(std::get<std::string>(it->second));
    }

    std::shared_ptr<PySysLinkBase::ISimulationBlock> CreateBlock(
        std::map<std::string, PySysLinkBase::ConfigurationValue> blockConfiguration,
        std::shared_ptr<PySysLinkBase::IBlockEventsHandler> eventHandler) override {
        return std::make_shared<SimulationBlockFMU>(
            definition_, std::move(blockConfiguration), std::move(eventHandler));
    }

private:
    std::shared_ptr<FMUDefinition> definition_;
};

}  // namespace BlockTypeSupports::BasicFMUSupport

#endif