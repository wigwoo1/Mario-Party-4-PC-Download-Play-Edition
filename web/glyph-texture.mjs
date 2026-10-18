import {controlGlyph} from './control-glyphs.mjs';

// Produce owned straight-alpha RGBA pixels from our generated glyphs only.
export async function glyphTexture({family,binding,pressed=false,size=64}) {
  if(!Number.isInteger(size)||size<16||size>256)throw RangeError('Invalid glyph texture size.');
  const {label,svg}=controlGlyph({family,binding});
  const url=URL.createObjectURL(new Blob([svg],{type:'image/svg+xml'}));
  try {
    const image=new Image();
    image.src=url;
    await image.decode();
    const canvas=document.createElement('canvas');canvas.width=canvas.height=size;
    const context=canvas.getContext('2d',{willReadFrequently:true});
    if(!context)throw Error('Glyph canvas unavailable.');
    if(pressed) {
      context.translate(size*0.08,size*0.12);
      context.scale(0.84,0.84);
    }
    context.drawImage(image,0,0,size,size);
    return {label,width:size,height:size,rgba:context.getImageData(0,0,size,size).data};
  } finally {URL.revokeObjectURL(url);}
}

// GX RGBA8 uses 4x4 tiles: sixteen AR pairs followed by sixteen GB pairs.
export function gxRgba8({width,height,rgba}) {
  if(!Number.isInteger(width)||!Number.isInteger(height)||width<1||height<1||width>256||height>256||rgba.length!==width*height*4)throw RangeError('Invalid RGBA image.');
  const tilesX=Math.ceil(width/4),tilesY=Math.ceil(height/4);
  const output=new Uint8Array(tilesX*tilesY*64);
  for(let y=0;y<height;y++)for(let x=0;x<width;x++) {
    const source=(y*width+x)*4;
    const dest=(Math.floor(y/4)*tilesX+Math.floor(x/4))*64+((y%4)*4+x%4)*2;
    output[dest]=rgba[source+3];output[dest+1]=rgba[source];
    output[dest+32]=rgba[source+1];output[dest+33]=rgba[source+2];
  }
  return output;
}
