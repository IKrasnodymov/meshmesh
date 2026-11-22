// The dice page of the 320x240 screens (included by Ui.cpp; Dice.h keeps the rolls): DIC3R's modes as tabs.
// RPG: the last roll drawn as dice (d4 a triangle, d6 with pips, d8 a diamond, d10 a kite, d12 a pentagon, d20
// a hexagon), its sum, the pool and the dice to choose; S opens the saved rolls (the sheet's abilities), E rolls a
// typed formula. Warhammer: a grid of up to 50 dice, the arrows choose how many, OK rolls them, high to low,
// green at or over the threshold. Sheet: the character's blocks in rows of twelve columns; the arrows move, OK
// rolls a stat's check or an ability, marks pips, +/- change a value, V types one, I shows the note. P picks the
// character (A: new from a template, S: send the sheet to a contact, O: a sheet a contact sent), M (or 1-3,
// or a tap on a tab) changes the mode. Blocks are built on the web page or in the app.
enum DiceSheet:uint8_t {DiceMain,DiceSaved,DiceHeroes,DiceTemplates,DiceSend,DiceOffer,DiceBlock};
enum DiceEdit:uint8_t {EditQuick,EditNewFormula,EditNewName,EditFormula,EditName,EditValue,EditHeroNew,EditHeroName,EditField};
uint8_t diceSheet=DiceMain,diceEditing=EditQuick;int diceSel=0,sheetSel=0,sheetTop=0,blockField=0;bool sheetNote=false;String diceDraft;
uint32_t diceArmed=0,diceRollAt=0,diceSeen=0,diceRolls=0,diceNotes=0,sheetRollAt=0;
constexpr uint32_t DiceSpin=450,SheetRollShow=5000; // ms of tumbling after a roll; of the roll card over the sheet
// Touch spots of the frame (uiTouch): what a tap there does.
enum DiceTap:uint8_t {TapTab,TapHero,TapType,TapRoll,TapCell,TapBlock,TapSaved,TapHeroRow,TapAdd,TapPick,TapField};
struct DiceSpot{int16_t x,y,w,h;uint8_t act;int16_t arg;};DiceSpot diceSpots[96];unsigned diceSpotCount=0;
void diceSpot(int x,int y,int w,int h,uint8_t act,int arg){if(diceSpotCount<96)diceSpots[diceSpotCount++]={int16_t(x),int16_t(y),int16_t(w),int16_t(h),act,int16_t(arg)};}
uint16_t diceColor(unsigned i){return rgb(dice::colors[i%dice::Colors]);}
bool diceSpinning(){return millis()-diceRollAt<DiceSpin;}
String diceModeName(uint8_t m){return m==dice::Rpg?String("RPG"):m==dice::Grid?String("Warhammer"):t("Sheet","Лист");}
String diceTileDetail(){
 const dice::Result* r=dicer.last();if(!r)return "RPG · Warhammer";
 return String(r->label[0]?r->label:r->formula)+(r->special?String():" = "+String(r->total));
}
// R.E.T.: successes or the failure of a roll.
String diceRetText(const dice::Result& r){
 if(r.fail)return r.fail==1?t("Critical failure (d4: 1)","Критический провал (d4: 1)"):t("Failure, d4: ","Провал, d4: ")+String(r.fail);
 return r.successes>1?t("Success x","Успех x")+String(r.successes):t("Success","Успех");
}
// A die with its value: a maximum is green, a one red, d66 and d% in the accent, a subtracted die grey.
void drawDie(int cx,int cy,uint8_t shape,unsigned value,const String& label,bool negative,int s=13){
 unsigned top=shape<dice::D66?dice::faces(shape):0;bool high=top&&value==top,one=top>=4&&value==1;
 uint32_t fill=negative?0x8d99a6:shape==dice::D66||shape==dice::DPercent?0x1fc2ae:high?0x4cc26b:one?0xe5564d:0xe4e9ee;uint16_t face=one?ink:bg;
 drawDieShape(g(),shape,cx,cy,s,rgb(fill),mix(fill,0x000000,.25f),face,shape==dice::D6&&s>=10?value:0);
 if(shape==dice::D6&&value>=1&&value<=6&&s>=10)return;
 bool tiny=label.length()>2||s<10;int dy=shape==dice::D4?s/2+2:shape==dice::D10?3:tiny?3:5;
 textCenter(cx,cy+dy,label,face,tiny?small:bold);
}
// The tabs and the character on top; touch spots for both. A sheet sent by a contact shows a mark by the name.
void diceTabs(){
 int x=8;for(uint8_t m=0;m<dice::ModeCount;m++){String n=diceModeName(m);int w=measure(n)+16;bool on=dicer.s.mode==m;
  panel(x,25,w,18,on?cardHi:card,5);text(x+8,38,n,on?ink:dim);if(on)g().fillRoundRect(x+6,40,w-12,2,1,accent);diceSpot(x,23,w,22,TapTab,m);x+=w+4;}
 String who=fit(dicer.heroName(dicer.s.character),306-x-24,small);int w=measure(who,small);
 icon(IcPerson,306-w-8,33,5,dicer.offered?warn:dicer.s.character?violet:faint);textRight(310,37,who,dicer.s.character?ink:dim,small);diceSpot(306-w-16,23,w+20,22,TapHero,0);
 if(dicer.offered)g().fillCircle(306-w-3,28,3,warn);
}
// RPG: the roll card, the pool and its dice, then the totals before it.
void drawDiceRpg(){
 const dice::Result* r=dicer.last();bool spin=diceSpinning();
 panel(8,48,200,148,card,10);diceSpot(8,48,200,148,TapRoll,0);
 if(!r){icon(IcDice,108,104,20,faint,card);textCenter(108,152,t("OK: roll ","OK: бросить ")+dicer.poolText(),dim);textCenter(108,168,t("E: any formula, 3d8+2","E: любая формула, 3к8+2"),faint,small);}
 else{
  String head=r->label[0]?String(r->label)+" · "+dice::readable(r->formula):dice::readable(r->formula);text(16,62,fit(head,184,small),r->label[0]?violet:dim,small);
  // Dice and numbers flow in rows of 30 px; more than two rows: the values as text.
  auto partLabel=[&](unsigned i){unsigned v=r->value[i];return r->kind[i]==dice::PartNumber?String(r->negative[i]?"-":"+")+String(v):r->kind[i]==dice::PartPercent?dice::valueText(dice::DPercent,v):String(v);};
  auto partWidth=[&](unsigned i){return r->kind[i]==dice::PartNumber?measure(partLabel(i),bold)+10:r->shape[i]>=dice::D66?33:28;};
  int x=16,row=0;bool many=r->more;
  for(unsigned i=0;i<r->parts&&!many;i++){int w=partWidth(i);if(x+w>200){x=16;row++;}many=row>1;x+=w+2;}
  if(many){String rows[6];unsigned n=wrap(dicer.resultText(*r,false),rows,r->special?6:4,184);
   for(unsigned i=0;i<n;i++)text(16,84+i*15,rows[i],ink);}
  else{x=16;row=0;
   for(unsigned i=0;i<r->parts;i++){bool num=r->kind[i]==dice::PartNumber;unsigned v=r->value[i];String label=partLabel(i);
    int w=partWidth(i);if(x+w>200){x=16;row++;}int cy=88+row*31;
    if(num){panel(x,cy-9,w,19,rgb(0x1d3557),5);textCenter(x+w/2,cy+5,label,info,bold);}
    else{int jx=0,jy=0;if(spin){unsigned top=r->shape[i]<dice::D66?dice::faces(r->shape[i]):r->kind[i]==dice::PartD66?66:100;v=r->kind[i]==dice::PartD66?(1+rand()%6)*10+1+rand()%6:1+rand()%top;jx=rand()%3-1;jy=rand()%3-1;
      label=r->kind[i]==dice::PartPercent?dice::valueText(dice::DPercent,v):String(v);}
     drawDie(x+w/2+jx,cy+jy,r->shape[i],v,label,r->negative[i]);}
    x+=w+2;}}
  if(!r->special&&!spin&&(r->successes||r->fail))text(16,170,diceRetText(*r),r->fail?bad:ok,small);
  if(!r->special&&!spin){String total=String(r->total);int tw=measure(total,digits);textRight(198,190,total,warn,digits);text(198-tw-16,180,"=",dim,big);
   if(r->parts>1&&!many){String rows[2];unsigned n=wrap(dicer.resultText(*r,false),rows,2,max(40,198-tw-34),small);for(unsigned i=0;i<n;i++)text(16,178+i*10,rows[i],dim,small);}}
  else if(r->special&&!spin)text(16,188,t("d66 and d%: each die on its own","d66 и d%: каждая кость отдельно"),faint,small);
 }
 // The pool: count and die, the modifier; the eight dice below.
 panel(214,48,98,40,cardHi,8);diceSpot(214,48,98,40,TapRoll,0);
 String pool=dicer.poolText();const Face& pf=measure(pool,big)<=90?big:bold;textCenter(263,pf.main==big.main?75:72,pool,ink,pf);
 for(int i=0;i<dice::TypeCount;i++){int x=214+(i%2)*50,y=93+(i/2)*22;bool on=dicer.s.type==i;panel(x,y,48,19,on?accent:card,5);textCenter(x+24,y+14,dice::typeNames[i],on?bg:ink);diceSpot(x,y,48,19,TapType,i);}
 text(214,193,"^v N  <> dX  +/- MOD",faint,small);
 // The rolls before this one, newest first.
 int x=8;if(dicer.resultCount>1){text(x,212,t("Before:","Ранее:"),faint,small);x+=measure(t("Before:","Ранее:"),small)+6;}
 for(int i=int(dicer.resultCount)-2;i>=0&&x<300;i--){const dice::Result& p=dicer.results[i];String v=p.special?String(p.formula):String(p.total);int w=measure(v,small);if(x+w>304)break;text(x,212,v,dim,small);x+=w+10;}
 unsigned saved=dicer.savedCount();textRight(312,212,t("S: saved ","S: сохранённые ")+String(saved),saved?accent:faint,small);
}
// Warhammer: ten dice a row; chosen blue, rolled green, under the threshold red.
void drawDiceGrid(){
 auto& d=g();auto& s=dicer.s;bool spin=diceSpinning()&&dicer.gridRolled;unsigned top=dice::faces(s.gridType);
 for(unsigned i=0;i<s.gridSize;i++){int x=11+(i%10)*30,y=49+(i/10)*27;bool chosen=i<s.gridChosen,rolled=chosen&&dicer.gridRolled;
  unsigned v=dicer.grid[i];if(spin&&chosen)v=s.gridType==dice::D66?(1+rand()%6)*10+1+rand()%6:1+rand()%top;
  bool pass=!s.thresholdOn||v>=s.threshold;uint16_t fill=rolled?(spin?cardHi:pass?rgb(0x2f8f4e):rgb(0xa8322c)):chosen?rgb(0x2f5f9e):card;
  d.fillRoundRect(x,y,27,24,5,fill);if(int(i)==int(s.gridChosen)-1)ring(x-1,y-1,29,26,6,ink);
  if(chosen)textCenter(x+13,y+16,dice::valueText(s.gridType,v),ink,rolled?bold:body);diceSpot(x,y,27,24,TapCell,i);}
 if(s.gridSize<dice::GridMax){unsigned i=s.gridSize;int x=11+(i%10)*30,y=49+(i/10)*27;d.drawRoundRect(x,y,27,24,5,line);textCenter(x+13,y+17,"+",dim,bold);diceSpot(x,y,27,24,TapAdd,0);}
 int y=196;String what=s.gridChosen?String(s.gridChosen)+" × "+dice::typeNames[s.gridType]:t("Choose dice: arrows","Выберите кости стрелками");
 text(8,y,what,ink,bold);int x=8+measure(what,bold)+10;
 if(dicer.gridRolled&&s.thresholdOn&&!spin){String n=count(dicer.gridSuccesses(),"success","successes","успех","успеха","успехов");text(x,y,n,ok,bold);x+=measure(n,bold)+8;}
 else if(dicer.gridRolled&&!spin){unsigned sum=0;for(unsigned i=0;i<s.gridChosen;i++)sum+=dicer.grid[i];String n=t("sum ","сумма ")+String(sum);text(x,y,n,dim);x+=measure(n)+8;}
 String th=s.thresholdOn?t("Threshold ","Порог ")+dice::valueText(s.gridType,s.threshold)+"+":t("No threshold","Без порога");textRight(312,y,th,s.thresholdOn?warn:faint,small);
 text(8,212,t("A: +5 dice   R: reset   D: die   T, +/-: threshold","A: +5 костей   R: сброс   D: кость   T, +/-: порог"),faint,small);
}
// The sheet: blocks in rows of twelve columns over 304 px, each row as tall as its tallest block.
struct SheetBox{int16_t x,y,w,h;};SheetBox sheetBoxes[dice::Blocks];int sheetHeightAll=0;
constexpr int SheetY=48,SheetH=158;
int sheetBlockHeight(unsigned i,int w){
 const dice::Block& b=dicer.sheet.block[i];
 switch(b.kind){
 case dice::Stat:return w<90?60:40;
 case dice::Track:return b.max||b.formula[0]?34:40;
 case dice::Pips:return 32;
 case dice::Ability:return 22;
 case dice::Note:{String rows[4];unsigned n=wrap(dicer.noteText(i),rows,3,w-12,small);return 18+max(1u,n)*10;}
 default:return 18;}
}
void sheetLayout(){
 int col=0,y=0,rowStart=0,rowH=0;unsigned n=dicer.sheet.count;
 auto close=[&](unsigned upto){for(unsigned k=rowStart;k<upto;k++)sheetBoxes[k].h=rowH;y+=rowH+4;rowH=0;col=0;rowStart=upto;};
 for(unsigned i=0;i<n;i++){int sp=dice::span(dicer.sheet.block[i].width);if(col+sp>12)close(i);
  int x0=8+col*308/12,x1=8+(col+sp)*308/12-4;sheetBoxes[i]={int16_t(x0),int16_t(y),int16_t(x1-x0),0};
  rowH=max(rowH,sheetBlockHeight(i,x1-x0));col+=sp;}
 if(n)close(n);sheetHeightAll=max(0,y-4);
}
// The page scrolls by whole rows: the top is always the start of a row.
void sheetKeepVisible(){
 if(!dicer.sheet.count)return;sheetSel=constrain(sheetSel,0,int(dicer.sheet.count)-1);const SheetBox& b=sheetBoxes[sheetSel];
 if(b.y<sheetTop)sheetTop=b.y;
 if(b.y+b.h>sheetTop+SheetH){int want=b.y+b.h-SheetH,top=b.y;for(unsigned i=0;i<dicer.sheet.count;i++)if(sheetBoxes[i].y>=want&&sheetBoxes[i].y<top)top=sheetBoxes[i].y;sheetTop=top;}
}
void drawSheetBlock(unsigned i,int x,int y,int w,int h,bool focus){
 auto& d=g();const dice::Block& b=dicer.sheet.block[i];uint32_t hue=dice::colors[b.color%dice::Colors];uint16_t c=rgb(hue);
 if(b.kind==dice::Header){text(x+2,y+13,fit(b.name,w-4,bold),focus?ink:dim,bold);d.drawFastHLine(x,y+16,w,focus?accent:line);return;}
 panel(x,y,w,h,focus?cardHi:card,6);if(focus)ring(x,y,w,h,6,b.kind==dice::Stat?c:accent);
 switch(b.kind){
 case dice::Stat:{
  bool bad1=!dicer.derivedOk(i),derived=b.formula[0];String v=bad1?"?":b.rule==dice::Bonus?(dicer.value(i)<0?String(dicer.value(i)):"+"+String(dicer.value(i))):String(dicer.value(i)),badge=dicer.badge(i);
  if(w<90){int r=min(w/2-6,17),cy=y+4+r;drawStatShape(d,b.shape,x+w/2,cy,r,mix(hue,0x131a22,.72f),c);
   const Face& f=measure(v,bold)<=2*r-4?bold:small;textCenter(x+w/2,cy+(f.main==bold.main?5:4),v,bad1?bad:derived?info:ink,f);
   textCenter(x+w/2,y+47,fit(b.name,w-4,small),focus?ink:dim,small);if(badge.length()&&!bad1)textCenter(x+w/2,y+57,badge,b.rule==dice::Ret?violet:accent,small);}
  else{int r=min(h/2-4,15),cx=x+6+r,cy=y+h/2;drawStatShape(d,b.shape,cx,cy,r,mix(hue,0x131a22,.72f),c);
   const Face& f=measure(v,bold)<=2*r-2?bold:small;textCenter(cx,cy+(f.main==bold.main?5:4),v,bad1?bad:derived?info:ink,f);
   int tx=cx+r+8;text(tx,y+16,fit(b.name,x+w-tx-4,small),focus?ink:dim,small);if(badge.length()&&!bad1)text(tx,y+31,badge,b.rule==dice::Ret?violet:accent,bold);}
  break;}
 case dice::Track:{
  int v=dicer.value(i),m=dicer.maxOf(i);d.fillRoundRect(x,y,4,h,2,c);text(x+9,y+13,fit(b.name,w-70,small),focus?ink:dim,small);
  if(m>0){String val=String(v)+"/"+String(m);textRight(x+w-6,y+14,val,v<=0?bad:ink,bold);int bw=w-16,fillW=constrain(v,0,m)*bw/m;
   d.fillRoundRect(x+9,y+20,bw,8,3,line);if(fillW)d.fillRoundRect(x+9,y+20,fillW,8,3,v*4<=m?bad:c);}
  else textCenter(x+w/2,y+h-7,String(v),ink,big);
  break;}
 case dice::Pips:{
  int m=dicer.maxOf(i),v=dicer.value(i);text(x+6,y+12,fit(b.name,w-12,small),focus?ink:dim,small);int step=min(14,(w-12)/max(1,m)),r=min(5,step/2-1);
  for(int k=0;k<m;k++){int cx=x+6+r+k*step,cy=y+23;if(k<v)d.fillCircle(cx,cy,r,c);else{d.drawCircle(cx,cy,r,faint);}}
  break;}
 case dice::Ability:{
  d.fillCircle(x+9,y+11,4,c);String roll=dicer.rollFormula(i);bool broken=b.formula[0]&&!roll.length();
  String side=broken?String("?"):roll.length()?dice::readable(roll.c_str()):b.use[0]&&b.spend?String("-")+String(b.spend)+" "+b.use:String();int sw=side.length()?measure(side,small)+6:0;
  text(x+17,y+15,fit(b.name,w-24-sw,focus?bold:body),focus?ink:dim,focus?bold:body);if(sw)textRight(x+w-5,y+15,side,broken?bad:focus?accent:faint,small);
  if(b.noteLen)d.fillCircle(x+w-sw-8,y+11,1,faint);
  break;}
 default:{ // a note
  text(x+6,y+12,fit(b.name,w-12,small),faint,small);String rows[3];unsigned n=wrap(dicer.noteText(i),rows,3,w-12,small);
  for(unsigned k=0;k<n;k++)text(x+6,y+24+k*10,rows[k],focus?ink:dim,small);
  if(!n)text(x+6,y+24,t("Empty: write on the web page","Пусто: заполните на веб-странице"),faint,small);
  break;}
 }
}
// The last roll over the sheet for a few seconds: its dice, the sum, R.E.T. successes or failure.
void drawSheetRoll(){
 const dice::Result* r=dicer.last();if(!r||millis()-sheetRollAt>SheetRollShow)return;bool spin=diceSpinning();
 panel(8,150,304,54,cardHi,10);g().drawRoundRect(8,150,304,54,10,line);
 text(16,164,fit(String(r->label[0]?r->label:r->formula)+" · "+dice::readable(r->formula),200,small),violet,small);
 int x=24;for(unsigned k=0;k<r->parts&&x<230;k++){unsigned v=r->value[k];
  if(r->kind[k]==dice::PartNumber){String l=String(r->negative[k]?"-":"+")+String(v);text(x-8,190,l,info,bold);x+=measure(l,bold)+6;continue;}
  if(spin){unsigned top=r->shape[k]<dice::D66?dice::faces(r->shape[k]):100;v=1+rand()%top;}
  drawDie(x,184,r->shape[k],v,String(v),r->negative[k],10);x+=26;}
 if(!spin){if(!r->special){String total=String(r->total);textRight(304,192,total,r->fail?bad:warn,big);}
  if(r->successes||r->fail){String m=diceRetText(*r);textRight(304,166,m,r->fail?bad:ok,small);}}
}
void drawDiceSheet(){
 auto& d=g();sheetLayout();unsigned n=dicer.sheet.count;
 if(!n){icon(IcDice,160,96,18,faint);textCenter(160,136,t("The sheet is empty","Лист пуст"),dim);
  textCenter(160,154,t("A: add a block","A: добавить блок"),faint,small);textCenter(160,166,t("P, A: a character from D&D or R.E.T.","P, A: персонаж из D&D или Р.Э.Т."),faint,small);return;}
 sheetKeepVisible();
 for(unsigned i=0;i<n;i++){const SheetBox& b=sheetBoxes[i];int y=SheetY+b.y-sheetTop;if(y<SheetY-1||y+b.h>SheetY+SheetH+1)continue;
  drawSheetBlock(i,b.x,y,b.w,b.h,int(i)==sheetSel);diceSpot(b.x,y,b.w,b.h,TapBlock,i);}
 if(sheetHeightAll>SheetH){int th=max(12,SheetH*SheetH/sheetHeightAll),ty=SheetY+(SheetH-th)*sheetTop/max(1,sheetHeightAll-SheetH);d.fillRoundRect(315,SheetY,3,SheetH,1,card);d.fillRoundRect(315,ty,3,th,1,faint);}
 // The chosen block's detail: its note, its formula.
 const dice::Block& b=dicer.sheet.block[sheetSel];String detail;
 if(b.kind==dice::Stat&&b.formula[0])detail="= "+String(b.formula);
 else if(b.kind==dice::Track&&b.formula[0])detail=t("max = ","макс. = ")+b.formula;
 else if(b.kind==dice::Ability&&b.noteLen)detail=dicer.noteText(sheetSel);
 else if(b.kind==dice::Ability&&b.formula[0])detail=b.formula;
 if(!detail.length())text(8,215,t("V: value   E: edit the block   A: new block","V: значение   E: изменить блок   A: новый блок"),faint,small);
 if(detail.length()){detail.replace("\n"," ");text(8,215,fit(detail,b.kind==dice::Ability&&b.noteLen?250:304,small),faint,small);if(b.kind==dice::Ability&&b.noteLen)textRight(312,215,t("I: more","I: далее"),accent,small);}
 if(sheetNote){panel(10,48,300,160,cardHi,10);d.drawRoundRect(10,48,300,160,10,line);text(24,70,fit(b.name,270,bold),ink,bold);
  String rows[9];unsigned k=wrap(dicer.noteText(sheetSel),rows,9,272,small);for(unsigned j=0;j<k;j++)text(24,86+j*12,rows[j],dim,small);}
 else drawSheetRoll();
}
// The block builder (E on a block, A for a new one): its fields as rows. Left/right change the kinds, widths,
// colours, shapes, rules and the position at once; OK types names, codes, numbers, formulas and descriptions
// into the edit dialog; the device checks them as for the web page (setField).
bool diceConfirm();void diceEdit(DiceEdit kind,const String& value);
enum BlockField:uint8_t {FieldKind,FieldName,FieldCode,FieldWidth,FieldColor,FieldShape,FieldRule,FieldValue,FieldMax,FieldFormula,FieldUse,FieldSpend,FieldNote,FieldMove,FieldCount};
const char* const blockFieldKeys[FieldCount]={"kind","name","code","width","color","shape","rule","value","max","formula","use","spend","note",""};
unsigned blockFields(uint8_t kind,uint8_t* out){
 unsigned n=0;auto add=[&](uint8_t f){out[n++]=f;};add(FieldKind);add(FieldName);
 switch(kind){
 case dice::Stat:add(FieldCode);add(FieldWidth);add(FieldColor);add(FieldShape);add(FieldRule);add(FieldValue);add(FieldFormula);break;
 case dice::Track:add(FieldCode);add(FieldWidth);add(FieldColor);add(FieldValue);add(FieldMax);add(FieldFormula);break;
 case dice::Pips:add(FieldCode);add(FieldWidth);add(FieldColor);add(FieldValue);add(FieldMax);break;
 case dice::Ability:add(FieldWidth);add(FieldColor);add(FieldFormula);add(FieldUse);add(FieldSpend);add(FieldNote);break;
 case dice::Note:add(FieldWidth);add(FieldColor);add(FieldNote);break;
 default:break;}
 add(FieldMove);return n;
}
String kindLabel(uint8_t k){
 switch(k){case dice::Stat:return t("Stat","Стат");case dice::Track:return t("Track","Шкала");case dice::Pips:return t("Marks","Отметки");case dice::Ability:return t("Ability","Способность");case dice::Note:return t("Note","Заметка");}
 return t("Header","Заголовок");
}
String ruleLabel(uint8_t r){return r==dice::Dnd?t("D&D: modifier","D&D: модификатор"):r==dice::Bonus?t("Bonus to d20","Бонус к d20"):r==dice::Ret?t("R.E.T.: die by level","Р.Э.Т.: куб уровня"):t("Plain number","Обычное число");}
String fieldLabel(uint8_t f,uint8_t kind){
 switch(f){
 case FieldKind:return t("Kind","Тип");case FieldName:return t("Name","Название");case FieldCode:return t("Code for formulas","Код для формул");
 case FieldWidth:return t("Width","Ширина");case FieldColor:return t("Colour","Цвет");case FieldShape:return t("Shape","Фигура");case FieldRule:return t("Rule","Правило");
 case FieldValue:return kind==dice::Pips?t("Marked","Отмечено"):kind==dice::Track?t("Now","Сейчас"):t("Value","Значение");
 case FieldMax:return kind==dice::Pips?t("Total (1-12)","Всего (1-12)"):t("Maximum (0: none)","Максимум (0 - нет)");
 case FieldFormula:return kind==dice::Ability?t("Roll","Бросок"):kind==dice::Track?t("Or the maximum by a formula","Или максимум по формуле"):t("Or compute","Или вычислять");
 case FieldUse:return t("Takes from the block (code)","Тратит из блока (код)");case FieldSpend:return t("How many","Сколько");
 case FieldNote:return t("Description","Описание");}
 return t("Position","Место");
}
String fieldText(uint8_t f,unsigned i){
 const dice::Block& b=dicer.sheet.block[i];
 switch(f){
 case FieldKind:return kindLabel(b.kind);case FieldName:return b.name;case FieldCode:return b.code[0]?String("@")+b.code:String("-");
 case FieldWidth:{const char* w[]={"1/6","1/4","1/3","1/2"};return b.width<dice::Whole?String(w[b.width]):t("whole row","вся строка");}
 case FieldRule:return ruleLabel(b.rule);case FieldValue:return String(b.value);case FieldMax:return String(b.max);
 case FieldFormula:return b.formula[0]?String(b.formula):String("-");case FieldUse:return b.use[0]?String(b.use):String("-");case FieldSpend:return String(b.spend);
 case FieldNote:{String v=dicer.noteText(i);v.replace("\n"," ");return v.length()?v:String("-");}
 case FieldMove:return String(i+1)+" / "+String(dicer.sheet.count);
 default:return String();}
}
bool fieldCycles(uint8_t f){return f==FieldKind||f==FieldWidth||f==FieldColor||f==FieldShape||f==FieldRule||f==FieldMove;}
bool fieldRaw(uint8_t f){return f!=FieldName&&f!=FieldNote;}
uint8_t blockFieldNow(){uint8_t fs[FieldCount];unsigned n=blockFields(dicer.sheet.block[sheetSel].kind,fs);blockField=constrain(blockField,0,int(n)-1);return fs[blockField];}
void drawDiceBlock(){
 auto& d=g();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 if(!dicer.sheet.count){diceSheet=DiceMain;return;}sheetSel=constrain(sheetSel,0,int(dicer.sheet.count)-1);
 const dice::Block& b=dicer.sheet.block[sheetSel];uint16_t hue=diceColor(b.color);
 d.fillCircle(22,62,5,hue);text(32,66,fit(t("Block ","Блок ")+String(sheetSel+1)+" · "+b.name,270,bold),ink,bold);
 uint8_t fs[FieldCount];unsigned n=blockFields(b.kind,fs);blockField=constrain(blockField,0,int(n)-1);static int first=0;if(blockField<first)first=blockField;if(blockField>=first+6)first=blockField-5;first=constrain(first,0,max(0,int(n)-6)); // scrolls only past an edge: a second tap hits the same row
 for(int k=first;k<int(n)&&k<first+6;k++){int y=74+(k-first)*19;bool focus=k==blockField;uint8_t f=fs[k];
  panel(14,y,292,17,focus?card:cardHi,5);
  String label=fieldLabel(f,b.kind);text(22,y+12,fit(label,128,small),focus?ink:dim,small);
  int vx=154;if(f==FieldColor){d.fillRoundRect(vx,y+4,24,9,3,hue);text(vx+30,y+12,String(b.color+1)+"/15",faint,small);}
  else if(f==FieldShape){drawStatShape(d,b.shape,vx+8,y+8,6,mix(dice::colors[b.color%dice::Colors],0x1c2835,.6f),hue);}
  else{String v=fieldText(f,sheetSel);text(vx,y+12,fit(v,fieldCycles(f)?124:146,small),f==FieldFormula||f==FieldCode||f==FieldUse?accent:ink,small);}
  if(fieldCycles(f)&&focus){textRight(302,y+12,"< >",accent,small);}
  diceSpot(14,y,292,17,TapField,k);}
 scrollbar(first,6,n,74,112);
 uint8_t f=fs[blockField];String hint=fieldCycles(f)?t("< >: change   DEL twice: delete   A: new block","< >: изменить   DEL дважды: удалить   A: новый блок"):t("OK: type   DEL twice: delete   A: new block","OK: ввести   DEL дважды: удалить   A: новый блок");
 if(!dicer.derivedOk(sheetSel)||(b.kind==dice::Ability&&b.formula[0]&&!dicer.rollFormula(sheetSel).length()))hint=t("The formula does not compute: check the codes","Формула не считается: проверьте коды");
 text(18,208,fit(hint,288,small),hint.startsWith("<")||hint.startsWith("OK")?faint:bad,small);
}
// A new block after the chosen one (a stat; the kind is the first row).
void diceNewBlock(){int at=dicer.sheet.count?sheetSel+1:0;int i=dicer.addBlock(dice::Stat,at);if(i<0){notice(t("64 blocks at most","Не больше 64 блоков"),warn);return;}sheetSel=i;blockField=0;sheetNote=false;diceSheet=DiceBlock;}
void diceFieldChange(uint8_t f,int step){
 const dice::Block& b=dicer.sheet.block[sheetSel];String error;int v=0,top=0;
 switch(f){
 case FieldKind:{int k=(b.kind+step+dice::KindCount)%dice::KindCount;if(dicer.fresh(sheetSel)){dicer.renew(sheetSel,k);return;} // a block not touched yet takes the new kind's defaults
  v=b.kind;top=dice::KindCount;break;}case FieldWidth:v=b.width;top=dice::WidthCount;break;case FieldColor:v=b.color;top=dice::Colors;break;
 case FieldShape:v=b.shape;top=dice::ShapeCount;break;case FieldRule:v=b.rule;top=dice::RuleCount;break;
 case FieldMove:{int to=sheetSel+step;if(to>=0&&to<int(dicer.sheet.count)&&dicer.moveBlock(sheetSel,to))sheetSel=to;return;}
 default:return;}
 if(!dicer.setField(sheetSel,blockFieldKeys[f],String((v+step+top)%top),&error))notice(error,bad);
 if(f==FieldKind){uint8_t fs[FieldCount];unsigned n=blockFields(dicer.sheet.block[sheetSel].kind,fs);blockField=min<int>(blockField,n-1);}
}
bool diceBlockKey(int key){
 if(!dicer.sheet.count){diceSheet=DiceMain;return true;}
 uint8_t fs[FieldCount];unsigned n=blockFields(dicer.sheet.block[sheetSel].kind,fs);blockField=constrain(blockField,0,int(n)-1);uint8_t f=fs[blockField];
 if(key==KeyBack||key=='e'||key=='E'){diceSheet=DiceMain;return true;}
 if(key==KeyUp||key==KeyDown){blockField=(blockField+(key==KeyUp?n-1:1))%n;return true;}
 if(key==KeyLeft||key==KeyRight){diceFieldChange(f,key==KeyLeft?-1:1);return true;}
 if(key=='a'||key=='A'){diceNewBlock();return true;}
 if(key==Erase){if(diceConfirm()){dicer.deleteBlock(sheetSel);sheetSel=max(0,sheetSel-1);diceSheet=DiceMain;notice(t("Block deleted","Блок удалён"),dim);}return true;}
 if(key==Enter||key==KeyHold){if(fieldCycles(f)){diceFieldChange(f,1);return true;}
  String v=f==FieldNote?dicer.noteText(sheetSel):f==FieldFormula?String(dicer.sheet.block[sheetSel].formula):f==FieldName?String(dicer.sheet.block[sheetSel].name):f==FieldCode?String(dicer.sheet.block[sheetSel].code):f==FieldUse?String(dicer.sheet.block[sheetSel].use):fieldText(f,sheetSel);
  v.replace("\n"," ");diceEdit(EditField,v);return true;}
 return true;
}
// The saved rolls (the sheet's abilities with a formula) over the page.
void drawDiceSaved(){
 auto& d=g();unsigned count=dicer.savedCount();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,t("Saved rolls · ","Сохранённые броски · ")+dicer.heroName(dicer.s.character),ink,bold);
 if(!count){icon(IcDice,160,112,14,faint,cardHi);textCenter(160,146,t("No saved rolls","Сохранённых бросков нет"),dim);textCenter(160,162,t("A: save a formula with a name","A: сохранить формулу с именем"),faint,small);}
 diceSel=count?constrain(diceSel,0,int(count)-1):0;int first=max(0,min(diceSel-2,int(count)-5));
 for(int i=first;i<int(count)&&i<first+5;i++){int y=72+(i-first)*24;bool focus=i==diceSel;const dice::Block& r=dicer.sheet.block[dicer.savedBlock(i)];
  panel(14,y,292,22,focus?card:cardHi,6);d.fillCircle(28,y+11,5,diceColor(r.color));
  String f=dice::readable(dicer.rollFormula(dicer.savedBlock(i)).c_str());if(!f.length())f="?";int fw=measure(f,small);text(40,y+15,fit(r.name,250-fw),focus?ink:dim,focus?bold:body);textRight(300,y+15,f,focus?accent:faint,small);diceSpot(14,y,292,22,TapSaved,i);}
 scrollbar(first,5,count,72,118);
 text(18,208,t("F: formula  N: name  C: colour  DEL: delete","F: формула  N: имя  C: цвет  DEL: удалить"),faint,small);
}
// Characters: the general set first, each with its sheet; a sheet a contact sent waits on top.
void drawDiceHeroes(){
 auto& d=g();auto& s=dicer.s;panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,t("Characters","Персонажи"),ink,bold);textRight(302,64,String(s.characters)+"/"+String(dice::Characters),faint,small);
 diceSel=constrain(diceSel,0,int(s.characters)-1);int y0=72;
 if(dicer.offered){panel(14,y0,292,20,rgb(0x3a2f12),6);text(22,y0+14,fit(t("O: sheet from ","O: лист от ")+dicer.offer.from+": "+dicer.offer.name,276,small),warn,small);y0+=22;}
 for(unsigned i=0;i<s.characters;i++){int y=y0+i*22;bool focus=int(i)==diceSel;const dice::Character& c=s.list[i];
  panel(14,y,292,20,focus?card:cardHi,6);
  if(i){d.fillCircle(28,y+10,7,avatarHues[i%8]);textCenter(28,y+14,initial(c.name),ink,small);}else{d.fillCircle(28,y+10,7,line);icon(IcPerson,28,y+10,4,dim,line);}
  text(42,y+14,fit(dicer.heroName(i),170,focus?bold:body),focus?ink:dim,focus?bold:body);
  if(i==s.character)textRight(300,y+14,count(dicer.sheet.count,"block","blocks","блок","блока","блоков")+" · "+t("now","сейчас"),ok,small);diceSpot(14,y,292,20,TapHeroRow,i);}
 text(18,208,t("A: new  N: name  S: send  DEL: delete","A: новый  N: имя  S: отправить  DEL: удалить"),faint,small);
}
// A new character: an empty sheet or a template.
String diceTemplateName(int i){return i==dice::DndTemplate?String("D&D 5e"):i==dice::RetTemplate?t("R.E.T. (Mind, Emotions, Body)","Р.Э.Т. (Разум, Эмоции, Тело)"):t("Empty sheet","Пустой лист");}
void drawDiceTemplates(){
 auto& d=g();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,t("Sheet of ","Лист: ")+diceDraft,ink,bold);diceSel=constrain(diceSel,0,int(dice::TemplateCount)-1);
 for(int i=0;i<dice::TemplateCount;i++){int y=74+i*34;bool focus=i==diceSel;panel(14,y,292,30,focus?card:cardHi,6);if(focus)ring(14,y,292,30,6,accent);
  text(24,y+14,diceTemplateName(i),focus?ink:dim,focus?bold:body);
  String detail=i==dice::DndTemplate?t("6 stats, AC, hit points, 18 skills, slots","6 характеристик, КД, хиты, 18 навыков, ячейки"):i==dice::RetTemplate?t("Stat tree, dice by level, health, sanity, luck","Дерево параметров, кубы по уровню, здоровье, рассудок, удача"):t("One counter; blocks from the web page","Один счётчик; блоки — на веб-странице");
  text(24,y+26,fit(detail,276,small),faint,small);diceSpot(14,y,292,30,TapPick,i);}
}
// Sending the sheet: chat contacts.
unsigned diceContacts(uint64_t* out,unsigned most){unsigned n=0;for(unsigned i=0;i<meshRadio.peerCount&&n<most;i++)if(meshRadio.peers[i].type==1)out[n++]=meshRadio.peers[i].id;return n;}
void drawDiceSend(){
 auto& d=g();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,fit(t("Send the sheet: ","Отправить лист: ")+dicer.heroName(dicer.s.character),284,bold),ink,bold);
 uint64_t ids[24];unsigned n=diceContacts(ids,24);
 if(!n){textCenter(160,130,t("No chat contacts yet","Пока нет контактов-собеседников"),dim);return;}
 diceSel=constrain(diceSel,0,int(n)-1);int first=max(0,min(diceSel-2,int(n)-5));
 for(int i=first;i<int(n)&&i<first+5;i++){int y=72+(i-first)*24;bool focus=i==diceSel;const Peer* p=meshRadio.findContact(ids[i]);
  panel(14,y,292,22,focus?card:cardHi,6);text(24,y+15,fit(p?p->name:"",200,focus?bold:body),focus?ink:dim,focus?bold:body);
  if(p&&p->heard)textRight(300,y+15,p->hops?String(p->hops)+t(" hops"," хоп."):t("direct","напрямую"),faint,small);diceSpot(14,y,292,22,TapPick,i);}
 scrollbar(first,5,n,72,118);
 const auto& o=dicer.out;if(o.parts){String st=o.done?t("Delivered to ","Доставлен: ")+o.name:o.failed?t("Not delivered to ","Не доставлен: ")+o.name:t("Sending: part ","Отправка: часть ")+String(o.part+1)+"/"+String(o.parts);text(18,208,fit(st,284,small),o.failed?bad:o.done?ok:accent,small);}
 else text(18,208,t("Parts go one by one, each with its ACK","Части уходят по одной, каждая с подтверждением"),faint,small);
}
void drawDiceOffer(){
 auto& d=g();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 if(!dicer.offered){textCenter(160,130,t("No sheets sent to you","Вам не присылали листов"),dim);return;}
 icon(IcPerson,40,92,14,warn,cardHi);text(66,84,fit(dicer.offer.name,236,bold),ink,bold);text(66,100,fit(t("from ","от ")+dicer.offer.from,236,small),dim,small);
 String rows[4];unsigned n=wrap(t("A character sheet came over the mesh. Accept it as a new character?","По сети пришёл лист персонажа. Добавить его новым персонажем?"),rows,4,280);
 for(unsigned i=0;i<n;i++)text(20,134+i*15,rows[i],dim);
 if(dicer.s.characters>=dice::Characters)text(20,200,t("6 characters at most: delete one first","Не больше 6 персонажей: сначала удалите одного"),bad,small);
}
void drawDice(){
 diceSpotCount=0;diceTabs();
 switch(diceSheet){
 case DiceSaved:drawDiceSaved();footer({{"OK",t("Roll","Бросить")},{"A",t("Add","Добавить")},{"^v",t("Select","Выбор")},{"BACK",t("Close","Закрыть")}});return;
 case DiceHeroes:drawDiceHeroes();footer({{"OK",t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"S",t("Send","Отправить")},{"BACK",t("Close","Закрыть")}});return;
 case DiceTemplates:drawDiceTemplates();footer({{"OK",t("Create","Создать")},{"^v",t("Select","Выбор")},{"BACK",t("Cancel","Отмена")}});return;
 case DiceSend:drawDiceSend();footer({{"OK",t("Send","Отправить")},{"^v",t("Select","Выбор")},{"BACK",t("Close","Закрыть")}});return;
 case DiceBlock:drawDiceBlock();footer({{"^v",t("Field","Поле")},{"<>",t("Change","Изменить")},{"OK",t("Type","Ввести")},{"BACK",t("Done","Готово")}});return;
 case DiceOffer:drawDiceOffer();footer({{"OK",t("Accept","Принять")},{"DEL",t("Decline","Отклонить")},{"BACK",t("Later","Позже")}});return;
 default:break;
 }
 switch(dicer.s.mode){
 case dice::Rpg:drawDiceRpg();footer({{"OK",t("Roll","Бросить")},{"S",t("Saved","Броски")},{"E",t("Formula","Формула")},{"M",t("Mode","Режим")}});break;
 case dice::Grid:drawDiceGrid();footer({{"OK",t("Roll","Бросить")},{"<>",t("Count","Число")},{"M",t("Mode","Режим")},{"P",t("Character","Персонаж")}});break;
 default:{drawDiceSheet();String act=dicer.sheet.count?dicer.actName(sheetSel):String();
  if(act.length())footer({{"OK",act},{"+/-","±1"},{"E",t("Edit","Изменить")},{"P",t("Character","Персонаж")}});
  else footer({{"<>",t("Select","Выбор")},{"+/-","±1"},{"E",t("Edit","Изменить")},{"P",t("Character","Персонаж")}});break;}
 }
}
// The edit dialog (drawEditing): its title, limit and hint; formulas take the Latin keys as they are.
uint8_t blockFieldNow();bool fieldRaw(uint8_t f);String fieldLabel(uint8_t f,uint8_t kind);
bool diceEditRaw(){return diceEditing==EditQuick||diceEditing==EditNewFormula||diceEditing==EditFormula||diceEditing==EditValue||(diceEditing==EditField&&fieldRaw(blockFieldNow()));}
String diceEditTitle(){
 switch(diceEditing){case EditQuick:return t("Formula to roll","Формула броска");case EditNewFormula:return t("New roll: formula","Новый бросок: формула");case EditNewName:return t("New roll: name","Новый бросок: имя");
 case EditField:return fit(fieldLabel(blockFieldNow(),dicer.sheet.block[sheetSel].kind)+": "+dicer.sheet.block[sheetSel].name,268,bold);
 case EditFormula:return t("Formula","Формула");case EditName:return t("Roll name","Имя броска");case EditValue:return fit(t("Value: ","Значение: ")+(dicer.sheet.count?String(dicer.sheet.block[sheetSel].name):String()),268,bold);case EditHeroNew:return t("New character","Новый персонаж");}
 return t("Character name","Имя персонажа");
}
unsigned diceEditLimit(){
 if(diceEditing==EditField){uint8_t f=blockFieldNow();return f==FieldName?dice::BlockName:f==FieldCode||f==FieldUse?dice::CodeBytes:f==FieldFormula?dice::BlockFormula:f==FieldNote?dice::NoteBytes:6;}
 return diceEditing==EditValue?6:diceEditRaw()?dice::FormulaBytes:diceEditing==EditName||diceEditing==EditNewName?dice::BlockName:dice::NameBytes;}
String diceEditOk(){return diceEditing==EditQuick?t("OK: roll   BACK: cancel","OK: бросить   BACK: отменить"):diceEditing==EditNewFormula||diceEditing==EditHeroNew?t("OK: next   BACK: cancel","OK: далее   BACK: отменить"):t("OK: save   BACK: cancel","OK: сохранить   BACK: отменить");}
String diceEditHint(){
 if(diceEditing==EditField){uint8_t f=blockFieldNow();uint8_t k=dicer.sheet.block[sheetSel].kind;
  if(f==FieldFormula)return k==dice::Ability?t("d20+@dex+@pb, 2d6+@str, r3+@fine!","d20+@dex+@pb, 2d6+@str, r3+@fine!"):t("10+@dex, (@a+@b)/2, @body*2; empty: none","10+@dex, (@a+@b)/2, @body*2; пусто - нет");
  if(f==FieldCode||f==FieldUse)return t("Latin: dex, hp, mana; empty: none","Латиница: dex, hp, mana; пусто - нет");
  return String();}
 return diceEditing==EditValue?t("23 sets it, +5 or -7 changes it","23 - задать, +5 или -7 - изменить"):diceEditRaw()?t("2d6+1, d20, 4d6-2, d%, d66, r4","2d6+1, d20, 4к6-2, d%, d66, r4"):String();}
void diceEditSave(){
 auto bad1=[&](const String& m){notice(m,bad);};String formulaError=t("Formula: 2d6+1, d20, d%, d66","Формула: 2d6+1, d20, d%, d66"),nameError=t("Name: 1-23 UTF-8 bytes","Имя: 1-23 байта UTF-8");
 unsigned count=dicer.savedCount();int block=dicer.savedBlock(diceSel);
 switch(diceEditing){
 case EditQuick:if(dicer.roll(edit)){editing=false;diceRollAt=millis();}else bad1(formulaError);return;
 case EditNewFormula:if(!dice::Dicer::valid(edit)){bad1(formulaError);return;}diceDraft=dice::Dicer::clean(edit);diceEditing=EditNewName;edit="";return;
 case EditNewName:if(dicer.addRoll(diceDraft,edit)){editing=false;diceSel=dicer.savedCount()-1;notice(t("Roll saved","Бросок сохранён"),ok);}else bad1(dicer.sheet.count>=dice::Blocks?t("64 blocks at most","Не больше 64 блоков"):nameError);return;
 case EditFormula:if(diceSel<int(count)&&dicer.editRoll(diceSel,edit,dicer.sheet.block[block].name)){editing=false;notice(t("Formula saved","Формула сохранена"),ok);}else bad1(formulaError);return;
 case EditName:if(diceSel<int(count)&&dicer.editRoll(diceSel,dicer.sheet.block[block].formula,edit)){editing=false;notice(t("Name saved","Имя сохранено"),ok);}else bad1(nameError);return;
 case EditValue:{String v=edit;v.trim();char* end;long n=strtol(v.c_str(),&end,10);bool rel=v.startsWith("+")||v.startsWith("-");
  if(!v.length()||*end||!(rel?dicer.change(sheetSel,n):dicer.setValue(sheetSel,n))){bad1(t("A number: 23, +5, -7","Число: 23, +5, -7"));return;}editing=false;return;}
 case EditField:{uint8_t f=blockFieldNow();String error;if(!dicer.setField(sheetSel,blockFieldKeys[f],edit,&error)){bad1(error);return;}editing=false;return;}
 case EditHeroNew:{String v=edit;v.trim();if(!v.length()||v.length()>dice::NameBytes){bad1(nameError);return;}diceDraft=v;editing=false;diceSheet=DiceTemplates;diceSel=0;return;}
 default:if(dicer.renameCharacter(edit)){editing=false;notice(t("Name saved","Имя сохранено"),ok);}else bad1(nameError);return;
 }
}
void diceEdit(DiceEdit kind,const String& value){diceEditing=kind;editing=true;edit=value;}
// DEL twice within five seconds deletes.
bool diceConfirm(){if(diceArmed&&millis()-diceArmed<5000){diceArmed=0;return true;}diceArmed=millis();notice(t("DEL again: delete","Ещё раз DEL: удалить"),warn);return false;}
void diceRolled(bool done){if(done)diceRollAt=millis();}
bool diceRpgKey(int key){
 auto& s=dicer.s;
 if(key==Enter||key==KeyHold){diceRolled(dicer.rollPool());return true;}
 if(key==KeyLeft||key==KeyRight){dicer.setPool(s.count,(s.type+(key==KeyLeft?dice::TypeCount-1:1))%dice::TypeCount,s.mod);return true;}
 if(key==KeyUp||key==KeyDown){dicer.setPool(s.count+(key==KeyUp?1:-1),s.type,s.mod);return true;}
 if(key=='+'||key=='='){dicer.setPool(s.count,s.type,s.mod+1);return true;}
 if(key=='-'||key=='_'){dicer.setPool(s.count,s.type,s.mod-1);return true;}
 if(key=='r'||key=='R'){dicer.setPool(1,s.type,0);return true;}
 if(key=='s'||key=='S'){diceSheet=DiceSaved;diceSel=0;return true;}
 if(key=='e'||key=='E'){diceEdit(EditQuick,dicer.poolText());return true;}
 if(key=='x'||key=='X'){dicer.clearResults();return true;}
 return key!=KeyBack;
}
bool diceGridKey(int key){
 auto& s=dicer.s;
 if(key==Enter||key==KeyHold){if(!s.gridChosen){notice(t("Choose dice: arrows","Выберите кости стрелками"),warn);return true;}dicer.gridRoll();diceRollAt=millis();return true;}
 if(key==KeyLeft||key==KeyRight){dicer.gridChoose(int(s.gridChosen)+(key==KeyLeft?-1:1));return true;}
 if(key==KeyUp||key==KeyDown){dicer.gridChoose(int(s.gridChosen)+(key==KeyUp?-10:10));return true;}
 if(key=='a'||key=='A'){if(s.gridSize>=dice::GridMax)notice(t("50 dice at most","Не больше 50 костей"),warn);else dicer.gridAddRow();return true;}
 if(key=='r'||key=='R'){dicer.gridReset();return true;}
 if(key=='d'||key=='D'){dicer.gridSetType((s.gridType+1)%dice::TypeCount);return true;}
 if(key=='t'||key=='T'){dicer.gridSetThreshold(!s.thresholdOn,s.threshold);return true;}
 if(key=='+'||key=='='||key=='-'||key=='_'){dicer.gridSetThreshold(true,int(s.threshold)+(key=='+'||key=='='?1:-1));return true;}
 if(key=='p'||key=='P'){diceSheet=DiceHeroes;diceSel=s.character;return true;}
 return key!=KeyBack;
}
// The sheet: left/right go block by block, up/down to the nearest block of the row above or below.
void sheetMove(int key){
 int n=dicer.sheet.count;if(!n)return;sheetLayout();sheetNote=false;
 if(key==KeyLeft||key==KeyRight){sheetSel=(sheetSel+(key==KeyLeft?n-1:1))%n;return;}
 const SheetBox& at=sheetBoxes[sheetSel];int cx=at.x+at.w/2,best=-1,bestRow=0,bestDx=0;
 for(int i=0;i<n;i++){const SheetBox& b=sheetBoxes[i];if(key==KeyUp?b.y>=at.y:b.y<=at.y)continue;
  int row=abs(b.y-at.y),dx=abs(b.x+b.w/2-cx);if(best<0||row<bestRow||(row==bestRow&&dx<bestDx)){best=i;bestRow=row;bestDx=dx;}}
 if(best>=0)sheetSel=best;else sheetSel=key==KeyUp?0:n-1;
}
bool diceSheetKey(int key){
 if(key=='a'||key=='A'){diceNewBlock();return true;}
 int n=dicer.sheet.count;if(!n)return key!=KeyBack;sheetSel=constrain(sheetSel,0,n-1);
 if(sheetNote&&(key==KeyBack||key=='i'||key=='I'||key==Enter)){sheetNote=false;return true;}
 if(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown){sheetMove(key);return true;}
 if(key==Enter||key==KeyHold){uint32_t before=dicer.rolls;if(dicer.act(sheetSel)){if(dicer.rolls!=before){diceRollAt=sheetRollAt=millis();}}else if(dicer.why.length())notice(dicer.why,warn);
  else if(dicer.sheet.block[sheetSel].noteLen)sheetNote=true;return true;}
 if(key=='+'||key=='='||key=='-'||key=='_'){if(!dicer.change(sheetSel,key=='+'||key=='='?1:-1))notice(dicer.sheet.block[sheetSel].formula[0]?t("Computed by its formula","Считается по формуле"):t("No value here","Здесь нет значения"),warn);return true;}
 if(key=='v'||key=='V'){const dice::Block& b=dicer.sheet.block[sheetSel];if((b.kind==dice::Stat&&!b.formula[0])||b.kind==dice::Track||b.kind==dice::Pips)diceEdit(EditValue,String(b.value));else notice(t("No value here","Здесь нет значения"),warn);return true;}
 if(key=='i'||key=='I'){sheetNote=!sheetNote&&dicer.sheet.block[sheetSel].noteLen;return true;}
 if(key=='e'||key=='E'){diceSheet=DiceBlock;blockField=0;sheetNote=false;return true;}
 return key!=KeyBack;
}
bool diceSavedKey(int key){
 int n=dicer.savedCount();int block=dicer.savedBlock(diceSel);
 if(key==KeyBack||key=='s'||key=='S'){diceSheet=DiceMain;return true;}
 if(key=='a'||key=='A'){if(dicer.sheet.count>=dice::Blocks)notice(t("64 blocks at most","Не больше 64 блоков"),warn);else diceEdit(EditNewFormula,dicer.poolText());return true;}
 if(!n)return true;
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?n-1:1))%n;return true;}
 if(key==Enter||key==KeyHold){if(dicer.rollSaved(diceSel)){diceRollAt=millis();diceSheet=DiceMain;}return true;}
 if(key=='f'||key=='F'){diceEdit(EditFormula,dicer.sheet.block[block].formula);return true;}
 if(key=='n'||key=='N'){diceEdit(EditName,dicer.sheet.block[block].name);return true;}
 if(key=='c'||key=='C'){dicer.colorRoll(diceSel,(dicer.sheet.block[block].color+1)%dice::Colors);return true;}
 if(key==Erase){if(diceConfirm()){dicer.deleteRoll(diceSel);diceSel=max(0,diceSel-1);notice(t("Roll deleted","Бросок удалён"),dim);}return true;}
 return true;
}
bool diceHeroKey(int key){
 auto& s=dicer.s;int n=s.characters;
 if(key==KeyBack){diceSheet=DiceMain;return true;}
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?n-1:1))%n;return true;}
 if(key==Enter||key==KeyHold){dicer.useCharacter(diceSel);diceSheet=DiceMain;sheetSel=sheetTop=0;return true;}
 if(key=='a'||key=='A'){if(n>=int(dice::Characters))notice(t("6 characters at most","Не больше 6 персонажей"),warn);else diceEdit(EditHeroNew,"");return true;}
 if(key=='n'||key=='N'){dicer.useCharacter(diceSel);diceEdit(EditHeroName,s.list[diceSel].name);return true;}
 if(key=='s'||key=='S'){dicer.useCharacter(diceSel);diceSheet=DiceSend;diceSel=0;return true;}
 if(key=='o'||key=='O'){diceSheet=DiceOffer;return true;}
 if(key==Erase){if(!diceSel){notice(t("The general set stays","Общий набор не удаляется"),warn);return true;}
  if(diceConfirm()){dicer.useCharacter(diceSel);dicer.deleteCharacter();diceSel=0;notice(t("Character deleted","Персонаж удалён"),dim);}return true;}
 return true;
}
bool diceTemplateKey(int key){
 if(key==KeyBack){diceSheet=DiceHeroes;return true;}
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?dice::TemplateCount-1:1))%dice::TemplateCount;return true;}
 if(key==Enter||key==KeyHold){if(dicer.addCharacter(diceDraft,diceSel)){dicer.setMode(dice::Sheet);diceSheet=DiceMain;sheetSel=sheetTop=0;notice(dicer.heroName(dicer.s.character),ok);}else notice(t("Name: 1-23 UTF-8 bytes","Имя: 1-23 байта UTF-8"),bad);return true;}
 return true;
}
bool diceSendKey(int key){
 uint64_t ids[24];int n=diceContacts(ids,24);
 if(key==KeyBack){diceSheet=DiceHeroes;diceSel=dicer.s.character;return true;}
 if(!n)return true;
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?n-1:1))%n;return true;}
 if(key==Enter||key==KeyHold){String error;if(!dicer.send(ids[constrain(diceSel,0,n-1)],&error))notice(error,bad);return true;}
 return true;
}
bool diceOfferKey(int key){
 if(key==KeyBack){diceSheet=DiceMain;return true;}
 if(!dicer.offered)return true;
 if(key==Enter||key==KeyHold){if(dicer.acceptOffer()){dicer.setMode(dice::Sheet);diceSheet=DiceMain;sheetSel=sheetTop=0;notice(dicer.heroName(dicer.s.character),ok);}else notice(dicer.why,bad);return true;}
 if(key==Erase){dicer.declineOffer();diceSheet=DiceMain;notice(t("Sheet declined","Лист отклонён"),dim);return true;}
 return true;
}
bool diceKey(int key){
 if(key!=Erase)diceArmed=0;
 if(key=='m'||key=='M'||(key>='1'&&key<='3'&&diceSheet==DiceMain)){dicer.setMode(key>='1'&&key<='3'?key-'1':(dicer.s.mode+1)%dice::ModeCount);diceSheet=DiceMain;sheetNote=false;return true;}
 if((key=='p'||key=='P')&&diceSheet!=DiceSaved){diceSheet=diceSheet==DiceHeroes?DiceMain:DiceHeroes;diceSel=dicer.s.character;return true;}
 switch(diceSheet){
 case DiceHeroes:return diceHeroKey(key);case DiceSaved:return diceSavedKey(key);case DiceTemplates:return diceTemplateKey(key);
 case DiceSend:return diceSendKey(key);case DiceOffer:return diceOfferKey(key);case DiceBlock:return diceBlockKey(key);default:break;}
 switch(dicer.s.mode){case dice::Rpg:return diceRpgKey(key);case dice::Grid:return diceGridKey(key);default:return diceSheetKey(key);}
}
// A tap: tabs, the character, the dice, grid cells, sheet blocks, list rows.
bool diceTap(int x,int y){
 for(unsigned i=0;i<diceSpotCount;i++){const DiceSpot& v=diceSpots[i];if(x<v.x||x>=v.x+v.w||y<v.y||y>=v.y+v.h)continue;
  switch(v.act){
  case TapTab:dicer.setMode(v.arg);diceSheet=DiceMain;break;
  case TapHero:diceSheet=diceSheet==DiceHeroes?DiceMain:DiceHeroes;diceSel=dicer.s.character;break;
  case TapType:dicer.setPool(dicer.s.count,v.arg,dicer.s.mod);break;
  case TapRoll:diceRolled(dicer.rollPool());break;
  case TapCell:dicer.gridChoose(int(dicer.s.gridChosen)==v.arg+1?v.arg:v.arg+1);break; // a tap on the last chosen die unchooses it
  case TapAdd:uiKey('a');return true;
  case TapBlock:if(sheetSel==v.arg)uiKey(Enter);else{sheetSel=v.arg;sheetNote=false;}break;
  case TapSaved:case TapHeroRow:case TapPick:if(diceSel==v.arg)uiKey(Enter);else diceSel=v.arg;break;
  case TapField:if(blockField==v.arg)uiKey(Enter);else blockField=v.arg;break;
  }
  dirty=true;lastInput=millis();return true;}
 return false;
}
