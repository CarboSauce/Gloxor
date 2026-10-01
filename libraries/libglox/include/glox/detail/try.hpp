#ifndef LIBGLOX_TRY
#define LIBGLOX_TRY

#ifndef TRY
#define TRY(expr)                                            \
    ({                                                       \
        auto&& val = (expr);                                 \
        if (not val)                                         \
            return glox::try_propagate_from_err<             \
                std::remove_reference_t<decltype(val.val())> \
            >(glox::try_propagate_err(RVALUE(val)));         \
        RVALUE(val.val());                                   \
    })
#endif

#endif
