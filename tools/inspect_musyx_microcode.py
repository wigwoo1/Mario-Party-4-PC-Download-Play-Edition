"""Inventory opcode boundaries using a caller-supplied Dolphin DSPTables reference.
This is linear decoding, not reachability or coefficient-bank verification.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',required=True)
    parser.add_argument('--source',required=True)
    parser.add_argument('--output',required=True)
    parser.add_argument('--arithmetic-reference')
    args=parser.parse_args()
    reference=Path(args.reference).read_text()
    table=reference.split('s_opcodes =',1)[1].split('const DSPOPCTemplate cw',1)[0]
    opcodes=[(name,int(op,16),int(mask,16),int(size)) for name,op,mask,size in
        re.findall(r'\{"([A-Z0-9]+)",\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),\s*([12]),',table)]
    if len(opcodes)!=230: raise ValueError('Unexpected primary opcode table')
    extendable={}
    for line in table.splitlines():
        name=re.search(r'\{"([A-Z0-9]+)"',line)
        flags=re.search(r'\},\s*(true|false),\s*(true|false)',line)
        if name and flags: extendable[name[1]]=flags[1]=='true'
    extension_table=reference.split('s_opcodes_ext =',1)[1].split('const std::array<pdlabel_t',1)[0]
    extensions=[(name,int(op,16),int(mask,16)) for name,op,mask in
        re.findall(r'\{"([A-Z0-9]+)",\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),',extension_table)]
    if len(extensions)!=25: raise ValueError('Unexpected extension opcode table')
    source=Path(args.source).read_text()
    arrays=re.findall(r'char dspSlave\[\].*?=\s*\{(.*?)\};',source,re.S)
    results=[]
    for index,body in enumerate(arrays):
        raw=bytes(int(value,16) for value in re.findall(r'0x([0-9A-Fa-f]{2})\b',body))
        words=struct.unpack('>'+str(len(raw)//2)+'H',raw)
        instructions=[]
        position=0
        while position<len(words):
            match=next((op for op in opcodes if words[position]&op[2]==op[1]),None)
            name,_,_,size=match or ('UNKNOWN',0,0,1)
            if position+size>len(words): raise ValueError('Truncated instruction')
            instruction={'offset':position,'opcode':name,'words':list(words[position:position+size])}
            if extendable.get(name):
                extension=words[position]&(0x7f if words[position]>>12==3 else 0xff)
                extension_match=next((e for e in extensions if extension&e[2]==e[1]),None)
                instruction['extension']=extension_match[0] if extension_match else 'UNKNOWN'
                instruction['extension_bits']=extension
            instructions.append(instruction)
            position+=size
        candidates=[]
        for n,instruction in enumerate(instructions):
            if instruction['opcode'] in ('LRI','ORI','ADDI','ANDI','LR','SR') and 0x1000<=instruction['words'][1]<0x2000:
                candidates.append({'register':instruction['words'][0]&31,
                    'immediate':instruction['words'][1],
                    'window':instructions[max(0,n-4):n+15]})
        results.append({'variant':index,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),
            'unknown_instructions':sum(i['opcode']=='UNKNOWN' for i in instructions),
            'linear_boundary_candidates':candidates,'primary_instructions':instructions})
        if index==0:
            # Trace the straight-line PB reads into the four history writes.
            if words[0x5a8:0x5aa]!=(0x0082,0x0bb8):
                raise ValueError('Source-state pointer changed')
            cursor=0xbb8
            for address in range(0x5aa,0x5e0,2):
                if words[address]!=0x195e:
                    raise ValueError('PB prefix read changed')
                cursor+=1
            if cursor!=0xb80+0xa6//2:
                raise ValueError('PB SRC layout mismatch')
            expected=(0x195b,0x1959,0x8100,0x195c,0x0080,0x0e44,
                0x195f,0x1b1f,0x195f,0x1b1f,0x195f,0x1b1f,0x185f,0x1b1f)
            if tuple(words[0x5e5:0x5f3])!=expected:
                raise ValueError('Original history initialization changed')
            results[-1]['history_initialization']={
                'pb_src_word_address':cursor,'history_word_addresses':list(range(cursor+3,cursor+7)),
                'scratch_word_addresses':list(range(0xe44,0xe48)),
                'scope':'Original PB history copied in array order; later source advancement and tap ordering unverified'}
            restore=(0x0004,0x189f,0x1adf,0x189f,0x1adf,
                0x189f,0x1adf,0x189f,0x1adf,0x1adc)
            if tuple(words[0x62b:0x635])!=restore:
                raise ValueError('Original history restoration changed')
            # AR2 remains on the fourth history word after its nonincrementing
            # initialization read. Four decrementing writes restore in reverse
            # address order, then currentAddressFrac is stored from register28.
            results[-1]['history_restoration']={
                'pb_history_write_order':list(range(cursor+6,cursor+2,-1)),
                'fraction_word_address':cursor+2,
                'scope':'Four trailing scratch samples restored to PB history without reversal; source consumption/tap ordering unverified'}
            # Both voice-processing paths read PB word5 at DMEM0xB85,
            # index the IMEM0xB34 table, and store its value at DMEM0xE16.
            dispatch=(0x00de,0x0b85,0x009f,0x0b34,0x4c00,0x1c7e,0x0213,0x00fe,0x0e16)
            for address in (0x1fd,0x3c4):
                if tuple(words[address:address+len(dispatch)])!=dispatch:
                    raise ValueError('MusyX2.0.0 coefficient dispatch changed')
            bases=list(words[0xb34:0xb37])
            if bases!=[0x1000,0x1200,0x1400]:
                raise ValueError('Unexpected original coefficient bank table')
            results[-1]['coefficient_selection']={'pb_word_address':0xb85,
                'dispatch_offsets':[0x1fd,0x3c4],'imem_table':0xb34,
                'bank_word_bases':bases,'destination_word_address':0xe16,
                'scope':'Two original dispatch sequences and serialized table verified; coefficient values and SRC arithmetic still unverified'}
            if args.arithmetic_reference:
                arithmetic=Path(args.arithmetic_reference).read_text()
                # MOVAX/LSL5/ADD forms fraction+32*ratio. LSR2 splits
                # whole source consumption into four-sample groups and a
                # remainder: IX0=middle, IX1=low; reloading low and LSL2
                # puts the remaining 0..3 whole samples into IX1.
                block=(0x6b00,0x1505,0x4d00,0x157e,0x1c9f,0x1cbd,
                    0x05e0,0x9900,0x7d00,0x1cdd,0x8900,0x1fa5,0x1502,0x1cbf)
                if tuple(words[0x5f3:0x601])!=block:
                    raise ValueError('Original block source-count sequence changed')
                checked=0
                for pitch in range(0x4000):
                    ratio=pitch*16
                    for fraction in (0,1,511,512,32767,65534,65535):
                        shifted=(fraction+32*ratio)>>2
                        groups=shifted>>16
                        remainder=((shifted&65535)<<2)>>16
                        consumed=0
                        phase=fraction
                        for _ in range(32):
                            phase+=ratio
                            consumed+=phase>>16
                            phase&=65535
                        if groups*4+remainder!=consumed:
                            raise ValueError('Portable block source count mismatch')
                        checked+=1
                results[-1]['block_source_count_validation']={
                    'vectors_checked':checked,'pitch_values_checked':0x4000,
                    'formula':'floor((fraction + 32 * pitch * 16) / 65536)',
                    'scope':'Block consumption count only; per-output source pointers/fraction writeback unverified'}
                lsr=arithmetic.split('void Interpreter::lsr(',1)[1].split('// ASL',1)[0]
                if 'shift = 0x40 - (opc & 0x3f)' not in lsr or 'acc >>= shift' not in lsr:
                    raise ValueError('DSP shift semantics changed')
                if words[0x601:0x603]!=(0x009a,0x01fc):
                    raise ValueError('Phase mask changed')
                for offset in (0x611,0x627):
                    if words[offset]!=0x1579: raise ValueError('Phase shift changed')
                shift=0x40-(0x1579&0x3f)
                for fraction in range(65536):
                    # Fraction copied into accumulator middle word. Reading
                    # middle after LSR7 and AND0x1FC gives the tap address.
                    address=(((fraction<<16)>>shift)>>16)&0x1fc
                    if address!=(fraction//512)*4:
                        raise ValueError('Portable phase index mismatch')
                results[-1]['phase_index_validation']={'fractions_checked':65536,
                    'right_shift':shift,'mask':0x1fc,'formula':'(fraction >> 9) * 4',
                    'arithmetic_reference_sha256':hashlib.sha256(arithmetic.encode()).hexdigest(),
                    'scope':'Phase indexing only; source progression and multiply/rounding unverified'}
    Path(args.output).write_text(json.dumps({'scope':'Primary/extension opcode names and linear boundaries; operand effects, reachability and coefficient values unverified',
        'reference_sha256':hashlib.sha256(reference.encode()).hexdigest(),'variants':results},indent=2))
    print([(r['variant'],r['unknown_instructions'],len(r['linear_boundary_candidates'])) for r in results])

if __name__=='__main__': main()
