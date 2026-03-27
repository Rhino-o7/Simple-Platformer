#pragma once

#include <gl/matrix.hpp>
#include <gl/palette.hpp>

#include <vector>
#include <fstream>

namespace vpg::data {
    bool parse_qb(gl::Matrix& matrix, gl::Palette& palette, std::ifstream& ifs, bool emissive);
}
