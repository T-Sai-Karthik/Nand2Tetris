import sys, re

comp = {"0":"0101010","1":"0111111","-1":"0111010","D":"0001100","A":"0110000","M":"1110000",
        "!D":"0001101","!A":"0110001","!M":"1110001","-D":"0001111","-A":"0110011","-M":"1110011",
        "D+1":"0011111","A+1":"0110111","M+1":"1110111","D-1":"0001110","A-1":"0110010","M-1":"1110010",
        "D+A":"0000010","D+M":"1000010","D-A":"0010011","D-M":"1010011","A-D":"0000111","M-D":"1000111",
        "D&A":"0000000","D&M":"1000000","D|A":"0010101","D|M":"1010101"}
dest = {"":"000","M":"001","D":"010","MD":"011","A":"100","AM":"101","AD":"110","AMD":"111"}
jump = {"":"000","JGT":"001","JEQ":"010","JGE":"011","JLT":"100","JNE":"101","JLE":"110","JMP":"111"}
sym = {"SP":0,"LCL":1,"ARG":2,"THIS":3,"THAT":4,"SCREEN":16384,"KBD":24576,**{f"R{i}":i for i in range(16)}}

def bin16(n): return format(n,'016b')

lines=[re.sub(r'//.*','',l).strip() for l in open(sys.argv[1])]
lines=[l for l in lines if l]


addr=0
for l in lines:
    if l.startswith("("): sym[l[1:-1]]=addr
    else: addr+=1


ram=16
out=[]
for l in lines:
    if l.startswith("("): continue
    if l.startswith("@"):
        s=l[1:]
        if s.isdigit(): out.append(bin16(int(s)))
        else:
            if s not in sym: sym[s]=ram; ram+=1
            out.append(bin16(sym[s]))
    else:
        d,c,j="","",""
        if "=" in l: d,l=l.split("=")
        if ";" in l: l,j=l.split(";")
        c=l
        out.append("111"+comp[c]+dest[d]+jump[j])

open(sys.argv[1].replace(".asm",".hack"),"w").write("\n".join(out))

