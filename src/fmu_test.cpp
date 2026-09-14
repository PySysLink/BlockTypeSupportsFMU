#include <cstdlib>
#include <iostream>
#include <string>

extern "C" {
#include <fmilib.h>
}

namespace {

void logger(jm_callbacks*, jm_string module, jm_log_level_enu_t level,
			jm_string message) {
	std::cerr << "[" << module << "][" << jm_log_level_to_string(level)
			  << "] " << message << '\n';
}

bool require_status(fmi3_status_t status, const char* operation) {
	if (status != fmi3_status_ok) {
		std::cerr << operation << " failed with status "
				  << fmi3_status_to_string(status) << '\n';
		return false;
	}
	return true;
}

}  // namespace

int main(int argc, char* argv[]) {
	const std::string fmu_path = argc > 1
		? argv[1]
		: DEFAULT_FMU_PATH;

	jm_callbacks callbacks{};
	callbacks.malloc = std::malloc;
	callbacks.calloc = std::calloc;
	callbacks.realloc = std::realloc;
	callbacks.free = std::free;
	callbacks.logger = logger;
	callbacks.log_level = jm_log_level_warning;

	fmi_import_context_t* context = fmi_import_allocate_context(&callbacks);
	if (context == nullptr) {
		std::cerr << "Could not allocate the FMI import context\n";
		return EXIT_FAILURE;
	}

	char* unpacked_path = fmi_import_mk_temp_dir(&callbacks, nullptr, nullptr);
	if (unpacked_path == nullptr) {
		std::cerr << "Could not create a temporary FMU directory\n";
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	const fmi_version_enu_t version = fmi_import_get_fmi_version(
		context, fmu_path.c_str(), unpacked_path);
	if (version != fmi_version_3_0_enu) {
		std::cerr << "Expected an FMI 3.0 FMU\n";
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	fmi3_import_t* fmu = fmi3_import_parse_xml(context, unpacked_path, nullptr);
	if (fmu == nullptr ||
		(fmi3_import_get_fmu_kind(fmu) & fmi3_fmu_kind_cs) != fmi3_fmu_kind_cs) {
		std::cerr << "Could not parse an FMI 3.0 Co-Simulation FMU\n";
		if (fmu != nullptr) {
			fmi3_import_free(fmu);
		}
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	if (fmi3_import_create_dllfmu(fmu, fmi3_fmu_kind_cs, nullptr, nullptr)
		!= jm_status_success) {
		std::cerr << "Could not load the FMU shared library\n";
		fmi3_import_free(fmu);
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	const jm_status_enu_t instantiate_status =
		fmi3_import_instantiate_co_simulation(
			fmu, "bouncing_ball", "", fmi3_false, fmi3_false,
			fmi3_false, fmi3_false, nullptr, 0, nullptr);
	if (instantiate_status != jm_status_success) {
		std::cerr << "Could not instantiate the FMU\n";
		fmi3_import_destroy_dllfmu(fmu);
		fmi3_import_free(fmu);
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	fmi3_import_variable_t* height_variable =
		fmi3_import_get_variable_by_name(fmu, "h");
	fmi3_import_variable_t* velocity_variable =
		fmi3_import_get_variable_by_name(fmu, "v");
	if (height_variable == nullptr || velocity_variable == nullptr) {
		std::cerr << "Could not find the h or v output variable\n";
		fmi3_import_free_instance(fmu);
		fmi3_import_destroy_dllfmu(fmu);
		fmi3_import_free(fmu);
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	const fmi3_value_reference_t value_references[] = {
		fmi3_import_get_variable_vr(height_variable),
		fmi3_import_get_variable_vr(velocity_variable),
	};
	if (!require_status(fmi3_import_enter_initialization_mode(
			fmu, fmi3_false, 0.0, 0.0, fmi3_false, 0.0),
			"enter initialization mode") ||
		!require_status(fmi3_import_exit_initialization_mode(fmu),
						"exit initialization mode")) {
		fmi3_import_free_instance(fmu);
		fmi3_import_destroy_dllfmu(fmu);
		fmi3_import_free(fmu);
		fmi_import_rmdir(&callbacks, unpacked_path);
		callbacks.free(unpacked_path);
		fmi_import_free_context(context);
		return EXIT_FAILURE;
	}

	constexpr fmi3_float64_t step_size = 0.01;
	fmi3_float64_t time = 0.0;
	for (int step = 0; step < 10; ++step) {
		fmi3_boolean_t event_handling_needed = fmi3_false;
		fmi3_boolean_t terminate = fmi3_false;
		fmi3_boolean_t early_return = fmi3_false;
		fmi3_float64_t last_successful_time = time;
		if (!require_status(fmi3_import_do_step(
				fmu, time, step_size, fmi3_true, &event_handling_needed,
				&terminate, &early_return, &last_successful_time),
				"do step")) {
			fmi3_import_free_instance(fmu);
			fmi3_import_destroy_dllfmu(fmu);
			fmi3_import_free(fmu);
			fmi_import_rmdir(&callbacks, unpacked_path);
			callbacks.free(unpacked_path);
			fmi_import_free_context(context);
			return EXIT_FAILURE;
		}

		fmi3_float64_t values[2]{};
		if (!require_status(fmi3_import_get_float64(
				fmu, value_references, 2, values, 2), "get outputs")) {
			fmi3_import_free_instance(fmu);
			fmi3_import_destroy_dllfmu(fmu);
			fmi3_import_free(fmu);
			fmi_import_rmdir(&callbacks, unpacked_path);
			callbacks.free(unpacked_path);
			fmi_import_free_context(context);
			return EXIT_FAILURE;
		}

		time += step_size;
		std::cout << "step=" << step + 1 << " time=" << time
				  << " h=" << values[0] << " v=" << values[1] << '\n';
	}

	fmi3_import_terminate(fmu);
	fmi3_import_free_instance(fmu);
	fmi3_import_destroy_dllfmu(fmu);
	fmi3_import_free(fmu);
	fmi_import_rmdir(&callbacks, unpacked_path);
	callbacks.free(unpacked_path);
	fmi_import_free_context(context);
	return EXIT_SUCCESS;
}
