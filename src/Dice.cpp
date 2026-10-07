#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash: the dice are not speed-critical (the rest of the nRF52 image is -O2)
#endif
#include "Dice.h"
#include "MeshRadio.h"
#include "I18n.h"
#include <ArduinoJson.h>
#include <Mm1Packet.h>
#if !defined(MM_UI_PREVIEW)
#include <esp_system.h>
#endif
dice::Dicer dicer;
namespace dice {
const char* const typeNames[TypeCount]={"d4","d6","d8","d10","d12","d20","d66","d%"};
namespace {
const char* const DiceFile="/meshmesh/dice.bin";const char* const DiceTemp="/meshmesh/dice.tmp";
#if defined(MM_UI_PREVIEW)
uint32_t entropy(){return rand();}
#else
uint32_t entropy(){return esp_random();} // the hardware generator
#endif
// 1..n without the bias of a plain modulo.
unsigned uniform(unsigned n){uint32_t limit=UINT32_MAX-UINT32_MAX%n,v;do v=entropy();while(v>=limit);return 1+v%n;}
uint32_t fnv(const uint8_t* p,size_t n){uint32_t h=2166136261u;while(n--)h=(h^*p++)*16777619u;return h;}
#define t(en,ru) String(tr(en,ru)) // a macro: tr() keys its translation at compile time (I18n.h)
bool fits(const String& v,unsigned bytes){return v.length()&&v.length()<=bytes&&meshmesh::validUtf8((const uint8_t*)v.c_str(),v.length());}
// One term of a formula: count dice of `sides` (66: d66, 100 with percent: d%) or a number.
struct Term{bool negative,die;uint8_t count,special;uint16_t sides;int32_t number;};
enum Special:uint8_t {Plain,Sixty,Percent};
unsigned parse(const String& f,Term* terms){
  unsigned n=0,i=0;const char* p=f.c_str();unsigned len=f.length();
  if(!len||len>FormulaBytes)return 0;
  while(i<len){
    if(n>=Terms)return 0;Term& v=terms[n];v={};
    if(p[i]=='+'||p[i]=='-'){v.negative=p[i]=='-';i++;}else if(n)return 0; // terms after the first need a sign
    uint32_t a=0;unsigned digits=0;while(i<len&&isdigit((unsigned char)p[i])&&digits<5){a=a*10+p[i]-'0';i++;digits++;}
    if(i<len&&p[i]=='d'){
      i++;v.die=true;if(digits&&(a<1||a>MaxCount))return 0;v.count=digits?a:1;
      if(i<len&&p[i]=='%'){i++;v.special=Percent;v.sides=100;}
      else{uint32_t b=0;unsigned bd=0;while(i<len&&isdigit((unsigned char)p[i])&&bd<5){b=b*10+p[i]-'0';i++;bd++;}
        if(!bd||b<2||b>1000)return 0;v.sides=b;if(b==66)v.special=Sixty;}
    }else{if(!digits||a>CounterLimit)return 0;v.number=a;}
    if(i<len&&isdigit((unsigned char)p[i]))return 0; // a number too long
    n++;
  }
  return n;
}
}
unsigned faces(uint8_t type){static const uint8_t f[TypeCount]={4,6,8,10,12,20,66,100};return type<TypeCount?f[type]:6;}
String valueText(uint8_t type,unsigned v){if(type!=DPercent)return String(v);if(v>=100)return "00";char b[4];snprintf(b,sizeof b,"%02u",v);return b;}
String Dicer::heroName(uint8_t i) const{if(i>=s.characters)return String();return s.list[i].name[0]?String(s.list[i].name):t("General","Общий");}
String Dicer::poolText() const{String v=String(s.count)+typeNames[s.type<TypeCount?s.type:D6];if(s.mod>0)v+="+"+String(s.mod);else if(s.mod<0)v+=String(s.mod);return v;}
String Dicer::clean(const String& formula){
  String v;for(unsigned i=0;i<formula.length();i++){uint8_t c=formula[i];
    if(c==0xd0&&i+1<formula.length()){uint8_t d=formula[i+1];if(d==0xba||d==0x9a||d==0xb4||d==0x94){v+='d';i++;continue;}} // к К д Д
    if(c==' '||c=='\t'){unsigned j=i;while(j<formula.length()&&(formula[j]==' '||formula[j]=='\t'))j++; // "2d6 3" stays apart (and invalid)
      if(v.length()&&j<formula.length()&&isalnum((unsigned char)v[v.length()-1])&&isalnum((unsigned char)formula[j]))v+=' ';i=j-1;continue;}
    v+=char(c>='A'&&c<='Z'?c+32:c);}
  return v;
}
bool Dicer::valid(const String& formula){Term terms[Terms];return parse(clean(formula),terms)>0;}
bool Dicer::roll(const String& formula,const String& label){
  String f=clean(formula);Term terms[Terms];unsigned n=parse(f,terms);if(!n)return false;
  if(resultCount==Results){memmove(results,results+1,sizeof(Result)*(Results-1));resultCount--;}
  Result& r=results[resultCount];memset(&r,0,sizeof r);
  strlcpy(r.formula,f.c_str(),sizeof r.formula);if(label.length())strlcpy(r.label,label.c_str(),sizeof r.label);
  auto push=[&](uint16_t v,uint8_t kind,bool neg,uint8_t shape){if(r.parts<Parts){r.value[r.parts]=v;r.kind[r.parts]=kind;r.shape[r.parts]=shape;r.negative[r.parts]=neg;r.parts++;}else r.more=true;};
  for(unsigned k=0;k<n;k++){const Term& v=terms[k];
    if(!v.die){push(v.number,PartNumber,v.negative,TypeCount);r.total+=v.negative?-v.number:v.number;continue;}
    uint8_t shape=TypeCount;for(uint8_t i=0;i<D66;i++)if(faces(i)==v.sides)shape=i;
    for(unsigned j=0;j<v.count;j++){
      if(v.special==Sixty){push(uniform(6)*10+uniform(6),PartD66,v.negative,D66);r.special=true;continue;}
      unsigned x=uniform(v.sides);if(v.special==Percent){push(x,PartPercent,v.negative,DPercent);r.special=true;continue;}
      push(x,PartDie,v.negative,shape);r.total+=v.negative?-int32_t(x):int32_t(x);}
  }
  r.serial=++rolls;resultCount++;events++;return true;
}
bool Dicer::rollSaved(unsigned i){const Character& c=hero();return i<c.rolls&&roll(c.roll[i].formula,c.roll[i].name);}
void Dicer::setPool(int count,int type,int mod){
  s.count=constrain(count,1,int(MaxCount));if(type>=0&&type<TypeCount)s.type=type;s.mod=constrain(mod,-MaxMod,MaxMod);changed();
}
String Dicer::resultText(const Result& r,bool sum) const{
  String v;for(unsigned i=0;i<r.parts;i++){if(i)v+=r.negative[i]?" - ":" + ";else if(r.negative[i])v+="-";
    v+=r.kind[i]==PartPercent?valueText(DPercent,r.value[i]):String(r.value[i]);}
  if(r.more)v+=" ...";if(sum&&!r.special)v+=" = "+String(r.total);return v;
}
void Dicer::gridChoose(int n){s.gridChosen=constrain(n,0,int(s.gridSize));gridRolled=false;changed();}
void Dicer::gridRoll(){
  if(!s.gridChosen)return;unsigned n=s.gridChosen,f=faces(s.gridType);
  for(unsigned i=0;i<n;i++)grid[i]=s.gridType==D66?uniform(6)*10+uniform(6):uniform(f);
  for(unsigned i=1;i<n;i++)for(unsigned j=i;j>0&&grid[j]>grid[j-1];j--)std::swap(grid[j],grid[j-1]); // high to low, as the app
  gridRolled=true;rolls++;events++;
}
void Dicer::gridAddRow(){if(s.gridSize+GridRow<=GridMax){for(unsigned i=s.gridSize;i<s.gridSize+GridRow;i++)grid[i]=1;s.gridSize+=GridRow;changed();}}
void Dicer::gridReset(){s.gridSize=GridStart;s.gridChosen=0;gridRolled=false;for(auto& v:grid)v=1;changed();}
void Dicer::gridSetType(int type){if(type<0||type>=TypeCount)return;s.gridType=type;gridRolled=false;s.threshold=constrain(s.threshold,2,int(faces(type)));changed();}
void Dicer::gridSetThreshold(bool on,int value){s.thresholdOn=on;s.threshold=constrain(value,2,int(faces(s.gridType)));changed();}
unsigned Dicer::gridSuccesses() const{if(!gridRolled||!s.thresholdOn)return 0;unsigned n=0;for(unsigned i=0;i<s.gridChosen;i++)n+=grid[i]>=s.threshold;return n;}
bool Dicer::addRoll(const String& formula,const String& name){
  Character& c=hero();String f=clean(formula),v=name;v.trim();if(c.rolls>=Rolls||!valid(f)||!fits(v,NameBytes))return false;
  SavedRoll& r=c.roll[c.rolls++];strlcpy(r.formula,f.c_str(),sizeof r.formula);strlcpy(r.name,v.c_str(),sizeof r.name);r.color=0;changed();return true;
}
bool Dicer::editRoll(unsigned i,const String& formula,const String& name){
  Character& c=hero();String f=clean(formula),v=name;v.trim();if(i>=c.rolls||!valid(f)||!fits(v,NameBytes))return false;
  strlcpy(c.roll[i].formula,f.c_str(),sizeof c.roll[i].formula);strlcpy(c.roll[i].name,v.c_str(),sizeof c.roll[i].name);changed();return true;
}
bool Dicer::deleteRoll(unsigned i){Character& c=hero();if(i>=c.rolls)return false;memmove(&c.roll[i],&c.roll[i+1],sizeof(SavedRoll)*(c.rolls-i-1));c.rolls--;changed();return true;}
bool Dicer::colorRoll(unsigned i,unsigned color){Character& c=hero();if(i>=c.rolls||color>=Colors)return false;c.roll[i].color=color;changed();return true;}
bool Dicer::addCounter(const String& name){
  Character& c=hero();if(c.counters>=Counters)return false;String v=name;v.trim();if(!v.length())v=t("Counter ","Счётчик ")+String(c.counters+1);
  if(!fits(v,CounterNameBytes))return false;Counter& k=c.counter[c.counters];strlcpy(k.name,v.c_str(),sizeof k.name);k.value=CounterStart;k.color=c.counters%Colors;c.counters++;changed();return true;
}
bool Dicer::changeCounter(unsigned i,int delta){Character& c=hero();if(i>=c.counters)return false;c.counter[i].value=constrain(c.counter[i].value+delta,-CounterLimit,CounterLimit);changed(true);return true;}
bool Dicer::setCounter(unsigned i,int value){Character& c=hero();if(i>=c.counters)return false;c.counter[i].value=constrain(value,-CounterLimit,CounterLimit);changed(true);return true;}
bool Dicer::renameCounter(unsigned i,const String& name){Character& c=hero();String v=name;v.trim();if(i>=c.counters||!fits(v,CounterNameBytes))return false;strlcpy(c.counter[i].name,v.c_str(),sizeof c.counter[i].name);changed();return true;}
bool Dicer::colorCounter(unsigned i,unsigned color){Character& c=hero();if(i>=c.counters||color>=Colors)return false;c.counter[i].color=color;changed();return true;}
bool Dicer::deleteCounter(unsigned i){Character& c=hero();if(i>=c.counters)return false;memmove(&c.counter[i],&c.counter[i+1],sizeof(Counter)*(c.counters-i-1));c.counters--;changed();return true;}
// A new character starts with one counter, as in the app.
bool Dicer::addCharacter(const String& name){
  String v=name;v.trim();if(s.characters>=Characters||!fits(v,NameBytes))return false;
  Character& c=s.list[s.characters];memset(&c,0,sizeof c);strlcpy(c.name,v.c_str(),sizeof c.name);
  s.character=s.characters++;addCounter();changed();return true;
}
bool Dicer::useCharacter(unsigned i){if(i>=s.characters)return false;s.character=i;changed();return true;}
bool Dicer::renameCharacter(const String& name){String v=name;v.trim();if(!fits(v,NameBytes))return false;strlcpy(hero().name,v.c_str(),sizeof hero().name);changed();return true;}
bool Dicer::deleteCharacter(){
  if(!s.character)return false;unsigned i=s.character;memmove(&s.list[i],&s.list[i+1],sizeof(Character)*(s.characters-i-1));
  s.characters--;memset(&s.list[s.characters],0,sizeof(Character));s.character=0;changed();return true;
}
void Dicer::setMode(uint8_t mode){if(mode<ModeCount&&mode!=s.mode){s.mode=mode;changed();}}
void Dicer::begin(){
  State* stored=new State;
  bool ok=readStored(DiceFile,DiceTemp,stored,sizeof(State))&&stored->version==1&&stored->check==fnv((const uint8_t*)stored,offsetof(State,check))
    &&stored->characters>=1&&stored->characters<=Characters&&stored->character<stored->characters&&stored->mode<ModeCount&&stored->type<TypeCount&&stored->gridType<TypeCount
    &&stored->count>=1&&stored->count<=MaxCount&&stored->gridSize>=GridStart&&stored->gridSize<=GridMax&&stored->gridChosen<=stored->gridSize;
  for(unsigned i=0;ok&&i<stored->characters;i++){const Character& c=stored->list[i];ok=c.rolls<=Rolls&&c.counters<=Counters&&memchr(c.name,0,sizeof c.name);
    for(unsigned j=0;ok&&j<c.rolls;j++)ok=memchr(c.roll[j].name,0,sizeof c.roll[j].name)&&memchr(c.roll[j].formula,0,sizeof c.roll[j].formula)&&c.roll[j].color<Colors;
    for(unsigned j=0;ok&&j<c.counters;j++)ok=memchr(c.counter[j].name,0,sizeof c.counter[j].name)&&c.counter[j].color<Colors;}
  if(ok)s=*stored;
  delete stored;for(auto& v:grid)v=1;events++;
}
void Dicer::tick(){if(saveDue&&int32_t(millis()-saveDue)>=0)save();}
void Dicer::changed(bool soon){events++;if(soon){saveDue=millis()+2000;return;}save();}
void Dicer::save(){saveDue=0;s.check=fnv((const uint8_t*)&s,offsetof(State,check));writeStored(DiceFile,DiceTemp,&s,sizeof(State));}
String Dicer::json() const{
  DynamicJsonDocument d(8192);const char* modes[]={"rpg","grid","counters"};d["mode"]=modes[s.mode<ModeCount?s.mode:0];
  d["character"]=s.character;JsonArray hs=d.createNestedArray("characters");for(unsigned i=0;i<s.characters;i++)hs.add(heroName(i));
  JsonObject p=d.createNestedObject("pool");p["count"]=s.count;p["type"]=typeNames[s.type];p["mod"]=s.mod;p["text"]=poolText();
  const Character& c=hero();
  JsonArray rs=d.createNestedArray("rolls");for(unsigned i=0;i<c.rolls;i++){JsonObject o=rs.createNestedObject();o["name"]=c.roll[i].name;o["formula"]=c.roll[i].formula;o["color"]=c.roll[i].color;}
  JsonArray cs=d.createNestedArray("counters");for(unsigned i=0;i<c.counters;i++){JsonObject o=cs.createNestedObject();o["name"]=c.counter[i].name;o["value"]=c.counter[i].value;o["color"]=c.counter[i].color;}
  // Parts: value*4+kind (kind 0 die, 1 number, 2 d66, 3 d%), negative when subtracted; one number each keeps the reply small.
  JsonArray out=d.createNestedArray("results");
  for(unsigned i=0;i<resultCount;i++){const Result& r=results[i];JsonObject o=out.createNestedObject();o["n"]=r.serial;if(r.label[0])o["label"]=r.label;o["formula"]=r.formula;
    JsonArray ps=o.createNestedArray("parts");for(unsigned k=0;k<r.parts;k++){long v=long(r.value[k])*4+r.kind[k];ps.add(r.negative[k]?-v:v);}
    if(r.more)o["more"]=true;if(r.special)o["special"]=true;else o["total"]=r.total;}
  JsonObject g=d.createNestedObject("grid");g["type"]=typeNames[s.gridType];g["size"]=s.gridSize;g["chosen"]=s.gridChosen;g["rolled"]=gridRolled;
  g["threshold"]=s.threshold;g["threshold_on"]=s.thresholdOn;g["successes"]=gridSuccesses();
  JsonArray gv=g.createNestedArray("values");for(unsigned i=0;i<s.gridSize;i++)gv.add(grid[i]);
  d["rolls_total"]=rolls;
  if(d.overflowed())d["overflowed"]=true;
  String v;serializeJson(d,v);return v;
}
// USB, BLE and the web page (docs/dice.md): "dice" shows the state; "dice roll [FORMULA]", "dice pool COUNT TYPE MOD",
// "dice mode rpg|grid|counters", "dice clear"; "dice saved add|edit I|del I|color I C|roll I"; "dice grid choose N|roll|row|reset|type T|threshold off|N";
// "dice counter add [NAME]|I +N|I -N|I set N|I name NAME|I color C|I del"; "dice char add NAME|use I|name NAME|del".
// A saved roll's formula comes first, its name after it: "dice saved add 8d6 Fireball".
String Dicer::command(const String& line){
  String arg=line.length()>5?line.substring(5):String();arg.trim();
  auto ok=[&](const String& r){return "OK "+r+" "+json();};
  auto take=[](String& rest){int sp=rest.indexOf(' ');String w=sp<0?rest:rest.substring(0,sp);rest=sp<0?String():rest.substring(sp+1);rest.trim();return w;};
  auto number=[](const String& w,long& v){if(!w.length())return false;char* end;v=strtol(w.c_str(),&end,10);return *end==0;};
  auto type=[](const String& w){for(int i=0;i<TypeCount;i++)if(w==typeNames[i])return i;return -1;};
  if(!arg.length())return json();
  String rest=arg,op=take(rest);long a=0,b=0;
  if(op=="roll"){if(!rest.length()){rollPool();return ok("rolled");}return roll(rest)?ok("rolled"):"ERR formula: NdX+NdX-N, X 2..1000, d% d66, up to 8 terms";}
  if(op=="clear"){clearResults();return ok("cleared");}
  if(op=="mode"){const char* modes[]={"rpg","grid","counters"};for(int i=0;i<ModeCount;i++)if(rest==modes[i]){setMode(i);return ok("mode");}return "ERR mode rpg|grid|counters";}
  if(op=="pool"){String c=take(rest),k=take(rest),m=take(rest);int ty=type(k);if(!number(c,a)||ty<0||(m.length()&&!number(m,b)))return "ERR pool COUNT TYPE [MOD]";setPool(a,ty,b);return ok("pool");}
  if(op=="saved"){String w=take(rest);
    if(w=="add"){String f=take(rest);return addRoll(f,rest)?ok("added"):"ERR saved add FORMULA NAME (10 rolls, name 1-23 bytes)";}
    if(!number(take(rest),a))return "ERR saved add|edit I|del I|color I C|roll I";
    if(w=="roll")return rollSaved(a)?ok("rolled"):"ERR no such roll";
    if(w=="del")return deleteRoll(a)?ok("deleted"):"ERR no such roll";
    if(w=="color")return number(rest,b)&&colorRoll(a,b)?ok("color"):"ERR saved color I 0-14";
    if(w=="edit"){String f=take(rest);return editRoll(a,f,rest)?ok("saved"):"ERR saved edit I FORMULA NAME";}
    return "ERR saved add|edit|del|color|roll";}
  if(op=="grid"){String w=take(rest);
    if(w=="choose")return number(rest,a)?(gridChoose(a),ok("chosen")):"ERR grid choose N";
    if(w=="roll"){if(!s.gridChosen)return "ERR choose the dice first";gridRoll();return ok("rolled");}
    if(w=="row"){if(s.gridSize>=GridMax)return "ERR 50 dice at most";gridAddRow();return ok("row");}
    if(w=="reset"){gridReset();return ok("reset");}
    if(w=="type"){int ty=type(rest);if(ty<0)return "ERR grid type d4|d6|d8|d10|d12|d20|d66|d%";gridSetType(ty);return ok("type");}
    if(w=="threshold"){if(rest=="off"){gridSetThreshold(false,s.threshold);return ok("threshold");}if(!number(rest,a))return "ERR grid threshold off|N";gridSetThreshold(true,a);return ok("threshold");}
    return "ERR grid choose|roll|row|reset|type|threshold";}
  if(op=="counter"){String w=take(rest);
    if(w=="add")return addCounter(rest)?ok("added"):"ERR 6 counters at most, name 1-23 bytes";
    if(!number(w,a))return "ERR counter add|I ...";String v=take(rest);
    if(v.startsWith("+")||v.startsWith("-"))return number(v.substring(v[0]=='+'?1:0),b)&&changeCounter(a,b)?ok("changed"):"ERR no such counter";
    if(v=="set")return number(rest,b)&&setCounter(a,b)?ok("set"):"ERR counter I set N";
    if(v=="name")return renameCounter(a,rest)?ok("renamed"):"ERR counter name: 1-23 bytes";
    if(v=="color")return number(rest,b)&&colorCounter(a,b)?ok("color"):"ERR counter I color 0-14";
    if(v=="del")return deleteCounter(a)?ok("deleted"):"ERR no such counter";
    return "ERR counter I +N|-N|set N|name NAME|color C|del";}
  if(op=="char"){String w=take(rest);
    if(w=="add")return addCharacter(rest)?ok("added"):"ERR 6 characters at most, name 1-23 bytes";
    if(w=="use")return number(rest,a)&&useCharacter(a)?ok("chosen"):"ERR no such character";
    if(w=="name")return renameCharacter(rest)?ok("renamed"):"ERR name: 1-23 bytes";
    if(w=="del")return deleteCharacter()?ok("deleted"):"ERR the general set stays";
    return "ERR char add|use|name|del";}
  return "ERR dice [roll|pool|mode|clear|saved|grid|counter|char]";
}
}
