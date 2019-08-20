// ベクトル式VRAM　コンポジットカラー信号出力プログラム　PIC32MX370F512H用ヘッダーファイル　by K.Tanaka Rev. 1.1

#define X_RES	256 // 横方向解像度
#define Y_RES	224 // 縦方向解像度
#define VRAM_X   256 // VRAMの横方向最大値
#define VRAM_Y   256 // VRAMの縦方向最大値

extern volatile char drawing;		//　表示期間中は-1
extern volatile unsigned short drawcount;		//　1画面表示終了ごとに1足す。アプリ側で0にする。
							// 最低1回は画面表示したことのチェックと、アプリの処理が何画面期間必要かの確認に利用。
extern unsigned char VRAM[]; // ビデオメモリ
extern volatile short vscanv1_x,vscanv1_y,vscanv2_x,vscanv2_y;	//映像表示スキャン用ベクトル
extern volatile short vscanstartx,vscanstarty; //映像表示スキャン開始座標
// (vstartx,vstarty):画面左上になるVRAM上の座標（256倍）
// (vscanv1_x,vscanv1_y):画面右方向のスキャンベクトル（256倍）
// (vscanv2_x,vscanv2_y):画面下方向のスキャンベクトル（256倍）

extern void start_composite(void); //カラーコンポジット出力開始
extern void stop_composite(void); //カラーコンポジット出力停止
extern void init_composite(void); //カラーコンポジット出力初期化
extern void clearscreen(void); //画面クリア
extern void set_palette(unsigned char n,unsigned char b,unsigned char r,unsigned char g); //カラーパレット設定
