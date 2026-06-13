#!/usr/bin/env python3
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
    elif seg in ("temp","pointer"):
        return [f"@{int(segments[seg])+int(idx)}", "D=M"]
    elif seg == "static":
        return [f"@{file}.{idx}", "D=M"]
    else:
        raise ValueError("Unknown push segment: " + seg)

def asm_pop(seg, idx, file):
    if seg in ("local","argument","this","that"):
        return [f"@{idx}","D=A",f"@{segments[seg]}","D=M+D","@R13","M=D",
                "@SP","AM=M-1","D=M","@R13","A=M","M=D"]
    elif seg in ("temp","pointer"):
        return ["@SP","AM=M-1","D=M",f"@{int(segments[seg])+int(idx)}","M=D"]
    elif seg == "static":
        return ["@SP","AM=M-1","D=M",f"@{file}.{idx}","M=D"]
    else:
        raise ValueError("Unknown pop segment: " + seg)

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
        global label_counter
        label = label_counter
        label_counter += 1
        return ["@SP","AM=M-1","D=M","A=A-1","D=M-D","M=-1",
                f"@TRUE{label}","D;"+comp[cmd],
                "@SP","A=M-1","M=0",f"(TRUE{label})"]
    raise ValueError("Unknown arithmetic command: " + cmd)

# label/goto/if now accept fileBase to scope labels to function OR file
def asm_label(name, func, fileBase):
    scope = func if func else fileBase
    return [f"({scope}${name})"]
    
def asm_goto(name, func, fileBase):
    scope = func if func else fileBase
    return [f"@{scope}${name}","0;JMP"]
    
def asm_if(name, func, fileBase):
    scope = func if func else fileBase
    return ["@SP","AM=M-1","D=M",f"@{scope}${name}","D;JNE"]

def asm_function(name, nVars):
    out = [f"({name})"]
    for _ in range(int(nVars)):
        out += ["@0","D=A","@SP","A=M","M=D","@SP","M=M+1"]
    return out

def asm_call(name, nArgs):
    global call_counter
    ret = f"{name}$ret.{call_counter}"
    call_counter += 1
    out = []
    # push return-address
    out += [f"@{ret}","D=A","@SP","A=M","M=D","@SP","M=M+1"]
    # push LCL, ARG, THIS, THAT
    for seg in ("LCL","ARG","THIS","THAT"):
        out += [f"@{seg}","D=M","@SP","A=M","M=D","@SP","M=M+1"]
    # ARG = SP - 5 - nArgs
    out += ["@SP","D=M","@5","D=D-A",f"@{nArgs}","D=D-A","@ARG","M=D"]
    # LCL = SP
    out += ["@SP","D=M","@LCL","M=D"]
    # goto f
    out += [f"@{name}","0;JMP"]
    # return label
    out += [f"({ret})"]
    return out

def asm_return():
    # Implements FRAME=LCL; RET = *(FRAME-5); *ARG = pop(); SP = ARG+1; restore THAT,THIS,ARG,LCL; goto RET
    return [
        "@LCL","D=M","@R13","M=D",    # R13 = FRAME
        "@5","A=D-A","D=M","@R14","M=D",  # R14 = RET = *(FRAME-5)
        "@SP","AM=M-1","D=M","@ARG","A=M","M=D", # *ARG = pop()
        "@ARG","D=M+1","@SP","M=D",   # SP = ARG + 1
        "@R13","AM=M-1","D=M","@THAT","M=D", # THAT = *(FRAME-1)
        "@R13","AM=M-1","D=M","@THIS","M=D", # THIS = *(FRAME-2)
        "@R13","AM=M-1","D=M","@ARG","M=D",  # ARG = *(FRAME-3)
        "@R13","AM=M-1","D=M","@LCL","M=D",  # LCL = *(FRAME-4)
        "@R14","A=M","0;JMP"
    ]

def translate(lines, fileBase, currentFunc):
    """
    Translate list of VM lines (cleaned) from one file.
    Returns tuple: (asm_lines, updated_currentFunc)
    """
    out = []
    for line in lines:
        if not line or line.startswith("//"):
            continue
        parts = line.split()
        cmd = parts[0]
        if cmd == "push":
            out += asm_push(parts[1], parts[2], fileBase) + ["@SP","A=M","M=D","@SP","M=M+1"]
        elif cmd == "pop":
            out += asm_pop(parts[1], parts[2], fileBase)
        elif cmd in ("add","sub","neg","eq","gt","lt","and","or","not"):
            out += asm_arith(cmd)
        elif cmd == "label":
            out += asm_label(parts[1], currentFunc, fileBase)
        elif cmd == "goto":
            out += asm_goto(parts[1], currentFunc, fileBase)
        elif cmd == "if-goto":
            out += asm_if(parts[1], currentFunc, fileBase)
        elif cmd == "function":
            # set current function scope to this function name
            currentFunc = parts[1]
            out += asm_function(parts[1], parts[2])
        elif cmd == "call":
            out += asm_call(parts[1], parts[2])
        elif cmd == "return":
            out += asm_return()
        else:
            raise ValueError("Unknown VM command: " + cmd)
    return out, currentFunc

def bootstrap():
    # Set SP = 256 and call Sys.init
    return ["@256","D=A","@SP","M=D"] + asm_call("Sys.init", 0)

def translate_vm_folder(path):
    if not os.path.exists(path):
        print("Error: path does not exist:", path)
        return

    if os.path.isdir(path):
        folder_name = os.path.basename(os.path.normpath(path))
        outname = os.path.join(path, folder_name + ".asm")

        # Collect all .vm files in the folder and sort deterministically.
        all_files = sorted([f for f in os.listdir(path) if f.endswith(".vm")])
        # Place Sys.vm first if present
        vm_files = []
        if "Sys.vm" in all_files:
            vm_files.append("Sys.vm")
        vm_files += [f for f in all_files if f != "Sys.vm"]

        # Start with bootstrap
        all_lines = bootstrap()

        currentFunc = ""  # keeps function scope between files
        for f in vm_files:
            vmfile = os.path.join(path, f)
            print("Translating:", vmfile)
            with open(vmfile, "r") as fh:
                lines = [l.split("//")[0].strip() for l in fh]
                cleaned = [ln for ln in lines if ln != ""]
                filebase = os.path.splitext(os.path.basename(f))[0]
                asm_lines, currentFunc = translate(cleaned, filebase, currentFunc)
                all_lines.extend(asm_lines)

        with open(outname, "w") as fout:
            fout.write("\n".join(all_lines))
        print("Generated:", outname)
    else:
        # Single file mode: translate single .vm file (no bootstrap)
        with open(path, "r") as fh:
            lines = [l.split("//")[0].strip() for l in fh]
            cleaned = [ln for ln in lines if ln != ""]
            filebase = os.path.splitext(os.path.basename(path))[0]
            asm_lines, _ = translate(cleaned, filebase, "")
            outname = path.replace(".vm", ".asm")
            with open(outname, "w") as fout:
                fout.write("\n".join(asm_lines))
        print("Generated:", outname)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 VMTranslator.py <file.vm or folder>")
        sys.exit(1)
    translate_vm_folder(sys.argv[1])

