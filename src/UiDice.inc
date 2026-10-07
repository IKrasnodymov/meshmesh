// The dice page of the 320x240 screens (included by Ui.cpp; Dice.h keeps the rolls): DIC3R's three modes as
// tabs. RPG: the last roll drawn as dice (d4 a triangle, d6 with pips, d8 a diamond, d10 a kite, d12 a
// pentagon, d20 a hexagon), its sum, the pool and the dice to choose; S opens the saved rolls, E rolls a typed
// formula. Warhammer: a grid of up to 50 dice, the arrows choose how many, OK rolls them, high to low, green
// at or over the threshold. Counters: up to six coloured values, up/down 1, +/- 5. P picks the character
// whose rolls and counters these are, M (or 1-3, or a tap on a tab) changes the mode.
enum DiceSheet:uint8_t {DiceMain,DiceSaved,DiceHeroes};
enum DiceEdit:uint8_t {EditQuick,EditNewFormula,EditNewName,EditFormula,EditName,EditCounter,EditHeroNew,EditHeroName};
uint8_t diceSheet=DiceMain,diceEditing=EditQuick;int diceSel=0,diceCounter=0;String diceDraft;
uint32_t diceArmed=0,diceRollAt=0,diceSeen=0,diceRolls=0;
constexpr uint32_t DiceSpin=450; // ms of tumbling after a roll
// Touch spots of the frame (uiTouch): what a tap there does.
enum DiceTap:uint8_t {TapTab,TapHero,TapType,TapRoll,TapCell,TapCounter,TapSaved,TapHeroRow,TapAdd};
struct DiceSpot{int16_t x,y,w,h;uint8_t act;int16_t arg;};DiceSpot diceSpots[72];unsigned diceSpotCount=0;
void diceSpot(int x,int y,int w,int h,uint8_t act,int arg){if(diceSpotCount<72)diceSpots[diceSpotCount++]={int16_t(x),int16_t(y),int16_t(w),int16_t(h),act,int16_t(arg)};}
uint16_t diceColor(unsigned i){return rgb(dice::colors[i%dice::Colors]);}
bool diceSpinning(){return millis()-diceRollAt<DiceSpin;}
String diceModeName(uint8_t m){return m==dice::Rpg?String("RPG"):m==dice::Grid?String("Warhammer"):t("Counters","Счётчики");}
String diceTileDetail(){
 const dice::Result* r=dicer.last();if(!r)return "RPG · Warhammer";
 return String(r->label[0]?r->label:r->formula)+(r->special?String():" = "+String(r->total));
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
// The tabs and the character on top; touch spots for both.
void diceTabs(){
 int x=8;for(uint8_t m=0;m<dice::ModeCount;m++){String n=diceModeName(m);int w=measure(n)+16;bool on=dicer.s.mode==m;
  panel(x,25,w,18,on?cardHi:card,5);text(x+8,38,n,on?ink:dim);if(on)g().fillRoundRect(x+6,40,w-12,2,1,accent);diceSpot(x,23,w,22,TapTab,m);x+=w+4;}
 String who=fit(dicer.heroName(dicer.s.character),306-x-24,small);int w=measure(who,small);
 icon(IcPerson,306-w-8,33,5,dicer.s.character?violet:faint);textRight(310,37,who,dicer.s.character?ink:dim,small);diceSpot(306-w-16,23,w+20,22,TapHero,0);
}
// RPG: the roll card, the pool and its dice, then the totals before it.
void drawDiceRpg(){
 const dice::Result* r=dicer.last();bool spin=diceSpinning();
 panel(8,48,200,148,card,10);diceSpot(8,48,200,148,TapRoll,0);
 if(!r){icon(IcDice,108,104,20,faint,card);textCenter(108,152,t("OK: roll ","OK: бросить ")+dicer.poolText(),dim);textCenter(108,168,t("E: any formula, 3d8+2","E: любая формула, 3к8+2"),faint,small);}
 else{
  String head=r->label[0]?String(r->label)+" · "+r->formula:String(r->formula);text(16,62,fit(head,184,small),r->label[0]?violet:dim,small);
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
 String saved=String(dicer.hero().rolls);textRight(312,212,t("S: saved ","S: сохранённые ")+saved,dicer.hero().rolls?accent:faint,small);
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
// Counters: three a row; the chosen one ringed. Taps on -5 -1 +1 +5 change it.
void drawDiceTally(){
 auto& d=g();const auto& c=dicer.hero();if(c.counters)diceCounter=constrain(diceCounter,0,int(c.counters)-1);
 for(unsigned i=0;i<=c.counters&&i<dice::Counters;i++){int x=8+(i%3)*102,y=48+(i/3)*82;
  if(i==c.counters){d.drawRoundRect(x,y,98,78,8,line);textCenter(x+49,y+38,"+",dim,big);textCenter(x+49,y+58,t("A: add","A: добавить"),faint,small);diceSpot(x,y,98,78,TapAdd,0);break;}
  const dice::Counter& k=c.counter[i];uint16_t hue=diceColor(k.color);bool focus=int(i)==diceCounter;
  panel(x,y,98,78,focus?cardHi:card,8);d.fillRoundRect(x,y,98,5,2,hue);if(focus)ring(x,y,98,78,8,hue);
  text(x+7,y+16,fit(k.name,84,small),focus?ink:dim,small);
  String v=String(k.value);bool large=measure(v,digits)<=86;textCenter(x+49,large?y+60:y+48,v,ink,large?digits:big);
  const int steps[]={-5,-1,1,5};for(int b=0;b<4;b++){int bx=x+3+b*24;String l=steps[b]>0?"+"+String(steps[b]):String(steps[b]);
   d.fillRoundRect(bx,y+62,22,13,3,focus?line:bg);textCenter(bx+11,y+72,l,steps[b]<0?dim:ink,small);diceSpot(bx,y+58,22,20,TapCounter,i*8+b);}
  diceSpot(x,y,98,58,TapCounter,i*8+7);}
 if(!c.counters)textCenter(160,190,t("Counters: life, wounds, resources","Счётчики: жизни, раны, ресурсы"),faint,small);
 else text(8,217,t("N: name   C: colour   DEL: delete","N: имя   C: цвет   DEL: удалить"),faint,small);
}
// The saved rolls of the character over the page.
void drawDiceSaved(){
 auto& d=g();const auto& c=dicer.hero();panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,t("Saved rolls · ","Сохранённые броски · ")+dicer.heroName(dicer.s.character),ink,bold);
 if(!c.rolls){icon(IcDice,160,112,14,faint,cardHi);textCenter(160,146,t("No saved rolls","Сохранённых бросков нет"),dim);textCenter(160,162,t("A: save a formula with a name","A: сохранить формулу с именем"),faint,small);}
 diceSel=c.rolls?constrain(diceSel,0,int(c.rolls)-1):0;int first=max(0,min(diceSel-2,int(c.rolls)-5));
 for(int i=first;i<int(c.rolls)&&i<first+5;i++){int y=72+(i-first)*24;bool focus=i==diceSel;const dice::SavedRoll& r=c.roll[i];
  panel(14,y,292,22,focus?card:cardHi,6);if(focus)d.fillRoundRect(14,y+5,3,12,1,accent);d.fillCircle(28,y+11,5,diceColor(r.color));
  String f=r.formula;int fw=measure(f,small);text(40,y+15,fit(r.name,250-fw),focus?ink:dim,focus?bold:body);textRight(300,y+15,f,focus?accent:faint,small);diceSpot(14,y,292,22,TapSaved,i);}
 scrollbar(first,5,c.rolls,72,118);
 text(18,208,t("F: formula  N: name  C: colour  DEL: delete","F: формула  N: имя  C: цвет  DEL: удалить"),faint,small);
}
// Characters: the general set first, each with its rolls and counters.
void drawDiceHeroes(){
 auto& d=g();auto& s=dicer.s;panel(8,48,304,168,cardHi,10);d.drawRoundRect(8,48,304,168,10,line);
 text(18,64,t("Characters","Персонажи"),ink,bold);textRight(302,64,String(s.characters)+"/"+String(dice::Characters),faint,small);
 diceSel=constrain(diceSel,0,int(s.characters)-1);
 for(unsigned i=0;i<s.characters;i++){int y=72+i*22;bool focus=int(i)==diceSel;const dice::Character& c=s.list[i];
  panel(14,y,292,20,focus?card:cardHi,6);if(focus)d.fillRoundRect(14,y+4,3,12,1,accent);
  if(i){d.fillCircle(28,y+10,7,avatarHues[i%8]);textCenter(28,y+14,initial(c.name),ink,small);}else{d.fillCircle(28,y+10,7,line);icon(IcPerson,28,y+10,4,dim,line);}
  text(42,y+14,fit(dicer.heroName(i),140,focus?bold:body),focus?ink:dim,focus?bold:body);
  String detail=count(c.rolls,"roll","rolls","бросок","броска","бросков")+" · "+count(c.counters,"counter","counters","счётчик","счётчика","счётчиков");textRight(i==s.character?262:300,y+14,detail,faint,small);
  if(i==s.character)textRight(300,y+14,t("now","сейчас"),ok,small);diceSpot(14,y,292,20,TapHeroRow,i);}
 text(18,208,t("A: new   N: rename   DEL: delete","A: новый   N: имя   DEL: удалить"),faint,small);
}
void drawDice(){
 diceSpotCount=0;diceTabs();
 if(diceSheet==DiceSaved){drawDiceSaved();footer({{"OK",t("Roll","Бросить")},{"A",t("Add","Добавить")},{"^v",t("Select","Выбор")},{"BACK",t("Close","Закрыть")}});return;}
 if(diceSheet==DiceHeroes){drawDiceHeroes();footer({{"OK",t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"BACK",t("Close","Закрыть")}});return;}
 switch(dicer.s.mode){
 case dice::Rpg:drawDiceRpg();footer({{"OK",t("Roll","Бросить")},{"S",t("Saved","Броски")},{"E",t("Formula","Формула")},{"M",t("Mode","Режим")}});break;
 case dice::Grid:drawDiceGrid();footer({{"OK",t("Roll","Бросить")},{"<>",t("Count","Число")},{"M",t("Mode","Режим")},{"P",t("Character","Персонаж")}});break;
 default:drawDiceTally();footer({{"^v","±1"},{"+/-","±5"},{"<>",t("Select","Выбор")},{"M",t("Mode","Режим")}});break;
 }
}
// The edit dialog (drawEditing): its title, limit and hint; formulas take the Latin keys as they are.
bool diceEditRaw(){return diceEditing==EditQuick||diceEditing==EditNewFormula||diceEditing==EditFormula;}
String diceEditTitle(){
 switch(diceEditing){case EditQuick:return t("Formula to roll","Формула броска");case EditNewFormula:return t("New roll: formula","Новый бросок: формула");case EditNewName:return t("New roll: name","Новый бросок: имя");
 case EditFormula:return t("Formula","Формула");case EditName:return t("Roll name","Имя броска");case EditCounter:return t("Counter name","Имя счётчика");case EditHeroNew:return t("New character","Новый персонаж");}
 return t("Character name","Имя персонажа");
}
unsigned diceEditLimit(){return diceEditRaw()?dice::FormulaBytes:diceEditing==EditCounter?dice::CounterNameBytes:dice::NameBytes;}
String diceEditOk(){return diceEditing==EditQuick?t("OK: roll   BACK: cancel","OK: бросить   BACK: отменить"):diceEditing==EditNewFormula?t("OK: next   BACK: cancel","OK: далее   BACK: отменить"):t("OK: save   BACK: cancel","OK: сохранить   BACK: отменить");}
String diceEditHint(){return diceEditRaw()?t("2d6+1, d20, 4d6-2, d%, d66","2d6+1, d20, 4к6-2, d%, d66"):String();}
void diceEditSave(){
 auto bad1=[&](const String& m){notice(m,bad);};String formulaError=t("Formula: 2d6+1, d20, d%, d66","Формула: 2d6+1, d20, d%, d66"),nameError=t("Name: 1-23 UTF-8 bytes","Имя: 1-23 байта UTF-8");
 const auto& c=dicer.hero();
 switch(diceEditing){
 case EditQuick:if(dicer.roll(edit)){editing=false;diceRollAt=millis();}else bad1(formulaError);return;
 case EditNewFormula:if(!dice::Dicer::valid(edit)){bad1(formulaError);return;}diceDraft=dice::Dicer::clean(edit);diceEditing=EditNewName;edit="";return;
 case EditNewName:if(dicer.addRoll(diceDraft,edit)){editing=false;diceSel=c.rolls-1;notice(t("Roll saved","Бросок сохранён"),ok);}else bad1(c.rolls>=dice::Rolls?t("10 saved rolls at most","Не больше 10 бросков"):nameError);return;
 case EditFormula:if(diceSel<int(c.rolls)&&dicer.editRoll(diceSel,edit,c.roll[diceSel].name)){editing=false;notice(t("Formula saved","Формула сохранена"),ok);}else bad1(formulaError);return;
 case EditName:if(diceSel<int(c.rolls)&&dicer.editRoll(diceSel,c.roll[diceSel].formula,edit)){editing=false;notice(t("Name saved","Имя сохранено"),ok);}else bad1(nameError);return;
 case EditCounter:if(dicer.renameCounter(diceCounter,edit)){editing=false;notice(t("Name saved","Имя сохранено"),ok);}else bad1(nameError);return;
 case EditHeroNew:if(dicer.addCharacter(edit)){editing=false;diceSheet=DiceMain;notice(dicer.heroName(dicer.s.character),ok);}else bad1(nameError);return;
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
bool diceTallyKey(int key){
 const auto& c=dicer.hero();int n=c.counters;
 if(key=='a'||key=='A'){if(dicer.addCounter())diceCounter=c.counters-1;else notice(t("6 counters at most","Не больше 6 счётчиков"),warn);return true;}
 if(!n)return key!=KeyBack;
 if(key==KeyLeft||key==KeyRight){diceCounter=(diceCounter+(key==KeyLeft?n-1:1))%n;return true;}
 if(key==KeyUp||key==Enter||key==KeyHold){dicer.changeCounter(diceCounter,1);return true;}
 if(key==KeyDown){dicer.changeCounter(diceCounter,-1);return true;}
 if(key=='+'||key=='='){dicer.changeCounter(diceCounter,5);return true;}
 if(key=='-'||key=='_'){dicer.changeCounter(diceCounter,-5);return true;}
 if(key=='n'||key=='N'){diceEdit(EditCounter,c.counter[diceCounter].name);return true;}
 if(key=='c'||key=='C'){dicer.colorCounter(diceCounter,(c.counter[diceCounter].color+1)%dice::Colors);return true;}
 if(key==Erase){if(diceConfirm()){dicer.deleteCounter(diceCounter);diceCounter=max(0,diceCounter-1);notice(t("Counter deleted","Счётчик удалён"),dim);}return true;}
 return key!=KeyBack;
}
bool diceSavedKey(int key){
 const auto& c=dicer.hero();int n=c.rolls;
 if(key==KeyBack||key=='s'||key=='S'){diceSheet=DiceMain;return true;}
 if(key=='a'||key=='A'){if(n>=int(dice::Rolls))notice(t("10 saved rolls at most","Не больше 10 бросков"),warn);else diceEdit(EditNewFormula,dicer.poolText());return true;}
 if(!n)return true;
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?n-1:1))%n;return true;}
 if(key==Enter||key==KeyHold){if(dicer.rollSaved(diceSel)){diceRollAt=millis();diceSheet=DiceMain;}return true;}
 if(key=='f'||key=='F'){diceEdit(EditFormula,c.roll[diceSel].formula);return true;}
 if(key=='n'||key=='N'){diceEdit(EditName,c.roll[diceSel].name);return true;}
 if(key=='c'||key=='C'){dicer.colorRoll(diceSel,(c.roll[diceSel].color+1)%dice::Colors);return true;}
 if(key==Erase){if(diceConfirm()){dicer.deleteRoll(diceSel);diceSel=max(0,diceSel-1);notice(t("Roll deleted","Бросок удалён"),dim);}return true;}
 return true;
}
bool diceHeroKey(int key){
 auto& s=dicer.s;int n=s.characters;
 if(key==KeyBack){diceSheet=DiceMain;return true;}
 if(key==KeyUp||key==KeyDown){diceSel=(diceSel+(key==KeyUp?n-1:1))%n;return true;}
 if(key==Enter||key==KeyHold){dicer.useCharacter(diceSel);diceSheet=DiceMain;diceCounter=0;return true;}
 if(key=='a'||key=='A'){if(n>=int(dice::Characters))notice(t("6 characters at most","Не больше 6 персонажей"),warn);else diceEdit(EditHeroNew,"");return true;}
 if(key=='n'||key=='N'){dicer.useCharacter(diceSel);diceEdit(EditHeroName,s.list[diceSel].name);return true;}
 if(key==Erase){if(!diceSel){notice(t("The general set stays","Общий набор не удаляется"),warn);return true;}
  if(diceConfirm()){dicer.useCharacter(diceSel);dicer.deleteCharacter();diceSel=0;notice(t("Character deleted","Персонаж удалён"),dim);}return true;}
 return true;
}
bool diceKey(int key){
 if(key!=Erase)diceArmed=0;
 if(key=='m'||key=='M'||(key>='1'&&key<='3'&&diceSheet==DiceMain)){dicer.setMode(key>='1'&&key<='3'?key-'1':(dicer.s.mode+1)%dice::ModeCount);diceSheet=DiceMain;return true;}
 if((key=='p'||key=='P')&&diceSheet!=DiceSaved){diceSheet=diceSheet==DiceHeroes?DiceMain:DiceHeroes;diceSel=dicer.s.character;return true;}
 if(diceSheet==DiceHeroes)return diceHeroKey(key);
 if(diceSheet==DiceSaved)return diceSavedKey(key);
 switch(dicer.s.mode){case dice::Rpg:return diceRpgKey(key);case dice::Grid:return diceGridKey(key);default:return diceTallyKey(key);}
}
// A tap: tabs, the character, the dice, grid cells, counter buttons, list rows.
bool diceTap(int x,int y){
 for(unsigned i=0;i<diceSpotCount;i++){const DiceSpot& v=diceSpots[i];if(x<v.x||x>=v.x+v.w||y<v.y||y>=v.y+v.h)continue;
  switch(v.act){
  case TapTab:dicer.setMode(v.arg);diceSheet=DiceMain;break;
  case TapHero:diceSheet=diceSheet==DiceHeroes?DiceMain:DiceHeroes;diceSel=dicer.s.character;break;
  case TapType:dicer.setPool(dicer.s.count,v.arg,dicer.s.mod);break;
  case TapRoll:diceRolled(dicer.rollPool());break;
  case TapCell:dicer.gridChoose(int(dicer.s.gridChosen)==v.arg+1?v.arg:v.arg+1);break; // a tap on the last chosen die unchooses it
  case TapAdd:uiKey('a');return true;
  case TapCounter:{int i=v.arg/8,b=v.arg%8;diceCounter=i;if(b<4){const int steps[]={-5,-1,1,5};dicer.changeCounter(i,steps[b]);}break;}
  case TapSaved:if(diceSel==v.arg)uiKey(Enter);else diceSel=v.arg;break;
  case TapHeroRow:if(diceSel==v.arg)uiKey(Enter);else diceSel=v.arg;break;
  }
  dirty=true;lastInput=millis();return true;}
 return false;
}
