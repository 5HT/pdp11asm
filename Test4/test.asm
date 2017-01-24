; Miner for BK0010 / САПЕР ДЛЯ БК0010
; (c) 5-03-2012 VINXRU (aleksey.f.morozov@gmail.com)

CONVERT1251TOKOI8R
DECIMALNUMBERS
ORG 01000

;----------------------------------------------------------------------------        

EntryPoint:
    MOV #3FFEh, SP
    MOV #main, PC

;----------------------------------------------------------------------------

SHLW:
    BIC #0FFF0h, R1
    BEQ SHLW1
SHLW2:
    ADD R0, R0
    SOB R1, SHLW2
SHLW1:
    RTS PC

;----------------------------------------------------------------------------

SHRW:
    BIC #0FFF0h, R1
    BEQ SHRW1
SHRW2:
    CLC
    ROR R0
    SOB R1, SHRW2
SHRW1:
    RTS PC

;----------------------------------------------------------------------------

__SWITCH:
    MOV (SP)+, R1
__SWITCH0:
    MOV (R1)+, R5
    BEQ __SWITCH1
    CMP R0, (R1)+
    BNE __SWITCH0
    MOV R5, PC
__SWITCH1:
    MOV (R1)+, PC

;----------------------------------------------------------------------------

SGNB0:
    BIC #0xFF00, R0
    TSTB R0
    BPL SGNB0R
    BIS #0xFF00, R0
SGNB0R:
    RTS PC

;----------------------------------------------------------------------------
; ДЕЛЕНИЕ
; R0/R1

DIVWU:
    CLR R5
DIVWU1:
    SUB R1, R0
    BCS DIVWU2
    INC R5
    BR DIVWU1
DIVWU2:
    ADD R1, R0
    MOV R0, MOD_DIV
    MOV R5, R0
    RTS PC

MOD_DIV: DW 0

;----------------------------------------------------------------------------
; void drawb(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);

drawb:
    MOV R1, R4
    MOV R2, R5
drawb1:
    MOVB (R0)+,(R4)+
    SOB R5, drawb1
    ADD #64., R1
    SOB R3, drawb
    RTS PC

;----------------------------------------------------------------------------
; void draw(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);

draw:
    MOV R1, R4
    MOV R2, R5
draw1:
    MOV (R0)+,(R4)+
    MOV (R0)+,(R4)+
    SOB R5, draw1
    ADD #64., R1
    SOB R3, draw
    RTS PC

;----------------------------------------------------------------------------
; void drawa(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);

drawa:
    MOV R1, R4
    MOV R2, R5
drawa1:
    BIC (R0)+,(R4)
    BIS (R0)+,(R4)+
    SOB R5, drawa1
    ADD #64., R1
    SOB R3, drawa
    RTS PC

;----------------------------------------------------------------------------

CLEARSCREEN:
    MOV #4000h, R0
    MOV #800h, R1
CLEARSCREEN1:
    CLR (R0)+
    CLR (R0)+
    CLR (R0)+
    CLR (R0)+
    SOB R1, CLEARSCREEN1
    RTS PC

;----------------------------------------------------------------------------

{

// BIOS

uint8_t     getc() @ emt 6;
void        putc(char c @ r0) @ emt 016;
void        gotoxy(uint16_t x @ r1, uint16_t y @ r2) @ emt 024;
const char* puttext(const char* @ r1, uint16_t flags @ r2) @ emt 020, r1;
uint16_t    displaystatus() @ emt 034;

// ASM

extern uint16_t mod_div;
void draw (void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);
void drawa(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);
void drawb(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);

// C

uint8_t rand();
void    startGame();
void    clearScreen();
void    print(const char*);
void    fillBlocks();
void    drawSmile(void*);
void    drawPlayField();
void    leftNumber();
void    rightNumber();
void    hideCursor();
void    drawCursor();
void    putBombs();
void    open(unsigned x, unsigned y);
void    checkWin();
void*   callCell2(unsigned x, unsigned y);
void*   getBitmap(uint8_t n);
void    redrawCell012(unsigned x, unsigned y);

uint16_t gameWidth    = 0;
uint16_t gameHeight   = 0;
uint16_t gameWidth1   = 0;
uint16_t gameHeight1  = 0;
uint16_t gameOverFlag = 0;
uint16_t cursorX      = 0;
uint16_t cursorY      = 0;
uint16_t playfieldVA  = 0;
uint16_t bombsCnt     = 0;
uint16_t bombsPutted  = 0;
uint16_t time         = 0;
uint8_t  lastTimer    = 0;
uint8_t  playfield[256];

extern void    bmpLogo[];
extern uint8_t bmpGood[];
extern uint8_t bmpWin[];
extern uint8_t bmpBad[];
extern uint8_t bmpCursor[];
extern uint8_t bmpUn[];
extern uint8_t bmpB[];
extern uint8_t bmpN0[];
extern uint8_t bmpBlock[];

// "0. Лошара\x00"
// "1. Новичок\x00"
// "2. Любитель\x00"
// "3. Профессионал\x00"

const char txtMenu[] =
    "\x0A\x0C" "\x920. Abc\x00"
    "\x0A\x0D" "1. Defg\x00"
    "\x0A\x0E" "2. Hijkl\x00"
    "\x0A\x0F" "3. Mnopqr\x00"
    "\x0C\x16" "\x91(c) 2012\x00"
    "\x03\x17" "\x93aleksey.f.morozov@gmail.com\x00\x00";

void puttext2(const char* a)
{
    do
    {
        uint8_t x = (uint8_t)*a++;
        gotoxy(x, (uint8_t)*a++);
        a = puttext(a,0);
    }
    while(*a);
}

uint8_t getc2()
{
    if((*(uint8_t*)0177660 & 128) == 0) return 0; // Нажата ли клавиша
    return (*(uint8_t*)0177660 & 128) ? *(uint8_t*)0177662 : 0;
}

void main()
{
    if((displaystatus() & 1) == 0) putc(0233); // Включение режима 256x256
    putc(0x9A); // Выключение курсора
    *(uint16_t*)0177706 = 731; // Запуск таймера
    *(uint16_t*)0177712 = 0160; // Запуск таймера
    *(uint16_t*)0177660 = 64; // Выключаем прерывание клавиатуры

    gameWidth=13; gameHeight=10; bombsCnt=20; playfieldVA=20486;
    startGame();

    for(;;)
    {
        clearScreen();
        draw((void*)045020, bmpLogo, 8, 32);
        puttext2(txtMenu);
        for(;;)
        {
            rand(); // Инициализируем генератор случайных чисел.
            switch(getc2())
            {
                case '0': gameWidth=9;  gameHeight=9;  bombsCnt=3;  playfieldVA=21006; break;
                case '1': gameWidth=9;  gameHeight=9;  bombsCnt=10; playfieldVA=21006; break;
                case '2': gameWidth=13; gameHeight=10; bombsCnt=20; playfieldVA=20486; break;
                case '3': gameWidth=16; gameHeight=14; bombsCnt=43; playfieldVA=18432; break;
                default: continue;
            }
            break;
        }

        startGame();
    }
}

void startGame()
{
    gameWidth1 = gameWidth-1;
    gameHeight1 = gameHeight-1;

    // Очистка экрана
    {   clearScreen();
        unsigned x,y;
        uint8_t* a = (uint8_t*)(040000 + 64*16*2);
        for(y=0; y<14; y++, a+=64*15)
            for(x=0; x<16; x++, a+=4)
                draw(a, bmpBlock, 1, 16);
    }

    // Очистка основных переменных
    cursorX = gameWidth >> 1;
    cursorY = gameHeight >> 1;
    bombsPutted = 0;
    gameOverFlag = 0;
    time = 0;

    // Очистка игрового поля
    {   unsigned i = 0;
        do
        {
            playfield[i] = 0;
            i++;
        } while(i != 256);
    }

    // Вывод смайлика
    drawSmile(bmpGood);

    // Рисование игрового поля
    drawPlayField();

    // Вывод чисел
    leftNumber();
    rightNumber();
		
    for(;;)
    {
        rand();

        if(!gameOverFlag && time!=999)
        {
            uint8_t t = *(uint16_t*)0177710 > 365;
            if(t != lastTimer)
            {
                lastTimer = t;
    		time++;
                rightNumber();
            }
        }

	// Анализируем клавиатуру
        switch(getc2())
        {
            case 0:
                break;
            case 8:
                if(cursorX==0) continue;
                hideCursor();
                cursorX--;
                drawCursor();
                continue;
                break;
            case 0x19:
                if(cursorX+1 == gameWidth) continue;
                hideCursor();
                cursorX++;
                drawCursor();
                break;
            case 0x1A:
                if(cursorY == 0) continue;
                hideCursor();
                cursorY--;
                drawCursor();
                break;
            case 0x1B:
                if(cursorY+1 == gameHeight) continue;
                hideCursor();
                cursorY++;
                drawCursor();
                break;
            case ' ':
                if(gameOverFlag) return;
                if(!bombsPutted) putBombs();
            	open(cursorX, cursorY);
                continue;
            default:
                if(gameOverFlag) return;
                uint8_t* a = &playfield[(cursorY << 4) + cursorX];
                uint8_t v = *a;
                if((v & 0x7F) > 2) continue;
                v++;
                if((v & 0x7F) == 3) v = v - 3;
                *a = v;
                hideCursor();
            	drawCursor();
                leftNumber();
                continue;
        }
    }
}

//----------------------------------------------------------------------------
// ПОМЕСТИТЬ БОМБЫ НА ПОЛЕ

void putBombs()
{
    unsigned x,y,bc;
    bombsPutted = 1;
    bc = bombsCnt;
    while(bc)
    {
	x = rand(); x = (x>>4) ^ (x&0xF);
	y = rand(); y = (y>>4) ^ (y&0xF);
        if(x >= gameWidth || y >= gameHeight) continue;
        if(cursorX==x && cursorY==y) continue; // Бомба не должна быть под крсором
        uint8_t* a = &playfield[(y<<4) + x];
        if(*a == 0x80) continue; // Бомба в этой клетке уже есть
        *a = 0x80;
        bc--;
    }
}

uint16_t calcCell2(uint16_t x, uint16_t y)
{
    return (y<<10) + (x<<2) + playfieldVA;
}

void hideCursor()
{
    redrawCell012(cursorX, cursorY);
}

void drawCursor()
{
    drawa(calcCell2(cursorX, cursorY), bmpCursor, 2, 16);
}

unsigned open_x, open_y, open_n;
uint8_t* open_a;

void open_check()
{
    if(open_x < gameWidth && open_y < gameHeight && (*open_a & 0x80) != 0) open_n++;
}

void open_int()
{
    if(open_x >= gameWidth || open_y >= gameHeight) return;
    if(*open_a & 0x80)
    {
        gameOverFlag = 1;
        return;
    }
    if(*open_a != 0) return;
    open_n = 3;
    open_x--; open_y--; open_a -= 17;
    open_check(); open_x++;  open_a++;
    open_check(); open_x++;  open_a++;
    open_check(); open_y++;  open_a+=16;
    open_check(); open_x-=2; open_a-=2;
    open_check(); open_y++;  open_a+=16;
    open_check(); open_x++;  open_a++;
    open_check(); open_x++;  open_a++;
    open_check();
    open_x--; open_y--; open_a -= 17;
    *open_a = open_n;
    if(open_n == 3)
    {
        open_x--; open_y--;  open_a-=17;
        open_int();  open_x++;  open_a++;
        open_int();  open_x++;  open_a++;
        open_int();  open_y++;  open_a+=16;
        open_int();  open_x-=2; open_a-=2;
        open_int();  open_y++;  open_a+=16;
        open_int();  open_x++;  open_a++;
        open_int();  open_x++;  open_a++;
        open_int();
        open_x--;  open_y--; open_a -= 17;
    }
    redrawCell012(open_x, open_y);
}

void open(unsigned x, unsigned y)
{
    hideCursor();
    open_x = x;
    open_y = y;
    open_a = &playfield[(y << 4) + x];
    open_int();
    drawCursor();
    if(gameOverFlag)
    {
        drawSmile(bmpBad);
        drawPlayField();
    }
    else
    {
        uint8_t* a = playfield;
        for(y=0; y<gameHeight; y++, a+=16, a-=gameWidth)
            for(x=0; x<gameWidth; x++, a++)
                if(*a == 0)
                    return;

        drawSmile(bmpWin);
        gameOverFlag = 1;
        drawPlayField();
    }
}

void* getBitmap(uint8_t n)
{
    if((n & 0x80) != 0 && gameOverFlag) return bmpB;
    return (((uint16_t)(n & 0x7F))<<6) + bmpUn;
}

void redrawCell012(unsigned x, unsigned y)
{
    draw(calcCell2(x,y), getBitmap(playfield[(y<<4)+x]), 1, 16);
}

uint8_t rand_state = 0xFA;
		
uint8_t rand()
{
    rand_state = (uint8_t)(rand_state << (uint8_t)2) + rand_state + (uint8_t)1;
    return rand_state;
}

void drawNumber(uint8_t* d, uint16_t n)
{
    uint8_t c=3;
    do
    {
        n = n / (uint16_t)10;
        drawb(d, bmpN0 + (mod_div << 6), 3, 21);
        d -= 3;
    } while(--c);
}

void leftNumber()
{
    unsigned i, b = bombsCnt;
    uint8_t* a;
    for(i=256, a=playfield; i!=0; i--, a++)
    {
        if((*a & 0x7F) == 1)
        {
            if(b==0) break;
            b--;
        }
    }
    drawNumber((void*)040510, b);
}

void rightNumber()
{
    drawNumber((void*)040573, time);
}

//-----------------------------------------------
// Вывод смайлика

void drawSmile(void* img)
{
    drawb((void*)040435, img, 6, 24);
}

//----------------------------------------------------------------------------
// Рисование игрового поля и курсора
// => Портит все регистры

void drawPlayField() 
{
    unsigned x, y;
    for(y=0; y<gameHeight; y++)
        for(x=0; x<gameWidth; x++)
           redrawCell012(x,y);
    drawCursor();
}

}

;----------------------------------------------------------------------------

drawTransImage:
    MOV     #16, R2
drawTransImag1:	BIC     (R0)+, (R1)
    BIS     (R0)+, (R1)+
    BIC     (R0)+, (R1)
    BIS     (R0)+, (R1)+
    ADD     #60, R1
    SOB	    R2, drawTransImag1
    RTS     PC

;----------------------------------------------------------------------------

drawImage:
    MOV     #16, R2
drawImage1:
    MOV     (R0)+, (R1)+
    MOV     (R0)+, (R1)+
    ADD     #60, R1
    SOB	R2, drawImage1
    RTS	PC

;----------------------------------------------------------------------------

.include "resources.inc"

endOfROM:              

;-----------------------------------------------

make_bk0010_rom "bk0010_miner.bin", EntryPoint, endOfROM
