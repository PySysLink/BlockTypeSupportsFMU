import html
import json
import os

from matplotlib import pyplot as plt
import yaml
import pysyslink_toolkit
from pysyslink_toolkit.LowLevelBlockStructure import LowLevelBlock, LowLevelBlockStructure
from pysyslink_toolkit.HighLevelBlock import HighLevelBlock
from pysyslink_toolkit.BlockRenderInformation import BlockRenderInformation
from pysyslink_toolkit.block_libraries.BlockLibraryPluginConfig import ConfigurationValue

import fmpy

class FMUPlugin(pysyslink_toolkit.BlockLibraryPlugin):   
    def _compile_block(self, high_level_block: HighLevelBlock, pslk_path: str) -> LowLevelBlockStructure:
        port_map = {}
        blocks = []

        fmu_parameters = self._get_fmu_parameters(
            self._get_fmu_path(high_level_block, pslk_path)
        )
        parameter_names = []
        parameter_values = {}
        for parameter in fmu_parameters:
            parameter_name = parameter.name
            property_name = self._fmu_parameter_property_name(parameter_name)
            configured_value = high_level_block.properties.get(property_name)
            if configured_value is not None:
                parameter_names.append(parameter_name)
                parameter_values[f"{property_name}[double]"] = float(
                    configured_value["value"]
                )

        block = LowLevelBlock(
            id=high_level_block.id,
            name=f"FMU {high_level_block.label} Low Level",
            block_type="BasicFMU",
            block_class="BasicBlocksFMU/BasicFMU",
            input_port_number=high_level_block.input_ports,
            input_port_types=[input_port_type.to_string() for input_port_type in high_level_block.input_port_types],
            output_port_number=high_level_block.output_ports,
            output_port_types=[output_port_type.to_string() for output_port_type in high_level_block.output_port_types],
            **{
                "FMUPath[string]": self._get_fmu_path(high_level_block),
                "FMUInputVariables[vector<string>]": self._get_property_value(
                    high_level_block, "FMUInputVariables", []
                ),
                "FMUOutputVariables[vector<string>]": self._get_property_value(
                    high_level_block, "FMUOutputVariables", []
                ),
                "FMUParameterNames[vector<string>]": parameter_names,
                **parameter_values,
                "SampleTime[double]": self._get_property_value(
                    high_level_block, "SampleTime", -1
                ),
            },
        )

        blocks.append(block)
        for input_port_idx in range(high_level_block.input_ports):
            port_map[("input", input_port_idx)] = (block.id, input_port_idx)

        for output_port_idx in range(high_level_block.output_ports):
            port_map[("output", output_port_idx)] = (block.id, output_port_idx)

        return LowLevelBlockStructure(blocks, [], port_map)

    def _get_block_render_information(self, high_level_block: HighLevelBlock, pslk_path: str) -> BlockRenderInformation:
        print("In block render info in fmu plugin")
        render_information = super()._get_block_render_information(high_level_block, pslk_path)
        fmu_path = self._get_fmu_path(high_level_block, pslk_path)

        print(f"FMU path: {fmu_path}")
        if fmu_path:
            print(f"Parameters: {self._get_fmu_parameters(fmu_path)}")
            render_information.additional_properties = {
                self._fmu_parameter_property_name(parameter.name):
                    ConfigurationValue(
                        name=self._fmu_parameter_property_name(parameter.name),
                        type="float",
                        defaultValue=float(parameter.start),
                        metadata={
                            "description": getattr(parameter, "description", None) or "",
                            "unit": self._variable_unit(parameter),
                            "fmuVariable": parameter.name,
                        },
                    )
                for parameter in self._get_fmu_parameters(fmu_path)
            }
        return render_information

    @staticmethod
    def _get_property_value(high_level_block, property_name, default=None):
        property_value = high_level_block.properties.get(property_name)
        if property_value is None:
            return default
        if isinstance(property_value, dict):
            return property_value.get("value", default)
        return property_value

    @staticmethod
    def _fmu_parameter_property_name(parameter_name):
        return f"FMUParameter_{parameter_name}"


    # ------------------------------------------------------------------
    # FMU inspection
    # ------------------------------------------------------------------

    def _get_fmu_path(self, high_level_block: HighLevelBlock, pslk_path: str = None) -> str:
        """
        Get the FMUPath parameter and resolve it relative to the .pslk
        file when necessary.

        Adapt the parameter access below to the actual HighLevelBlock API.
        """

        # Example assumption:
        #     high_level_block.parameters["FMUPath"]
        #
        # If your API exposes parameters differently, change this part.

        fmu_path = self._get_property_value(high_level_block, "FMUPath")

        if not fmu_path:
            return None


        if not os.path.isabs(fmu_path) and pslk_path:
            pslk_dir = os.path.dirname(os.path.abspath(pslk_path))
            fmu_path = os.path.normpath(
                os.path.join(pslk_dir, fmu_path)
            )

        return fmu_path

    def _get_fmu_parameters(self, fmu_path):
        if not fmu_path or not os.path.isfile(fmu_path):
            print(f"FMU path does not exist or is not a file: {fmu_path}")
            return []

        model = self._inspect_fmu(fmu_path)
        print(f"Model: {model}")
        print(f"Model variables: {getattr(model, 'modelVariables', [])}")
        return [
            variable
            for variable in getattr(model, "modelVariables", [])
            if getattr(variable, "causality", None) == "parameter"
            and self._variable_type(variable) == "Float64"
            and getattr(variable, "start", None) is not None
            and getattr(variable, "initial", None) != "calculated"
        ]

    def _inspect_fmu(self, fmu_path):
        """
        Read the FMU modelDescription.xml using FMPy.
        """

        model_description = fmpy.read_model_description(fmu_path)

        return model_description

    @staticmethod
    def _variable_type(variable):
        """
        Return a readable FMI variable type.

        FMPy's variable objects have different attributes depending on
        FMI version/type, so keep this deliberately defensive.
        """

        # FMPy normally exposes the type through variable.type
        variable_type = getattr(variable, "type", None)

        if variable_type:
            return str(variable_type)

        return "Unknown"

    @staticmethod
    def _variable_start(variable):
        """
        Get the start value if available.
        """

        start = getattr(variable, "start", None)

        if start is None:
            return ""

        return str(start)

    @staticmethod
    def _variable_unit(variable):
        unit = getattr(variable, "unit", None)

        if unit is None:
            return ""

        return str(unit)

    def _fmu_html(self, fmu_path):
        """
        Generate an HTML description of the FMU.
        """

        if not os.path.exists(fmu_path):
            return (
                '<div class="fmu-error">'
                f"FMU not found: <code>{html.escape(fmu_path)}</code>"
                "</div>"
            )

        if not os.path.isfile(fmu_path):
            return (
                '<div class="fmu-error">'
                f"FMU path is not a file: "
                f"<code>{html.escape(fmu_path)}</code>"
                "</div>"
            )

        try:
            model = self._inspect_fmu(fmu_path)

        except Exception as exc:
            return (
                '<div class="fmu-error">'
                "<h3>Unable to read FMU</h3>"
                f"<p>{html.escape(str(exc))}</p>"
                f"<p>File: <code>{html.escape(fmu_path)}</code></p>"
                "</div>"
            )

        # --------------------------------------------------------------
        # General FMU information
        # --------------------------------------------------------------

        fmi_version = getattr(model, "fmiVersion", "Unknown")
        model_name = getattr(model, "modelName", "Unknown")
        description = getattr(model, "description", None)

        generation_tool = getattr(model, "generationTool", None)
        generation_date = getattr(model, "generationDateAndTime", None)

        # --------------------------------------------------------------
        # Supported FMI interfaces
        # --------------------------------------------------------------

        interfaces = []

        if getattr(model, "coSimulation", None) is not None:
            interfaces.append("Co-Simulation")

        if getattr(model, "modelExchange", None) is not None:
            interfaces.append("Model Exchange")

        if getattr(model, "scheduledExecution", None) is not None:
            interfaces.append("Scheduled Execution")

        if not interfaces:
            interfaces.append("None detected")

        # --------------------------------------------------------------
        # Variables
        # --------------------------------------------------------------

        variables = getattr(model, "modelVariables", [])

        inputs = [
            v for v in variables
            if getattr(v, "causality", None) == "input"
        ]

        outputs = [
            v for v in variables
            if getattr(v, "causality", None) == "output"
        ]

        parameters = [
            v for v in variables
            if getattr(v, "causality", None) == "parameter"
        ]

        # --------------------------------------------------------------
        # Build HTML
        # --------------------------------------------------------------

        parts = []

        parts.append("""
<style>
.fmu-info {
    font-family: sans-serif;
    font-size: 13px;
}

.fmu-info h2 {
    margin-top: 0;
    margin-bottom: 8px;
}

.fmu-info h3 {
    margin-top: 18px;
    margin-bottom: 6px;
}

.fmu-summary {
    border-collapse: collapse;
    margin-bottom: 10px;
}

.fmu-summary td {
    padding: 3px 10px 3px 0;
    vertical-align: top;
}

.fmu-summary td:first-child {
    font-weight: bold;
}

.fmu-table {
    border-collapse: collapse;
    width: 100%;
    margin-top: 5px;
}

.fmu-table th,
.fmu-table td {
    border: 1px solid #ccc;
    padding: 4px 6px;
    text-align: left;
}

.fmu-table th {
    background: #eee;
}

.fmu-path {
    font-family: monospace;
    word-break: break-all;
}

.fmu-error {
    font-family: sans-serif;
    padding: 10px;
    border: 1px solid #d66;
}
</style>
""")

        parts.append('<div class="fmu-info">')

        parts.append(
            f"<h2>FMU: {html.escape(str(model_name))}</h2>"
        )

        parts.append('<table class="fmu-summary">')

        parts.append(
            "<tr><td>Path</td>"
            f"<td class='fmu-path'>{html.escape(fmu_path)}</td></tr>"
        )

        parts.append(
            f"<tr><td>FMI version</td>"
            f"<td>{html.escape(str(fmi_version))}</td></tr>"
        )

        parts.append(
            "<tr><td>Interfaces</td>"
            f"<td>{html.escape(', '.join(interfaces))}</td></tr>"
        )

        if description:
            parts.append(
                "<tr><td>Description</td>"
                f"<td>{html.escape(str(description))}</td></tr>"
            )

        if generation_tool:
            parts.append(
                "<tr><td>Generation tool</td>"
                f"<td>{html.escape(str(generation_tool))}</td></tr>"
            )

        if generation_date:
            parts.append(
                "<tr><td>Generation date</td>"
                f"<td>{html.escape(str(generation_date))}</td></tr>"
            )

        parts.append("</table>")

        # --------------------------------------------------------------
        # Variables
        # --------------------------------------------------------------

        def add_variable_table(title, variable_list):
            parts.append(
                f"<h3>{html.escape(title)} "
                f"({len(variable_list)})</h3>"
            )

            if not variable_list:
                parts.append("<p>None</p>")
                return

            parts.append("""
<table class="fmu-table">
<tr>
    <th>Name</th>
    <th>Type</th>
    <th>Unit</th>
    <th>Start</th>
</tr>
""")

            for variable in variable_list:
                name = getattr(variable, "name", "")
                variable_type = self._variable_type(variable)
                unit = self._variable_unit(variable)
                start = self._variable_start(variable)

                parts.append(
                    "<tr>"
                    f"<td>{html.escape(str(name))}</td>"
                    f"<td>{html.escape(variable_type)}</td>"
                    f"<td>{html.escape(unit)}</td>"
                    f"<td>{html.escape(start)}</td>"
                    "</tr>"
                )

            parts.append("</table>")

        add_variable_table("Inputs", inputs)
        add_variable_table("Outputs", outputs)
        add_variable_table("Parameters", parameters)

        parts.append("</div>")

        return "".join(parts)

    # ------------------------------------------------------------------
    # Block HTML
    # ------------------------------------------------------------------

    def _get_block_html(self, high_level_block, pslk_path):

        fmu_path = self._get_fmu_path(high_level_block, pslk_path)

        if not fmu_path:
            return (
                "<div class='fmu-error'>"
                "<h3>No FMUPath specified</h3>"
                "</div>"
            )

        return self._fmu_html(fmu_path)
