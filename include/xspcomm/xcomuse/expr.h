#ifndef XSPCOMM_XCOMUSE_EXPR_H
#define XSPCOMM_XCOMUSE_EXPR_H

#include "xspcomm/xcomuse/callback.h"
#include "xspcomm/xexpr.h"

#include <unordered_map>

namespace xspcomm {

class XClock;

    class ComUseExprCheck: public ComUseStepCb{
        ExprEngine engine;
        struct ExprItem{
            std::string name;
            int root = -1;
            uint64_t last_trigger_cycle = (uint64_t)-1;
        };
        std::vector<XClock*> clk_list;
        std::vector<ExprItem> expr_list;
        std::unordered_map<std::string, size_t> expr_index;
        uint64_t last_eval_cycle = 0;
    public:
        ComUseExprCheck(XClock* clk=nullptr){if(clk)this->clk_list.push_back(clk);}
        void BindXClock(XClock *clk);
        int ExprNewConst(uint64_t v);
        int ExprNewSignal(XData* sig);
        int ExprNewUnary(int op, int child);
        int ExprNewBinary(int op, int lhs, int rhs);
        int ExprNewCompare(int op, int lhs, int rhs);
        int ExprNewCompareSigSig(int op, XData* lhs, XData* rhs);
        int ExprNewCompareSigConst(int op, XData* lhs, uint64_t rhs);
        int ExprNewCompareConstSig(int op, uint64_t lhs, XData* rhs);
        int ExprNewCompareSigConstBytes(int op, XData* lhs,
                                        std::vector<unsigned char> &rhs);
        int ExprNewCompareConstBytesSig(int op,
                                        std::vector<unsigned char> &lhs,
                                        XData* rhs);
        int CompileExpr(std::string expr, XSignalCFG* cfg);
        void SetExpr(std::string name, int root);
        void RemoveExpr(std::string name);
        std::map<std::string, bool> ListExpr();
        std::vector<std::string> GetTriggeredExprKeys();
        void ClearExpr();
        void ClearAll(){this->ClearExpr();};
        virtual void Call();
    };
}

#endif
