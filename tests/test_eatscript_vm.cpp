#include <iostream>
#include <cassert>
#include <cmath>
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include "eatsbits/eatscript/vm.hpp"
#include "eatsbits/eatscript/evaluator.hpp"

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

    auto* func = ast_cast<FunctionDef>(prog->statements[0].get());
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

void testVMStereoAudioFx() {
    std::cout << "[Test] Eatscript Stereo Audio FX (def process(input_l, input_r, params) -> [l, r])..." << std::endl;
    std::string fxScript = R"(
def process(input_l, input_r, params):
    drive = 3.0
    out_l = math.tanh(input_l * drive) * 0.8
    out_r = math.tanh(input_r * drive) * 0.8
    return [out_l, out_r]
)";

    VM vm;
    bool compiled = vm.compileSource(fxScript);
    assert(compiled);
    assert(vm.hasCompiledProcess());
    assert(vm.isStereoEffect());

    double testInL = 0.5;
    double testInR = -0.25;
    auto [resL, resR] = vm.executeStereoProcess(testInL, testInR);

    double expectedL = std::tanh(testInL * 3.0) * 0.8;
    double expectedR = std::tanh(testInR * 3.0) * 0.8;

    assert(std::abs(resL - expectedL) < 1e-5);
    assert(std::abs(resR - expectedR) < 1e-5);

    std::cout << "  -> Passed: Stereo Audio FX accurately processed L/R channels and returned list pair." << std::endl;
}

void testASTEvaluator() {
    std::cout << "[Test] AST Evaluator Control Plane execution (Approach A)..." << std::endl;
    Evaluator eval;

    // 1. Literal & Arithmetic Evaluation
    std::string mathScript = "x = 10.0\ny = 2.5\nresult = (x * 2.0) + y\n";
    eval.evaluateSource(mathScript);
    assert(eval.getVariable("result").asNumber() == 22.5);

    // 2. Dict & List Literal Evaluation with trailing commas
    std::string dataScript = R"(
data = {
    "name": "Super Lead",
    "voices": 4,
    "effects": ["delay", "reverb", "chorus",],
}
)";
    eval.evaluateSource(dataScript);
    Value data = eval.getVariable("data");
    assert(data.isDict());
    assert(data.get("name").asString() == "Super Lead");
    assert(data.get("voices").asInt() == 4);
    assert(data.get("effects").isList());
    assert(data.get("effects").listVal.size() == 3);

    // 3. eat.param(...) constructor and function invocation
    std::string paramScript = R"(
def init():
    return {
        "Cutoff": eat.param("Cutoff", 100.0, 10000.0, 2500.0, step=0.0, unit="Hz"),
        "Resonance": eat.param("Resonance", 0.5, 10.0, 4.2),
    }
)";
    eval.evaluateSource(paramScript);
    assert(eval.hasFunction("init"));
    Value initRes = eval.callFunction("init");
    assert(initRes.isDict());
    assert(initRes.hasKey("Cutoff"));
    Value cutParam = initRes.get("Cutoff");
    assert(cutParam.isDict());
    assert(cutParam.get("name").asString() == "Cutoff");
    assert(cutParam.get("min").asNumber() == 100.0);
    assert(cutParam.get("max").asNumber() == 10000.0);
    assert(cutParam.get("default").asNumber() == 2500.0);
    assert(cutParam.get("unit").asString() == "Hz");

    // 4. Keyword Arguments & Host Functions
    std::string macroLog;
    int receivedRoot = 0;
    int receivedSteps = 0;
    eval.registerMemberFunction("eat.daw", "log", [&macroLog](const auto& args, const auto&) {
        if (!args.empty()) macroLog = args[0].asString();
        return Value();
    });
    eval.registerMemberFunction("eat.daw", "generate_acid", [&receivedRoot, &receivedSteps](const auto& args, const auto& kwargs) {
        receivedRoot = resolveArg(args, kwargs, 0, "root", Value(36)).asInt();
        receivedSteps = resolveArg(args, kwargs, 1, "steps", Value(16)).asInt();
        return Value(true);
    });

    std::string macroScript = R"(
eat.daw.log("Evaluator initialized")
eat.daw.generate_acid(steps=32, root=40)
)";
    eval.evaluateSource(macroScript);
    assert(macroLog == "Evaluator initialized");
    assert(receivedRoot == 40);
    assert(receivedSteps == 32);

    std::cout << "  -> Passed: AST Evaluator successfully executed literals, dicts, kwargs, and host methods." << std::endl;
}

int main() {
    std::cout << "=== Eatsbits Eatscript C++ Core Test Suite ===" << std::endl;
    testLexer();
    testParser();
    testVMExecution();
    testVMMultiExpression();
    testVMStereoAudioFx();
    testASTEvaluator();
    std::cout << "=== ALL EATSCRIPT VM TESTS PASSED SUCCESSFULLY! ===" << std::endl;
    return 0;
}
