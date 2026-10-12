#include "xspcomm/xexpr.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool measuring = false;
thread_local size_t allocations = 0;
}

void *operator new(std::size_t size) {
    if (void *p = std::malloc(size ? size : 1)) {
        if (measuring) ++allocations;
        return p;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

int main() {
    using namespace xspcomm;
    ExprEngine engine;
    XData signal(129, XData::InOut);
    std::vector<unsigned char> negative(17, 255), divisor(17, 255), amount{3};
    negative[0] = 249; negative.back() = 1; // signed -7
    divisor[0] = 253; divisor.back() = 1;   // signed -3
    signal.SetVU8(negative);
    const int a = engine.NewBitsSignal(&signal, true);
    const int b = engine.NewBitsConst(129, true, divisor);
    const int shift = engine.NewBitsConst(8, false, amount);
    const std::array<int, 5> roots{
        engine.NewBits(ExprOp::ADD, a, b, 129, true),
        engine.NewBits(ExprOp::MUL, a, b, 129, true),
        engine.NewBits(ExprOp::DIV, a, b, 129, true),
        engine.NewBits(ExprOp::MOD, a, b, 129, true),
        engine.NewBits(ExprOp::SHR, a, shift, 129, true),
    };
    for (int root : roots) engine.Eval(root);
    if (engine.Eval(roots[2]) != 2) return 1;
    uint64_t checksum = 0;
    measuring = true;
    for (int i = 0; i < 1000; ++i)
        for (int root : roots) checksum += engine.Eval(root);
    measuring = false;
    std::cout << "5000 warm typed evaluations: " << allocations << " allocations, checksum=" << checksum << '\n';
    return allocations == 0 ? 0 : 1;
}
