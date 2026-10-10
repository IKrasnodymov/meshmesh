// The dice page of the one-button and joystick boards (included by UiHeltec.cpp after its actions; Dice.h keeps
// the rolls). Hold (OK on the joystick) opens the menu with the roll first: hold, hold rolls. RPG shows the sum
// and the dice, Warhammer the grid (filled: at or over the threshold), counters one at a time; the joystick's
// up/down change the dice count, the chosen grid dice or the counter. One button counts in its own mode
// ("Count with button"): click +1, hold -1, holding on to 5 s leaves it. Saved rolls, characters and names
// come from the web page or the app.
bool diceRepeatSaved=false;int diceSavedSel=0,diceCounterSel=0;uint32_t diceSeen=0;
String diceTypeName(uint8_t type){return dice::typeNames[type<dice::TypeCount?type:dice::D6];}
String diceModeName(uint8_t m){return m==dice::Rpg?String("RPG"):m==dice::Grid?String("Warhammer"):t("Counters","Счётчики");}
String diceSigned(int v){return v>0?"+"+String(v):String(v);}
unsigned diceActions(Act* out){
 unsigned n=0;const auto& s=dicer.s;const auto& c=dicer.hero();
 switch(s.mode){
 case dice::Rpg:out[n++]=ActDiceRoll;if(c.rolls){out[n++]=ActDiceSaved;if(c.rolls>1)out[n++]=ActDiceNextSaved;}out[n++]=ActDiceCount;out[n++]=ActDiceType;out[n++]=ActDiceMod;break;
 case dice::Grid:out[n++]=ActDiceRoll;out[n++]=ActDiceGridMore;out[n++]=ActDiceGridFive;out[n++]=ActDiceGridType;out[n++]=ActDiceThreshold;if(s.gridSize<dice::GridMax)out[n++]=ActDiceGridRow;break;
 default:if(c.counters){
#if defined(MM_JOYSTICK)
  out[n++]=ActDicePlus1;out[n++]=ActDiceMinus1;
#else
  out[n++]=ActDiceTap;
#endif
  out[n++]=ActDicePlus5;out[n++]=ActDiceMinus5;if(c.counters>1)out[n++]=ActDiceNextCounter;}if(c.counters<dice::Counters)out[n++]=ActDiceAddCounter;break;
 }
 if(s.characters>1)out[n++]=ActDiceHero;out[n++]=ActDiceMode;out[n++]=ActDiceExit;return n;
}
bool diceKeeps(Act a){return a!=ActDiceRoll&&a!=ActDiceSaved&&a!=ActDiceMode&&a!=ActDiceAddCounter&&a!=ActDiceTap&&a!=ActDiceExit;}
const dice::Counter* diceShownCounter(){const auto& c=dicer.hero();if(!c.counters)return nullptr;diceCounterSel=constrain(diceCounterSel,0,int(c.counters)-1);return &c.counter[diceCounterSel];}
// Counting with the button. The hold's -1 comes at 1.2 s, before the button can tell a 5 s hold, so leaving
// puts the value back as it was before that hold.
bool diceTapping=false,diceTapUndo=false;int diceTapBefore=0;
bool diceTapOn(){if(diceTapping&&(page!=DicePage||dicer.s.mode!=dice::Tally||!diceShownCounter()))diceTapping=false;return diceTapping;}
bool diceTapKey(int key){
 if(!diceTapOn())return false;
 const dice::Counter* k=diceShownCounter();
 if(key==13||key==0x82){diceTapUndo=false;dicer.changeCounter(diceCounterSel,1);}
 else if(key==0xa3){diceTapUndo=true;diceTapBefore=k->value;dicer.changeCounter(diceCounterSel,-1);}
 else if(key==0xa4){if(diceTapUndo)dicer.setCounter(diceCounterSel,diceTapBefore);diceTapUndo=diceTapping=false;notice(t("Counting done","Счёт завершён"));}
 return true;
}
String diceActName(Act a){
 const auto& s=dicer.s;const auto& c=dicer.hero();const dice::Counter* k=diceShownCounter();String value=k?" ("+String(k->value)+")":String();
 switch(a){
 case ActDiceExit:return t("Exit dice","Выйти из костей");
 case ActDiceRoll:return t("Roll ","Бросить ")+(s.mode==dice::Grid?String(s.gridChosen)+"x"+diceTypeName(s.gridType):dicer.poolText());
 case ActDiceSaved:{diceSavedSel=c.rolls?diceSavedSel%c.rolls:0;return t("Roll: ","Бросок: ")+String(c.roll[diceSavedSel].name);}
 case ActDiceNextSaved:return t("Next saved roll","Другой бросок");
 case ActDiceCount:return t("Dice: ","Кости: ")+String(s.count);
 case ActDiceType:case ActDiceGridType:return t("Die: ","Кость: ")+diceTypeName(a==ActDiceType?s.type:s.gridType);
 case ActDiceMod:return t("Modifier: ","Модификатор: ")+diceSigned(s.mod);
 case ActDiceHero:return t("Character: ","Персонаж: ")+dicer.heroName(s.character);
 case ActDiceMode:return t("Mode: ","Режим: ")+diceModeName((s.mode+1)%dice::ModeCount);
 case ActDiceGridMore:return t("Dice +1: ","Кости +1: ")+String(s.gridChosen);
 case ActDiceGridFive:return t("Dice +5: ","Кости +5: ")+String(s.gridChosen);
 case ActDiceThreshold:return t("Threshold: ","Порог: ")+(s.thresholdOn?dice::valueText(s.gridType,s.threshold)+"+":t("off","выкл."));
 case ActDiceGridRow:return t("Add 5 dice","Ещё 5 костей");
 case ActDicePlus1:return "+1"+value;case ActDiceMinus1:return "-1"+value;case ActDicePlus5:return "+5"+value;case ActDiceMinus5:return "-5"+value;
 case ActDiceNextCounter:return t("Next counter","Другой счётчик");
 case ActDiceAddCounter:return t("Add counter","Добавить счётчик");
 case ActDiceTap:return t("Count with button","Считать кнопкой")+value;
 default:return "";
 }
}
void diceRun(Act a){
 auto& s=dicer.s;const auto& c=dicer.hero();
 switch(a){
 case ActDiceExit:diceTapping=false;showPage(nextPage(page));break;
 case ActDiceRoll:diceRepeatSaved=false;if(s.mode==dice::Grid){if(!s.gridChosen){notice(t("Choose dice: Dice +1","Выберите: Кости +1"));break;}dicer.gridRoll();}else dicer.rollPool();break;
 case ActDiceSaved:diceRepeatSaved=true;dicer.rollSaved(diceSavedSel);break;
 case ActDiceNextSaved:diceSavedSel=c.rolls?(diceSavedSel+1)%c.rolls:0;break;
 case ActDiceCount:diceRepeatSaved=false;dicer.setPool(s.count>=10?1:s.count+1,s.type,s.mod);break;
 case ActDiceType:diceRepeatSaved=false;dicer.setPool(s.count,(s.type+1)%dice::TypeCount,s.mod);break;
 case ActDiceMod:diceRepeatSaved=false;dicer.setPool(s.count,s.type,s.mod>=10?-5:s.mod+1);break;
 case ActDiceHero:diceRepeatSaved=false;dicer.useCharacter((s.character+1)%s.characters);diceCounterSel=diceSavedSel=0;break;
 case ActDiceMode:dicer.setMode((s.mode+1)%dice::ModeCount);break;
 case ActDiceGridMore:dicer.gridChoose(s.gridChosen>=s.gridSize?1:s.gridChosen+1);break;
 case ActDiceGridFive:dicer.gridChoose(s.gridChosen>=s.gridSize?0:min<int>(s.gridSize,s.gridChosen+5));break;
 case ActDiceGridType:dicer.gridSetType((s.gridType+1)%dice::TypeCount);break;
 case ActDiceThreshold:{unsigned top=dice::faces(s.gridType),step=top<=20?1:top<=66?5:10,next=s.thresholdOn?s.threshold+step:2;if(next>top)dicer.gridSetThreshold(false,s.threshold);else dicer.gridSetThreshold(true,next);break;}
 case ActDiceGridRow:dicer.gridAddRow();break;
 case ActDicePlus1:dicer.changeCounter(diceCounterSel,1);break;case ActDiceMinus1:dicer.changeCounter(diceCounterSel,-1);break;
 case ActDicePlus5:dicer.changeCounter(diceCounterSel,5);break;case ActDiceMinus5:dicer.changeCounter(diceCounterSel,-5);break;
 case ActDiceNextCounter:diceCounterSel=c.counters?(diceCounterSel+1)%c.counters:0;break;
 case ActDiceAddCounter:if(dicer.addCounter())diceCounterSel=c.counters-1;break;
 case ActDiceTap:diceTapping=true;diceTapUndo=false;break;
 default:break;
 }
}
// In the roll modes every short press repeats the chosen pool or preset; the hold opens options.
bool diceRollKey(int key){if(page!=DicePage||dicer.s.mode==dice::Tally||(key!=13&&key!=0x82))return false;if(dicer.s.mode==dice::Grid)dicer.gridRoll();else if(diceRepeatSaved&&dicer.hero().rolls)dicer.rollSaved(diceSavedSel%dicer.hero().rolls);else dicer.rollPool();return true;}
// Joystick up/down inside the page.
void diceScroll(int step){
 auto& s=dicer.s;
 if(s.mode==dice::Rpg)dicer.setPool(s.count-step,s.type,s.mod);
 else if(s.mode==dice::Grid)dicer.gridChoose(int(s.gridChosen)-step);
 else if(diceShownCounter())dicer.changeCounter(diceCounterSel,-step);
}
String diceTitle(){
 const auto& s=dicer.s;
 if(s.mode==dice::Grid){String v=(dicer.gridRolled&&s.thresholdOn?String(dicer.gridSuccesses())+"/":String())+String(s.gridChosen)+"x"+diceTypeName(s.gridType);if(s.thresholdOn)v+=" "+dice::valueText(s.gridType,s.threshold)+"+";return v;}
 if(s.mode==dice::Tally){const auto& c=dicer.hero();return t("Counters","Счётчики")+(c.counters?" "+String(diceCounterSel+1)+"/"+String(c.counters):String());}
 return t("Dice ","Кости ")+dicer.poolText();
}
// ASCII rows of at most `cols` characters, broken after " + " and " - " where it can.
unsigned diceRows(const String& v,String* rows,unsigned most,unsigned cols){
 unsigned n=0,at=0;while(at<v.length()&&n<most){unsigned end=min<unsigned>(v.length(),at+cols);if(end<v.length()){unsigned sp=end;while(sp>at&&v[sp]!=' ')sp--;if(sp>at)end=sp;}
  rows[n++]=v.substring(at,end);at=end;while(at<v.length()&&v[at]==' ')at++;}
 return n;
}
void drawDiceMono(){
 auto& c=*hardware.canvas;const auto& s=dicer.s;
 if(s.mode==dice::Grid){
  for(unsigned i=0;i<s.gridSize;i++){int x=4+(i%10)*12,y=12+(i/10)*9;bool chosen=i<s.gridChosen,rolled=chosen&&dicer.gridRolled,pass=!s.thresholdOn||dicer.grid[i]>=s.threshold;
   if(!chosen){c.drawPixel(x+5,y+4,1);continue;}
   if(rolled&&pass)c.fillRect(x,y,11,8,1);else c.drawRect(x,y,11,8,1);
   if(rolled){String v=dice::valueText(s.gridType,dicer.grid[i]);const uint8_t* f=u8g2_font_4x6_t_cyrillic;say(x+6-width(v,f)/2,y+7,v,f,pass?0:1);}}
  return;}
 if(s.mode==dice::Tally){const dice::Counter* k=diceShownCounter();
  if(!k){say(0,30,t("No counters","Счётчиков нет"));say(0,44,t("Add one in the menu","Добавьте в меню"),small);return;}
  say(0,24,clipped(k->name,21),bold);String v=String(k->value);say(64-width(v,u8g2_font_10x20_t_cyrillic)/2,45,v,u8g2_font_10x20_t_cyrillic);
  const auto& h=dicer.hero();String all;for(unsigned i=0;i<h.counters;i++)all+=(i?" ":"")+String(h.counter[i].value);if(h.counters>1)say(0,55,clipped(all,13),small);return;}
 const dice::Result* r=dicer.last();
 if(!r){say(0,30,t("No rolls yet","Бросков ещё нет"));say(0,44,t("Click to roll","Клик - бросить"),small);return;}
 say(0,20,clipped(r->label[0]?String(r->label)+" "+r->formula:String(r->formula),21),small);
 int x=0;if(!r->special){String total=String(r->total);say(0,44,total,u8g2_font_10x20_t_cyrillic);x=width(total,u8g2_font_10x20_t_cyrillic)+6;}
 String rows[3];unsigned n=diceRows(dicer.resultText(*r,false),rows,r->special?3:2,(128-x)/5);
 for(unsigned i=0;i<n;i++)say(x,(r->special?33:44)+i*9,rows[i],small); // beside the sum, on its baseline
}
