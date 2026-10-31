#ifndef BLOCK_TYPE_SUPPORTS_FMU_SIMULATION_BLOCK_H
#define BLOCK_TYPE_SUPPORTS_FMU_SIMULATION_BLOCK_H

#include <fmilib.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <PySysLinkBase/ConfigurationValue.h>
#include <PySysLinkBase/ISimulationBlock.h>
#include <PySysLinkBase/PortsAndSignalValues/InputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/OutputPort.h>
#include <PySysLinkBase/PortsAndSignalValues/SignalValue.h>
#include <PySysLinkBase/SampleTime.h>

#include "FMUDefinition.h"

namespace BlockTypeSupports::BasicFMUSupport {

class SimulationBlockFMU : public PySysLinkBase::ISimulationBlock {
public:
    SimulationBlockFMU(
        std::shared_ptr<const FMUDefinition> definition,
        std::map<std::string, PySysLinkBase::ConfigurationValue> configuration,
        std::shared_ptr<PySysLinkBase::IBlockEventsHandler> eventHandler)
        : ISimulationBlock(configuration, eventHandler), definition_(std::move(definition)) 
    {
        for (const auto& name : getNames(configuration, "FMUInputVariables")) {
            inputVariables_.push_back(definition_->requireVariable(name, fmi3_causality_enu_input));
            inputPorts_.push_back(std::make_shared<PySysLinkBase::InputPort>(false, std::make_shared<PySysLinkBase::SignalValue<double>>(0.0)));
        }
        for (const auto& name : getNames(configuration, "FMUOutputVariables")) {
            outputVariables_.push_back(definition_->requireVariable(name, fmi3_causality_enu_output));
            outputPorts_.push_back(std::make_shared<PySysLinkBase::OutputPort>(std::make_shared<PySysLinkBase::SignalValue<double>>(0.0)));
        }
        for (const auto& name : getNames(configuration, "FMUParameterNames")) {
            parameterVariables_.push_back(
                definition_->requireVariable(name, fmi3_causality_enu_parameter));
            parameterValues_.push_back(
                PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<double>(
                    "FMUParameter_" + name, configuration));
        }

        context_ = fmi_import_allocate_context(&definition_->callbacks());
        if (context_ == nullptr) {
            throw std::runtime_error("Could not allocate the block FMI context");
        }

        fmu_ = fmi3_import_parse_xml(context_, definition_->unpackedPath().c_str(), nullptr);

        if (fmu_ == nullptr ||
            (fmi3_import_get_fmu_kind(fmu_) & fmi3_fmu_kind_cs) != fmi3_fmu_kind_cs) {
            releaseFmu();
            throw std::runtime_error("Could not parse the block FMI model");
        }

        if (fmi3_import_create_dllfmu(fmu_, fmi3_fmu_kind_cs, nullptr, nullptr)
            != jm_status_success) {
            releaseFmu();
            throw std::runtime_error("Could not load the FMU shared library");
        }

        dllLoaded_ = true;

        const std::string instanceName = GetName().empty() ? GetId() : GetName();

        if (fmi3_import_instantiate_co_simulation(
                fmu_, instanceName.c_str(), "", fmi3_false, fmi3_false,
                fmi3_false, fmi3_false, nullptr, 0, nullptr)
            != jm_status_success) {
            releaseFmu();
            throw std::runtime_error("Could not instantiate the FMU block");
        }

        instanceCreated_ = true;

        double sampleTimeValue = PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<double>("SampleTime", configuration);
        if (sampleTimeValue <= 0 && sampleTimeValue != -1)
        {
            throw std::invalid_argument("SampleTime must be greater than 0 or equal to -1 for inherited sample time.");
        }
        if (sampleTimeValue == -1)
        {
            this->sampleTime_ = std::make_shared<PySysLinkBase::SampleTime>(PySysLinkBase::SampleTimeType::inherited, std::vector<PySysLinkBase::SampleTimeType>{PySysLinkBase::SampleTimeType::discrete});
        }
        else
        {
            this->sampleTime_ = std::make_shared<PySysLinkBase::SampleTime>(PySysLinkBase::SampleTimeType::discrete, sampleTimeValue);
        }
    }

    ~SimulationBlockFMU() override {
        releaseFmu();
        if (context_ != nullptr) {
            fmi_import_free_context(context_);
        }
    }

    const std::shared_ptr<PySysLinkBase::SampleTime> GetSampleTime() const override {
        return sampleTime_;
    }

    void SetSampleTime(std::shared_ptr<PySysLinkBase::SampleTime> sampleTime) override {
        sampleTime_ = std::move(sampleTime);
    }

    std::vector<std::shared_ptr<PySysLinkBase::InputPort>> GetInputPorts() const override {
        return inputPorts_;
    }

    const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> GetOutputPorts() const override {
        return outputPorts_;
    }

    const std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> _ComputeOutputsOfBlock(
        const std::shared_ptr<PySysLinkBase::SampleTime> sampleTime, double currentTime, bool) override 
    {
        const double stepSize = sampleTime->GetDiscreteSampleTime();

        if (!initialized_) {
            if (fmi3_import_enter_initialization_mode(
                    fmu_, fmi3_false, 0.0, currentTime, fmi3_false, 0.0)
                != fmi3_status_ok) {
                throw std::runtime_error("Could not initialize the FMU block");
            }
            if (!parameterVariables_.empty() &&
                fmi3_import_set_float64(
                    fmu_, parameterValueReferences().data(),
                    parameterVariables_.size(), parameterValues_.data(),
                    parameterValues_.size()) != fmi3_status_ok) {
                throw std::runtime_error("Could not set FMU parameter values");
            }
            if (fmi3_import_exit_initialization_mode(fmu_) != fmi3_status_ok) {
                throw std::runtime_error("Could not initialize the FMU block");
            }
            initialized_ = true;
        }

        if (hasStepped_) {
            const double timeTolerance =
                1e-12 * std::max(1.0, std::abs(currentTime));
            const double timeDifference = currentTime - lastStepStartTime_;
            if (std::abs(timeDifference) <= timeTolerance) {
                return outputPorts_;
            }
            if (timeDifference < 0.0) {
                throw std::runtime_error(
                    "FMU block does not support simulation time rollback");
            }
        }

        std::vector<fmi3_float64_t> inputValues(inputPorts_.size());

        for (size_t index = 0; index < inputPorts_.size(); ++index) {
            auto value = inputPorts_[index]->GetValue()->TryCastToTyped<double>();
            inputValues[index] = value->GetPayload();
        }

        if (!inputVariables_.empty() &&
            fmi3_import_set_float64(
                fmu_, inputValueReferences().data(), inputVariables_.size(),
                inputValues.data(), inputValues.size()) != fmi3_status_ok) {
            throw std::runtime_error("Could not set FMU input variables");
        }

        fmi3_boolean_t eventHandlingNeeded = fmi3_false;
        fmi3_boolean_t terminate = fmi3_false;
        fmi3_boolean_t earlyReturn = fmi3_false;
        fmi3_float64_t lastSuccessfulTime = currentTime;

        if (fmi3_import_do_step(
                fmu_, currentTime, stepSize, fmi3_true, &eventHandlingNeeded,
                &terminate, &earlyReturn, &lastSuccessfulTime) != fmi3_status_ok) {
            throw std::runtime_error("Could not advance the FMU block");
        }

        lastStepStartTime_ = currentTime;
        hasStepped_ = true;

        std::vector<fmi3_float64_t> outputValues(outputVariables_.size());

        if (!outputVariables_.empty() &&
            fmi3_import_get_float64(
                fmu_, outputValueReferences().data(), outputVariables_.size(),
                outputValues.data(), outputValues.size()) != fmi3_status_ok) {
            throw std::runtime_error("Could not read FMU output variables");
        }

        for (size_t index = 0; index < outputPorts_.size(); ++index) {
            auto value = outputPorts_[index]->GetValue()->TryCastToTyped<double>();
            value->SetPayload(outputValues[index]);
            outputPorts_[index]->SetValue(
                std::make_shared<PySysLinkBase::SignalValue<double>>(*value));
        }

        return outputPorts_;
    }

    bool _TryUpdateConfigurationValue(std::string, PySysLinkBase::ConfigurationValue) override {
        return false;
    }

private:
    static std::vector<std::string> getNames(
        const std::map<std::string, PySysLinkBase::ConfigurationValue>& configuration,
        const std::string& key) 
    {
        const auto it = configuration.find(key);

        if (it == configuration.end()) {
            return {};
        }

        return PySysLinkBase::ConfigurationValueManager::TryGetConfigurationValue<
            std::vector<std::string>>(key, configuration);
    }

    std::vector<fmi3_value_reference_t> inputValueReferences() const 
    {
        std::vector<fmi3_value_reference_t> references;

        for (const auto& variable : inputVariables_) {
            references.push_back(variable.valueReference);
        }
        return references;
    }

    std::vector<fmi3_value_reference_t> outputValueReferences() const 
    {
        std::vector<fmi3_value_reference_t> references;
        for (const auto& variable : outputVariables_) {
            references.push_back(variable.valueReference);
        }
        return references;
    }

    std::vector<fmi3_value_reference_t> parameterValueReferences() const
    {
        std::vector<fmi3_value_reference_t> references;
        for (const auto& variable : parameterVariables_) {
            references.push_back(variable.valueReference);
        }
        return references;
    }

    void releaseFmu() {
        if (fmu_ == nullptr) {
            if (context_ != nullptr) {
                fmi_import_free_context(context_);
                context_ = nullptr;
            }
            return;
        }
        if (initialized_) {
            fmi3_import_terminate(fmu_);
            initialized_ = false;
        }
        if (instanceCreated_) {
            fmi3_import_free_instance(fmu_);
            instanceCreated_ = false;
        }
        if (dllLoaded_) {
            fmi3_import_destroy_dllfmu(fmu_);
            dllLoaded_ = false;
        }
        fmi3_import_free(fmu_);
        fmu_ = nullptr;
        if (context_ != nullptr) {
            fmi_import_free_context(context_);
            context_ = nullptr;
        }
    }

    std::shared_ptr<const FMUDefinition> definition_;
    fmi_import_context_t* context_ = nullptr;
    fmi3_import_t* fmu_ = nullptr;
    bool dllLoaded_ = false;
    bool instanceCreated_ = false;
    bool initialized_ = false;
    bool hasStepped_ = false;
    double lastStepStartTime_ = 0.0;
    std::shared_ptr<PySysLinkBase::SampleTime> sampleTime_;
    std::vector<FMUVariable> inputVariables_;
    std::vector<FMUVariable> outputVariables_;
    std::vector<FMUVariable> parameterVariables_;
    std::vector<fmi3_float64_t> parameterValues_;
    std::vector<std::shared_ptr<PySysLinkBase::InputPort>> inputPorts_;
    std::vector<std::shared_ptr<PySysLinkBase::OutputPort>> outputPorts_;
};

}  // namespace BlockTypeSupports::BasicFMUSupport

#endif