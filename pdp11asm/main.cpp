// PDP11 Assembler (c) 15-01-2015 vinxru

#include "compiler.h"
#include <iostream>

#ifdef WIN32
int _tmain(int argc, wchar_t** argv) {
    setlocale(LC_ALL, "RUSSIAN");
#else // LINUX
int main(int argc, char** argv) {
#endif
    printf("PDP11/8080 Assembler/C\nPRE PRE PRE ALPHA VERSION\n2017 (c) aleksey.f.morozov@gmail.com\n");
    try {
        Compiler c;

        // DEBUG
        c.compileFile("../Test4/test.asm");
        std::cout << "Done" << std::endl;
        return 0;

        // Ожидается один аргумент
        if(argc != 2) {
            std::cout << "Specify one file name on the command line" << std::endl;
            return 0;
        }
    
        // Компиляция
        c.compileFile(argv[1]);

        // Выход без ошибок
        std::cout << "Done" << std::endl;
        return 0;

        // Выход с ошибками
    } catch(std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }
}

#ifdef __MINGW32__
#include <windows.h>

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, INT nCmdShow)
{
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    return _tmain(argc, argv);
}
#endif
