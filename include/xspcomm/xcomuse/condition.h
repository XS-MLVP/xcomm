#ifndef XSPCOMM_XCOMUSE_CONDITION_H
#define XSPCOMM_XCOMUSE_CONDITION_H

#include "xspcomm/xcomuse/callback.h"
#include "xspcomm/common/compare.h"
#include "xspcomm/xdata.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace xspcomm {

    class XClock;
    class ExprEngine;

    // Condition Checker
    class ComUseCondCheck: public ComUseStepCb{
        using XDataCmpFn = bool (*)(XData*, XData*);
        using PtrCmpFn = bool (*)(ComUseCondCheck*, uint64_t, uint64_t, int);
        template <CompareOp Op>
        static bool XDataCmp(XData* a, XData* b) {
            return compare::Compare<Op>(*a, *b);
        }
        template <CompareOp Op>
        static bool PtrCmp(ComUseCondCheck*, uint64_t a, uint64_t b, int bytes) {
            const int order = compare::CompareSignedBytes(
                reinterpret_cast<const unsigned char*>(a),
                reinterpret_cast<const unsigned char*>(b), bytes);
            return compare::Compare<Op>(order, 0);
        }
        static XDataCmpFn SelectXDataCmpFn(CompareOp cmp);
        static PtrCmpFn SelectPtrCmpFn(CompareOp cmp);
        std::vector<XClock*> clk_list;
        struct CondXDataEntry {
            std::string name;
            XData* pin = nullptr;
            XData* val = nullptr;
            CompareOp cmp = CompareOp::EQ;
            XData* valid = nullptr;
            XData* valid_value = nullptr;
            xfunction<bool, XData*, XData*, uint64_t> func = nullptr;
            int triggered = 0;
            uint64_t arg = 0;
            CompareOp valid_cmp = CompareOp::EQ;
            XDataCmpFn cmp_fn = nullptr;
            XDataCmpFn valid_cmp_fn = nullptr;
        };
        struct CondUint64Entry {
            std::string name;
            uint64_t pin_ptr = 0;
            uint64_t val_ptr = 0;
            CompareOp cmp = CompareOp::EQ;
            int bytes = 0;
            uint64_t valid_ptr = 0;
            uint64_t valid_value_ptr = 0;
            int valid_bytes = 1;
            xfunction<bool, uint64_t, uint64_t, uint64_t> func = nullptr;
            int triggered = 0;
            uint64_t arg = 0;
            CompareOp valid_cmp = CompareOp::EQ;
            PtrCmpFn cmp_fn = nullptr;
            PtrCmpFn valid_cmp_fn = nullptr;
        };
        struct ExprEntry {
            std::string name;
            ExprEngine *engine = nullptr;
            int root = -1;
            bool triggered = false;
        };
        std::vector<ExprEntry> expressions;
        std::unordered_map<std::string, size_t> expression_index;
        void RemoveExpression(const std::string &name);
        inline bool Evaluate(CondXDataEntry &entry);
        inline bool Evaluate(CondUint64Entry &entry);
        inline bool Evaluate(ExprEntry &entry);
        std::unordered_map<std::string, size_t> cond_idx_xdata;
        std::unordered_map<std::string, size_t> cond_idx_uint64;
        std::vector<CondXDataEntry> cond_vec_xdata;
        std::vector<CondUint64Entry> cond_vec_uint64;
        static bool RemoveXDataCond(std::unordered_map<std::string, size_t> &idx,
                                    std::vector<CondXDataEntry> &vec,
                                    const std::string &name);
        static bool RemoveUint64Cond(std::unordered_map<std::string, size_t> &idx,
                                     std::vector<CondUint64Entry> &vec,
                                     const std::string &name);
    protected:
        void SetExpression(std::string name, ExprEngine &engine, int root);
    public:
        ComUseCondCheck(XClock* clk=nullptr){if(clk)this->clk_list.push_back(clk);}
        void BindXClock(XClock *clk);
        void SetCondition(std::string unique_name, XData* pin, XData* val, CompareOp cmp, XData *valid = nullptr, XData *valid_value = nullptr, xfunction<bool, XData*, XData*, uint64_t> func = nullptr, uint64_t arg=0);
        void SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, CompareOp cmp, int bytes, uint64_t valid_ptr = 0, uint64_t valid_value_ptr = 0, int valid_bytes = 1, xfunction<bool, uint64_t, uint64_t, uint64_t> func = nullptr, uint64_t arg=0);
        CompareOp GetValidCmpMode(std::string unique_name);
        void SetValidCmpMode(std::string unique_name, CompareOp cmp);
        void RemoveCondition(std::string unique_name);
        std::map<std::string, bool> ListCondition();
        std::vector<std::string> GetTriggeredConditionKeys();
        void ClearClock();
        void ClearCondition();
        void ClearAll(){this->ClearClock(); this->ClearCondition();};
        virtual void Call();
        xfunction<bool, XData*, XData*, uint64_t> AsXDataXFunc(uint64_t func);
        xfunction<bool, uint64_t, uint64_t, uint64_t> AsPtrXFunc(uint64_t func);
    };

} // namespace xspcomm

#endif
