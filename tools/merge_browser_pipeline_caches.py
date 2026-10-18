"""Merge observed browser pipeline definitions; never certify scene coverage."""
import argparse, hashlib, json, sqlite3
from pathlib import Path

def merge(paths, output, report):
    sources=[];records={};versions={}
    for path in paths:
        path=Path(path).resolve()
        db=sqlite3.connect(path.as_uri()+'?mode=ro',uri=True)
        try:
            if db.execute('pragma integrity_check').fetchone()[0]!='ok' or db.execute('select value from aurora_schema').fetchall()!=[(1,)]:
                raise ValueError('Invalid Aurora database: '+str(path))
            rows=db.execute('select type,hash,config_version,config_size,config,first_frame_used from pipeline_cache').fetchall()
            for row in rows:
                kind,key,version,size,config,frame=row
                if not isinstance(config,bytes) or len(config)!=size or size<=0 or not isinstance(frame,int) or frame<0:
                    raise ValueError('Invalid pipeline record')
                if kind in versions and versions[kind]!=(version,size):raise ValueError('Incompatible pipeline config ABI')
                versions[kind]=(version,size)
                previous=records.get((kind,key))
                if previous and previous[2:5]!=row[2:5]:raise ValueError('Conflicting pipeline hash/config')
                records[(kind,key)]=row if not previous else (*row[:5],min(frame,previous[5]))
            sources.append({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'rows':len(rows)})
        finally:db.close()
    output=Path(output).resolve();report=Path(report).resolve()
    if output.exists() or report.exists():raise ValueError('Output already exists')
    db=sqlite3.connect(output)
    try:
        db.executescript('create table aurora_schema(value integer);insert into aurora_schema values(1);create table pipeline_cache(type integer not null,hash integer not null,config_version integer not null,config_size integer not null,config blob not null,first_frame_used integer not null,primary key(type,hash));create index pipeline_cache_load_order_idx on pipeline_cache(type,config_version,first_frame_used);')
        db.executemany('insert into pipeline_cache values(?,?,?,?,?,?)',records.values());db.commit()
        if db.execute('pragma integrity_check').fetchone()[0]!='ok':raise ValueError('Merged integrity failure')
    finally:db.close()
    result={'scope':'observed-scenes-only','fullCoverage':False,'sources':sources,'rows':len(records),'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}
    report.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(json.dumps(result))
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',required=True);parser.add_argument('--report',required=True);parser.add_argument('sources',nargs='+');args=parser.parse_args();merge(args.sources,args.output,args.report)
