# BlockTypeSupportsFMU
Block type support for FMU-s

## Minimum example

This project contains a small FMI 3.0 Co-Simulation example using the Modelon
FMI Library. CMake downloads and builds the library automatically.

```sh
cmake -S . -B build
cmake --build build
cmake --build build --target run_fmu_test
```

The executable accepts an optional FMU path:

```sh
build/fmu_test test/reference_fmus/BouncingBall.fmu
```
