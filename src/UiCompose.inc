// Included by UiHeltec.cpp on joystick boards (GAT562): an on-screen keyboard for writing a
// message. Arrows move over the grid, the centre types the key, a centre hold sends, Back erases
// the last character (on an empty draft it closes), a Back hold closes. The input language is
// switched on the keyboard and is independent of the interface language.
namespace {
const char* const layouts[3][2][3]={
 {{"abcdefghijklm","nopqrstuvwxyz","1234567890.,?"},{"ABCDEFGHIJKLM","NOPQRSTUVWXYZ","1234567890.,?"}},
 {{"абвгдежзийклм","нопрстуфхцчшщ","ъыьэюяё.,?!-:"},{"АБВГДЕЖЗИЙКЛМ","НОПРСТУФХЦЧШЩ","ЪЫЬЭЮЯЁ.,?!-:"}},
 {{"!?.,:;-+=/()@","#%&*\"'_<>[]~","0123456789$^|"},{"!?.,:;-+=/()@","#%&*\"'_<>[]~","0123456789$^|"}}};
const char* const layoutNames[3]={"EN","RU","#"};
enum Special {KeyShift,KeyLayout,KeySpace,KeyErase,KeySend,SpecialCount};
bool composing=false,kbShift=true;int kbRow=0,kbCol=0,kbLayout=0;String draft;uint64_t composeTo=meshmesh::Broadcast;String composeName;
// The n-th UTF-8 character of a row.
String cell(const char* row,int n){int i=0;while(row[i]&&n>0){uint8_t c=row[i];i+=c<128?1:(c&0xe0)==0xc0?2:3;n--;}if(!row[i])return "";uint8_t c=row[i];int len=c<128?1:(c&0xe0)==0xc0?2:3;return String(row).substring(i,i+len);}
void eraseLast(){int i=draft.length();if(!i)return;do i--;while(i>0&&(uint8_t(draft[i])&0xc0)==0x80);draft.remove(i);}
void openCompose(uint64_t to,const String& name){composing=true;draft="";composeTo=to;composeName=name;kbRow=kbCol=0;kbShift=true;kbLayout=config.lang==LangRu||config.lang==LangUk?1:0;menuOpen=false;dirty=true;}
void sendDraft(){
 String text=draft;text.trim();if(!text.length()){notice(t("Nothing to send","Пустое сообщение"));return;}
 bool sent=composeTo!=meshRadio.nodeId&&meshRadio.sendMessage(text,composeTo);
 if(sent){composing=false;notice(t("Queued to ","В очереди: ")+composeName);if(channels::isChannel(composeTo)||page==Messages)showPage(Messages);}
 else notice(t("Not sent: radio busy?","Не отправлено"));
}
void typeKey(){
 if(kbRow<3){String c=cell(layouts[kbLayout][kbShift][kbRow],kbCol);if(!c.length())return;
  if(draft.length()+c.length()>meshRadio.messageLimit(composeTo)){notice(t("Message is full","Сообщение заполнено"));return;}
  draft+=c;if(kbShift&&kbLayout<2)kbShift=false;return;}
 switch(kbCol){
 case KeyShift:kbShift=!kbShift;break;
 case KeyLayout:kbLayout=(kbLayout+1)%3;break;
 case KeySpace:if(draft.length()<meshRadio.messageLimit(composeTo))draft+=' ';break;
 case KeyErase:eraseLast();break;
 case KeySend:sendDraft();break;
 }
}
void composeKey(int key){
 int cols=kbRow<3?13:SpecialCount;
 switch(key){
 case 0xb5:kbRow=(kbRow+3)%4;break;
 case 0xb6:kbRow=(kbRow+1)%4;break;
 case 0xb4:kbCol=(kbCol+cols-1)%cols;break;
 case 0xb7:kbCol=(kbCol+1)%cols;break;
 case 13:typeKey();break;
 case 0xa3:sendDraft();break;
 case 0x86:if(draft.length())eraseLast();else composing=false;break;
 case 0x82:composing=false;notice(t("Draft discarded","Черновик удалён"));break;
 }
 // Columns map between the 13-key rows and the 5 special keys by position.
 if(key==0xb5||key==0xb6){if(kbRow==3&&kbCol>=SpecialCount)kbCol=kbCol*SpecialCount/13;}
}
void drawCompose(){
 auto& c=*hardware.canvas;c.fillScreen(0);
 // Draft: the tail that fits, a cursor, and the bytes left.
 unsigned left=meshRadio.messageLimit(composeTo)-min(unsigned(draft.length()),meshRadio.messageLimit(composeTo));
 String tail=draft;while(chars(tail)>20){int i=0;uint8_t b=tail[0];i=b<128?1:(b&0xe0)==0xc0?2:3;tail.remove(0,i);}
 say(0,8,draft.length()?tail+((millis()/500)%2?"_":" "):t("To ","Кому: ")+clipped(composeName,15),small);
 sayRight(128,8,String(left),small);c.drawFastHLine(0,10,128,1);
 for(int r=0;r<3;r++)for(int k=0;k<13;k++){
  String ch=cell(layouts[kbLayout][kbShift][r],k);int x=1+k*9+(r==2?0:0),y=20+r*11;bool focus=r==kbRow&&k==kbCol;
  if(focus)c.fillRect(x,y-9,9,11,1);useFont(u8g2_font_6x12_t_cyrillic);int w=hardware.font.getUTF8Width(ch.c_str());hardware.text(x+(9-w)/2,y,ch,!focus);
 }
 const char* labels[SpecialCount]={kbShift?"ABC":"abc",layoutNames[(kbLayout+1)%3],tr("space","пробел"),"<-",tr("Send","Отпр.")};
 const int xs[SpecialCount+1]={0,22,40,82,100,128};
 for(int k=0;k<SpecialCount;k++){bool focus=kbRow==3&&k==kbCol;int x=xs[k],w=xs[k+1]-xs[k];
  if(focus)c.fillRect(x,54,w-1,10,1);else c.drawRect(x,54,w-1,10,1);int tw=width(labels[k],small);say(x+(w-1-tw)/2,62,labels[k],small,!focus);}
 if(action.length()&&millis()-actionAt<2500){c.fillRect(0,22,128,20,0);c.drawRect(0,22,128,20,1);say(3,36,clipped(action,20));}
 hardware.flush();
}
}
