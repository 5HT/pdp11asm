    org 1000h
entry:
    MOV #1000h, SP
    BR MAIN
puttext:
    MOV R1, -2(SP)
    CLR R2
    EMT 20
    RET
{

int g1 = 1;
long g2 = 256*256*256;
const char* g3 = "Hello world";
const char g4[] = "xyz";
const char* g5[] = { "xyz", "as", "xx" };
const short g5[16] = { 1,2,3,4,5 }; 

void puttext(const char* text);

void test(int b)
{
    int a = b;
}

void main() 
{
    int16_t o = 1;
    o = g2-g1;
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
