
#include <iostream>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

//  def = variable defined in this statement [AND] use = variables used on RHS (ignreo constants)
struct Instruction {
    string def;
    vector<string> use;
};

// IR forCFG
enum class OpKind { LABEL, ASSIGN, BINOP, GOTO, IFGOTO, RET, NOP };
struct IRInstr {
    OpKind op = OpKind::NOP;

    string label;

    string dst;
    string a;
    string b;
    char bop = 0;

    string target;

    string cond;
    string tlabel;
    string flabel;

    bool aImm = false, bImm = false;
    long long aVal = 0, bVal = 0;

    string raw;
};

// IR vlock forcfg
struct Block {
    int id = -1;
    int start = -1;
    int end = -1;
    vector<int> succ;
};

// Basic string helpers
static inline string trim(const string &s) {
    size_t i = 0, j = s.size();

    while(i < j && isspace((unsigned char)s[i])) {
        i++;
    }

    while(j > i && isspace((unsigned char)s[j - 1])) {
        j--;
    }
    return s.substr(i, j - i);
}

static inline bool isNumberToken(const string &t) {
    if(t.empty())
        return false;

    size_t i = 0;
    if(t[0] == '-' || t[0] == '+')
        i = 1;

    if(i >= t.size())
        return false;
    
    for(; i < t.size(); i++)
        if(!isdigit((unsigned char)t[i]))
            return false;
    return true;
}

static inline bool isIdentStart(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static inline bool isIdentChar(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static vector<string> tokenizeRHS(const string &rhs) {
    vector<string> tok;
    for(size_t i = 0; i < rhs.size();) {
        char c = rhs[i];
        if(isspace((unsigned char)c)) {
            i++;
            continue;
        }

        if(c == '+' || c == '-' || c == '*' || c == '/') {
            tok.push_back(string(1, c));
            i++;
            continue;
        }

        if(isIdentChar(c) || isdigit((unsigned char)c)) {
            size_t j = i;
            while((j < rhs.size() && (isIdentChar(rhs[j])) || isdigit((unsigned char)rhs[j]))) {
                j++;
            }

            tok.push_back(rhs.substr(i, j - i));
            i = j;
            continue;
        }
        i++;
    }
    return tok;
}

static bool looksLikeCFGLine(const string &line) {
    string s = trim(line);

    if(s.empty())
        return false;
    
    if(s.size() >= 2 && s.back() == ':' && isIdentStart(s[0]))
        return true;
    
    if(s.rfind("goto ", 0) == 0)
        return true;
    
    if(s.rfind("if", 0) == 0)
        return true;
    
    return false;
}

enum class InputMode {AUTO, LINEAR, CFG};

// linear
static bool parseLinearLineToInstruction(const string &lineRaw, Instruction &outInstr) {
    string line = lineRaw;

    // Remove comments
    size_t posComment = line.find("//");
    if(posComment != string::npos)
        line = line.substr(0, posComment);

    line = trim(line);
    if(line.empty())
        return false;

    // Skip braces/function hader
    if(line == "{" || line == "}")
        return false;
    
    if(line.find('(') != string::npos && line.find(')') != string::npos && line.find('{') != string::npos)
        return false;

    // Skip loops
    if(line.find("for") != string::npos || line.find("while") != string::npos || line.find("if") != string::npos)
        return false;

    // skip prefix
    if(line.rfind("int ", 0) == 0)
        line = trim(line.substr(4));
    if(line.rfind("long ", 0) == 0)
        line = trim(line.substr(5));

    // return
    if(line.rfind("return", 0) == 0) {
        string rhs = trim(line.substr(6));
        
        if(!rhs.empty() && rhs.back() == ';') {
            rhs.pop_back();
        }

        rhs = trim(rhs);
        if(rhs.empty())
            return false;
        
        line = "ret = " + rhs + ";";
    }

    // Strip ';'
    if(!line.empty() && line.back() == ';')
        line.pop_back();
    line = trim(line);

    size_t eq = line.find('=');
    if(eq == string::npos)
        return false;

    string lhs = trim(line.substr(0, eq));
    string rhs = trim(line.substr(eq + 1));

    if(lhs.empty() || rhs.empty())
        return false;

    vector<string> t = tokenizeRHS(rhs);
    if(t.empty())
        return false;

    outInstr.def = lhs;
    outInstr.use.clear();

    auto addUseIfVar = [&](const string &x) {
        if(!x.empty() && !isNumberToken(x))
            outInstr.use.push_back(x);
    };

    // Simple cases
    if(t.size() == 1) {
        addUseIfVar(t[0]);
        return true;
    }

    if(t.size() == 3 && (t[1] == "+" || t[1] == "-" || t[1] == "*" || t[1] == "/")) {
        addUseIfVar(t[0]);
        addUseIfVar(t[2]);
        return true;
    }

    // Long expression: keep all identifiers
    for(auto &tok : t) {
        if(tok == "+" || tok == "-" || tok == "*" || tok == "/")
            continue;
        addUseIfVar(tok);
    }
    return true;
}

static vector<Instruction> parseLinearFile(const string &filename) {
    ifstream fin(filename);
    if(!fin) {
        cerr << "Error: cannot open file: " << filename << "\n";
        return {};
    }
    vector<Instruction> prog;
    string line;

    while(getline(fin, line)) {
        Instruction ins;
        if(parseLinearLineToInstruction(line, ins))
            prog.push_back(ins);
    }
    return prog;
}

// CFG
static bool parseCFGAssignment(const string &line, IRInstr &out) {
    string s = trim(line);
    if(s.empty()) return false;

    if(!s.empty() && s.back() == ';') {
        s.pop_back();
    }
    s = trim(s);

    size_t eq = s.find('=');
    if(eq == string::npos) return false;

    string lhs = trim(s.substr(0, eq));
    string rhs = trim(s.substr(eq + 1));
    
    if(lhs.empty() || rhs.empty())
        return false;

    vector<string> t = tokenizeRHS(rhs);
    if(t.empty())
        return false;

    out = IRInstr{};
    out.raw = line;
    out.dst = lhs;

    auto parseAtom = [&](const string &tok, bool &isImm, long long &val, string &name) {
        if(isNumberToken(tok)) {
            isImm = true;
            val = stoll(tok);
            name = "";
        }
        else {
            isImm = false;
            val = 0;
            name = tok;
        }
    };

    // x = atom
    if(t.size() == 1) {
        out.op = OpKind::ASSIGN;
        parseAtom(t[0], out.aImm, out.aVal, out.a);
        return true;
    }

    // x = a op b (binary)
    if(t.size() == 3 && (t[1] == "+" || t[1] == "-" || t[1] == "*" || t[1] == "/")) {
        out.op = OpKind::BINOP;
        out.bop = t[1][0];

        parseAtom(t[0], out.aImm, out.aVal, out.a);
        parseAtom(t[2], out.bImm, out.bVal, out.b);

        return true;
    }

    out.op = OpKind::ASSIGN;
    out.aImm = false; out.aVal = 0;
    out.a = rhs;
    
    return true;
}

static vector<IRInstr> parseCFGFileToIR(const string &filename) {
    ifstream fin(filename);
    
    if(!fin) {
        cerr << "Error: cannot open file: " << filename << "\n";
        return {};
    }

    vector<IRInstr> ir;
    string lineRaw;

    while(getline(fin, lineRaw)) {
        string line = lineRaw;
        // remove comments
        size_t cpos = line.find("//");
        if(cpos != string::npos) line = line.substr(0, cpos);

        line = trim(line);
        if(line.empty()) continue;

        // Label: L0:
        if(line.size() >= 2 && line.back() == ':' && isIdentStart(line[0])) {
            IRInstr ins;
            ins.op = OpKind::LABEL;
            ins.label = trim(line.substr(0, line.size() - 1));
            ins.raw = lineRaw;
            ir.push_back(ins);
            continue;
        }

        // goto Lx
        if(line.rfind("goto ", 0) == 0) {
            IRInstr ins;
            ins.op = OpKind::GOTO;
            ins.target = trim(line.substr(5));
            if(!ins.target.empty() && ins.target.back() == ';') {
                ins.target.pop_back();
            }
            ins.target = trim(ins.target);
            ins.raw = lineRaw;
            ir.push_back(ins);
            continue;
        }

        // ifv goto L1 else L2
        if(line.rfind("if", 0) == 0) {
            vector<string> parts;
            {
                stringstream ss(line);
                string w;
                while(ss >> w) {
                    parts.push_back(w);
                }
            }
            
            if(parts.size() >= 6 && parts[0] == "if" && parts[2] == "goto" && parts[4] == "else") {
                IRInstr ins;
                ins.op = OpKind::IFGOTO;
                ins.cond = parts[1];
                ins.tlabel = parts[3];
                ins.flabel = parts[5];
                // strip ; ifattached
                if(!ins.tlabel.empty() && ins.tlabel.back() == ';') {
                    ins.tlabel.pop_back();
                }
                if(!ins.flabel.empty() && ins.flabel.back() == ';') {
                    ins.flabel.pop_back();
                }
                
                ins.raw = lineRaw;
                ir.push_back(ins);
                continue;
            }
        }

        // return expr
        if(line.rfind("return", 0) == 0) {
            string rhs = trim(line.substr(6));
            if(!rhs.empty() && rhs.back() == ';') {
                rhs.pop_back();
            }
            rhs = trim(rhs);

            IRInstr ins;
            
            ins.op = OpKind::RET;
            ins.raw = lineRaw;

            vector<string> t = tokenizeRHS(rhs);
            if(t.size() == 1) {
                if(isNumberToken(t[0])) {
                    ins.aImm = true; ins.aVal = stoll(t[0]);
                }
                else{
                    ins.aImm = false; ins.a = t[0];
                }
            }
            else if(t.size() == 3 && (t[1] == "+" || t[1] == "-" || t[1] == "*" || t[1] == "/")) {
                ins.a = rhs;
                ins.aImm = false;
            }
            else {
                ins.a = rhs;
                ins.aImm = false;
            }
            ir.push_back(ins);
            continue;
        }

        // Assignment / binop
        IRInstr ins;
        if(parseCFGAssignment(line, ins)) {
            ir.push_back(ins);
            continue;
        }
    }
    return ir;
}

// build cfg
static unordered_map<string, int> buildLabelToIndex(const vector<IRInstr> &ir) {
    unordered_map<string, int> mp;

    for(int i = 0; i < (int)ir.size(); i++) {
        if(ir[i].op == OpKind::LABEL) mp[ir[i].label] = i;
    }
    return mp;
}

static bool isTerminator(const IRInstr &ins) {
    return (ins.op == OpKind::GOTO) || (ins.op == OpKind::IFGOTO) || (ins.op == OpKind::RET);
}

// split into basic blocks using leaders algo
static vector<Block> buildBlocks(const vector<IRInstr> &ir) {
    vector<int> leaders;
    auto labelIndex = buildLabelToIndex(ir);

    auto markLeader = [&](int idx) {
        if(idx < 0 || idx >= (int)ir.size())
            return;
        leaders.push_back(idx);
    };

    if(!ir.empty()) {
        markLeader(0);
    }

    for(int i = 0; i < (int)ir.size(); i++) {
        const auto &ins = ir[i];
        if(ins.op == OpKind::GOTO) {
            if(labelIndex.count(ins.target)) {
                markLeader(labelIndex[ins.target]);
            }
            markLeader(i + 1);
        }
        else if(ins.op == OpKind::IFGOTO) {
            if(labelIndex.count(ins.tlabel)) {
                markLeader(labelIndex[ins.tlabel]);
            }

            if(labelIndex.count(ins.flabel)) markLeader(labelIndex[ins.flabel]);
            markLeader(i + 1);
        }
        else if(ins.op == OpKind::RET) {
            markLeader(i + 1);
        }
    }

    sort(leaders.begin(), leaders.end());
    leaders.erase(unique(leaders.begin(), leaders.end()), leaders.end());

    while(!leaders.empty() && leaders.back() >= (int)ir.size()) {
        leaders.pop_back();
    }

    vector<Block> blocks;
    for(int bi = 0; bi < (int)leaders.size(); bi++) {
        int start = leaders[bi];
        int end = (bi + 1 < (int)leaders.size()) ? leaders[bi + 1] : (int)ir.size();
        Block b;
        b.id = bi;
        b.start = start;
        b.end = end;
        blocks.push_back(b);
    }

    // Build map: IR index -> block id
    vector<int> blockOf(ir.size(), -1);
    for(auto &b : blocks) {
        for(int i = b.start; i < b.end; i++) {
            blockOf[i] = b.id;
        }
    }

    auto labelToBlock = [&](const string &lab)->int {
        auto it = labelIndex.find(lab);
        if(it == labelIndex.end()) return -1;
        
        int idx = it->second;
        
        if(idx < 0 || idx >= (int)blockOf.size()) return -1;
        return blockOf[idx];
    };

    for(auto &b : blocks) {
        b.succ.clear();

        int last = b.end - 1;
        
        while(last >= b.start && ir[last].op == OpKind::LABEL) {
            last--;
        }
        if(last < b.start) {
            if(b.id + 1 < (int)blocks.size()) {
                b.succ.push_back(b.id + 1);
            }
            continue;
        }

        const auto &term = ir[last];
        if(term.op == OpKind::GOTO) {
            int t = labelToBlock(term.target);
            if(t != -1) {
                b.succ.push_back(t);
            }
        }
        else if(term.op == OpKind::IFGOTO) {
            int t = labelToBlock(term.tlabel);
            int f = labelToBlock(term.flabel);
            
            if(t != -1) {
                b.succ.push_back(t);
            }
            if(f != -1 && f != t) b.succ.push_back(f);
        }
        else if(term.op == OpKind::RET) {
            // no successor
        }
        else {
            if(b.id + 1 < (int)blocks.size()) {
                b.succ.push_back(b.id + 1);
            }
        }
    }
    return blocks;
}


static Instruction toAnalysisInstr(const IRInstr &ins) {
    Instruction a;
    a.def = "";
    a.use.clear();

    auto addUseIfVar = [&](const string &x) {
        if(x.empty()) return;
        
        if(isNumberToken(x)) return;
        a.use.push_back(x);
    };

    if(ins.op == OpKind::LABEL || ins.op == OpKind::GOTO)
        return a;

    if(ins.op == OpKind::IFGOTO) {
        addUseIfVar(ins.cond);
        return a;
    }

    if(ins.op == OpKind::RET) {
        if(ins.aImm) return a;

        vector<string> t = tokenizeRHS(ins.a.empty() ? "" : ins.a);
        
        if(t.empty()) addUseIfVar(ins.a);
        else {
            for(auto &tok : t) {
                if(tok != "+" && tok != "-" && tok != "*" && tok != "/") {
                    addUseIfVar(tok);
                }
            }
        }
        return a;
    }

    a.def = ins.dst;

    if(ins.op == OpKind::BINOP) {
        if(!ins.aImm) 
            addUseIfVar(ins.a);
        if(!ins.bImm) 
            addUseIfVar(ins.b);
        return a;
    }

    // ASSIGN: could be atom or long expression stored in 'a' string
    if(ins.op == OpKind::ASSIGN) {
        if(ins.aImm)
            return a;
        // ifa looks like expression, tokenize
        vector<string> t = tokenizeRHS(ins.a);

        if(t.empty()) addUseIfVar(ins.a);
        else {
            for(auto &tok : t) {
                if(tok != "+" && tok != "-" && tok != "*" && tok != "/") {
                    addUseIfVar(tok);
                }
            }
        }
        return a;
    }

    return a;
}

// Linear liveness (single successor i+1)
static void livenessLinear(const vector<Instruction> &program, vector<set<string>> &IN, vector<set<string>> &OUT) {
    int n = (int)program.size();
    IN.assign(n, {});
    OUT.assign(n, {});

    bool changed = true;
    while(changed) {
        changed = false;

        for(int i = n - 1; i >= 0; i--) {
            auto oldIN = IN[i];
            auto oldOUT = OUT[i];

            if(i != n - 1)
                OUT[i] = IN[i + 1];
            else OUT[i].clear();

            IN[i].clear();

            for(auto &u : program[i].use)
                IN[i].insert(u);
            for(auto &o : OUT[i]) if(o != program[i].def)
                IN[i].insert(o);

            if(IN[i] != oldIN || OUT[i] != oldOUT)
                changed = true;
        }
    }
}

// CFG liveness
//  1) compute USE/DEF per block
//  2) fixed point on IN/OUT blocks
//  3) refine to per-instruction by scanning backward within each block

static void livenessCFG(const vector<Instruction> &A, const vector<Block> &blocks, vector<set<string>> &INi, vector<set<string>> &OUTi) {
    int n = (int)A.size();
    INi.assign(n, {});
    OUTi.assign(n, {});

    int B = (int)blocks.size();
    vector<set<string>> USE(B), DEF(B), INB(B), OUTB(B);

    // Build USE/DEF foreach block
    for(auto &blk : blocks) {
        set<string> defset, useset;
        for(int i = blk.start; i < blk.end; i++) {
            for(auto &u : A[i].use) {
                if(!defset.count(u))
                    useset.insert(u);
            }
            // defs
            if(!A[i].def.empty())
                defset.insert(A[i].def);
        }

        USE[blk.id] = useset;
        DEF[blk.id] = defset;
    }

    bool changed = true;
    while(changed) {
        changed = false;
        for(int bi = B - 1; bi >= 0; bi--) {
            auto oldIN = INB[bi];
            auto oldOUT = OUTB[bi];

            OUTB[bi].clear();
            for(int s : blocks[bi].succ) {
                for(auto &v : INB[s]){
                    OUTB[bi].insert(v);
                }
            }

            INB[bi] = USE[bi];
            for(auto &v : OUTB[bi]) if(!DEF[bi].count(v)) INB[bi].insert(v);

            if(INB[bi] != oldIN || OUTB[bi] != oldOUT) changed = true;
        }
    }

    // Instruction-level liveness within each block
    for(auto &blk : blocks) {
        set<string> live = OUTB[blk.id];
        for(int i = blk.end - 1; i >= blk.start; i--) {
            OUTi[i] = live;
            INi[i].clear();
            for(auto &u : A[i].use)
                INi[i].insert(u);
            for(auto &v : OUTi[i]) if(v != A[i].def)
                INi[i].insert(v);
            live = INi[i];
        }
    }
}

// Interference Graph
static map<string, set<string>> buildInterferenceGraph(const vector<Instruction> &program, const vector<set<string>> &OUT) {
    map<string, set<string>> g;

    // Ensure all variables appear as nodes
    for(auto &ins : program) {
        if(!ins.def.empty())
            g[ins.def];
        for(auto &u : ins.use)
            g[u];
    }

    for(int i = 0; i < (int)program.size(); i++) {
        const string &def = program[i].def;
        if(def.empty()) continue;

        for(auto &live : OUT[i]) {
            if(live == def) continue;
            g[def].insert(live);
            g[live].insert(def);
        }
    }
    return g;
}

// Features
static map<string, int> computeUseCounts(const vector<Instruction> &program) {
    map<string, int> useCount;
    for(auto &instr : program) {
        for(auto &u : instr.use) {
            useCount[u]++;
        }
    }
    return useCount;
}

static map<string, int> computeUseInstrCounts(const vector<Instruction> &program) {
    map<string, int> useInstrCount;
    for(int i = 0; i < (int)program.size(); i++) {
        set<string> seen;
        for(auto &u : program[i].use)
            seen.insert(u);
        for(auto &v : seen)
            useInstrCount[v]++;
    }
    return useInstrCount;
}

static map<string, int> computeLiveRangeFromDataflow(const vector<Instruction> &program, const vector<set<string>> &IN, const vector<set<string>> &OUT, const set<string> &varsAll) {
    map<string, int> startIdx, endIdx;
    for(auto &v : varsAll) {
        startIdx[v] = -1; endIdx[v] = -1;
    }

    for(int i = 0; i < (int)program.size(); i++) {
        for(auto &v : IN[i]) {
            if(startIdx[v] == -1)
                startIdx[v] = i; endIdx[v] = max(endIdx[v], i);
        }
        if(!program[i].def.empty()) {
            string d = program[i].def;
            if(startIdx[d] == -1)
                startIdx[d] = i;
            endIdx[d] = max(endIdx[d], i);
        }

        for(auto &v : OUT[i]) {
            if(startIdx[v] == -1)
                startIdx[v] = i; endIdx[v] = max(endIdx[v], i);
        }
    }

    map<string, int> liveLen;
    for(auto &v : varsAll) {
        int s = startIdx[v], e = endIdx[v];
        
        if(s == -1 || e == -1)
            liveLen[v] = 0;
        else
            liveLen[v] = max(0, e - s);
    }

    return liveLen;
}

static map<string, int> computeSpillCost(const set<string> &varsAll, const map<string, int> &useCount, const map<string, int> &liveLen) {
    map<string, int> cost;
    for(auto &v : varsAll) {
        int uc = useCount.count(v) ? useCount.at(v) : 0;
        int lr = liveLen.count(v) ? liveLen.at(v) : 0;
        cost[v] = uc * (lr + 1);
    }
    
    return cost;
}

// 9) Baseline allocator (greedy graph coloring style)
static vector<string> buildAllocationOrder(const set<string> &defs, const map<string, set<string>> &interference, const map<string, int> &spillCost) {
    vector<string> order(defs.begin(), defs.end());

    sort(order.begin(), order.end(), [&](const string &a, const string &b) {
        int da = interference.count(a) ? (int)interference.at(a).size() : 0;
        int db = interference.count(b) ? (int)interference.at(b).size() : 0;

        if(da != db)
            return da > db; // first choose highest degree

        int ca = spillCost.count(a) ? spillCost.at(a) : 0;
        int cb = spillCost.count(b) ? spillCost.at(b) : 0;

        if(ca != cb)
            return ca > cb; // first choose high spill cost

        return a < b;
    });
    return order;
}

static map<string, int> greedyRegisterAllocationOrdered(const map<string, set<string>> &interference, const vector<string> &order, int K) {
    map<string, int> regAssignment; // var -> regIndex, -1 spill

    for(auto &v : order) {
        set<int> used;
        if(interference.count(v)) {
            for(auto &nbr : interference.at(v)) {
                if(regAssignment.count(nbr) && regAssignment[nbr] != -1)
                    used.insert(regAssignment[nbr]);
            }
        }

        int reg = 0;
        
        for(; reg < K; reg++)
            if(!used.count(reg)) break;
        regAssignment[v] = (reg < K) ? reg : -1;
    }

    return regAssignment;
}

// 10) CSV + baseline report outputs
static void dumpFeaturesCSV(const string &file, const string &programId, int K, const set<string> &defs, const map<string, set<string>> &interference, const map<string, int> &useCount, const map<string, int> &useInstrCount, const map<string, int> &liveLen, const map<string, int> &spillCost, const map<string, int> &alloc) {
    bool needHeader = false;
    {
        ifstream fin(file);
        needHeader = !fin.good() || fin.peek() == ifstream::traits_type::eof();
    }

    ofstream fout(file, ios::app);

    if(!fout) {
        cerr << "Error: cannot open " << file << " forwriting\n";
        return;
    }
    
    if(needHeader) {
        fout << "program_id,var,degree,use_count,use_instr_count,live_range,spill_cost,K,label\n";
    }

    for(auto &v : defs) {
        int degree = interference.count(v) ? (int)interference.at(v).size() : 0;
        int uc = useCount.count(v) ? useCount.at(v) : 0;
        int uic = useInstrCount.count(v) ? useInstrCount.at(v) : 0;
        int lr = liveLen.count(v) ? liveLen.at(v) : 0;
        int sc = spillCost.count(v) ? spillCost.at(v) : 0;
        int label = (alloc.count(v) && alloc.at(v) != -1) ? 1 : 0;

        fout << programId << "," << v << "," << degree << "," << uc << "," << uic << "," << lr << "," << sc << "," << K << "," << label << "\n";
    }
}

static void dumpBaselineReport(const string &file, const string &programId, int K, const set<string> &defs, const map<string, int> &alloc, const map<string, int> &spillCost) {
    bool needHeader = false;
    {
        ifstream fin(file);
        needHeader = !fin.good() || fin.peek() == ifstream::traits_type::eof();
    }

    ofstream fout(file, ios::app);
    if(!fout) {
        cerr << "Error: cannot open " << file << " forwriting\n";
        return;
    }

    if(needHeader) fout << "program_id,K,vars,spills,spill_cost_sum\n";

    int spills = 0;
    long long spillCostSum = 0;
    
    for(auto &v : defs) {
        if(alloc.count(v) && alloc.at(v) == -1) {
            spills++;
            spillCostSum += spillCost.count(v) ? spillCost.at(v) : 0;
        }
    }
    
    fout << programId << "," << K << "," << defs.size() << "," << spills << "," << spillCostSum << "\n";
}

// File collection (folder scan)
static vector<string> collectInputFiles(const vector<string> &args) {
    vector<string> files;

    if(args.size() == 1) {
        fs::path p(args[0]);
        if(fs::exists(p) && fs::is_directory(p)) {
            for(auto &entry : fs::directory_iterator(p)) {
                if(!entry.is_regular_file()) continue;
                auto ext = entry.path().extension().string();
                // accept c-like and cfg-like extensions
                if(ext == ".c" || ext == ".txt" || ext == ".ir" || ext == ".toy" || ext == ".cfg") {
                    files.push_back(entry.path().string());
                }
            }
            sort(files.begin(), files.end());
            return files;
        }
    }
    files = args;
    return files;
}

// 13) runn
static void runProgramLinear(const string &filename, const vector<int> &Kvalues, const string &featuresCSV, const string &baselineCSV) {
    auto prog = parseLinearFile(filename);
    if(prog.empty()) {
        cerr << "Skipping (no parsable instructions): " << filename << "\n";
        return;
    }

    vector<set<string>> IN, OUT;
    livenessLinear(prog, IN, OUT);

    auto interference = buildInterferenceGraph(prog, OUT);

    set<string> defs, varsAll;
    for(auto &ins : prog) {
        if(!ins.def.empty()) {
            defs.insert(ins.def);
            varsAll.insert(ins.def);
        }

        for(auto &u : ins.use)
            varsAll.insert(u);
    }

    auto useCnt = computeUseCounts(prog);
    auto useInstrCnt = computeUseInstrCounts(prog);
    auto liveLen = computeLiveRangeFromDataflow(prog, IN, OUT, varsAll);
    auto spillCost = computeSpillCost(varsAll, useCnt, liveLen);

    fs::path p(filename);
    string programId = p.filename().string();
    for(char &c : programId)
        if(c == ',')
            c = '_';

    for(int K : Kvalues) {
        auto order = buildAllocationOrder(defs, interference, spillCost);
        auto alloc = greedyRegisterAllocationOrdered(interference, order, K);

        dumpFeaturesCSV(featuresCSV, programId, K, defs, interference, useCnt, useInstrCnt, liveLen, spillCost, alloc);
        dumpBaselineReport(baselineCSV, programId, K, defs, alloc, spillCost);
    }
}

static void runProgramCFG(const string &filename, const vector<int> &Kvalues, const string &featuresCSV, const string &baselineCSV) {
    auto ir = parseCFGFileToIR(filename);

    if(ir.empty()) {
        cerr << "Skipping (no parsable IR): " << filename << "\n";
        return;
    }

    // Build blocks
    auto blocks = buildBlocks(ir);

    // Build analysis-only program (def/use list aligned with IR indices)
    vector<Instruction> A;
    A.reserve(ir.size());
    for(auto &ins : ir)
        A.push_back(toAnalysisInstr(ins));

    vector<set<string>> IN, OUT;
    livenessCFG(A, blocks, IN, OUT);

    auto interference = buildInterferenceGraph(A, OUT);

    set<string> defs, varsAll;
    for(auto &ins : A) {
        if(!ins.def.empty()) {
            defs.insert(ins.def);
            varsAll.insert(ins.def);
        }

        for(auto &u : ins.use)
            varsAll.insert(u);
    }

    auto useCnt = computeUseCounts(A);
    auto useInstrCnt = computeUseInstrCounts(A);
    auto liveLen = computeLiveRangeFromDataflow(A, IN, OUT, varsAll);
    auto spillCost = computeSpillCost(varsAll, useCnt, liveLen);

    fs::path p(filename);
    string programId = p.filename().string();
    for(char &c : programId)
        if(c == ',')
            c = '_';

    for(int K : Kvalues) {
        auto order = buildAllocationOrder(defs, interference, spillCost);
        auto alloc = greedyRegisterAllocationOrdered(interference, order, K);

        dumpFeaturesCSV(featuresCSV, programId, K, defs, interference, useCnt, useInstrCnt, liveLen, spillCost, alloc);
        dumpBaselineReport(baselineCSV, programId, K, defs, alloc, spillCost);
    }
}

// main
static void printUsage(const char *exe) {
    cout << "Usage:\n";
    cout << "  " << exe << " <folder>\n";
    cout << "  " << exe << " <file1> [file2 ...]\n\n";
    cout << "Optional flags:\n";
    cout << "  --mode=auto|linear|cfg    (default: auto)\n";
    cout << "Notes:\n";
    cout << "  - Linear mode accepts .c/.txt with lines like: int x = a + 1; return x;\n";
    cout << "  - CFG mode uses mini-language: labels, goto, if-goto-else, return.\n";
}

int main(int argc, char **argv) {
    if(argc < 2) {
        printUsage(argv[0]);
        return 0;
    }
    InputMode mode = InputMode::AUTO;

    vector<string> positional;
    for(int i = 1; i < argc; i++) {
        string a = argv[i];
        if(a.rfind("--mode=", 0) == 0) {
            string m = a.substr(7);
            if(m == "auto")
                mode = InputMode::AUTO;
            else if(m == "linear")
                mode = InputMode::LINEAR;
            else if(m == "cfg")
                mode = InputMode::CFG;
            else
                cerr << "Unknown mode: " << m << "\n";
        }
        else {
            positional.push_back(a);
        }
    }

    vector<string> inputFiles = collectInputFiles(positional);
    if(inputFiles.empty()) {
        cerr << "No input files found.\n";
        return 1;
    }

    vector<int> Kvalues = {1, 2, 3, 4, 5};

    const string featuresCSV = "features.csv";
    const string baselineCSV = "baseline_report.csv";

    cout << "Processing " << inputFiles.size() << " file(s)\n";
    cout << "Mode: " << (mode==InputMode::AUTO?"auto":(mode==InputMode::LINEAR?"linear":"cfg")) << "\n";
    cout << "K values: ";
    
    for(size_t i = 0; i < Kvalues.size(); i++)
        cout << Kvalues[i] << (i+1<Kvalues.size()? ", ":"");
    cout << "\n\n";

    for(auto &filename : inputFiles) {
        InputMode fileMode = mode;

        if(mode == InputMode::AUTO) {
            ifstream fin(filename);
            bool cfg = false;
            string line;
            int count = 0;
            while(count++ < 50 && getline(fin, line)) {
                if(looksLikeCFGLine(line)) {
                    cfg = true;
                    break;
                }
            }
            fileMode = cfg ? InputMode::CFG : InputMode::LINEAR;
        }

        cout << "-->" << filename << " -> " << (fileMode==InputMode::CFG?"CFG":"LINEAR") << "\n";
        
        if(fileMode == InputMode::LINEAR) {
            runProgramLinear(filename, Kvalues, featuresCSV, baselineCSV);
        }
        else {
            runProgramCFG(filename, Kvalues, featuresCSV, baselineCSV);
        }
    }

    cout << "\nGenerated:\n  " << featuresCSV << "\n  " << baselineCSV << endl;
    cout << "finished." << endl;
    return 0;
}
