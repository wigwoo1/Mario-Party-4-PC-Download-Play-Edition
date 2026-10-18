import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {controlGlyph} from '../web/control-glyphs.mjs';
const destination=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../assets/control-glyphs');
fs.mkdirSync(destination,{recursive:true});
for(const family of ['xbox','playstation','switch','generic'])
  for(const button of ['south','east','west','north','l1','r1','l2','r2','start','select'])
    fs.writeFileSync(path.join(destination,`${family}-${button}.svg`),controlGlyph({family,binding:{kind:'button',button}}).svg+'\n');
for(const label of ['Enter','Space','Esc','Z','X','W','A','S','D'])
  fs.writeFileSync(path.join(destination,`key-${label.toLowerCase()}.svg`),controlGlyph({binding:{kind:'key',label}}).svg+'\n');
console.log('Generated 49 SVG control glyphs.');
const files=fs.readdirSync(destination).filter(name=>name.endsWith('.svg'));
fs.writeFileSync(path.join(destination,'preview.html'),`<!doctype html><meta charset="utf-8"><title>Control glyphs</title><style>body{background:#111820;color:#fff;font:14px Arial;margin:24px}main{display:grid;grid-template-columns:repeat(7,140px);gap:12px}figure{margin:0;text-align:center;padding:8px;background:#1b2531;border-radius:8px}img{width:64px;height:64px}figcaption{margin-top:5px;font-size:11px}</style><h1>Control glyphs</h1><main>${files.map(name=>`<figure><img src="${name}" alt="${name}"><figcaption>${name.slice(0,-4)}</figcaption></figure>`).join('')}</main>`);
