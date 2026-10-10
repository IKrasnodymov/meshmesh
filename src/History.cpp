#if defined(MM_NRF52)
#pragma GCC optimize("Os")
#endif
#include "MeshRadio.h"
#include <time.h>
static uint32_t upSeconds(){return uint32_t(millis()/1000)+1;}
namespace {
unsigned retainedLimit(const MeshRadio& radio,const ChatMessage& m){auto* c=radio.channel(m.destination);return c?(c->policy.deviceLimit?c->policy.deviceLimit:c->policy.priority==0?16:64):64;}
unsigned historyPriority(const MeshRadio& radio,const ChatMessage& m){auto* c=radio.channel(m.destination);return c?c->policy.priority:1;}
}
bool MeshRadio::historyProtected(const ChatMessage& m) const {for(const auto& p:pending)if(p.active&&!p.message.game&&p.message.source==m.source&&p.message.session==m.session&&p.message.id==m.id)return true;return false;}
void MeshRadio::historyErase(unsigned index){if(index>=historyCount)return;memmove(history+index,history+index+1,(historyCount-index-1)*sizeof(ChatMessage));historyCount--;historyGeneration++;compactRequested=true;}
void MeshRadio::trimHistory(){for(unsigned i=0;i<historyCount;){unsigned n=0;for(unsigned j=i;j<historyCount;j++)if(history[j].destination==history[i].destination)n++;if(n>retainedLimit(*this,history[i])&&!historyProtected(history[i]))historyErase(i);else i++;}compactRequested=true;dirty=true;}
void MeshRadio::addMessage(const ChatMessage& m,bool save) {
  if(save&&!m.outgoing&&!m.game)received++;
  unsigned n=0,old=64;for(unsigned i=0;i<historyCount;i++)if(history[i].destination==m.destination){n++;if(old==64&&!historyProtected(history[i]))old=i;}
  if(n>=retainedLimit(*this,m)){if(old==64)return;historyErase(old);}
  if(historyCount==64){
    // A bounded channel gets its requested depth before recycling its own rows.
    // Without this, a new low-priority channel can stay invisible in a full history.
    bool filling=retainedLimit(*this,m)<64&&n<retainedLimit(*this,m);
    unsigned victim=64,priority=3;
    for(unsigned i=0;i<historyCount;i++)if(!historyProtected(history[i])&&(!filling||history[i].destination!=m.destination)){
      unsigned rank=historyPriority(*this,history[i]);if(victim==64||rank<priority){victim=i;priority=rank;}}
    if(victim==64)return;historyErase(victim);
  }
  ChatMessage& added=history[historyCount++];added=m;historyGeneration++;
  if(save&&!clockSet()){added.timestamp=0;added.uptime=upSeconds();unstamped++;}
  if(save)persist(added);dirty=true;
}
