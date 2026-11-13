#if defined(MM_NRF52)
#pragma GCC optimize("Os") // 1 MB flash, as UiHeltec.cpp
#endif
#include "App.h"

// The screen's apps and folders in the chosen order (config.apps): IDs separated by spaces, "-" before an ID
// hides it; "{NAME" opens a folder (its apps follow, "}" closes it), "-{NAME" hides the folder with its apps.
// A folder name has "_" for its spaces; "games" is the folder of the games, named in the interface language.
// IDs the text does not name (apps new in this firmware) follow in the default place: a game goes to the games
// folder when there is one, anything else to the end. Settings stay at the top and shown: the way to everything else.
namespace {
const char* const GameIds[]={"chess","checkers","sea","pet","dice","solitaire"};
const char GamesFolder[]="games";
int appIndex(const char* id,size_t n){for(uint8_t i=0;i<uiAppCount;i++)if(strlen(uiApps[i])==n&&!strncmp(uiApps[i],id,n))return i;return -1;}
bool isGame(uint8_t i){for(const char* g:GameIds)if(!strcmp(uiApps[i],g))return true;return false;}
bool isSettings(uint8_t i){return !strcmp(uiApps[i],"settings");}
int gamesFolder(const AppMenu& m){for(uint8_t f=0;f<m.folders;f++)if(!strcmp(m.folder[f].name,GamesFolder))return f;return -1;}
// Without a saved order: the default of this interface with its games in the games folder (where the first one is).
void defaults(AppMenu& m){
  m=AppMenu();int games=-1;
  for(uint8_t i=0;i<uiAppCount;i++){
    if(!isGame(i)){m.top[m.topCount++]=i;continue;}
    if(games<0){games=m.folders++;strcpy(m.folder[games].name,GamesFolder);m.top[m.topCount++]=uint8_t(AppFolder|games);}
    auto& f=m.folder[games];f.apps[f.count++]=i;
  }
}
bool validName(const char* p,size_t n){if(!n||n>=FolderNameMax)return false;for(size_t i=0;i<n;i++)if(p[i]=='{'||p[i]=='}'||(p[i]=='-'&&!i)||uint8_t(p[i])<' ')return false;return true;}
}
bool appsParse(const char* text,AppMenu& m,bool strict){
  if(!*text){defaults(m);return true;}
  m=AppMenu();bool seen[AppsMax]={};int open=-1;
  for(const char* p=text;*p;){
    while(*p==' '||*p==',')p++;if(!*p)break;
    bool off=*p=='-';if(off)p++;const char* end=p;while(*end&&*end!=' '&&*end!=',')end++;
    const char* stop=end;bool close=stop>p&&stop[-1]=='}';if(close)stop--;const char* id=p;p=end;
    if(*id=='{'){ // a folder
      if(open>=0){if(strict)return false;open=-1;}
      size_t n=stop-id-1;if(!validName(id+1,n)||m.folders>=FoldersMax){if(strict)return false;continue;}
      bool twice=false;for(uint8_t f=0;f<m.folders;f++)twice|=strlen(m.folder[f].name)==n&&!strncmp(m.folder[f].name,id+1,n);if(twice){if(strict)return false;continue;}
      uint8_t f=m.folders++;memcpy(m.folder[f].name,id+1,n);m.folder[f].name[n]=0;m.folder[f].hidden=off;m.top[m.topCount++]=uint8_t(AppFolder|f);
      if(!close)open=f;continue;
    }
    if(stop==id){if(open<0&&strict)return false;open=-1;continue;} // "}"
    int i=appIndex(id,stop-id);
    if(i<0||seen[i]){if(strict)return false;}
    else{seen[i]=true;m.hidden[i]=off&&!isSettings(i);
      if(open>=0&&!isSettings(i)){auto& f=m.folder[open];f.apps[f.count++]=uint8_t(i);}else m.top[m.topCount++]=uint8_t(i);}
    if(close){if(open<0&&strict)return false;open=-1;}
  }
  if(open>=0&&strict)return false;
  int games=gamesFolder(m);
  for(uint8_t i=0;i<uiAppCount;i++)if(!seen[i]){if(isGame(i)&&games>=0){auto& f=m.folder[games];f.apps[f.count++]=i;}else m.top[m.topCount++]=i;}
  return true;
}
String appsText(const char* text){
  AppMenu m;appsParse(text,m,false);String s;
  for(uint8_t k=0;k<m.topCount;k++){uint8_t e=m.top[k];if(k)s+=' ';
    if(!(e&AppFolder)){if(m.hidden[e])s+='-';s+=uiApps[e];continue;}
    auto& f=m.folder[e&~AppFolder];if(f.hidden)s+='-';s+='{';s+=f.name;
    for(uint8_t j=0;j<f.count;j++){s+=' ';if(m.hidden[f.apps[j]])s+='-';s+=uiApps[f.apps[j]];}s+='}';}
  return s;
}
// 0.24.0 changed the default order. A saved order that is only an old default (nothing hidden; quick, people and
// wardrive, which came later, anywhere in it) is not a choice: it goes back to the default, any other order stays.
bool appsOldDefault(const char* text){
  static const char* const before[2][12]={{"chats","map","nodes","nav","connect","radar","health","settings","solitaire","chess","pet","dice"},
    {"chats","nodes","chess","pet","dice","radar","gps","wifi","ble","settings","health"}};
  if(!*text||strchr(text,'-')||strchr(text,'{'))return false;
  const char* ids[AppsMax];size_t lens[AppsMax];unsigned n=0;
  for(const char* p=text;*p;){while(*p==' '||*p==',')p++;if(!*p)break;const char* end=p;while(*end&&*end!=' '&&*end!=',')end++;size_t len=end-p;
    bool later=(len==5&&!strncmp(p,"quick",5))||(len==6&&!strncmp(p,"people",6))||(len==8&&!strncmp(p,"wardrive",8));
    if(!later){if(n>=AppsMax)return false;ids[n]=p;lens[n++]=len;}p=end;}
  for(auto& list:before){unsigned k=0;bool same=true;
    for(const char* id:list){if(!id)break;bool named=false;for(unsigned i=0;i<n;i++)if(lens[i]==strlen(id)&&!strncmp(ids[i],id,lens[i]))named=true;
      if(!named)continue;if(k>=n||lens[k]!=strlen(id)||strncmp(ids[k],id,lens[k])){same=false;break;}k++;}
    if(same&&k==n)return true;}
  return false;
}
// 0.33.0 brought the folders: the games of an order saved before go to the games folder, where the first of them
// was; their order and hidden marks stay.
void appsAddGames(char* text,size_t cap){
  if(!*text||strchr(text,'{'))return;
  AppMenu m;appsParse(text,m,false);String s;bool done=false;
  for(uint8_t k=0;k<m.topCount;k++){uint8_t e=m.top[k];if(e&AppFolder)continue;
    if(!isGame(e)){if(s.length())s+=' ';if(m.hidden[e])s+='-';s+=uiApps[e];continue;}
    if(done)continue;done=true;if(s.length())s+=' ';s+='{';s+=GamesFolder;
    for(uint8_t j=k;j<m.topCount;j++){uint8_t g=m.top[j];if(!(g&AppFolder)&&isGame(g)){s+=' ';if(m.hidden[g])s+='-';s+=uiApps[g];}}s+='}';}
  if(s.length()<cap)strlcpy(text,s.c_str(),cap);
}
const AppMenu& appsMenu(){
  static char parsed[sizeof config.apps];static AppMenu menu;static bool ready=false; // in .bss: no flash for an initial value
  if(!ready||strcmp(parsed,config.apps)){ready=true;strlcpy(parsed,config.apps,sizeof parsed);appsParse(config.apps,menu,false);}
  return menu;
}
unsigned appsShown(uint8_t* out,int folder){
  const AppMenu& m=appsMenu();unsigned n=0;
  if(folder>=0){if(folder>=m.folders)return 0;auto& f=m.folder[folder];for(uint8_t j=0;j<f.count;j++)if(!m.hidden[f.apps[j]])out[n++]=f.apps[j];return n;}
  for(uint8_t k=0;k<m.topCount;k++){uint8_t e=m.top[k];
    if(!(e&AppFolder)){if(!m.hidden[e])out[n++]=e;continue;}
    auto& f=m.folder[e&~AppFolder];if(f.hidden)continue;bool any=false;for(uint8_t j=0;j<f.count;j++)any|=!m.hidden[f.apps[j]];if(any)out[n++]=e;}
  return n;
}
int appsFolderOf(uint8_t app){const AppMenu& m=appsMenu();for(uint8_t f=0;f<m.folders;f++)for(uint8_t j=0;j<m.folder[f].count;j++)if(m.folder[f].apps[j]==app)return f;return -1;}
String appsFolderName(uint8_t folder){
  const AppMenu& m=appsMenu();if(folder>=m.folders)return "";const char* n=m.folder[folder].name;
  if(!strcmp(n,GamesFolder))return tr("Games","Игры");String s=n;s.replace('_',' ');return s;
}
