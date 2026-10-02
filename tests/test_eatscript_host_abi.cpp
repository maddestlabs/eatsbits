#include <iostream>
#include <cassert>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <vector>

#include "eatsbits/abi/eats_host_abi.h"
#include "eatsbits/abi/host_registry.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/eatscript/stdlib.hpp"

#define REQUIRE(cond) do { \
    if (!(cond)) { \
        std::cerr << "Assertion failed: (" #cond ") at " << __FILE__ << ":" << __LINE__ << std::endl; \
        std::exit(1); \
    } \
} while(0)

using namespace eatsbits;
using namespace eatsbits::abi;
using namespace eatsbits::eatscript;

// -------------------------------------------------------------
// Sample C-ABI Struct for Reflection Testing
// -------------------------------------------------------------
#pragma pack(push, 1)
struct TestNodeConfig {
    int32_t     nodeId;
    float       cutoff;
    double      resonance;
    uint8_t     active;
    const char* label;
};
#pragma pack(pop)

static const EatsFieldDescriptor kTestNodeFields[] = {
    {"nodeId",    EATS_ABI_TYPE_I32,    offsetof(TestNodeConfig, nodeId),    sizeof(int32_t),     nullptr},
    {"cutoff",    EATS_ABI_TYPE_F32,    offsetof(TestNodeConfig, cutoff),    sizeof(float),       nullptr},
    {"resonance", EATS_ABI_TYPE_F64,    offsetof(TestNodeConfig, resonance), sizeof(double),      nullptr},
    {"active",    EATS_ABI_TYPE_BOOL,   offsetof(TestNodeConfig, active),    sizeof(uint8_t),     nullptr},
    {"label",     EATS_ABI_TYPE_STRING, offsetof(TestNodeConfig, label),     sizeof(const char*), nullptr}
};

static const EatsStructDescriptor kTestNodeDesc = {
    "TestNodeConfig",
    sizeof(TestNodeConfig),
    5,
    kTestNodeFields
};

// -------------------------------------------------------------
// Sample C-ABI Dispatch Function: add(a: f64, b: f64) -> f64
// -------------------------------------------------------------
static int32_t c_host_add(void* /*context*/, const EatsAbiValue* args, uint32_t numArgs, EatsAbiValue* outResult) {
    if (numArgs < 2 || !outResult) return EATS_ABI_ERROR_INVALID_ARG;
    double a = args[0].as.f64;
    double b = args[1].as.f64;
    *outResult = eats_abi_value_f64(a + b);
    return EATS_ABI_OK;
}

// Sample C-ABI Dispatch Function: compute_buffer_sum(buf: buffer) -> f64
static int32_t c_host_buffer_sum(void* /*context*/, const EatsAbiValue* args, uint32_t numArgs, EatsAbiValue* outResult) {
    if (numArgs < 1 || !outResult) return EATS_ABI_ERROR_INVALID_ARG;
    if (args[0].type != EATS_ABI_TYPE_BUFFER) return EATS_ABI_ERROR_TYPE_MISMATCH;

    const auto& buf = args[0].as.buffer;
    if (!buf.data) return EATS_ABI_ERROR_INVALID_ARG;

    const float* floats = static_cast<const float*>(buf.data);
    size_t count = buf.sizeBytes / sizeof(float);
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        sum += floats[i];
    }
    *outResult = eats_abi_value_f64(sum);
    return EATS_ABI_OK;
}

void testHostReflectionABI() {
    std::cout << "[Test 1] Generic Host Reflection ABI: struct descriptors, field reflection & dynamic mutation..." << std::endl;

    HostRegistry registry;
    bool regOk = registry.registerStruct(kTestNodeDesc);
    REQUIRE(regOk);
    REQUIRE(registry.hasStruct("TestNodeConfig"));

    const auto* found = registry.findStruct("TestNodeConfig");
    REQUIRE(found != nullptr);
    REQUIRE(found->fieldCount == 5);

    TestNodeConfig node{};
    node.nodeId = 42;
    node.cutoff = 1500.0f;
    node.resonance = 0.707;
    node.active = 1;
    node.label = "AcidLead";

    // Dynamic field inspection
    EatsAbiValue idVal = HostRegistry::getFieldValue(kTestNodeDesc, &node, "nodeId");
    REQUIRE(idVal.type == EATS_ABI_TYPE_I32);
    REQUIRE(idVal.as.i32 == 42);

    EatsAbiValue cutVal = HostRegistry::getFieldValue(kTestNodeDesc, &node, "cutoff");
    REQUIRE(cutVal.type == EATS_ABI_TYPE_F32);
    REQUIRE(std::abs(cutVal.as.f32 - 1500.0f) < 1e-4f);

    EatsAbiValue resVal = HostRegistry::getFieldValue(kTestNodeDesc, &node, "resonance");
    REQUIRE(resVal.type == EATS_ABI_TYPE_F64);
    REQUIRE(std::abs(resVal.as.f64 - 0.707) < 1e-6);

    EatsAbiValue actVal = HostRegistry::getFieldValue(kTestNodeDesc, &node, "active");
    REQUIRE(actVal.type == EATS_ABI_TYPE_BOOL);
    REQUIRE(actVal.as.boolean == 1);

    EatsAbiValue lblVal = HostRegistry::getFieldValue(kTestNodeDesc, &node, "label");
    REQUIRE(lblVal.type == EATS_ABI_TYPE_STRING);
    REQUIRE(std::strcmp(lblVal.as.stringVal, "AcidLead") == 0);

    // Dynamic field mutation
    bool setOk = HostRegistry::setFieldValue(kTestNodeDesc, &node, "cutoff", eats_abi_value_f64(2400.0));
    REQUIRE(setOk);
    REQUIRE(std::abs(node.cutoff - 2400.0f) < 1e-4f);

    setOk = HostRegistry::setFieldValue(kTestNodeDesc, &node, "active", eats_abi_value_bool(0));
    REQUIRE(setOk);
    REQUIRE(node.active == 0);

    // Struct <-> Eatscript Dict conversion
    Value dictVal = HostRegistry::structToScriptDict(kTestNodeDesc, &node);
    REQUIRE(dictVal.isDict());
    REQUIRE(dictVal.hasKey("nodeId"));
    REQUIRE(dictVal.get("nodeId").asInt() == 42);
    REQUIRE(dictVal.get("cutoff").asFloat() == 2400.0f);
    REQUIRE(dictVal.get("active").asBoolean() == false);
    REQUIRE(dictVal.get("label").asString() == "AcidLead");

    // Mutate in dict and deserialize back into struct
    std::map<std::string, Value> updatedDict = dictVal.dictVal;
    updatedDict["nodeId"] = Value(101);
    updatedDict["cutoff"] = Value(3200.0);
    updatedDict["active"] = Value(true);
    Value updatedVal(updatedDict);

    bool desOk = HostRegistry::scriptDictToStruct(kTestNodeDesc, updatedVal, &node);
    REQUIRE(desOk);
    REQUIRE(node.nodeId == 101);
    REQUIRE(std::abs(node.cutoff - 3200.0f) < 1e-4f);
    REQUIRE(node.active == 1);

    std::cout << "  [PASS] Struct reflection and dynamic field mutation verified." << std::endl;
}

void testHostFunctionDispatchAndMemoryBuffers() {
    std::cout << "[Test 2] Universal C-ABI function registration, buffer management & dispatch..." << std::endl;

    HostRegistry registry;

    // Register C host function: add
    static const uint32_t kAddParamTypes[] = {EATS_ABI_TYPE_F64, EATS_ABI_TYPE_F64};
    EatsFunctionDescriptor addDesc{};
    addDesc.name = "host_add";
    addDesc.docstring = "Adds two 64-bit floating point numbers";
    addDesc.returnType = EATS_ABI_TYPE_F64;
    addDesc.paramCount = 2;
    addDesc.paramTypes = kAddParamTypes;
    addDesc.dispatch = c_host_add;

    REQUIRE(registry.registerFunction(addDesc));

    // Register C host function: buffer_sum
    static const uint32_t kBufParamTypes[] = {EATS_ABI_TYPE_BUFFER};
    EatsFunctionDescriptor bufSumDesc{};
    bufSumDesc.name = "host_buffer_sum";
    bufSumDesc.docstring = "Calculates the sum of float elements in a buffer";
    bufSumDesc.returnType = EATS_ABI_TYPE_F64;
    bufSumDesc.paramCount = 1;
    bufSumDesc.paramTypes = kBufParamTypes;
    bufSumDesc.dispatch = c_host_buffer_sum;

    REQUIRE(registry.registerFunction(bufSumDesc));

    // Invoke through C-ABI interface
    EatsHostInterface hostIface = registry.getHostInterface();
    REQUIRE(hostIface.invoke_host != nullptr);

    EatsAbiValue addArgs[2];
    addArgs[0] = eats_abi_value_f64(18.5);
    addArgs[1] = eats_abi_value_f64(23.5);
    EatsAbiValue addResult = eats_abi_value_void();

    int32_t status = hostIface.invoke_host(hostIface.hostContext, "host_add", addArgs, 2, &addResult);
    REQUIRE(status == EATS_ABI_OK);
    REQUIRE(addResult.type == EATS_ABI_TYPE_F64);
    REQUIRE(std::abs(addResult.as.f64 - 42.0) < 1e-6);

    // Memory buffer allocation & mutation
    constexpr size_t NUM_FLOATS = 128;
    EatsMemoryBuffer buffer = registry.createBuffer(NUM_FLOATS * sizeof(float), sizeof(float), EATS_ABI_TYPE_F32);
    REQUIRE(buffer.data != nullptr);
    REQUIRE(buffer.sizeBytes == NUM_FLOATS * sizeof(float));

    float* floatData = static_cast<float*>(buffer.data);
    double expectedSum = 0.0;
    for (size_t i = 0; i < NUM_FLOATS; ++i) {
        floatData[i] = static_cast<float>(i + 1);
        expectedSum += floatData[i];
    }

    EatsAbiValue bufArgs[1];
    bufArgs[0] = eats_abi_value_buffer(buffer.data, buffer.sizeBytes, buffer.elementSize, buffer.elementType, buffer.flags);
    EatsAbiValue sumResult = eats_abi_value_void();

    status = hostIface.invoke_host(hostIface.hostContext, "host_buffer_sum", bufArgs, 1, &sumResult);
    REQUIRE(status == EATS_ABI_OK);
    REQUIRE(std::abs(sumResult.as.f64 - expectedSum) < 1e-4);

    registry.releaseBuffer(buffer);
    REQUIRE(buffer.data == nullptr);

    std::cout << "  [PASS] C-ABI dispatch and memory buffers verified." << std::endl;
}

void testEatscriptEvaluatorHostBinding() {
    std::cout << "[Test 3] Eatscript Evaluator binding: script-to-host invocation and struct interchange..." << std::endl;

    HostRegistry registry;

    // Register C++ lambda host function: multiply
    registry.registerHostFunction("multiply", EATS_ABI_TYPE_F64,
        {EATS_ABI_TYPE_F64, EATS_ABI_TYPE_F64},
        [](const std::vector<EatsAbiValue>& args) -> EatsAbiValue {
            double a = args.size() > 0 ? args[0].as.f64 : 0.0;
            double b = args.size() > 1 ? args[1].as.f64 : 0.0;
            return eats_abi_value_f64(a * b);
        }, "Multiplies two numbers");

    // Register C host function: c_host_add
    static const uint32_t kAddParams[] = {EATS_ABI_TYPE_F64, EATS_ABI_TYPE_F64};
    EatsFunctionDescriptor addDesc{};
    addDesc.name = "c_add";
    addDesc.returnType = EATS_ABI_TYPE_F64;
    addDesc.paramCount = 2;
    addDesc.paramTypes = kAddParams;
    addDesc.dispatch = c_host_add;
    registry.registerFunction(addDesc);

    Evaluator eval;
    registerStandardLibrary(eval, &registry);

    // Script executing host functions directly and via host namespace
    std::string script = R"(
x = host.invoke("multiply", 6.0, 7.0)
y = host.c_add(10.0, 20.0)
z = multiply(3.0, 4.0)
result = x + y + z
)";

    Value res = eval.evaluateSource(script);
    REQUIRE(eval.getLastError().empty());
    REQUIRE(eval.hasVariable("result"));
    // x = 42, y = 30, z = 12 -> total = 84
    REQUIRE(eval.getVariable("result").asNumber() == 84.0);

    // Host Introspection via Eatscript
    Value funcsList = eval.evaluateSource("host.functions()");
    REQUIRE(funcsList.isList());
    REQUIRE(funcsList.listVal.size() >= 2);

    std::cout << "  [PASS] Evaluator host reflection binding verified." << std::endl;
}

void testStandardLibraryFoundations() {
    std::cout << "[Test 4] Eatscript Standard Library: Console I/O, Math, Strings, System & File I/O..." << std::endl;

    Evaluator eval;
    registerStandardLibrary(eval);

    // 1. Console I/O
    eval.evaluateSource("print(\"Hello\", \"Eatscript\", 42)");
    REQUIRE(eval.getLastError().empty());
    REQUIRE(!eval.getLogs().empty());
    REQUIRE(eval.getLogs().back() == "Hello Eatscript 42");

    eval.evaluateSource("console.warn(\"Warning message\")");
    REQUIRE(eval.getLogs().back() == "[WARN] Warning message");

    // 2. Math Extensions
    Value vClamp = eval.evaluateSource("math.clamp(150.0, 0.0, 100.0)");
    REQUIRE(vClamp.asNumber() == 100.0);

    Value vLerp = eval.evaluateSource("math.lerp(10.0, 20.0, 0.5)");
    REQUIRE(vLerp.asNumber() == 15.0);

    Value vAbs = eval.evaluateSource("abs(-42.5)");
    REQUIRE(vAbs.asNumber() == 42.5);

    Value vMin = eval.evaluateSource("min(10.0, 5.0, 25.0)");
    REQUIRE(vMin.asNumber() == 5.0);

    Value vMax = eval.evaluateSource("max(10.0, 5.0, 25.0)");
    REQUIRE(vMax.asNumber() == 25.0);

    // 3. String Manipulation
    Value vLenStr = eval.evaluateSource("len(\"hello world\")");
    REQUIRE(vLenStr.asInt() == 11);

    Value vUpper = eval.evaluateSource("string.upper(\"synth\")");
    REQUIRE(vUpper.asString() == "SYNTH");

    Value vTrim = eval.evaluateSource("string.trim(\"   acid 303   \")");
    REQUIRE(vTrim.asString() == "acid 303");

    Value vSplit = eval.evaluateSource("string.split(\"a,b,c\", \",\")");
    REQUIRE(vSplit.isList());
    REQUIRE(vSplit.listVal.size() == 3);
    REQUIRE(vSplit.listVal[0].asString() == "a");
    REQUIRE(vSplit.listVal[1].asString() == "b");
    REQUIRE(vSplit.listVal[2].asString() == "c");

    Value vJoin = eval.evaluateSource("string.join(\"-\", [\"kick\", \"snare\", \"hat\"])");
    REQUIRE(vJoin.asString() == "kick-snare-hat");

    Value vReplace = eval.evaluateSource("string.replace(\"foo_bar_foo\", \"foo\", \"baz\")");
    REQUIRE(vReplace.asString() == "baz_bar_baz");

    Value vStarts = eval.evaluateSource("string.starts_with(\"TB-303\", \"TB\")");
    REQUIRE(vStarts.asBoolean() == true);

    Value vContains = eval.evaluateSource("string.contains(\"super_saw_lead\", \"saw\")");
    REQUIRE(vContains.asBoolean() == true);

    // 4. System Utilities
    Value vPlatform = eval.evaluateSource("sys.platform");
    REQUIRE(vPlatform.isString());
    REQUIRE(!vPlatform.asString().empty());

    Value vVer = eval.evaluateSource("sys.version");
    REQUIRE(vVer.asString() == "1.0.0");

    Value vTime = eval.evaluateSource("sys.time()");
    REQUIRE(vTime.asNumber() > 0.0);

    // 5. File I/O
    std::string testPath = "test_scratch_eatscript.txt";
    eval.setVariable("testPath", Value(testPath));

    Value vWrite = eval.evaluateSource("fs.write_file(testPath, \"Eatscript File Content 12345\")");
    REQUIRE(vWrite.asBoolean() == true);

    Value vExists = eval.evaluateSource("fs.exists(testPath)");
    REQUIRE(vExists.asBoolean() == true);

    Value vRead = eval.evaluateSource("fs.read_file(testPath)");
    REQUIRE(vRead.asString() == "Eatscript File Content 12345");

    Value vSize = eval.evaluateSource("fs.file_size(testPath)");
    REQUIRE(vSize.asInt() == 28);

    // Clean up temporary scratch file
    std::filesystem::remove(testPath);
    REQUIRE(!std::filesystem::exists(testPath));

    std::cout << "  [PASS] Standard library foundations passed." << std::endl;
}

int main() {
    std::cout << "=== Running Eatsbits Eatscript Generic Host Reflection ABI & Stdlib Tests ===" << std::endl;

    testHostReflectionABI();
    testHostFunctionDispatchAndMemoryBuffers();
    testEatscriptEvaluatorHostBinding();
    testStandardLibraryFoundations();

    std::cout << "=== ALL EATSCRIPT HOST ABI & STDLIB TESTS PASSED (100% HEADLESS)! ===" << std::endl;
    return 0;
}
