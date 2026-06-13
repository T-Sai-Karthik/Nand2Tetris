#include <bits/stdc++.h>
#include <filesystem>
using namespace std;
namespace fs = std::filesystem;

static inline bool is_symchar(char c) {
    static const string syms = "{}()[].,;+-*/&|<>=~";
    return syms.find(c) != string::npos;
}

static inline bool is_kw(const string &s) {
    static const unordered_set<string> keywords = {
        "class","constructor","function","method","field","static","var",
        "int","char","boolean","void","true","false","null","this",
        "let","do","if","else","while","return"
    };
    return keywords.count(s);
}


struct JToken {
    string kind;
    string text;
};


class JackLexer {
    string source;
    size_t pos = 0;

public:
    JackLexer(const string &src) : source(src), pos(0) {}

    static string stripCommentsKeepStrings(const string &s) {
        string out;
        out.reserve(s.size());
        size_t i = 0, n = s.size();
        while (i < n) {
            if (s[i] == '"') {
                out.push_back(s[i++]);
                while (i < n && s[i] != '"') out.push_back(s[i++]);
                if (i < n) out.push_back(s[i++]);
            } else if (i + 1 < n && s[i] == '/' && s[i+1] == '/') {
                i += 2;
                while (i < n && s[i] != '\n') ++i;
                if (i < n && s[i] == '\n') { out.push_back('\n'); ++i; }
            } else if (i + 1 < n && s[i] == '/' && s[i+1] == '*') {
                i += 2;
                while (i + 1 < n && !(s[i] == '*' && s[i+1] == '/')) ++i;
                if (i + 1 < n) i += 2;
            } else {
                out.push_back(s[i++]);
            }
        }
        return out;
    }

    vector<JToken> tokenizeAll() {
        vector<JToken> tokens;
        size_t n = source.size();
        while (pos < n) {
            char c = source[pos];

            if (isspace(static_cast<unsigned char>(c))) { ++pos; continue; }


            if (c == '"') {
                ++pos;
                string val;
                while (pos < n && source[pos] != '"') val.push_back(source[pos++]);
                if (pos < n && source[pos] == '"') ++pos;
                tokens.push_back({"stringConstant", val});
                continue;
            }


            if (is_symchar(c)) {
                tokens.push_back({"symbol", string(1, c)});
                ++pos;
                continue;
            }


            if (isdigit(static_cast<unsigned char>(c))) {
                string num;
                while (pos < n && isdigit(static_cast<unsigned char>(source[pos]))) num.push_back(source[pos++]);
                tokens.push_back({"integerConstant", num});
                continue;
            }

            if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
                string id;
                while (pos < n && (isalnum(static_cast<unsigned char>(source[pos])) || source[pos] == '_'))
                    id.push_back(source[pos++]);
                if (is_kw(id)) tokens.push_back({"keyword", id});
                else tokens.push_back({"identifier", id});
                continue;
            }

            tokens.push_back({"symbol", string(1, c)});
            ++pos;
        }
        return tokens;
    }

    static vector<JToken> fromFile(const fs::path &p) {
        ifstream ifs(p);
        string text((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
        string cleaned = stripCommentsKeepStrings(text);
        JackLexer L(cleaned);
        return L.tokenizeAll();
    }
};

struct VarInfo { string type; string kind; int index; };

class VarTable {
    unordered_map<string, VarInfo> classScope;
    unordered_map<string, VarInfo> subScope;
    int countStatic = 0, countField = 0, countArg = 0, countVar = 0;

public:
    void startSubroutine() {
        subScope.clear();
        countArg = 0;
        countVar = 0;
    }

    void define(const string &name, const string &type, const string &kind) {
        if (kind == "static") classScope[name] = {type, kind, countStatic++};
        else if (kind == "field") classScope[name] = {type, kind, countField++};
        else if (kind == "arg") subScope[name] = {type, kind, countArg++};
        else if (kind == "var") subScope[name] = {type, kind, countVar++};
    }

    bool exists(const string &name) const {
        return subScope.count(name) || classScope.count(name);
    }

    VarInfo get(const string &name) const {
        if (subScope.count(name)) return subScope.at(name);
        return classScope.at(name);
    }

    int varCount(const string &kind) const {
        if (kind == "field") return countField;
        if (kind == "var") return countVar;
        return 0;
    }
};


static inline string kindToSeg(const string &k) {
    if (k == "static") return "static";
    if (k == "field") return "this";
    if (k == "arg") return "argument";
    if (k == "var") return "local";
    return "";
}


class VMOut {
    ofstream ofs;
public:
    VMOut(const fs::path &p) { ofs.open(p); }
    void writeFunction(const string &name, int nlocals) { ofs << "function " << name << " " << nlocals << "\n"; }
    void writePush(const string &seg, int idx) { ofs << "push " << seg << " " << idx << "\n"; }
    void writePop(const string &seg, int idx) { ofs << "pop " << seg << " " << idx << "\n"; }
    void writeOp(const string &op) { ofs << op << "\n"; }
    void writeCall(const string &name, int n) { ofs << "call " << name << " " << n << "\n"; }
    void writeReturn() { ofs << "return\n"; }
    void writeLabel(const string &L) { ofs << "label " << L << "\n"; }
    void writeGoto(const string &L) { ofs << "goto " << L << "\n"; }
    void writeIf(const string &L) { ofs << "if-goto " << L << "\n"; }
};


class Compiler {
    vector<JToken> toks;
    size_t ptr = 0;
    VarTable vt;
    VMOut *vm;
    string className;
    int labelGen = 0;

    JToken peek() const { return ptr < toks.size() ? toks[ptr] : JToken(); }
    JToken nextTok() { return ptr < toks.size() ? toks[ptr++] : JToken(); }
    bool isVal(const string &s) const { return ptr < toks.size() && toks[ptr].text == s; }
    void expect(const string &s) {
        if (!isVal(s)) {
            cerr << "Parse error: expected '" << s << "' got '" << (ptr < toks.size() ? toks[ptr].text : "<eof>") << "'\n";
        } else ++ptr;
    }
    bool isType() const {
        if (ptr >= toks.size()) return false;
        return toks[ptr].kind == "identifier"
            || (toks[ptr].kind == "keyword" && (toks[ptr].text == "int" || toks[ptr].text == "char" || toks[ptr].text == "boolean"));
    }


    void writeOperator(const string &op) {
        if (op == "+") vm->writeOp("add");
        else if (op == "-") vm->writeOp("sub");
        else if (op == "&") vm->writeOp("and");
        else if (op == "|") vm->writeOp("or");
        else if (op == "<") vm->writeOp("lt");
        else if (op == ">") vm->writeOp("gt");
        else if (op == "=") vm->writeOp("eq");
        else if (op == "*") vm->writeCall("Math.multiply", 2);
        else if (op == "/") vm->writeCall("Math.divide", 2);
    }

    string freshLabel(const string &base) { return base + to_string(labelGen++); }


    void compileClass() {
        expect("class");
        className = nextTok().text; 
        expect("{");
        while (isVal("static") || isVal("field")) compileClassVar();
        while (isVal("constructor") || isVal("function") || isVal("method")) compileSubroutine();
        expect("}");
    }

    void compileClassVar() {
        string kind = nextTok().text; 
        string type = nextTok().text;
        string name = nextTok().text; 
        vt.define(name, type, kind);
        while (isVal(",")) {
            ++ptr;
            string n2 = nextTok().text;
            vt.define(n2, type, kind);
        }
        expect(";");
    }

    void compileSubroutine() {
        string subKind = nextTok().text;
        if (isVal("void") || isType()) ++ptr;
        string subName = nextTok().text;
        vt.startSubroutine();
        if (subKind == "method") vt.define("this", className, "arg");
        expect("(");
        compileParameterList();
        expect(")");
        expect("{");
        while (isVal("var")) compileVarDec();
        int locals = vt.varCount("var");
        vm->writeFunction(className + "." + subName, locals);
        if (subKind == "constructor") {
            vm->writePush("constant", vt.varCount("field"));
            vm->writeCall("Memory.alloc", 1);
            vm->writePop("pointer", 0);
        } else if (subKind == "method") {
            vm->writePush("argument", 0);
            vm->writePop("pointer", 0);
        }
        compileStatements();
        expect("}");
    }

    void compileParameterList() {
        if (isType()) {
            string t = nextTok().text;
            string name = nextTok().text;
            vt.define(name, t, "arg");
            while (isVal(",")) {
                ++ptr;
                string tt = nextTok().text;
                string nn = nextTok().text;
                vt.define(nn, tt, "arg");
            }
        }
    }

    void compileVarDec() {
        expect("var");
        string t = nextTok().text;
        string name = nextTok().text;
        vt.define(name, t, "var");
        while (isVal(",")) {
            ++ptr;
            string nn = nextTok().text;
            vt.define(nn, t, "var");
        }
        expect(";");
    }

    void compileStatements() {
        while (true) {
            if (isVal("let")) compileLet();
            else if (isVal("if")) compileIf();
            else if (isVal("while")) compileWhile();
            else if (isVal("do")) compileDo();
            else if (isVal("return")) compileReturn();
            else break;
        }
    }

    void compileDo() {
        expect("do");
        compileSubroutineCall();
        expect(";");
        vm->writePop("temp", 0);
    }

    void compileLet() {
        expect("let");
        string name = nextTok().text;
        bool isArray = false;
        if (isVal("[")) {
            ++ptr;
            compileExpression();
            expect("]");
            VarInfo vi = vt.get(name);
            vm->writePush(kindToSeg(vi.kind), vi.index);
            vm->writeOp("add");
            isArray = true;
        }
        expect("=");
        compileExpression();
        expect(";");
        if (isArray) {
            vm->writePop("temp", 0);
            vm->writePop("pointer", 1);
            vm->writePush("temp", 0);
            vm->writePop("that", 0);
        } else {
            VarInfo vi = vt.get(name);
            vm->writePop(kindToSeg(vi.kind), vi.index);
        }
    }

    void compileWhile() {
        expect("while");
        string L1 = freshLabel("WHILE_EXP");
        string L2 = freshLabel("WHILE_END");
        vm->writeLabel(L1);
        expect("(");
        compileExpression();
        expect(")");
        vm->writeOp("not");
        vm->writeIf(L2);
        expect("{");
        compileStatements();
        expect("}");
        vm->writeGoto(L1);
        vm->writeLabel(L2);
    }

    void compileIf() {
        expect("if");
        expect("(");
        compileExpression();
        expect(")");
        string Lfalse = freshLabel("IF_FALSE");
        string Lend = freshLabel("IF_END");
        vm->writeOp("not");
        vm->writeIf(Lfalse);
        expect("{");
        compileStatements();
        expect("}");
        if (isVal("else")) {
            vm->writeGoto(Lend);
            vm->writeLabel(Lfalse);
            ++ptr;
            expect("{");
            compileStatements();
            expect("}");
            vm->writeLabel(Lend);
        } else {
            vm->writeLabel(Lfalse);
        }
    }

    void compileReturn() {
        expect("return");
        if (!isVal(";")) {
            compileExpression();
        } else {
            vm->writePush("constant", 0);
        }
        expect(";");
        vm->writeReturn();
    }

    int compileExpressionList() {
        int cnt = 0;
        if (!isVal(")")) {
            compileExpression();
            ++cnt;
            while (isVal(",")) {
                ++ptr;
                compileExpression();
                ++cnt;
            }
        }
        return cnt;
    }

    void compileSubroutineCall() {
        string id = nextTok().text;
        int nargs = 0;
        if (isVal(".")) {
            ++ptr;
            string subname = nextTok().text;
            expect("(");
            if (vt.exists(id)) {
                VarInfo vi = vt.get(id);
                vm->writePush(kindToSeg(vi.kind), vi.index);
                nargs = 1 + compileExpressionList();
                expect(")");
                vm->writeCall(vi.type + "." + subname, nargs);
            } else {
                nargs = compileExpressionList();
                expect(")");
                vm->writeCall(id + "." + subname, nargs);
            }
        } else {
            expect("(");
            vm->writePush("pointer", 0);
            nargs = 1 + compileExpressionList();
            expect(")");
            vm->writeCall(className + "." + id, nargs);
        }
    }

    void compileExpression() {
        compileTerm();
        while (isVal("+") || isVal("-") || isVal("*") || isVal("/") || isVal("&")
               || isVal("|") || isVal("<") || isVal(">") || isVal("=")) {
            string op = nextTok().text;
            compileTerm();
            writeOperator(op);
        }
    }

    void compileTerm() {
        JToken tk = nextTok();
        if (tk.kind == "integerConstant") {
            vm->writePush("constant", stoi(tk.text));
            return;
        }
        if (tk.kind == "stringConstant") {
            vm->writePush("constant", static_cast<int>(tk.text.size()));
            vm->writeCall("String.new", 1);
            for (char ch : tk.text) {
                vm->writePush("constant", static_cast<int>(ch));
                vm->writeCall("String.appendChar", 2);
            }
            return;
        }
        if (tk.kind == "keyword") {
            if (tk.text == "true") { vm->writePush("constant", 1); vm->writeOp("neg"); }
            else if (tk.text == "false" || tk.text == "null") vm->writePush("constant", 0);
            else if (tk.text == "this") vm->writePush("pointer", 0);
            return;
        }
        if (tk.text == "(") {
            compileExpression();
            expect(")");
            return;
        }
        if (tk.text == "-" || tk.text == "~") {
            compileTerm();
            if (tk.text == "-") vm->writeOp("neg");
            else vm->writeOp("not");
            return;
        }

        if (tk.kind == "identifier") {
            string name = tk.text;
            if (isVal("[")) {
                ++ptr;
                compileExpression();
                expect("]");
                VarInfo vi = vt.get(name);
                vm->writePush(kindToSeg(vi.kind), vi.index);
                vm->writeOp("add");
                vm->writePop("pointer", 1);
                vm->writePush("that", 0);
                return;
            }
            if (isVal("(") || isVal(".")) {

                --ptr;
                compileSubroutineCall();
                return;
            }
            if (vt.exists(name)) {
                VarInfo vi = vt.get(name);
                vm->writePush(kindToSeg(vi.kind), vi.index);
            }
            return;
        }
    }

public:
    Compiler(const vector<JToken> &tokens, VMOut *writer) : toks(tokens), ptr(0), vt(), vm(writer), className(), labelGen(0) {}

    void run() {
        compileClass();
    }
};

int main(int argc, char **argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    if (argc < 2) {
        cerr << "Usage: JackCompiler <file-or-directory>\n";
        return 1;
    }

    fs::path input(argv[1]);
    vector<fs::path> targets;
    if (fs::is_directory(input)) {
        for (auto &entry : fs::directory_iterator(input)) {
            if (entry.path().extension() == ".jack") targets.push_back(entry.path());
        }
        sort(targets.begin(), targets.end());
    } else if (input.extension() == ".jack") {
        targets.push_back(input);
    } else {
        cerr << "Input must be a .jack file or a directory containing .jack files\n";
        return 1;
    }

    for (auto &jackpath : targets) {
        vector<JToken> tokens = JackLexer::fromFile(jackpath);
        fs::path outvm = jackpath.parent_path() / (jackpath.stem().string() + ".vm");
        VMOut writer(outvm);
        Compiler comp(tokens, &writer);
        comp.run();
        cout << "Compiled " << jackpath.filename().string() << " -> " << outvm.filename().string() << "\n";
    }

    return 0;
}

