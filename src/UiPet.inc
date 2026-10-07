// The pet page of the 320x240 screens (included by Ui.cpp; Pet.h has the creature): the creature on its
// card with its words in a bubble, then name, stage, level and the food, joy and health bars. OK pets it,
// F feeds a snack, H treats it, N renames it, D switches death on or off; a grave takes OK for a new egg.
uint16_t petInk(uint8_t ink,unsigned kind,pet::Mood m){
 bool dead=m==pet::Gone,ill=m==pet::Sick;uint32_t body=dead?0x8d99a6:ill?0x9fb08a:pet::bodies[kind];
 switch(ink){case pet::InkOutline:return dead?rgb(0x2a3542):mix(body,0x000000,.68f);case pet::InkBody:return rgb(body);case pet::InkLight:return rgb(dead?0xc5ccd3:pet::accents[kind]);case pet::InkShell:return rgb(0xf4ecd8);
 case pet::InkEye:return rgb(0x101418);case pet::InkCheek:return rgb(0xf472b6);case pet::InkTear:return info;case pet::InkGlow:return rgb(0xf6f669);}return bg;
}
void petSprite(int x,int y,int scale,uint32_t now){
 uint8_t grid[pet::Size][pet::Size];creature.sprite(grid,now);pet::Mood m=creature.mood();unsigned k=creature.s.species%pet::Species;
 y-=creature.bob(now)*max(1,scale/3);x+=creature.sway(now)*max(1,scale/3);
 for(unsigned r=0;r<pet::Size;r++)for(unsigned c=0;c<pet::Size;c++)if(grid[r][c])g().fillRect(x+c*scale,y+r*scale,scale,scale,petInk(grid[r][c],k,m));
}
void heart(int cx,int cy,int s,uint16_t c){g().fillCircle(cx-s/2,cy,s/2+1,c);g().fillCircle(cx+s/2,cy,s/2+1,c);g().fillTriangle(cx-s-1,cy+1,cx+s+1,cy+1,cx,cy+s+2,c);}
void sparkle(int cx,int cy,int s,uint16_t c){g().drawFastHLine(cx-s,cy,2*s+1,c);g().drawFastVLine(cx,cy-s,2*s+1,c);g().drawPixel(cx-1,cy-1,c);g().drawPixel(cx+1,cy+1,c);g().drawPixel(cx+1,cy-1,c);g().drawPixel(cx-1,cy+1,c);}
// Hearts rise, stars twinkle, a packet falls into the mouth, a green cross for medicine, z's at night.
void petEffects(int x,int y,int scale,uint32_t now){
 uint32_t e=now-creature.effectAt;int top=y+3*scale,mid=x+8*scale;
 if(creature.mood()==pet::Sleepy){int k=(now/600)%3;for(int i=0;i<=k;i++)text(mid+3*scale+i*9,top-i*10,i%2?"Z":"z",dim,i==2?bold:body);}
 if(e>=2500)return;float f=e/2500.f;
 switch(creature.effect){
 case pet::Hearts:for(int i=0;i<3;i++)heart(x+(3+i*5)*scale,top-int(f*30)-(i%2)*10,4+(i==1),rgb(0xf472b6));break;
 case pet::Sparkle:for(int i=0;i<4;i++){int a=(now/120+i*2)%4;sparkle(x+(2+i*4)*scale,top-12+(i%2)*14,2+a%2,rgb(0xf6f669));}break;
 case pet::Food:{int fy=top-20+int(f*(6*scale));icon(IcMail,mid,fy,6,warn,card);break;}
 case pet::Medicine:{int cy=top-14-int(f*16);g().fillRect(mid-8,cy-3,17,7,ok);g().fillRect(mid-3,cy-8,7,17,ok);break;}
 default:break;
 }
}
void petBar(int x,int y,int w,const String& name,uint16_t value){
 uint16_t c=value>=500?ok:value>=250?warn:bad;text(x,y,name,dim,small);textRight(x+w,y,String((value+5)/10)+"%",value<250?bad:ink,small);
 g().fillRoundRect(x,y+4,w,7,3,line);if(value)g().fillRoundRect(x,y+4,max(4,int(w*value/pet::Full)),7,3,c);
}
String petTileDetail(){
 if(!creature.alive())return t("Gone","Умер");if(creature.s.stage==pet::Egg)return t("Egg · ","Яйцо · ")+creature.ageText(creature.hatchLeft());
 return String(creature.s.name)+" · "+creature.moodName();
}
void drawPet(){
 auto& d=g();uint32_t now=millis();const auto& s=creature.s;bool dead=!creature.alive(),egg=s.stage==pet::Egg;
 panel(8,26,156,188,card,10);target(8,26,156,188,-Enter);
 d.fillRoundRect(28,192,116,8,4,rgb(0x0b1016)); // its shadow on the floor
 petSprite(22,66,8,now);petEffects(22,66,8,now);
 if(creature.talking(now)){String rows[2];unsigned n=wrap(creature.speech,rows,2,128,small);int h=10+n*11;
  panel(14,30,144,h,ink,7);d.fillTriangle(78,30+h-1,92,30+h-1,84,30+h+6,ink);for(unsigned i=0;i<n;i++)textCenter(86,40+i*11,rows[i],bg,small);}
 int x=172,w=138;text(x,45,fit(s.name,w,big),ink,big);
 String stage=(dead||egg?String():String(creature.kind().name)+" · ")+creature.stageName(s.stage);text(x,61,fit(stage,w,small),dim,small);
 if(egg){
  uint32_t left=creature.hatchLeft();text(x,80,t("Hatches in ","Вылупится через "),dim,small);text(x,96,creature.ageText(left),ink,bold);
  d.fillRoundRect(x,102,w,7,3,line);d.fillRoundRect(x,102,max(4,int(w*(900-left)/900)),7,3,accent);
  String rows[6];unsigned n=wrap(t("It will feed on the radio: packets, messages and ACKs. Pet it, feed it snacks, keep it company.","Будет кормиться эфиром: пакеты, сообщения, подтверждения. Гладьте, кормите вкусняшками, не оставляйте одного."),rows,6,w,small);
  for(unsigned i=0;i<n;i++)text(x,126+i*12,rows[i],faint,small);
  footer({{"OK",t("Knock","Постучать")},{"N",t("Name","Имя")},{"BACK",t("Menu","Меню")}});return;}
 if(dead){
  text(x,82,fit(t("Lived ","Прожил ")+creature.ageText(s.age),w),ink);text(x,98,fit(creature.causeText(s.cause),w,small),bad,small);
  text(x,124,t("In memory","Память"),dim,small);
  for(unsigned i=0;i<s.graveCount&&i<3;i++){const auto& gr=s.graves[i];int y=140+i*22;d.fillCircle(x+5,y-4,4,rgb(pet::bodies[gr.species%pet::Species]));
   text(x+14,y,fit(String(gr.name)+" · "+t("Lv ","Ур. ")+String(gr.level),w-14,small),ink,small);text(x+14,y+10,fit(creature.ageText(gr.age),w-14,small),faint,small);}
  footer({{"OK",t("New egg","Новое яйцо")},{"D",s.mortal?t("Death: on","Смерть: вкл."):t("Death: off","Смерть: выкл.")},{"BACK",t("Menu","Меню")}});return;}
 pet::Mood m=creature.mood();uint16_t mc=m==pet::Happy?ok:m==pet::Hungry||m==pet::Lonely?warn:m==pet::Sick?bad:dim;
 text(x,76,creature.moodName(),mc,bold);
 uint32_t from=creature.levelXp(creature.level()),to=creature.levelXp(creature.level()+1);textRight(x+w,76,t("Lv ","Ур. ")+String(creature.level())+" · "+String(s.xp-from)+"/"+String(to-from),faint,small);
 d.fillRoundRect(x,81,w,3,1,line);d.fillRoundRect(x,81,max(2,int(w*(s.xp-from)/max<uint32_t>(1,to-from))),3,1,violet);
 petBar(x,100,w,t("Food","Сытость"),s.food);petBar(x,124,w,t("Joy","Радость"),s.joy);petBar(x,148,w,t("Health","Здоровье"),s.health);
 text(x,175,fit(t("Snacks ","Вкусняшки ")+String(s.snacks)+" · "+count(s.friends,"friend","friends","друг","друга","друзей"),w,small),ink,small);
 text(x,189,fit(t("Age ","Возраст ")+creature.ageText(s.age),w,small),dim,small);
 text(x,203,fit(s.mortal?t("D: death on","D: смерть вкл."):t("D: death off","D: смерть выкл."),w,small),faint,small);
 footer({{"OK",t("Pet","Погладить")},{"F",t("Feed","Кормить")},{"H",t("Heal","Лечить")},{"N",t("Name","Имя")}});
}
// A small pet on the locked screen when no game or message takes the place.
void drawLockPet(int y){
 uint32_t now=millis();petSprite(136,y,3,now);
 if(creature.mood()==pet::Sleepy)text(188,y+14,"z",dim,small);
 if(creature.needsCare())text(194,y+30,fit(creature.moodName(),110,small),warn,small);
}
// The pet answers in its bubble; only the death switch takes a notice.
bool petKey(int key){
 if(key==Enter||key==KeyHold){if(creature.alive())creature.cuddle();else creature.newEgg();return true;}
 if(key=='f'||key=='F'){creature.feed();return true;}
 if(key=='h'||key=='H'){creature.heal();return true;}
 if(key=='d'||key=='D'){notice(creature.setMortal(!creature.s.mortal),creature.s.mortal?warn:ok);return true;}
 if(key=='n'||key=='N'){editing=true;edit=creature.s.name;return true;}
 return false;
}
