.i8080

entry:
    jmp main

__sgnb0:
    mov l, a
    mvi h, 0
    ret

{

void puts(const char* msg @ hl) @ 0xF818;

void main()
{
    char str[16];
    unsigned char i;

    for(i=0;;i++)
    {
        str[0] = 'A' + (i & 15);
        str[1] = 13;
        str[2] = 10;
        str[3] = 0;
        puts(str);
    }
}

}

make_radio86rk_rom "test.rk", entry
