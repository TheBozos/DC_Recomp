"""Read-only artifact inventory for LoadChara2; validator checks exceed the game.

Only metadata/hashes are emitted. These are snapshot sizes, not runtime constants.
No payload is extracted and no generic alias-flow result is discharged here.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import sys

sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent))
from dc_loadchara_audit import SITES, size_envelope


def pack_nodes(blob, limit=10000):
    nodes=[];seen=set();at=0
    while True:
        if not 0<=at<len(blob):raise ValueError('pack node outside outer file')
        if blob[at]==0:return nodes
        if at in seen:raise ValueError('pack node cycle')
        if len(nodes)>=limit:raise ValueError('pack node limit')
        seen.add(at)
        if at+80>len(blob):raise ValueError('pack header outside outer file')
        nul=blob.find(b'\0',at,at+64)
        if nul<0:raise ValueError('pack name lacks header terminator')
        relative,size,nxt=struct.unpack_from('<3I',blob,at+64)
        start=at+relative;end=start+size
        if not at+80<=start<=end<=len(blob):raise ValueError('pack payload outside outer file or in header')
        nodes.append(dict(name=blob[at:nul].decode('latin-1'),node=at,payload_relative=relative,
                          payload=start,size=size,next_relative=nxt,payload_end=end))
        if not nxt:raise ValueError('nonterminal pack node has zero next')
        # Current artifacts use forward relative words. This validator does not
        # model signed, wrapping, malformed game traversal or arbitrary overlaps.
        at+=nxt


def terminated(data, at, end):
    if not 0<=at<end<=len(data):raise ValueError('name outside artifact')
    nul=data.find(b'\0',at,end)
    if nul<0:raise ValueError('unterminated artifact name')
    return data[at:nul].decode('latin-1')


def inventory(root, regions):
    game=root/'game';boot=(game/'SCUS_971.11').read_bytes();dun=(game/'DUN.BIN').read_bytes()
    hed=(game/'DATA.HED').read_bytes();hd2=(game/'DATA.HD2').read_bytes();dat=game/'DATA.DAT'
    if len(hed)%80:raise ValueError('partial HED record')
    phoff=struct.unpack_from('<I',boot,28)[0]
    segments=[struct.unpack_from('<8I',boot,phoff+32*i) for i in range(struct.unpack_from('<H',boot,44)[0])]
    def resident(at,size):
        for p in segments:
            if p[0]==1 and p[2]<=at and at+size<=p[2]+p[4]:return boot[p[1]+at-p[2]:p[1]+at-p[2]+size]
        raise ValueError('resident table/name not file backed')
    table=[]
    for selector in range(6):
        pointer=struct.unpack('<I',resident(0x27e7b0+4*selector,4))[0]
        raw=resident(pointer,64)
        table.append(dict(selector=selector,table_address=0x27e7b0+4*selector,name_address=pointer,
                          name=terminated(raw,0,len(raw))))
    normalize=lambda n:n.replace('/','\\').lower()
    selected={normalize(x['name']):x['selector'] for x in table}
    base=struct.unpack_from('<I',dun,8)[0]
    predicates={0x1dba6d8:0x24020005,0x1dba6dc:0x16a2004d,0x1dba6e0:0,
                0x1dba814:0x24020003,0x1dba818:0x16a2001e,0x1dba81c:0}
    if any(struct.unpack_from('<I',dun,pc-base)[0]!=value for pc,value in predicates.items()):
        raise ValueError('LoadChara2 artifact selector predicates changed')
    requests={}
    for (_,pc,_,_,name),selector in zip(SITES,(5,5,3)):
        text=terminated(dun,name-base,min(name-base+64,len(dun)))
        requests[text.lower()]=dict(copy_pc=pc,name_address=name,name=text,selector=selector)
    packs=[];members=[];dat_size=dat.stat().st_size
    with dat.open('rb') as f:
        for i in range(len(hed)//80):
            name=terminated(hed,i*80,i*80+64)
            if normalize(name) not in selected:continue
            offset,size,sector,sectors=struct.unpack_from('<4I',hed,i*80+64)
            hrow=struct.unpack_from('<8I',hd2,i*32)
            hdname=terminated(hd2,hrow[0],len(hd2))
            if normalize(hdname)!=normalize(name) or hrow[4:]!=(offset,size,sector,sectors):
                raise ValueError('selected HED/HD2 records disagree')
            if sector*2048!=offset or size>sectors*2048 or offset+size>dat_size:
                raise ValueError('selected outer pack exceeds DATA.DAT/sector bounds')
            f.seek(offset);blob=f.read(size)
            if len(blob)!=size:raise ValueError('short artifact read')
            nodes=pack_nodes(blob);digest=hashlib.sha256(blob).hexdigest()
            selector=selected[normalize(name)]
            packs.append(dict(name=name,selector=selector,hed_record=i,offset=offset,size=size,sha256=digest,nodes=nodes))
            for node in nodes:
                request=requests.get(node['name'].lower())
                if request is None:continue
                if selector!=request['selector']:raise ValueError('requested member in an unexpected initial selector pack')
                members.append(dict(request,**{k:v for k,v in node.items() if k!='name'},outer_name=name,
                                    outer_sha256=digest,payload_sha256=hashlib.sha256(blob[node['payload']:node['payload_end']]).hexdigest(),
                                    conditional_envelope=size_envelope(node['size'],regions)))
    if len(packs)!=6 or len(members)!=3 or len({x['copy_pc'] for x in members})!=3:
        raise ValueError('initial selector packs/request members are missing or duplicated')
    sources={}
    for name in ('SCUS_971.11','DUN.BIN','DATA.HED','DATA.HD2','DATA.DAT'):
        path=game/name
        with path.open('rb') as f:sources['game/'+name]=dict(bytes=path.stat().st_size,sha256=hashlib.file_digest(f,'sha256').hexdigest())
    return dict(selector_table=table,packs=packs,requested_members=members,sources=sources,
                limit='artifact snapshot only; complete loads, selector/path identity, preserved source headers and valid child cursor are unproved runtime preconditions; scanner bounds checks are not game checks')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=pathlib.Path,default=pathlib.Path.cwd())
    p.add_argument('--audit-report',type=pathlib.Path,required=True,help='current dc_packet_audit report supplying watched ranges')
    args=p.parse_args()
    regions=json.loads(args.audit_report.read_text(encoding='utf-8'))['pointer_alias_audit']['regions']
    report=inventory(args.root,regions)
    report['audit_report_sha256']=hashlib.sha256(args.audit_report.read_bytes()).hexdigest()
    print(json.dumps(report,indent=2))
