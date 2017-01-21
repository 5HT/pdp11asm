; Miner for BK0010 / САПЕР ДЛЯ БК0010
; (c) 5-03-2012 VINXRU (aleksey.f.morozov@gmail.com)

CONVERT1251TOKOI8R
DECIMALNUMBERS
ORG 01000

;----------------------------------------------------------------------------        
; МЕНЮ        

EntryPoint:
		MOV #16384, SP
        MOV @#main, PC

SHLW:
SHRW:
DIVWI:
MODWI:
MULWI:
__SWITCH:
MEMSET:
GOTOXY:
PUTTEXT:
PUTC:
    MOV #MAIN, PC
{

extern uint16_t mod_div;

uint16_t gameWidth    = 0;
uint16_t gameHeight   = 0;
uint16_t gameOverFlag = 0;
uint16_t cursorX      = 0;
uint16_t cursorY      = 0;
uint16_t playfieldVA  = 0;
uint16_t bombsCnt     = 0;
uint16_t bombsPutted  = 0;
uint16_t time         = 0;
uint16_t lastTimer    = 0;
uint8_t  playfield[256];
uint8_t  userMarks[256];
uint8_t  rand();

void putc(char c @ r0) @ emt016;
void gotoxy(uint16_t x @ r1, uint16_t y @ r2) @ emt024;
void draw(void* d @ r1, void* s @ r0, uint16_t w @ r2, uint16_t h @ r3);
void startGame(uint16_t* params);
void clearScreen();
void print(const char*);
void fillBlocks();
void drawSmile(void*);
void drawPlayField();
void leftNumber();
void rightNumber();
void hideCursor();
void drawCursor();
void putBombs();
void open(unsigned x, unsigned y);
void checkWin();

extern void bmpLogo[];
extern void bmpGood[];
extern void bmpWin[];
extern void bmpBad[];
extern void bmpCursor[];
extern uint8_t bmpUn[];
extern uint8_t bmpN0[];

const char* txtMenu =
    "\x0A\x0C" "0. Лошара\x00"
    "\x0A\x0D" "1. Новичок\x00"
    "\x0A\x0E" "2. Любитель\x00"
    "\x0A\x0F" "3. Профессионал\x00"
    "\x09\x16" "(c) 2012 VINXRU\x00"
	"\x03\x17" "aleksey.f.morozov@gmail.com\x00\xFF";

//txtMenu:	DB 10,12,0222,"0. Лошара",0
//		DB 10,13,0222,"1. Новичок",0
//		DB 10,14,     "2. Любитель",0
//		DB 10,15,     "3. Профессионал",0
//		DB  9,22,0221,"(c) 2012 VINXRU",0
//		DB  3,23,0223,"aleksey.f.morozov@gmail.com",0,255

uint16_t menuItems[] = {
    9, 9, 3, 21006, // ширина, высота, кол-во бомб, положение на экране
    9, 9, 10, 21006,
    13, 10, 20, 20486,
    16, 14, 43, 18432
};

void main()
{
    putc(0233); // Включение режима 256x256
    putc(0232); // Отключение курсора
    *(uint16_t*)0177706 = 731; // Запуск таймера
    *(uint16_t*)0177712 = 0160; // Запуск таймера
    *(uint16_t*)0177660 = 64; // Выключаем прерывание клавиатуры
    
    for(;;)
    {
        clearScreen();
        draw((void*)045020, bmpLogo, 16, 32);
        print(txtMenu);

        uint8_t key;
        do
        {
            rand(); // Инициализируем генератор случайных чисел.
	    	key = *(uint16_t*)0177662 - '0';
        } while(key >= 4);

//        startGame(menuItems[key*4]);
    }
}

void startGame(uint16_t* params)
{
    // Параметры игрового поля
    gameWidth = *params++;
	gameHeight = *params++;
	bombsCnt = *params++;
	playfieldVA = *params;

	// Очистка экрана
	clearScreen();
	fillBlocks();

	// Устанавливаем курсор в центр поля
	cursorX = gameWidth / 2;
    cursorY = gameHeight / 2;

    // Очистка основных переменных
    bombsPutted = 0;
    gameOverFlag = 0;
    time = 0;

    // Очистка игрового поля
    uint8_t i = 0;
    do 
    {
        playfield[i] = 254;
        userMarks[i] = 0;
        i++;
    } while(i != 0);

    // Вывод смайлика
    drawSmile(bmpGood);

    // Рисование игрового поля
	drawPlayField();
	
    // Вывод чисел
    leftNumber();
    rightNumber();
		
    for(;;)
    {
        if(!gameOverFlag && time!=999)
        {
//    		; Прошла секунда?
//    		CLR R0
//    		CMP @#0177710, #365
//    		ADC R0
//    		CMP lastTimer, R0
//    		BEQ mainLoop1
//    		MOV R0, lastTimer
    		time++;
            rightNumber();
        }

        // Нажата ли клавиша
		if((*(uint16_t*)0177660 & 128) == 0) continue;

		// Анализируем клавиатуру
		switch(*(uint16_t*)0177662)
        {
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
                if(!bombsPutted) putBombs();
                if(gameOverFlag) continue;
            	open(cursorX, cursorY);
            	drawCursor();
            	checkWin();
                continue;
            default:
                if(gameOverFlag) continue;
                unsigned a = cursorX + cursorY * 16;
                unsigned t = userMarks[a] + 1;
                userMarks[a] = t<3 ? t : 0;
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
    bombsPutted = 1;
	unsigned bc = bombsCnt;
    do 
    {
        unsigned x = rand() % gameWidth;
        unsigned y = rand() % gameHeight;
        if(cursorX==x && cursorY==y) continue; // Бомба не должна быть под крсором
        unsigned a = x+y*16;
        if(playfield[a] == 255) continue; // Бомба в этой клетке уже есть
        playfield[a] = 255;
    } while(--bc);
}

void* callCell2(unsigned x, unsigned y);
void* getBitmap(uint8_t n);
void redrawCell012(unsigned x, unsigned y);
void drawTransImage(void*, void*, unsigned, unsigned);

void hideCursor()
{
    redrawCell012(cursorX, cursorY);
}

void drawCursor()
{
//    drawTransImage(calcCell2(cursorX, cursorY), bmpCursor, 4, 16);
}

uint16_t calcCell2(uint16_t x, uint16_t y)
{
    return y*1024 + x*4;
}

void open(unsigned x, unsigned y)
{
    if(x >= gameWidth || y >= gameHeight) return;
    uint8_t a = playfield[x + y*16];
    switch(a)
    {
        case 255:
            drawSmile(bmpBad);
            gameOverFlag = 1;
            break;
        case 254:
            return;
    }
    playfield[x + y*16] = 0;
}

//die:		; Вывод смайлика
//		MOV #bmpBad, R0
//		JSR PC, @#drawSmile
//		JMP @#gameOver

void* getBitmap(uint8_t n)
{
    n += 2;
    if(!gameOverFlag && n==1) n=0;
    return bmpUn + n*64;
}

void redrawCell012(unsigned x, unsigned y)
{
    draw(calcCell2(x,y), getBitmap(playfield[x+y*16]), 4, 16);
}

uint16_t rand_state = 0x1245;
		
uint8_t rand()
{
    rand_state = (rand_state << 2) ^ (rand_state >> 5);
    return rand_state;
}

void checkWin()
{
    unsigned y = 0;
    do
    {
        unsigned x = 0;
        do
        {
            if(playfield[x+y*16] == 254) return;
        } while(++x < gameWidth)
    } while(++y < gameHeight);    

    drawSmile(bmpWin);
    //gameOver:
    gameOverFlag = 1;
    drawPlayField();
}

void drawNumber(uint8_t* d, uint16_t n)
{
    uint8_t c=3;
    do
    {
        n /= 10;
        draw(d, bmpN0 + mod_div*64, 3, 21);
        d += 3;
    } while(--c);
}

void leftNumber()
{
    unsigned c=0, b=bombsCnt;
    do
    {
        if(userMarks[c] == 1) b--;
    } while(b!=0 && ++c);
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
    draw((void*)040435, img, 4, 24);
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

void memset(void*,uint8_t,unsigned);

void clearScreen()
{
    memset((void*)040000, 0, 040000);
}

void drawTransImage(uint16_t* s @ r0, uint16_t* d @ r1);
void drawImage(uint16_t* s @ r0, uint16_t* d @ r1);
void puttext(const char*);

void print(uint8_t* s)
{
    do
    {
        uint8_t y = *s++;
        gotoxy(*s++, y);
        puttext(*s);
        while(*s) s++;
    } while(*s != 0xFF)
}

}

FILLBLOCKS:
        MOV #044000, R0
                MOV #14, R4
fillBlocks3:	MOV #bmpBlock, R1
                MOV #16, R3
fillBlocks2:	MOV #16, R2
fillBlocks1:	MOV (R1)+, (R0)+
                MOV (R1)+, (R0)+
                SUB #4, R1
                SOB R2, fillBlocks1
                ADD #4, R1
                SOB R3, fillBlocks2
                SOB R4, fillBlocks3
                RTS PC


drawTransImage:
    MOV     #16, R2
drawTransImag1:	BIC     (R0)+, (R1)
    BIS     (R0)+, (R1)+
    BIC     (R0)+, (R1)
    BIS     (R0)+, (R1)+
    ADD     #60, R1                        
    SOB	    R2, drawTransImag1
    RTS     PC

drawImage:
    MOV     #16, R2
drawImage1:
    MOV     (R0)+, (R1)+
    MOV     (R0)+, (R1)+
    ADD     #60, R1
    SOB	R2, drawImage1
    RTS	PC

;----------------------------------------------------------------------------

SHLW:
    RTS PC

;----------------------------------------------------------------------------

draw:
    MOV R4, R1
    MOV R5, R2
draw1:
    MOV (R0)+,(R4)+
    SOB R5, draw1
    ADD #64, R1
    SOB R3, draw
    RTS PC


.include "resources.inc"

endOfROM:              

;-----------------------------------------------

make_bk0010_rom "bk0010_miner.bin", EntryPoint, endOfROM
