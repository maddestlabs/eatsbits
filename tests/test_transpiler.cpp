#include <iostream>
#include <cassert>
#include "eatsbits/eatscript/transpiler.hpp"

using namespace eatsbits::eatscript;

void testTranspiler() {
    std::cout << "[Test] Transpiling Eatscript DSL to pure C++ plugin code..." << std::endl;
    std::string script = R"(
def init():
    return {
        "Cutoff": 1200.0
    }

def process(time, freq, note, params):
    val = freq * 0.5
    return math.sin(2.0 * math.pi * freq * time)
)";

    Transpiler transpiler;
    std::string cppCode = transpiler.transpileSource(script, "sine_synth", "Sine Synthesizer");

    assert(!cppCode.empty());
    assert(cppCode.find("struct PluginInstance_sine_synth") != std::string::npos);
    assert(cppCode.find("create_instance_sine_synth") != std::string::npos);
    assert(cppCode.find("process_sine_synth") != std::string::npos);
    assert(cppCode.find("g_plugin_sine_synth") != std::string::npos);
    assert(cppCode.find("std::sin") != std::string::npos);

    std::cout << "  -> Generated C++ Code Snippet:\n" << cppCode.substr(0, 300) << "...\n" << std::endl;
    std::cout << "  -> Passed: Transpiler emitted valid C-ABI plugin code." << std::endl;
}

int main() {
    std::cout << "=== Eatsbits Transpiler Test Suite ===" << std::endl;
    testTranspiler();
    std::cout << "=== ALL TRANSPILER TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
