// Radio actions added by the UI; kept here so the preview links.
bool MeshRadio::resetPath(uint64_t id){Peer* p=contact(id);if(!p)return false;p->pathLength=255;return true;}
bool MeshRadio::removeContact(uint64_t id){Peer* p=contact(id);if(!p)return false;unsigned i=p-peers;memmove(peers+i,peers+i+1,sizeof(Peer)*(peerCount-i-1));peerCount--;return true;}
