#include "xspcomm/monitor/expr.h"
#include "xspcomm/xclock.h"

namespace xspcomm {

int XExprCheck::ExprNewConst(uint64_t v){
    return this->engine.NewConst(v);
}

int XExprCheck::ExprNewSignal(XData* sig){
    return this->engine.NewSignal(sig);
}

int XExprCheck::ExprNewUnary(int op, int child){
    return this->engine.NewUnary(static_cast<ExprOp>(op), child);
}

int XExprCheck::ExprNewBinary(int op, int lhs, int rhs){
    return this->engine.NewBinary(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompare(int op, int lhs, int rhs){
    return this->engine.NewCompare(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompareSigSig(int op, XData* lhs, XData* rhs){
    return this->engine.NewCompareSigSig(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompareSigConst(int op, XData* lhs, uint64_t rhs){
    return this->engine.NewCompareSigConst(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompareConstSig(int op, uint64_t lhs, XData* rhs){
    return this->engine.NewCompareConstSig(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompareSigConstBytes(
    int op, XData* lhs, std::vector<unsigned char> &rhs){
    return this->engine.NewCompareSigConstBytes(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::ExprNewCompareConstBytesSig(
    int op, std::vector<unsigned char> &lhs, XData* rhs){
    return this->engine.NewCompareConstBytesSig(static_cast<ExprOp>(op), lhs, rhs);
}

int XExprCheck::CompileExpr(std::string expr, XSignalCFG* cfg){
    try{
        return this->engine.CompileExpr(expr, cfg);
    }catch(const std::exception &e){
        Error("CompileExpr failed: %s", e.what());
        return -1;
    }
}

void XExprCheck::SetExpr(std::string name, int root) {
    SetExpression(std::move(name), engine, root);
}

void XExprCheck::RemoveExpr(std::string name) { RemoveCondition(std::move(name)); }
std::map<std::string, bool> XExprCheck::ListExpr() { return ListCondition(); }
std::vector<std::string> XExprCheck::GetTriggeredExprKeys() { return GetTriggeredConditionKeys(); }

void XExprCheck::ClearExpr() {
    ClearCondition();
    engine.Clear();
}

void XExprCheck::Call() {
    engine.SetCycle(cycle);
    XConditionCheck::Call();
}

} // namespace xspcomm
