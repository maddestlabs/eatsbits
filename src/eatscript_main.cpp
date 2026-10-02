#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include "eatsbits/abi/eats_host_abi.h"
#include "eatsbits/abi/host_registry.hpp"
#include "eatsbits/eatscript/lexer.hpp"
#include "eatsbits/eatscript/parser.hpp"
#include "eatsbits/eatscript/vm.hpp"
#include "eatsbits/eatscript/bytecode.hpp"
#include "eatsbits/eatscript/transpiler.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/eatscript/stdlib.hpp"
#include "eatsbits/eatscript/repl.hpp"

using namespace eatsbits;
using namespace eatsbits::eatscript;
using namespace eatsbits::abi;

static void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options] [script.eats]\n\n"
              << "Standalone Eatscript Compiler, Bytecode VM & AOT Transpiler.\n\n"
              << "Commands & Options:\n"
              << "  --shell, --repl           Launch the interactive Eatscript REPL shell\n"
              << "  --run <file.eats>         Compile and execute an Eatscript file\n"
              << "  --eval \"<expr>\"           Directly evaluate an expression or statement string\n"
              << "  --bytecode <file.eats>    Compile script to VM bytecode and print disassembly\n"
              << "  --transpile <file.eats>   Transpile Eatscript to pure C-ABI C++ plugin code\n"
              << "  -o <output.cpp>           Specify output file for --transpile\n"
              << "  -h, --help                Show this help screen\n"
              << "  -v, --version             Display version information\n\n"
              << "Examples:\n"
              << "  " << progName << "\n"
              << "  " << progName << " --shell\n"
              << "  " << progName << " --eval \"print(math.sin(math.pi * 0.5))\"\n"
              << "  " << progName << " --run script.eats\n"
              << "  " << progName << " --bytecode synth.eats\n"
              << "  " << progName << " --transpile synth.eats -o synth_plugin.cpp\n"
              << std::endl;
}

static void printVersion() {
    std::cout << "eatscript version 1.0.0\n"
              << "Eatsbits Universal Host ABI v"
              << EATS_HOST_ABI_VERSION_MAJOR << "." << EATS_HOST_ABI_VERSION_MINOR
              << " (Standard Library + Reflection Enabled)\n";
}

static std::string readFileContents(const std::string& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static int executeRun(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Error: File not found: " << filePath << std::endl;
        return 1;
    }

    std::string source = readFileContents(filePath);
    if (source.empty()) {
        std::cerr << "Error: Script file is empty or cannot be read: " << filePath << std::endl;
        return 1;
    }

    HostRegistry hostRegistry;
    registerHostStdlib(hostRegistry);

    Evaluator evaluator;
    registerStandardLibrary(evaluator, &hostRegistry);

    Value result = evaluator.evaluateSource(source);
    if (!evaluator.getLastError().empty()) {
        std::cerr << "Eatscript Error: " << evaluator.getLastError() << std::endl;
        return 1;
    }

    // If script defines main(), invoke it
    if (evaluator.hasFunction("main")) {
        result = evaluator.callFunction("main");
        if (!evaluator.getLastError().empty()) {
            std::cerr << "Eatscript Runtime Error in main(): " << evaluator.getLastError() << std::endl;
            return 1;
        }
    }

    if (!result.isNil()) {
        std::cout << result.toString() << std::endl;
    }

    return 0;
}

static int executeEval(const std::string& expression) {
    HostRegistry hostRegistry;
    registerHostStdlib(hostRegistry);

    Evaluator evaluator;
    registerStandardLibrary(evaluator, &hostRegistry);

    Value result = evaluator.evaluateSource(expression);
    if (!evaluator.getLastError().empty()) {
        std::cerr << "Eatscript Error: " << evaluator.getLastError() << std::endl;
        return 1;
    }

    if (!result.isNil()) {
        std::cout << result.toString() << std::endl;
    }

    return 0;
}

static int executeBytecode(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Error: File not found: " << filePath << std::endl;
        return 1;
    }

    std::string source = readFileContents(filePath);
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
    if (!program) {
        std::cerr << "Eatscript Syntax Error: Failed to parse AST" << std::endl;
        return 1;
    }

    VM vm;
    if (!vm.compileProgram(*program)) {
        std::cerr << "Eatscript VM Error: Failed to compile program to bytecode chunk" << std::endl;
        return 1;
    }

    std::string filename = std::filesystem::path(filePath).filename().string();
    std::cout << disassembleChunk(vm.getProcessChunk(), filename);
    return 0;
}

static int executeTranspile(const std::string& filePath, const std::string& outputPath) {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Error: File not found: " << filePath << std::endl;
        return 1;
    }

    std::string source = readFileContents(filePath);
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
    if (!program) {
        std::cerr << "Eatscript Syntax Error: Failed to parse AST for transpilation" << std::endl;
        return 1;
    }

    std::string stem = std::filesystem::path(filePath).stem().string();
    Transpiler transpiler;
    std::string cppCode = transpiler.transpile(*program, stem, stem);

    if (!outputPath.empty()) {
        std::ofstream outFile(outputPath);
        if (!outFile.is_open()) {
            std::cerr << "Error: Unable to open output file: " << outputPath << std::endl;
            return 1;
        }
        outFile << cppCode;
        std::cout << "Successfully transpiled '" << filePath << "' -> '" << outputPath << "'\n";
    } else {
        std::cout << cppCode << std::endl;
    }

    return 0;
}

static int executeRepl() {
    ReplEngine repl;
    std::cout << "Eatscript 1.0.0 Interactive Shell\n"
              << "Type \"exit\" or \"quit\" to exit.\n\n";

    std::string line;
    while (true) {
        std::cout << repl.getCurrentPrompt();
        std::cout.flush();
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            break;
        }
        if (line == "exit" || line == "quit" || line == "exit()" || line == "quit()") {
            break;
        }
        ReplResult res = repl.feedLine(line);
        if (res.status == ReplResult::Status::Error) {
            std::cerr << "Eatscript Error: " << res.error << "\n";
        } else if (res.status == ReplResult::Status::Complete) {
            if (!res.output.empty()) {
                std::cout << res.output << "\n";
            }
        }
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return executeRepl();
    }

    std::string command;
    std::string targetFile;
    std::string evalExpr;
    std::string outputFile;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "--shell" || arg == "--repl") {
            command = "shell";
        } else if (arg == "--run") {
            if (i + 1 < argc) {
                command = "run";
                targetFile = argv[++i];
            } else {
                std::cerr << "Error: --run requires a file path argument.\n";
                return 1;
            }
        } else if (arg == "--eval") {
            if (i + 1 < argc) {
                command = "eval";
                evalExpr = argv[++i];
            } else {
                std::cerr << "Error: --eval requires an expression string.\n";
                return 1;
            }
        } else if (arg == "--bytecode") {
            if (i + 1 < argc) {
                command = "bytecode";
                targetFile = argv[++i];
            } else {
                std::cerr << "Error: --bytecode requires a file path argument.\n";
                return 1;
            }
        } else if (arg == "--transpile") {
            if (i + 1 < argc) {
                command = "transpile";
                targetFile = argv[++i];
            } else {
                std::cerr << "Error: --transpile requires a file path argument.\n";
                return 1;
            }
        } else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                outputFile = argv[++i];
            } else {
                std::cerr << "Error: -o requires a file path argument.\n";
                return 1;
            }
        } else if (!arg.empty() && arg[0] != '-') {
            // Positional argument: file to run by default
            if (targetFile.empty()) {
                command = "run";
                targetFile = arg;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (command == "shell") {
        return executeRepl();
    } else if (command == "run") {
        return executeRun(targetFile);
    } else if (command == "eval") {
        return executeEval(evalExpr);
    } else if (command == "bytecode") {
        return executeBytecode(targetFile);
    } else if (command == "transpile") {
        return executeTranspile(targetFile, outputFile);
    } else {
        printUsage(argv[0]);
        return 0;
    }
}
