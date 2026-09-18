#ifndef EATS_TRANSPILER_HPP
#define EATS_TRANSPILER_HPP

#include <string>
#include <memory>
#include "ast.hpp"

namespace eatsbits::eatscript {

/**
 * Ahead-Of-Time (AOT) C++ Transpiler for Eatscript scripts.
 * Emits pure C++ structs and C-ABI export functions matching EatsPluginDescriptor.
 */
class Transpiler : public ASTVisitor {
public:
    Transpiler() = default;

    std::string transpile(const Program& program, const std::string& pluginId = "my_synth",
                          const std::string& pluginName = "My Synth");

    std::string transpileSource(const std::string& source, const std::string& pluginId = "my_synth",
                                const std::string& pluginName = "My Synth");

    // AST Visitor methods
    void visit(NumberLiteral* node) override;
    void visit(StringLiteral* node) override;
    void visit(BooleanLiteral* node) override;
    void visit(IdentifierExpr* node) override;
    void visit(MemberExpr* node) override;
    void visit(BinaryExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(CallExpr* node) override;
    void visit(ListLiteral* node) override;
    void visit(DictLiteral* node) override;
    void visit(ExprStmt* node) override;
    void visit(AssignStmt* node) override;
    void visit(ReturnStmt* node) override;
    void visit(IfStmt* node) override;
    void visit(FunctionDef* node) override;

private:
    std::string emitIndent() const;

    std::string output_;
    std::string processBodyCode_;
    int indentLevel_{0};
    bool inProcessFunction_{false};
};

} // namespace eatsbits::eatscript

#endif // EATS_TRANSPILER_HPP
