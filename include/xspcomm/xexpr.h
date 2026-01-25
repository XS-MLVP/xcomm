#ifndef __xspcomm_xexpr_h__
#define __xspcomm_xexpr_h__

#include "xspcomm/xcomuse_base.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <string>

namespace xspcomm {

    // Expression Engine
    enum class ExprOp{
        CONST = 0,
        SIGNAL = 1,
        ADD = 2,
        SUB = 3,
        MUL = 4,
        DIV = 5,
        MOD = 6,
        BAND = 7,
        BOR = 8,
        BXOR = 9,
        BNOT = 10,
        SHL = 11,
        SHR = 12,
        LAND = 13,
        LOR = 14,
        LNOT = 15,
        EQ = 16,
        NE = 17,
        GT = 18,
        GE = 19,
        LT = 20,
        LE = 21,
        WITHIN = 22,
        HOLD = 23,
    };

    struct ExprNode{
        ExprOp op = ExprOp::CONST;
        int lhs = -1;
        int rhs = -1;
        XData* sig = nullptr;
        uint64_t imm = 0;
        uint32_t width = 64;
        bool use_xdata_cmp = false;
        XData* lhs_xdata = nullptr;
        XData* rhs_xdata = nullptr;
        uint32_t cost = 0;
        uint64_t window = 0;
        uint64_t last_true_cycle = (uint64_t)-1;
        uint64_t last_hold_cycle = (uint64_t)-1;
        uint64_t hold_count = 0;
    };

    class ExprEngine{
        struct EvalFrame{
            int id;
            int state;
            uint64_t lhs_val;
        };
        std::vector<ExprNode> nodes;
        std::vector<std::unique_ptr<XData>> const_xdata;
        std::map<std::string, std::unique_ptr<XData>> signal_xdata;
        std::map<std::string, XData*> external_signal_xdata;
        std::vector<EvalFrame> eval_stack;
        std::vector<uint64_t> eval_vals;
        uint64_t current_cycle = 0;
        uint64_t EvalNode(int id);
        bool EvalCompareNode(const ExprNode &node);
        static bool CompareXData(XData* lhs, XData* rhs, ExprOp op);
        XData* MakeConstXData(uint32_t width, uint64_t value);
        uint64_t EvalIterative(int root);
        int ComputeCost(int id, std::vector<int> &memo);
        int ComputeStateful(int id, std::vector<int> &memo);
        void ReorderShortCircuit(int id, const std::vector<int> &memo,
                                 const std::vector<int> &stateful);
    public:
        int NewConst(uint64_t v);
        int NewSignal(XData* sig);
        int NewUnary(ExprOp op, int child);
        int NewBinary(ExprOp op, int lhs, int rhs);
        int NewCompare(ExprOp op, int lhs, int rhs);
        int NewCompareSigSig(ExprOp op, XData* lhs, XData* rhs);
        int NewCompareSigConst(ExprOp op, XData* lhs, uint64_t rhs);
        int NewCompareConstSig(ExprOp op, uint64_t lhs, XData* rhs);
        int NewWithin(int child, uint64_t window);
        int NewHold(int child, uint64_t window);
        XData* GetOrCreateSignal(XSignalCFG* cfg, const std::string &name);
        void RegisterExternalSignal(const std::string &name, XData* sig);
        void ResetState();
        int CompileExpr(std::string expr, XSignalCFG* cfg);
        uint64_t Eval(int root);
        void OptimizeShortCircuitOrder(int root);
        void SetCycle(uint64_t cycle){ this->current_cycle = cycle; }
        void Clear();
    };

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
