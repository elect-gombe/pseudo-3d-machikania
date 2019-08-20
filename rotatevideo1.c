// �x�N�g����VRAM�@�R���|�W�b�g�J���[�M���o�̓v���O�����@PIC32MX370F512H�p�@by K.Tanaka Rev. 1.1
// �o�� PORTE�iRE0-4�j
// VRAM�𑜓x256�~256�h�b�g
// ��ʏo�͉𑜓x256�~224�h�b�g
// 256�F�����\���A1�o�C�g��1�h�b�g��\��
// �J���[�p���b�g�Ή�
// �N���b�N���g��3.579545MHz�~4�~20/3�{
// 1�h�b�g���J���[�T�u�L�����A��5����3�����i16�N���b�N�j
// 1�h�b�g������4��M���o�́i�J���[�T�u�L�����A1����������3����20��o�́j

// (vstartx,vstarty):��ʍ���ɂȂ�VRAM��̍��W�i256�{�j
// (vscanv1_x,vscanv1_y):��ʉE�����̃X�L�����x�N�g���i256�{�j
// (vscanv2_x,vscanv2_y):��ʉ������̃X�L�����x�N�g���i256�{�j


#include "rotatevideo.h"
#include <plib.h>
#include <stdint.h>
//�J���[�M���o�̓f�[�^
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

// �p���X���萔
#define V_NTSC		262				// 262�{/���
#define V_SYNC		10				// ���������{��
#define V_PREEQ		18				// �u�����L���O��ԏ㑤
#define V_LINE		Y_RES				// �摜�`����
#define H_NTSC		6080				// 1���C���̃N���b�N���i��63.5��63.7��sec�j�i�F�������g��228�����j
#define H_SYNC		449				// �����������A��4.7��sec
//#define H_BACK	858				// ���X�y�[�X�i�������������オ�肩��j�i���g�p�j

#define nop()	asm volatile("nop")
#define nop5()	nop();nop();nop();nop();nop();
#define nop10()	nop5();nop5();

// �O���[�o���ϐ���`
unsigned char VRAM[VRAM_X*VRAM_Y] __attribute__ ((aligned (4))); //VRAM
volatile short vscanv1_x,vscanv1_y,vscanv2_x,vscanv2_y;	//�f���\���X�L�����p�x�N�g��
volatile short vscanstartx,vscanstarty; //�f���\���X�L�����J�n���W
volatile short vscanx,vscany; //�f���\���X�L�������������W

volatile unsigned short LineCount;	// �������̍s
volatile unsigned short drawcount;	//�@1��ʕ\���I�����Ƃ�1�����B�A�v������0�ɂ���B
					// �Œ�1��͉�ʕ\���������Ƃ̃`�F�b�N�ƁA�A�v���̏���������ʊ��ԕK�v���̊m�F�ɗ��p�B
volatile char drawing;		//�@�f����ԏ�������-1�A���̑���0


//�J���[�M���g�`�e�[�u��
//256�F���̃J���[�p���b�g
//20����3�����P�ʂ�3������
unsigned char ClTable[20*256] __attribute__ ((aligned (4)));

int32_t ffx1,fnx1,ffy1,fny1;
int32_t ffx2,fnx2,ffy2,fny2;

/**********************
*  Timer2 ���荞�ݏ����֐�
***********************/
void __ISR(8, ipl5) T2Handler(void)
{
	asm volatile("#":::"a0");
	asm volatile("#":::"v0");

	//TMR2�̒l�Ń^�C�~���O�̂����␳
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
	asm volatile("sb	$a0,0($v0)");// �����M������������B��������ɑS�Ă̐M���o�͂̃^�C�~���O�𒲐�����

	if(LineCount<V_SYNC){
		// ������������
		OC3R = H_NTSC-H_SYNC-1;	// �؂荞�݃p���X���ݒ�
		OC3CON = 0x8001;
	}
	else{
		OC1R = H_SYNC-1-8;		// �����p���X��4.7usec
		OC1CON = 0x8001;		// �^�C�}2�I�������V���b�g
		if(LineCount>=V_SYNC+V_PREEQ && LineCount<V_SYNC+V_PREEQ+V_LINE){
			drawing=-1; // �摜�`����
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
	mT2ClearIntFlag();			// T2���荞�݃t���O�N���A
}

/*********************
*  OC3���荞�ݏ����֐� ���������؂荞�݃p���X
*********************/
void __ISR(14, ipl5) OC3Handler(void)
{
	asm volatile("#":::"v0");
	asm volatile("#":::"v1");
	asm volatile("#":::"a0");

	//TMR2�̒l�Ń^�C�~���O�̂����␳
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
	// �����M���̃��Z�b�g
	//	LATE=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("sb	$v1,0($v0)");	// �����M�����Z�b�g�B�����M�����������肩��5631�T�C�N��

	mOC3ClearIntFlag();			// OC3���荞�݃t���O�N���A
}

/*********************
*  OC1���荞�ݏ����֐� �������������オ��?�J���[�o�[�X�g?�f���M��
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

	//TMR2�̒l�Ń^�C�~���O�̂����␳
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
	// �����M���̃��Z�b�g
	//	LATE=C_BLK;
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BLK));
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("sb	$v1,0($v0)");	// �����M�����Z�b�g�B�����������������肩��449�T�C�N��

	// 54�N���b�N�E�F�C�g
	nop10();nop10();nop10();nop10();nop10();nop();nop();nop();nop();

	// �J���[�o�[�X�g�M�� 9�����o��
	asm volatile("la	$v0,%0"::"i"(&LATE));
	asm volatile("addiu	$v1,$zero,%0"::"n"(C_BST1));

	asm volatile("sb	$v1,0($v0)");	// �J���[�o�[�X�g�J�n�B�����������������肩��507�T�C�N��
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
	asm volatile("sb	$v1,0($v0)");	// �J���[�o�[�X�g�I���B�����������������肩��747�T�C�N��


	//	if(drawing==0) goto label3;  //�f�����ԂłȂ���ΏI��
	asm volatile("la	$v0,%0"::"i"(&drawing));
	asm volatile("lb	$t1,0($v0)");
	asm volatile("beqz	$t1,label3");
	nop();
	// HI,LO���W�X�^�̑ޔ�
	asm volatile("mfhi	$t1");
	asm volatile("mflo	$v0");
	asm volatile("addiu	$sp,$sp,-8");
	asm volatile("sw	$t1,4($sp)");
	asm volatile("sw	$v0,8($sp)");

	// 521�N���b�N�E�F�C�g
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();nop10();
	nop10();nop10();nop();


// �f���M���o��

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

	asm volatile("swl	$t4,3($a2)");// �ŏ��̉f���M���o�́B�������������肩��1307�T�C�N��
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

	// HI,LO���W�X�^�̕��A
	asm volatile("lw	$v0,8($sp)");
	asm volatile("lw	$t1,4($sp)");
	asm volatile("addiu	$sp,$sp,8");
	asm volatile("mtlo	$v0");
	asm volatile("mthi	$t1");

	vscanx+=vscanv2_x; // ���̍s��
	vscany+=vscanv2_y;
	if(LineCount==V_SYNC+V_PREEQ+V_LINE){ // 1��ʍŌ�̕`��I��
			drawing=0;
			drawcount++;
	}
asm volatile("label3:");
	mOC1ClearIntFlag();			// OC1���荞�݃t���O�N���A
}

// ��ʃN���A
void clearscreen(void)
{
	unsigned int *vp;
	int i;
	vp=(unsigned int *)VRAM;
	for(i=0;i<VRAM_X*VRAM_Y/4;i++) *vp++=0;
}

void set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g)
{
	// �J���[�p���b�g�ݒ�i5�r�b�gDA�A�d��3.3V�A1������5�����j
	// n:�p���b�g�ԍ�0?255�Ar,g,b:0?255
	// �P�xY=0.587*G+0.114*B+0.299*R
	// �M��N=Y+0.4921*(B-Y)*sin��+0.8773*(R-Y)*cos��
	// �o�̓f�[�^S=(N*0.71[v]+0.29[v])/3.3[v]*64*1.3

	int y;
	y=(150*g+29*b+77*r+128)/256;

	ClTable[n*20+ 0]=(4582*y+   0*((int)b-y)+4020*((int)r-y)+1872*256+32768)/65536;//��=2��*3*0/20
	ClTable[n*20+ 1]=(4582*y+1824*((int)b-y)+2363*((int)r-y)+1872*256+32768)/65536;//��=2��*3*1/20
	ClTable[n*20+ 2]=(4582*y+2145*((int)b-y)-1242*((int)r-y)+1872*256+32768)/65536;//��=2��*3*2/20
	ClTable[n*20+ 3]=(4582*y+ 697*((int)b-y)-3824*((int)r-y)+1872*256+32768)/65536;//��=2��*3*3/20
	ClTable[n*20+ 4]=(4582*y-1325*((int)b-y)-3252*((int)r-y)+1872*256+32768)/65536;//��=2��*3*4/20
	ClTable[n*20+ 5]=(4582*y-2255*((int)b-y)+   0*((int)r-y)+1872*256+32768)/65536;//��=2��*3*5/20
	ClTable[n*20+ 6]=(4582*y-1326*((int)b-y)+3252*((int)r-y)+1872*256+32768)/65536;//��=2��*3*6/20
	ClTable[n*20+ 7]=(4582*y+ 697*((int)b-y)+3824*((int)r-y)+1872*256+32768)/65536;//��=2��*3*7/20
	ClTable[n*20+ 8]=(4582*y+2145*((int)b-y)+1242*((int)r-y)+1872*256+32768)/65536;//��=2��*3*8/20
	ClTable[n*20+ 9]=(4582*y+1824*((int)b-y)-2363*((int)r-y)+1872*256+32768)/65536;//��=2��*3*9/20
	ClTable[n*20+10]=(4582*y+   0*((int)b-y)-4020*((int)r-y)+1872*256+32768)/65536;//��=2��*3*10/20
	ClTable[n*20+11]=(4582*y-1824*((int)b-y)-2363*((int)r-y)+1872*256+32768)/65536;//��=2��*3*11/20
	ClTable[n*20+12]=(4582*y-2145*((int)b-y)+1242*((int)r-y)+1872*256+32768)/65536;//��=2��*3*12/20
	ClTable[n*20+13]=(4582*y- 697*((int)b-y)+3823*((int)r-y)+1872*256+32768)/65536;//��=2��*3*13/20
	ClTable[n*20+14]=(4582*y+1325*((int)b-y)+3252*((int)r-y)+1872*256+32768)/65536;//��=2��*3*14/20
	ClTable[n*20+15]=(4582*y+2255*((int)b-y)+   0*((int)r-y)+1872*256+32768)/65536;//��=2��*3*15/20
	ClTable[n*20+16]=(4582*y+1326*((int)b-y)-3252*((int)r-y)+1872*256+32768)/65536;//��=2��*3*16/20
	ClTable[n*20+17]=(4582*y- 697*((int)b-y)-3824*((int)r-y)+1872*256+32768)/65536;//��=2��*3*17/20
	ClTable[n*20+18]=(4582*y-2145*((int)b-y)-1242*((int)r-y)+1872*256+32768)/65536;//��=2��*3*18/20
	ClTable[n*20+19]=(4582*y-1824*((int)b-y)+2363*((int)r-y)+1872*256+32768)/65536;//��=2��*3*19/20

}

void start_composite(void)
{
	// �ϐ������ݒ�
	LineCount=0;				// ���������C���J�E���^�[
	drawing=0;

	PR2 = H_NTSC -1; 			// ��63.5usec�ɐݒ�
	T2CONSET=0x8000;			// �^�C�}2�X�^�[�g
}
void stop_composite(void)
{
	T2CONCLR = 0x8000;			// �^�C�}2��~
}

// �J���[�R���|�W�b�g�o�͏�����
void init_composite(void)
{
	unsigned int i;
	clearscreen();
	//�J���[�p���b�g������
	for(i=0;i<8;i++){
		set_palette(i,255*(i&1),255*((i>>1)&1),255*(i>>2));
	}
	for(i=0;i<8;i++){
		set_palette(i+8,128*(i&1),128*((i>>1)&1),128*(i>>2));
	}
	for(i=16;i<256;i++){
		set_palette(i,255,255,255);
	}
	//VRAM�X�L�����x�N�g��������
	vscanv1_x=256;
	vscanv1_y=0;
	vscanv2_x=0;
	vscanv2_y=256;
	vscanstartx=0;
	vscanstarty=0;

	// �^�C�}2�̏����ݒ�,�����N���b�N��63.5usec�����A1:1
	T2CON = 0x0000;				// �^�C�}2��~���
	mT2SetIntPriority(5);			// ���荞�݃��x��5
	mT2ClearIntFlag();
	mT2IntEnable(1);			// �^�C�}2���荞�ݗL����

	// OC1�̊��荞�ݗL����
	mOC1SetIntPriority(5);			// ���荞�݃��x��5
	mOC1ClearIntFlag();
	mOC1IntEnable(1);			// OC1���荞�ݗL����

	// OC3�̊��荞�ݗL����
	mOC3SetIntPriority(5);			// ���荞�݃��x��5
	mOC3ClearIntFlag();
	mOC3IntEnable(1);			// OC3���荞�ݗL����

	OSCCONCLR=0x10; // WAIT���߂̓A�C�h�����[�h
	INTEnableSystemMultiVectoredInt();
	SYSTEMConfig(95000000,SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE); //�L���b�V���L�����i���ӃN���b�N�ɂ͓K�p���Ȃ��j
	BMXCONCLR=0x40;	// RAM�A�N�Z�X�E�F�C�g0
	start_composite();
}
