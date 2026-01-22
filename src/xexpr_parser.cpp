#include "xspcomm/xexpr.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace xspcomm {
namespace {

enum class TokType {
    End,
    Ident,
    Number,
    Op,
    LParen,
    RParen,
    Comma,
};

struct Token {
    TokType type = TokType::End;
    std::string text;
};

class Lexer {
    std::string input;
    size_t pos = 0;
public:
    explicit Lexer(std::string s) : input(std::move(s)) {}
    Token NextToken(){
        while(pos < input.size() && std::isspace((unsigned char)input[pos])){
            pos++;
        }
        if(pos >= input.size()){
            return Token{TokType::End, ""};
        }
        char c = input[pos];
        // number
        if(std::isdigit((unsigned char)c)){
            size_t start = pos;
            pos++;
            while(pos < input.size()){
                char d = input[pos];
                if(std::isalnum((unsigned char)d) || d == '_' ){
                    pos++;
                    continue;
                }
                break;
            }
            return Token{TokType::Number, input.substr(start, pos - start)};
        }
        // identifier
        if(std::isalpha((unsigned char)c) || c == '_' || c == '$'){
            size_t start = pos;
            pos++;
            while(pos < input.size()){
                char d = input[pos];
                if(std::isalnum((unsigned char)d) || d == '_' || d == '.' || d == '$'){
                    pos++;
                    continue;
                }
                break;
            }
            return Token{TokType::Ident, input.substr(start, pos - start)};
        }
        // paren
        if(c == '('){
            pos++;
            return Token{TokType::LParen, "("};
        }
        if(c == ')'){
            pos++;
            return Token{TokType::RParen, ")"};
        }
        if(c == ','){
            pos++;
            return Token{TokType::Comma, ","};
        }
        // operators (2-char)
        auto two = (pos + 1 < input.size()) ? input.substr(pos, 2) : "";
        if(two == "&&" || two == "||" || two == "==" || two == "!=" ||
           two == ">=" || two == "<=" || two == "<<" || two == ">>"){
            pos += 2;
            return Token{TokType::Op, two};
        }
        // operators (1-char)
        if(std::string("+-*/%&|^~!<>").find(c) != std::string::npos){
            pos++;
            return Token{TokType::Op, std::string(1, c)};
        }
        throw std::runtime_error(std::string("unexpected character: ") + c);
    }
};

struct NodeInfo {
    int id = -1;
    bool is_const = false;
    uint64_t const_val = 0;
    bool is_signal = false;
    uint32_t width = 64;
    XData* sig = nullptr;
};

class ExprParser {
    Lexer lex;
    Token cur;
    ExprEngine &engine;
    XSignalCFG* cfg;
public:
    ExprParser(std::string expr, ExprEngine &eng, XSignalCFG* cfg)
        : lex(std::move(expr)), engine(eng), cfg(cfg) {
        cur = lex.NextToken();
    }

    int Parse(){
        NodeInfo n = ParseOr();
        if(cur.type != TokType::End){
            throw std::runtime_error("unexpected token: " + cur.text);
        }
        return n.id;
    }

private:
    static std::string ToLower(std::string s){
        for(auto &c : s){ c = (char)std::tolower((unsigned char)c); }
        return s;
    }

    bool IsKeyword(const std::string &kw){
        return cur.type == TokType::Ident && ToLower(cur.text) == kw;
    }

    bool IsOp(const std::string &op){
        return cur.type == TokType::Op && cur.text == op;
    }

    void Next(){ cur = lex.NextToken(); }

    NodeInfo MakeConst(uint64_t v){
        NodeInfo n;
        n.id = engine.NewConst(v);
        n.is_const = true;
        n.const_val = v;
        n.width = 64;
        return n;
    }

    NodeInfo MakeSignal(const std::string &name){
        NodeInfo n;
        XData* sig = engine.GetOrCreateSignal(cfg, name);
        n.id = engine.NewSignal(sig);
        n.is_signal = true;
        n.width = sig ? sig->W() : 64;
        n.sig = sig;
        return n;
    }

    void RequireNarrow(const NodeInfo &n, const char *op){
        if(n.is_signal && n.width > 64){
            throw std::runtime_error(std::string("wide signal in '") + op + "' is not supported in v1");
        }
    }

    static uint64_t Mask64(uint64_t v){
        return v & ((uint64_t)-1);
    }

    uint64_t ParseNumber(const std::string &txt){
        std::string s;
        s.reserve(txt.size());
        for(char c : txt){
            if(c != '_') s.push_back(c);
        }
        if(s.size() >= 2 && (s[0] == '0') && (s[1] == 'x' || s[1] == 'X')){
            return std::stoull(s, nullptr, 16);
        }
        if(s.size() >= 2 && (s[0] == '0') && (s[1] == 'b' || s[1] == 'B')){
            uint64_t v = 0;
            for(size_t i = 2; i < s.size(); i++){
                char c = s[i];
                if(c == '0' || c == '1'){
                    v = (v << 1) | (uint64_t)(c - '0');
                }else{
                    throw std::runtime_error("invalid binary literal");
                }
            }
            return v;
        }
        return std::stoull(s, nullptr, 10);
    }

    NodeInfo FoldUnary(const std::string &op, const NodeInfo &child){
        if(!child.is_const) return NodeInfo{};
        uint64_t v = child.const_val;
        if(op == "not"){
            return MakeConst(v ? 0 : 1);
        }
        if(op == "invert"){
            return MakeConst(Mask64(~v));
        }
        if(op == "neg"){
            return MakeConst(Mask64((~v) + 1));
        }
        return NodeInfo{};
    }

    NodeInfo FoldBinary(const std::string &op, const NodeInfo &lhs, const NodeInfo &rhs){
        if(!(lhs.is_const && rhs.is_const)) return NodeInfo{};
        uint64_t a = lhs.const_val;
        uint64_t b = rhs.const_val;
        if(op == "and") return MakeConst((a && b) ? 1 : 0);
        if(op == "or") return MakeConst((a || b) ? 1 : 0);
        if(op == "add") return MakeConst(Mask64(a + b));
        if(op == "sub") return MakeConst(Mask64(a - b));
        if(op == "mul") return MakeConst(Mask64(a * b));
        if(op == "div") return MakeConst(b == 0 ? 0 : Mask64(a / b));
        if(op == "mod") return MakeConst(b == 0 ? 0 : Mask64(a % b));
        if(op == "band") return MakeConst(Mask64(a & b));
        if(op == "bor") return MakeConst(Mask64(a | b));
        if(op == "bxor") return MakeConst(Mask64(a ^ b));
        if(op == "shl") return MakeConst(Mask64(a << (b & 0x3F)));
        if(op == "shr") return MakeConst(Mask64(a >> (b & 0x3F)));
        return NodeInfo{};
    }

    NodeInfo FoldCompare(ExprOp op, const NodeInfo &lhs, const NodeInfo &rhs){
        if(!(lhs.is_const && rhs.is_const)) return NodeInfo{};
        uint64_t a = lhs.const_val;
        uint64_t b = rhs.const_val;
        bool r = false;
        switch(op){
        case ExprOp::EQ: r = (a == b); break;
        case ExprOp::NE: r = (a != b); break;
        case ExprOp::GT: r = (a > b); break;
        case ExprOp::GE: r = (a >= b); break;
        case ExprOp::LT: r = (a < b); break;
        case ExprOp::LE: r = (a <= b); break;
        default: break;
        }
        return MakeConst(r ? 1 : 0);
    }

    NodeInfo BuildCompare(const NodeInfo &lhs, ExprOp op, const NodeInfo &rhs){
        NodeInfo folded = FoldCompare(op, lhs, rhs);
        if(folded.id >= 0) return folded;
        if(lhs.is_signal && rhs.is_signal){
            if(lhs.width != rhs.width){
                throw std::runtime_error("signal width mismatch in comparison");
            }
            if(lhs.width > 64){
                NodeInfo n;
                n.id = engine.NewCompareSigSig(op, lhs.sig, rhs.sig);
                n.width = 1;
                return n;
            }
        }
        if(lhs.is_signal || rhs.is_signal){
            const NodeInfo &sig = lhs.is_signal ? lhs : rhs;
            const NodeInfo &oth = lhs.is_signal ? rhs : lhs;
            if(sig.width > 64){
                if(!oth.is_const && !oth.is_signal){
                    throw std::runtime_error("wide signal compare requires literal constant or signal");
                }
                if(oth.is_signal){
                    throw std::runtime_error("signal width mismatch in comparison");
                }
                NodeInfo n;
                if(lhs.is_signal){
                    n.id = engine.NewCompareSigConst(op, sig.sig, oth.const_val);
                }else{
                    n.id = engine.NewCompareConstSig(op, oth.const_val, sig.sig);
                }
                n.width = 1;
                return n;
            }
        }
        NodeInfo n;
        n.id = engine.NewCompare(op, lhs.id, rhs.id);
        n.width = 1;
        return n;
    }

    NodeInfo ParseOr(){
        NodeInfo lhs = ParseAnd();
        while(IsOp("||") || IsKeyword("or")){
            Next();
            NodeInfo rhs = ParseAnd();
            NodeInfo folded = FoldBinary("or", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::LOR, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseAnd(){
        NodeInfo lhs = ParseNot();
        while(IsOp("&&") || IsKeyword("and")){
            Next();
            NodeInfo rhs = ParseNot();
            NodeInfo folded = FoldBinary("and", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::LAND, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseNot(){
        if(IsOp("!") || IsKeyword("not")){
            Next();
            NodeInfo child = ParseNot();
            NodeInfo folded = FoldUnary("not", child);
            if(folded.id >= 0) return folded;
            NodeInfo n;
            n.id = engine.NewUnary(ExprOp::LNOT, child.id);
            return n;
        }
        return ParseCompare();
    }

    NodeInfo ParseCompare(){
        NodeInfo left = ParseBor();
        if(IsCompareOp()){
            std::vector<NodeInfo> comps;
            while(IsCompareOp()){
                ExprOp op = ParseCompareOp();
                NodeInfo right = ParseBor();
                comps.push_back(BuildCompare(left, op, right));
                left = right;
            }
            if(comps.size() == 1){
                return comps[0];
            }
            NodeInfo cur = comps[0];
            for(size_t i = 1; i < comps.size(); i++){
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::LAND, cur.id, comps[i].id);
                cur = n;
            }
            return cur;
        }
        return left;
    }

    bool IsCompareOp(){
        return IsOp("==") || IsOp("!=") || IsOp(">") || IsOp(">=") || IsOp("<") || IsOp("<=");
    }

    ExprOp ParseCompareOp(){
        if(IsOp("==")){ Next(); return ExprOp::EQ; }
        if(IsOp("!=")){ Next(); return ExprOp::NE; }
        if(IsOp(">")){ Next(); return ExprOp::GT; }
        if(IsOp(">=")){ Next(); return ExprOp::GE; }
        if(IsOp("<")){ Next(); return ExprOp::LT; }
        if(IsOp("<=")){ Next(); return ExprOp::LE; }
        throw std::runtime_error("expected compare operator");
    }

    NodeInfo ParseBor(){
        NodeInfo lhs = ParseBxor();
        while(IsOp("|")){
            Next();
            NodeInfo rhs = ParseBxor();
            RequireNarrow(lhs, "|");
            RequireNarrow(rhs, "|");
            NodeInfo folded = FoldBinary("bor", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::BOR, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseBxor(){
        NodeInfo lhs = ParseBand();
        while(IsOp("^")){
            Next();
            NodeInfo rhs = ParseBand();
            RequireNarrow(lhs, "^");
            RequireNarrow(rhs, "^");
            NodeInfo folded = FoldBinary("bxor", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::BXOR, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseBand(){
        NodeInfo lhs = ParseShift();
        while(IsOp("&")){
            Next();
            NodeInfo rhs = ParseShift();
            RequireNarrow(lhs, "&");
            RequireNarrow(rhs, "&");
            NodeInfo folded = FoldBinary("band", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(ExprOp::BAND, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseShift(){
        NodeInfo lhs = ParseAdd();
        while(IsOp("<<") || IsOp(">>")){
            std::string op = cur.text;
            Next();
            NodeInfo rhs = ParseAdd();
            RequireNarrow(lhs, op.c_str());
            RequireNarrow(rhs, op.c_str());
            NodeInfo folded = FoldBinary(op == "<<" ? "shl" : "shr", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(op == "<<" ? ExprOp::SHL : ExprOp::SHR, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseAdd(){
        NodeInfo lhs = ParseMul();
        while(IsOp("+") || IsOp("-")){
            std::string op = cur.text;
            Next();
            NodeInfo rhs = ParseMul();
            RequireNarrow(lhs, op.c_str());
            RequireNarrow(rhs, op.c_str());
            NodeInfo folded = FoldBinary(op == "+" ? "add" : "sub", lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                NodeInfo n;
                n.id = engine.NewBinary(op == "+" ? ExprOp::ADD : ExprOp::SUB, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseMul(){
        NodeInfo lhs = ParseUnary();
        while(IsOp("*") || IsOp("/") || IsOp("%")){
            std::string op = cur.text;
            Next();
            NodeInfo rhs = ParseUnary();
            RequireNarrow(lhs, op.c_str());
            RequireNarrow(rhs, op.c_str());
            NodeInfo folded = FoldBinary(op == "*" ? "mul" : (op == "/" ? "div" : "mod"), lhs, rhs);
            if(folded.id >= 0){
                lhs = folded;
            }else{
                ExprOp eop = (op == "*") ? ExprOp::MUL : (op == "/" ? ExprOp::DIV : ExprOp::MOD);
                NodeInfo n;
                n.id = engine.NewBinary(eop, lhs.id, rhs.id);
                lhs = n;
            }
        }
        return lhs;
    }

    NodeInfo ParseUnary(){
        if(IsOp("~")){
            Next();
            NodeInfo child = ParseUnary();
            RequireNarrow(child, "~");
            NodeInfo folded = FoldUnary("invert", child);
            if(folded.id >= 0) return folded;
            NodeInfo n;
            n.id = engine.NewUnary(ExprOp::BNOT, child.id);
            return n;
        }
        if(IsOp("-")){
            Next();
            NodeInfo child = ParseUnary();
            RequireNarrow(child, "-");
            NodeInfo folded = FoldUnary("neg", child);
            if(folded.id >= 0) return folded;
            NodeInfo zero = MakeConst(0);
            NodeInfo n;
            n.id = engine.NewBinary(ExprOp::SUB, zero.id, child.id);
            return n;
        }
        if(IsOp("+")){
            Next();
            return ParseUnary();
        }
        return ParsePrimary();
    }

    NodeInfo ParsePrimary(){
        if(cur.type == TokType::Number){
            uint64_t v = ParseNumber(cur.text);
            Next();
            return MakeConst(v);
        }
        if(cur.type == TokType::Ident){
            std::string name = cur.text;
            std::string low = ToLower(name);
            if(low == "within" || low == "hold"){
                Next();
                if(cur.type != TokType::LParen){
                    throw std::runtime_error("expected '(' after " + name);
                }
                Next();
                if(cur.type != TokType::Number){
                    throw std::runtime_error("expected window number in " + name);
                }
                uint64_t window = ParseNumber(cur.text);
                Next();
                if(cur.type != TokType::Comma){
                    throw std::runtime_error("expected ',' in " + name);
                }
                Next();
                NodeInfo child = ParseOr();
                if(cur.type != TokType::RParen){
                    throw std::runtime_error("missing ')' in " + name);
                }
                Next();
                NodeInfo n;
                n.id = (low == "within") ? engine.NewWithin(child.id, window)
                                         : engine.NewHold(child.id, window);
                n.width = 1;
                return n;
            }
            if(low == "and" || low == "or" || low == "not"){
                throw std::runtime_error("unexpected keyword: " + name);
            }
            Next();
            return MakeSignal(name);
        }
        if(cur.type == TokType::LParen){
            Next();
            NodeInfo n = ParseOr();
            if(cur.type != TokType::RParen){
                throw std::runtime_error("missing ')'");
            }
            Next();
            return n;
        }
        throw std::runtime_error("unexpected token: " + cur.text);
    }
};

} // namespace

int ExprEngine::CompileExpr(std::string expr, XSignalCFG* cfg){
    if(expr.empty()){
        throw std::runtime_error("expression is empty");
    }
    ExprParser parser(std::move(expr), *this, cfg);
    int root = parser.Parse();
    this->OptimizeShortCircuitOrder(root);
    return root;
}

} // namespace xspcomm
