from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, cmake_layout
from conan.errors import ConanInvalidConfiguration


class TritonCommonConan(ConanFile):
    name = "triton-common"
    version = "2.68.0"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "enable_grpc":    [True, False],
        "enable_metrics": [True, False],
    }
    default_options = {
        "enable_grpc":    True,
        "enable_metrics": True,
    }

    def validate(self):
        if self.settings.os != "Linux":
            raise ConanInvalidConfiguration("triton-common only supports Linux")

    def requirements(self):
        self.requires("rapidjson/cci.20230929")
        self.requires("protobuf/3.21.12")
        if self.options.enable_grpc:
            self.requires("grpc/1.54.3")
        if self.options.enable_metrics:
            self.requires("prometheus-cpp/1.2.4")

    def configure(self):
        if self.options.enable_grpc:
            self.options["grpc"].shared = False
            self.options["grpc"].cpp_plugin = True
        if self.options.enable_metrics:
            self.options["prometheus-cpp"].shared = False
            self.options["prometheus-cpp"].with_pull = False
            self.options["prometheus-cpp"].with_push = False

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["TRITON_COMMON_ENABLE_GRPC"]     = self.options.enable_grpc
        tc.variables["TRITON_COMMON_ENABLE_PROTOBUF"] = True
        tc.variables["TRITON_COMMON_ENABLE_JSON"]     = True
        tc.generate()
        CMakeDeps(self).generate()
