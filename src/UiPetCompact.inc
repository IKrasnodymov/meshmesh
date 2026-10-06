// The pet page of the one-button and joystick boards (included by UiHeltec.cpp; Pet.h has the creature).
// On the 128x64 OLED it is drawn in outline: the lit pixels are its outline, eyes and mouth, the body stays
// dark, as on the LCD of a keychain pet. Hold (OK on the joystick) opens its menu: pet, feed, heal, death.
void petMono(int x,int y,int scale,uint32_t now){
 auto& c=*hardware.canvas;uint8_t grid[pet::Size][pet::Size];creature.sprite(grid,now);bool dead=!creature.alive();
 y+=creature.bob(now);x+=creature.sway(now); // down: the header line is right above
 for(unsigned r=0;r<pet::Size;r++)for(unsigned k=0;k<pet::Size;k++){uint8_t ink=grid[r][k];
  bool lit=ink==pet::InkOutline||ink==pet::InkEye||ink==pet::InkTear||ink==pet::InkGlow||(dead&&ink==pet::InkLight);
  if(lit)c.fillRect(x+k*scale,y+r*scale,scale,scale,1);}
}
// Tiny 7x7 signs of the bars: a bowl (food), a heart (joy), a cross (health).
void petSign(int x,int y,int which){
 auto& c=*hardware.canvas;
 if(which==0){c.drawFastHLine(x,y,7,1);c.drawLine(x,y,x+2,y+3,1);c.drawLine(x+6,y,x+4,y+3,1);c.drawFastHLine(x+2,y+3,3,1);c.drawPixel(x+3,y-2,1);}
 else if(which==1){c.fillCircle(x+1,y,1,1);c.fillCircle(x+5,y,1,1);c.fillTriangle(x,y,x+6,y,x+3,y+3,1);}
 else{c.fillRect(x+2,y-2,3,7,1);c.fillRect(x,y,7,3,1);}
}
void petBarMono(int x,int y,int w,uint16_t v){auto& c=*hardware.canvas;c.drawRect(x,y,w,5,1);int f=(w-2)*v/pet::Full;if(f)c.fillRect(x+1,y+1,f,3,1);}
String petTitle(){return creature.alive()&&creature.s.stage!=pet::Egg?String(creature.s.name)+" "+t("Lv","Ур.")+String(creature.level()):String(creature.s.name);}
void drawPetMono(){
 auto& c=*hardware.canvas;uint32_t now=millis();const auto& s=creature.s;
 petMono(0,11,3,now);
 if(creature.mood()==pet::Sleepy)say(42,20+((now/700)%2)*3,"z",small);
 if(creature.effect==pet::Hearts&&now-creature.effectAt<2500){int dy=(now-creature.effectAt)/200;c.fillCircle(38,24-dy,1,1);c.fillCircle(42,24-dy,1,1);c.fillTriangle(37,24-dy,43,24-dy,40,27-dy,1);}
 int x=52;
 if(creature.talking(now)){
  String rows[4];unsigned n=0,at=0;String v=creature.speech;
  while(at<v.length()&&n<4){unsigned end=at,space=0,cells=0;while(end<v.length()){unsigned next=end;uint32_t cp=utf8Next(v,next);if(cells+glyphCells(cp)>15)break;cells+=glyphCells(cp);end=next;if(cp==' ')space=end;}
   if(end<v.length()&&space>at)end=space;rows[n++]=v.substring(at,end);at=end;while(at<v.length()&&v[at]==' ')at++;}
  c.drawRoundRect(x-2,12,78,45,3,1);c.drawLine(x-2,40,x-6,44,1);c.drawLine(x-6,44,x-2,46,1);c.drawFastVLine(x-2,41,5,0);
  for(unsigned i=0;i<n;i++)say(x+1,22+i*9,rows[i],small);return;}
 if(s.stage==pet::Egg){say(x,21,creature.stageName(pet::Egg),small);say(x,31,t("hatches in","до вылупления"),small);say(x,41,creature.ageText(creature.hatchLeft()),bold);
  petBarMono(x,46,75,(900-creature.hatchLeft())*pet::Full/900);return;}
 if(!creature.alive()){say(x,21,creature.stageName(pet::Dead),bold);say(x,31,clipped(creature.causeText(s.cause),15),small);say(x,41,clipped(t("lived ","прожил ")+creature.ageText(s.age),15),small);
  say(x,51,clipped(s.mortal?t("death: on","смерть: вкл."):t("death: off","смерть: выкл."),15),small);return;}
 say(x,19,clipped(creature.moodName(),15),small);
 const uint16_t values[3]={s.food,s.joy,s.health};
 for(int i=0;i<3;i++){int y=26+i*9;petSign(x,y+1,i);petBarMono(x+10,y,65,values[i]);}
 say(x,55,clipped(t("snacks ","вкусн. ")+String(s.snacks),15),small);
}
