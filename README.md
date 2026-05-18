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
```

Exemplu pentru benchmark serial fara output grafic:

```bash
./build/life_serial --width 1000 --height 1000 --steps 100 --pattern random --seed 42 --density 0.25 --snapshot-interval 0
```

## Validare serial vs MPI

Corectitudinea versiunii MPI se verifica prin comparatie bit-cu-bit cu versiunea
seriala, folosind aceeasi grila, acelasi seed si acelasi numar de generatii.

Daca MPI este disponibil, `ctest` ruleaza automat si testul
`validation_serial_vs_mpi`:

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
