#ifndef EATS_BYTECODE_HPP
#define EATS_BYTECODE_HPP

#include <vector>
#include <cstdint>
#include <string>

namespace eatsbits::eatscript {

enum class OpCode : uint8_t {
    Constant,       // [op, const_idx]
    GetLocal,       // [op, slot]
    SetLocal,       // [op, slot]
    GetParam,       // [op, param_idx]
    
    Add,            // a + b
    Subtract,       // a - b
    Multiply,       // a * b
    Divide,         // a / b
    Modulo,         // a % b
    Power,          // a ** b
    Negate,         // -a

    Equal,          // a == b
    NotEqual,       // a != b
    Less,           // a < b
    LessEqual,      // a <= b
    Greater,        // a > b
    GreaterEqual,   // a >= b

    MathSin,        // sin(a)
    MathCos,        // cos(a)
    MathTanh,       // tanh(a)
    MathExp,        // exp(a)
    MathFloor,      // floor(a)
    MathSqrt,       // sqrt(a)
    MathRandom,     // random()

    Jump,           // [op, offset_hi, offset_lo]
    JumpIfFalse,    // [op, offset_hi, offset_lo]
    Return          // returns top of stack
};

struct Chunk {
    std::vector<uint8_t> code;
    std::vector<double> constants;
    std::vector<std::string> localNames;

    size_t addConstant(double val) {
        constants.push_back(val);
        return constants.size() - 1;
    }

    void emitByte(uint8_t byte) {
        code.push_back(byte);
    }

    void emitOp(OpCode op) {
        code.push_back(static_cast<uint8_t>(op));
    }
};

} // namespace eatsbits::eatscript

#endif // EATS_BYTECODE_HPP
