# MPI Game of Life

Simulator pentru Conway's Game of Life pe grile 2D, cu versiune seriala si
versiune MPI. Versiunea seriala este folosita ca referinta de corectitudine
pentru implementarea paralela.

## Cerinte

- CMake
- compilator C++17: GCC/MinGW, Clang sau MSVC
- optional: o implementare MPI pentru `life_mpi`

Pentru MPI se pot folosi OpenMPI/MPICH pe Linux/macOS/cluster sau MS-MPI pe
Windows.

## Structura

```text
include/life.hpp      grila, reguli, pattern-uri initiale
include/pgm.hpp       export PGM
src/life.cpp          implementarea regulilor Game of Life
src/pgm.cpp           writer PGM binar P5
src/main_serial.cpp   executabil serial
src/main_mpi.cpp      executabil MPI, descompunere 1D pe linii
tests/test_life.cpp   teste pentru logica seriala
```

## Build

### Linux/macOS/cluster

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows cu Visual Studio / MSVC

Din `Developer PowerShell for Visual Studio`:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### Windows cu MinGW/MSYS2

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Rulare seriala

### Linux/macOS/cluster

```bash
./build/life_serial --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
```

### Windows cu Visual Studio / MSVC

```powershell
.\build\Release\life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
```

### Windows cu MinGW/MSYS2

```powershell
.\build\life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
```

## Rulare MPI

CMake construieste tinta `life_mpi` doar daca gaseste MPI instalat.

Versiunea MPI foloseste descompunere 1D pe linii. Fiecare proces primeste un
bloc de randuri si schimba cate un rand halo cu vecinii de sus si jos folosind
`MPI_Isend` / `MPI_Irecv`.

### Linux/macOS/cluster

```bash
mpirun -np 4 ./build/life_mpi --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames_mpi
```

### Windows cu Visual Studio / MSVC

```powershell
mpiexec -n 4 .\build\Release\life_mpi.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames_mpi
```

### Windows cu MinGW/MSYS2

```powershell
mpiexec -n 4 .\build\life_mpi.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames_mpi
```

## Optiuni

Optiunile principale sunt aceleasi pentru executabilul serial si pentru cel MPI.

```text
--width N               latimea grilei
--height N              inaltimea grilei
--steps N               numarul de generatii
--snapshot-interval N   salveaza un frame la fiecare N pasi; 0 dezactiveaza output-ul
--pattern NAME          random, glider, blinker, block
--seed N                seed pentru pattern-ul random
--density P             probabilitatea unei celule vii pentru random
--output DIR            directorul pentru frame-uri PGM
--csv FILE              adauga metricile rularii intr-un fisier CSV
```

Exemplu pentru benchmark serial fara output grafic:

```bash
./build/life_serial --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results.csv
```

Fisierul CSV contine:

```text
implementation,processes,width,height,steps,pattern,seed,density,snapshot_interval,alive_cells,total_seconds,communication_seconds,computation_seconds,communication_percent
```

## Validare serial vs MPI

Corectitudinea versiunii MPI se verifica prin comparatie bit-cu-bit cu versiunea
seriala, folosind aceeasi grila, acelasi seed si acelasi numar de generatii.
Validarea automata compara rezultatul dupa 100 de generatii pe o grila mica.

Daca MPI este disponibil, `ctest` ruleaza automat suita `validation_suite`,
care executa testele seriale, rularea seriala de referinta, rularea MPI si
comparatia finala:

```bash
ctest --test-dir build --output-on-failure
```

Validare manuala pe Linux/macOS/cluster:

```bash
./build/life_serial --width 64 --height 64 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 100 --output frames_serial
mpirun -np 4 ./build/life_mpi --width 64 --height 64 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 100 --output frames_mpi
cmp frames_serial/frame_000100.pgm frames_mpi/frame_000100.pgm
```

Validare manuala pe Windows:

```powershell
.\build\life_serial.exe --width 64 --height 64 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 100 --output frames_serial
mpiexec -n 4 .\build\life_mpi.exe --width 64 --height 64 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 100 --output frames_mpi
fc /b frames_serial\frame_000100.pgm frames_mpi\frame_000100.pgm
```

Pentru Visual Studio, executabilele sunt de obicei in `build\Release`.

## Benchmarking

Pentru masuratori de performanta foloseste `--snapshot-interval 0`, ca timpul de
scriere pe disk sa nu influenteze rezultatele.

Benchmark-urile pot fi rulate automat cu scriptul Python:

```bash
python scripts/run_benchmarks.py --processes 1,4,8,16,32 --steps 100
```

Scriptul produce:

```text
results/results_strong.csv
results/results_weak.csv
results/summary_strong.csv
results/summary_weak.csv
```

Graficele se genereaza cu:

```bash
python scripts/plot_results.py
```

Output:

```text
plots/strong_speedup.svg
plots/strong_efficiency.svg
plots/strong_time.svg
plots/strong_compute_vs_comm.svg
plots/weak_time.svg
plots/weak_compute_vs_comm.svg
```

Strong scaling: dimensiunea problemei ramane fixa, iar numarul de procese creste.
Setul de procese folosit este: `p = 1, 4, 8, 16, 32`.

```powershell
mpiexec -n 1 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_strong.csv
mpiexec -n 4 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_strong.csv
mpiexec -n 8 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_strong.csv
mpiexec -n 16 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_strong.csv
mpiexec -n 32 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_strong.csv
```

Weak scaling: dimensiunea problemei creste odata cu numarul de procese.
Pentru weak scaling, fiecare proces primeste aproximativ aceeasi cantitate de
lucru, deci dimensiunea grilei creste proportional cu `p`.

```powershell
mpiexec -n 1 .\build\life_mpi.exe --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_weak.csv
mpiexec -n 4 .\build\life_mpi.exe --width 1000 --height 4000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_weak.csv
mpiexec -n 8 .\build\life_mpi.exe --width 1000 --height 8000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_weak.csv
mpiexec -n 16 .\build\life_mpi.exe --width 1000 --height 16000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_weak.csv
mpiexec -n 32 .\build\life_mpi.exe --width 1000 --height 32000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0 --csv results_weak.csv
```

Metricile cerute in raport se calculeaza astfel:

```text
speedup(p) = T1 / Tp
eficienta(p) = speedup(p) / p
procent comunicatie = communication_seconds / total_seconds * 100
```

Pentru strong scaling, scriptul foloseste ca baseline rularea MPI cu un singur
proces (`mpi-1d`, `p = 1`). Rularea seriala ramane in CSV-ul brut pentru
comparatie, dar nu este folosita la eficienta paralela.

## Vizualizare

Output-ul grafic este salvat ca fisiere `PGM P5` in directorul ales cu
`--output`.

Conversie in GIF cu ImageMagick:

```bash
magick frames/frame_*.pgm life.gif
```

Conversie in MP4 cu ffmpeg:

```bash
ffmpeg -framerate 12 -i frames/frame_%06d.pgm life.mp4
```
