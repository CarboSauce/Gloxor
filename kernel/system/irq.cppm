export module gloxor.system.irq;
import glox;
import std;

namespace gx::irq {
export struct IrqHandler
{
    glox::string name;
    glox::function<void()> handler;
};

inline glox::array<IrqHandler, 256> irqHandlers = { };

export template <typename Func>
void attach_handler(std::string_view str, int irqNumber, Func func)
{
    irqHandlers[irqNumber] = { .name = str, .handler = func };
}

export glox::option<IrqHandler&> handler_at(int irqNumber)
{
    auto& h = irqHandlers[irqNumber];
    if (h.handler) {
        return h;
    } else {
        return { };
    }
}
} // namespace gx::irq
