    org 1000h
entry:
    MOV #1000h, SP
    BR MAIN

puttext:
    MOV 2(SP), R1
    CLR R2
    EMT 20
    MOV 4(SP), R1
    CLR R2
    EMT 20
    RET
{

void puttext(const char* text, const char* text2);

void puttext2(const char* text, const char* text2)
{
    const char* text3 = text;
    const char* text4 = text2;
    puttext(text4, text3);
}

void main() 
{
    const char* a = " world";
    const char* b = "Hello";
    puttext2(a, b);
}

}

make_bk0010_rom "test.bin", entry
