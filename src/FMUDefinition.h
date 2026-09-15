#ifndef BLOCK_TYPE_SUPPORTS_FMU_DEFINITION_H
#define BLOCK_TYPE_SUPPORTS_FMU_DEFINITION_H

#include <fmilib.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <iostream>

namespace BlockTypeSupports::BasicFMUSupport {

struct FMUVariable {
    std::string name;
    fmi3_value_reference_t valueReference;
    fmi3_causality_enu_t causality;
    fmi3_base_type_enu_t baseType;
};

inline void FMULogger(jm_callbacks*, jm_string module, jm_log_level_enu_t level,
			jm_string message) 
{
    std::cerr << "[" << module << "][" << jm_log_level_to_string(level)
			  << "] " << message << '\n';
}

class FMUDefinition {
public:
    explicit FMUDefinition(const std::string& fmuPath) : fmuPath_(fmuPath) {
        callbacks_.malloc = std::malloc;
        callbacks_.calloc = std::calloc;
        callbacks_.realloc = std::realloc;
        callbacks_.free = std::free;
        callbacks_.logger = FMULogger;
        callbacks_.log_level = jm_log_level_warning;

        context_ = fmi_import_allocate_context(&callbacks_);
        if (context_ == nullptr) {
            throw std::runtime_error("Could not allocate the FMI import context");
        }
        unpackedPath_ = fmi_import_mk_temp_dir(&callbacks_, nullptr, nullptr);
        if (unpackedPath_ == nullptr) {
            fmi_import_free_context(context_);
            context_ = nullptr;
            throw std::runtime_error("Could not create an FMU extraction directory");
        }
        unpackedPathString_ = unpackedPath_;

        if (fmi_import_get_fmi_version(context_, fmuPath_.c_str(), unpackedPath_)
            != fmi_version_3_0_enu) {
            cleanup();
            throw std::runtime_error("Only FMI 3.0 FMUs are currently supported");
        }

        fmi3_import_t* parsed =
            fmi3_import_parse_xml(context_, unpackedPath_, nullptr);
        if (parsed == nullptr ||
            (fmi3_import_get_fmu_kind(parsed) & fmi3_fmu_kind_cs) != fmi3_fmu_kind_cs) {
            if (parsed != nullptr) {
                fmi3_import_free(parsed);
            }
            cleanup();
            throw std::runtime_error(
                "The FMU must provide FMI 3.0 Co-Simulation");
        }
        try {
            collectVariables(parsed);
        } catch (...) {
            fmi3_import_free(parsed);
            cleanup();
            throw;
        }
        fmi3_import_free(parsed);
    }

    ~FMUDefinition() {
        cleanup();
    }

    void cleanup() {
        if (unpackedPath_ != nullptr) {
            fmi_import_rmdir(&callbacks_, unpackedPath_);
            callbacks_.free(unpackedPath_);
            unpackedPath_ = nullptr;
        }
        if (context_ != nullptr) {
            fmi_import_free_context(context_);
            context_ = nullptr;
        }
    }

    FMUDefinition(const FMUDefinition&) = delete;
    FMUDefinition& operator=(const FMUDefinition&) = delete;

    const std::string& unpackedPath() const { return unpackedPathString_; }
    jm_callbacks& callbacks() const {
        return const_cast<jm_callbacks&>(callbacks_);
    }

    const FMUVariable& requireVariable(const std::string& name, fmi3_causality_enu_t expectedCausality) const {
        const auto it = variables_.find(name);
        if (it == variables_.end()) {
            throw std::invalid_argument("FMU variable not found: " + name);
        }
        if (it->second.baseType != fmi3_base_type_float64) {
            throw std::invalid_argument(
                "Only Float64 FMU variables are currently supported: " + name);
        }
        if (it->second.causality != expectedCausality) {
            throw std::invalid_argument("FMU variable has unexpected causality: " + name);
        }
        return it->second;
    }

private:
    void collectVariables(fmi3_import_t* fmu) {
        auto* variableList = fmi3_import_get_variable_list(fmu, 0);
        if (variableList == nullptr) {
            throw std::runtime_error("Could not inspect FMU variables");
        }
        const size_t count = fmi3_import_get_variable_list_size(variableList);
        for (size_t index = 0; index < count; ++index) {
            auto* variable = fmi3_import_get_variable(variableList, index);
            FMUVariable metadata{
                fmi3_import_get_variable_name(variable),
                fmi3_import_get_variable_vr(variable),
                fmi3_import_get_variable_causality(variable),
                fmi3_import_get_variable_base_type(variable),
            };
            variables_.emplace(metadata.name, std::move(metadata));
        }
        fmi3_import_free_variable_list(variableList);
    }

    std::string fmuPath_;
    jm_callbacks callbacks_{};
    fmi_import_context_t* context_ = nullptr;
    char* unpackedPath_ = nullptr;
    std::string unpackedPathString_;
    std::unordered_map<std::string, FMUVariable> variables_;
};

}  // namespace BlockTypeSupports::BasicFMUSupport

#endif