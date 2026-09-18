#ifndef EATS_AST_HPP
#define EATS_AST_HPP

#include <string>
#include <vector>
#include <memory>
#include "token.hpp"

namespace eatsbits::eatscript {

struct ASTVisitor;

// Base Expression
struct Expr {
    virtual ~Expr() = default;
    virtual void accept(ASTVisitor* visitor) = 0;
};

// Base Statement
struct Stmt {
    virtual ~Stmt() = default;
    virtual void accept(ASTVisitor* visitor) = 0;
};

// Expressions
struct NumberLiteral : Expr {
    double value{0.0};
    explicit NumberLiteral(double v) : value(v) {}
    void accept(ASTVisitor* visitor) override;
};

struct StringLiteral : Expr {
    std::string value;
    explicit StringLiteral(std::string v) : value(std::move(v)) {}
    void accept(ASTVisitor* visitor) override;
};

struct BooleanLiteral : Expr {
    bool value{false};
    explicit BooleanLiteral(bool v) : value(v) {}
    void accept(ASTVisitor* visitor) override;
};

struct IdentifierExpr : Expr {
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
    void accept(ASTVisitor* visitor) override;
};

struct MemberExpr : Expr {
    std::shared_ptr<Expr> object;
    std::string member;
    MemberExpr(std::shared_ptr<Expr> obj, std::string mem) : object(std::move(obj)), member(std::move(mem)) {}
    void accept(ASTVisitor* visitor) override;
};

struct BinaryExpr : Expr {
    std::shared_ptr<Expr> left;
    TokenType op;
    std::shared_ptr<Expr> right;
    BinaryExpr(std::shared_ptr<Expr> l, TokenType o, std::shared_ptr<Expr> r)
        : left(std::move(l)), op(o), right(std::move(r)) {}
    void accept(ASTVisitor* visitor) override;
};

struct UnaryExpr : Expr {
    TokenType op;
    std::shared_ptr<Expr> operand;
    UnaryExpr(TokenType o, std::shared_ptr<Expr> opnd) : op(o), operand(std::move(opnd)) {}
    void accept(ASTVisitor* visitor) override;
};

struct CallExpr : Expr {
    std::shared_ptr<Expr> callee;
    std::vector<std::shared_ptr<Expr>> args;
    CallExpr(std::shared_ptr<Expr> c, std::vector<std::shared_ptr<Expr>> a)
        : callee(std::move(c)), args(std::move(a)) {}
    void accept(ASTVisitor* visitor) override;
};

struct ListLiteral : Expr {
    std::vector<std::shared_ptr<Expr>> elements;
    explicit ListLiteral(std::vector<std::shared_ptr<Expr>> el) : elements(std::move(el)) {}
    void accept(ASTVisitor* visitor) override;
};

struct DictEntry {
    std::string key;
    std::shared_ptr<Expr> value;
};

struct DictLiteral : Expr {
    std::vector<DictEntry> entries;
    explicit DictLiteral(std::vector<DictEntry> e) : entries(std::move(e)) {}
    void accept(ASTVisitor* visitor) override;
};

// Statements
struct ExprStmt : Stmt {
    std::shared_ptr<Expr> expression;
    explicit ExprStmt(std::shared_ptr<Expr> e) : expression(std::move(e)) {}
    void accept(ASTVisitor* visitor) override;
};

struct AssignStmt : Stmt {
    std::string variableName;
    std::shared_ptr<Expr> value;
    AssignStmt(std::string name, std::shared_ptr<Expr> val) : variableName(std::move(name)), value(std::move(val)) {}
    void accept(ASTVisitor* visitor) override;
};

struct ReturnStmt : Stmt {
    std::shared_ptr<Expr> value;
    explicit ReturnStmt(std::shared_ptr<Expr> val) : value(std::move(val)) {}
    void accept(ASTVisitor* visitor) override;
};

struct IfStmt : Stmt {
    std::shared_ptr<Expr> condition;
    std::vector<std::shared_ptr<Stmt>> thenBranch;
    std::vector<std::shared_ptr<Stmt>> elseBranch;
    IfStmt(std::shared_ptr<Expr> cond, std::vector<std::shared_ptr<Stmt>> t, std::vector<std::shared_ptr<Stmt>> e = {})
        : condition(std::move(cond)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
    void accept(ASTVisitor* visitor) override;
};

struct FunctionDef : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::shared_ptr<Stmt>> body;
    FunctionDef(std::string n, std::vector<std::string> p, std::vector<std::shared_ptr<Stmt>> b)
        : name(std::move(n)), params(std::move(p)), body(std::move(b)) {}
    void accept(ASTVisitor* visitor) override;
};

struct Program {
    std::vector<std::shared_ptr<Stmt>> statements;
};

// Visitor Interface
struct ASTVisitor {
    virtual ~ASTVisitor() = default;
    virtual void visit(NumberLiteral* node) = 0;
    virtual void visit(StringLiteral* node) = 0;
    virtual void visit(BooleanLiteral* node) = 0;
    virtual void visit(IdentifierExpr* node) = 0;
    virtual void visit(MemberExpr* node) = 0;
    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(UnaryExpr* node) = 0;
    virtual void visit(CallExpr* node) = 0;
    virtual void visit(ListLiteral* node) = 0;
    virtual void visit(DictLiteral* node) = 0;
    virtual void visit(ExprStmt* node) = 0;
    virtual void visit(AssignStmt* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(FunctionDef* node) = 0;
};

inline void NumberLiteral::accept(ASTVisitor* v) { v->visit(this); }
inline void StringLiteral::accept(ASTVisitor* v) { v->visit(this); }
inline void BooleanLiteral::accept(ASTVisitor* v) { v->visit(this); }
inline void IdentifierExpr::accept(ASTVisitor* v) { v->visit(this); }
inline void MemberExpr::accept(ASTVisitor* v) { v->visit(this); }
inline void BinaryExpr::accept(ASTVisitor* v) { v->visit(this); }
inline void UnaryExpr::accept(ASTVisitor* v) { v->visit(this); }
inline void CallExpr::accept(ASTVisitor* v) { v->visit(this); }
inline void ListLiteral::accept(ASTVisitor* v) { v->visit(this); }
inline void DictLiteral::accept(ASTVisitor* v) { v->visit(this); }
inline void ExprStmt::accept(ASTVisitor* v) { v->visit(this); }
inline void AssignStmt::accept(ASTVisitor* v) { v->visit(this); }
inline void ReturnStmt::accept(ASTVisitor* v) { v->visit(this); }
inline void IfStmt::accept(ASTVisitor* v) { v->visit(this); }
inline void FunctionDef::accept(ASTVisitor* v) { v->visit(this); }

} // namespace eatsbits::eatscript

#endif // EATS_AST_HPP
