#include "xspcomm/xexpr.h"
#include "xspcomm/xsignal_cfg.h"
#include "xspcomm/common/compare.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace xspcomm {
int ExprEngine::NewConst(uint64_t v){
    ExprNode n;
    n.op = ExprOp::CONST;
    n.imm = v;
    n.width = 64;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewSignal(XData* sig){
    if(sig && sig->W() > 64) return -1;
    ExprNode n;
    n.op = ExprOp::SIGNAL;
    n.sig = sig;
    n.width = sig ? sig->W() : 64;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewUnary(ExprOp op, int child){
    ExprNode n;
    n.op = op;
    n.lhs = child;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewBinary(ExprOp op, int lhs, int rhs){
    ExprNode n;
    n.op = op;
    n.lhs = lhs;
    n.rhs = rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompare(ExprOp op, int lhs, int rhs){
    ExprNode n;
    n.op = op;
    n.lhs = lhs;
    n.rhs = rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewSelect(int cond, int lhs, int rhs){
    ExprNode n;
    n.op = ExprOp::SELECT;
    n.lhs = cond;
    n.rhs = lhs;
    n.imm = (uint64_t)rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompareSigSig(ExprOp op, XData* lhs, XData* rhs){
    ExprNode n;
    n.op = op;
    n.use_xdata_cmp = true;
    n.lhs_xdata = lhs;
    n.rhs_xdata = rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompareSigConst(ExprOp op, XData* lhs, uint64_t rhs){
    ExprNode n;
    n.op = op;
    n.use_xdata_cmp = true;
    n.lhs_xdata = lhs;
    n.rhs_xdata = this->MakeConstXData(lhs ? lhs->W() : 64, rhs);
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompareConstSig(ExprOp op, uint64_t lhs, XData* rhs){
    ExprNode n;
    n.op = op;
    n.use_xdata_cmp = true;
    n.lhs_xdata = this->MakeConstXData(rhs ? rhs->W() : 64, lhs);
    n.rhs_xdata = rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompareSigConstBytes(
    ExprOp op, XData* lhs, std::vector<unsigned char> &rhs){
    if(!lhs) return -1;
    ExprNode n;
    n.op = op;
    n.use_xdata_cmp = true;
    n.lhs_xdata = lhs;
    n.rhs_xdata = this->MakeConstXDataBytes(lhs->W(), rhs);
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewCompareConstBytesSig(
    ExprOp op, std::vector<unsigned char> &lhs, XData* rhs){
    if(!rhs) return -1;
    ExprNode n;
    n.op = op;
    n.use_xdata_cmp = true;
    n.lhs_xdata = this->MakeConstXDataBytes(rhs->W(), lhs);
    n.rhs_xdata = rhs;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewMaskedCompareSigConstBytes(
    XData* lhs, std::vector<unsigned char> &value, std::vector<unsigned char> &mask){
    if(!lhs) return -1;
    const uint32_t width = std::max(1u, lhs->W());
    const size_t size = (static_cast<size_t>(width) + 7) / 8;
    // Reject malformed descriptors rather than silently truncating high bits.
    if(value.size() != size || mask.size() != size) return -1;
    const unsigned int last_mask = width % 8 ? (1u << (width % 8)) - 1 : 255;
    if((value.back() | mask.back()) & ~last_mask) return -1;
    for(size_t i = 0; i < size; ++i) if(value[i] & ~mask[i]) return -1;
    ExprNode n;
    n.op = ExprOp::EQ;
    n.use_xdata_cmp = true;
    n.lhs_xdata = lhs;
    n.rhs_xdata = MakeConstXDataBytes(width, value);
    n.mask_xdata = MakeConstXDataBytes(width, mask);
    nodes.push_back(n);
    return static_cast<int>(nodes.size()) - 1;
}

int ExprEngine::NewWithin(int child, uint64_t window){
    ExprNode n;
    n.op = ExprOp::WITHIN;
    n.lhs = child;
    n.window = window;
    n.last_true_cycle = (uint64_t)-1;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

int ExprEngine::NewHold(int child, uint64_t window){
    ExprNode n;
    n.op = ExprOp::HOLD;
    n.lhs = child;
    n.window = window;
    n.last_hold_cycle = (uint64_t)-1;
    n.hold_count = 0;
    this->nodes.push_back(n);
    return (int)this->nodes.size() - 1;
}

uint64_t ExprEngine::Eval(int root){
    return this->EvalIterative(root);
}

bool ExprEngine::IsKnown(int root){
    return this->IsKnownNode(root);
}

bool ExprEngine::IsKnownNode(int id){
    if(id < 0 || id >= (int)this->nodes.size()) return false;
    const ExprNode &n = this->nodes[id];
    if(n.op == ExprOp::CONST) return true;
    if(n.op == ExprOp::SIGNAL){
        if(!n.sig) return false;
        n.sig->U();
        return n.sig->DataValid();
    }
    if(n.use_xdata_cmp){
        if(!n.lhs_xdata || !n.rhs_xdata) return false;
        n.lhs_xdata->U();
        n.rhs_xdata->U();
        return n.lhs_xdata->DataValid() && n.rhs_xdata->DataValid();
    }
    if(!IsKnownNode(n.lhs)) return false;
    if(n.op == ExprOp::LNOT || n.op == ExprOp::BNOT ||
       n.op == ExprOp::WITHIN || n.op == ExprOp::HOLD){
        return true;
    }
    if(n.op == ExprOp::SELECT){
        return IsKnownNode(n.rhs) && IsKnownNode((int)n.imm);
    }
    return IsKnownNode(n.rhs);
}

void ExprEngine::Clear(){
    this->nodes.clear();
    this->const_xdata.clear();
    this->signal_xdata.clear();
    this->external_signal_xdata.clear();
}

uint64_t ExprEngine::EvalNode(int id){
    if(id < 0 || id >= (int)this->nodes.size()) return 0;
    const ExprNode &n = this->nodes[id];
    switch(n.op){
    case ExprOp::CONST:
        return n.imm;
    case ExprOp::SIGNAL:
        return n.sig ? n.sig->U() : 0;
    case ExprOp::BNOT:
        return ~EvalNode(n.lhs);
    case ExprOp::LNOT:
        return EvalNode(n.lhs) ? 0 : 1;
    case ExprOp::LAND: {
        uint64_t lv = EvalNode(n.lhs);
        if(!lv) return 0;
        return EvalNode(n.rhs) ? 1 : 0;
    }
    case ExprOp::LOR: {
        uint64_t lv = EvalNode(n.lhs);
        if(lv) return 1;
        return EvalNode(n.rhs) ? 1 : 0;
    }
    case ExprOp::SELECT:
        return EvalNode(n.lhs) ? EvalNode(n.rhs) : EvalNode((int)n.imm);
    case ExprOp::ADD:
        return EvalNode(n.lhs) + EvalNode(n.rhs);
    case ExprOp::SUB:
        return EvalNode(n.lhs) - EvalNode(n.rhs);
    case ExprOp::MUL:
        return EvalNode(n.lhs) * EvalNode(n.rhs);
    case ExprOp::DIV: {
        uint64_t rv = EvalNode(n.rhs);
        return rv == 0 ? 0 : EvalNode(n.lhs) / rv;
    }
    case ExprOp::MOD: {
        uint64_t rv = EvalNode(n.rhs);
        return rv == 0 ? 0 : EvalNode(n.lhs) % rv;
    }
    case ExprOp::BAND:
        return EvalNode(n.lhs) & EvalNode(n.rhs);
    case ExprOp::BOR:
        return EvalNode(n.lhs) | EvalNode(n.rhs);
    case ExprOp::BXOR:
        return EvalNode(n.lhs) ^ EvalNode(n.rhs);
    case ExprOp::SHL:
        return EvalNode(n.lhs) << (EvalNode(n.rhs) & 0x3F);
    case ExprOp::SHR:
        return EvalNode(n.lhs) >> (EvalNode(n.rhs) & 0x3F);
    case ExprOp::EQ:
    case ExprOp::NE:
    case ExprOp::GT:
    case ExprOp::GE:
    case ExprOp::LT:
    case ExprOp::LE:
        return this->EvalCompareNode(n) ? 1 : 0;
    case ExprOp::WITHIN: {
        uint64_t v = EvalNode(n.lhs);
        ExprNode &nn = this->nodes[id];
        if(v != 0){
            nn.last_true_cycle = this->current_cycle;
            return 1;
        }
        if(nn.window == 0){
            return 0;
        }
        if(nn.last_true_cycle == (uint64_t)-1){
            return 0;
        }
        return (this->current_cycle - nn.last_true_cycle <= nn.window) ? 1 : 0;
    }
    case ExprOp::HOLD: {
        uint64_t v = EvalNode(n.lhs);
        ExprNode &nn = this->nodes[id];
        if(v != 0){
            if(nn.last_hold_cycle + 1 == this->current_cycle){
                nn.hold_count += 1;
            }else{
                nn.hold_count = 1;
            }
            nn.last_hold_cycle = this->current_cycle;
            if(nn.window <= 1){
                return 1;
            }
            return (nn.hold_count >= nn.window) ? 1 : 0;
        }
        nn.hold_count = 0;
        nn.last_hold_cycle = (uint64_t)-1;
        return 0;
    }
    default:
        break;
    }
    return 0;
}

uint64_t ExprEngine::EvalIterative(int root){
    if(unlikely(root < 0 || root >= (int)this->nodes.size())){
        return 0;
    }
    auto &stack = this->eval_stack;
    auto &vals = this->eval_vals;
    stack.clear();
    vals.clear();
    if (stack.capacity() < this->nodes.size()) {
        stack.reserve(this->nodes.size());
    }
    if (vals.capacity() < this->nodes.size()) {
        vals.reserve(this->nodes.size());
    }
    stack.push_back({root, 0, 0});
    auto pop_val = [&vals]() -> uint64_t {
        if(vals.empty()) return 0;
        uint64_t v = vals.back();
        vals.pop_back();
        return v;
    };
    while(!stack.empty()){
        EvalFrame &f = stack.back();
        if(unlikely(f.id < 0 || f.id >= (int)this->nodes.size())){
            vals.push_back(0);
            stack.pop_back();
            continue;
        }
        ExprNode &n = this->nodes[f.id];
        switch(n.op){
        case ExprOp::CONST:
            vals.push_back(n.imm);
            stack.pop_back();
            break;
        case ExprOp::SIGNAL:
            vals.push_back(n.sig ? n.sig->U() : 0);
            stack.pop_back();
            break;
        case ExprOp::BNOT:
        case ExprOp::LNOT:
            if(f.state == 0){
                f.state = 1;
            stack.push_back({n.lhs, 0, 0});
            }else{
                uint64_t v = pop_val();
                if(n.op == ExprOp::BNOT){
                    vals.push_back(~v);
                }else{
                    vals.push_back(v ? 0 : 1);
                }
                stack.pop_back();
            }
            break;
        case ExprOp::EQ:
        case ExprOp::NE:
        case ExprOp::GT:
        case ExprOp::GE:
        case ExprOp::LT:
        case ExprOp::LE:
            if(n.use_xdata_cmp){
                vals.push_back(EvalCompareNode(n) ? 1 : 0);
                stack.pop_back();
            }else if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else if(f.state == 1){
                f.lhs_val = pop_val();
                f.state = 2;
                stack.push_back({n.rhs, 0, 0});
            }else{
                uint64_t rv = pop_val();
                uint64_t lv = f.lhs_val;
                vals.push_back(compare::Compare(n.op, lv, rv) ? 1 : 0);
                stack.pop_back();
            }
            break;
        case ExprOp::LAND:
        case ExprOp::LOR:
            if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else if(f.state == 1){
                uint64_t lv = pop_val();
                if(n.op == ExprOp::LAND){
                    if(!lv){
                        vals.push_back(0);
                        stack.pop_back();
                        break;
                    }
                }else{
                    if(lv){
                        vals.push_back(1);
                        stack.pop_back();
                        break;
                    }
                }
                f.lhs_val = lv;
                f.state = 2;
                stack.push_back({n.rhs, 0, 0});
            }else{
                uint64_t rv = pop_val();
                if(n.op == ExprOp::LAND){
                    vals.push_back(rv ? 1 : 0);
                }else{
                    vals.push_back(rv ? 1 : 0);
                }
                stack.pop_back();
            }
            break;
        case ExprOp::SELECT:
            if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else if(f.state == 1){
                f.lhs_val = pop_val();
                f.state = 2;
                stack.push_back({f.lhs_val ? n.rhs : (int)n.imm, 0, 0});
            }else{
                stack.pop_back();
            }
            break;
        case ExprOp::WITHIN:
            if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else{
                uint64_t v = pop_val();
                if(v != 0){
                    n.last_true_cycle = this->current_cycle;
                    vals.push_back(1);
                }else{
                    if(n.window == 0 || n.last_true_cycle == (uint64_t)-1){
                        vals.push_back(0);
                    }else{
                        vals.push_back((this->current_cycle - n.last_true_cycle <= n.window) ? 1 : 0);
                    }
                }
                stack.pop_back();
            }
            break;
        case ExprOp::HOLD:
            if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else{
                uint64_t v = pop_val();
                if(v != 0){
                    if(n.last_hold_cycle + 1 == this->current_cycle){
                        n.hold_count += 1;
                    }else{
                        n.hold_count = 1;
                    }
                    n.last_hold_cycle = this->current_cycle;
                    if(n.window <= 1){
                        vals.push_back(1);
                    }else{
                        vals.push_back((n.hold_count >= n.window) ? 1 : 0);
                    }
                }else{
                    n.hold_count = 0;
                    n.last_hold_cycle = (uint64_t)-1;
                    vals.push_back(0);
                }
                stack.pop_back();
            }
            break;
        case ExprOp::ADD:
        case ExprOp::SUB:
        case ExprOp::MUL:
        case ExprOp::DIV:
        case ExprOp::MOD:
        case ExprOp::BAND:
        case ExprOp::BOR:
        case ExprOp::BXOR:
        case ExprOp::SHL:
        case ExprOp::SHR:
            if(f.state == 0){
                f.state = 1;
                stack.push_back({n.lhs, 0, 0});
            }else if(f.state == 1){
                f.lhs_val = pop_val();
                f.state = 2;
                stack.push_back({n.rhs, 0, 0});
            }else{
                uint64_t rv = pop_val();
                uint64_t lv = f.lhs_val;
                uint64_t out = 0;
                switch(n.op){
                case ExprOp::ADD: out = lv + rv; break;
                case ExprOp::SUB: out = lv - rv; break;
                case ExprOp::MUL: out = lv * rv; break;
                case ExprOp::DIV: out = (rv == 0) ? 0 : (lv / rv); break;
                case ExprOp::MOD: out = (rv == 0) ? 0 : (lv % rv); break;
                case ExprOp::BAND: out = lv & rv; break;
                case ExprOp::BOR: out = lv | rv; break;
                case ExprOp::BXOR: out = lv ^ rv; break;
                case ExprOp::SHL: out = lv << (rv & 0x3F); break;
                case ExprOp::SHR: out = lv >> (rv & 0x3F); break;
                default: break;
                }
                vals.push_back(out);
                stack.pop_back();
            }
            break;
        default:
            vals.push_back(0);
            stack.pop_back();
            break;
        }
    }
    return vals.empty() ? 0 : vals.back();
}

bool ExprEngine::EvalCompareNode(const ExprNode &node){
    if(node.use_xdata_cmp){
        if(!node.lhs_xdata || !node.rhs_xdata) return false;
        if(node.mask_xdata){
            // Refresh once, then compare native words without allocating per sample.
            const auto low = node.lhs_xdata->U();
            if(!node.lhs_xdata->DataValid()) return false;
            if(node.lhs_xdata->W() == 0)
                return (low & node.mask_xdata->U()) == node.rhs_xdata->U();
            const size_t words = (static_cast<size_t>(node.lhs_xdata->W()) + 31) / 32;
            for(size_t i = 0; i < words; ++i)
                if((node.lhs_xdata->pVecData[i].aval & node.mask_xdata->pVecData[i].aval)
                   != node.rhs_xdata->pVecData[i].aval) return false;
            return true;
        }
        return compare::Compare(node.op, *node.lhs_xdata, *node.rhs_xdata);
    }
    uint64_t lv = EvalNode(node.lhs);
    uint64_t rv = EvalNode(node.rhs);
    return compare::Compare(node.op, lv, rv);
}

XData* ExprEngine::MakeConstXData(uint32_t width, uint64_t value){
    auto ptr = std::make_unique<XData>(width, XData::InOut);
    if(width > 0){
        int bytes = (width + 7) / 8;
        std::vector<unsigned char> buf;
        buf.resize(bytes, 0);
        for(int i = 0; i < bytes && i < 8; i++){
            buf[i] = (unsigned char)((value >> (i * 8)) & 0xFF);
        }
        ptr->SetVU8(buf);
    }else{
        (*ptr) = (uint64_t)value;
    }
    this->const_xdata.push_back(std::move(ptr));
    return this->const_xdata.back().get();
}

XData* ExprEngine::MakeConstXDataBytes(
    uint32_t width, std::vector<unsigned char> &value){
    auto data = std::make_unique<XData>(width, XData::InOut);
    data->SetVU8(value);
    XData *raw = data.get();
    this->const_xdata.push_back(std::move(data));
    return raw;
}

XData* ExprEngine::GetOrCreateSignal(XSignalCFG* cfg, const std::string &name){
    auto eit = this->external_signal_xdata.find(name);
    if(eit != this->external_signal_xdata.end()){
        return eit->second;
    }
    auto it = this->signal_xdata.find(name);
    if(it != this->signal_xdata.end()){
        return it->second.get();
    }
    if(cfg == nullptr){
        throw std::runtime_error("XSignalCFG is null");
    }
    XData* sig = cfg->NewXData(name);
    if(sig == nullptr){
        throw std::runtime_error("signal not found: " + name);
    }
    this->signal_xdata[name] = std::unique_ptr<XData>(sig);
    return this->signal_xdata[name].get();
}

void ExprEngine::RegisterExternalSignal(const std::string &name, XData* sig){
    if(sig == nullptr){
        throw std::runtime_error("external signal is null: " + name);
    }
    this->external_signal_xdata[name] = sig;
}

void ExprEngine::ResetState(){
    for(auto &n : this->nodes){
        if(n.op == ExprOp::WITHIN){
            n.last_true_cycle = (uint64_t)-1;
        }else if(n.op == ExprOp::HOLD){
            n.last_hold_cycle = (uint64_t)-1;
            n.hold_count = 0;
        }
    }
}
int ExprEngine::ComputeCost(int id, std::vector<int> &memo){
    if(id < 0 || id >= (int)this->nodes.size()) return 0;
    if(memo[id] >= 0) return memo[id];
    const ExprNode &n = this->nodes[id];
    int cost = 1;
    switch(n.op){
    case ExprOp::CONST:
        cost = 1;
        break;
    case ExprOp::SIGNAL:
        cost = 2;
        break;
    case ExprOp::BNOT:
    case ExprOp::LNOT:
        cost = 1 + ComputeCost(n.lhs, memo);
        break;
    case ExprOp::EQ:
    case ExprOp::NE:
    case ExprOp::GT:
    case ExprOp::GE:
    case ExprOp::LT:
    case ExprOp::LE:
        if(n.use_xdata_cmp){
            cost = 3;
        }else{
            cost = 1 + ComputeCost(n.lhs, memo) + ComputeCost(n.rhs, memo);
        }
        break;
    case ExprOp::SELECT:
        cost = 1 + ComputeCost(n.lhs, memo) + ComputeCost(n.rhs, memo)
             + ComputeCost((int)n.imm, memo);
        break;
    case ExprOp::WITHIN:
    case ExprOp::HOLD:
        cost = 2 + ComputeCost(n.lhs, memo);
        break;
    default:
        cost = 1 + ComputeCost(n.lhs, memo) + ComputeCost(n.rhs, memo);
        break;
    }
    memo[id] = cost;
    return cost;
}

int ExprEngine::ComputeStateful(int id, std::vector<int> &memo){
    if(id < 0 || id >= (int)this->nodes.size()) return 0;
    if(memo[id] >= 0) return memo[id];
    const ExprNode &n = this->nodes[id];
    int stateful = 0;
    if(n.op == ExprOp::WITHIN || n.op == ExprOp::HOLD){
        stateful = 1;
    }
    if(n.lhs >= 0) stateful |= ComputeStateful(n.lhs, memo);
    if(n.rhs >= 0) stateful |= ComputeStateful(n.rhs, memo);
    if(n.op == ExprOp::SELECT && (int)n.imm >= 0){
        stateful |= ComputeStateful((int)n.imm, memo);
    }
    memo[id] = stateful;
    return stateful;
}

void ExprEngine::ReorderShortCircuit(int id, const std::vector<int> &memo,
                                     const std::vector<int> &stateful){
    if(id < 0 || id >= (int)this->nodes.size()) return;
    ExprNode &n = this->nodes[id];
    if(n.lhs >= 0) ReorderShortCircuit(n.lhs, memo, stateful);
    if(n.rhs >= 0) ReorderShortCircuit(n.rhs, memo, stateful);
    if(n.op == ExprOp::SELECT && (int)n.imm >= 0){
        ReorderShortCircuit((int)n.imm, memo, stateful);
    }
    if(n.op == ExprOp::LAND || n.op == ExprOp::LOR){
        int lc = (n.lhs >= 0 && n.lhs < (int)memo.size()) ? memo[n.lhs] : 0;
        int rc = (n.rhs >= 0 && n.rhs < (int)memo.size()) ? memo[n.rhs] : 0;
        int ls = (n.lhs >= 0 && n.lhs < (int)stateful.size()) ? stateful[n.lhs] : 0;
        int rs = (n.rhs >= 0 && n.rhs < (int)stateful.size()) ? stateful[n.rhs] : 0;
        if(!ls && !rs && rc < lc){
            std::swap(n.lhs, n.rhs);
        }
    }
}

void ExprEngine::OptimizeShortCircuitOrder(int root){
    std::vector<int> memo(this->nodes.size(), -1);
    ComputeCost(root, memo);
    std::vector<int> stateful(this->nodes.size(), -1);
    ComputeStateful(root, stateful);
    ReorderShortCircuit(root, memo, stateful);
    if(root >= 0 && root < (int)this->nodes.size()){
        this->nodes[root].cost = (uint32_t)memo[root];
    }
}


} // namespace xspcomm
