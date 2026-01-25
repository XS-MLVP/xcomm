#ifndef __xspcomm_xfsm_h__
#define __xspcomm_xfsm_h__

#include "xspcomm/xexpr.h"

namespace xspcomm {

    class ComUseFsmTrigger: public ComUseStepCb{
        struct FsmTransition{
            int cond_root = -1;
            std::string next_name;
            int next_state = -1;
            bool trigger = false;
            int line = 0;
        };
        struct FsmAction {
            enum class Type{
                SetFlag,
                ClearFlag,
                IncCounter,
                ResetCounter,
            };
            Type type = Type::SetFlag;
            int index = -1;
            void (*exec)(ComUseFsmTrigger*, int) = nullptr;
        };
        struct FsmState{
            std::string name;
            std::vector<FsmAction> actions;
            std::vector<FsmTransition> transitions;
        };
        struct FsmVar{
            std::string name;
            uint64_t value = 0;
            std::unique_ptr<XData> xdata;
        };
        ExprEngine engine;
        std::vector<XClock*> clk_list;
        std::vector<FsmState> states;
        std::map<std::string, int> state_map;
        std::map<std::string, int> flag_map;
        std::map<std::string, int> counter_map;
        std::vector<FsmVar> flags;
        std::vector<FsmVar> counters;
        int start_state = -1;
        int current_state = -1;
        bool triggered = false;
        std::string triggered_state;
        static void ExecSetFlag(ComUseFsmTrigger* self, int idx);
        static void ExecClearFlag(ComUseFsmTrigger* self, int idx);
        static void ExecIncCounter(ComUseFsmTrigger* self, int idx);
        static void ExecResetCounter(ComUseFsmTrigger* self, int idx);
    public:
        ComUseFsmTrigger(XClock* clk=nullptr){if(clk)this->clk_list.push_back(clk);}        
        void BindXClock(XClock *clk);
        void LoadProgram(std::string program, XSignalCFG* cfg);
        void Reset();
        void Clear();
        bool IsTriggered(){return this->triggered;}
        std::string GetTriggeredState(){return this->triggered_state;}
        std::string GetCurrentState();
        std::vector<std::string> ListStates();
        virtual void Call();
    };
}

#endif
