#include "xspcomm/xexpr.h"
#include "xspcomm/common/compare.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace xspcomm {
namespace {
using Bytes = std::vector<unsigned char>;
constexpr uint64_t SignedFlag = uint64_t{1} << 32;
constexpr uint64_t BoolFlag = uint64_t{1} << 33;
ExprOp BitsOp(const ExprNode &node) { return static_cast<ExprOp>(uint32_t(node.imm)); }
bool BitsSigned(const ExprNode &node) { return (node.imm & SignedFlag) != 0; }
bool IsBool(const ExprNode &node) { return (node.imm & BoolFlag) != 0; }

void Normalize(Bytes &value, uint32_t width) {
    value.resize((size_t(width) + 7) / 8, 0);
    if (width % 8) value.back() &= (1u << (width % 8)) - 1;
}
bool Negative(const Bytes &value, uint32_t width) {
    return (value[(width - 1) / 8] >> ((width - 1) % 8)) & 1;
}
bool Nonzero(const Bytes &value) {
    return std::any_of(value.begin(), value.end(), [](auto byte) { return byte != 0; });
}
int CompareBytes(const Bytes &a, const Bytes &b) {
    const memory::ByteWords lhs{a.data(), a.size()}, rhs{b.data(), b.size()};
    return compare::CompareWords(lhs.Count(), lhs, rhs);
}
int CompareBits(const Bytes &a, const Bytes &b, uint32_t width, bool is_signed) {
    if (is_signed) {
        if (width % 8 == 0) return compare::CompareSignedBytes(a.data(), b.data(), a.size());
        const bool anegative = Negative(a, width), bnegative = Negative(b, width);
        if (anegative != bnegative) return anegative ? -1 : 1;
    }
    return CompareBytes(a, b);
}
void ReadSignal(Bytes &result, XData &signal, uint32_t width) {
    // Refresh once, then read the existing XData storage without GetVU8 temporaries.
    const auto scalar = signal.U();
    result.resize((size_t(width) + 7) / 8);
    if (signal.W() == 0) result[0] = static_cast<unsigned char>(scalar);
    else for (size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<unsigned char>(signal.pVecData[i / 4].aval >> (8 * (i % 4)));
}
// These byte operations permit result to alias either operand.
void AddBytes(Bytes &result, const Bytes &a, const Bytes &b, bool subtract = false) {
    result.resize(a.size());
    unsigned carry = subtract;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned sum = unsigned(a[i]) + (subtract ? unsigned(b[i] ^ 255) : b[i]) + carry;
        result[i] = sum & 255;
        carry = sum >> 8;
    }
}
void NegateBytes(Bytes &result, const Bytes &a, uint32_t width) {
    result.resize(a.size());
    unsigned carry = 1;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned sum = unsigned(a[i] ^ 255) + carry;
        result[i] = sum & 255;
        carry = sum >> 8;
    }
    Normalize(result, width);
}
void ShiftBytes(Bytes &result, const Bytes &a, uint32_t width, uint32_t amount, bool left, bool sign) {
    result.assign(a.size(), 0);
    for (uint32_t bit = 0; bit < width; ++bit) {
        const uint64_t source = left ? uint64_t(bit) - amount : uint64_t(bit) + amount;
        const bool value = source < width ? ((a[source / 8] >> (source % 8)) & 1) : (!left && sign);
        if (value) result[bit / 8] |= 1u << (bit % 8);
    }
}
void DivideBytes(Bytes &result, Bytes &remainder, const Bytes &a, const Bytes &b, uint32_t width, bool modulo) {
    if (!Nonzero(b)) throw std::domain_error("typed expression division by zero");
    result.assign(a.size(), 0);
    remainder.assign(a.size(), 0);
    for (uint32_t bit = width; bit-- > 0;) {
        unsigned carry = (a[bit / 8] >> (bit % 8)) & 1;
        for (auto &byte : remainder) {
            const unsigned shifted = unsigned(byte) * 2 + carry;
            byte = shifted & 255;
            carry = shifted >> 8;
        }
        if (CompareBytes(remainder, b) >= 0) {
            AddBytes(remainder, remainder, b, true);
            result[bit / 8] |= 1u << (bit % 8);
        }
    }
    if (modulo) result.assign(remainder.begin(), remainder.end());
}
} // namespace

// Execution scratch belongs to the existing ExprEngine. Values are valid only
// within one evaluation, so signal changes at the same clock tick remain visible.
struct ExprEngine::BitsWorkspace {
    std::vector<Bytes> values;
    std::vector<uint64_t> visits;
    std::vector<EvalFrame> stack;
    std::array<Bytes, 3> division;
    uint64_t epoch = 0;
};

ExprEngine::ExprEngine() = default;
ExprEngine::~ExprEngine() = default;

void ExprEngine::ClearBitsWorkspace() { bits_workspace.reset(); }

const std::vector<unsigned char> &ExprEngine::EvalBitsValue(int root) {
    if (!ValidRoot(root) || nodes[root].op != ExprOp::TYPED)
        throw std::invalid_argument("expected typed expression root");
    if (!bits_workspace) bits_workspace = std::make_unique<BitsWorkspace>();
    auto &work = *bits_workspace;
    work.values.resize(nodes.size());
    work.visits.resize(nodes.size(), 0);
    if (++work.epoch == 0) {
        std::fill(work.visits.begin(), work.visits.end(), 0);
        ++work.epoch;
    }
    work.stack.clear();
    work.stack.push_back({root, 0, 0});
    while (!work.stack.empty()) {
        auto &frame = work.stack.back();
        const int id = frame.id;
        if (work.visits[id] == work.epoch) { work.stack.pop_back(); continue; }
        const auto &node = nodes[id];
        const auto op = BitsOp(node);
        auto &result = work.values[id];
        auto complete = [&] {
            Normalize(result, node.width);
            work.visits[id] = work.epoch;
            work.stack.pop_back();
        };
        if (op == ExprOp::CONST || op == ExprOp::SIGNAL) {
            ReadSignal(result, *(op == ExprOp::CONST ? node.lhs_xdata : node.sig), node.width);
            complete();
            continue;
        }
        if (frame.state == 0) {
            frame.state = 1;
            work.stack.push_back({node.lhs, 0, 0});
            continue;
        }
        const auto &a = work.values[node.lhs];
        const auto &left = nodes[node.lhs];
        const bool negative_a = BitsSigned(left) && Negative(a, left.width);
        if ((op == ExprOp::LAND && !Nonzero(a)) || (op == ExprOp::LOR && Nonzero(a))) {
            result.assign(1, op == ExprOp::LOR);
            complete();
            continue;
        }
        if (frame.state == 1 && node.rhs >= 0) {
            frame.state = 2;
            work.stack.push_back({node.rhs, 0, 0});
            continue;
        }
        if (op == ExprOp::CAST) {
            result.assign(a.begin(), a.end());
            Normalize(result, node.width);
            if (negative_a && node.width > left.width)
                for (uint32_t bit = left.width; bit < node.width; ++bit) result[bit / 8] |= 1u << (bit % 8);
        } else if (op == ExprOp::LNOT) result.assign(1, !Nonzero(a));
        else if (op == ExprOp::BNOT) {
            result.resize(a.size());
            for (size_t i = 0; i < a.size(); ++i) result[i] = a[i] ^ 255;
        } else if (op == ExprOp::SUB && node.rhs < 0) NegateBytes(result, a, node.width);
        else {
            const auto &b = work.values[node.rhs];
            const auto &right = nodes[node.rhs];
            const bool negative_b = BitsSigned(right) && Negative(b, right.width);
            switch (op) {
            case ExprOp::ADD: AddBytes(result, a, b); break;
            case ExprOp::SUB: AddBytes(result, a, b, true); break;
            case ExprOp::MUL:
                result.assign(a.size(), 0);
                for (size_t i = 0; i < a.size(); ++i) {
                    unsigned carry = 0;
                    for (size_t j = 0; i + j < result.size(); ++j) {
                        const unsigned sum = unsigned(a[i]) * b[j] + result[i + j] + carry;
                        result[i + j] = sum & 255;
                        carry = sum >> 8;
                    }
                }
                break;
            case ExprOp::DIV: case ExprOp::MOD: {
                auto &magnitude_a = work.division[0], &magnitude_b = work.division[1];
                if (negative_a) NegateBytes(magnitude_a, a, left.width);
                if (negative_b) NegateBytes(magnitude_b, b, right.width);
                DivideBytes(result, work.division[2], negative_a ? magnitude_a : a,
                            negative_b ? magnitude_b : b, node.width, op == ExprOp::MOD);
                if (op == ExprOp::DIV ? negative_a != negative_b : negative_a)
                    NegateBytes(result, result, node.width);
                break;
            }
            case ExprOp::BAND: case ExprOp::BOR: case ExprOp::BXOR:
                result.resize(a.size());
                for (size_t i = 0; i < a.size(); ++i)
                    result[i] = op == ExprOp::BAND ? a[i] & b[i] : op == ExprOp::BOR ? a[i] | b[i] : a[i] ^ b[i];
                break;
            case ExprOp::SHL: case ExprOp::SHR: {
                if (negative_b) throw std::domain_error("negative typed shift count");
                uint32_t amount = 0;
                for (size_t i = b.size(); i-- > 0;)
                    amount = static_cast<uint32_t>(std::min<uint64_t>(left.width, uint64_t(amount) * 256 + b[i]));
                ShiftBytes(result, a, left.width, amount, op == ExprOp::SHL, negative_a);
                break;
            }
            case ExprOp::LAND: case ExprOp::LOR: result.assign(1, Nonzero(b)); break;
            default:
                result.assign(1, compare::Compare(op, CompareBits(a, b, left.width, BitsSigned(left)), 0));
                break;
            }
        }
        complete();
    }
    return work.values[root];
}

uint64_t ExprEngine::EvalBitsScalar(int root) {
    const auto &bytes = EvalBitsValue(root);
    uint64_t value = 0;
    for (size_t i = 0; i < bytes.size() && i < 8; ++i) value |= uint64_t(bytes[i]) << (8 * i);
    return value;
}

std::vector<unsigned char> ExprEngine::EvalBits(int root) { return EvalBitsValue(root); }

int ExprEngine::NewBitsConst(uint32_t width, bool is_signed, std::vector<unsigned char> &value) {
    if (!width || value.size() != (size_t(width) + 7) / 8 ||
        (width % 8 && (value.back() >> (width % 8))))
        throw std::invalid_argument("invalid typed constant width/bytes");
    ExprNode node; node.op = ExprOp::TYPED; node.width = width;
    node.imm = uint64_t(ExprOp::CONST) | (is_signed ? SignedFlag : 0);
    node.lhs_xdata = MakeConstXDataBytes(width, value);
    nodes.push_back(node); return static_cast<int>(nodes.size()) - 1;
}
int ExprEngine::NewBitsSignal(XData *signal, bool is_signed) {
    if (!signal) throw std::invalid_argument("null typed signal");
    ExprNode node; node.op = ExprOp::TYPED; node.width = std::max(1u, signal->W());
    node.sig = signal; node.imm = uint64_t(ExprOp::SIGNAL) | (is_signed ? SignedFlag : 0);
    nodes.push_back(node); return static_cast<int>(nodes.size()) - 1;
}
int ExprEngine::NewBool(bool value) {
    Bytes bytes{static_cast<unsigned char>(value)};
    const int root = NewBitsConst(1, false, bytes);
    nodes[root].imm |= BoolFlag;
    return root;
}
int ExprEngine::NewBits(ExprOp op, int lhs, int rhs, uint32_t width, bool is_signed) {
    if (!width || !ValidRoot(lhs) || nodes[lhs].op != ExprOp::TYPED ||
        (rhs != -1 && (!ValidRoot(rhs) || nodes[rhs].op != ExprOp::TYPED)))
        throw std::invalid_argument("invalid typed operands");
    const bool unary = op == ExprOp::CAST || op == ExprOp::BNOT || op == ExprOp::LNOT || (op == ExprOp::SUB && rhs == -1);
    const bool logical = op == ExprOp::LAND || op == ExprOp::LOR || op == ExprOp::LNOT;
    const bool comparison = op >= ExprOp::EQ && op <= ExprOp::LE;
    const bool shift = op == ExprOp::SHL || op == ExprOp::SHR;
    if ((!unary && rhs < 0) || (unary && rhs != -1) ||
        (!unary && !logical && !shift &&
         (nodes[lhs].width != nodes[rhs].width || BitsSigned(nodes[lhs]) != BitsSigned(nodes[rhs]) ||
          IsBool(nodes[lhs]) != IsBool(nodes[rhs]))) ||
        (!logical && op != ExprOp::CAST &&
         ((IsBool(nodes[lhs]) && (!comparison || (op != ExprOp::EQ && op != ExprOp::NE))) ||
          (rhs >= 0 && IsBool(nodes[rhs]) && !comparison))) ||
        ((logical || comparison) && (width != 1 || is_signed)) ||
        (!logical && !comparison && op != ExprOp::CAST &&
         (width != nodes[lhs].width || is_signed != BitsSigned(nodes[lhs]))) ||
        !(op == ExprOp::CAST || op == ExprOp::LNOT || op == ExprOp::BNOT ||
          (op >= ExprOp::ADD && op <= ExprOp::LE)))
        throw std::invalid_argument("inconsistent typed expression types");
    ExprNode node; node.op = ExprOp::TYPED; node.width = width; node.lhs = lhs; node.rhs = rhs;
    node.imm = uint64_t(op) | (is_signed ? SignedFlag : 0) | ((logical || comparison) ? BoolFlag : 0);
    nodes.push_back(node); return static_cast<int>(nodes.size()) - 1;
}
} // namespace xspcomm
