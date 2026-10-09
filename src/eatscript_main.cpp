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
#include "eatsbits/eatscript/dispatch_scanner.hpp"
#include "eatsbits/eatscript/evaluator.hpp"
#include "eatsbits/eatscript/stdlib.hpp"
#include "eatsbits/eatscript/repl.hpp"

using namespace eatsbits;
using namespace eatsbits::eatscript;
using namespace eatsbits::abi;

static void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [command|options] [script.eats]\n\n"
              << "Standalone Eatscript Compiler, Bytecode VM & AOT Transpiler.\n\n"
              << "Commands & Options:\n"
              << "  export --format=clap <file.eats> -o <output.clap>\n"
              << "                            Export patch directly to a standalone native CLAP plugin\n"
              << "  --shell, --repl           Launch the interactive Eatscript REPL shell\n"
              << "  --run <file.eats>         Compile and execute an Eatscript file\n"
              << "  --eval \"<expr>\"           Directly evaluate an expression or statement string\n"
              << "  --bytecode <file.eats>    Compile script to VM bytecode and print disassembly\n"
              << "  --transpile <file.eats>   Transpile Eatscript to pure C-ABI C++ plugin code\n"
              << "  --format=<clap|cpp>       Target format for export (default: clap)\n"
              << "  -o <output.clap|cpp>      Specify output file for export/transpile\n"
              << "  -h, --help                Show this help screen\n"
              << "  -v, --version             Display version information\n\n"
              << "Examples:\n"
              << "  " << progName << "\n"
              << "  " << progName << " --shell\n"
              << "  " << progName << " --eval \"print(math.sin(math.pi * 0.5))\"\n"
              << "  " << progName << " --run script.eats\n"
              << "  " << progName << " --bytecode synth.eats\n"
              << "  " << progName << " --transpile synth.eats -o synth_plugin.cpp\n"
              << "  " << progName << " export --format=clap acid_bass.eats -o acid_bass.clap\n"
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

static int executeExport(const std::string& filePath, const std::string& format, const std::string& outputPath) {
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "Error: File not found: " << filePath << std::endl;
        return 1;
    }

    std::string source = readFileContents(filePath);
    if (source.empty()) {
        std::cerr << "Error: Script file is empty or cannot be read: " << filePath << std::endl;
        return 1;
    }

    auto scanResult = DependencyScanner::scan(source);
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    auto program = parser.parse();
    if (!program) {
        std::cerr << "Eatscript Syntax Error: Failed to parse AST for export: " << filePath << std::endl;
        return 1;
    }

    std::string stem = std::filesystem::path(filePath).stem().string();
    Transpiler transpiler;
    std::string cppCode = transpiler.emitClapPluginEntry(*program, scanResult);

    std::string resolvedOutput = outputPath;
    if (resolvedOutput.empty()) {
        resolvedOutput = (format == "cpp") ? (stem + "_clap.cpp") : (stem + ".clap");
    }

    if (format == "cpp" || resolvedOutput.ends_with(".cpp")) {
        std::ofstream outFile(resolvedOutput);
        if (!outFile.is_open()) {
            std::cerr << "Error: Unable to open output file: " << resolvedOutput << std::endl;
            return 1;
        }
        outFile << cppCode;
        std::cout << "[SUCCESS] Exported CLAP C++ bundle '" << filePath << "' -> '" << resolvedOutput << "'\n";
        return 0;
    }

    // Exporting as compiled .clap binary
    std::string tempCpp = stem + "_bundle.cpp";
    {
        std::ofstream bundleFile(tempCpp);
        if (!bundleFile.is_open()) {
            std::cerr << "Error: Unable to write bundle file: " << tempCpp << std::endl;
            return 1;
        }
        bundleFile << cppCode;
    }

    std::cout << "[INFO] Harvesting dependencies for " << scanResult.metadata.name 
              << " (" << scanResult.metadata.id << ")...\n";
    std::cout << "[INFO] Generated CLAP adapter: " << tempCpp << " (" << cppCode.size() << " bytes)\n";
    std::cout << "[INFO] Compiling native CLAP shared library -> " << resolvedOutput << "...\n";

    std::string compileCmd;
    bool compilerFound = false;

#if defined(_WIN32)
    const std::vector<std::string> vcvarsCandidates = {
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Auxiliary\\Build\\vcvars64.bat",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\VC\\Auxiliary\\Build\\vcvars64.bat"
    };

    std::string vcvarsPath;
    for (const auto& path : vcvarsCandidates) {
        if (std::filesystem::exists(path)) {
            vcvarsPath = path;
            break;
        }
    }

    std::string incDir = "include";
    std::string clapDir = "third_party/clap/include";
    if (!std::filesystem::exists(incDir)) {
        if (std::filesystem::exists("../include")) {
            incDir = "../include";
            clapDir = "../third_party/clap/include";
        }
    }

    std::string includeFlag = "/I\"" + incDir + "\" /I\"" + clapDir + "\"";

    if (!vcvarsPath.empty()) {
        compileCmd = "call \"" + vcvarsPath + "\" >nul 2>&1 && cl /nologo /O2 /LD /std:c++20 /EHsc " 
                   + includeFlag + " \"" + tempCpp + "\" /Fe:\"" + resolvedOutput + "\" /link /INCREMENTAL:NO >nul 2>&1";
        compilerFound = true;
    } else {
        compileCmd = "cl /nologo /O2 /LD /std:c++20 /EHsc " + includeFlag + " \"" + tempCpp + "\" /Fe:\"" + resolvedOutput + "\" >nul 2>&1";
        compilerFound = true;
    }
#else
    std::string includeFlag = "-Iinclude -Ithird_party/clap/include";
    compileCmd = "clang++ -O3 -shared -fPIC -std=c++20 " + includeFlag + " \"" + tempCpp + "\" -o \"" + resolvedOutput + "\" 2>/dev/null";
    compilerFound = true;
#endif

    int ret = -1;
    if (compilerFound) {
        ret = std::system(compileCmd.c_str());
    }

    if (ret == 0 && std::filesystem::exists(resolvedOutput)) {
        auto fileSize = std::filesystem::file_size(resolvedOutput);
        std::cout << "[SUCCESS] Exported CLAP plugin: " << resolvedOutput << " (" << fileSize << " bytes)\n";
        return 0;
    } else {
        std::cerr << "[WARNING] Native compilation exited with code " << ret << ".\n"
                  << "[INFO] Standalone bundle saved to '" << tempCpp << "'.\n"
                  << "[INFO] You can compile manually with: cl.exe /O2 /LD /std:c++20 /Iinclude /Ithird_party/clap/include "
                  << tempCpp << " /Fe:" << resolvedOutput << "\n";
        return (ret == 0) ? 0 : 1;
    }
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
    std::string format = "clap";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        } else if (arg == "export" || arg == "--export") {
            command = "export";
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
        } else if (arg == "--transpile-clap") {
            command = "export";
            format = "cpp";
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                targetFile = argv[++i];
            }
        } else if (arg.rfind("--format=", 0) == 0) {
            format = arg.substr(9);
        } else if (arg == "--format") {
            if (i + 1 < argc) {
                format = argv[++i];
            } else {
                std::cerr << "Error: --format requires a format argument (e.g. clap, cpp).\n";
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
            // Positional argument
            if (targetFile.empty()) {
                if (command.empty()) {
                    command = "run";
                }
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
    } else if (command == "export") {
        if (targetFile.empty()) {
            std::cerr << "Error: export requires a script file path.\n";
            return 1;
        }
        return executeExport(targetFile, format, outputFile);
    } else {
        printUsage(argv[0]);
        return 0;
    }
}
