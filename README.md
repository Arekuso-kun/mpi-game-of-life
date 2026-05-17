# MPI Game of Life

Simulator pentru Conway's Game of Life, pregatit pentru extensie MPI. Prima etapa este
implementarea seriala, folosita ca referinta de corectitudine pentru versiunea paralela.

## Structura

```text
include/life.hpp      reguli, grila, pattern-uri initiale
include/pgm.hpp       export PGM
src/life.cpp          implementarea algoritmului serial
src/pgm.cpp           writer PGM binar P5
src/main_serial.cpp   CLI pentru rularea versiunii seriale
```

## Build si rulare

### Cerinte

Ai nevoie de:

- CMake;
- un compilator C++ cu suport C++17: GCC/MinGW, Clang sau MSVC.

Verifica in terminal:

```powershell
cmake --version
g++ --version   # pentru GCC/MinGW
cl              # pentru MSVC, din Developer PowerShell for Visual Studio
```

### Linux/macOS

```bash
cmake -S . -B build
cmake --build build
./build/life_serial --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
```

Testele se ruleaza cu:

```bash
ctest --test-dir build --output-on-failure
```

Alternativ, fara CMake:

```bash
g++ -std=c++17 -O3 -Iinclude src/main_serial.cpp src/life.cpp src/pgm.cpp -o life_serial
g++ -std=c++17 -O2 -Iinclude tests/test_life.cpp src/life.cpp src/pgm.cpp -o life_tests
./life_serial --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
./life_tests
```

### Windows cu Visual Studio / MSVC

Din "Developer PowerShell for Visual Studio":

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
ctest --test-dir build -C Release --output-on-failure
```

Daca CMake genereaza executabilele in `Debug`, foloseste:

```powershell
.\build\Debug\life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
```

### Windows cu MinGW/MSYS2

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
./build/life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
ctest --test-dir build --output-on-failure
```

Alternativ, fara CMake:

```bash
g++ -std=c++17 -O3 -Iinclude src/main_serial.cpp src/life.cpp src/pgm.cpp -o life_serial.exe
g++ -std=c++17 -O2 -Iinclude tests/test_life.cpp src/life.cpp src/pgm.cpp -o life_tests.exe
./life_serial.exe --width 200 --height 200 --steps 100 --pattern glider --snapshot-interval 10 --output frames
./life_tests.exe
```

## Optiuni utile

Pattern-uri disponibile:

```text
random, glider, blinker, block
```

Pentru un test random reproductibil:

```bash
./build/life_serial --width 512 --height 512 --steps 200 --pattern random --seed 42 --density 0.25
```

Output-ul grafic este salvat ca fisiere `PGM P5` in directorul `frames/`.

## Conversie imagini

Cu ImageMagick:

```bash
magick frames/frame_*.pgm life.gif
```

Cu ffmpeg:

```bash
ffmpeg -framerate 12 -i frames/frame_%06d.pgm life.mp4
```
