// ベクトル式VRAM　コンポジットカラー信号出力プログラム　PIC32MX370F512H用　by K.Tanaka Rev. 1.1
// 出力 PORTE（RE0-4）
// VRAM解像度256×256ドット
// 画面出力解像度256×224ドット
// 256色同時表示、1バイトで1ドットを表す
// カラーパレット対応
// クロック周波数3.579545MHz×4×20/3倍
// 1ドットがカラーサブキャリアの5分の3周期（16クロック）
// 1ドットあたり4回信号出力（カラーサブキャリア1周期あたり3分の20回出力）

// (vstartx,vstarty):画面左上になるVRAM上の座標（256倍）
// (vscanv1_x,vscanv1_y):画面右方向のスキャンベクトル（256倍）
// (vscanv2_x,vscanv2_y):画面下方向のスキャンベクトル（256倍）


#include "rotatevideo.h"
#include <plib.h>
#include <stdint.h>
//カラー信号出力データ
//
#define C_SYN	0
#define C_BLK	7

#define C_BST1	7
#define C_BST2	5
#define C_BST3	4
#define C_BST4	6
#define C_BST5	10
#define C_BST6	11
#define C_BST7	10
#define C_BST8	6
#define C_BST9	4
#define C_BST10	5
#define C_BST11	7
#define C_BST12	10
#define C_BST13	11
#define C_BST14	9
#define C_BST15	5
#define C_BST16	4
#define C_BST17	5
#define C_BST18	9
#define C_BST19	11
#define C_BST20	10

// パルス幅定数
#define V_NTSC		262				// 262本/画面
#define V_SYNC		10				// 垂直同期本数
#define V_PREEQ		18				// ブランキング区間上側
#define V_LINE		Y_RES				// 画像描画区間
#define H_NTSC		6080				// 1ラインのクロック数（約63.5→63.7μsec）（色副搬送波の228周期）
#define H_SYNC		449				// 水平同期幅、約4.7μsec
//#define H_BACK	858				// 左スペース（水平同期立ち上がりから）（未使用）

#define nop()	asm volatile("nop")
#define nop5()	nop();nop();nop();nop();nop();
#define nop10()	nop5();nop5();

// グローバル変数定義
unsigned char VRAM[VRAM_X*VRAM_Y] __attribute__ ((aligned (4))); //VRAM
volatile short vscanv1_x,vscanv1_y,vscanv2_x,vscanv2_y;	//映像表示スキャン用ベクトル
volatile short vscanstartx,vscanstarty; //映像表示スキャン開始座標
volatile short vscanx,vscany; //映像表示スキャン処理中座標

volatile unsigned short LineCount;	// 処理中の行
volatile unsigned short drawcount;	//　1画面表示終了ごとに1足す。アプリ側で0にする。
					// 最低1回は画面表示したことのチェックと、アプリの処理が何画面期間必要かの確認に利用。
volatile char drawing;		//　映像区間処理中は-1、その他は0


//カラー信号波形テーブル
//256色分のカラーパレット
//20分の3周期単位で3周期分
unsigned char ClTable[20*256] __attribute__ ((aligned (4)));

int32_t ffx1,fnx1,ffy1,fny1;
int32_t ffx2,fnx2,ffy2,fny2;

/**********************
*  Timer2 割り込み処理関数
***********************/
void __ISR(8, ipl5) T2Handler(void)
{
	asm volatile("#":::"a0");
	asm volatile("#":::"v0");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("addiu	$a0,$a0,-23");
	asm volatile("bltz	$a0,label1_2");
	asm volatile("addiu	$v0,$a0,-18");
	asm volatile("bgtz	$v0,label1_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label1");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");
asm volatile("label1:");
	nop10();nop10();nop();nop();

asm volatile("label1_2:");
	//LATE=C_SYN;
	asm volatile("addiu	$a0,$zero,%0"::"n"(C_SYN));
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("sb	$a0,0($v0)");// 同期信号立ち下がり。ここを基準に全ての信号出力のタイミングを調整する

	if(LineCount<V_SYNC){
		// 垂直同期期間
		OC3R = H_NTSC-H_SYNC-1;	// 切り込みパルス幅設定
		OC3CON = 0x8001;
	}
	else{
		OC1R = H_SYNC-1-8;		// 同期パルス幅4.7usec
		OC1CON = 0x8001;		// タイマ2選択ワンショット
		if(LineCount>=V_SYNC+V_PREEQ && LineCount<V_SYNC+V_PREEQ+V_LINE){
			drawing=-1; // 画像描画区間
			int32_t depth = 256*250/(LineCount+100);
			int32_t sx,sy,ex,ey;
			sx = (((ffx1-fnx1)*depth)/256)+fnx1;
			sy = (((ffy1-fny1)*depth)/256)+fny1;
			ex = (((ffx2-fnx2)*depth)/256)+fnx2;
			ey = (((ffy2-fny2)*depth)/256)+fny2;
			int32_t dx,dy;
			vscanx = sx;
			vscany = sy;
			vscanv1_x = (ex - sx)/256;
			vscanv1_y = (ey - sy)/256;
		}
		else{
			/* vscanx=vscanstartx; */
			/* vscany=vscanstarty; */
		}
	}
	LineCount++;
	if(LineCount>=V_NTSC) LineCount=0;
	mT2ClearIntFlag();			// T2割り込みフラグクリア
}

/*********************
*  OC3割り込み処理関数 垂直同期切り込みパルス
*********************/
void __ISR(14, ipl5) OC3Handler(void)
{
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(H_NTSC-H_SYNC+23)));
	asm volatile("bltz	$a0,label4_2");
	asm volatile("addiu	$v0,$a0,-18");
	asm volatile("bgtz	$v0,label4_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label4");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label4:");
	nop10();nop10();nop();nop();

asm volatile("label4_2:");
	// 同期信号のリセット
	//	LATE=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("sb	$v1,0($v0)");	// 同期信号リセット。同期信号立ち下がりから5631サイクル

	mOC3ClearIntFlag();			// OC3割り込みフラグクリア
}

/*********************
*  OC1割り込み処理関数 水平同期立ち上がり?カラーバースト?映像信号
*********************/
void __ISR(6, ipl5) OC1Handler(void)
{
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");
	asm volatile("#":::"a1");
	asm volatile("#":::"a2");
	asm volatile("#":::"a3");
	asm volatile("#":::"t0");
	asm volatile("#":::"t1");
	asm volatile("#":::"t2");
	asm volatile("#":::"t3");
	asm volatile("#":::"t4");

	//TMR2の値でタイミングのずれを補正
	asm volatile("la	$v0,%0"::"i"(&TMR2));
	asm volatile("lhu	$a0,0($v0)");
	asm volatile("addiu	$a0,$a0,%0"::"n"(-(H_SYNC+23)));
	asm volatile("bltz	$a0,label2_2");
	asm volatile("addiu	$v0,$a0,-18");
	asm volatile("bgtz	$v0,label2_2");
	asm volatile("sll	$a0,$a0,2");
	asm volatile("la	$v0,label2");
	asm volatile("addu	$a0,$v0");
	asm volatile("jr	$a0");

asm volatile("label2:");
	nop10();nop10();nop();nop();

asm volatile("label2_2:");
	// 同期信号のリセット
	//	LATE=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("sb	$v1,0($v0)");	// 同期信号リセット。水平同期立ち下がりから449サイクル

	// 54クロックウェイト
	nop10();nop10();nop10();nop10();nop10();nop();nop();nop();nop();

	// カラーバースト信号 9周期出力
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BST1));

	asm volatile("sb	$v1,0($v0)");	// カラーバースト開始。水平同期立ち下がりから507サイクル
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BST2));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST3));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST4));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST5));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST6));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST7));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST8));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST9));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST10));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST11));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST12));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST13));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST14));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST15));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST16));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST17));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST18));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST19));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST20));nop();nop();

	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST1));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST2));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST3));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST4));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST5));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST6));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST7));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST8));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST9));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST10));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST11));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST12));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST13));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST14));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST15));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST16));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST17));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST18));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST19));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST20));nop();nop();

	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST1));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST2));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST3));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST4));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST5));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST6));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST7));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST8));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST9));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST10));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST11));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST12));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST13));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST14));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST15));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST16));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST17));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST18));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST19));nop();nop();
	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST20));nop();nop();

	asm volatile("sb $v1,0($v0)");asm volatile("addiu $v1,$zero,%0"::"n"(C_BST1));nop();nop();
	asm volatile("sb	$v1,0($v0)");	// カラーバースト終了。水平同期立ち下がりから747サイクル


	//	if(drawing==0) goto label3;  //映像期間でなければ終了
	asm volatile("la	$v0,%0"::"i"(&drawing));
	asm volatile("lb	$t1,0($v0)");
	asm volatile("beqz	$t1,label3");
	nop();
	// HI,LOレジスタの退避
	asm volatile("mfhi	$t1");
	asm volatile("mflo	$v0");
	asm volatile("addiu	$sp,$sp,-8");
	asm volatile("sw	$t1,4($sp)");
	asm volatile("sw	$v0,8($sp)");

	// 521クロックウェイト
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop();


// 映像信号出力

	//	a0=VRAM;
	asm volatile("la	$a0,%0"::"i"(VRAM));
	//	a1=ClTable;
	asm volatile("la	$a1,%0"::"i"(ClTable));
	//	a2=&LATE;
	asm volatile("la	$a2,%0"::"i"(&LATE));
	//	a3=20;
	asm volatile("addiu	$a3,$zero,20");
	//	t0=vscanv1_x;
	asm volatile("la	$v0,%0"::"i"(&vscanv1_x));
	asm volatile("lhu	$t0,0($v0)");
	//	t1=vscanv1_y;
	asm volatile("la	$v0,%0"::"i"(&vscanv1_y));
	asm volatile("lhu	$t1,0($v0)");
	//	t2=vscanx;
	asm volatile("la	$v0,%0"::"i"(&vscanx));
	asm volatile("lhu	$t2,0($v0)");
	//	t3=vscanx;
	asm volatile("la	$v0,%0"::"i"(&vscany));
	asm volatile("lhu	$t3,0($v0)");

	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");

//----------------------------------------------------------------------

	asm volatile("swl	$t4,3($a2)");// 最初の映像信号出力。水平同期立下りから1307サイクル
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,4($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,8($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,12($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,16($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,0($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,4($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,8($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,12($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");
	asm volatile("addu	$v1,$v1,$a1");
	asm volatile("lw	$t4,16($v1)");
	asm volatile("addu	$t3,$t3,$t1");
	asm volatile("swl	$t4,3($a2)");
		asm volatile("ext	$v0,$t2,8,8");
		asm volatile("rotr	$v1,$t3,8");
		asm volatile("ins	$v0,$v1,8,8");
	asm volatile("swl	$t4,2($a2)"); //2 clock operation
		asm volatile("addu	$v0,$v0,$a0");
		asm volatile("lbu	$v1,0($v0)");
	asm volatile("swl	$t4,1($a2)");
		asm volatile("mul	$v1,$v1,$a3");
		asm volatile("addu	$t2,$t2,$t0");
	asm volatile("swl	$t4,0($a2)");
		asm volatile("addu	$v1,$v1,$a1");
		asm volatile("lw	$t4,0($v1)");
		asm volatile("addu	$t3,$t3,$t1");
		asm volatile("swl	$t4,3($a2)");
	asm volatile("ext	$v0,$t2,8,8");
	asm volatile("rotr	$v1,$t3,8");
	asm volatile("ins	$v0,$v1,8,8");
		asm volatile("swl	$t4,2($a2)"); //2 clock operation
	asm volatile("addu	$v0,$v0,$a0");
	asm volatile("lbu	$v1,0($v0)");
		asm volatile("swl	$t4,1($a2)");
	asm volatile("mul	$v1,$v1,$a3");
	asm volatile("addu	$t2,$t2,$t0");
		asm volatile("swl	$t4,0($a2)");



//----------------------------------------------------------------------


	nop();nop();

	//	LATE=C_BLK;
	asm volatile("addiu	$t4,$zero,%0"::"n"(C_BLK));
	asm volatile("sb	$t4,0($a2)");

	// HI,LOレジスタの復帰
	asm volatile("lw	$v0,8($sp)");
	asm volatile("lw	$t1,4($sp)");
	asm volatile("addiu	$sp,$sp,8");
	asm volatile("mtlo	$v0");
	asm volatile("mthi	$t1");

	vscanx+=vscanv2_x; // 次の行へ
	vscany+=vscanv2_y;
	if(LineCount==V_SYNC+V_PREEQ+V_LINE){ // 1画面最後の描画終了
			drawing=0;
			drawcount++;
	}
asm volatile("label3:");
	mOC1ClearIntFlag();			// OC1割り込みフラグクリア
}

// 画面クリア
void clearscreen(void)
{
	unsigned int *vp;
	int i;
	vp=(unsigned int *)VRAM;
	for(i=0;i<VRAM_X*VRAM_Y/4;i++) *vp++=0;
}

void set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g)
{
	// カラーパレット設定（5ビットDA、電源3.3V、1周期を5分割）
	// n:パレット番号0?255、r,g,b:0?255
	// 輝度Y=0.587*G+0.114*B+0.299*R
	// 信号N=Y+0.4921*(B-Y)*sinθ+0.8773*(R-Y)*cosθ
	// 出力データS=(N*0.71[v]+0.29[v])/3.3[v]*64*1.3

	int y;
	y=(150*g+29*b+77*r+128)/256;

	ClTable[n*20+ 0]=(4582*y+   0*((int)b-y)+4020*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*0/20
	ClTable[n*20+ 1]=(4582*y+1824*((int)b-y)+2363*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*1/20
	ClTable[n*20+ 2]=(4582*y+2145*((int)b-y)-1242*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*2/20
	ClTable[n*20+ 3]=(4582*y+ 697*((int)b-y)-3824*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*3/20
	ClTable[n*20+ 4]=(4582*y-1325*((int)b-y)-3252*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*4/20
	ClTable[n*20+ 5]=(4582*y-2255*((int)b-y)+   0*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*5/20
	ClTable[n*20+ 6]=(4582*y-1326*((int)b-y)+3252*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*6/20
	ClTable[n*20+ 7]=(4582*y+ 697*((int)b-y)+3824*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*7/20
	ClTable[n*20+ 8]=(4582*y+2145*((int)b-y)+1242*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*8/20
	ClTable[n*20+ 9]=(4582*y+1824*((int)b-y)-2363*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*9/20
	ClTable[n*20+10]=(4582*y+   0*((int)b-y)-4020*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*10/20
	ClTable[n*20+11]=(4582*y-1824*((int)b-y)-2363*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*11/20
	ClTable[n*20+12]=(4582*y-2145*((int)b-y)+1242*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*12/20
	ClTable[n*20+13]=(4582*y- 697*((int)b-y)+3823*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*13/20
	ClTable[n*20+14]=(4582*y+1325*((int)b-y)+3252*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*14/20
	ClTable[n*20+15]=(4582*y+2255*((int)b-y)+   0*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*15/20
	ClTable[n*20+16]=(4582*y+1326*((int)b-y)-3252*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*16/20
	ClTable[n*20+17]=(4582*y- 697*((int)b-y)-3824*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*17/20
	ClTable[n*20+18]=(4582*y-2145*((int)b-y)-1242*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*18/20
	ClTable[n*20+19]=(4582*y-1824*((int)b-y)+2363*((int)r-y)+1872*256+32768)/65536;//θ=2Π*3*19/20

}

void start_composite(void)
{
	// 変数初期設定
	LineCount=0;				// 処理中ラインカウンター
	drawing=0;

	PR2 = H_NTSC -1; 			// 約63.5usecに設定
	T2CONSET=0x8000;			// タイマ2スタート
}
void stop_composite(void)
{
	T2CONCLR = 0x8000;			// タイマ2停止
}

// カラーコンポジット出力初期化
void init_composite(void)
{
	unsigned int i;
	clearscreen();
	//カラーパレット初期化
	for(i=0;i<8;i++){
		set_palette(i,255*(i&1),255*((i>>1)&1),255*(i>>2));
	}
	for(i=0;i<8;i++){
		set_palette(i+8,128*(i&1),128*((i>>1)&1),128*(i>>2));
	}
	for(i=16;i<256;i++){
		set_palette(i,255,255,255);
	}
	//VRAMスキャンベクトル初期化
	vscanv1_x=256;
	vscanv1_y=0;
	vscanv2_x=0;
	vscanv2_y=256;
	vscanstartx=0;
	vscanstarty=0;

	// タイマ2の初期設定,内部クロックで63.5usec周期、1:1
	T2CON = 0x0000;				// タイマ2停止状態
	mT2SetIntPriority(5);			// 割り込みレベル5
	mT2ClearIntFlag();
	mT2IntEnable(1);			// タイマ2割り込み有効化

	// OC1の割り込み有効化
	mOC1SetIntPriority(5);			// 割り込みレベル5
	mOC1ClearIntFlag();
	mOC1IntEnable(1);			// OC1割り込み有効化

	// OC3の割り込み有効化
	mOC3SetIntPriority(5);			// 割り込みレベル5
	mOC3ClearIntFlag();
	mOC3IntEnable(1);			// OC3割り込み有効化

	OSCCONCLR=0x10; // WAIT命令はアイドルモード
	INTEnableSystemMultiVectoredInt();
	SYSTEMConfig(95000000,SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE); //キャッシュ有効化（周辺クロックには適用しない）
	BMXCONCLR=0x40;	// RAMアクセスウェイト0
	start_composite();
}
