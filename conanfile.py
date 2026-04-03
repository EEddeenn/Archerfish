from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout


class ArcherfishConan(ConanFile):
    name = "archerfish"
    version = "0.1.0"
    description = "CLI-first programmable vector signal generator for USRP"
    license = "MIT"
    settings = "os", "compiler", "build_type", "arch"

    requires = (
        "cli11/2.6.0",
        "spdlog/1.17.0",
        "nlohmann_json/3.12.0",
        "catch2/3.5.1",
    )

    # UHD is installed system-wide and found via CMake find_package.
    # libsamplerate and nlohmann_json_schema_validator will be added in later increments.

    tool_requires = "cmake/[>=3.25 <4]"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.generate()
