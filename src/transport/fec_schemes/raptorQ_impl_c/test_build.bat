@echo off
echo Testing C compilation...

echo Compiling Symbol.c...
gcc -c -std=c99 -Wall -Wextra Symbol.c -o Symbol.o
if %errorlevel% neq 0 (
    echo Symbol.c compilation failed
    pause
    exit /b 1
)
echo Symbol.c compiled successfully

echo Compiling Helper.c...
gcc -c -std=c99 -Wall -Wextra Helper.c -o Helper.o
if %errorlevel% neq 0 (
    echo Helper.c compilation failed
    pause
    exit /b 1
)
echo Helper.c compiled successfully

echo Compiling Generators.c...
gcc -c -std=c99 -Wall -Wextra Generators.c -o Generators.o
if %errorlevel% neq 0 (
    echo Generators.c compilation failed
    pause
    exit /b 1
)
echo Generators.c compiled successfully

echo Compiling Encoder.c...
gcc -c -std=c99 -Wall -Wextra Encoder.c -o Encoder.o
if %errorlevel% neq 0 (
    echo Encoder.c compilation failed
    pause
    exit /b 1
)
echo Encoder.c compiled successfully

echo Compiling Decoder.c...
gcc -c -std=c99 -Wall -Wextra Decoder.c -o Decoder.o
if %errorlevel% neq 0 (
    echo Decoder.c compilation failed
    pause
    exit /b 1
)
echo Decoder.c compiled successfully

echo Compiling Main.c...
gcc -c -std=c99 -Wall -Wextra Main.c -o Main.o
if %errorlevel% neq 0 (
    echo Main.c compilation failed
    pause
    exit /b 1
)
echo Main.c compiled successfully

echo Linking all objects...
gcc Symbol.o Helper.o Generators.o Encoder.o Decoder.o Main.o -o test_main.exe -lm
if %errorlevel% neq 0 (
    echo Linking failed
    pause
    exit /b 1
)
echo Linking successful

echo Cleaning up...
del *.o
del test_main.exe

echo All tests passed! Project is ready for pure C compilation.
pause 