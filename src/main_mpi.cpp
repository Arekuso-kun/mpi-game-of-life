#include "life.hpp"
#include "pgm.hpp"

#include <mpi.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr int root_rank = 0;

    struct Options
    {
        std::size_t width = 128;
        std::size_t height = 128;
        std::size_t steps = 100;
        std::size_t snapshot_interval = 10;
        std::uint32_t seed = 42;
        double density = 0.25;
        std::string pattern = "glider";
        std::filesystem::path output_dir = "frames_mpi";
        std::filesystem::path csv_path;
        bool csv_enabled = false;
    };

    void print_usage(const char *program)
    {
        std::cout
            << "Usage: mpirun -np P " << program << " [options]\n\n"
            << "Options:\n"
            << "  --width N               Grid width (default: 128)\n"
            << "  --height N              Grid height (default: 128)\n"
            << "  --steps N               Number of generations (default: 100)\n"
            << "  --snapshot-interval N   Save every N steps; 0 disables snapshots (default: 10)\n"
            << "  --pattern NAME          random, glider, blinker, block (default: glider)\n"
            << "  --seed N                Seed for random pattern (default: 42)\n"
            << "  --density P             Alive probability for random pattern (default: 0.25)\n"
            << "  --output DIR            Directory for PGM frames (default: frames_mpi)\n"
            << "  --csv FILE              Append benchmark metrics to CSV file\n"
            << "  --help                  Show this message\n";
    }

    bool has_help(int argc, char **argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (std::string(argv[i]) == "--help")
            {
                return true;
            }
        }
        return false;
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
            if (arg == "--width")
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
                           int processes,
                           unsigned long long alive_cells,
                           double total_seconds,
                           double communication_seconds,
                           double computation_seconds)
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

        const double communication_percent =
            total_seconds > 0.0 ? (communication_seconds / total_seconds) * 100.0 : 0.0;

        if (write_header)
        {
            out << "implementation,processes,width,height,steps,pattern,seed,density,"
                << "snapshot_interval,alive_cells,total_seconds,communication_seconds,"
                << "computation_seconds,communication_percent\n";
        }

        out << std::setprecision(10)
            << "mpi-1d,"
            << processes << ','
            << options.width << ','
            << options.height << ','
            << options.steps << ','
            << options.pattern << ','
            << options.seed << ','
            << options.density << ','
            << options.snapshot_interval << ','
            << alive_cells << ','
            << total_seconds << ','
            << communication_seconds << ','
            << computation_seconds << ','
            << communication_percent << '\n';
    }

    std::size_t rows_for_rank(std::size_t height, int world_size, int rank)
    {
        const auto size = static_cast<std::size_t>(world_size);
        const auto base = height / size;
        const auto remainder = height % size;
        return base + (static_cast<std::size_t>(rank) < remainder ? 1 : 0);
    }

    std::size_t first_row_for_rank(std::size_t height, int world_size, int rank)
    {
        const auto size = static_cast<std::size_t>(world_size);
        const auto base = height / size;
        const auto remainder = height % size;
        const auto rank_size = static_cast<std::size_t>(rank);
        return rank_size * base + std::min(rank_size, remainder);
    }

    int checked_int_count(std::size_t value, const std::string &name)
    {
        if (value > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            throw std::overflow_error(name + " is too large for this MPI call");
        }
        return static_cast<int>(value);
    }

    std::vector<int> build_counts(const Options &options, int world_size)
    {
        std::vector<int> counts(static_cast<std::size_t>(world_size));
        for (int rank = 0; rank < world_size; ++rank)
        {
            counts[static_cast<std::size_t>(rank)] =
                checked_int_count(rows_for_rank(options.height, world_size, rank) * options.width,
                                  "rank cell count");
        }
        return counts;
    }

    std::vector<int> build_displacements(const Options &options, int world_size)
    {
        std::vector<int> displacements(static_cast<std::size_t>(world_size));
        for (int rank = 0; rank < world_size; ++rank)
        {
            displacements[static_cast<std::size_t>(rank)] =
                checked_int_count(first_row_for_rank(options.height, world_size, rank) * options.width,
                                  "rank displacement");
        }
        return displacements;
    }

    std::size_t row_offset(std::size_t row, std::size_t width)
    {
        return row * width;
    }

    life::Cell cell_at(const std::vector<life::Cell> &grid,
                       std::size_t row,
                       std::size_t x,
                       std::size_t width)
    {
        return grid[row_offset(row, width) + x];
    }

    void set_cell(std::vector<life::Cell> &grid,
                  std::size_t row,
                  std::size_t x,
                  std::size_t width,
                  life::Cell value)
    {
        grid[row_offset(row, width) + x] = value ? 1 : 0;
    }

    void exchange_halos(std::vector<life::Cell> &current,
                        std::size_t local_rows,
                        std::size_t width,
                        int rank,
                        int world_size)
    {
        if (world_size == 1)
        {
            std::copy_n(current.data() + row_offset(local_rows, width),
                        width,
                        current.data() + row_offset(0, width));
            std::copy_n(current.data() + row_offset(1, width),
                        width,
                        current.data() + row_offset(local_rows + 1, width));
            return;
        }

        const int up = (rank - 1 + world_size) % world_size;
        const int down = (rank + 1) % world_size;

        MPI_Request requests[4];
        MPI_Irecv(current.data() + row_offset(0, width),
                  checked_int_count(width, "halo width"),
                  MPI_BYTE,
                  up,
                  100,
                  MPI_COMM_WORLD,
                  &requests[0]);
        MPI_Irecv(current.data() + row_offset(local_rows + 1, width),
                  checked_int_count(width, "halo width"),
                  MPI_BYTE,
                  down,
                  101,
                  MPI_COMM_WORLD,
                  &requests[1]);
        MPI_Isend(current.data() + row_offset(1, width),
                  checked_int_count(width, "halo width"),
                  MPI_BYTE,
                  up,
                  101,
                  MPI_COMM_WORLD,
                  &requests[2]);
        MPI_Isend(current.data() + row_offset(local_rows, width),
                  checked_int_count(width, "halo width"),
                  MPI_BYTE,
                  down,
                  100,
                  MPI_COMM_WORLD,
                  &requests[3]);

        MPI_Waitall(4, requests, MPI_STATUSES_IGNORE);
    }

    void compute_next_generation(const std::vector<life::Cell> &current,
                                 std::vector<life::Cell> &next,
                                 std::size_t local_rows,
                                 std::size_t width)
    {
        for (std::size_t y = 1; y <= local_rows; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                const auto left = (x == 0) ? width - 1 : x - 1;
                const auto right = (x + 1 == width) ? 0 : x + 1;

                const int neighbours =
                    cell_at(current, y - 1, left, width) +
                    cell_at(current, y - 1, x, width) +
                    cell_at(current, y - 1, right, width) +
                    cell_at(current, y, left, width) +
                    cell_at(current, y, right, width) +
                    cell_at(current, y + 1, left, width) +
                    cell_at(current, y + 1, x, width) +
                    cell_at(current, y + 1, right, width);

                const bool alive = cell_at(current, y, x, width) != 0;
                const bool survives = alive && (neighbours == 2 || neighbours == 3);
                const bool born = !alive && neighbours == 3;
                set_cell(next, y, x, width, (survives || born) ? 1 : 0);
            }
        }
    }

    std::filesystem::path frame_path(const std::filesystem::path &output_dir, std::size_t step)
    {
        std::ostringstream name;
        name << "frame_" << std::setw(6) << std::setfill('0') << step << ".pgm";
        return output_dir / name.str();
    }

    void gather_full_grid(const std::vector<life::Cell> &local_current,
                          std::size_t local_rows,
                          std::size_t width,
                          const std::vector<int> &counts,
                          const std::vector<int> &displacements,
                          std::optional<life::Grid> &full_grid)
    {
        MPI_Gatherv(local_current.data() + row_offset(1, width),
                    checked_int_count(local_rows * width, "local cell count"),
                    MPI_BYTE,
                    full_grid ? full_grid->cells().data() : nullptr,
                    counts.data(),
                    displacements.data(),
                    MPI_BYTE,
                    root_rank,
                    MPI_COMM_WORLD);
    }

    void maybe_write_snapshot(const Options &options,
                              const std::vector<life::Cell> &local_current,
                              std::size_t local_rows,
                              std::size_t width,
                              const std::vector<int> &counts,
                              const std::vector<int> &displacements,
                              std::optional<life::Grid> &full_grid,
                              int rank,
                              std::size_t step)
    {
        if (options.snapshot_interval == 0 || step % options.snapshot_interval != 0)
        {
            return;
        }

        gather_full_grid(local_current, local_rows, width, counts, displacements, full_grid);
        if (rank == root_rank)
        {
            life::write_pgm(frame_path(options.output_dir, step), *full_grid);
        }
    }

    std::size_t count_local_alive(const std::vector<life::Cell> &local_current,
                                  std::size_t local_rows,
                                  std::size_t width)
    {
        std::size_t alive = 0;
        for (std::size_t y = 1; y <= local_rows; ++y)
        {
            for (std::size_t x = 0; x < width; ++x)
            {
                alive += cell_at(local_current, y, x, width) != 0 ? 1 : 0;
            }
        }
        return alive;
    }

} // namespace

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);

    int rank = 0;
    int world_size = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    try
    {
        if (has_help(argc, argv))
        {
            if (rank == root_rank)
            {
                print_usage(argv[0]);
            }
            MPI_Finalize();
            return 0;
        }

        const auto options = parse_options(argc, argv);
        if (options.height < static_cast<std::size_t>(world_size))
        {
            throw std::invalid_argument("grid height must be at least the number of MPI processes");
        }

        const auto local_rows = rows_for_rank(options.height, world_size, rank);
        const auto counts = build_counts(options, world_size);
        const auto displacements = build_displacements(options, world_size);

        std::optional<life::Grid> full_grid;
        if (rank == root_rank)
        {
            full_grid.emplace(options.width, options.height);
            life::seed_pattern(*full_grid, options.pattern, options.seed, options.density);
        }

        std::vector<life::Cell> current((local_rows + 2) * options.width, 0);
        std::vector<life::Cell> next((local_rows + 2) * options.width, 0);

        MPI_Scatterv(rank == root_rank ? full_grid->cells().data() : nullptr,
                     counts.data(),
                     displacements.data(),
                     MPI_BYTE,
                     current.data() + row_offset(1, options.width),
                     checked_int_count(local_rows * options.width, "local cell count"),
                     MPI_BYTE,
                     root_rank,
                     MPI_COMM_WORLD);

        MPI_Barrier(MPI_COMM_WORLD);
        const auto start = MPI_Wtime();

        maybe_write_snapshot(options,
                             current,
                             local_rows,
                             options.width,
                             counts,
                             displacements,
                             full_grid,
                             rank,
                             0);

        double communication_seconds = 0.0;
        double computation_seconds = 0.0;

        for (std::size_t generation = 1; generation <= options.steps; ++generation)
        {
            const auto comm_start = MPI_Wtime();
            exchange_halos(current, local_rows, options.width, rank, world_size);
            communication_seconds += MPI_Wtime() - comm_start;

            const auto compute_start = MPI_Wtime();
            compute_next_generation(current, next, local_rows, options.width);
            computation_seconds += MPI_Wtime() - compute_start;

            std::swap(current, next);
            maybe_write_snapshot(options,
                                 current,
                                 local_rows,
                                 options.width,
                                 counts,
                                 displacements,
                                 full_grid,
                                 rank,
                                 generation);
        }

        const auto elapsed = MPI_Wtime() - start;
        const auto local_alive = count_local_alive(current, local_rows, options.width);
        unsigned long long global_alive = 0;
        const auto local_alive_ull = static_cast<unsigned long long>(local_alive);
        MPI_Reduce(&local_alive_ull,
                   &global_alive,
                   1,
                   MPI_UNSIGNED_LONG_LONG,
                   MPI_SUM,
                   root_rank,
                   MPI_COMM_WORLD);

        double max_elapsed = 0.0;
        double max_communication = 0.0;
        double max_computation = 0.0;
        MPI_Reduce(&elapsed, &max_elapsed, 1, MPI_DOUBLE, MPI_MAX, root_rank, MPI_COMM_WORLD);
        MPI_Reduce(&communication_seconds,
                   &max_communication,
                   1,
                   MPI_DOUBLE,
                   MPI_MAX,
                   root_rank,
                   MPI_COMM_WORLD);
        MPI_Reduce(&computation_seconds,
                   &max_computation,
                   1,
                   MPI_DOUBLE,
                   MPI_MAX,
                   root_rank,
                   MPI_COMM_WORLD);

        if (rank == root_rank)
        {
            std::cout
                << "Completed " << options.steps << " generations on "
                << options.width << "x" << options.height << " grid with "
                << world_size << " MPI processes\n"
                << "Pattern: " << options.pattern << "\n"
                << "Alive cells: " << global_alive << "\n"
                << "Elapsed seconds: " << max_elapsed << "\n"
                << "Communication seconds: " << max_communication << "\n"
                << "Computation seconds: " << max_computation << "\n";

            append_csv_result(options,
                              world_size,
                              global_alive,
                              max_elapsed,
                              max_communication,
                              max_computation);
        }
    }
    catch (const std::exception &ex)
    {
        if (rank == root_rank)
        {
            std::cerr << "Error: " << ex.what() << "\n\n";
            print_usage(argv[0]);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Finalize();
    return 0;
}
