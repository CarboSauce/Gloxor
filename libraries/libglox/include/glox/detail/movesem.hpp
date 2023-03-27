#ifndef LIBGLOX_MOVESEM
#define LIBGLOX_MOVESEM

namespace glox::detail
{
template<typename T> struct remove_ref      { using type = T; };
template<typename T> struct remove_ref<T&>  { using type = T; };
template<typename T> struct remove_ref<T&&> { using type = T; };
}
#define RVALUE(...) static_cast<typename glox::detail::remove_ref<decltype(__VA_ARGS__)>::type&&>(__VA_ARGS__)
#define FORWARD(...) static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#endif
