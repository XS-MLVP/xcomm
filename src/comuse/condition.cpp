#include "xspcomm/xcomuse/condition.h"
#include "xspcomm/xclock.h"
#include "xspcomm/xexpr.h"

namespace xspcomm {

void ComUseCondCheck::BindXClock(XClock *clk){
    this->clk_list.push_back(clk);
}
void ComUseCondCheck::SetCondition(std::string unique_name, XData* pin, XData* val, ComUseCondCmp cmp, XData *valid, XData *valid_value, xfunction<bool, XData*, XData*, uint64_t> func, uint64_t arg){
    Assert(func == nullptr, "func type error: %d", func.func == nullptr);
    RemoveExpression(unique_name);
    if (!pin || !val || !SelectXDataCmpFn(cmp) || (valid && !valid_value))
        throw std::invalid_argument("invalid XData comparison");
    RemoveUint64Cond(this->cond_idx_uint64, this->cond_vec_uint64, unique_name);
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it == this->cond_idx_xdata.end()){
        CondXDataEntry entry;
        entry.name = unique_name;
        entry.pin = pin;
        entry.val = val;
        entry.cmp = cmp;
        entry.valid = valid;
        entry.valid_value = valid_value;
        entry.func = func;
        entry.arg = arg;
        entry.valid_cmp = ComUseCondCmp::EQ;
        entry.cmp_fn = SelectXDataCmpFn(cmp);
        entry.valid_cmp_fn = SelectXDataCmpFn(entry.valid_cmp);
        this->cond_vec_xdata.push_back(std::move(entry));
        this->cond_idx_xdata[unique_name] = this->cond_vec_xdata.size() - 1;
    }else{
        auto &entry = this->cond_vec_xdata[it->second];
        entry.pin = pin;
        entry.val = val;
        entry.cmp = cmp;
        entry.valid = valid;
        entry.valid_value = valid_value;
        entry.func = func;
        entry.arg = arg;
        entry.cmp_fn = SelectXDataCmpFn(cmp);
        entry.valid_cmp_fn = SelectXDataCmpFn(entry.valid_cmp);
    }
};
void ComUseCondCheck::SetCondition(std::string unique_name, uint64_t pin_ptr, uint64_t val_ptr, ComUseCondCmp cmp, int bytes, uint64_t valid_ptr, uint64_t valid_value_ptr, int valid_bytes, xfunction<bool, uint64_t, uint64_t, uint64_t> func, uint64_t arg){
    RemoveExpression(unique_name);
    if (!SelectPtrCmpFn(cmp) || (!func && (!pin_ptr || !val_ptr || bytes <= 0)) ||
        (valid_ptr && (!valid_value_ptr || valid_bytes <= 0)))
        throw std::invalid_argument("invalid pointer comparison");
    RemoveXDataCond(this->cond_idx_xdata, this->cond_vec_xdata, unique_name);
    auto it = this->cond_idx_uint64.find(unique_name);
    if(it == this->cond_idx_uint64.end()){
        CondUint64Entry entry;
        entry.name = unique_name;
        entry.pin_ptr = pin_ptr;
        entry.val_ptr = val_ptr;
        entry.cmp = cmp;
        entry.bytes = bytes;
        entry.valid_ptr = valid_ptr;
        entry.valid_value_ptr = valid_value_ptr;
        entry.valid_bytes = valid_bytes;
        entry.func = func;
        entry.arg = arg;
        entry.valid_cmp = ComUseCondCmp::EQ;
        entry.cmp_fn = SelectPtrCmpFn(cmp);
        entry.valid_cmp_fn = SelectPtrCmpFn(entry.valid_cmp);
        this->cond_vec_uint64.push_back(std::move(entry));
        this->cond_idx_uint64[unique_name] = this->cond_vec_uint64.size() - 1;
    }else{
        auto &entry = this->cond_vec_uint64[it->second];
        entry.pin_ptr = pin_ptr;
        entry.val_ptr = val_ptr;
        entry.cmp = cmp;
        entry.bytes = bytes;
        entry.valid_ptr = valid_ptr;
        entry.valid_value_ptr = valid_value_ptr;
        entry.valid_bytes = valid_bytes;
        entry.func = func;
        entry.arg = arg;
        entry.cmp_fn = SelectPtrCmpFn(cmp);
        entry.valid_cmp_fn = SelectPtrCmpFn(entry.valid_cmp);
    }
};
void ComUseCondCheck::RemoveCondition(std::string unique_name){
    RemoveExpression(unique_name);
    RemoveXDataCond(this->cond_idx_xdata, this->cond_vec_xdata, unique_name);
    RemoveUint64Cond(this->cond_idx_uint64, this->cond_vec_uint64, unique_name);
};
std::vector<std::string> ComUseCondCheck::GetTriggeredConditionKeys(){
    std::vector<std::string> ret;
    for(auto &e : this->cond_vec_xdata){
        if(e.triggered)ret.push_back(e.name);
    }
    for(auto &e : this->cond_vec_uint64){
        if(e.triggered)ret.push_back(e.name);
    }
    for (const auto &entry : expressions) if (entry.triggered) ret.push_back(entry.name);
    return ret;
}
std::map<std::string, bool> ComUseCondCheck::ListCondition(){
    std::map<std::string, bool> ret;
    for(auto &e : this->cond_vec_xdata){
        ret[e.name] = e.triggered ? true : false;
    }
    for(auto &e : this->cond_vec_uint64){
        ret[e.name] = e.triggered ? true : false;
    }
    for (const auto &entry : expressions) ret[entry.name] = entry.triggered;
    return ret;
}
ComUseCondCmp ComUseCondCheck::GetValidCmpMode(std::string unique_name){
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it != this->cond_idx_xdata.end()){
        return this->cond_vec_xdata[it->second].valid_cmp;
    }
    auto it2 = this->cond_idx_uint64.find(unique_name);
    if(it2 != this->cond_idx_uint64.end()){
        return this->cond_vec_uint64[it2->second].valid_cmp;
    }
    Error("Condition not found: %s", unique_name.c_str());
    return ComUseCondCmp::EQ; // default
}
void ComUseCondCheck::SetValidCmpMode(std::string unique_name, ComUseCondCmp cmp){
    if (!SelectXDataCmpFn(cmp)) throw std::invalid_argument("invalid comparison operation");
    auto it = this->cond_idx_xdata.find(unique_name);
    if(it != this->cond_idx_xdata.end()){
        auto &entry = this->cond_vec_xdata[it->second];
        entry.valid_cmp = cmp;
        entry.valid_cmp_fn = SelectXDataCmpFn(cmp);
        return;
    }
    auto it2 = this->cond_idx_uint64.find(unique_name);
    if(it2 != this->cond_idx_uint64.end()){
        auto &entry = this->cond_vec_uint64[it2->second];
        entry.valid_cmp = cmp;
        entry.valid_cmp_fn = SelectPtrCmpFn(cmp);
        return;
    }
    Error("Condition not found: %s", unique_name.c_str());
}
void ComUseCondCheck::ClearClock(){this->clk_list.clear();}
void ComUseCondCheck::ClearCondition(){
    expressions.clear();
    expression_index.clear();
    this->cond_idx_xdata.clear();
    this->cond_idx_uint64.clear();
    this->cond_vec_xdata.clear();
    this->cond_vec_uint64.clear();
};
xfunction<bool, XData*, XData*, uint64_t> ComUseCondCheck::AsXDataXFunc(uint64_t func){
    xfunction<bool, XData*, XData*, uint64_t> ret = (bool (*)(XData*, XData*, uint64_t))func;
    return ret;
}
xfunction<bool, uint64_t, uint64_t, uint64_t> ComUseCondCheck::AsPtrXFunc(uint64_t func){
    xfunction<bool, uint64_t, uint64_t, uint64_t> ret = (bool (*)(uint64_t, uint64_t, uint64_t))func;
    return ret;
}
namespace {
template<class Index, class Entries>
bool RemoveEntry(Index &index, Entries &entries, const std::string &name) {
    const auto found = index.find(name);
    if (found == index.end()) return false;
    const size_t at = found->second;
    if (at + 1 != entries.size()) {
        entries[at] = std::move(entries.back());
        index[entries[at].name] = at;
    }
    entries.pop_back();
    index.erase(found);
    return true;
}

template<class Entries, class Evaluate, class OnHit>
void CheckEntries(Entries &entries, Evaluate evaluate, OnHit on_hit) {
    for (auto &entry : entries) {
        entry.triggered = evaluate(entry);
        if (entry.triggered) on_hit();
    }
}
}

inline bool ComUseCondCheck::Evaluate(CondXDataEntry &entry) {
    if (entry.valid && !entry.valid_cmp_fn(entry.valid, entry.valid_value)) return false;
    return entry.func ? entry.func(entry.pin, entry.val, entry.arg)
                      : entry.cmp_fn(entry.pin, entry.val);
}

inline bool ComUseCondCheck::Evaluate(CondUint64Entry &entry) {
    if (entry.valid_ptr && !entry.valid_cmp_fn(this, entry.valid_ptr, entry.valid_value_ptr, entry.valid_bytes))
        return false;
    return entry.func ? entry.func(entry.pin_ptr, entry.val_ptr, entry.arg)
                      : entry.cmp_fn(this, entry.pin_ptr, entry.val_ptr, entry.bytes);
}

inline bool ComUseCondCheck::Evaluate(ExprEntry &entry) {
    return entry.root >= 0 && entry.engine->Eval(entry.root) != 0;
}

void ComUseCondCheck::Call() {
    // Keep comparisons specialized at registration; no per-sample allocation.
    auto evaluate = [this](auto &entry) { return Evaluate(entry); };
    bool triggered = false;
    auto on_hit = [&] {
        if (triggered) return;
        for (auto *clock : clk_list) clock->Disable();
        IncCbCount();
        triggered = true;
    };
    CheckEntries(cond_vec_xdata, evaluate, on_hit);
    CheckEntries(cond_vec_uint64, evaluate, on_hit);
    CheckEntries(expressions, evaluate, on_hit);
}

void ComUseCondCheck::SetExpression(std::string name, ExprEngine &engine, int root) {
    RemoveXDataCond(cond_idx_xdata, cond_vec_xdata, name);
    RemoveUint64Cond(cond_idx_uint64, cond_vec_uint64, name);
    const auto found = expression_index.find(name);
    if (found == expression_index.end()) {
        expression_index[name] = expressions.size();
        expressions.push_back({std::move(name), &engine, root});
    } else {
        auto &entry = expressions[found->second];
        entry.engine = &engine;
        entry.root = root;
    }
}

void ComUseCondCheck::RemoveExpression(const std::string &name) {
    RemoveEntry(expression_index, expressions, name);
}

ComUseCondCheck::XDataCmpFn ComUseCondCheck::SelectXDataCmpFn(ComUseCondCmp cmp){
    switch (cmp) {
    case ComUseCondCmp::EQ: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::EQ>;
    case ComUseCondCmp::NE: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::NE>;
    case ComUseCondCmp::GT: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::GT>;
    case ComUseCondCmp::GE: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::GE>;
    case ComUseCondCmp::LT: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::LT>;
    case ComUseCondCmp::LE: return &ComUseCondCheck::XDataCmp<ComUseCondCmp::LE>;
    default: return nullptr;
    }
}

ComUseCondCheck::PtrCmpFn ComUseCondCheck::SelectPtrCmpFn(ComUseCondCmp cmp){
    switch (cmp) {
    case ComUseCondCmp::EQ: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::EQ>;
    case ComUseCondCmp::NE: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::NE>;
    case ComUseCondCmp::GT: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::GT>;
    case ComUseCondCmp::GE: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::GE>;
    case ComUseCondCmp::LT: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::LT>;
    case ComUseCondCmp::LE: return &ComUseCondCheck::PtrCmp<ComUseCondCmp::LE>;
    default: return nullptr;
    }
}

bool ComUseCondCheck::RemoveXDataCond(std::unordered_map<std::string, size_t> &index,
                                    std::vector<CondXDataEntry> &entries, const std::string &name) {
    return RemoveEntry(index, entries, name);
}

bool ComUseCondCheck::RemoveUint64Cond(std::unordered_map<std::string, size_t> &index,
                                     std::vector<CondUint64Entry> &entries, const std::string &name) {
    return RemoveEntry(index, entries, name);
}

} // namespace xspcomm
