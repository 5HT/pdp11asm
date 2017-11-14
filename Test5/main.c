.i8080

entry:
    jmp main

__padd16:
    mov c, m
    inx h
    mov b, m
    xchg
    dcx b
    xchg
    mov m, e
    dcx h
    mov m, d
    mov l, c
    mov h, b
    ret

__sadd16:
    mov c, m
    inx h
    mov b, m
    xchg
    dcx b
    xchg
    mov m, e
    dcx h
    mov m, d
    ret

__sgnb0:
    mov l, a
    mvi h, 0
    ret
    
__or16:
    mov a, h
    ora d
    mov h, a
    mov a, l
    ora e
    mov l, a
    ret
{

void puts(const char* msg @ hl) @ 0xF818;

unsigned char i, j;
unsigned int* pi;
unsigned int ii;
uint8_t* addr;

void main()
{
    i = 1;
    i = 2;
    i = 3;
//    i++;
    i = ~i;
    j = i;
    
/*
    addr = (uint8_t*)(0x76D0 + 78*3 + 8);
    for(i=64; i; i--)
    {
	*addr = '*';
	addr++;
    }

    addr = (uint8_t*)(0x76D0 + 78*(3+24) + 8);
    for(i=64; i; i--)
    {
	*addr = '*';
	addr++;
    }

    addr = (uint8_t*)(0x76D0 + 78*3 + 8);
    for(i=25; i; i--)
    {
	*addr = '*';
	addr+=63;
	*addr = '*';
	addr+=78-63;
    }
*/
    for(;;);    

    // Не проходит компиляцию
    // *addr++ = '*'
    
    // Не проходит компиляцию
    // i = (i=1) + (i=2);

    // Не проходит компиляцию
    // j = i = 1;
}

}

make_radio86rk_rom "test.rk", entry
