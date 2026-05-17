#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace life
{
    using Cell = std::uint8_t;

    class Grid
    {
    public:
        Grid(std::size_t width, std::size_t height);

        std::size_t width() const { return width_; }
        std::size_t height() const { return height_; }
        std::size_t size() const { return cells_.size(); }

        Cell get(std::size_t x, std::size_t y) const;
        Cell get_wrapped(long long x, long long y) const;
        void set(std::size_t x, std::size_t y, Cell value);
        void set_wrapped(long long x, long long y, Cell value);
        void clear();

        const std::vector<Cell> &cells() const { return cells_; }
        std::vector<Cell> &cells() { return cells_; }

    private:
        std::size_t index(std::size_t x, std::size_t y) const;
        std::size_t wrap(long long value, std::size_t limit) const;

        std::size_t width_;
        std::size_t height_;
        std::vector<Cell> cells_;
    };

    void step(const Grid &current, Grid &next);
    std::size_t count_alive(const Grid &grid);

    void seed_random(Grid &grid, std::uint32_t seed, double alive_probability);
    void seed_glider(Grid &grid, std::size_t origin_x, std::size_t origin_y);
    void seed_blinker(Grid &grid, std::size_t origin_x, std::size_t origin_y);
    void seed_block(Grid &grid, std::size_t origin_x, std::size_t origin_y);

    bool is_known_pattern(const std::string &pattern);
    void seed_pattern(Grid &grid, const std::string &pattern, std::uint32_t seed, double alive_probability);
} // namespace life
