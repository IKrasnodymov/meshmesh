#include "UiIcons.h"
#include "Dice.h"
#include <Arduino.h>
#include <math.h>
#include <initializer_list>
namespace {
void arc(Adafruit_GFX& d,int cx,int cy,int r,int a0,int a1,uint16_t c){for(int a=a0;a<=a1;a+=3){float rad=a*M_PI/180;d.drawPixel(cx+roundf(r*sinf(rad)),cy-roundf(r*cosf(rad)),c);}}
void thick(Adafruit_GFX& d,int x0,int y0,int x1,int y1,uint16_t c){d.drawLine(x0,y0,x1,y1,c);d.drawLine(x0+1,y0,x1+1,y1,c);}
}
void drawIcon(Adafruit_GFX& d,Icon id,int cx,int cy,int s,uint16_t c,uint16_t hole,uint16_t shade){
 int h=s/2,q=max(1,s/4),w=max(1,s/8);
 switch(id){
 case IcChat:d.fillRoundRect(cx-s,cy-s*3/4,2*s,s*3/2,max(2,s/3),c);d.fillTriangle(cx-h,cy+s*3/4-1,cx,cy+s*3/4-1,cx-s*3/4,cy+s,c);for(int k=-1;k<=1;k++)d.fillCircle(cx+k*h,cy,w,hole);break;
 case IcPin:d.fillCircle(cx,cy-q,s*5/8,c);d.fillTriangle(cx-s*9/16,cy,cx+s*9/16,cy,cx,cy+s,c);d.fillCircle(cx,cy-q,max(1,s/4),hole);break;
 case IcMesh:{int ax=cx,ay=cy-s*3/4,bx=cx-s*3/4,by=cy+s*5/8,ex=cx+s*3/4;thick(d,ax,ay,bx,by,c);thick(d,ax,ay,ex,by,c);thick(d,bx,by,ex,by,c);for(int p:{0,1,2})d.fillCircle(p==0?ax:p==1?bx:ex,p==0?ay:by,max(2,s/4),c);break;}
 case IcCompass:d.drawCircle(cx,cy,s,c);if(s>7)d.drawCircle(cx,cy,s-1,c);d.fillTriangle(cx,cy-s*3/4,cx-max(2,s/4),cy,cx+max(2,s/4),cy,c);d.fillTriangle(cx,cy+s*3/4,cx-max(2,s/4),cy,cx+max(2,s/4),cy,shade);break;
 case IcWifi:{int oy=cy+s*5/8;for(int r:{s,s*2/3,s/3}){arc(d,cx,oy,r,-45,45,c);arc(d,cx,oy,r-1,-45,45,c);}d.fillCircle(cx,oy,max(1,s/6),c);break;}
 case IcBle:d.drawLine(cx,cy-s,cx,cy+s,c);d.drawLine(cx,cy-s,cx+h,cy-h,c);d.drawLine(cx+h,cy-h,cx-h,cy+h,c);d.drawLine(cx,cy+s,cx+h,cy+h,c);d.drawLine(cx+h,cy+h,cx-h,cy-h,c);if(s>6){d.drawLine(cx+1,cy-s,cx+1,cy+s,c);}break;
 case IcGear:d.fillCircle(cx,cy,s*5/8,c);for(int k=0;k<8;k++){float a=k*M_PI/4;d.fillCircle(cx+roundf(s*.8f*cosf(a)),cy+roundf(s*.8f*sinf(a)),max(1,s/4),c);}d.fillCircle(cx,cy,max(1,s/4),hole);break;
 case IcTower:d.drawLine(cx,cy-q,cx-h,cy+s,c);d.drawLine(cx,cy-q,cx+h,cy+s,c);d.drawLine(cx,cy-q,cx,cy+s,c);d.fillCircle(cx,cy-h,max(1,s/5),c);arc(d,cx,cy-h,h+1,-75,-30,c);arc(d,cx,cy-h,h+1,30,75,c);arc(d,cx,cy-h,s-1,-75,-35,c);arc(d,cx,cy-h,s-1,35,75,c);break;
 case IcRoom:d.fillTriangle(cx-s,cy,cx+s,cy,cx,cy-s,c);d.fillRect(cx-s*3/4,cy,s*3/2,s,c);d.fillRect(cx-max(1,s/5),cy+h-1,max(2,s*2/5),s-h+1,hole);break;
 case IcSensor:d.fillRoundRect(cx-max(1,s/5),cy-s,max(3,s*2/5),s*3/2,max(1,s/5),c);d.fillCircle(cx,cy+h+q/2,max(2,s*2/5),c);break;
 case IcPerson:d.fillCircle(cx,cy-h+1,max(2,s*3/8),c);d.fillRoundRect(cx-s*5/8,cy+1,s*5/4,s,max(2,s/2),c);break;
 case IcLock:arc(d,cx,cy-q,max(2,s*3/8),-90,90,c);arc(d,cx,cy-q,max(2,s*3/8)-1,-90,90,c);d.drawFastVLine(cx-max(2,s*3/8),cy-q,q+1,c);d.drawFastVLine(cx+max(2,s*3/8),cy-q,q+1,c);d.fillRoundRect(cx-s*5/8,cy,s*5/4,s*7/8,max(1,s/5),c);break;
 case IcMail:d.drawRect(cx-s,cy-s*5/8,2*s,s*5/4,c);d.drawLine(cx-s,cy-s*5/8,cx,cy+q/2,c);d.drawLine(cx+s-1,cy-s*5/8,cx,cy+q/2,c);break;
 case IcHash:thick(d,cx-q,cy-s,cx-h,cy+s,c);thick(d,cx+h,cy-s,cx+q,cy+s,c);d.fillRect(cx-s,cy-q-1,2*s,2,c);d.fillRect(cx-s,cy+q-1,2*s,2,c);break;
 case IcPulse:{int p[][2]={{-s,0},{-h,0},{-q,-s*3/4},{q,s*3/4},{h,0},{s,0}};for(int k=0;k<5;k++)thick(d,cx+p[k][0],cy+p[k][1],cx+p[k+1][0],cy+p[k+1][1],c);break;}
 case IcHelp:d.drawCircle(cx,cy,s,c);break; // the caller writes "?" in its font
 case IcRadio:d.drawFastVLine(cx,cy-h,s+h,c);d.fillCircle(cx,cy-h,max(1,s/5),c);arc(d,cx,cy-h,h+1,-120,-60,c);arc(d,cx,cy-h,h+1,60,120,c);arc(d,cx,cy-h,s,-125,-55,c);arc(d,cx,cy-h,s,55,125,c);break;
 case IcRadar:d.drawCircle(cx,cy,s,c);d.drawCircle(cx,cy,max(2,s/2),c);thick(d,cx,cy,cx+roundf(s*.7f)-1,cy-roundf(s*.7f)+1,c);d.fillCircle(cx,cy,max(1,s/6),c);d.fillCircle(cx-h,cy+q+1,max(1,s/6),c);break;
 case IcCards:{d.fillRoundRect(cx-s,cy-s,s*9/8,s*3/2,2,c);int x0=cx-s/4,y0=cy-s/2,w=s*5/4,h=s*3/2,mx=x0+w/2,my=y0+h/2;d.fillRoundRect(x0-2,y0-2,w+4,h+4,3,hole);d.fillRoundRect(x0,y0,w,h,2,c);d.fillTriangle(mx,my-q-2,mx-q-1,my,mx+q+1,my,hole);d.fillTriangle(mx,my+q+2,mx-q-1,my,mx+q+1,my,hole);break;}
 case IcPaw:{int r=max(1,s/5);d.fillCircle(cx,cy+q+1,h+1,c);d.fillCircle(cx-q,cy+h,q+1,c);d.fillCircle(cx+q,cy+h,q+1,c);
  d.fillCircle(cx-s*3/4,cy-q+1,r,c);d.fillCircle(cx-q-1,cy-s*3/4+1,r,c);d.fillCircle(cx+q+1,cy-s*3/4+1,r,c);d.fillCircle(cx+s*3/4,cy-q+1,r,c);break;}
 case IcChess:d.fillCircle(cx,cy-s/2,max(2,s*3/10),c);d.fillTriangle(cx,cy-s/2,cx-s/2,cy+s/2,cx+s/2,cy+s/2,c);d.fillRoundRect(cx-s*3/4,cy+s/2,s*3/2,max(2,s/3),1,c);d.fillRect(cx-s*3/8,cy-s/6,s*3/4,max(1,s/6),c);break;
 case IcDice:{int r=max(2,s/4),p=max(1,s/5);d.fillRoundRect(cx-s*7/8,cy-s*7/8,s*7/4,s*7/4,r,c);for(int k:{-1,0,1})d.fillCircle(cx+k*h,cy+k*h,p,hole);break;} // a die showing three
 case IcPower:{int r=s*3/4;arc(d,cx,cy+q/2,r,40,320,c);arc(d,cx,cy+q/2,r-1,40,320,c);thick(d,cx,cy-s,cx,cy,c);break;}
 case IcScreen:d.drawRoundRect(cx-s,cy-s*5/8,2*s,s*5/4,2,c);d.fillRect(cx-s+3,cy-s*5/8+3,2*s-6,s*5/4-6,c);break;
 case IcKey:d.drawCircle(cx-h,cy,max(2,s*3/8),c);d.drawFastHLine(cx-h+s*3/8,cy,s+1,c);d.drawFastVLine(cx+h,cy,max(2,s/3),c);d.drawFastVLine(cx+s-1,cy,max(2,s/3),c);break;
 }
}
namespace {
void polygon(Adafruit_GFX& d,int cx,int cy,int r,int n,uint16_t c){int xp=0,yp=0;for(int k=0;k<=n;k++){float a=-M_PI/2+k*2*M_PI/n;int x=cx+roundf(r*cosf(a)),y=cy+roundf(r*sinf(a));if(k)d.fillTriangle(cx,cy,xp,yp,x,y,c);xp=x;yp=y;}}
}
void drawDieShape(Adafruit_GFX& d,uint8_t shape,int cx,int cy,int s,uint16_t fill,uint16_t edge,uint16_t face,unsigned pips){
 switch(shape){
 case dice::D4:d.fillTriangle(cx,cy-s-1,cx-s-2,cy+s-1,cx+s+2,cy+s-1,fill);break;
 case dice::D6:d.fillRoundRect(cx-s+1,cy-s+1,2*s-1,2*s-1,max(2,s/3),fill);
  if(pips>=1&&pips<=6){int o=s/2,r=max(1,o/3);auto p=[&](int dx,int dy){d.fillCircle(cx+dx*o,cy+dy*o,r,face);};
   if(pips&1)p(0,0);if(pips>=2){p(-1,-1);p(1,1);}if(pips>=4){p(1,-1);p(-1,1);}if(pips==6){p(-1,0);p(1,0);}}break;
 case dice::D8:d.fillTriangle(cx,cy-s-1,cx-s,cy,cx+s,cy,fill);d.fillTriangle(cx,cy+s+1,cx-s,cy,cx+s,cy,fill);break;
 case dice::D10:d.fillTriangle(cx,cy-s-1,cx-s,cy+2,cx+s,cy+2,fill);d.fillTriangle(cx,cy+s-1,cx-s,cy+2,cx+s,cy+2,fill);break;
 case dice::D12:polygon(d,cx,cy+1,s+1,5,fill);break;
 case dice::D20:{polygon(d,cx,cy,s+1,6,fill);int r=s*5/8;d.drawTriangle(cx,cy-r-2,cx-r-2,cy+r-1,cx+r+2,cy+r-1,edge);break;}
 default:d.fillRoundRect(cx-s-2,cy-s+3,2*s+5,2*s-5,max(2,s/3),fill);break;
 }
}
