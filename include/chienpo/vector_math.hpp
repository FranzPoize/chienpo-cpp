#pragma once

#include "chienpo/types.hpp"
#include <optional>

namespace chienpo {

[[nodiscard]] Vec2 mul_v(Vec2 vector, double scalar);
[[nodiscard]] Vec2 add_v(Vec2 vector, Vec2 other);
[[nodiscard]] Vec2 sub_v(Vec2 vector, Vec2 other);

// Deliberately compares every entry exactly, matching the original helper.
[[nodiscard]] bool mat_equal(const TransformMatrix & a,
                             const TransformMatrix & b);

// Returns only proper segment intersections. Parallel or collinear segments,
// and intersections at either segment's endpoints, have no result.
[[nodiscard]] std::optional<Vec2>
intersect_lines(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2);

[[nodiscard]] double floor_modulo(double numerator, double divisor);

// The box bounds are translated by origin. Edges retain the original
// bottom/right/top/left search order, so the first edge in that order wins.
[[nodiscard]] std::optional<Vec2>
intersect_line_and_box(Vec2 a1, Vec2 a2, Vec2 origin, const Box & box);

} // namespace chienpo
