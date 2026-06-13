#include <bits/stdc++.h>
using namespace std;

const unordered_set<string> KEYWORDS = {
    "class","constructor","function","method","field","static","var","int","char","boolean","void","true","false","null","this","let","do","if","else","while","return"
};

const unordered_set<char> SYMBOLS = {
    '{','}','(',')','[',']','.',';',',','+','-','*','/','&','|','<','>','=','~'
};

string escapeXml(const string &s) {
    string out;
    out.reserve(s.size()*2);
    for(char c : s) {
        if(c == '<') out += "&lt;";
        else if(c == '>') out += "&gt;";
        else if(c == '&') out += "&amp;";
        else out.push_back(c);
    }
    return out;
}

string OnlyStrings(const string &src) {
    string out;
    out.reserve(src.size());
    size_t n = src.size();
    size_t i = 0;
    while(i < n) {
        if (src[i] == '"') {
            out.push_back(src[i++]);
            while (i < n && src[i] != '"') {
                out.push_back(src[i++]);
            }
            if (i < n) { out.push_back(src[i++]); }
        }
        else if (i + 1 < n && src[i] == '/' && src[i+1] == '/') {
            i += 2;
            while (i < n && src[i] != '\n') i++;
            if (i < n && src[i] == '\n') {
                out.push_back('\n');
                i++;
            }
        }
        else if (i + 1 < n && src[i] == '/' && src[i+1] == '*') {
            i += 2;
            while (i + 1 < n && !(src[i] == '*' && src[i+1] == '/')) i++;
            if (i + 1 < n) i += 2;
        }
        else {
            out.push_back(src[i++]);
        }
    }
    return out;
}

struct Token {
    string type; 
    string value;
};

bool isIdentifierStart(char c) {
    return isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool isIdentifierPart(char c) {
    return isalnum(static_cast<unsigned char>(c)) || c == '_';
}

vector<Token> tokenizeJack(const string &src) {
    vector<Token> tokens;
    size_t n = src.size();
    size_t i = 0;

    while(i < n) {
        if (isspace(static_cast<unsigned char>(src[i]))) {
            i++; continue;
        }

        char c = src[i];

        if(c == '"') {
            i++; 
            string val;
            while (i < n && src[i] != '"') {
                val.push_back(src[i++]);
            }
            if (i < n && src[i] == '"') i++; 
            tokens.push_back({"stringConstant", val});
            continue;
        }

        if(SYMBOLS.count(c)) {
            string s(1, c);
            tokens.push_back({"symbol", s});
            i++;
            continue;
        }

        if(isdigit(static_cast<unsigned char>(c))) {
            long long val = 0;
            string num;
            while (i < n && isdigit(static_cast<unsigned char>(src[i]))) {
                num.push_back(src[i]);
                i++;
            }
            tokens.push_back({"integerConstant", num});
            continue;
        }

        if(isIdentifierStart(c)) {
            string id;
            while (i < n && isIdentifierPart(src[i])) {
                id.push_back(src[i++]);
            }
            if (KEYWORDS.count(id)) {
                tokens.push_back({"keyword", id});
            } else {
                tokens.push_back({"identifier", id});
            }
            continue;
        }

        string s(1, c);
        tokens.push_back({"symbol", s});
        i++;
    }

    return tokens;
}

int main(int argc, char** argv) {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    if(argc < 2) {
        cerr << "Usage: " << argv[0] << " Input.jack\n";
        return 1;
    }

    string filename = argv[1];
    ifstream ifs(filename);
    if(!ifs) {
        cerr << "Cannot open input file: " << filename << "\n";
        return 1;
    }

    string src((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());

    string cleaned = OnlyStrings(src);

    vector<Token> tokens = tokenizeJack(cleaned);

    cout << "<tokens>\n";
    for(const auto &tk : tokens) {
        if(tk.type == "keyword") {
            cout << "<keyword> " << tk.value << " </keyword>\n";
        } else if(tk.type == "symbol") {
            string escaped = escapeXml(tk.value);
            cout << "<symbol> " << escaped << " </symbol>\n";
        } else if(tk.type == "integerConstant") {
            cout << "<integerConstant> " << tk.value << " </integerConstant>\n";
        } else if(tk.type == "stringConstant") {
            cout << "<stringConstant> " << tk.value << " </stringConstant>\n";
        } else if(tk.type == "identifier") {
            cout << "<identifier> " << tk.value << " </identifier>\n";
        }
    }
    cout << "</tokens>\n";

    return 0;
}

