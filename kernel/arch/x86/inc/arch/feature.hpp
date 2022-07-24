#pragma once
#include <gloxor/types.hpp>
namespace arch
{
enum class featureBit
{
	avx,
	xsave,

};
constexpr featureBit operator|(featureBit l, featureBit r)
{
	return featureBit(static_cast<u64>(l) | static_cast<u64>(r));
}

bool isFeatureSupported(featureBit);
} // namespace arch