import sys, os

label_counter = 0

segments = {
    "local": "LCL", "argument": "ARG", "this": "THIS", "that": "THAT",
    "temp": "5", "pointer": "3"
}

def asm_push(seg, idx, file):
    if seg == "constant":
        return [f"@{idx}", "D=A"]
    elif seg in ("local","argument","this","that"):
        return [f"@{idx}", "D=A", f"@{segments[seg]}", "A=M+D", "D=M"]
    elif seg == "temp" or seg == "pointer":
        return [f"@{int(segments[seg])+int(idx)}", "D=M"]
    elif seg == "static":
        return [f"@{file}.{idx}", "D=M"]
    return []

def asm_pop(seg, idx, file):
    if seg in ("local","argument","this","that"):
        return [f"@{idx}","D=A",f"@{segments[seg]}","D=M+D","@R13","M=D",
                "@SP","AM=M-1","D=M","@R13","A=M","M=D"]
    elif seg == "temp" or seg == "pointer":
        return ["@SP","AM=M-1","D=M",f"@{int(segments[seg])+int(idx)}","M=D"]
    elif seg == "static":
        return ["@SP","AM=M-1","D=M",f"@{file}.{idx}","M=D"]
    return []

def asm_arith(cmd):
    global label_counter
    binary = {"add":"M=D+M","sub":"M=M-D","and":"M=D&M","or":"M=D|M"}
    unary  = {"neg":"M=-M","not":"M=!M"}
    comp   = {"eq":"JEQ","gt":"JGT","lt":"JLT"}
    if cmd in binary:
        return ["@SP","AM=M-1","D=M","A=A-1",binary[cmd]]
    if cmd in unary:
        return ["@SP","A=M-1",unary[cmd]]
    if cmd in comp:
        label,label_counter=label_counter,label_counter+1
        return ["@SP","AM=M-1","D=M","A=A-1","D=M-D","M=-1",
                f"@TRUE{label}","D;"+comp[cmd],
                "@SP","A=M-1","M=0",f"(TRUE{label})"]
    return []

def translate(lines, file):
    out=[]
    for line in lines:
        if not line or line.startswith("//"): continue
        parts=line.split()
        if parts[0]=="push":
            out+=asm_push(parts[1],parts[2],file)+["@SP","A=M","M=D","@SP","M=M+1"]
        elif parts[0]=="pop":
            out+=asm_pop(parts[1],parts[2],file)
        else:
            out+=asm_arith(parts[0])
    return out

if __name__=="__main__":
    vmfile=sys.argv[1]
    base=os.path.splitext(os.path.basename(vmfile))[0]
    lines=[l.split("//")[0].strip() for l in open(vmfile)]
    asm=translate(lines, base)
    with open(base+".asm","w") as f: f.write("\n".join(asm))

