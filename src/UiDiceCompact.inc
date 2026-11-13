// The dice page of the one-button and joystick boards (included by UiHeltec.cpp after its actions; Dice.h keeps
// the rolls). Hold (OK on the joystick) opens the menu with the roll first: hold, hold rolls. RPG shows the sum
// and the dice, Warhammer the grid (filled: at or over the threshold), the sheet one block at a time: a click
// goes to the next block, the menu rolls its check or ability, marks pips and changes values; the joystick's
// up/down change the dice count, the chosen grid dice or the block's value. One button counts in its own mode
// ("Count with button"): click +1, hold -1, holding on to 5 s leaves it. Saved rolls, characters, names and the
// sheet's blocks come from the web page or the app.
bool diceRepeatSaved=false;int diceSavedSel=0,diceBlockSel=0;uint32_t diceSeen=0,diceNotes=0,diceRollsSeen=0,diceSeenAt=0; // diceSeenAt: when the last roll came
String diceTypeName(uint8_t type){return dice::typeNames[type<dice::TypeCount?type:dice::D6];}
String diceModeName(uint8_t m){return m==dice::Rpg?String("RPG"):m==dice::Grid?String("Warhammer"):t("Sheet","Лист");}
String diceSigned(int v){return v>0?"+"+String(v):String(v);}
// The block shown on the sheet page; -1 on an empty sheet. Values: stats without a formula, tracks, pips.
int diceBlock(){int n=dicer.sheet.count;if(!n)return -1;diceBlockSel=constrain(diceBlockSel,0,n-1);return diceBlockSel;}
bool diceChangeable(int i){if(i<0)return false;const dice::Block& b=dicer.sheet.block[i];return (b.kind==dice::Stat&&!b.formula[0])||b.kind==dice::Track||b.kind==dice::Pips;}
String diceRetText(const dice::Result& r){
 if(r.fail)return r.fail==1?t("Critical failure","Критический провал"):t("Failure, d4: ","Провал, d4: ")+String(r.fail);
 return r.successes>1?t("Success x","Успех x")+String(r.successes):t("Success","Успех");
}
unsigned diceActions(Act* out){
 unsigned n=0;const auto& s=dicer.s;unsigned saved=dicer.savedCount();
 if(dicer.offered){out[n++]=ActDiceAccept;out[n++]=ActDiceDecline;}
 switch(s.mode){
 case dice::Rpg:out[n++]=ActDiceRoll;if(saved){out[n++]=ActDiceSaved;if(saved>1)out[n++]=ActDiceNextSaved;}out[n++]=ActDiceCount;out[n++]=ActDiceType;out[n++]=ActDiceMod;break;
 case dice::Grid:out[n++]=ActDiceRoll;out[n++]=ActDiceGridMore;out[n++]=ActDiceGridFive;out[n++]=ActDiceGridType;out[n++]=ActDiceThreshold;if(s.gridSize<dice::GridMax)out[n++]=ActDiceGridRow;break;
 default:{int i=diceBlock();
  if(i>=0&&dicer.actName(i).length())out[n++]=ActDiceAct;
  if(diceChangeable(i)){
#if defined(MM_JOYSTICK)
   out[n++]=ActDicePlus1;out[n++]=ActDiceMinus1;
#else
   out[n++]=ActDiceTap;
#endif
   if(dicer.sheet.block[i].kind!=dice::Pips){out[n++]=ActDicePlus5;out[n++]=ActDiceMinus5;}}
  if(dicer.sheet.count>1){out[n++]=ActDiceNextCounter;out[n++]=ActDicePrevBlock;}
  if(dicer.sheet.count<dice::Blocks)out[n++]=ActDiceAddCounter;break;}
 }
 if(s.characters>1)out[n++]=ActDiceHero;out[n++]=ActDiceMode;out[n++]=ActDiceExit;return n;
}
bool diceKeeps(Act a){return a!=ActDiceRoll&&a!=ActDiceSaved&&a!=ActDiceMode&&a!=ActDiceAddCounter&&a!=ActDiceTap&&a!=ActDiceAct&&a!=ActDiceAccept&&a!=ActDiceDecline&&a!=ActDiceExit;}
// Counting with the button. The hold's -1 comes at 0.8 s, before the button can tell a 5 s hold, so leaving
// puts the value back as it was before that hold.
bool diceTapping=false,diceTapUndo=false;int diceTapBefore=0;
bool diceTapOn(){if(diceTapping&&(page!=DicePage||dicer.s.mode!=dice::Sheet||!diceChangeable(diceBlock())))diceTapping=false;return diceTapping;}
bool diceTapKey(int key){
 if(!diceTapOn())return false;int i=diceBlock();
 if(key==13||key==0x82){diceTapUndo=false;dicer.change(i,1);}
 else if(key==0xa3){diceTapUndo=true;diceTapBefore=dicer.sheet.block[i].value;dicer.change(i,-1);}
 else if(key==0xa4){if(diceTapUndo)dicer.setValue(i,diceTapBefore);diceTapUndo=diceTapping=false;notice(t("Counting done","Счёт завершён"));}
 return true;
}
String diceActName(Act a){
 const auto& s=dicer.s;int i=diceBlock();String value=diceChangeable(i)?" ("+String(dicer.value(i))+")":String();
 switch(a){
 case ActDiceExit:return "< "+t("Exit dice","Выйти из костей");
 case ActDiceRoll:return t("Roll ","Бросить ")+(s.mode==dice::Grid?String(s.gridChosen)+"x"+diceTypeName(s.gridType):dicer.poolText());
 case ActDiceSaved:{unsigned n=dicer.savedCount();diceSavedSel=n?diceSavedSel%n:0;int b=dicer.savedBlock(diceSavedSel);return t("Roll: ","Бросок: ")+String(b>=0?dicer.sheet.block[b].name:"");}
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
 case ActDiceNextCounter:return t("Next block","Следующий блок");
 case ActDicePrevBlock:return t("Previous block","Предыдущий блок");
 case ActDiceAddCounter:return t("Add counter","Добавить счётчик");
 case ActDiceTap:return t("Count with button","Считать кнопкой")+value;
 case ActDiceAct:return i<0?String():dicer.actName(i)+": "+dicer.sheet.block[i].name;
 case ActDiceAccept:return t("Accept sheet: ","Принять лист: ")+dicer.offer.name;
 case ActDiceDecline:return t("Decline the sheet","Отклонить лист");
 default:return "";
 }
}
void diceRun(Act a){
 auto& s=dicer.s;int i=diceBlock();
 switch(a){
 case ActDiceExit:diceTapping=false;showPage(nextPage());break;
 case ActDiceRoll:diceRepeatSaved=false;if(s.mode==dice::Grid){if(!s.gridChosen){notice(t("Choose dice: Dice +1","Выберите: Кости +1"));break;}dicer.gridRoll();}else dicer.rollPool();break;
 case ActDiceSaved:diceRepeatSaved=true;dicer.rollSaved(diceSavedSel);break;
 case ActDiceNextSaved:{unsigned n=dicer.savedCount();diceSavedSel=n?(diceSavedSel+1)%n:0;break;}
 case ActDiceCount:diceRepeatSaved=false;dicer.setPool(s.count>=10?1:s.count+1,s.type,s.mod);break;
 case ActDiceType:diceRepeatSaved=false;dicer.setPool(s.count,(s.type+1)%dice::TypeCount,s.mod);break;
 case ActDiceMod:diceRepeatSaved=false;dicer.setPool(s.count,s.type,s.mod>=10?-5:s.mod+1);break;
 case ActDiceHero:diceRepeatSaved=false;dicer.useCharacter((s.character+1)%s.characters);diceBlockSel=diceSavedSel=0;break;
 case ActDiceMode:dicer.setMode((s.mode+1)%dice::ModeCount);break;
 case ActDiceGridMore:dicer.gridChoose(s.gridChosen>=s.gridSize?1:s.gridChosen+1);break;
 case ActDiceGridFive:dicer.gridChoose(s.gridChosen>=s.gridSize?0:min<int>(s.gridSize,s.gridChosen+5));break;
 case ActDiceGridType:dicer.gridSetType((s.gridType+1)%dice::TypeCount);break;
 case ActDiceThreshold:{unsigned top=dice::faces(s.gridType),step=top<=20?1:top<=66?5:10,next=s.thresholdOn?s.threshold+step:2;if(next>top)dicer.gridSetThreshold(false,s.threshold);else dicer.gridSetThreshold(true,next);break;}
 case ActDiceGridRow:dicer.gridAddRow();break;
 case ActDicePlus1:dicer.change(i,1);break;case ActDiceMinus1:dicer.change(i,-1);break;
 case ActDicePlus5:dicer.change(i,5);break;case ActDiceMinus5:dicer.change(i,-5);break;
 case ActDiceNextCounter:diceBlockSel=dicer.sheet.count?(diceBlockSel+1)%dicer.sheet.count:0;break;
 case ActDicePrevBlock:diceBlockSel=dicer.sheet.count?(diceBlockSel+dicer.sheet.count-1)%dicer.sheet.count:0;break;
 case ActDiceAddCounter:{int k=dicer.addBlock(dice::Track,i<0?-1:i+1);if(k>=0){unsigned n=0;for(unsigned j=0;j<dicer.sheet.count;j++)n+=dicer.sheet.block[j].kind==dice::Track;
  dicer.setField(k,"name",t("Counter ","Счётчик ")+String(n));dicer.setField(k,"max","0");dicer.setValue(k,dice::CounterStart);dicer.setField(k,"width","2");diceBlockSel=k;}break;}
 case ActDiceTap:diceTapping=true;diceTapUndo=false;break;
 case ActDiceAct:if(i>=0&&!dicer.act(i)&&dicer.why.length())notice(dicer.why);break;
 case ActDiceAccept:if(dicer.acceptOffer()){dicer.setMode(dice::Sheet);diceBlockSel=0;notice(dicer.heroName(dicer.s.character));}else notice(dicer.why);break;
 case ActDiceDecline:dicer.declineOffer();break;
 default:break;
 }
}
// In the roll modes every short press repeats the chosen pool or preset; on the sheet it goes to the next block.
bool diceRollKey(int key){
 if(page!=DicePage||(key!=13&&key!=0x82))return false;
 if(dicer.s.mode==dice::Sheet){if(dicer.sheet.count)diceBlockSel=(diceBlockSel+1)%dicer.sheet.count;return true;}
 if(dicer.s.mode==dice::Grid)dicer.gridRoll();else if(diceRepeatSaved&&dicer.savedCount())dicer.rollSaved(diceSavedSel%dicer.savedCount());else dicer.rollPool();return true;
}
// Joystick up/down inside the page.
void diceScroll(int step){
 auto& s=dicer.s;
 if(s.mode==dice::Rpg)dicer.setPool(s.count-step,s.type,s.mod);
 else if(s.mode==dice::Grid)dicer.gridChoose(int(s.gridChosen)-step);
 else if(diceChangeable(diceBlock()))dicer.change(diceBlock(),-step);
}
String diceTitle(){
 const auto& s=dicer.s;
 if(s.mode==dice::Grid){String v=(dicer.gridRolled&&s.thresholdOn?String(dicer.gridSuccesses())+"/":String())+String(s.gridChosen)+"x"+diceTypeName(s.gridType);if(s.thresholdOn)v+=" "+dice::valueText(s.gridType,s.threshold)+"+";return v;}
 if(s.mode==dice::Sheet)return t("Sheet","Лист")+(dicer.sheet.count?" "+String(diceBlock()+1)+"/"+String(dicer.sheet.count):String());
 return t("Dice ","Кости ")+dicer.poolText();
}
// ASCII rows of at most `cols` characters, broken after " + " and " - " where it can.
unsigned diceRows(const String& v,String* rows,unsigned most,unsigned cols){
 unsigned n=0,at=0;while(at<v.length()&&n<most){unsigned end=min<unsigned>(v.length(),at+cols);if(end<v.length()){unsigned sp=end;while(sp>at&&v[sp]!=' ')sp--;if(sp>at)end=sp;}
  rows[n++]=v.substring(at,end);at=end;while(at<v.length()&&v[at]==' ')at++;}
 return n;
}
// The value line of a block: "16 +3", "d8", "23/31", "##-", the roll of an ability.
String diceBlockValue(int i){
 const dice::Block& b=dicer.sheet.block[i];
 switch(b.kind){
 case dice::Stat:{if(!dicer.derivedOk(i))return "?";String v=b.rule==dice::Bonus?diceSigned(dicer.value(i)):String(dicer.value(i));String bd=dicer.badge(i);return bd.length()?v+" "+bd:v;}
 case dice::Track:{int m=dicer.maxOf(i);return m?String(dicer.value(i))+"/"+String(m):String(dicer.value(i));}
 case dice::Pips:{String v;int m=dicer.maxOf(i);for(int k=0;k<m;k++)v+=k<dicer.value(i)?"#":"-";return v;}
 case dice::Ability:{String r=dicer.rollFormula(i);return r.length()?dice::readable(r.c_str()):b.formula[0]?String("?"):b.use[0]&&b.spend?"-"+String(b.spend)+" "+b.use:String();}
 default:return String();
 }
}
void drawDiceMono(){
 auto& c=*hardware.canvas;const auto& s=dicer.s;
 if(s.mode==dice::Grid){
  for(unsigned i=0;i<s.gridSize;i++){int x=4+(i%10)*12,y=12+(i/10)*9;bool chosen=i<s.gridChosen,rolled=chosen&&dicer.gridRolled,pass=!s.thresholdOn||dicer.grid[i]>=s.threshold;
   if(!chosen){c.drawPixel(x+5,y+4,1);continue;}
   if(rolled&&pass)c.fillRect(x,y,11,8,1);else c.drawRect(x,y,11,8,1);
   if(rolled){String v=dice::valueText(s.gridType,dicer.grid[i]);const uint8_t* f=u8g2_font_4x6_t_cyrillic;say(x+6-width(v,f)/2,y+7,v,f,pass?0:1);}}
  return;}
 if(s.mode==dice::Sheet){int i=diceBlock();
  if(i<0){say(0,30,t("The sheet is empty","Лист пуст"));say(0,44,t("Blocks: web page, app","Блоки: веб, приложение"),small);return;}
  // The last roll of this block stays on top for a few seconds.
  const dice::Result* r=dicer.last();const dice::Block& b=dicer.sheet.block[i];
  if(r&&millis()-diceSeenAt<4000&&String(r->label)==b.name){say(0,22,clipped(String(r->label)+" "+dice::readable(r->formula),21),small);String total=String(r->total);say(0,44,total,u8g2_font_10x20_t_cyrillic);
   if(r->successes||r->fail)say(width(total,u8g2_font_10x20_t_cyrillic)+6,40,clipped(diceRetText(*r),14),small);
   else{String rows[2];int x=width(total,u8g2_font_10x20_t_cyrillic)+6;unsigned n=diceRows(dicer.resultText(*r,false),rows,2,(128-x)/5);for(unsigned k=0;k<n;k++)say(x,36+k*9,rows[k],small);}return;}
  say(0,22,clipped(b.kind==dice::Header?String("== ")+b.name:String(b.name),21),bold);
  String v=diceBlockValue(i);
  if(b.kind==dice::Note){String rows[4];unsigned n=diceRows(dicer.noteText(i),rows,4,21);for(unsigned k=0;k<n;k++)say(0,33+k*9,rows[k],small);return;}
  if(v.length())say(0,44,clipped(v,12),u8g2_font_10x20_t_cyrillic);
  if(b.kind==dice::Track&&dicer.maxOf(i)>0){int m=dicer.maxOf(i),w=constrain(dicer.value(i),0,m)*124/m;c.drawRect(0,48,126,5,1);c.fillRect(1,49,w,3,1);}
  else if(b.noteLen){String rows[1];diceRows(dicer.noteText(i),rows,1,21);say(0,55,rows[0],small);}
  else if(b.formula[0]&&b.kind==dice::Stat)say(0,55,clipped("= "+String(b.formula),21),small);
  return;}
 const dice::Result* r=dicer.last();
 if(!r){say(0,30,t("No rolls yet","Бросков ещё нет"));say(0,44,appOpen?t("Click to roll","Клик - бросить"):t("hold: open","держ: открыть"),small);return;}
 say(0,20,clipped(r->label[0]?String(r->label)+" "+dice::readable(r->formula):dice::readable(r->formula),21),small);
 int x=0;if(!r->special){String total=String(r->total);say(0,44,total,u8g2_font_10x20_t_cyrillic);x=width(total,u8g2_font_10x20_t_cyrillic)+6;}
 if(r->successes||r->fail){say(x,36,clipped(diceRetText(*r),(128-x)/5),small);}
 String rows[3];unsigned n=diceRows(dicer.resultText(*r,false),rows,r->special?3:2,(128-x)/5);
 for(unsigned i=0;i<n;i++)say(x,(r->special?33:44)+i*9,rows[i],small); // beside the sum, on its baseline
}
