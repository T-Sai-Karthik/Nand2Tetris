import sys, os

label_counter = 0
call_counter = 0

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
    
def asm_label(name, func):
    return [f"({func}${name})"]
    
def asm_goto(name, func):
    return [f"@{func}${name}","0;JMP"]
    
def asm_if(name, func):
    return ["@SP","AM=M-1","D=M",f"@{func}${name}","D;JNE"]
    
def asm_function(name, nVars):
    out = [f"({name})"]
    for _ in range(int(nVars)):
        out = out + ["@0","D=A","@SP","A=M","M=D","@SP","M=M+1"]
    return out
    
def asm_call(name, nArgs, currentFunc):
    global call_counter
    ret=f"{name}$ret{call_counter}"; call_counter+=1
    return [
        f"@{ret}","D=A","@SP","A=M","M=D","@SP","M=M+1",
        "@LCL","D=M","@SP","A=M","M=D","@SP","M=M+1",
        "@ARG","D=M","@SP","A=M","M=D","@SP","M=M+1",
        "@THIS","D=M","@SP","A=M","M=D","@SP","M=M+1",
        "@THAT","D=M","@SP","A=M","M=D","@SP","M=M+1",
        "@SP","D=M","@5","D=D-A",f"@{nArgs}","D=D-A","@ARG","M=D",
        "@SP","D=M","@LCL","M=D",
        f"@{name}","0;JMP",
        f"({ret})"
    ]

def asm_return():
    return [
        "@LCL","D=M","@R13","M=D", 
        "@5","A=D-A","D=M","@R14","M=D",
        "@SP","AM=M-1","D=M","@ARG","A=M","M=D", 
        "@ARG","D=M+1","@SP","M=D", 
        "@R13","AM=M-1","D=M","@THAT","M=D",
        "@R13","AM=M-1","D=M","@THIS","M=D",
        "@R13","AM=M-1","D=M","@ARG","M=D",
        "@R13","AM=M-1","D=M","@LCL","M=D",
        "@R14","A=M","0;JMP"             
    ]

def translate(lines, file, currentFunc=""):
    out=[]
    for line in lines:
        if not line or line.startswith("//"): continue
        parts=line.split()
        cmd=parts[0]
        if cmd=="push":
            out+=asm_push(parts[1],parts[2],file)+["@SP","A=M","M=D","@SP","M=M+1"]
        elif cmd=="pop":
            out+=asm_pop(parts[1],parts[2],file)
        elif cmd in ("add","sub","neg","eq","gt","lt","and","or","not"):
            out+=asm_arith(cmd)
        elif cmd=="label": out+=asm_label(parts[1],currentFunc)
        elif cmd=="goto": out+=asm_goto(parts[1],currentFunc)
        elif cmd=="if-goto": out+=asm_if(parts[1],currentFunc)
        elif cmd=="function":
            currentFunc=parts[1]
            out+=asm_function(parts[1],parts[2])
        elif cmd=="call": out+=asm_call(parts[1],parts[2],currentFunc)
        elif cmd=="return": out+=asm_return()
    return out
    
def bootstrap():
    return ["@256","D=A","@SP","M=D"]+asm_call("Sys.init",0,"")

def translate_vm_folder(path):
    # Make sure path exists
    if not os.path.exists(path):
        print("Error: path does not exist:", path)
        return

    if os.path.isdir(path):
        # Name of output file = folder name
        folder_name = os.path.basename(os.path.normpath(path))
        outname = os.path.join(path, folder_name + ".asm")

        # Collect all .vm files in the folder
        vm_files = [os.path.join(path, f) for f in os.listdir(path) if f.endswith(".vm")]
        vm_files.sort()  # optional: ensure deterministic order

        all_lines = []
        for vmfile in vm_files:
            print("Translating:", vmfile)
            with open(vmfile, "r") as f:
                lines = [l.split("//")[0].strip() for l in f if l.strip() and not l.strip().startswith("//")]
                all_lines.extend(translate(lines, os.path.splitext(os.path.basename(vmfile))[0]))

        # Write output
        with open(outname, "w") as f:
            f.write("\n".join(all_lines))
        print("Generated:", outname)
    else:
        # Single file mode
        with open(path, "r") as f:
            lines = [l.split("//")[0].strip() for l in f if l.strip() and not l.strip().startswith("//")]
            outname = path.replace(".vm", ".asm")
            all_lines = translate(lines, os.path.splitext(os.path.basename(path))[0])
            with open(outname, "w") as f2:
                f2.write("\n".join(all_lines))
        print("Generated:", outname)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 VMTranslator.py <file.vm or folder>")
        sys.exit(1)

    translate_vm_folder(sys.argv[1])


