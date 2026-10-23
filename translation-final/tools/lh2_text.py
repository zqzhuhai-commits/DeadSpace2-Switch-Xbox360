"""Dead Space 2 Xbox 360 LH2 UTF-8 text export/rebuild. Python 3, no dependencies."""
import argparse,json,struct,re
from pathlib import Path

def parse(p):
 if p[:4]!=b'2HCL':raise ValueError('Expected big-endian Xbox 360 LH2 (2HCL)')
 if int.from_bytes(p[4:8],'big')!=len(p):raise ValueError('LH2 length mismatch')
 n=int.from_bytes(p[16:20],'big');ids=struct.unpack_from(f'>{n}I',p,32);offs=struct.unpack_from(f'>{n}I',p,32+4*n)
 if offs[0]!=32+8*n:raise ValueError('Unsupported table layout')
 rows=[]
 for i,(key,off) in enumerate(zip(ids,offs)):
  end=p.index(0,off)
  if end+1!=(offs[i+1] if i+1<n else len(p)):raise ValueError('Non-contiguous string pool')
  rows.append({'index':i,'id':f'{key:08X}','offset':off,'source':p[off:end].decode('utf-8'),'translation':''})
 return rows

def rebuild(p,rows):
 original=parse(p)
 if len(rows)!=len(original):raise ValueError('Row count changed')
 n=len(rows);result=bytearray(p[:32+8*n]);pool=bytearray()
 for old,row in zip(original,rows):
  if any(row[k]!=old[k] for k in ['index','id','source']):raise ValueError('Source/index/id changed')
  text=row.get('translation') or row['source']
  if '\0' in text:raise ValueError('NUL in translation')
  tokens=lambda s:sorted(re.findall(r'\[(?i:c=[^\]]+|menu[ab]|[lr]s(?:top|bottom)|[lr]d(?:up|down|left|right)|[lr]stick3?|select|start)\]|\{[^}\r\n]+\}|%(?:\d+\$)?[-+0#]*\d*(?:\.\d+)?[gds]|%\d+',s))
  if tokens(text)!=tokens(old['source']):raise ValueError(f'Control tokens changed: {row["id"]}')
  struct.pack_into('>I',result,32+4*n+4*old['index'],len(result)+len(pool));pool+=text.encode('utf-8')+b'\0'
 result+=pool;struct.pack_into('>I',result,4,len(result));parse(bytes(result));return bytes(result)

if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('mode',choices=['export','rebuild']);a.add_argument('original');a.add_argument('json');a.add_argument('output',nargs='?');v=a.parse_args();p=Path(v.original).read_bytes()
 if v.mode=='export':Path(v.json).write_text(json.dumps(parse(p),ensure_ascii=False,indent=2),encoding='utf-8')
 else:
  if not v.output:a.error('rebuild requires output path')
  dst=Path(v.output)
  if dst.resolve()==Path(v.original).resolve():raise ValueError('Use a separate output file')
  dst.write_bytes(rebuild(p,json.loads(Path(v.json).read_text(encoding='utf-8-sig'))))
