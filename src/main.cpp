#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <stack>
#include <deque>
#include <algorithm>

// Global node counter
int nodeCount = 0;

const char EPSILON = '#';

// Global character sets for NFA and DFA
std::set<char> nfaCharSet;
std::set<char> dfaCharSet;

// Global result for generated code
std::string resultCode;

// Set to string conversion
std::string set2string(const std::set<int>& s) {
    std::string result;
    for (int i : s) {
        result.append(std::to_string(i));
        result.append(",");
    }
    if (!result.empty()) result.pop_back(); // Remove trailing comma
    return result;
}

/*============================ Regular Expression Preprocessing ==============================*/

// Check if a character is alphabetic
bool isChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Process regular expression (add concatenation dots)
std::string handleRegex(std::string_view regex) {
    std::string regexStd = std::string(regex);
    std::cout << "Processing regex: " << regexStd << '\n';

    // Handle square brackets
    std::string result;
    bool insideBrackets = false;
    std::string currentString;

    for (char c : regexStd) {
        if (c == '[') {
            insideBrackets = true;
            currentString.push_back('(');
        } else if (c == ']') {
            insideBrackets = false;
            currentString.push_back(')');
            result += currentString;
            currentString.clear();
        } else if (insideBrackets) {
            if (currentString.length() > 1) {
                currentString.push_back('|');
            }
            currentString.push_back(c);
        } else {
            result.push_back(c);
        }
    }

    regexStd = result;

    // Handle '+' operator
    for (size_t i = 0; i < regexStd.size(); ++i) {
        if (regexStd[i] == '+') {
            int kcount = 0;
            size_t j = i;
            do {
                --j;
                if (regexStd[j] == ')') ++kcount;
                else if (regexStd[j] == '(') --kcount;
            } while (kcount != 0);
            std::string str1 = regexStd.substr(0, j);
            std::string kstr = regexStd.substr(j, i - j);
            std::string str2 = regexStd.substr(i + 1);
            regexStd = str1 + kstr + kstr + "*" + str2;
        }
    }

    // Insert concatenation dots
    for (size_t i = 0; i < regexStd.size() - 1; ++i) {
        if ((isChar(regexStd[i]) && isChar(regexStd[i + 1])) ||
            (isChar(regexStd[i]) && regexStd[i + 1] == '(') ||
            (regexStd[i] == ')' && isChar(regexStd[i + 1])) ||
            (regexStd[i] == ')' && regexStd[i + 1] == '(') ||
            (regexStd[i] == '*' && regexStd[i + 1] != ')' && regexStd[i + 1] != '|' && regexStd[i + 1] != '?') ||
            (regexStd[i] == '?' && regexStd[i + 1] != ')') ||
            (regexStd[i] == '+' && regexStd[i + 1] != ')')) {
            regexStd.insert(i + 1, 1, '.');
            ++i; // Skip the inserted dot
        }
    }

    return regexStd;
}

// Handle multiple regex lines
std::string handleMoreLine(std::string_view regex) {
    std::istringstream iss(regex.data());
    std::vector<std::string> lines;
    std::string line;

    while (std::getline(iss, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    std::string output;
    for (size_t i = 0; i < lines.size(); ++i) {
        output += "(" + lines[i] + ")";
        if (i < lines.size() - 1) {
            output += "|";
        }
    }

    return output;
}

/*============================ NFA Construction ==============================*/

struct nfaNode;

// NFA edge
struct nfaEdge {
    char c;
    nfaNode* next;
};

// NFA node
struct nfaNode {
    int id;
    bool isStart;
    bool isEnd;
    std::vector<nfaEdge> edges;
    nfaNode() : id(nodeCount++), isStart(false), isEnd(false) {}
};

// NFA structure
struct NFA {
    nfaNode* start;
    nfaNode* end;
    NFA() = default;
    NFA(nfaNode* s, nfaNode* e) : start(s), end(e) {}
};

// Create basic NFA for a single character
NFA CreateBasicNFA(char character) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();
    start->isStart = true;
    end->isEnd = true;
    start->edges.push_back({character, end});
    nfaCharSet.insert(character);
    dfaCharSet.insert(character);
    return {start, end};
}

// Create concatenation NFA
NFA CreateConcatenationNFA(NFA nfa1, NFA nfa2) {
    nfa1.end->isEnd = false;
    nfa2.start->isStart = false;
    nfa1.end->edges.push_back({EPSILON, nfa2.start});
    return {nfa1.start, nfa2.end};
}

// Create union NFA
NFA CreateUnionNFA(NFA nfa1, NFA nfa2) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();
    start->isStart = true;
    end->isEnd = true;
    start->edges.push_back({EPSILON, nfa1.start});
    start->edges.push_back({EPSILON, nfa2.start});
    nfa1.start->isStart = false;
    nfa2.start->isStart = false;
    nfa1.end->isEnd = false;
    nfa2.end->isEnd = false;
    nfa1.end->edges.push_back({EPSILON, end});
    nfa2.end->edges.push_back({EPSILON, end});
    return {start, end};
}

// Create zero or more (Kleene star) NFA
NFA CreateZeroOrMoreNFA(NFA nfa1) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();
    start->isStart = true;
    end->isEnd = true;
    start->edges.push_back({EPSILON, nfa1.start});
    start->edges.push_back({EPSILON, end});
    nfa1.start->isStart = false;
    nfa1.end->isEnd = false;
    nfa1.end->edges.push_back({EPSILON, nfa1.start});
    nfa1.end->edges.push_back({EPSILON, end});
    return {start, end};
}

// Create optional NFA
NFA CreateOptionalNFA(NFA nfa1) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();
    start->isStart = true;
    end->isEnd = true;
    start->edges.push_back({EPSILON, nfa1.start});
    start->edges.push_back({EPSILON, end});
    nfa1.start->isStart = false;
    nfa1.end->isEnd = false;
    nfa1.end->edges.push_back({EPSILON, end});
    return {start, end};
}

// Operator precedence
int Precedence(char op) {
    switch (op) {
        case '|': return 1;
        case '.': return 2;
        case '*':
        case '?': return 3;
        default: return 0;
    }
}

struct statusTableNode {
    std::string flag;
    int id;
    std::map<char, std::set<int>> m;
    statusTableNode() : flag("") {}
};

std::unordered_map<int, statusTableNode> statusTable;
std::vector<int> insertionOrder;
std::set<int> startNFAstatus;
std::set<int> endNFAstatus;

// Create NFA status table
void createNFAStatusTable(NFA& nfa) {
    std::stack<nfaNode*> nfaStack;
    std::set<nfaNode*> visitedNodes;
    nfaNode* startNode = nfa.start;
    statusTableNode startStatusNode;
    startStatusNode.flag = "-";
    startStatusNode.id = startNode->id;
    statusTable[startNode->id] = startStatusNode;
    insertionOrder.push_back(startNode->id);
    startNFAstatus.insert(startNode->id);
    nfaStack.push(startNode);

    while (!nfaStack.empty()) {
        nfaNode* currentNode = nfaStack.top();
        nfaStack.pop();
        if (visitedNodes.count(currentNode)) continue;
        visitedNodes.insert(currentNode);

        for (const auto& edge : currentNode->edges) {
            char transitionChar = edge.c;
            nfaNode* nextNode = edge.next;
            statusTable[currentNode->id].m[transitionChar].insert(nextNode->id);

            if (!visitedNodes.count(nextNode)) {
                nfaStack.push(nextNode);
                statusTableNode nextStatus;
                nextStatus.id = nextNode->id;
                if (nextNode->isStart) {
                    nextStatus.flag += "-";
                    startNFAstatus.insert(nextStatus.id);
                }
                if (nextNode->isEnd) {
                    nextStatus.flag += "+";
                    endNFAstatus.insert(nextStatus.id);
                }
                statusTable[nextNode->id] = nextStatus;
                if (!nextNode->isEnd) {
                    insertionOrder.push_back(nextNode->id);
                }
            }
        }
    }

    insertionOrder.push_back(nfa.end->id);
}

// Convert regex to NFA
NFA regex2NFA(std::string_view regex) {
    std::stack<char> opStack;
    std::stack<NFA> nfaStack;

    for (char c : regex) {
        switch (c) {
            case ' ': break;
            case '(':
                opStack.push(c);
                break;
            case ')':
                while (!opStack.empty() && opStack.top() != '(') {
                    char op = opStack.top();
                    opStack.pop();
                    if (op == '|') {
                        NFA nfa2 = nfaStack.top(); nfaStack.pop();
                        NFA nfa1 = nfaStack.top(); nfaStack.pop();
                        nfaStack.push(CreateUnionNFA(nfa1, nfa2));
                    } else if (op == '.') {
                        NFA nfa2 = nfaStack.top(); nfaStack.pop();
                        NFA nfa1 = nfaStack.top(); nfaStack.pop();
                        nfaStack.push(CreateConcatenationNFA(nfa1, nfa2));
                    }
                }
                if (opStack.empty()) {
                    std::cerr << "Error: Unmatched parenthesis in regex\n";
                    std::exit(1);
                }
                opStack.pop();
                break;
            case '|':
            case '.':
                while (!opStack.empty() && opStack.top() != '(' &&
                       Precedence(opStack.top()) >= Precedence(c)) {
                    char op = opStack.top();
                    opStack.pop();
                    NFA nfa2 = nfaStack.top(); nfaStack.pop();
                    NFA nfa1 = nfaStack.top(); nfaStack.pop();
                    if (op == '|') {
                        nfaStack.push(CreateUnionNFA(nfa1, nfa2));
                    } else if (op == '.') {
                        nfaStack.push(CreateConcatenationNFA(nfa1, nfa2));
                    }
                }
                opStack.push(c);
                break;
            case '?':
            case '*':
                if (!nfaStack.empty()) {
                    NFA nfa = nfaStack.top(); nfaStack.pop();
                    if (c == '?') {
                        nfaStack.push(CreateOptionalNFA(nfa));
                    } else if (c == '*') {
                        nfaStack.push(CreateZeroOrMoreNFA(nfa));
                    }
                } else {
                    std::cerr << "Error: Closure operator without operand\n";
                    std::exit(1);
                }
                break;
            default:
                nfaStack.push(CreateBasicNFA(c));
                break;
        }
    }

    while (!opStack.empty()) {
        char op = opStack.top();
        opStack.pop();
        if (op == '|' || op == '.') {
            if (nfaStack.size() < 2) {
                std::cerr << "Error: Insufficient operands for operator " << op << '\n';
                std::exit(1);
            }
            NFA nfa2 = nfaStack.top(); nfaStack.pop();
            NFA nfa1 = nfaStack.top(); nfaStack.pop();
            if (op == '|') {
                nfaStack.push(CreateUnionNFA(nfa1, nfa2));
            } else {
                nfaStack.push(CreateConcatenationNFA(nfa1, nfa2));
            }
        } else {
            std::cerr << "Error: Unknown operator " << op << '\n';
            std::exit(1);
        }
    }

    NFA result = nfaStack.top();
    std::cout << "NFA constructed\n";
    createNFAStatusTable(result);
    std::cout << "NFA status table constructed\n";
    return result;
}

/*============================ NFA to DFA ==============================*/

struct dfaNode {
    std::string flag;
    std::set<int> nfaStates;
    std::map<char, std::set<int>> transitions;
    dfaNode() : flag("") {}
};

std::set<std::set<int>> dfaStatusSet;
std::vector<dfaNode> dfaTable;
std::set<int> dfaEndStatusSet;
std::set<int> dfaNotEndStatusSet;
std::map<std::set<int>, int> dfa2numberMap;
int startStatus;

std::string setHasStartOrEnd(const std::set<int>& statusSet) {
    std::string result;
    if (statusSet.count(*startNFAstatus.begin())) result += "-";
    for (const int& element : endNFAstatus) {
        if (statusSet.count(element)) {
            result += "+";
            break;
        }
    }
    return result;
}

std::set<int> epsilonClosure(int id) {
    std::set<int> eResult{id};
    std::stack<int> stack;
    stack.push(id);

    while (!stack.empty()) {
        int current = stack.top();
        stack.pop();
        for (int t : statusTable[current].m[EPSILON]) {
            if (eResult.insert(t).second) {
                stack.push(t);
            }
        }
    }
    return eResult;
}

std::set<int> otherCharClosure(int id, char ch) {
    std::set<int> otherResult;
    std::set<int> processed;
    std::stack<int> stack;
    stack.push(id);

    while (!stack.empty()) {
        int current = stack.top();
        stack.pop();
        if (processed.count(current)) continue;
        processed.insert(current);
        for (int o : statusTable[current].m[ch]) {
            auto tmp = epsilonClosure(o);
            otherResult.insert(tmp.begin(), tmp.end());
            stack.push(o);
        }
    }
    return otherResult;
}

void NFA2DFA(NFA& nfa) {
    int dfaStatusCount = 1;
    dfaNode startDFANode;
    startDFANode.nfaStates = epsilonClosure(nfa.start->id);
    startDFANode.flag = setHasStartOrEnd(startDFANode.nfaStates);
    std::deque<std::set<int>> newStatus;
    dfa2numberMap[startDFANode.nfaStates] = dfaStatusCount;
    startStatus = dfaStatusCount;
    if (startDFANode.flag.find('+') != std::string::npos) {
        dfaEndStatusSet.insert(dfaStatusCount++);
    } else {
        dfaNotEndStatusSet.insert(dfaStatusCount++);
    }

    for (char ch : dfaCharSet) {
        std::set<int> thisChClosure;
        for (int c : startDFANode.nfaStates) {
            auto tmp = otherCharClosure(c, ch);
            thisChClosure.insert(tmp.begin(), tmp.end());
        }
        if (thisChClosure.empty()) continue;
        size_t presize = dfaStatusSet.size();
        dfaStatusSet.insert(thisChClosure);
        startDFANode.transitions[ch] = thisChClosure;
        if (dfaStatusSet.size() > presize) {
            dfa2numberMap[thisChClosure] = dfaStatusCount;
            newStatus.push_back(thisChClosure);
            if (setHasStartOrEnd(thisChClosure).find('+') != std::string::npos) {
                dfaEndStatusSet.insert(dfaStatusCount++);
            } else {
                dfaNotEndStatusSet.insert(dfaStatusCount++);
            }
        }
    }
    dfaTable.push_back(startDFANode);

    while (!newStatus.empty()) {
        auto ns = newStatus.front();
        newStatus.pop_front();
        dfaNode DFANode;
        DFANode.nfaStates = ns;
        DFANode.flag = setHasStartOrEnd(ns);

        for (char ch : dfaCharSet) {
            std::set<int> thisChClosure;
            for (int c : ns) {
                auto tmp = otherCharClosure(c, ch);
                thisChClosure.insert(tmp.begin(), tmp.end());
            }
            if (thisChClosure.empty()) continue;
            size_t presize = dfaStatusSet.size();
            dfaStatusSet.insert(thisChClosure);
            DFANode.transitions[ch] = thisChClosure;
            if (dfaStatusSet.size() > presize) {
                dfa2numberMap[thisChClosure] = dfaStatusCount;
                newStatus.push_back(thisChClosure);
                if (setHasStartOrEnd(thisChClosure).find('+') != std::string::npos) {
                    dfaEndStatusSet.insert(dfaStatusCount++);
                } else {
                    dfaNotEndStatusSet.insert(dfaStatusCount++);
                }
            }
        }
        dfaTable.push_back(DFANode);
    }
    std::cout << "DFA constructed\n";
}

/*============================ DFA Minimization ==============================*/

struct dfaMinNode {
    std::string flag;
    int id;
    std::map<char, int> transitions;
    dfaMinNode() : flag("") {}
};

std::vector<dfaMinNode> dfaMinTable;
std::vector<std::set<int>> divideVector;
std::map<int, int> dfaMinMap;

std::string minSetHasStartOrEnd(const std::set<int>& statusSet) {
    std::string result;
    if (statusSet.count(startStatus)) result += "-";
    for (const int& element : dfaEndStatusSet) {
        if (statusSet.count(element)) {
            result += "+";
            break;
        }
    }
    return result;
}

void splitSet(int i, char ch) {
    std::set<int> result;
    auto& node = divideVector[i];
    int s = -2;

    for (int state : node) {
        int thisNum;
        if (dfaTable[state - 1].transitions.find(ch) == dfaTable[state - 1].transitions.end()) {
            thisNum = -1;
        } else {
            int next_state = dfa2numberMap[dfaTable[state - 1].transitions[ch]];
            thisNum = dfaMinMap[next_state];
        }
        if (s == -2) s = thisNum;
        else if (thisNum != s) result.insert(state);
    }

    for (int state : result) node.erase(state);
    if (!result.empty()) {
        divideVector.push_back(result);
        for (int a : result) dfaMinMap[a] = divideVector.size() - 1;
    }
}

void DFAminimize() {
    divideVector.clear();
    dfaMinMap.clear();

    if (!dfaNotEndStatusSet.empty()) {
        divideVector.push_back(dfaNotEndStatusSet);
        for (int t : dfaNotEndStatusSet) dfaMinMap[t] = divideVector.size() - 1;
    }
    divideVector.push_back(dfaEndStatusSet);
    for (int t : dfaEndStatusSet) dfaMinMap[t] = divideVector.size() - 1;

    bool continueFlag = true;
    while (continueFlag) {
        continueFlag = false;
        size_t size1 = divideVector.size();
        for (size_t i = 0; i < size1; ++i) {
            for (char ch : dfaCharSet) splitSet(i, ch);
        }
        if (divideVector.size() > size1) continueFlag = true;
    }

    for (size_t dfaMinCount = 0; dfaMinCount < divideVector.size(); ++dfaMinCount) {
        auto& v = divideVector[dfaMinCount];
        dfaMinNode d;
        d.flag = minSetHasStartOrEnd(v);
        d.id = dfaMinCount;
        for (char ch : dfaCharSet) {
            if (v.empty()) {
                d.transitions[ch] = -1;
                continue;
            }
            int i = *v.begin();
            if (dfaTable[i - 1].transitions.find(ch) == dfaTable[i - 1].transitions.end()) {
                d.transitions[ch] = -1;
                continue;
            }
            int next_state = dfa2numberMap[dfaTable[i - 1].transitions[ch]];
            d.transitions[ch] = dfaMinMap[next_state];
        }
        dfaMinTable.push_back(d);
    }

    std::cout << "Minimized DFA:\n";
    for (const auto& node : dfaMinTable) {
        std::cout << "State " << node.id << ":\n";
        std::cout << "Flag: " << node.flag << '\n';
        for (const auto& [ch, next] : node.transitions) {
            std::cout << ch << " -> " << next << '\n';
        }
    }
}

/*============================ Code Generation ==============================*/

void generateLexerCode() {
    std::ostringstream codeStream;
    codeStream << "#include <iostream>\n";
    codeStream << "#include <string>\n\n";
    codeStream << "using namespace std;\n\n";
    codeStream << "int main() {\n";
    codeStream << "    string input;\n";
    codeStream << "    cout << \"Enter input string: \";\n";
    codeStream << "    cin >> input;\n";
    codeStream << "    int currentState = 0;\n";
    codeStream << "    int length = input.length();\n";
    codeStream << "    for (int i = 0; i < length; i++) {\n";
    codeStream << "        char c = input[i];\n";
    codeStream << "        switch (currentState) {\n";

    for (const auto& node : dfaMinTable) {
        codeStream << "            case " << node.id << ":\n";
        codeStream << "                switch (c) {\n";
        for (const auto& [ch, next] : node.transitions) {
            codeStream << "                    case '" << ch << "':\n";
            if (next == -1) {
                codeStream << "                        cout << \"Error: Invalid input character '\" << c << \"'\" << endl;\n";
                codeStream << "                        return 1;\n";
            } else {
                codeStream << "                        currentState = " << next << ";\n";
            }
            codeStream << "                        break;\n";
        }
        codeStream << "                    default:\n";
        codeStream << "                        cout << \"Error: Invalid input character '\" << c << \"'\" << endl;\n";
        codeStream << "                        return 1;\n";
        codeStream << "                }\n";
        codeStream << "                break;\n";
    }

    codeStream << "        }\n";
    codeStream << "    }\n";
    codeStream << "    switch (currentState) {\n";

    for (const auto& node : dfaMinTable) {
        if (node.flag.find('+') != std::string::npos) {
            codeStream << "        case " << node.id << ":\n";
            codeStream << "            cout << \"Accepted\" << endl;\n";
            codeStream << "            break;\n";
        }
    }

    codeStream << "        default:\n";
    codeStream << "            cout << \"Not Accepted\" << endl;\n";
    codeStream << "    }\n";
    codeStream << "    return 0;\n";
    codeStream << "}\n";

    resultCode = codeStream.str();
}

/*============================ Main Program ==============================*/

void processRegex(const std::string& inputFilePath, const std::string& nfaOutputPath, const std::string& dfaOutputPath, const std::string& minDfaOutputPath, const std::string& codeOutputPath) {
    // Read input regex
    std::ifstream inputFile(inputFilePath);
    if (!inputFile) {
        std::cerr << "Error: Cannot open input file " << inputFilePath << '\n';
        std::exit(1);
    }
    std::stringstream buffer;
    buffer << inputFile.rdbuf();
    std::string regex = buffer.str();
    inputFile.close();

    // Clear global variables
    nodeCount = 0;
    nfaCharSet.clear();
    dfaCharSet.clear();
    statusTable.clear();
    insertionOrder.clear();
    startNFAstatus.clear();
    endNFAstatus.clear();
    dfaStatusSet.clear();
    dfaEndStatusSet.clear();
    dfaNotEndStatusSet.clear();
    dfaMinTable.clear();
    divideVector.clear();
    dfaMinMap.clear();
    dfaTable.clear();
    resultCode.clear();
    nfaCharSet.insert(EPSILON);

    // Process regex
    regex = handleMoreLine(regex);
    regex = handleRegex(regex);
    std::cout << "Processed regex: " << regex << '\n';

    // Convert to NFA
    NFA nfa = regex2NFA(regex);

    // Convert to DFA
    NFA2DFA(nfa);

    // Minimize DFA
    DFAminimize();

    // Generate lexer code
    generateLexerCode();

    // Output NFA table
    if (!nfaOutputPath.empty()) {
        std::ofstream out(nfaOutputPath);
        if (!out) {
            std::cerr << "Error: Cannot open NFA output file " << nfaOutputPath << '\n';
            return;
        }
        out << "NFA Status Table:\n";
        out << "Flag\tID";
        for (char ch : nfaCharSet) out << "\t" << ch;
        out << '\n';
        for (int id : insertionOrder) {
            const auto& node = statusTable[id];
            out << node.flag << "\t" << node.id;
            for (char ch : nfaCharSet) {
                auto it = node.m.find(ch);
                out << "\t" << (it != node.m.end() ? set2string(it->second) : "");
            }
            out << '\n';
        }
        out.close();
        std::cout << "NFA table written to " << nfaOutputPath << '\n';
    }

    // Output DFA table
    if (!dfaOutputPath.empty()) {
        std::ofstream out(dfaOutputPath);
        if (!out) {
            std::cerr << "Error: Cannot open DFA output file " << dfaOutputPath << '\n';
            return;
        }
        out << "DFA Status Table:\n";
        out << "Flag\tStates";
        for (char ch : dfaCharSet) out << "\t" << ch;
        out << '\n';
        for (const auto& node : dfaTable) {
            out << node.flag << "\t{" << set2string(node.nfaStates) << "}";
            for (char ch : dfaCharSet) {
                auto it = node.transitions.find(ch);
                out << "\t" << (it != node.transitions.end() ? "{" + set2string(it->second) + "}" : "");
            }
            out << '\n';
        }
        out.close();
        std::cout << "DFA table written to " << dfaOutputPath << '\n';
    }

    // Output minimized DFA table
    if (!minDfaOutputPath.empty()) {
        std::ofstream out(minDfaOutputPath);
        if (!out) {
            std::cerr << "Error: Cannot open minimized DFA output file " << minDfaOutputPath << '\n';
            return;
        }
        out << "Minimized DFA Table:\n";
        out << "Flag\tID";
        for (char ch : dfaCharSet) out << "\t" << ch;
        out << '\n';
        for (const auto& node : dfaMinTable) {
            out << node.flag << "\t" << node.id;
            for (char ch : dfaCharSet) {
                auto it = node.transitions.find(ch);
                out << "\t" << (it != node.transitions.end() && it->second != -1 ? std::to_string(it->second) : "");
            }
            out << '\n';
        }
        out.close();
        std::cout << "Minimized DFA table written to " << minDfaOutputPath << '\n';
    }

    // Output generated code
    if (!codeOutputPath.empty()) {
        std::ofstream out(codeOutputPath);
        if (!out) {
            std::cerr << "Error: Cannot open code output file " << codeOutputPath << '\n';
            return;
        }
        out << resultCode;
        out.close();
        std::cout << "Generated code written to " << codeOutputPath << '\n';
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_regex_file> [nfa_output_file] [dfa_output_file] [min_dfa_output_file] [code_output_file]\n";
        return 1;
    }

    std::string inputFilePath = argv[1];
    std::string nfaOutputPath = (argc > 2) ? argv[2] : "";
    std::string dfaOutputPath = (argc > 3) ? argv[3] : "";
    std::string minDfaOutputPath = (argc > 4) ? argv[4] : "";
    std::string codeOutputPath = (argc > 5) ? argv[5] : "";

    processRegex(inputFilePath, nfaOutputPath, dfaOutputPath, minDfaOutputPath, codeOutputPath);
    return 0;
}