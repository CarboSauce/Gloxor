#pragma once
#include <gloxor/types.hpp>
namespace arch {
enum class FeatureBit {
	avx,
	xsave,
};
constexpr FeatureBit operator|(FeatureBit l, FeatureBit r)
{
	return FeatureBit(static_cast<u64>(l) | static_cast<u64>(r));
}

bool is_feature_supported(FeatureBit);
} // namespace arch