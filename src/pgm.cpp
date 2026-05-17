#include "pgm.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

namespace life
{
    void write_pgm(const std::filesystem::path &path, const Grid &grid)
    {
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            throw std::runtime_error("could not open PGM output file: " + path.string());
        }

        out << "P5\n"
            << grid.width() << ' ' << grid.height() << "\n255\n";

        std::vector<unsigned char> pixels(grid.size());
        for (std::size_t i = 0; i < grid.size(); ++i)
        {
            pixels[i] = grid.cells()[i] ? 0 : 255;
        }

        out.write(reinterpret_cast<const char *>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
        if (!out)
        {
            throw std::runtime_error("failed while writing PGM output file: " + path.string());
        }
    }
} // namespace life
