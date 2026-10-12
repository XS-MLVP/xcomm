#ifndef XSPCOMM_MONITOR_EXPR_H
#define XSPCOMM_MONITOR_EXPR_H

#include "xspcomm/monitor/condition.h"
#include "xspcomm/xexpr.h"


namespace xspcomm {

class XClock;

    class XExprCheck: public XConditionCheck{
        ExprEngine engine;
    public:
        XExprCheck(XClock* clk=nullptr) : XConditionCheck(clk) {}
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
