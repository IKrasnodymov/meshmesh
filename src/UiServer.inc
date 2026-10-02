// Device role (included by Ui.cpp): the choice offered for 5 s after boot or from Settings, and the
// home page of a repeater or room server with its MeshCore settings. Changes go through the server's
// CLI, as from the MeshCore app.
bool bootPick=false;uint32_t bootPickAt=0;
constexpr uint32_t bootPickMs=5000;
enum ServerRow {RowForward,RowAdmin,RowGuest,RowReadOnly,RowPost,RowAdvert,RowFlood,RowAnnounce,RowMode};
const ServerRow repeaterRows[]={RowForward,RowAdmin,RowGuest,RowAdvert,RowFlood,RowAnnounce,RowMode};
const ServerRow roomRows[]={RowPost,RowAdmin,RowGuest,RowReadOnly,RowAdvert,RowFlood,RowForward,RowAnnounce,RowMode};
int serverRowCount(){return config.role==RoleRoom?9:7;}
ServerRow serverRow(int i){return config.role==RoleRoom?roomRows[constrain(i,0,8)]:repeaterRows[constrain(i,0,6)];}
bool serverPostEdit(){return page==ServerHome&&serverRow(selected)==RowPost;}
unsigned serverEditLimit(){return serverPostEdit()?151:15;}
Page homePage(){return config.role==RoleNormal?Home:ServerHome;}
String roleTitle(int role){return role==RoleRepeater?t("MeshCore repeater","Репитер MeshCore"):role==RoleRoom?t("MeshCore room server","Комната MeshCore"):t("Normal mode","Обычный режим");}
void drawRolePick(){
 for(int r=0;r<RoleCount;r++){
  int y=24+r*58;bool focus=selected==r,current=config.role==r;panel(8,y,304,54,focus?cardHi:card,8);if(focus)ring(8,y,304,54,8);
  Icon ic=r==RoleRepeater?IcTower:r==RoleRoom?IcRoom:IcChat;uint16_t hue=r==RoleRepeater?violet:r==RoleRoom?warn:accent;
  g().fillRoundRect(16,y+8,28,28,7,bg);icon(ic,30,y+22,8,hue,bg);text(54,y+19,roleTitle(r),ink,bold);
  if(current){String now=t("running","сейчас");int w=measure(now,small)+10;g().fillRoundRect(302-w,y+8,w,14,7,rgb(0x14524b));textCenter(302-w/2,y+18,now,accent,small);}
  const char* about[][2]={{tr("Screen, chats, maps, radar and games","Экран, чаты, карты, радар и игры"),tr("A MeshCore chat node; relays as set","Узел-собеседник MeshCore; пересылка по настройке")},
   {tr("Relays the network's packets; no chats","Пересылает пакеты сети; без чатов"),tr("Login and settings from the MeshCore app","Вход и настройка из приложения MeshCore")},
   {tr("Keeps posts and hands them to members","Хранит посты и раздаёт их участникам"),tr("Room password and settings from the MeshCore app","Пароль комнаты и настройка из приложения MeshCore")}};
  for(int i=0;i<2;i++)text(54,y+33+i*12,fit(about[r][i],250,small),dim,small);
 }
 if(bootPick){uint32_t gone=millis()-bootPickAt;unsigned left=gone>=bootPickMs?0:(bootPickMs-gone+999)/1000;textCenter(160,212,t("Continues as now in ","Продолжение в текущем режиме через ")+String(left)+t(" s"," с"),warn);}
 else textCenter(160,212,t("A change restarts; data and settings stay","Смена перезапускает; данные и настройки сохраняются"),faint,small);
 footer({{"OK",t("Choose","Выбрать")},{"^v",t("Select","Выбор")},{"BACK",bootPick?t("Continue","Продолжить"):t("Cancel","Отмена")}});
}
bool rolePickKey(int key){
 bootPick=false;
 if(key==KeyUp||key==KeyDown){selected=(selected+(key==KeyUp?RoleCount-1:1))%RoleCount;return true;}
 if(key==KeyBack){change(homePage());return true;}
 if(key!=Enter)return false; // HOME, CTRL, ADV keep their meaning
 if(selected==config.role){change(homePage());return true;}
 String reply=setRole(selected);bool done=reply.startsWith("OK");notice(done?t("Restarting: ","Перезапуск: ")+roleTitle(selected):reply,done?ok:bad);return true;
}
void openRolePick(bool atBoot){change(RolePick);selected=config.role;bootPick=atBoot;bootPickAt=millis();}

// Server page: status, then the settings an owner changes on the spot.
String uptimeText(uint32_t s){return s<3600?String(s/60)+t(" min"," мин"):s<86400?String(s/3600)+t(" h "," ч ")+String(s/60%60)+t(" min"," мин"):String(s/86400)+t(" d "," д ")+String(s/3600%24)+t(" h"," ч");}
String serverValue(const ServerView& v,ServerRow r){
 switch(r){case RowForward:return flag(v.forwarding);case RowAdmin:return v.password;case RowGuest:return v.guest.length()?v.guest:t("none","нет");case RowReadOnly:return flag(v.readOnlyLogin);
 case RowAdvert:return v.advertMinutes?String(v.advertMinutes)+t(" min"," мин"):t("Off","Выкл");case RowFlood:return v.floodAdvertHours?String(v.floodAdvertHours)+t(" h"," ч"):t("Off","Выкл");
 case RowMode:return roleTitle(config.role);default:return "";}
}
String serverName(ServerRow r){
 bool room=config.role==RoleRoom;
 switch(r){case RowForward:return t("Forwarding","Пересылка");case RowAdmin:return t("Admin password","Пароль администратора");case RowGuest:return room?t("Room password","Пароль комнаты"):t("Guest password","Гостевой пароль");
 case RowReadOnly:return t("Read without password","Чтение без пароля");case RowPost:return t("Post to the room","Написать в комнату");case RowAdvert:return t("Nearby advert","Объявление рядом");
 case RowFlood:return t("Network advert","Объявление на сеть");case RowAnnounce:return t("Advertise now","Объявить сейчас");case RowMode:return t("Device mode","Режим работы");}return "";
}
String serverHint(ServerRow r){
 bool room=config.role==RoleRoom;
 switch(r){case RowForward:return room?t("Forward other nodes' packets (off for a room)","Пересылать чужие пакеты (у комнаты обычно выкл.)"):t("Forward other MeshCore nodes' packets","Пересылать пакеты других узлов MeshCore");
 case RowAdmin:return t("Admin login from the MeshCore app; OK: change","Вход администратора из приложения MeshCore; OK: изменить");
 case RowGuest:return room?t("Members read and post with it; none: blank","Участники читают и пишут; нет - пустой пароль"):t("Read-only login; none: a blank password logs in","Вход только для чтения; нет - вход с пустым паролем");
 case RowReadOnly:return t("Any password gives read-only access","С любым паролем - только чтение");case RowPost:return t("A post from the room itself; OK: write","Пост от имени комнаты; OK: написать");
 case RowAdvert:return t("Zero-hop advert: off or 60-240 min","Объявление без пересылки: выкл или 60-240 мин");case RowFlood:return t("Flooded advert: off or 3-168 h","Объявление на всю сеть: выкл или 3-168 ч");
 case RowAnnounce:return t("Tell the whole network about this node","Сообщить об узле всей сети");case RowMode:return t("Normal, repeater or room; a change restarts","Обычный, репитер или комната; смена перезапускает");}return "";
}
void drawServer(){
 ServerView v=meshServer.view();auto& d=g();bool room=config.role==RoleRoom;
 panel(8,25,304,58,card,8);d.fillRoundRect(16,33,26,26,6,bg);icon(room?IcRoom:IcTower,29,46,8,!meshRadio.ready?bad:room?warn:v.forwarding?violet:bad,bg);
 text(50,40,fit(String(config.name),150,bold),ink,bold);
 text(50,54,fit(meshRadio.ready?String(config.frequency,3)+t(" MHz · SF"," МГц · SF")+String(config.sf)+" · "+String(config.power)+" dBm":t("Radio error ","Ошибка радио ")+String(meshRadio.radioError),170,small),meshRadio.ready?dim:bad,small);
 textRight(302,40,String(meshRadio.txCount),ink,small);tri(302-measure(String(meshRadio.txCount),small)-7,37,0,3,accent);
 textRight(302,54,String(meshRadio.rxCount),ink,small);tri(302-measure(String(meshRadio.rxCount),small)-7,51,2,3,ok);
 String line=room?t("Posts ","Постов ")+String(v.posts)+t(" · members "," · вошли ")+String(v.clients):t("Relayed ","Переслано ")+String(meshRadio.relayed)+t(" · neighbours "," · соседей ")+String(v.neighbours)+t(" · logged in "," · вошли ")+String(v.clients);
 text(16,76,fit(line+" · "+uptimeText(v.uptime),288,small),dim,small);
 int rows=serverRowCount(),first=max(0,selected-4);
 for(int i=first;i<rows&&i<first+5;i++){
  int y=87+(i-first)*22;bool focus=selected==i;ServerRow r=serverRow(i);
  if(r==RowAnnounce||r==RowPost){panel(8,y,304,21,focus?accent:card,6);textCenter(160,y+15,serverName(r),focus?bg:ink,bold);continue;}
  listRow(y,21,focus);text(24,y+15,serverName(r),focus?ink:dim);
  if(r==RowForward||r==RowReadOnly){toggle(274,y+3,r==RowForward?v.forwarding:v.readOnlyLogin);continue;}
  String value=serverValue(v,r);bool opens=r==RowMode||r==RowAdmin||r==RowGuest;
  if(focus&&(r==RowAdvert||r==RowFlood)){int w=measure(value,bold);tri(300,y+10,1,4,accent);text(292-w,y+15,value,accent,bold);tri(284-w,y+10,3,4,accent);}
  else{textRight(opens?292:300,y+15,fit(value,140),focus?accent:ink,focus?bold:body);if(opens)tri(302,y+10,1,4,focus?accent:faint);}
 }
 scrollbar(first,5,rows,87,108);text(10,210,fit(serverHint(serverRow(selected)),300,small),faint,small);
 ServerRow r=serverRow(selected);
 if(r==RowAdvert||r==RowFlood)footer({{"<>",t("Change","Изменить")},{"^v",t("Select","Выбор")},{"ADV",t("Advertise","Объявить")}});
 else footer({{"OK",r==RowForward||r==RowReadOnly?t("Switch","Переключить"):t("Open","Открыть")},{"^v",t("Select","Выбор")},{"ADV",t("Advertise","Объявить")},{"CTRL",t("Settings","Настройки")}});
}
void serverNotice(const String& reply){bool done=reply.startsWith("OK")||reply.startsWith("password now");notice(done?t("Saved","Сохранено"):reply,done?ok:bad);}
// Intervals step through the values the CLI accepts.
void serverAlter(int dir){
 ServerView v=meshServer.view();ServerRow r=serverRow(selected);
 if(r==RowForward)serverNotice(meshServer.command(v.forwarding?"set repeat off":"set repeat on"));
 if(r==RowReadOnly)serverNotice(meshServer.command(v.readOnlyLogin?"set allow.read.only off":"set allow.read.only on"));
 if(r==RowAdvert){int m=v.advertMinutes;m=dir>0?(m?min(240,m+30):60):(m<=60?0:m-30);serverNotice(meshServer.command("set advert.interval "+String(m)));}
 if(r==RowFlood){const int steps[]={0,3,6,12,24,47,72,120,168};int n=sizeof(steps)/sizeof(*steps),i=0;
  if(dir>0){while(i<n-1&&steps[i]<=int(v.floodAdvertHours))i++;}else{i=n-1;while(i>0&&steps[i]>=int(v.floodAdvertHours))i--;}
  serverNotice(meshServer.command("set flood.advert.interval "+String(steps[i])));}
}
bool serverKey(int key){
 int rows=serverRowCount();
 if(key==KeyUp||key==KeyDown){selected=(selected+(key==KeyUp?rows-1:1))%rows;return true;}
 if(key==KeyLeft||key==KeyRight){serverAlter(key==KeyLeft?-1:1);return true;}
 if(key!=Enter)return false;
 switch(serverRow(selected)){
 case RowForward:case RowReadOnly:case RowAdvert:case RowFlood:serverAlter(1);break;
 case RowAdmin:editing=true;edit=meshServer.view().password;break;
 case RowGuest:editing=true;edit=meshServer.view().guest;break;
 case RowPost:editing=true;edit="";break;
 case RowAnnounce:{bool sent=meshServer.advertise(true);notice(sent?t("Announced to the network","Узел объявлен в сети"):t("Announcement failed","Не удалось объявить"),sent?ok:bad);break;}
 case RowMode:openRolePick(false);break;
 }
 return true;
}
String serverEditTitle(){ServerRow r=serverRow(selected);return r==RowPost?t("Post to the room","Пост в комнату"):serverName(r);}
String serverEditHint(){ServerRow r=serverRow(selected);return r==RowPost?t("Up to 151 bytes; @: input language","До 151 байта; @ - язык ввода"):r==RowAdmin?t("1-15 characters; -> after a letter: capital","1-15 символов; -> после буквы: заглавная"):t("0-15 characters; empty: blank login","0-15 символов; пусто - вход без пароля");}
// Passwords: up to 15 bytes of printable ASCII; the admin password cannot be empty (a blank login would be admin).
void serverSaveEdit(){
 ServerRow r=serverRow(selected);
 if(r==RowPost){if(!edit.length()){notice(t("Empty post","Пустой пост"),bad);return;}bool stored=meshServer.post(edit);notice(stored?t("Posted; members get it in turn","Пост сохранён; участники получат его по очереди"):t("Post not stored","Пост не сохранён"),stored?ok:bad);if(stored)editing=false;return;}
 for(unsigned i=0;i<edit.length();i++)if(edit[i]<32||edit[i]>126){notice(t("Latin letters, digits and symbols only","Только латиница, цифры и символы"),bad);return;}
 if(r==RowAdmin&&!edit.length()){notice(t("Admin password cannot be empty","Пароль администратора не может быть пустым"),bad);return;}
 if(edit.length()>15){notice(t("Up to 15 characters","Не более 15 символов"),bad);return;}
 serverNotice(meshServer.command(r==RowAdmin?"password "+edit:"set guest.password "+edit));editing=false;
}
