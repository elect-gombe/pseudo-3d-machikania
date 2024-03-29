// rotatevideoデモプログラム PIC32MX370F512H用　by K.Tanaka
// アルファベットスクロール、カラーバー、直線、円、ビットマップキャラクター移動

#include <plib.h>
#include "rotatevideo.h"
#include "graphlib.h"
#include <math.h>
#include <stdint.h>

//外付けクリスタル with PLL (20/3倍)
//クリスタルは3.579545×4＝14.31818MHz
#pragma config FSRSSEL = PRIORITY_7
#pragma config PMDL1WAY = OFF
#pragma config IOL1WAY = OFF
//#pragma config FUSBIDIO = OFF
//#pragma config FVBUSONIO = OFF
#pragma config FPLLIDIV = DIV_3
#pragma config FPLLMUL = MUL_20
//#pragma config UPLLIDIV = DIV_1
//#pragma config UPLLEN = OFF
#pragma config FPLLODIV = DIV_1
#pragma config FNOSC = PRIPLL
#pragma config FSOSCEN = OFF
#pragma config IESO = OFF
#pragma config POSCMOD = XT
#pragma config OSCIOFNC = OFF
#pragma config FPBDIV = DIV_1
#pragma config FCKSM = CSDCMD
#pragma config FWDTEN = OFF
#pragma config DEBUG = OFF
#pragma config PWP = OFF
#pragma config BWP = OFF
#pragma config CP = OFF

// 入力ボタンのポート、ビット定義
#define KEYPORT PORTD
#define KEYDOWN 0x0001
#define KEYLEFT 0x0002
#define KEYUP 0x0004
#define KEYRIGHT 0x0008
#define KEYSTART 0x0010
#define KEYFIRE 0x0020

// 32x32ドットキャラクターデータ
const unsigned char bmp1[32*32]=
{
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,5,7,0,5,7,7,5,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,1,6,3,5,7,3,5,2,5,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,5,7,0,7,5,7,6,1,6,5,3,6,1,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,2,5,7,0,7,0,5,7,7,5,7,1,6,1,4,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,5,7,5,7,5,7,7,0,3,0,4,7,7,5,3,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,3,4,3,7,0,7,1,7,5,6,5,7,3,5,0,7,5,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,4,7,5,0,7,7,7,7,0,7,3,5,6,5,7,1,2,7,7,7,
	7,7,7,7,7,7,7,7,7,7,3,5,7,1,6,1,5,7,0,5,7,5,7,0,7,3,4,7,5,7,7,7,
	7,7,7,7,7,7,7,0,3,0,7,2,1,6,3,6,7,5,7,2,5,2,5,7,7,5,7,0,7,7,7,7,
	7,7,7,7,7,7,7,3,6,7,0,7,7,1,3,5,3,0,7,5,7,7,1,7,0,7,1,7,7,0,7,7,
	7,7,7,7,2,0,2,5,3,5,3,6,1,6,0,6,3,4,3,4,3,5,7,0,7,5,7,6,5,7,7,7,
	7,7,7,7,1,7,5,2,6,0,7,1,6,3,7,1,6,3,5,3,7,6,7,5,7,7,1,0,4,3,7,7,
	7,7,5,2,7,2,3,5,3,7,2,7,2,1,2,7,1,6,3,4,5,1,4,3,7,0,4,4,0,4,7,7,
	7,7,7,7,0,3,5,2,7,0,7,0,5,7,0,7,0,3,4,3,2,0,4,0,4,0,4,1,4,7,7,7,
	7,7,2,0,7,7,0,3,0,7,3,1,7,2,7,3,6,1,7,0,4,4,0,4,0,4,5,0,7,7,7,7,
	7,7,5,3,0,7,2,5,2,1,6,2,1,7,0,7,1,6,3,5,3,0,5,0,5,0,4,0,7,7,7,7,
	7,7,7,0,7,0,7,2,1,6,1,7,4,2,1,6,3,0,7,2,7,7,4,0,4,0,7,7,7,7,7,7,
	7,7,7,7,2,3,7,1,6,3,1,6,3,1,7,2,5,2,7,1,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,0,5,2,7,1,6,2,7,7,2,5,3,7,0,0,4,7,7,7,7,7,7,7,7,7,7,7,7,
	7,0,4,0,4,0,4,0,3,5,3,0,3,4,3,6,3,5,0,0,7,7,7,7,0,4,0,5,2,7,7,7,
	1,4,0,4,0,5,0,4,0,2,4,1,4,0,0,1,4,0,4,0,7,7,5,0,4,0,4,0,4,0,7,7,
	7,0,4,1,4,0,4,0,4,7,1,6,0,4,4,4,0,4,4,0,6,7,0,4,0,4,1,4,0,4,0,4,
	7,4,1,4,0,4,1,4,0,0,4,1,4,0,5,0,4,1,0,7,5,0,4,0,4,1,4,0,5,4,0,7,
	7,7,4,0,4,4,4,0,4,3,0,7,0,4,0,4,0,4,7,7,2,5,0,4,0,4,4,4,0,4,1,7,
	7,7,7,4,1,0,4,0,5,7,4,7,7,0,5,0,7,7,7,5,0,4,0,5,0,5,0,0,4,0,7,7,
	7,7,7,7,7,4,1,7,7,7,0,7,7,7,7,7,7,7,7,2,7,7,4,0,4,0,4,5,0,7,7,7,
	7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,0,5,7,7,7,5,0,4,0,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,4,6,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,4,0,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const unsigned char bmp2[32*32]={
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,2,3,2,3,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,3,2,3,0,3,1,2,3,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,3,2,3,0,3,2,2,3,2,3,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,3,7,3,0,3,7,3,0,3,7,7,7,7,7,6,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,3,1,3,2,7,7,7,7,2,3,2,3,7,4,4,4,4,4,6,7,7,7,7,7,
	7,7,7,7,7,7,7,7,2,3,2,0,7,7,7,3,1,0,3,0,4,4,4,6,4,4,4,7,7,7,7,7,
	7,7,7,7,7,7,7,7,3,0,3,3,7,7,7,3,2,3,3,6,4,6,4,4,4,6,4,4,7,7,7,7,
	7,7,7,7,7,7,7,7,7,3,2,3,3,0,3,7,3,2,2,4,4,4,6,4,4,4,6,4,6,7,7,7,
	7,7,7,7,7,7,7,7,7,2,3,7,2,3,0,2,3,3,1,6,4,4,4,4,6,4,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,1,2,3,0,3,3,3,2,4,4,7,6,4,6,4,4,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,2,3,2,3,7,7,7,7,7,4,4,4,4,6,4,7,7,7,7,7,
	7,7,7,7,7,7,3,2,3,2,3,3,7,5,5,7,7,7,7,7,7,7,4,6,4,4,6,7,7,7,7,7,
	7,7,7,7,7,3,2,3,2,3,2,2,3,6,7,7,7,7,7,7,7,7,7,0,6,4,4,7,7,7,7,7,
	7,7,7,7,3,2,3,3,2,3,2,3,2,3,7,7,7,7,7,7,7,7,7,7,4,4,6,7,7,7,7,7,
	7,7,7,7,2,3,7,3,2,3,7,3,2,3,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,3,2,2,2,7,7,7,7,2,3,2,3,7,7,7,5,1,1,5,1,7,7,7,7,7,7,7,7,7,
	7,7,7,7,3,3,2,3,7,7,3,2,3,2,7,7,7,5,1,5,5,1,5,1,5,7,7,7,7,7,7,7,
	7,7,7,3,2,3,2,7,7,7,7,2,3,2,3,7,5,1,5,1,1,5,1,5,1,5,7,7,7,7,7,7,
	7,7,7,7,2,3,3,3,2,3,3,3,2,3,7,7,1,5,7,7,5,5,7,5,1,5,7,7,7,7,7,7,
	7,7,7,7,3,2,3,2,3,2,2,3,3,2,5,5,1,1,5,7,7,7,7,1,5,1,5,7,7,7,7,7,
	7,7,7,7,7,3,2,3,2,3,3,2,3,7,7,1,5,5,5,5,7,7,5,5,5,1,7,7,7,7,7,7,
	7,7,7,7,7,4,0,2,3,2,2,2,4,6,7,7,5,1,1,7,7,7,7,1,1,1,5,7,7,7,7,7,
	7,7,7,7,6,4,7,4,4,4,4,4,4,4,7,5,1,5,7,5,5,5,7,5,5,5,7,7,7,7,7,7,
	7,7,7,7,7,7,6,4,6,4,4,6,4,4,4,7,5,1,5,1,5,1,1,5,1,5,7,7,7,7,7,7,
	7,7,7,7,7,7,4,4,4,6,4,4,4,6,4,7,7,5,1,5,1,5,5,1,5,7,7,7,7,7,7,7,
	7,7,7,7,7,7,6,4,6,4,4,6,7,4,4,7,7,7,7,5,1,5,1,5,6,7,7,7,7,7,7,7,
	7,7,7,7,7,7,4,4,4,4,0,7,7,7,7,7,7,7,7,7,4,4,6,6,4,4,7,7,7,7,7,7,
	7,7,7,7,7,7,6,4,6,4,6,7,7,7,7,7,7,7,7,7,7,4,4,4,6,7,7,7,7,7,7,7,
	7,7,7,7,7,7,6,4,4,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const unsigned char bmp3[32*32]={
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,6,6,2,7,7,6,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,6,6,7,3,6,6,7,7,2,6,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,6,6,0,6,6,6,6,0,6,6,7,7,6,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,6,7,0,6,6,6,2,6,2,6,6,6,2,4,6,2,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,6,6,6,2,6,6,6,6,6,0,6,6,2,6,6,6,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,3,6,6,2,6,2,0,2,0,2,0,2,6,6,2,6,1,6,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,5,2,6,6,2,0,6,0,6,0,6,0,6,2,6,6,6,6,7,7,7,7,7,7,7,
	7,7,7,7,7,7,6,6,6,2,6,0,6,0,2,1,2,0,6,0,6,6,2,6,3,7,7,7,7,7,7,7,
	7,7,7,7,7,7,2,6,6,6,0,3,0,6,0,6,2,5,2,0,2,6,6,0,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,6,2,6,2,4,2,2,0,6,0,2,0,6,0,2,6,6,6,0,7,7,7,7,7,7,
	7,7,7,7,7,7,4,2,6,6,0,0,6,0,7,0,2,0,6,2,0,6,6,6,6,7,7,7,7,7,7,7,
	7,7,7,7,7,2,6,6,6,2,0,6,0,2,0,2,4,2,0,5,2,6,2,2,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,2,4,6,2,0,7,0,6,2,0,7,2,0,6,6,6,6,2,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,2,6,6,2,0,2,2,4,2,0,4,2,2,2,6,6,6,6,7,7,7,7,7,7,
	7,7,7,7,7,7,6,6,6,6,6,2,6,0,4,3,4,2,2,6,6,6,6,1,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,2,5,2,6,6,0,6,2,0,2,4,6,2,6,6,6,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,6,6,2,6,6,6,6,6,2,6,6,2,6,2,6,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,6,7,6,2,6,2,6,6,2,6,7,7,5,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,2,4,7,1,6,6,1,6,6,0,7,6,0,5,2,0,0,1,6,7,7,7,7,7,
	7,7,7,7,7,7,4,0,4,0,0,4,2,4,7,7,2,7,5,0,4,0,4,4,6,4,0,7,7,7,7,7,
	7,7,7,7,7,7,7,0,1,4,0,4,0,0,6,7,0,0,0,4,0,4,0,0,0,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,4,0,4,0,5,4,0,5,6,4,0,0,0,5,0,5,4,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,1,4,0,6,0,6,0,1,7,4,1,6,0,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,2,7,0,7,7,0,6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,4,3,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const unsigned char bmp4[32*32]={
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,4,6,4,7,4,7,7,7,7,7,7,7,7,7,7,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,7,
	6,4,0,4,0,4,0,4,4,5,6,2,2,2,2,2,2,2,2,2,2,2,6,7,7,7,7,7,6,5,6,5,
	7,4,7,4,4,5,4,0,4,2,2,6,2,2,2,2,2,2,2,2,2,2,2,7,7,4,4,0,4,4,0,6,
	7,4,0,7,0,6,2,7,2,6,2,2,7,6,2,2,2,0,2,2,2,2,2,2,6,0,4,4,2,4,6,5,
	7,3,4,4,4,2,6,2,2,2,2,6,2,2,2,2,2,2,2,2,2,2,2,2,2,5,4,5,4,5,0,6,
	7,4,4,0,7,2,2,6,2,6,7,2,2,2,2,2,2,2,3,2,2,2,2,6,2,6,0,4,0,6,4,7,
	7,7,4,4,2,6,2,2,6,2,2,2,2,2,2,2,0,2,2,0,2,2,2,2,2,2,5,4,4,1,4,7,
	7,7,1,4,2,2,2,6,3,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,6,0,4,4,4,7,
	7,7,7,4,2,6,2,2,6,2,2,7,0,2,0,2,2,2,2,2,0,2,2,2,2,2,3,4,5,4,0,7,
	7,7,7,0,2,2,2,6,2,2,2,0,2,2,2,2,2,2,3,2,2,2,2,2,2,6,4,4,0,4,7,7,
	7,7,7,7,2,2,2,2,2,0,2,2,0,2,2,2,0,2,6,2,2,2,2,2,2,2,3,4,4,0,7,7,
	7,7,7,7,2,2,7,2,2,2,2,2,2,2,0,2,2,2,2,2,7,2,2,2,2,2,0,0,4,5,7,7,
	7,7,7,7,2,2,2,2,2,2,2,2,2,0,2,2,0,2,2,2,2,2,0,2,2,0,2,4,5,7,7,7,
	7,7,7,7,2,2,2,2,2,2,2,2,3,6,2,2,6,2,2,2,6,2,2,2,0,2,4,5,7,7,7,7,
	7,7,7,7,2,2,2,2,2,2,2,2,2,2,6,2,2,2,6,2,3,2,2,2,2,5,7,7,7,7,7,7,
	7,7,7,7,0,2,2,2,2,2,2,2,2,2,2,2,6,2,2,2,2,2,2,0,2,7,7,7,7,7,7,7,
	7,7,7,5,2,2,0,2,2,2,6,2,6,2,2,6,2,2,7,2,2,2,2,2,2,7,7,7,7,7,7,7,
	7,7,7,4,4,4,2,2,2,2,2,2,2,2,6,2,2,6,2,0,2,2,0,2,7,7,7,7,7,7,7,7,
	7,7,7,0,4,5,2,2,2,2,2,2,2,2,2,2,6,2,2,2,2,2,2,2,2,7,7,7,7,7,7,7,
	7,7,7,4,0,4,5,0,2,0,2,2,2,2,6,3,2,2,2,2,2,2,2,2,7,7,7,7,7,7,7,7,
	7,7,4,0,5,4,4,2,2,2,2,0,2,2,2,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,7,
	7,7,4,6,4,0,4,5,0,2,4,2,0,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,5,0,4,5,0,4,4,5,4,5,4,5,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,4,7,4,6,4,4,4,0,4,0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,6,4,0,4,0,5,0,4,5,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,4,0,6,5,6,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,6,5,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

const unsigned char bmp5[32*32]={
	7,7,7,7,7,7,7,7,7,7,7,7,7,2,2,2,2,2,3,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,2,2,7,2,2,2,2,3,3,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,2,2,2,2,2,2,2,3,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,6,2,2,2,6,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,6,2,2,2,2,6,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,6,2,2,2,6,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,6,2,2,2,2,6,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,2,6,2,2,6,2,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,2,6,2,2,2,2,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,2,2,2,2,2,2,6,2,2,2,2,2,2,6,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,6,2,2,2,2,2,2,6,2,2,2,2,2,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,2,2,2,2,2,2,2,2,2,2,2,6,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,2,2,2,2,2,2,2,2,2,2,7,7,7,4,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,2,2,2,2,2,2,2,2,7,7,7,7,0,7,7,7,7,7,7,7,
	7,7,7,7,7,7,5,4,7,7,7,7,7,7,6,0,2,7,7,7,7,7,7,4,4,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,4,7,7,7,7,7,7,7,4,4,7,7,7,7,7,4,4,4,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,4,4,5,7,7,7,7,6,4,6,7,7,7,7,4,4,0,5,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,4,4,4,4,7,7,7,7,4,4,7,7,7,4,4,4,4,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,5,4,4,4,4,7,7,7,4,4,7,7,4,0,4,4,4,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,4,4,4,4,4,4,6,4,6,7,4,4,4,4,0,4,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,4,4,6,4,4,4,4,4,0,4,4,6,4,0,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,4,4,4,4,4,4,4,4,4,4,4,4,4,4,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,4,4,4,6,4,6,4,0,4,4,6,4,4,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,4,4,4,4,4,4,4,4,6,4,4,0,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,4,4,4,6,4,4,4,4,4,4,4,4,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,4,4,4,4,4,4,0,6,4,4,4,4,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,4,4,6,4,4,4,4,4,4,4,0,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,4,4,4,4,0,6,4,4,4,4,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,5,4,4,4,4,4,4,4,0,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,4,4,0,4,4,4,4,5,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,4,4,0,4,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};

// 14x13ドットキャラクターデータ
const unsigned char bmp1413_1[14*13]={
	0,0,0,0,0,2,2,2,2,0,0,0,0,0,
	0,0,0,2,2,2,2,2,2,2,2,0,0,0,
	0,0,2,2,2,2,2,2,2,2,2,2,0,0,
	0,2,7,7,2,2,2,2,7,7,2,2,2,0,
	0,7,7,7,7,2,2,7,7,7,7,2,2,0,
	0,8,8,7,7,2,2,8,8,7,7,2,2,0,
	2,8,8,7,7,2,2,8,8,7,7,2,2,2,
	2,2,7,7,2,2,2,2,7,7,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,0,2,2,2,2,0,2,2,2,2,
	0,2,2,0,0,0,2,2,0,0,0,2,2,0
};
const unsigned char bmp1413_2[14*13]={
	0,0,0,0,0,5,5,5,5,0,0,0,0,0,
	0,0,0,5,5,5,5,5,5,5,5,0,0,0,
	0,0,5,5,5,5,5,5,5,5,5,5,0,0,
	0,5,7,7,5,5,5,5,7,7,5,5,5,0,
	0,7,7,7,7,5,5,7,7,7,7,5,5,0,
	0,8,8,7,7,5,5,8,8,7,7,5,5,0,
	5,8,8,7,7,5,5,8,8,7,7,5,5,5,
	5,5,7,7,5,5,5,5,7,7,5,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	5,5,0,5,5,5,0,0,5,5,5,0,5,5,
	5,0,0,0,5,5,0,0,5,5,0,0,0,5
};

extern
int32_t ffx1,fnx1,ffy1,fny1;
extern
int32_t ffx2,fnx2,ffy2,fny2;

float angle ;
float fwx = 0;
float fwy = 0;

float far = 0.03f;
float near = 0.005f;
float fFovHalf = 3.14159265358979323/8;

void setvector(int s){
  float s1,c1,s2,c2,t=angle;
  c1 = cosf(t-fFovHalf)*256;
  c2 = cosf(t+fFovHalf)*256;
  s1 = sinf(t-fFovHalf)*256;
  s2 = sinf(t+fFovHalf)*256;

  ffx1 = fwx+c1*far;
  fnx1 = fwx+c1*near;
  
  ffy1 = fwy+s1*far;
  fny1 = fwy+s1*near;
  
  ffx2 = fwx+c2*far;
  fnx2 = fwx+c2*near;
  
  ffy2 = fwy+s2*far;
  fny2 = fwy+s2*near;  
}


int main(void){
	int i,j,k;
	unsigned int n;
	unsigned char *ad1,*ad2,tempbuf[VRAM_X];
	int x1,y1,dx1,dy1,old_x1,old_y1;
	int x2,y2,dx2,dy2,old_x2,old_y2;
	int scene;

	/* ポートの初期設定 */
	TRISB = 0x0000;						// 全て出力
	TRISC = 0x0000;						// 全て出力
	TRISD = 0x001F;						// キー入力
	TRISE = 0x0000;						// 全て出力
	TRISF = 0x0000;						// 全て出力
	TRISG = 0x0000;						// 全て出力

	ANSELD = 0x0000; // 全てデジタル
	CNPUDSET=KEYSTART | KEYFIRE | KEYUP | KEYDOWN | KEYLEFT | KEYRIGHT;// プルアップ設定


	init_composite(); // ビデオメモリクリア、割り込み初期化、カラービデオ出力開始

	//上部にアルファベット表示
	k=0;
	for(i=0;i<=24;i+=8){
		for(j=0;j<=VRAM_X-8;j+=8){
			putfont(j,i,k%16,k%26+'A');
			k++;
		}
	}

	//カラーパターン
	for(i=42;i<54;i++){
		for(j=0;j<256;j++){
			pset(j,i,(256-1-j)/32);
		}
	}
	for(;i<65;i++){
		for(j=0;j<256;j++){
			pset(j,i,(256-1-j)/32+8);
		}
	}

	//32x32キャラクター
	putbmpmn(0,68,32,32,bmp1);
	putbmpmn(32,68,32,32,bmp2);
	putbmpmn(64,68,32,32,bmp3);
	putbmpmn(96,68,32,32,bmp4);
	putbmpmn(128,68,32,32,bmp5);
	putbmpmn(160,68,32,32,bmp1);
	putbmpmn(192,68,32,32,bmp2);
	putbmpmn(224,68,32,32,bmp3);

	//直線
	for(i=0;i<=120;i+=8){
		line(i,102,0,222-i,4);
	}

	//円
	for(i=1;i<16;i++){
		circle(70,160,i*4,i);
	}

	//
	line(134,102,VRAM_X-1,102,7);
	line(134,102,134,VRAM_Y-1,7);
	line(VRAM_X-1,102,VRAM_X-1,VRAM_Y-1,7);
	line(134,VRAM_Y-1,VRAM_X-1,VRAM_Y-1,7);

	//移動キャラクターの設定
	x1=135;
	y1=120;
	old_x1=x1;
	old_y1=y1;
	dx1=1;
	dy1=1;

	x2=140;
	y2=150;
	old_x2=x2;
	old_y2=y2;
	dx2=1;
	dy2=0;
	drawcount=0;
	n=0;
	k=0;
	scene=0;
	while(1){
		//映像区間終了待ち（約60分の1秒のウェイト）
		while(drawcount==0) asm("wait");
		drawcount=0;

		//アルファベット部分スクロール
		ad1=VRAM;
		ad2=tempbuf;
		for(i=0;i<VRAM_X;i++) *ad2++=*ad1++;
		ad2=VRAM;
		for(i=0;i<31;i++){
			for(j=0;j<VRAM_X;j++){
				*ad2++=*ad1++;
			}
		}
		ad1=tempbuf;
		for(i=0;i<VRAM_X;i++) *ad2++=*ad1++;

	//カウンタ表示
		if(++k==6){
			k=0;
			printnum(0,32,7,n++);
			if(n==0) printstr(0,32,7,"          ");
		}

	//キャラクターの移動
		clrbmpmn(old_x1,old_y1,14,13);	//移動前のキャラクターの消去
		clrbmpmn(old_x2,old_y2,14,13);	//移動前のキャラクターの消去

		putbmpmn(x1,y1,14,13,bmp1413_1);	//移動後のキャラクターの表示
		putbmpmn(x2,y2,14,13,bmp1413_2);	//移動後のキャラクターの表示

		old_x1=x1;
		old_y1=y1;
		x1+=dx1;
		y1+=dy1;
		if((x1+dx1<135) || (x1+dx1>VRAM_X-1-14)) dx1=-dx1;
		if((y1+dy1<103) || (y1+dy1>VRAM_Y-1-13)) dy1=-dy1;

		old_x2=x2;
		old_y2=y2;
		x2+=dx2;
		y2+=dy2;
		if((x2+dx2<135) || (x2+dx2>VRAM_X-1-14)) dx2=-dx2;
		if((y2+dy2<103) || (y2+dy2>VRAM_Y-1-13)) dy2=-dy2;

		//画面のスクロール、拡大、回転設定
		setvector(scene);
		scene++;
		if(scene==2845) scene=0;
		
		uint32_t keystatus=~KEYPORT & (KEYUP | KEYDOWN | KEYLEFT | KEYRIGHT | KEYSTART | KEYFIRE);
		if(keystatus&KEYUP){
		  fwx+=cosf(angle)*500;
		  fwy+=sinf(angle)*500;
		}
		if(keystatus&KEYDOWN){
		  fwx-=cosf(angle)*500;
		  fwy-=sinf(angle)*500;
		}
		if(keystatus&KEYFIRE)
		  far+=1.f;
		if(keystatus&KEYSTART)
		  far -= 1.f;
		if(keystatus&KEYRIGHT)
		  angle+=0.05f;
		if(keystatus&KEYLEFT)
		  angle -= 0.05f;
		
	}
}
