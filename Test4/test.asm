    org 1000h
entry:
    MOV #1000h, SP
    BR MAIN
{

void test(int b)
{
    int a = b;
}

void main() 
{
    int16_t o = 1;
    o = o-o;
    uint16_t* a;
    test(1);
    for(;;)
    {
        a = (uint16_t*)0x4000;
	do
        {
	    uint16_t x = 0;
	    do {
        	*a ^= 0xAAAA;
        	a++;
		x++;
	    } while(x < 32);
	    do {
        	*a ^= 0x5555;
        	a++;
		x++;
	    } while(x < 64);
	} 
	while(a < (uint16_t*)0x8000);

        a = (uint16_t*)0x8000;
	do
        {
	    uint16_t x = 0;
	    do {
        	a--;
        	*a ^= 0xAAAA;
		x++;
	    } while(x < 32);
	    do {
        	a--;
        	*a ^= 0x5555;
		x++;
	    } while(x < 64);
	} 
	while(a > (uint16_t*)0x4000);
    }
}

}

make_bk0010_rom "test.bin", entry
