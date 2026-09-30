#!/usr/bin/env python3
"""Test the real M9 protocol handlers through USB using an independent AES-GCM peer.

This tests the protocol, not reception by a second physical radio.
It transmits a few test packets at the configured power and adds USB-TEST chat rows.
"""
import argparse
import hashlib
import json
import struct
import time
from pathlib import Path
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from device import connect,command
from ports import M9_PORT, HELTEC_PORT

HEADER=struct.Struct('<2sBBIQQIIBBBB')
ALL=2**64-1
PEER=0x112233445566

def seal(key,kind,source,destination,session,sequence,body,hops=0,maximum=3):
    network=int.from_bytes(hashlib.sha256(key).digest()[:4],'little')
    header=HEADER.pack(b'MM',1,kind,network,source,destination,session,sequence,maximum,hops,len(body),0)
    iv=struct.pack('<QII',source,session,sequence)
    return header+AESGCM(key).encrypt(iv,body,header[:33]+header[34:36])

def unseal(key,raw):
    h=HEADER.unpack(raw[:36]);iv=struct.pack('<QII',h[4],h[6],h[7])
    return h,AESGCM(key).decrypt(iv,raw[36:],raw[:33]+raw[34:36])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--port',default=M9_PORT);p.add_argument('--output',type=Path,default=Path('artifacts/protocol-check.json'));a=p.parse_args()
    checks=[]
    def check(name,truth):
        checks.append({'name':name,'pass':bool(truth)});print(('PASS ' if truth else 'FAIL ')+name)
        if not truth:raise AssertionError(name)
    with connect(a.port) as s:
        cfg=json.loads(command(s,'key'));key=bytes.fromhex(cfg['key'])
        node=int(json.loads(command(s,'status'))['node'],16)
        check('Device crypto self-test',command(s,'selftest').startswith('OK'))
        before=json.loads(command(s,'status'));peer_session=int(time.time())
        name=b'USB-TEST';text='Привет с независимого виртуального узла'.encode()
        body=struct.pack('<I',901)+bytes([len(name)])+name+text
        raw=seal(key,1,PEER,node,peer_session,1,body)
        check('Independent AES-GCM packet accepted',command(s,'ingest '+raw.hex()).startswith('OK'))
        history=json.loads(command(s,'messages'))
        check('Incoming UTF-8 message delivered to chat',any(m['text']==text.decode() and m['source']==f'{PEER:012X}' for m in history))
        frame=bytes.fromhex(command(s,'txframe'));h,ack=unseal(key,frame)
        check('Device encrypted ACK decodes independently',h[2]==2 and h[5]==PEER and struct.unpack('<II',ack)==(peer_session,901))
        count=len(history)
        check('Duplicate packet accepted without duplicate chat',command(s,'ingest '+raw.hex()).startswith('OK') and len(json.loads(command(s,'messages')))==count)
        retry=seal(key,1,PEER,node,peer_session,2,body)
        check('Retry gets ACK without duplicate chat',command(s,'ingest '+retry.hex()).startswith('OK') and len(json.loads(command(s,'messages')))==count)
        tampered=bytearray(seal(key,1,PEER,node,peer_session,3,body));tampered[-1]^=1
        check('Tampered ciphertext rejected',command(s,'ingest '+tampered.hex()).startswith('ERR'))
        alien=seal(bytes([0x17])*32,1,PEER,node,peer_session,4,body)
        check('Other network key rejected',command(s,'ingest '+alien.hex()).startswith('ERR'))
        bad_body=struct.pack('<I',902)+b'\x01\xd0\x90'
        check('UTF-8 split between name/text rejected',command(s,'ingest '+seal(key,1,PEER,node,peer_session,5,bad_body).hex()).startswith('ERR'))
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            cfg_reply=command(s,'set {"power":0}')
            if cfg_reply.startswith('OK'):break
            time.sleep(.5)
        check('Radio settings accepted',cfg_reply.startswith('OK'))
        check('Outbound message queued',command(s,f'send {PEER:012X} USB-TEST outgoing').startswith('OK'))
        h,body=unseal(key,bytes.fromhex(command(s,'txframe')))
        original_id=struct.unpack('<I',body[:4])[0]
        check('Device message decodes independently',h[2]==1 and h[5]==PEER and body[5+body[4]:]==b'USB-TEST outgoing')
        ack=seal(key,2,PEER,node,peer_session,6,struct.pack('<II',h[6],original_id))
        check('Delivery ACK accepted',command(s,'ingest '+ack.hex()).startswith('OK'))
        history=json.loads(command(s,'messages'))
        check('Only matching ACK marks DELIVERED',any(m['outgoing'] and m['id']==original_id and m['status']==3 for m in history))
        # Check the hardware TX-complete interrupt independently of the virtual peer.
        time.sleep(4);after=json.loads(command(s,'status'))
        check('Physical LR1110 reports TxDone',after['tx']>before['tx'] and after['radio_error']==0)
        check('USB injection is counted separately from physical RX',after['rx']==before['rx'] and after['diagnostic_rx']>=before['diagnostic_rx']+3)
        command(s,'set '+json.dumps({'power':cfg['power']}))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'transport':'USB simulated peer; physical TxDone only','checks':checks,'status':after},ensure_ascii=False,indent=2)+'\n')

if __name__=='__main__':main()
