#pragma once
#include <Arduino.h>
// Dice roller, the port of DIC3R (docs/dice.md): three modes as in the app. RPG rolls a pool ("2d6+1": count,
// die, modifier) or a formula ("3d8+2d4-1"; "2к6" in Russian notation; d66 and d% show each die, without a
// sum); the results keep the last eight rolls. The Warhammer grid rolls the first N of its dice at once,
// sorted from high to low, and with a success threshold ("4+") counts the dice that pass it. Counters are
// named, coloured values changed by 1 and 5 (life, wounds, resources). Saved rolls and counters belong to a
// character; character 0 is the general set. The device keeps it all (a file in MeshMesh storage), so the
// screens, the web page and the app show the same rolls; the hardware generator rolls the dice.
namespace dice {
enum Mode:uint8_t {Rpg,Grid,Tally,ModeCount};                 // Tally: the counters
enum Type:uint8_t {D4,D6,D8,D10,D12,D20,D66,DPercent,TypeCount};
constexpr unsigned Characters=6,Rolls=10,Counters=6,Colors=15,Results=8,Parts=24,Terms=8;
constexpr unsigned GridStart=20,GridRow=5,GridMax=50,MaxCount=99,NameBytes=23,FormulaBytes=27,CounterNameBytes=23;
constexpr int MaxMod=99,CounterStart=20,CounterLimit=9999;
extern const char* const typeNames[TypeCount];               // "d4" ... "d66", "d%"
// The palette of the app's counters and saved rolls (DIC3R colorPalette), as 0xRRGGBB.
constexpr uint32_t colors[Colors]={0xff6b6b,0x4ecdc4,0x45b7d1,0xffa07a,0x98d8c8,0xf06292,0xaed581,0xffd54f,0x4dd0e1,0xba68c8,0x4fc3f7,0xfff176,0x81c784,0xdce775,0x64b5f6};
unsigned faces(uint8_t type);                                // the largest value: 6 for d6, 66 for d66, 100 for d%
String valueText(uint8_t type,unsigned v);                   // d% shows 100 as "00" and 1..9 as "01".."09"
struct SavedRoll{char name[NameBytes+1];char formula[FormulaBytes+1];uint8_t color;};
struct Counter{char name[CounterNameBytes+1];int16_t value;uint8_t color;};
struct Character{char name[NameBytes+1];uint8_t rolls,counters;SavedRoll roll[Rolls];Counter counter[Counters];};
struct State{
  uint32_t version=1;
  uint8_t character=0,characters=1,mode=Rpg;
  uint8_t type=D6,count=2;int8_t mod=0;                      // the RPG pool
  uint8_t gridType=D6,gridSize=GridStart,gridChosen=0,threshold=4;bool thresholdOn=false;
  Character list[Characters]={};
  uint32_t check=0;
};
// A roll: its parts in order (dice, numbers, special dice) and the sum of the ordinary ones.
enum Part:uint8_t {PartDie,PartNumber,PartD66,PartPercent};
struct Result{
  char label[NameBytes+1];char formula[FormulaBytes+1];
  uint8_t parts;uint16_t value[Parts];uint8_t kind[Parts],shape[Parts];bool negative[Parts]; // shape: the Type the screens draw, TypeCount for other dice
  bool more,special;                                         // more: dice beyond the shown parts; special: d66/d% (no sum)
  int32_t total;uint32_t serial;
};
class Dicer {
 public:
  // On the heap, made at start-up: about 6 KB do not fit the static RAM of the ESP32 boards without PSRAM.
  State& s=*new State;
  Result* const results=new Result[Results];unsigned resultCount=0;uint32_t rolls=0; // newest last
  uint8_t grid[GridMax]={};bool gridRolled=false;            // the values on the grid; rolled: the chosen ones are fresh
  uint32_t events=0;                                         // grows with every change a screen should show
  void begin();void tick();void flush(){if(saveDue)save();}
  Character& hero(){return s.list[s.character];}
  const Character& hero() const{return s.list[s.character];}
  String heroName(uint8_t i) const;                          // "General" for character 0 without a name
  String poolText() const;                                   // "2d6+1"
  // Rolls. A formula: terms joined by + and -, each NdX (N up to 99, X 2..1000, d% d66), dX or a number.
  bool roll(const String& formula,const String& label=String());
  bool rollPool(){return roll(poolText());}
  bool rollSaved(unsigned i);
  void setPool(int count,int type,int mod);
  static bool valid(const String& formula);
  static String clean(const String& formula);                // lower case, no spaces, "к" and "д" as "d"
  const Result* last() const{return resultCount?&results[resultCount-1]:nullptr;}
  String resultText(const Result& r,bool sum=true) const;    // "4 + 3 + 1 = 8"
  void clearResults(){resultCount=0;events++;}
  // The Warhammer grid.
  void gridChoose(int n);void gridRoll();void gridAddRow();void gridReset();void gridSetType(int type);void gridSetThreshold(bool on,int value);
  unsigned gridSuccesses() const;
  // Saved rolls and counters of the current character; false when full, missing or invalid.
  bool addRoll(const String& formula,const String& name);bool editRoll(unsigned i,const String& formula,const String& name);
  bool deleteRoll(unsigned i);bool colorRoll(unsigned i,unsigned color);
  bool addCounter(const String& name=String());bool changeCounter(unsigned i,int delta);bool setCounter(unsigned i,int value);
  bool renameCounter(unsigned i,const String& name);bool colorCounter(unsigned i,unsigned color);bool deleteCounter(unsigned i);
  // Characters: up to six with their own rolls and counters; the general set (0) cannot be removed.
  bool addCharacter(const String& name);bool useCharacter(unsigned i);bool renameCharacter(const String& name);bool deleteCharacter();
  void setMode(uint8_t mode);
  String json() const;
  String command(const String& line);                        // USB "dice ..."
 private:
  uint32_t saveDue=0;
  void changed(bool soon=false);                             // soon: counters, written two seconds after the last press
  void save();
};
}
extern dice::Dicer dicer;
