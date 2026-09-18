#ifndef EATS_VM_HPP
#define EATS_VM_HPP

#include <array>
#include <string>
#include <memory>
#include <unordered_map>
#include "bytecode.hpp"
#include "ast.hpp"

namespace eatsbits::eatscript {

/**
 * High-Performance, Allocation-Free Bytecode Virtual Machine for Eatscript.
 * Executes live scripts during real-time audio synthesis callbacks.
 */
class VM {
public:
    VM() noexcept;

    // Compilation (offline / hot-reloading thread)
    bool compileSource(const std::string& source);
    bool compileProgram(const Program& program);

    // Real-Time Safe Execution (audio thread callback)
    [[nodiscard]] double executeProcess(double time, double freq, double note,
                                       const float* params = nullptr, size_t numParams = 0) noexcept;

    [[nodiscard]] bool hasCompiledProcess() const noexcept {
        return hasProcess_;
    }

    [[nodiscard]] const Chunk& getProcessChunk() const noexcept {
        return processChunk_;
    }

    // Set parameter value
    void setParam(uint32_t id, float value) noexcept;
    [[nodiscard]] float getParam(uint32_t id) const noexcept;

private:
    bool compileFunction(FunctionDef* func);
    void compileStmt(Stmt* stmt);
    void compileExpr(Expr* expr);

    int resolveLocal(const std::string& name);
    int addLocal(const std::string& name);

    Chunk processChunk_;
    bool hasProcess_{false};

    // Pre-allocated runtime evaluation environment (Zero runtime allocations)
    static constexpr size_t STACK_MAX = 256;
    static constexpr size_t LOCALS_MAX = 64;
    static constexpr size_t PARAMS_MAX = 64;

    alignas(64) std::array<double, STACK_MAX> stack_{};
    alignas(64) std::array<double, LOCALS_MAX> locals_{};
    alignas(64) std::array<float, PARAMS_MAX> params_{};

    std::vector<std::string> localSymbolTable_;
    uint32_t rngState_{123456789};
};

} // namespace eatsbits::eatscript

#endif // EATS_VM_HPP
