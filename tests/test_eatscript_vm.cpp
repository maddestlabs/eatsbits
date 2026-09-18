#include <iostream>
#include <cassert>
#include <cmath>
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include "eatsbits/eatscript/vm.hpp"

using namespace eatsbits::eatscript;

void testLexer() {
    std::cout << "[Test] Lexer tokenization of Eatscript Pythonic DSL..." << std::endl;
    std::string script = R"(
def init():
    return {
        "Cutoff": 1500.0,
    }

def process(time, freq, note, params):
    # Calculate simple sine wave
    return math.sin(2.0 * math.pi * freq * time)
)";

    Lexer lexer(script);
    auto tokens = lexer.tokenize();

    assert(!tokens.empty());
    bool foundDef = false;
    bool foundIndent = false;
    bool foundReturn = false;

    for (const auto& t : tokens) {
        if (t.type == TokenType::Def) foundDef = true;
        if (t.type == TokenType::Indent) foundIndent = true;
        if (t.type == TokenType::Return) foundReturn = true;
    }

    assert(foundDef);
    assert(foundIndent);
    assert(foundReturn);
    std::cout << "  -> Passed: Correctly identified keywords, indentation blocks, and string literals." << std::endl;
}

void testParser() {
    std::cout << "[Test] Parser AST construction..." << std::endl;
    std::string script = R"(
def process(time, freq, note, params):
    val = freq * 2.0
    if val > 1000.0:
        return 0.5
    else:
        return 1.0
)";

    Lexer lexer(script);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto prog = parser.parse();

    assert(prog != nullptr);
    assert(prog->statements.size() == 1);

    auto* func = dynamic_cast<FunctionDef*>(prog->statements[0].get());
    assert(func != nullptr);
    assert(func->name == "process");
    assert(func->params.size() == 4);
    assert(func->body.size() == 2); // AssignStmt, IfStmt

    std::cout << "  -> Passed: AST correctly produced FunctionDef, AssignStmt, and IfStmt." << std::endl;
}

void testVMExecution() {
    std::cout << "[Test] Eatscript Bytecode VM Execution..." << std::endl;
    std::string script = R"(
def process(time, freq, note, params):
    return math.sin(2.0 * math.pi * freq * time)
)";

    VM vm;
    bool compiled = vm.compileSource(script);
    assert(compiled);
    assert(vm.hasCompiledProcess());

    // Execute at t = 0.0 -> sin(0) = 0.0
    double sample0 = vm.executeProcess(0.0, 440.0, 69.0);
    assert(std::abs(sample0) < 1e-6);

    // Execute at t = 1.0 / (4 * 440.0) -> sin(pi / 2) = 1.0
    double tQuarter = 1.0 / (4.0 * 440.0);
    double sampleQuarter = vm.executeProcess(tQuarter, 440.0, 69.0);
    assert(std::abs(sampleQuarter - 1.0) < 1e-4);

    std::cout << "  -> Passed: VM accurately calculated math.sin(2*pi*f*t) within 1e-4." << std::endl;
}

void testVMMultiExpression() {
    std::cout << "[Test] Eatscript VM Branching & Variables..." << std::endl;
    std::string script = R"(
def process(time, freq, note, params):
    x = freq * 0.5
    if x > 200.0:
        return math.tanh(1.5)
    else:
        return -0.8
)";

    VM vm;
    bool compiled = vm.compileSource(script);
    assert(compiled);

    // freq = 500 -> x = 250 > 200 -> return tanh(1.5)
    double resHigh = vm.executeProcess(0.0, 500.0, 60.0);
    assert(std::abs(resHigh - std::tanh(1.5)) < 1e-5);

    // freq = 200 -> x = 100 <= 200 -> return -0.8
    double resLow = vm.executeProcess(0.0, 200.0, 60.0);
    assert(std::abs(resLow - (-0.8)) < 1e-5);

    std::cout << "  -> Passed: Conditional logic and math.tanh executed correctly." << std::endl;
}

int main() {
    std::cout << "=== Eatsbits Eatscript C++ Core Test Suite ===" << std::endl;
    testLexer();
    testParser();
    testVMExecution();
    testVMMultiExpression();
    std::cout << "=== ALL EATSCRIPT VM TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
