#include "xspcomm/xcomuse/expr.h"
#include "xspcomm/xclock.h"

namespace xspcomm {

int ComUseExprCheck::ExprNewConst(uint64_t v){
    return this->engine.NewConst(v);
}

int ComUseExprCheck::ExprNewSignal(XData* sig){
    return this->engine.NewSignal(sig);
}

int ComUseExprCheck::ExprNewUnary(int op, int child){
    return this->engine.NewUnary(static_cast<ExprOp>(op), child);
}

int ComUseExprCheck::ExprNewBinary(int op, int lhs, int rhs){
    return this->engine.NewBinary(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompare(int op, int lhs, int rhs){
    return this->engine.NewCompare(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigSig(int op, XData* lhs, XData* rhs){
    return this->engine.NewCompareSigSig(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigConst(int op, XData* lhs, uint64_t rhs){
    return this->engine.NewCompareSigConst(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareConstSig(int op, uint64_t lhs, XData* rhs){
    return this->engine.NewCompareConstSig(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigConstBytes(
    int op, XData* lhs, std::vector<unsigned char> &rhs){
    return this->engine.NewCompareSigConstBytes(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareConstBytesSig(
    int op, std::vector<unsigned char> &lhs, XData* rhs){
    return this->engine.NewCompareConstBytesSig(static_cast<ExprOp>(op), lhs, rhs);
}

int ComUseExprCheck::CompileExpr(std::string expr, XSignalCFG* cfg){
    try{
        return this->engine.CompileExpr(expr, cfg);
    }catch(const std::exception &e){
        Error("CompileExpr failed: %s", e.what());
        return -1;
    }
}

void ComUseExprCheck::SetExpr(std::string name, int root) {
    SetExpression(std::move(name), engine, root);
}

void ComUseExprCheck::RemoveExpr(std::string name) { RemoveCondition(std::move(name)); }
std::map<std::string, bool> ComUseExprCheck::ListExpr() { return ListCondition(); }
std::vector<std::string> ComUseExprCheck::GetTriggeredExprKeys() { return GetTriggeredConditionKeys(); }

void ComUseExprCheck::ClearExpr() {
    ClearCondition();
    engine.Clear();
}

void ComUseExprCheck::Call() {
    engine.SetCycle(cycle);
    ComUseCondCheck::Call();
}

} // namespace xspcomm
