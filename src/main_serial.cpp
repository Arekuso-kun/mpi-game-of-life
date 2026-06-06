#include "life.hpp"
#include "pgm.hpp"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
    struct Options
    {
        std::size_t width = 128;
        std::size_t height = 128;
        std::size_t steps = 100;
        std::size_t snapshot_interval = 10;
        std::uint32_t seed = 42;
        double density = 0.25;
        std::string pattern = "glider";
        std::filesystem::path output_dir = "frames";
        std::filesystem::path csv_path;
        bool csv_enabled = false;
    };

    void print_usage(const char *program)
    {
        std::cout
            << "Usage: " << program << " [options]\n\n"
            << "Options:\n"
            << "  --width N               Grid width (default: 128)\n"
            << "  --height N              Grid height (default: 128)\n"
            << "  --steps N               Number of generations (default: 100)\n"
            << "  --snapshot-interval N   Save every N steps; 0 disables snapshots (default: 10)\n"
            << "  --pattern NAME          random, glider, blinker, block (default: glider)\n"
            << "  --seed N                Seed for random pattern (default: 42)\n"
            << "  --density P             Alive probability for random pattern (default: 0.25)\n"
            << "  --output DIR            Directory for PGM frames (default: frames)\n"
            << "  --csv FILE              Append benchmark metrics to CSV file\n"
            << "  --help                  Show this message\n";
    }

    std::string require_value(int &index, int argc, char **argv)
    {
        if (index + 1 >= argc)
        {
            throw std::invalid_argument(std::string("missing value for ") + argv[index]);
        }
        ++index;
        return argv[index];
    }

    std::size_t parse_size(const std::string &value, const std::string &name)
    {
        if (value.empty() || value.front() == '-')
        {
            throw std::invalid_argument("invalid value for " + name + ": " + value);
        }
        std::size_t consumed = 0;
        const auto parsed = std::stoull(value, &consumed);
        if (consumed != value.size())
        {
            throw std::invalid_argument("invalid value for " + name + ": " + value);
        }
        return static_cast<std::size_t>(parsed);
    }

    std::uint32_t parse_u32(const std::string &value, const std::string &name)
    {
        if (value.empty() || value.front() == '-')
        {
            throw std::invalid_argument("invalid value for " + name + ": " + value);
        }
        std::size_t consumed = 0;
        const auto parsed = std::stoul(value, &consumed);
        if (consumed != value.size() || parsed > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::invalid_argument("invalid value for " + name + ": " + value);
        }
        return static_cast<std::uint32_t>(parsed);
    }

    double parse_double(const std::string &value, const std::string &name)
    {
        std::size_t consumed = 0;
        const auto parsed = std::stod(value, &consumed);
        if (consumed != value.size())
        {
            throw std::invalid_argument("invalid value for " + name + ": " + value);
        }
        return parsed;
    }

    Options parse_options(int argc, char **argv)
    {
        Options options;

        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help")
            {
                print_usage(argv[0]);
                std::exit(0);
            }
            else if (arg == "--width")
            {
                options.width = parse_size(require_value(i, argc, argv), arg);
            }
            else if (arg == "--height")
            {
                options.height = parse_size(require_value(i, argc, argv), arg);
            }
            else if (arg == "--steps")
            {
                options.steps = parse_size(require_value(i, argc, argv), arg);
            }
            else if (arg == "--snapshot-interval")
            {
                options.snapshot_interval = parse_size(require_value(i, argc, argv), arg);
            }
            else if (arg == "--pattern")
            {
                options.pattern = require_value(i, argc, argv);
            }
            else if (arg == "--seed")
            {
                options.seed = parse_u32(require_value(i, argc, argv), arg);
            }
            else if (arg == "--density")
            {
                options.density = parse_double(require_value(i, argc, argv), arg);
            }
            else if (arg == "--output")
            {
                options.output_dir = require_value(i, argc, argv);
            }
            else if (arg == "--csv")
            {
                options.csv_path = require_value(i, argc, argv);
                options.csv_enabled = true;
            }
            else
            {
                throw std::invalid_argument("unknown option: " + arg);
            }
        }

        if (options.width == 0 || options.height == 0)
        {
            throw std::invalid_argument("width and height must be positive");
        }
        if (!life::is_known_pattern(options.pattern))
        {
            throw std::invalid_argument("unknown pattern: " + options.pattern);
        }
        if (options.density < 0.0 || options.density > 1.0)
        {
            throw std::invalid_argument("density must be between 0 and 1");
        }

        return options;
    }

    void append_csv_result(const Options &options,
                           double total_seconds,
                           double computation_seconds,
                           std::size_t alive_cells)
    {
        if (!options.csv_enabled)
        {
            return;
        }

        if (options.csv_path.has_parent_path())
        {
            std::filesystem::create_directories(options.csv_path.parent_path());
        }

        const bool write_header =
            !std::filesystem::exists(options.csv_path) ||
            std::filesystem::file_size(options.csv_path) == 0;

        std::ofstream out(options.csv_path, std::ios::app);
        if (!out)
        {
            throw std::runtime_error("could not open CSV file: " + options.csv_path.string());
        }

        if (write_header)
        {
            out << "implementation,processes,width,height,steps,pattern,seed,density,"
                << "snapshot_interval,alive_cells,total_seconds,communication_seconds,"
                << "computation_seconds,communication_percent\n";
        }

        out << std::setprecision(10)
            << "serial,1,"
            << options.width << ','
            << options.height << ','
            << options.steps << ','
            << options.pattern << ','
            << options.seed << ','
            << options.density << ','
            << options.snapshot_interval << ','
            << alive_cells << ','
            << total_seconds << ','
            << 0.0 << ','
            << computation_seconds << ','
            << 0.0 << '\n';
    }

    std::filesystem::path frame_path(const std::filesystem::path &output_dir, std::size_t step)
    {
        std::ostringstream name;
        name << "frame_" << std::setw(6) << std::setfill('0') << step << ".pgm";
        return output_dir / name.str();
    }

    void maybe_write_snapshot(const Options &options, const life::Grid &grid, std::size_t step)
    {
        if (options.snapshot_interval == 0 || step % options.snapshot_interval != 0)
        {
            return;
        }
        life::write_pgm(frame_path(options.output_dir, step), grid);
    }

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const auto options = parse_options(argc, argv);

        life::Grid current(options.width, options.height);
        life::Grid next(options.width, options.height);
        life::seed_pattern(current, options.pattern, options.seed, options.density);

        const auto start = std::chrono::steady_clock::now();
        maybe_write_snapshot(options, current, 0);

        for (std::size_t generation = 1; generation <= options.steps; ++generation)
        {
            life::step(current, next);
            std::swap(current, next);
            maybe_write_snapshot(options, current, generation);
        }

        const auto stop = std::chrono::steady_clock::now();
        const std::chrono::duration<double> elapsed = stop - start;

        std::cout
            << "Completed " << options.steps << " generations on "
            << options.width << "x" << options.height << " grid\n"
            << "Pattern: " << options.pattern << "\n"
            << "Alive cells: " << life::count_alive(current) << "\n"
            << "Elapsed seconds: " << elapsed.count() << "\n";

        append_csv_result(options,
                          elapsed.count(),
                          elapsed.count(),
                          life::count_alive(current));
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n\n";
        print_usage(argv[0]);
        return 1;
    }

    return 0;
}
