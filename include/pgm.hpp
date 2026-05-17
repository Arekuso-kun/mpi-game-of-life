#pragma once

#include "life.hpp"

#include <filesystem>

namespace life
{
    void write_pgm(const std::filesystem::path &path, const Grid &grid);
} // namespace life
