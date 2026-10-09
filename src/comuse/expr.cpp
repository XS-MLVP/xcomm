#include "xspcomm/xcomuse/expr.h"
#include "xspcomm/xclock.h"

namespace xspcomm {

void ComUseExprCheck::BindXClock(XClock *clk){
    this->clk_list.push_back(clk);
}

int ComUseExprCheck::ExprNewConst(uint64_t v){
    return this->engine.NewConst(v);
}

int ComUseExprCheck::ExprNewSignal(XData* sig){
    return this->engine.NewSignal(sig);
}

int ComUseExprCheck::ExprNewUnary(int op, int child){
    return this->engine.NewUnary((ExprOp)op, child);
}

int ComUseExprCheck::ExprNewBinary(int op, int lhs, int rhs){
    return this->engine.NewBinary((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompare(int op, int lhs, int rhs){
    return this->engine.NewCompare((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigSig(int op, XData* lhs, XData* rhs){
    return this->engine.NewCompareSigSig((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigConst(int op, XData* lhs, uint64_t rhs){
    return this->engine.NewCompareSigConst((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareConstSig(int op, uint64_t lhs, XData* rhs){
    return this->engine.NewCompareConstSig((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareSigConstBytes(
    int op, XData* lhs, std::vector<unsigned char> &rhs){
    return this->engine.NewCompareSigConstBytes((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::ExprNewCompareConstBytesSig(
    int op, std::vector<unsigned char> &lhs, XData* rhs){
    return this->engine.NewCompareConstBytesSig((ExprOp)op, lhs, rhs);
}

int ComUseExprCheck::CompileExpr(std::string expr, XSignalCFG* cfg){
    try{
        return this->engine.CompileExpr(expr, cfg);
    }catch(const std::exception &e){
        Error("CompileExpr failed: %s", e.what());
        return -1;
    }
}

void ComUseExprCheck::SetExpr(std::string name, int root){
    auto it = this->expr_index.find(name);
    if(it == this->expr_index.end()){
        ExprItem item;
        item.name = name;
        item.root = root;
        this->expr_list.push_back(std::move(item));
        this->expr_index[name] = this->expr_list.size() - 1;
    }else{
        this->expr_list[it->second].root = root;
    }
}

void ComUseExprCheck::RemoveExpr(std::string name){
    auto it = this->expr_index.find(name);
    if(it == this->expr_index.end()) return;
    size_t at = it->second;
    size_t last = this->expr_list.size() - 1;
    if(at != last){
        this->expr_list[at] = std::move(this->expr_list[last]);
        this->expr_index[this->expr_list[at].name] = at;
    }
    this->expr_list.pop_back();
    this->expr_index.erase(it);
}

std::map<std::string, bool> ComUseExprCheck::ListExpr(){
    std::map<std::string, bool> ret;
    for(const auto &item : this->expr_list){
        ret[item.name] = (item.last_trigger_cycle == this->last_eval_cycle);
    }
    return ret;
}

std::vector<std::string> ComUseExprCheck::GetTriggeredExprKeys(){
    std::vector<std::string> ret;
    for(const auto &item : this->expr_list){
        if(item.last_trigger_cycle == this->last_eval_cycle){
            ret.push_back(item.name);
        }
    }
    return ret;
}

void ComUseExprCheck::ClearExpr(){
    this->expr_index.clear();
    this->expr_list.clear();
    this->last_eval_cycle = 0;
    this->engine.Clear();
}

void ComUseExprCheck::Call(){
    if (likely(this->expr_list.empty())) {
        return;
    }
    bool triggered = false;
    this->engine.SetCycle(this->cycle);
    this->last_eval_cycle = this->cycle;
    for(auto &item : this->expr_list){
        if(item.root < 0) continue;
        uint64_t v = this->engine.Eval(item.root);
        if(v != 0){
            item.last_trigger_cycle = this->last_eval_cycle;
            if(!triggered){
                for(auto &clk : this->clk_list){
                    clk->Disable();
                }
                triggered = true;
                this->IncCbCount();
            }
        }
    }
}

} // namespace xspcomm
