#include "life.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    void require(bool condition, const std::string &message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    bool same_grid(const life::Grid &left, const life::Grid &right)
    {
        return left.width() == right.width() && left.height() == right.height() && left.cells() == right.cells();
    }

    void test_block_still_life()
    {
        life::Grid current(8, 8);
        life::Grid next(8, 8);
        life::seed_block(current, 3, 3);

        const auto initial = current;
        life::step(current, next);

        require(same_grid(initial, next), "block should remain unchanged after one step");
    }

    void test_blinker_period_two()
    {
        life::Grid current(8, 8);
        life::Grid next(8, 8);
        life::Grid second(8, 8);
        life::seed_blinker(current, 3, 3);

        const auto initial = current;
        life::step(current, next);
        life::step(next, second);

        require(same_grid(initial, second), "blinker should return to initial state after two steps");
    }

    void test_random_seed_reproducible()
    {
        life::Grid first(32, 32);
        life::Grid second(32, 32);

        life::seed_random(first, 42, 0.25);
        life::seed_random(second, 42, 0.25);

        require(same_grid(first, second), "random pattern should be reproducible with the same seed");
    }

    void test_toroidal_wrap_keeps_glider_alive()
    {
        life::Grid current(5, 5);
        life::Grid next(5, 5);
        life::seed_glider(current, 4, 4);

        life::step(current, next);

        require(life::count_alive(next) == 5, "glider near border should wrap toroidally");
    }

} // namespace

int main()
{
    try
    {
        test_block_still_life();
        test_blinker_period_two();
        test_random_seed_reproducible();
        test_toroidal_wrap_keeps_glider_alive();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Test failed: " << ex.what() << "\n";
        return 1;
    }

    std::cout << "All life_core tests passed\n";
    return 0;
}
