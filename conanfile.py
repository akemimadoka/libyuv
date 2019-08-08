from conans import ConanFile, CMake, tools

class LibYuvConan(ConanFile):
    name = "libyuv"
    version = "1735"
    license = "BSD-3-Clause"
    author = "Google"
    url = "https://github.com/akemimadoka/libyuv"
    topics = ("C++")
    settings = "os", "compiler", "build_type", "arch"

    options = {"shared": [True, False], "YUV_TEST": [
        True, False], "YUV_TOOL": [True, False]}
    default_options = ["shared=False", "YUV_TEST=False", "YUV_TOOL=False"]
    default_options = tuple(default_options)

    requires = ("libjpeg-turbo/2.0.2@bincrafters/stable")

    generators = "cmake"

    exports_sources = "include*", "source*", "unit_test*", "CMakeLists.txt"

    def requirements(self):
        if self.options.YUV_TEST:
            self.requires("gtest/1.8.1@bincrafters/stable")
            self.requires("gflags/2.2.2@bincrafters/stable")

    def configure_cmake(self):
        cmake = CMake(self)

        cmake.definitions["BUILD_SHARED_LIBS"] = "ON" if self.options.shared else "OFF"
        cmake.definitions["YUV_TEST"] = "ON" if self.options.YUV_TEST else "OFF"
        cmake.definitions["YUV_TOOL"] = "ON" if self.options.YUV_TOOL else "OFF"

        cmake.configure()
        return cmake

    def build(self):
        cmake = self.configure_cmake()
        cmake.build()

    def package(self):
        cmake = self.configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = tools.collect_libs(self)
