#ifndef LIBGLOX_MOVEUTILS
#define LIBGLOX_MOVEUTILS
namespace glox
{
template <typename T>
void swap(T& l, T& r)
{
	auto tmp = RVALUE(l);
	l = RVALUE(r);
	r = RVALUE(tmp);
}
template <typename T, typename U = T>
T exchange(T& l, U&& r)
{
	auto tmp = RVALUE(l);
	l = r;
	return tmp;
}
} // namespace glox
#endif

