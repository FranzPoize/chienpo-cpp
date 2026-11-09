#include "chienpo/vector_math.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace chienpo;

namespace {

void check(bool value, const std::string & message)
{
    if (!value) throw std::runtime_error(message);
}

bool close(double a, double b)
{
    return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) < 1e-12;
}

bool same(Vec2 a, Vec2 b) { return close(a.x, b.x) && close(a.y, b.y); }

void vector_arithmetic()
{
    check(same(mul_v({-2.5, 4}, -0.5), {1.25, -2}),
          "mul_v scales both coordinates");
    check(same(add_v({-2.5, 4}, {3, -6}), {0.5, -2}),
          "add_v adds corresponding coordinates");
    check(same(sub_v({-2.5, 4}, {3, -6}), {-5.5, 10}),
          "sub_v subtracts corresponding coordinates");
}

void exact_matrix_equality()
{
    auto matrix = g_identity_matrix;
    check(mat_equal(matrix, g_identity_matrix),
          "identical matrices compare equal");
    matrix[1][2] = std::nextafter(0.0, 1.0);
    check(!mat_equal(matrix, g_identity_matrix),
          "matrix comparison does not use a tolerance");
    matrix = g_identity_matrix;
    matrix[2][1] = std::numeric_limits<double>::quiet_NaN();
    check(!mat_equal(matrix, matrix), "NaN retains numeric equality behavior");
}

void proper_line_intersections()
{
    const auto crossing = intersect_lines({0, 0}, {4, 4}, {0, 4}, {4, 0});
    check(crossing && same(*crossing, {2, 2}),
          "proper crossing returns its intersection");
    check(!intersect_lines({0, 0}, {1, 0}, {1, 0}, {1, 1}),
          "shared endpoint is excluded");
    check(!intersect_lines({0, 0}, {3, 0}, {1, 0}, {2, 0}),
          "collinear overlap has no intersection");
    check(!intersect_lines({0, 0}, {3, 0}, {0, 1}, {3, 1}),
          "parallel segments have no intersection");
}

void floor_modulo_behavior()
{
    check(close(floor_modulo(-0.5, 2), 1.5),
          "negative numerator wraps toward the positive divisor");
    check(close(floor_modulo(5.5, 2), 1.5),
          "fractional numerator keeps its fractional remainder");
    check(close(floor_modulo(5, -3), -1),
          "negative divisor retains floor-modulo behavior");
    check(std::isnan(floor_modulo(1, 0)),
          "zero divisor retains numeric NaN behavior");
}

void translated_box_intersections()
{
    const Vec2 origin{10, 20};
    const Box box{1, 3, 2, 6};

    const auto crossing =
        intersect_line_and_box({12, 30}, {12, 18}, origin, box);
    check(crossing && same(*crossing, {12, 22}),
          "bottom edge wins before the nearer top edge");
    check(!intersect_line_and_box({11.5, 23}, {12.5, 25}, origin, box),
          "segment wholly inside the translated box has no boundary hit");
    check(!intersect_line_and_box({9, 20}, {12, 23}, origin, box),
          "corner-only contact is excluded by strict edge endpoints");
}

} // namespace

int main()
{
    try
    {
        vector_arithmetic();
        exact_matrix_equality();
        proper_line_intersections();
        floor_modulo_behavior();
        translated_box_intersections();
        std::cout << "PASS complete vector math behavior\n";
        return 0;
    }
    catch (const std::exception & error)
    {
        std::cerr << "FAIL vector math: " << error.what() << '\n';
        return 1;
    }
}
