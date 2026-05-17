#include "life.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace life
{
    Grid::Grid(std::size_t width, std::size_t height) : width_(width), height_(height), cells_(width * height, 0)
    {
        if (width == 0 || height == 0)
        {
            throw std::invalid_argument("grid dimensions must be positive");
        }
    }

    Cell Grid::get(std::size_t x, std::size_t y) const
    {
        return cells_[index(x, y)];
    }

    Cell Grid::get_wrapped(long long x, long long y) const
    {
        return get(wrap(x, width_), wrap(y, height_));
    }

    void Grid::set(std::size_t x, std::size_t y, Cell value)
    {
        cells_[index(x, y)] = value ? 1 : 0;
    }

    void Grid::set_wrapped(long long x, long long y, Cell value)
    {
        set(wrap(x, width_), wrap(y, height_), value);
    }

    void Grid::clear()
    {
        std::fill(cells_.begin(), cells_.end(), 0);
    }

    std::size_t Grid::index(std::size_t x, std::size_t y) const
    {
        if (x >= width_ || y >= height_)
        {
            throw std::out_of_range("grid coordinates out of range");
        }
        return y * width_ + x;
    }

    std::size_t Grid::wrap(long long value, std::size_t limit) const
    {
        const auto signed_limit = static_cast<long long>(limit);
        auto wrapped = value % signed_limit;
        if (wrapped < 0)
        {
            wrapped += signed_limit;
        }
        return static_cast<std::size_t>(wrapped);
    }

    void step(const Grid &current, Grid &next)
    {
        if (current.width() != next.width() || current.height() != next.height())
        {
            throw std::invalid_argument("current and next grids must have the same dimensions");
        }

        for (std::size_t y = 0; y < current.height(); ++y)
        {
            for (std::size_t x = 0; x < current.width(); ++x)
            {
                int neighbours = 0;
                for (int dy = -1; dy <= 1; ++dy)
                {
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        if (dx == 0 && dy == 0)
                        {
                            continue;
                        }
                        neighbours += current.get_wrapped(static_cast<long long>(x) + dx, static_cast<long long>(y) + dy);
                    }
                }

                const bool alive = current.get(x, y) != 0;
                const bool survives = alive && (neighbours == 2 || neighbours == 3);
                const bool born = !alive && neighbours == 3;
                next.set(x, y, (survives || born) ? 1 : 0);
            }
        }
    }

    std::size_t count_alive(const Grid &grid)
    {
        return static_cast<std::size_t>(std::count(grid.cells().begin(), grid.cells().end(), static_cast<Cell>(1)));
    }

    void seed_random(Grid &grid, std::uint32_t seed, double alive_probability)
    {
        if (alive_probability < 0.0 || alive_probability > 1.0)
        {
            throw std::invalid_argument("alive probability must be between 0 and 1");
        }

        std::mt19937 generator(seed);
        std::bernoulli_distribution alive(alive_probability);
        for (auto &cell : grid.cells())
        {
            cell = alive(generator) ? 1 : 0;
        }
    }

    void seed_glider(Grid &grid, std::size_t origin_x, std::size_t origin_y)
    {
        grid.clear();
        grid.set_wrapped(static_cast<long long>(origin_x) + 1, static_cast<long long>(origin_y), 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 2, static_cast<long long>(origin_y) + 1, 1);
        grid.set_wrapped(static_cast<long long>(origin_x), static_cast<long long>(origin_y) + 2, 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 1, static_cast<long long>(origin_y) + 2, 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 2, static_cast<long long>(origin_y) + 2, 1);
    }

    void seed_blinker(Grid &grid, std::size_t origin_x, std::size_t origin_y)
    {
        grid.clear();
        grid.set_wrapped(static_cast<long long>(origin_x), static_cast<long long>(origin_y), 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 1, static_cast<long long>(origin_y), 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 2, static_cast<long long>(origin_y), 1);
    }

    void seed_block(Grid &grid, std::size_t origin_x, std::size_t origin_y)
    {
        grid.clear();
        grid.set_wrapped(static_cast<long long>(origin_x), static_cast<long long>(origin_y), 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 1, static_cast<long long>(origin_y), 1);
        grid.set_wrapped(static_cast<long long>(origin_x), static_cast<long long>(origin_y) + 1, 1);
        grid.set_wrapped(static_cast<long long>(origin_x) + 1, static_cast<long long>(origin_y) + 1, 1);
    }

    bool is_known_pattern(const std::string &pattern)
    {
        return pattern == "random" || pattern == "glider" || pattern == "blinker" || pattern == "block";
    }

    void seed_pattern(Grid &grid, const std::string &pattern, std::uint32_t seed, double alive_probability)
    {
        const auto origin_x = grid.width() / 2;
        const auto origin_y = grid.height() / 2;

        if (pattern == "random")
        {
            seed_random(grid, seed, alive_probability);
        }
        else if (pattern == "glider")
        {
            seed_glider(grid, origin_x, origin_y);
        }
        else if (pattern == "blinker")
        {
            seed_blinker(grid, origin_x, origin_y);
        }
        else if (pattern == "block")
        {
            seed_block(grid, origin_x, origin_y);
        }
        else
        {
            throw std::invalid_argument("unknown pattern: " + pattern);
        }
    }
} // namespace life
