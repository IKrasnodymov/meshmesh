#include "I18n.h"
#include "Config.h"
#include <U8g2_for_Adafruit_GFX.h>
#include "I18nTable.h"
#include "I18nFonts.h"
#include <algorithm>
#include <initializer_list>
#include <vector>
#if !defined(MM_NRF52) && !defined(MM_UI_PREVIEW)
#include <esp_spi_flash.h>
#endif

const char* const langCodes[LangCount]={"en","ru","uk","es","pt","fr","de","it","pl","tr","zh","ja","ko","ar","id"};
const char* const langNames[LangCount]={"English","Русский","Українська","Español","Português","Français","Deutsch","Italiano","Polski","Türkçe","中文","日本語","한국어","العربية","Bahasa Indonesia"};
int langFromCode(const String& code){for(int i=0;i<LangCount;i++)if(code==langCodes[i])return i;return -1;}

namespace {
uint8_t lang(){return config.lang<LangCount?config.lang:LangEn;}
// FNV-1a over the parts joined by 0x1f, as tools/i18n.py keys the translations.
uint32_t hashParts(std::initializer_list<const char*> parts){
 uint32_t h=0x811c9dc5;bool first=true;
 for(const char* p:parts){if(!first)h=(h^0x1f)*0x01000193;first=false;while(*p)h=(h^uint8_t(*p++))*0x01000193;}
 return h;
}
// The translation for the interface language, nullptr when there is none.
const char* lookup(uint32_t key){
 uint8_t l=lang();if(l<2)return nullptr;
 unsigned lo=0,hi=i18nTable::count;
 while(lo<hi){unsigned mid=(lo+hi)/2;if(i18nTable::keys[mid]<key)lo=mid+1;else hi=mid;}
 if(lo>=i18nTable::count||i18nTable::keys[lo]!=key)return nullptr;
 const char* s=i18nTable::texts[l-2]+i18nTable::offsets[l-2][lo];return *s?s:nullptr;
}
// CLDR plural categories: 0 zero, 1 one, 2 two, 3 few, 4 many, 5 other.
uint8_t category(uint8_t l,unsigned n){
 unsigned d=n%10,h=n%100;
 switch(l){
 case LangRu:case LangUk:return d==1&&h!=11?1:d>=2&&d<=4&&(h<12||h>14)?3:4;
 case LangPl:return n==1?1:d>=2&&d<=4&&(h<12||h>14)?3:4;
 case LangFr:case LangPt:return n<=1?1:5;
 case LangZh:case LangJa:case LangKo:case LangId:return 5;
 case LangAr:return n==0?0:n==1?1:n==2?2:h>=3&&h<=10?3:h>=11?4:5;
 default:return n==1?1:5;
 }
}
}

const char* tr(const char* en,const char* ru){
 uint8_t l=lang();if(l==LangEn)return en;if(l==LangRu)return ru;
 const char* s=lookup(hashParts({en,ru}));return s?s:en;
}

String plural(unsigned n,const char* one,const char* many,const char* ru1,const char* ru2,const char* ru5){
 uint8_t l=lang();String form;
 if(l==LangRu){uint8_t c=category(l,n);form=c==1?ru1:c==3?ru2:ru5;}
 else if(const char* s=l==LangEn?nullptr:lookup(hashParts({one,many,ru1,ru2,ru5}))){
  // six forms separated by 0x1f; a missing one is "other"
  String all(s),forms[6];unsigned at=0;for(int i=0;i<6;i++){int end=all.indexOf('\x1f',at);if(end<0)end=all.length();forms[i]=all.substring(at,end);at=end+1;}
  form=forms[category(l,n)];if(!form.length())form=forms[5];
  // Translations place the number with {n}; a form without it is the whole phrase (Arabic "two messages").
  if(form.length()){form.replace("{n}",String(n));return form;}
 }
 if(!form.length())form=n==1?one:many;
 return String(n)+" "+form;
}

#if defined(MM_NRF52)
// The site installer writes the chosen language over "--" (site/index.html), and the DFU CRC with it.
extern "C" __attribute__((used)) const volatile char mmInstallLang[16]="MMLANG:--";
#endif
String installLanguage(){
 char b[17]={};
#if defined(MM_UI_PREVIEW)
 return "";
#elif defined(MM_NRF52)
 for(int i=0;i<16;i++)b[i]=mmInstallLang[i];
#else
 // The tail of the partition table sector: tools/pages.py writes "MMLANG:<code>" there in partitions-<code>.bin.
 if(spi_flash_read(0x8c00,b,16)!=ESP_OK)return "";
#endif
 b[16]=0;if(strncmp(b,"MMLANG:",7))return "";
 String code(b+7);return langFromCode(code)>=0?code:"";
}

// Arabic: letters U+0621..U+064A as their isolated presentation form (U+FE80..) and how they join:
// 1 never, 2 only to the previous letter (isolated, final), 4 both ways (isolated, final, initial, medial).
namespace {
struct ArabicLetter{uint16_t isolated;uint8_t forms;};
const ArabicLetter arabicLetters[]={
 {0xFE80,1},{0xFE81,2},{0xFE83,2},{0xFE85,2},{0xFE87,2},{0xFE89,4},{0xFE8D,2},{0xFE8F,4},{0xFE93,2},{0xFE95,4},
 {0xFE99,4},{0xFE9D,4},{0xFEA1,4},{0xFEA5,4},{0xFEA9,2},{0xFEAB,2},{0xFEAD,2},{0xFEAF,2},{0xFEB1,4},{0xFEB5,4},
 {0xFEB9,4},{0xFEBD,4},{0xFEC1,4},{0xFEC5,4},{0xFEC9,4},{0xFECD,4},{0,0},{0,0},{0,0},{0,0},{0,0},{0x0640,4},
 {0xFED1,4},{0xFED5,4},{0xFED9,4},{0xFEDD,4},{0xFEE1,4},{0xFEE5,4},{0xFEE9,4},{0xFEED,2},{0xFEEF,2},{0xFEF1,4}};
const ArabicLetter* letter(uint32_t cp){return cp>=0x621&&cp<=0x64a&&arabicLetters[cp-0x621].forms?&arabicLetters[cp-0x621]:nullptr;}
bool rtl(uint32_t cp){return (cp>=0x590&&cp<=0x8ff)||(cp>=0xfb1d&&cp<=0xfdff)||(cp>=0xfe70&&cp<=0xfeff);}
bool neutral(uint32_t cp){return cp<0x80?!isalnum(int(cp)):(cp>=0xa0&&cp<=0xbf)||cp==0xd7||cp==0xf7||(cp>=0x2000&&cp<=0x2bff)||(cp>=0x3000&&cp<=0x303f)||(cp>=0xff01&&cp<=0xff0f);}
uint32_t mirrored(uint32_t cp){switch(cp){case '(':return ')';case ')':return '(';case '[':return ']';case ']':return '[';case '{':return '}';case '}':return '{';case '<':return '>';case '>':return '<';case 0xab:return 0xbb;case 0xbb:return 0xab;}return cp;}
uint32_t nextCodepoint(const String& s,unsigned& i){uint8_t c=s[i];unsigned n=c<0x80?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:4;uint32_t cp=n==1?c:n==2?c&0x1f:n==3?c&0x0f:c&0x07;for(unsigned k=1;k<n&&i+k<s.length();k++)cp=cp<<6|(uint8_t(s[i+k])&0x3f);i+=n;return cp;}
void append(String& s,uint32_t cp){char b[5]={};if(cp<0x80)b[0]=cp;else if(cp<0x800){b[0]=0xc0|cp>>6;b[1]=0x80|(cp&0x3f);}else if(cp<0x10000){b[0]=0xe0|cp>>12;b[1]=0x80|((cp>>6)&0x3f);b[2]=0x80|(cp&0x3f);}else{b[0]=0xf0|cp>>18;b[1]=0x80|((cp>>12)&0x3f);b[2]=0x80|((cp>>6)&0x3f);b[3]=0x80|(cp&0x3f);}s+=b;}
}

String visualText(const String& logical){
 bool any=false;for(unsigned i=0;i<logical.length();){if(rtl(nextCodepoint(logical,i))){any=true;break;}}
 if(!any)return logical;
 // Shape: harakat dropped, letters joined, lam + alef as one ligature.
 std::vector<uint32_t> in,cp;
 for(unsigned i=0;i<logical.length();){uint32_t c=nextCodepoint(logical,i);if((c>=0x64b&&c<=0x65f)||c==0x670)continue;in.push_back(c);}
 auto joinsNext=[&](size_t k){const ArabicLetter* a=k<in.size()?letter(in[k]):nullptr;return a&&a->forms==4;};
 for(size_t k=0;k<in.size();k++){
  const ArabicLetter* a=letter(in[k]);if(!a){cp.push_back(in[k]);continue;}
  bool before=k>0&&joinsNext(k-1);
  if(in[k]==0x644&&k+1<in.size()&&(in[k+1]==0x622||in[k+1]==0x623||in[k+1]==0x625||in[k+1]==0x627)){
   uint16_t lig=in[k+1]==0x622?0xFEF5:in[k+1]==0x623?0xFEF7:in[k+1]==0x625?0xFEF9:0xFEFB;cp.push_back(lig+(before?1:0));k++;continue;}
  if(a->isolated==0x0640){cp.push_back(0x640);continue;}
  const ArabicLetter* n=k+1<in.size()?letter(in[k+1]):nullptr;bool after=a->forms==4&&n&&n->forms>=2;
  cp.push_back(a->forms==1?a->isolated:a->forms==2?a->isolated+(before?1:0):a->isolated+(before&&after?3:after?2:before?1:0));
 }
 // Order: a minimal bidi. Numbers (with a sign) read left to right but do not set the paragraph direction:
 // the first letter does, and in the Arabic interface every line with Arabic is right-to-left.
 // Neutrals take the direction of the strong letters around them, else the paragraph's.
 size_t n=cp.size();std::vector<uint8_t> dir(n,2); // 0 left-to-right, 1 right-to-left, 2 neutral
 auto digit=[&](size_t k){return k<n&&cp[k]>='0'&&cp[k]<='9';};
 int base=lang()==LangAr?1:-1;
 for(size_t k=0;k<n;k++){
  if(rtl(cp[k]))dir[k]=1;else if(!neutral(cp[k])||((cp[k]=='-'||cp[k]=='+')&&digit(k+1)))dir[k]=0;
  if(base<0&&dir[k]!=2&&!digit(k)&&cp[k]!='-'&&cp[k]!='+')base=dir[k];
 }
 if(base<0)base=1;
 for(size_t k=0;k<n;){if(dir[k]!=2){k++;continue;}size_t e=k;while(e<n&&dir[e]==2)e++;int before=k?dir[k-1]:base,after=e<n?dir[e]:base;for(size_t j=k;j<e;j++)dir[j]=before==after?before:base;k=e;}
 // Runs of one direction; right-to-left runs are reversed, and in a right-to-left paragraph so is their order.
 std::vector<std::pair<size_t,size_t>> runs;for(size_t k=0;k<n;){size_t e=k;while(e<n&&dir[e]==dir[k])e++;runs.push_back({k,e});k=e;}
 if(base==1)std::reverse(runs.begin(),runs.end());
 String out;
 for(auto& r:runs){if(dir[r.first]==1)for(size_t j=r.second;j-->r.first;)append(out,mirrored(cp[j]));else for(size_t j=r.first;j<r.second;j++)append(out,cp[j]);}
 return out;
}

bool fontHasGlyph(const uint8_t* font,uint16_t cp){
 // The lookup of u8g2_font_get_glyph_data (U8g2_for_Adafruit_GFX.cpp), without selecting the font.
 if(!font)return false;auto be16=[](const uint8_t* p){return uint16_t(p[0]<<8|p[1]);};
 const uint8_t* f=font+23;
 if(cp<=255){if(cp>='a')f+=be16(font+19);else if(cp>='A')f+=be16(font+17);for(;;){if(!f[1])return false;if(f[0]==cp)return true;f+=f[1];}}
 f+=be16(font+21);const uint8_t* table=f;uint16_t e;
 do{f+=be16(table);e=be16(table+2);table+=4;}while(e<cp);
 for(;;){e=be16(f);if(!e)return false;if(e==cp)return true;f+=f[2];}
}

uint32_t utf8Next(const String& s,unsigned& i){return nextCodepoint(s,i);}
uint8_t glyphCells(uint32_t cp){
 return (cp>=0x1100&&cp<=0x115f)||(cp>=0x2e80&&cp<=0xa4cf)||(cp>=0xac00&&cp<=0xd7a3)||(cp>=0xf900&&cp<=0xfaff)||(cp>=0xfe30&&cp<=0xfe4f)||(cp>=0xff00&&cp<=0xff60)||(cp>=0xffe0&&cp<=0xffe6)?2:1;
}

const uint8_t* const* fallbackFonts(const uint8_t* font){
 static const uint8_t* list[8];unsigned n=0;
 bool small=font==u8g2_font_5x8_t_cyrillic||font==u8g2_font_4x6_t_cyrillic;
 if(font==u8g2_font_6x13_t_cyrillic){list[n++]=u8g2_font_6x13_tf;list[n++]=mmFontExt13;}
 else if(font==u8g2_font_6x13B_t_cyrillic){list[n++]=u8g2_font_6x13B_tf;list[n++]=mmFontExt13B;}
 else if(font==u8g2_font_5x8_t_cyrillic){list[n++]=u8g2_font_5x8_tf;list[n++]=mmFontExt8;}
 else if(font==u8g2_font_4x6_t_cyrillic){list[n++]=u8g2_font_4x6_tf;list[n++]=mmFontExt6;}
 else if(font==u8g2_font_10x20_t_cyrillic){list[n++]=u8g2_font_10x20_tf;list[n++]=mmFontExt20;}
 else if(font==u8g2_font_6x12_t_cyrillic){list[n++]=u8g2_font_6x12_tf;list[n++]=mmFontExt12;}
 // CJK and Arabic: the interface language's own script first, so shared Han characters take its forms.
 const uint8_t* zh=small?mmFontZh8:mmFontZh12,*ja=small?mmFontJa8:mmFontJa12,*ko=small?mmFontKo8:mmFontKo12;
 uint8_t l=lang();
 if(l==LangJa){list[n++]=ja;list[n++]=zh;}else{list[n++]=zh;list[n++]=ja;}
 list[n++]=ko;list[n++]=mmFontAr12;list[n]=nullptr;
 return list;
}
