#include "xspcomm/xfsm.h"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace xspcomm {

namespace {
std::string ToLowerCopy(std::string s){
    for(auto &c : s){
        c = (char)std::tolower((unsigned char)c);
    }
    return s;
}

std::string TrimCopy(const std::string &s){
    size_t start = 0;
    while(start < s.size() && std::isspace((unsigned char)s[start])){
        start++;
    }
    size_t end = s.size();
    while(end > start && std::isspace((unsigned char)s[end - 1])){
        end--;
    }
    return s.substr(start, end - start);
}

std::string StripTrailingSemicolon(const std::string &s){
    std::string out = TrimCopy(s);
    if(!out.empty() && out.back() == ';'){
        out.pop_back();
        return TrimCopy(out);
    }
    return out;
}

bool StartsWithWord(const std::string &s, const std::string &word, std::string *rest){
    if(s.size() < word.size()) return false;
    auto head = s.substr(0, word.size());
    if(ToLowerCopy(head) != ToLowerCopy(word)) return false;
    if(s.size() > word.size()){
        char c = s[word.size()];
        if(!std::isspace((unsigned char)c)) return false;
    }
    if(rest){
        *rest = TrimCopy(s.substr(word.size()));
    }
    return true;
}

bool IsFlagName(const std::string &name){
    std::string low = ToLowerCopy(name);
    return low.rfind("$flag", 0) == 0;
}

bool IsCounterName(const std::string &name){
    std::string low = ToLowerCopy(name);
    return low.rfind("$counter", 0) == 0;
}

bool FindKeywordOutsideParen(const std::string &line, const std::string &kw, size_t &pos){
    int depth = 0;
    std::string low = ToLowerCopy(line);
    std::string lkw = ToLowerCopy(kw);
    for(size_t i = 0; i + lkw.size() <= low.size(); i++){
        char c = low[i];
        if(c == '(') depth++;
        else if(c == ')') depth--;
        if(depth != 0) continue;
        if(low.compare(i, lkw.size(), lkw) == 0){
            bool left_ok = (i == 0) || std::isspace((unsigned char)low[i - 1]);
            bool right_ok = (i + lkw.size() >= low.size()) ||
                            std::isspace((unsigned char)low[i + lkw.size()]);
            if(left_ok && right_ok){
                pos = i;
                return true;
            }
        }
    }
    return false;
}

void CollectSpecialNames(const std::string &expr,
                         std::vector<std::string> &flags,
                         std::vector<std::string> &counters){
    size_t i = 0;
    while(i < expr.size()){
        unsigned char c = (unsigned char)expr[i];
        if(std::isalpha(c) || c == '_' || c == '$'){
            size_t start = i;
            i++;
            while(i < expr.size()){
                unsigned char d = (unsigned char)expr[i];
                if(std::isalnum(d) || d == '_' || d == '.' || d == '$'){
                    i++;
                    continue;
                }
                break;
            }
            std::string name = expr.substr(start, i - start);
            if(IsFlagName(name)){
                flags.push_back(name);
            }else if(IsCounterName(name)){
                counters.push_back(name);
            }
            continue;
        }
        i++;
    }
}
} // namespace
void ComUseFsmTrigger::ExecSetFlag(ComUseFsmTrigger* self, int idx){
    if(idx >= 0 && idx < (int)self->flags.size()){
        self->flags[idx].value = 1;
        (*self->flags[idx].xdata) = (uint64_t)1;
    }
}

void ComUseFsmTrigger::ExecClearFlag(ComUseFsmTrigger* self, int idx){
    if(idx >= 0 && idx < (int)self->flags.size()){
        self->flags[idx].value = 0;
        (*self->flags[idx].xdata) = (uint64_t)0;
    }
}

void ComUseFsmTrigger::ExecIncCounter(ComUseFsmTrigger* self, int idx){
    if(idx >= 0 && idx < (int)self->counters.size()){
        self->counters[idx].value += 1;
        (*self->counters[idx].xdata) = self->counters[idx].value;
    }
}

void ComUseFsmTrigger::ExecResetCounter(ComUseFsmTrigger* self, int idx){
    if(idx >= 0 && idx < (int)self->counters.size()){
        self->counters[idx].value = 0;
        (*self->counters[idx].xdata) = (uint64_t)0;
    }
}

void ComUseFsmTrigger::BindXClock(XClock *clk){
    this->clk_list.push_back(clk);
}

void ComUseFsmTrigger::LoadProgram(std::string program, XSignalCFG* cfg){
    this->Clear();
    try{
    if(program.empty()){
        throw std::runtime_error("fsm program is empty");
    }
    std::string start_state_name;
    FsmState *cur_state = nullptr;
    int line_no = 0;
    size_t pos = 0;
    while(pos < program.size()){
        size_t nl = program.find('\n', pos);
        std::string line = (nl == std::string::npos) ? program.substr(pos)
                                                     : program.substr(pos, nl - pos);
        pos = (nl == std::string::npos) ? program.size() : nl + 1;
        line_no++;
        size_t cpos = line.find('#');
        if(cpos != std::string::npos){
            line = line.substr(0, cpos);
        }
        line = StripTrailingSemicolon(line);
        if(line.empty()){
            continue;
        }
        std::string rest;
        if(StartsWithWord(line, "start", &rest)){
            if(rest.empty()){
                throw std::runtime_error("line " + std::to_string(line_no) + ": start needs a state name");
            }
            start_state_name = rest;
            continue;
        }
        if(StartsWithWord(line, "state", &rest)){
            if(rest.empty() || rest.back() != ':'){
                throw std::runtime_error("line " + std::to_string(line_no) + ": state line must end with ':'");
            }
            rest.pop_back();
            rest = TrimCopy(rest);
            if(rest.empty()){
                throw std::runtime_error("line " + std::to_string(line_no) + ": state name is empty");
            }
            if(this->state_map.find(rest) != this->state_map.end()){
                throw std::runtime_error("line " + std::to_string(line_no) + ": duplicate state " + rest);
            }
            FsmState st;
            st.name = rest;
            this->state_map[rest] = (int)this->states.size();
            this->states.push_back(st);
            cur_state = &this->states.back();
            continue;
        }
        if(cur_state == nullptr){
            throw std::runtime_error("line " + std::to_string(line_no) + ": statement outside state");
        }
        std::string action_rest;
        if(StartsWithWord(line, "set", &action_rest)){
            if(!IsFlagName(action_rest)){
                throw std::runtime_error("line " + std::to_string(line_no) + ": set expects $flag*");
            }
            auto it = this->flag_map.find(action_rest);
            int idx = -1;
            if(it == this->flag_map.end()){
                FsmVar v;
                v.name = action_rest;
                v.xdata = std::make_unique<XData>(1, XData::InOut);
                (*v.xdata) = (uint64_t)0;
                idx = (int)this->flags.size();
                this->flags.push_back(std::move(v));
                this->flag_map[action_rest] = idx;
                this->engine.RegisterExternalSignal(action_rest, this->flags[idx].xdata.get());
            }else{
                idx = it->second;
            }
            FsmAction act;
            act.type = FsmAction::Type::SetFlag;
            act.index = idx;
            act.exec = &ComUseFsmTrigger::ExecSetFlag;
            cur_state->actions.push_back(act);
            continue;
        }
        if(StartsWithWord(line, "clear", &action_rest)){
            if(!IsFlagName(action_rest)){
                throw std::runtime_error("line " + std::to_string(line_no) + ": clear expects $flag*");
            }
            auto it = this->flag_map.find(action_rest);
            int idx = -1;
            if(it == this->flag_map.end()){
                FsmVar v;
                v.name = action_rest;
                v.xdata = std::make_unique<XData>(1, XData::InOut);
                (*v.xdata) = (uint64_t)0;
                idx = (int)this->flags.size();
                this->flags.push_back(std::move(v));
                this->flag_map[action_rest] = idx;
                this->engine.RegisterExternalSignal(action_rest, this->flags[idx].xdata.get());
            }else{
                idx = it->second;
            }
            FsmAction act;
            act.type = FsmAction::Type::ClearFlag;
            act.index = idx;
            act.exec = &ComUseFsmTrigger::ExecClearFlag;
            cur_state->actions.push_back(act);
            continue;
        }
        if(StartsWithWord(line, "inc", &action_rest)){
            if(!IsCounterName(action_rest)){
                throw std::runtime_error("line " + std::to_string(line_no) + ": inc expects $counter*");
            }
            auto it = this->counter_map.find(action_rest);
            int idx = -1;
            if(it == this->counter_map.end()){
                FsmVar v;
                v.name = action_rest;
                v.xdata = std::make_unique<XData>(64, XData::InOut);
                (*v.xdata) = (uint64_t)0;
                idx = (int)this->counters.size();
                this->counters.push_back(std::move(v));
                this->counter_map[action_rest] = idx;
                this->engine.RegisterExternalSignal(action_rest, this->counters[idx].xdata.get());
            }else{
                idx = it->second;
            }
            FsmAction act;
            act.type = FsmAction::Type::IncCounter;
            act.index = idx;
            act.exec = &ComUseFsmTrigger::ExecIncCounter;
            cur_state->actions.push_back(act);
            continue;
        }
        if(StartsWithWord(line, "reset", &action_rest)){
            if(!IsCounterName(action_rest)){
                throw std::runtime_error("line " + std::to_string(line_no) + ": reset expects $counter*");
            }
            auto it = this->counter_map.find(action_rest);
            int idx = -1;
            if(it == this->counter_map.end()){
                FsmVar v;
                v.name = action_rest;
                v.xdata = std::make_unique<XData>(64, XData::InOut);
                (*v.xdata) = (uint64_t)0;
                idx = (int)this->counters.size();
                this->counters.push_back(std::move(v));
                this->counter_map[action_rest] = idx;
                this->engine.RegisterExternalSignal(action_rest, this->counters[idx].xdata.get());
            }else{
                idx = it->second;
            }
            FsmAction act;
            act.type = FsmAction::Type::ResetCounter;
            act.index = idx;
            act.exec = &ComUseFsmTrigger::ExecResetCounter;
            cur_state->actions.push_back(act);
            continue;
        }
        auto parse_transition = [&](const std::string &expr, const std::string &target, bool trig){
            FsmTransition tr;
            tr.line = line_no;
            tr.trigger = trig;
            if(!expr.empty()){
                std::vector<std::string> flag_names;
                std::vector<std::string> counter_names;
                CollectSpecialNames(expr, flag_names, counter_names);
                for(const auto &n : flag_names){
                    if(this->flag_map.find(n) == this->flag_map.end()){
                        FsmVar v;
                        v.name = n;
                        v.xdata = std::make_unique<XData>(1, XData::InOut);
                        (*v.xdata) = (uint64_t)0;
                        int idx = (int)this->flags.size();
                        this->flags.push_back(std::move(v));
                        this->flag_map[n] = idx;
                        this->engine.RegisterExternalSignal(n, this->flags[idx].xdata.get());
                    }
                }
                for(const auto &n : counter_names){
                    if(this->counter_map.find(n) == this->counter_map.end()){
                        FsmVar v;
                        v.name = n;
                        v.xdata = std::make_unique<XData>(64, XData::InOut);
                        (*v.xdata) = (uint64_t)0;
                        int idx = (int)this->counters.size();
                        this->counters.push_back(std::move(v));
                        this->counter_map[n] = idx;
                        this->engine.RegisterExternalSignal(n, this->counters[idx].xdata.get());
                    }
                }
                try{
                    tr.cond_root = this->engine.CompileExpr(expr, cfg);
                }catch(const std::exception &e){
                    throw std::runtime_error("line " + std::to_string(line_no) + ": " + e.what());
                }
            }
            tr.next_name = target;
            cur_state->transitions.push_back(tr);
        };
        if(StartsWithWord(line, "if", &rest) ||
           StartsWithWord(line, "elif", &rest) ||
           StartsWithWord(line, "elseif", &rest)){
            size_t kwpos = std::string::npos;
            bool is_trigger = false;
            std::string expr;
            std::string target;
            if(FindKeywordOutsideParen(rest, "goto", kwpos)){
                expr = TrimCopy(rest.substr(0, kwpos));
                target = TrimCopy(rest.substr(kwpos + 4));
            }else if(FindKeywordOutsideParen(rest, "trigger", kwpos)){
                expr = TrimCopy(rest.substr(0, kwpos));
                is_trigger = true;
            }else{
                throw std::runtime_error("line " + std::to_string(line_no) + ": if/elif needs goto/trigger");
            }
            if(expr.empty()){
                throw std::runtime_error("line " + std::to_string(line_no) + ": missing condition");
            }
            parse_transition(expr, target, is_trigger);
            continue;
        }
        if(StartsWithWord(line, "else", &rest)){
            size_t kwpos = std::string::npos;
            bool is_trigger = false;
            std::string target;
            if(FindKeywordOutsideParen(rest, "goto", kwpos)){
                target = TrimCopy(rest.substr(kwpos + 4));
            }else if(FindKeywordOutsideParen(rest, "trigger", kwpos)){
                is_trigger = true;
            }else{
                throw std::runtime_error("line " + std::to_string(line_no) + ": else needs goto/trigger");
            }
            FsmTransition tr;
            tr.line = line_no;
            tr.cond_root = -1;
            tr.trigger = is_trigger;
            tr.next_name = target;
            cur_state->transitions.push_back(tr);
            continue;
        }
        if(StartsWithWord(line, "goto", &rest)){
            if(rest.empty()){
                throw std::runtime_error("line " + std::to_string(line_no) + ": goto needs a target state");
            }
            FsmTransition tr;
            tr.line = line_no;
            tr.cond_root = -1;
            tr.next_name = rest;
            tr.trigger = false;
            cur_state->transitions.push_back(tr);
            continue;
        }
        if(ToLowerCopy(line) == "trigger"){
            FsmTransition tr;
            tr.line = line_no;
            tr.cond_root = -1;
            tr.trigger = true;
            cur_state->transitions.push_back(tr);
            continue;
        }
        throw std::runtime_error("line " + std::to_string(line_no) + ": unknown statement");
    }
    if(this->states.empty()){
        throw std::runtime_error("fsm program has no states");
    }
    if(!start_state_name.empty()){
        auto it = this->state_map.find(start_state_name);
        if(it == this->state_map.end()){
            throw std::runtime_error("start state not found: " + start_state_name);
        }
        this->start_state = it->second;
    }else{
        this->start_state = 0;
    }
    for(auto &st : this->states){
        for(auto &tr : st.transitions){
            if(tr.trigger){
                tr.next_state = -1;
                continue;
            }
            if(tr.next_name.empty()){
                throw std::runtime_error("line " + std::to_string(tr.line) + ": missing target state");
            }
            auto it = this->state_map.find(tr.next_name);
            if(it == this->state_map.end()){
                throw std::runtime_error("line " + std::to_string(tr.line) + ": unknown state " + tr.next_name);
            }
            tr.next_state = it->second;
        }
    }
    this->Reset();
    }catch(const std::exception &e){
        Error("FSM parse/load failed: %s", e.what());
        this->Clear();
        return;
    }
}

void ComUseFsmTrigger::Reset(){
    this->triggered = false;
    this->triggered_state.clear();
    this->current_state = this->start_state;
    for(auto &v : this->flags){
        v.value = 0;
        if(v.xdata){
            (*v.xdata) = (uint64_t)0;
        }
    }
    for(auto &v : this->counters){
        v.value = 0;
        if(v.xdata){
            (*v.xdata) = (uint64_t)0;
        }
    }
    this->engine.ResetState();
}

void ComUseFsmTrigger::Clear(){
    this->states.clear();
    this->state_map.clear();
    this->flag_map.clear();
    this->counter_map.clear();
    this->flags.clear();
    this->counters.clear();
    this->start_state = -1;
    this->current_state = -1;
    this->triggered = false;
    this->triggered_state.clear();
    this->engine.Clear();
}

std::string ComUseFsmTrigger::GetCurrentState(){
    if(this->current_state < 0 || this->current_state >= (int)this->states.size()){
        return "";
    }
    return this->states[this->current_state].name;
}

std::vector<std::string> ComUseFsmTrigger::ListStates(){
    std::vector<std::string> ret;
    for(const auto &st : this->states){
        ret.push_back(st.name);
    }
    return ret;
}

void ComUseFsmTrigger::Call(){
    if(this->triggered){
        return;
    }
    if(this->current_state < 0 || this->current_state >= (int)this->states.size()){
        return;
    }
    this->engine.SetCycle(this->cycle);
    FsmState &st = this->states[this->current_state];
    for(const auto &act : st.actions){
        if(unlikely(act.exec == nullptr)){
            continue;
        }
        act.exec(this, act.index);
    }
    bool moved = false;
    for(const auto &tr : st.transitions){
        if(tr.cond_root >= 0){
            uint64_t v = this->engine.Eval(tr.cond_root);
            if(v == 0){
                continue;
            }
        }
        if(tr.trigger){
            this->triggered = true;
            this->triggered_state = st.name;
            for(auto &clk : this->clk_list){
                clk->Disable();
            }
            this->IncCbCount();
            return;
        }
        if(tr.next_state >= 0 && tr.next_state < (int)this->states.size()){
            this->current_state = tr.next_state;
            moved = true;
        }
        break;
    }
    if(!moved){
        return;
    }
}

} // namespace xspcomm
