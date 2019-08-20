// グラフィックライブラリ用ヘッダーファイル

extern const unsigned char FontData[];
extern void pset(int x,int y,unsigned char c);
extern void putbmpmn(int x,int y,char m,char n,const unsigned char bmp[]);
extern void clrbmpmn(int x,int y,char m,char n);
extern void putfont(int x,int y,unsigned int c,unsigned char n);
extern void line(int x1,int y1,int x2,int y2,unsigned int c);
extern void circle(int x0,int y0,unsigned int r,unsigned int c);
extern void printstr(int x,int y,unsigned int c,unsigned char *s);
extern void printnum(int x,int y,unsigned char c,unsigned int n);
