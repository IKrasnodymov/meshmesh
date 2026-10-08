// A repeater or room of the mesh (included by Ui.cpp; include/Remote.h): login, status, CLI commands and path
// trace, opened from the node card. A chat node's card opens it for the trace only.
enum RemoteAct {RemLogin,RemStatus,RemCommand,RemTrace};
enum RemoteEdit {EditNone,EditPassword,EditCommand};
RemoteEdit remoteEdit=EditNone;int remoteAction=0;
unsigned remoteActs(const Peer& p,RemoteAct* out){
 unsigned n=0;bool server=p.type==2||p.type==3;remote::Session* s=remote::find(p.id);
 if(server){out[n++]=RemLogin;out[n++]=RemStatus;if(s&&s->login==remote::Done&&s->admin)out[n++]=RemCommand;}
 if(remote::traceable(p.id))out[n++]=RemTrace;return n;
}
String remoteState(remote::State s,const String& done){
 switch(s){case remote::Waiting:return t("waiting for an answer...","ждём ответа...");case remote::Done:return done;case remote::Refused:return t("refused","отказ");case remote::Silent:return t("no answer","нет ответа");default:return "";}
}
String loginText(const Peer& p){
 remote::Session* s=remote::find(p.id);
 if(!s||s->login==remote::Idle)return remote::saved(p.id)?t("not logged in · password saved","вход не выполнен · пароль сохранён"):t("not logged in","вход не выполнен");
 return remoteState(s->login,s->admin?t("logged in as admin","вход: администратор"):t("logged in","вход выполнен"));
}
void drawRemote(){
 Peer* p=focusedPeer();if(!p){textCenter(160,110,t("Node was removed","Узел удалён"),dim);footer({{"BACK",t("Nodes","Узлы")}});return;}
 bool server=p->type==2||p->type==3;remote::Session* s=remote::find(p->id);
 avatar(30,44,16,p->id,p->name,p->type);text(54,42,fit(p->name,256,big),ink,big);
 text(54,58,fit(typeText(p->type)+(server?" · "+loginText(*p):String()),256,small),s&&s->login==remote::Done?ok:dim,small);
 int y=80;
 if(server){
  if(!s||s->status==remote::Idle||(s->status!=remote::Done&&!s->statusAt))text(14,y,s&&s->status!=remote::Idle?t("Status: ","Статус: ")+remoteState(s->status,""):t("Status: not asked yet","Статус: не запрашивался"),dim,small);
  else{
   String cells[6]={t("Battery ","Батарея ")+String(s->battery/1000.0f,2)+t(" V"," В"),t("Up ","Работает ")+uptimeText(s->uptime),t("Rx/tx ","Приём/передача ")+String(s->received)+"/"+String(s->sent),
    t("Noise ","Шум ")+String(s->noise)+" dBm",String("RSSI ")+String(s->rssi)+" · SNR "+String(s->snr/4.0f,1),s->room?t("Posts ","Постов ")+String(s->posted):t("Air ","В эфире ")+uptimeText(s->airtime)};
   for(int i=0;i<6;i++)text(14+(i%2)*150,y+(i/2)*13,fit(cells[i],146,small),ink,small);
   if(s->status!=remote::Done)textRight(306,y+39,remoteState(s->status,""),warn,small);
  }
  y+=44;
 }
 if(remote::trace.id==p->id&&remote::trace.state!=remote::Idle){
  auto& tr=remote::trace;String line=t("Trace: ","Трассировка: ");
  if(tr.state!=remote::Done)line+=remoteState(tr.state,"");
  else{for(unsigned i=0;i<tr.hops;i++){char h[3];snprintf(h,3,"%02X",tr.hashes[i]);line+=String(h)+" "+String(tr.snr[i]/4.0f,1)+" > ";}line+=t("you ","вы ")+String(tr.snr[tr.hops]/4.0f,1);}
  String rows[2];unsigned n=wrap(line,rows,2,292,small);for(unsigned i=0;i<n;i++)text(14,y+i*12,rows[i],tr.state==remote::Done?ink:dim,small);y+=n*12+6;
 } else if(!server){text(14,y,remote::traceable(p->id)?t("Trace shows the SNR at every repeater of the route","Трассировка покажет SNR на каждом ретрансляторе маршрута"):t("No route through repeaters to trace","Нет маршрута через ретрансляторы"),dim,small);y+=18;}
 if(s&&s->replies){text(14,y,t("Replies:","Ответы:"),dim,small);y+=12;for(unsigned i=0;i<s->replies&&y<178;i++,y+=12)text(14,y,fit(s->reply[i],292,small),ink,small);}
 else if(s&&s->command!=remote::Idle)text(14,y,t("Command: ","Команда: ")+remoteState(s->command,""),dim,small);
 RemoteAct acts[4];unsigned n=remoteActs(*p,acts);if(!n){footer({{"BACK",t("Node","Узел")}});return;}remoteAction=constrain(remoteAction,0,int(n)-1);
 const String names[]={t("Login","Войти"),t("Status","Статус"),t("Command","Команда"),t("Trace","Трасса")};
 int w=(304-(n-1)*6)/n;for(unsigned i=0;i<n;i++){int x=8+i*(w+6);bool focus=remoteAction==int(i);target(x,184,w,28,i);panel(x,184,w,28,focus?cardHi:card,7);if(focus)ring(x,184,w,28,7);textCenter(x+w/2,202,fit(names[acts[i]],w-8),ink);}
 footer({{"<>",t("Action","Действие")},{"OK",t("Run","Выполнить")},{"BACK",t("Node","Узел")}});
}
void remoteRun(){
 Peer* p=focusedPeer();if(!p)return;RemoteAct acts[4];unsigned n=remoteActs(*p,acts);if(!n)return;
 switch(acts[constrain(remoteAction,0,int(n)-1)]){
 case RemLogin:remoteEdit=EditPassword;edit="";editing=true;break;
 case RemCommand:remoteEdit=EditCommand;edit="";editing=true;break;
 case RemStatus:{bool sent=remote::status(p->id);notice(sent?t("Status requested","Статус запрошен"):t("Not sent: radio or contact","Не отправлено: радио или контакт"),sent?dim:bad);break;}
 case RemTrace:{bool sent=remote::traceTo(p->id);notice(sent?t("Trace sent","Трассировка отправлена"):t("Not sent: radio or route","Не отправлено: радио или маршрут"),sent?dim:bad);break;}
 }
}
bool remoteKey(int key){
 if(key==KeyBack){change(Node);return true;}
 Peer* p=focusedPeer();RemoteAct acts[4];unsigned n=p?remoteActs(*p,acts):0;
 if(n&&(key==KeyLeft||key==KeyRight||key==KeyUp||key==KeyDown)){remoteAction=(remoteAction+(key==KeyLeft||key==KeyUp?-1:1)+n)%n;return true;}
 if(key==Enter){remoteRun();return true;}
 return false;
}
String remoteEditTitle(){return remoteEdit==EditPassword?t("Password (empty: saved or guest)","Пароль (пусто: сохранённый или гость)"):t("Command to the server","Команда серверу");}
unsigned remoteEditLimit(){return remoteEdit==EditPassword?15:151;}
void remoteSaveEdit(){
 Peer* p=focusedPeer();if(!p){editing=false;return;}
 bool sent=remoteEdit==EditPassword?remote::login(p->id,edit):remote::command(p->id,edit);
 if(remoteEdit==EditCommand&&!edit.length()){notice(t("Type a command, e.g. ver","Введите команду, например ver"),warn);return;}
 editing=false;edit="";notice(sent?(remoteEdit==EditPassword?t("Login sent","Вход отправлен"):t("Command sent","Команда отправлена")):t("Not sent: radio or contact","Не отправлено: радио или контакт"),sent?dim:bad);
}
