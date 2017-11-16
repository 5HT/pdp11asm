.i8080

entry:
    jmp main

__padd16:
    mov c, m
    inx h
    mov b, m
    xchg
    dad b
    xchg
    mov m, d
    dcx h
    mov m, e
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

.include "include/__mul16.inc"
{

void puts(const char* msg @ hl) @ 0xF818;
void puthex(uint8_t @ a) @ 0xF815;
uint8_t bioskey() @ 0xF81B;

void main()
{
    uint8_t* addr = (uint8_t*)(0x76D0 + 78*3 + 8);
    uint8_t i;
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

    uint8_t x=0, y=0;
    for(;;)
    {
	uint8_t a = bioskey();
//	if(a != 0xFF) { puthex(a); puts("\r\n"); }
	switch(a)
	{
	    case 0x19: // up
		if(y==0) continue;
		y--;
		break;
	    case 0x1A: // down
		if(y==24) continue;
		y++;
		break;
	    case 0x08: // left
		if(x==0) continue;
		x--;
		break;
	    case 0x18: // right
		if(x==64) continue;
		x++;
		break;
	    default:
		continue;
	}
	*(uint8_t*)((0x76D0 + 78*3 + 8) + y*78 + x) = 'X';
	while(bioskey() != 0xFF);
    }

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
