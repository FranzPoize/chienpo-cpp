#include "chienpo/vector_math.hpp"

#include <array>
#include <cmath>

namespace chienpo {

Vec2 mul_v(Vec2 vector, double scalar)
{
    return {vector.x * scalar, vector.y * scalar};
}

Vec2 add_v(Vec2 vector, Vec2 other)
{
    return {vector.x + other.x, vector.y + other.y};
}

Vec2 sub_v(Vec2 vector, Vec2 other)
{
    return {vector.x - other.x, vector.y - other.y};
}

bool mat_equal(const TransformMatrix & a, const TransformMatrix & b)
{
    return a[0][0] == b[0][0] && a[0][1] == b[0][1] && a[0][2] == b[0][2]
           && a[1][0] == b[1][0] && a[1][1] == b[1][1] && a[1][2] == b[1][2]
           && a[2][0] == b[2][0] && a[2][1] == b[2][1] && a[2][2] == b[2][2];
}

std::optional<Vec2> intersect_lines(Vec2 a1, Vec2 a2, Vec2 b1, Vec2 b2)
{
    const double denominator =
        -((a1.x - a2.x) * (b1.y - b2.y) - (a1.y - a2.y) * (b1.x - b2.x));
    if (denominator == 0) return std::nullopt;

    const double t =
        ((b1.x - a1.x) * (b1.y - b2.y) - (b1.y - a1.y) * (b1.x - b2.x))
        / denominator;
    const double u =
        ((b1.x - a1.x) * (a1.y - a2.y) - (b1.y - a1.y) * (a1.x - a2.x))
        / denominator;

    if (0 < t && t < 1 && 0 < u && u < 1)
        return Vec2{a1.x + t * (a2.x - a1.x), a1.y + t * (a2.y - a1.y)};
    return std::nullopt;
}

double floor_modulo(double numerator, double divisor)
{
    return numerator - divisor * std::floor(numerator / divisor);
}

std::optional<Vec2>
intersect_line_and_box(Vec2 a1, Vec2 a2, Vec2 origin, const Box & box)
{
    const std::array<Vec2, 5> box_points{{
        {origin.x + box.xmin, origin.y + box.ymin},
        {origin.x + box.xmax, origin.y + box.ymin},
        {origin.x + box.xmax, origin.y + box.ymax},
        {origin.x + box.xmin, origin.y + box.ymax},
        {origin.x + box.xmin, origin.y + box.ymin},
    }};

    std::optional<Vec2> intersection;
    for (std::size_t index = 0; index < box_points.size(); ++index)
    {
        const auto previous = static_cast<std::size_t>(
            floor_modulo(static_cast<double>(index) - 1,
                         static_cast<double>(box_points.size())));
        if (!intersection)
            intersection = intersect_lines(a1, a2, box_points[previous],
                                           box_points[index]);
    }
    return intersection;
}

} // namespace chienpo
