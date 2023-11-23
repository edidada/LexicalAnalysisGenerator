#include "widget.h"
#include "ui_widget.h"
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QFileDialog>
#include <QTextCodec>
#include <QMessageBox>
#include <iostream>
#include <map>
#include <vector>
#include <stack>
#include <unordered_map>
#include <queue>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#pragma execution_character_set("utf-8")
using namespace std;

// 全局结点计数器
int nodeCount = 0;

const char EPSILON = '#';

// 全局字符统计
set<char> nfaCharSet;
set<char> dfaCharSet;


Widget::Widget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

/*============================共用函数==================================*/

// set转string
string set2string(set<int> s)
{
    string result;

    for (int i : s) {
        result.append(to_string(i));
        result.append(",");
    }

    if (result.size() != 0)
        result.pop_back(); //弹出最后一个逗号

    return result;
}

/*============================正则表达式前置处理==================================*/

// 判断是不是字符
bool isChar(char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return true;
    return false;
}

// 处理正则表达式（加.符号，方便后续操作）
QString handleRegex(QString regex)
{
    string regexStd = regex.toStdString();
    qDebug() << "enter handleRegex: " << regex;

    // 处理中括号
    string result;
    bool insideBrackets = false;
    string currentString;

    for (char c : regexStd) {
        if (c == '[') {
            insideBrackets = true;
            currentString.push_back('(');
        }
        else if (c == ']') {
            insideBrackets = false;
            currentString.push_back(')');
            result += currentString;
            currentString.clear();
        }
        else if (insideBrackets) {
            if (currentString.length() > 1) {
                currentString.push_back('|');
            }
            currentString.push_back(c);
        }
        else {
            result.push_back(c);
        }
    }

    regexStd = result;

    //先处理+号
    for (int i = 0; i < regexStd.size(); i++)
    {
        if (regexStd[i] == '+')
        {
            int kcount = 0;
            int j = i;
            do
            {
                j--;
                if (regexStd[j] == ')')
                {
                    kcount++;
                }
                else if (regexStd[j] == '(')
                {
                    kcount--;
                }
            } while (kcount != 0);
            string str1 = regexStd.substr(0, j);
            string kstr = regexStd.substr(j, i - j);
            string str2 = regexStd.substr(i + 1, (regexStd.size() - i));
            regexStd = str1 + kstr + kstr + "*" + str2;
        }
    }

    for (int i = 0; i < regexStd.size() - 1; i++)
    {
        if (isChar(regexStd[i]) && isChar(regexStd[i + 1])
            || isChar(regexStd[i]) && regexStd[i + 1] == '('
            || regexStd[i] == ')' && isChar(regexStd[i + 1])
            || regexStd[i] == ')' && regexStd[i + 1] == '('
            || regexStd[i] == '*' && regexStd[i + 1] != ')' && regexStd[i + 1] != '|' && regexStd[i + 1] != '?'
            || regexStd[i] == '?' && regexStd[i + 1] != ')'
            || regexStd[i] == '+' && regexStd[i + 1] != ')')
        {
            string str1 = regexStd.substr(0, i + 1);
            string str2 = regexStd.substr(i + 1, (regexStd.size() - i));
            str1 += ".";
            regexStd = str1 + str2;
        }

    }
    return QString::fromStdString(regexStd);
}

// 多行处理
QString handleMoreLine(QString regex)
{
    string regexStd = regex.toStdString();
    vector<std::string> lines;
    istringstream iss(regexStd);
    string line;

    // 防止中间有换行符
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

    return QString::fromStdString(output);

}

/*============================正则表达式转NFA==================================*/

struct nfaNode; // 声明一下，不然报错

// 定义NFA图的边
struct nfaEdge
{
    char c;
    nfaNode* next;
};

// 定义一个nfa图的结点
struct nfaNode
{
    int id; // 结点唯一编号
    bool isStart;   // 初态标识
    bool isEnd; // 终态标识
    vector<nfaEdge> edges;  // 边，用vector因为有可能一个结点有多条边可走
    nfaNode() {
        id = nodeCount++;
        isStart = false;
        isEnd = false;
    }
};

// 定义一个NFA图，我们只要一个起始和终止就可以了
struct NFA
{
    nfaNode* start;
    nfaNode* end;
    NFA() {}
    NFA(nfaNode* s, nfaNode* e)
    {
        start = s;
        end = e;
    }
};

// 创建基本字符NFA，即只包含一个字符的NFA图
NFA CreateBasicNFA(char character) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();

    start->isStart = true;
    end->isEnd = true;

    nfaEdge edge;
    edge.c = character;
    edge.next = end;
    start->edges.push_back(edge);

    NFA nfa(start, end);

    // 存入全局nfa字符set
    nfaCharSet.insert(character);
    // 存入全局dfa字符set
    dfaCharSet.insert(character);

    return nfa;
}

// 创建连接（.）运算符的NFA图
NFA CreateConcatenationNFA(NFA nfa1, NFA nfa2) {
    // 把nfa1的终止状态与nfa2的起始状态连接起来
    nfa1.end->isEnd = false;
    nfa2.start->isStart = false;

    nfaEdge edge;
    edge.c = EPSILON; // 这里用EPSILON表示空边
    edge.next = nfa2.start;
    nfa1.end->edges.push_back(edge);

    NFA nfa;
    nfa.start = nfa1.start;
    nfa.end = nfa2.end;

    return nfa;
}

// 创建选择（|）运算符的NFA图
NFA CreateUnionNFA(NFA nfa1, NFA nfa2) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();

    start->isStart = true;
    end->isEnd = true;

    // 把新的初态与nfa1和nfa2的初态连接起来
    nfaEdge edge1;
    edge1.c = EPSILON;
    edge1.next = nfa1.start;
    start->edges.push_back(edge1);
    nfa1.start->isStart = false;    // 初态结束

    nfaEdge edge2;
    edge2.c = EPSILON;
    edge2.next = nfa2.start;
    start->edges.push_back(edge2);
    nfa2.start->isStart = false;    // 初态结束

    // 把nfa1和nfa2的终止状态与新的终止状态连接起来
    nfa1.end->isEnd = false;
    nfa2.end->isEnd = false;

    nfaEdge edge3;
    edge3.c = EPSILON;
    edge3.next = end;
    nfa1.end->edges.push_back(edge3);

    nfaEdge edge4;
    edge4.c = EPSILON;
    edge4.next = end;
    nfa2.end->edges.push_back(edge4);

    NFA nfa{ start , end };

    return nfa;
}

// 创建零次或多次（*）运算符的NFA图
NFA CreateZeroOrMoreNFA(NFA nfa1) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();

    start->isStart = true;
    end->isEnd = true;

    // 把新的初态与nfa1的初态连接起来
    nfaEdge edge1;
    edge1.c = EPSILON;
    edge1.next = nfa1.start;
    start->edges.push_back(edge1);
    nfa1.start->isStart = false;    // 初态结束

    // 把新的初态与新的终止状态连接起来
    nfaEdge edge2;
    edge2.c = EPSILON;
    edge2.next = end;
    start->edges.push_back(edge2);

    // 把nfa1的终止状态与新的终止状态连接起来
    nfa1.end->isEnd = false;

    nfaEdge edge3;
    edge3.c = EPSILON;
    edge3.next = nfa1.start;
    nfa1.end->edges.push_back(edge3);

    nfaEdge edge4;
    edge4.c = EPSILON;
    edge4.next = end;
    nfa1.end->edges.push_back(edge4);

    NFA nfa{ start,end };

    return nfa;
}

// 创建零次或一次（?）运算符的NFA图
NFA CreateOptionalNFA(NFA nfa1) {
    nfaNode* start = new nfaNode();
    nfaNode* end = new nfaNode();

    start->isStart = true;
    end->isEnd = true;

    // 把新的初态与nfa1的初态连接起来
    nfaEdge edge1;
    edge1.c = EPSILON;
    edge1.next = nfa1.start;
    start->edges.push_back(edge1);
    nfa1.start->isStart = false;    // 初态结束

    // 把新的初态与新的终止状态连接起来
    nfaEdge edge2;
    edge2.c = EPSILON;
    edge2.next = end;
    start->edges.push_back(edge2);

    // 把nfa1的终止状态与新的终止状态连接起来
    nfa1.end->isEnd = false;

    nfaEdge edge3;
    edge3.c = EPSILON;
    edge3.next = end;
    nfa1.end->edges.push_back(edge3);

    NFA nfa(start, end);

    return nfa;
}

// 优先级判断
int Precedence(char op) {
    if (op == '|') {
        return 1;  // 选择运算符 "|" 的优先级
    }
    else if (op == '.') {
        return 2;  // 连接运算符 "." 的优先级
    }
    else if (op == '*' || op == '?') {
        return 3;  // 闭包运算符 "*"和 "?" 的优先级
    }
    else {
        return 0;  // 默认情况下，将其它字符的优先级设为0
    }
}

struct statusTableNode
{
    string flag;  // 标记初态还是终态
    int id; // 唯一id值
    map<char, set<int>> m;  // 对应字符能到达的状态
    statusTableNode()
    {
        flag = ""; // 默认为空
    }
};

// 全局存储状态转换表
unordered_map<int, statusTableNode> statusTable;
// statusTable插入顺序记录，方便后续输出
vector<int> insertionOrder;
set<int> startNFAstatus;
set<int> endNFAstatus;

// 对NFA图进行DFS，形成状态转换表
void createNFAStatusTable(NFA& nfa)
{
    stack<nfaNode*> nfaStack;
    set<nfaNode*> visitedNodes;

    // 初态
    nfaNode* startNode = nfa.start;
    statusTableNode startStatusNode;
    startStatusNode.flag = '-'; // -表示初态
    startStatusNode.id = startNode->id;
    statusTable[startNode->id] = startStatusNode;
    insertionOrder.push_back(startNode->id);
    startNFAstatus.insert(startNode->id);

    nfaStack.push(startNode);

    while (!nfaStack.empty()) {
        nfaNode* currentNode = nfaStack.top();
        nfaStack.pop();
        visitedNodes.insert(currentNode);

        for (nfaEdge edge : currentNode->edges) {
            char transitionChar = edge.c;
            nfaNode* nextNode = edge.next;

            // 记录状态转换信息
            statusTable[currentNode->id].m[transitionChar].insert(nextNode->id);

            // 如果下一个状态未被访问，将其加入堆栈
            if (visitedNodes.find(nextNode) == visitedNodes.end()) {
                nfaStack.push(nextNode);

                // 记录状态信息
                statusTableNode nextStatus;
                nextStatus.id = nextNode->id;
                if (nextNode->isStart) {
                    nextStatus.flag += '-'; // -表示初态
                    startNFAstatus.insert(nextStatus.id);
                }
                else if (nextNode->isEnd) {
                    nextStatus.flag += '+'; // +表示终态
                    endNFAstatus.insert(nextStatus.id);
                }
                statusTable[nextNode->id] = nextStatus;
                // 记录插入顺序（排除终态）
                if (!nextNode->isEnd)
                {
                    insertionOrder.push_back(nextNode->id);
                }
            }
        }
    }

    // 顺序表才插入终态
    nfaNode* endNode = nfa.end;
    insertionOrder.push_back(endNode->id);
}

// 测试输出NFA状态转换表程序（debug使用）
void printStatusTable() {
    // 打印状态表按照插入顺序
    for (int id : insertionOrder) {
        const statusTableNode& node = statusTable[id];
        qDebug() << "Node ID: " << node.id << ", Flag: " << QString::fromStdString(node.flag) << "\n";

        for (const auto& entry : node.m) {
            char transitionChar = entry.first;
            const std::set<int>& targetStates = entry.second;

            qDebug() << "  Transition: " << transitionChar << " -> {";
            for (int targetState : targetStates) {
                qDebug() << targetState << " ";
            }
            qDebug() << "}\n";
        }
    }
}


// 正则表达式转NFA核心算法
NFA regex2NFA(string regex)
{
    // 双栈法，创建两个栈opStack（运算符栈）,nfaStack（nfa图栈）
    stack<char> opStack;
    stack<NFA> nfaStack;

    // 对表达式进行类似于逆波兰表达式处理
    // 运算符：| .（） ？ +  *
    for (char c : regex)
    {
        switch (c)
        {
        case ' ': // 空格跳过
            break;
        case '(':
            opStack.push(c);
            break;
        case ')':
            while (!opStack.empty() && opStack.top() != '(')
            {
                // 处理栈顶运算符，构建NFA图，并将结果入栈
                char op = opStack.top();
                opStack.pop();

                if (op == '|') {
                    // 处理并构建"|"运算符
                    NFA nfa2 = nfaStack.top();
                    nfaStack.pop();
                    NFA nfa1 = nfaStack.top();
                    nfaStack.pop();

                    // 创建新的NFA，表示nfa1 | nfa2
                    NFA resultNFA = CreateUnionNFA(nfa1, nfa2);
                    nfaStack.push(resultNFA);
                }
                else if (op == '.') {
                    // 处理并构建"."运算符
                    NFA nfa2 = nfaStack.top();
                    nfaStack.pop();
                    NFA nfa1 = nfaStack.top();
                    nfaStack.pop();

                    // 创建新的NFA，表示nfa1 . nfa2
                    NFA resultNFA = CreateConcatenationNFA(nfa1, nfa2);
                    nfaStack.push(resultNFA);
                }
            }
            if (opStack.empty())
            {
                qDebug() << "括号未闭合，请检查正则表达式！";
                exit(-1);
            }
            else
            {
                opStack.pop(); // 弹出(
            }
            break;
        case '|':
        case '.':
            // 处理运算符 | 和 .
            while (!opStack.empty() && (opStack.top() == '|' || opStack.top() == '.') &&
                Precedence(opStack.top()) >= Precedence(c))
            {
                char op = opStack.top();
                opStack.pop();

                // 处理栈顶运算符，构建NFA图，并将结果入栈
                if (op == '|') {
                    // 处理并构建"|"运算符
                    NFA nfa2 = nfaStack.top();
                    nfaStack.pop();
                    NFA nfa1 = nfaStack.top();
                    nfaStack.pop();

                    // 创建新的NFA，表示nfa1 | nfa2
                    NFA resultNFA = CreateUnionNFA(nfa1, nfa2);
                    nfaStack.push(resultNFA);
                }
                else if (op == '.') {
                    // 处理并构建"."运算符
                    NFA nfa2 = nfaStack.top();
                    nfaStack.pop();
                    NFA nfa1 = nfaStack.top();
                    nfaStack.pop();

                    // 创建新的 NFA，表示 nfa1 . nfa2
                    NFA resultNFA = CreateConcatenationNFA(nfa1, nfa2);
                    nfaStack.push(resultNFA);
                }
            }
            opStack.push(c);
            break;
        case '?':
        case '*':
            // 处理闭包运算符 ? + *
            // 从nfaStack弹出NFA，应用相应的闭包操作，并将结果入栈
            if (!nfaStack.empty()) {
                NFA nfa = nfaStack.top();
                nfaStack.pop();
                if (c == '?') {
                    // 处理 ?
                    NFA resultNFA = CreateOptionalNFA(nfa);
                    nfaStack.push(resultNFA);
                }
                else if (c == '*') {
                    // 处理 *
                    NFA resultNFA = CreateZeroOrMoreNFA(nfa);
                    nfaStack.push(resultNFA);
                }
            }
            else {
                qDebug() << "正则表达式语法错误：闭包操作没有NFA可用！";
                exit(-1);
            }
            break;
        default:
            // 处理字母字符
            NFA nfa = CreateBasicNFA(c); // 创建基本的字符NFA
            nfaStack.push(nfa);
            break;
        }

    }

    // 处理栈中剩余的运算符
    while (!opStack.empty())
    {
        char op = opStack.top();
        opStack.pop();

        if (op == '|' || op == '.')
        {
            // 处理并构建运算符 | 和 .
            if (nfaStack.size() < 2)
            {
                qDebug() << "正则表达式语法错误：不足以处理运算符 " << op << "！";
                exit(-1);
            }

            NFA nfa2 = nfaStack.top();
            nfaStack.pop();
            NFA nfa1 = nfaStack.top();
            nfaStack.pop();

            if (op == '|')
            {
                // 创建新的 NFA，表示 nfa1 | nfa2
                NFA resultNFA = CreateUnionNFA(nfa1, nfa2);
                nfaStack.push(resultNFA);
            }
            else if (op == '.')
            {
                // 创建新的 NFA，表示 nfa1 . nfa2
                NFA resultNFA = CreateConcatenationNFA(nfa1, nfa2);
                nfaStack.push(resultNFA);
            }
        }
        else
        {
            qDebug() << "正则表达式语法错误：未知的运算符 " << op << "！";
            exit(-1);
        }
    }

    // 最终的NFA图在nfaStack的顶部
    NFA result = nfaStack.top();
    qDebug() << "NFA图构建完毕" << endl;

    createNFAStatusTable(result);
    qDebug() << "状态转换表构建完毕" << endl;

    return result;
}

/*============================NFA转DFA==================================*/
//map<int, nfaNode> nfaStates;

// dfa节点
struct dfaNode
{
    string flag; // 是否包含终态（+）或初态（-）
    set<int> nfaStates; // 该DFA状态包含的NFA状态的集合
    map<char, set<int>> transitions; // 字符到下一状态的映射
    dfaNode() {
        flag = "";
    }
};
// dfa状态去重集
set<set<int>> dfaStatusSet;

// dfa最终结果
vector<dfaNode> dfaTable;

//下面用于DFA最小化
// dfa终态集合
set<int> dfaEndStatusSet;
// dfa非终态集合
set<int> dfaNotEndStatusSet;
// set对应序号MAP
map<set<int>, int> dfa2numberMap;
int startStaus;

// 判断是否含有初态终态，含有则返回对应字符串
string setHasStartOrEnd(set<int>& statusSet)
{
    string result = "";
    for (const int& element : startNFAstatus) {
        if (statusSet.count(element) > 0) {
            result += "-";
        }
    }

    for (const int& element : endNFAstatus) {
        if (statusSet.count(element) > 0) {
            result += "+";
        }
    }

    return result;
}

set<int> epsilonClosure(int id)
{
    set<int> eResult{ id };
    stack<int> stack;
    stack.push(id);

    while (!stack.empty())
    {
        int current = stack.top();
        stack.pop();

        set<int> eClosure = statusTable[current].m[EPSILON];
        for (auto t : eClosure)
        {
            if (eResult.find(t) == eResult.end())
            {
                eResult.insert(t);
                stack.push(t);
            }
        }
    }

    return eResult;
}

set<int> otherCharClosure(int id, char ch)
{
    set<int> otherResult{};
    set<int> processed;
    stack<int> stack;
    stack.push(id);

    while (!stack.empty())
    {
        int current = stack.top();
        stack.pop();

        if (processed.find(current) != processed.end())
            continue;

        processed.insert(current);

        set<int> otherClosure = statusTable[current].m[ch];
        for (auto o : otherClosure)
        {
            auto tmp = epsilonClosure(o);
            otherResult.insert(tmp.begin(), tmp.end());
            stack.push(o);
        }
    }

    return otherResult;
}



// DFA debug输出函数
void printDfaTable(const vector<dfaNode>& dfaTable) {
    for (size_t i = 0; i < dfaTable.size(); ++i) {
        qDebug() << "DFA Node " << i << " - Flag: " << QString::fromStdString(dfaTable[i].flag);
        qDebug() << "NFA States: ";
        for (const int state : dfaTable[i].nfaStates) {
            qDebug() << state;
        }
        qDebug() << "Transitions: ";
        for (const auto& transition : dfaTable[i].transitions) {
            qDebug() << "  Input: " << transition.first;
            qDebug() << "  Next States: ";
            for (const int nextState : transition.second) {
                qDebug() << nextState;
            }
        }
        qDebug() << "---------------------";
    }
}

void NFA2DFA(NFA& nfa)
{
    int dfaStatusCount = 1;
    auto start = nfa.start; // 获得NFA图的起始位置
    auto startId = start->id;   // 获得起始编号
    dfaNode startDFANode;
    startDFANode.nfaStates = epsilonClosure(startId); // 初始闭包
    startDFANode.flag = setHasStartOrEnd(startDFANode.nfaStates); // 判断初态终态
    deque<set<int>> newStatus{};
    dfa2numberMap[startDFANode.nfaStates] = dfaStatusCount;
    startStaus = dfaStatusCount;
    if (setHasStartOrEnd(startDFANode.nfaStates).find("+") != string::npos) {
        dfaEndStatusSet.insert(dfaStatusCount++);
    }
    else
    {
        dfaNotEndStatusSet.insert(dfaStatusCount++);
    }
    // 对每个字符进行遍历
    for (auto ch : dfaCharSet)
    {
        set<int> thisChClosure{};
        for (auto c : startDFANode.nfaStates)
        {
            set<int> tmp = otherCharClosure(c, ch);
            thisChClosure.insert(tmp.begin(), tmp.end());
        }
        if (thisChClosure.empty())  // 如果这个闭包是空集没必要继续下去了
        {
            continue;
        }
        int presize = dfaStatusSet.size();
        dfaStatusSet.insert(thisChClosure);
        int lastsize = dfaStatusSet.size();
        // 不管一不一样都是该节点这个字符的状态
        startDFANode.transitions[ch] = thisChClosure;
        // 如果大小不一样，证明是新状态
        if (lastsize > presize)
        {
            dfa2numberMap[thisChClosure] = dfaStatusCount;
            newStatus.push_back(thisChClosure);
            if (setHasStartOrEnd(thisChClosure).find("+") != string::npos) {
                dfaEndStatusSet.insert(dfaStatusCount++);
            }
            else
            {
                dfaNotEndStatusSet.insert(dfaStatusCount++);
            }

        }

    }
    dfaTable.push_back(startDFANode);

    // 对后面的新状态进行不停遍历
    while (!newStatus.empty())
    {
        // 拿出一个新状态
        set<int> ns = newStatus.front();
        newStatus.pop_front();
        dfaNode DFANode;
        DFANode.nfaStates = ns;  // 该节点状态集合
        DFANode.flag = setHasStartOrEnd(ns);

        for (auto ch : dfaCharSet)
        {

            set<int> thisChClosure{};
            for (auto c : ns)
            {
                set<int> tmp = otherCharClosure(c, ch);
                thisChClosure.insert(tmp.begin(), tmp.end());
            }
            if (thisChClosure.empty())  // 如果这个闭包是空集没必要继续下去了
            {
                continue;
            }
            int presize = dfaStatusSet.size();
            dfaStatusSet.insert(thisChClosure);
            int lastsize = dfaStatusSet.size();
            // 不管一不一样都是该节点这个字符的状态
            DFANode.transitions[ch] = thisChClosure;
            // 如果大小不一样，证明是新状态
            if (lastsize > presize)
            {
                dfa2numberMap[thisChClosure] = dfaStatusCount;
                newStatus.push_back(thisChClosure);
                if (setHasStartOrEnd(thisChClosure).find("+") != string::npos) {
                    dfaEndStatusSet.insert(dfaStatusCount++);
                }
                else
                {
                    dfaNotEndStatusSet.insert(dfaStatusCount++);
                }

            }

        }
        dfaTable.push_back(DFANode);

    }

    // dfa debug
    // printDfaTable(dfaTable);
}

/*============================DFA最小化==================================*/
/*
// dfa终态集合
set<int> dfaEndStatusSet;
// dfa非终态集合
set<int> dfaNotEndStatusSet;
// set对应序号MAP
map<set<int>, int> dfa2numberMap;
*/

// 判断是否含有初态终态，含有则返回对应字符串
string minSetHasStartOrEnd(set<int>& statusSet)
{
    string result = "";
    if (statusSet.count(startStaus) > 0) {
        result += "-";
    }

    for (const int& element : dfaEndStatusSet) {
        if (statusSet.count(element) > 0) {
            result += "+";
            break;  // 可能会有多个终态同时包含，我们只要一个
        }
    }

    return result;
}

// dfa最小化节点
struct dfaMinNode
{
    string flag; // 是否包含终态（+）或初态（-）
    int id;
    map<char, int> transitions; // 字符到下一状态的映射
    dfaMinNode() {
        flag = "";
    }
};

vector<dfaMinNode> dfaMinTable;

// 用于分割集合
vector<set<int>> divideVector;
// 存下标
map<int, int> dfaMinMap;


// 根据字符 ch 将状态集合 node 分成两个子集合
void splitSet(int i, char ch)
{
    set<int> result;
    auto& node = divideVector[i];
    int s = -2;

    for (auto state : node)
    {
        int thisNum;
        if (dfaTable[state - 1].transitions.find(ch) == dfaTable[state - 1].transitions.end())
        {
            thisNum = -1; // 空集
        }
        else
        {
            // 根据字符 ch 找到下一个状态
            int next_state = dfa2numberMap[dfaTable[state - 1].transitions[ch]];
            thisNum = dfaMinMap[next_state];    // 这个状态的下标是多少
        }

        if (s == -2)    // 初始下标
        {
            s = thisNum;
        }
        else if (thisNum != s)   // 如果下标不同，就是有问题，需要分出来
        {
            result.insert(state);
        }
    }

    // 删除要删除的元素
    for (int state : result) {
        node.erase(state);
    }

    // 都遍历完了，如果result不是空，证明有新的，加入vector中
    if (!result.empty())
    {
        divideVector.push_back(result);
        // 同时更新下标
        for (auto a : result)
        {
            dfaMinMap[a] = divideVector.size() - 1;
        }
    }

}

void DFAminimize()
{
    divideVector.clear();
    dfaMinMap.clear();

    // 存入非终态、终态集合
    if (dfaNotEndStatusSet.size() != 0)
    {
        divideVector.push_back(dfaNotEndStatusSet);
    }
    // 初始化map
    for (auto t : dfaNotEndStatusSet)
    {
        dfaMinMap[t] = divideVector.size() - 1;
    }

    divideVector.push_back(dfaEndStatusSet);

    for (auto t : dfaEndStatusSet)
    {
        dfaMinMap[t] = divideVector.size() - 1;
    }

    // 当flag为1时，一直循环
    int continueFlag = 1;

    while (continueFlag)
    {
        continueFlag = 0;
        int size1 = divideVector.size();

        for (int i = 0; i < size1; i++)
        {

            // 逐个字符尝试分割状态集合
            for (char ch : dfaCharSet)
            {
                splitSet(i, ch);
            }
        }
        int size2 = divideVector.size();
        if (size2 > size1)
        {
            continueFlag = 1;
        }
    }

    for (int dfaMinCount = 0; dfaMinCount < divideVector.size(); dfaMinCount++)
    {
        auto& v = divideVector[dfaMinCount];
        dfaMinNode d;
        d.flag = minSetHasStartOrEnd(v);
        d.id = dfaMinCount;
        // 逐个字符
        for (char ch : dfaCharSet)
        {
            if (v.size() == 0)
            {
                d.transitions[ch] = -1;   // 空集特殊判断
                continue;
            }
            int i = *(v.begin()); // 拿一个出来
            if (dfaTable[i - 1].transitions.find(ch) == dfaTable[i - 1].transitions.end())
            {
                d.transitions[ch] = -1;   // 空集特殊判断
                continue;
            }
            int next_state = dfa2numberMap[dfaTable[i - 1].transitions[ch]];
            int thisNum = dfaMinMap[next_state];    // 这个状态下标
            d.transitions[ch] = thisNum;
        }
        dfaMinTable.push_back(d);
    }

    // 输出 dfaMinTable
    for (const dfaMinNode& node : dfaMinTable) {
        qDebug() << "State " << node.id << ":";
        qDebug() << "Flag: " << QString::fromStdString(node.flag);

        for (const auto& entry : node.transitions) {
            qDebug() << entry.first << " -> " << entry.second;
        }
    }

    qDebug() << "DFA最小化完成！";
}

/*============================C++代码生成==================================*/

string resultCode;

// 生成词法分析器代码并返回为字符串
void generateLexerCode() {
    ostringstream codeStream; // Create a stringstream to capture the output

    codeStream << "#include <iostream>" << endl;
    codeStream << "#include <string>" << endl;
    codeStream << endl;
    codeStream << "using namespace std;" << endl;
    codeStream << endl;
    codeStream << "int main() {" << endl;
    codeStream << "    string input;" << endl;
    codeStream << "    cout << \"Enter input string: \";" << endl;
    codeStream << "    cin >> input;" << endl;
    codeStream << "    int currentState = 0;" << endl;
    codeStream << "    int length = input.length();" << endl;
    codeStream << "    for (int i = 0; i < length; i++) {" << endl;
    codeStream << "        char c = input[i];" << endl;
    codeStream << "        switch (currentState) {" << endl;

    for (const dfaMinNode& node : dfaMinTable) {
        codeStream << "            case " << node.id << ":" << endl;
        codeStream << "                switch (c) {" << endl;
        for (const auto& transition : node.transitions) {
            codeStream << "                    case '" << transition.first << "':" << endl;
            if (transition.second == -1) {
                codeStream << "                        cout << \"Error: Invalid input character '\" << c << \"'\" << endl;";
                codeStream << "                        return 1;" << endl;
            }
            else {
                codeStream << "                        currentState = " << transition.second << ";" << endl;
            }
            codeStream << "                        break;" << endl;
        }
        codeStream << "                    default:" << endl;
        codeStream << "                        cout << \"Error: Invalid input character '\" << c << \"'\" << endl;" << endl;
        codeStream << "                        return 1;" << endl;
        codeStream << "                }" << endl;
        codeStream << "                break;" << endl;
    }

    codeStream << "        }" << endl;
    codeStream << "    }" << endl;
    codeStream << "    switch (currentState) {" << endl;

    for (const dfaMinNode& node : dfaMinTable) {
        if (node.flag.find("+") != string::npos) {
            codeStream << "        case " << node.id << ":" << endl;
            codeStream << "            cout << \"Accepted\" << endl;" << endl;
            codeStream << "            break;" << endl;
        }
    }

    codeStream << "        default:" << endl;
    codeStream << "            cout << \"Not Accepted\" << endl;" << endl;
    codeStream << "    }" << endl;
    codeStream << "    return 0;" << endl;
    codeStream << "}" << endl;

    resultCode = codeStream.str();
}


/*============================主程序入口==================================*/

// 开始分析按钮
void Widget::on_pushButton_clicked()
{
    // 清空全局变量
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

    nfaCharSet.insert(EPSILON); // 放入epsilon
    QString regex = ui->plainTextEdit_2->toPlainText();   // 拿到正则表达式

    // 多行处理
    regex = handleMoreLine(regex);

    // 前置处理
    regex = handleRegex(regex);
    qDebug() << regex;

    // 转换成string方便后续处理
    string regexStd = regex.toStdString();

    //正则表达式转换成NFA图
    NFA nfa = regex2NFA(regexStd);

    // NFA转DFA
    NFA2DFA(nfa);

    DFAminimize();

    generateLexerCode();

    QMessageBox::about(this, "提示", "分析成功！请点击其余按钮查看结果！");
}


// 生成NFA按钮
void Widget::on_pushButton_4_clicked()
{
    ui->tableWidget->clearContents(); // 清除表格中的数据
    ui->tableWidget->setRowCount(0); // 清除所有行
    ui->tableWidget->setColumnCount(0); // 清除所有列
    // 设置列数
    int n = 2 + nfaCharSet.size(); // 默认两列：Flag 和 ID
    ui->tableWidget->setColumnCount(n);

    // 字符和第X列存起来对应
    map<char, int> headerCharNum;

    // 设置表头
    QStringList headerLabels;
    headerLabels << "标志" << "ID";
    int headerCount = 3;
    for (const auto& ch : nfaCharSet) {
        headerLabels << QString(ch);
        headerCharNum[ch] = headerCount++;
    }
    ui->tableWidget->setHorizontalHeaderLabels(headerLabels);

    // 设置行数
    int rowCount = statusTable.size();
    ui->tableWidget->setRowCount(rowCount);

    // 填充数据
    int row = 0;
    for (auto id : insertionOrder) {
        const statusTableNode& node = statusTable[id];

        // Flag 列
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(node.flag)));

        // ID 列
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(node.id)));

        // TransitionChar 列
        int col = 2;
        for (const auto& transitionEntry : node.m) {
            string resutlt = set2string(transitionEntry.second);

            // 放到指定列数据
            ui->tableWidget->setItem(row, headerCharNum[transitionEntry.first] - 1, new QTableWidgetItem(QString::fromStdString(resutlt)));
            col++;
        }

        row++;
    }

    // 调整列宽
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 显示表格
    ui->tableWidget->show();
}

// 生成DFA按钮
void Widget::on_pushButton_3_clicked()
{
    ui->tableWidget->clearContents(); // 清除表格中的数据
    ui->tableWidget->setRowCount(0); // 清除所有行
    ui->tableWidget->setColumnCount(0); // 清除所有列

    // 设置列数
    int n = 2 + dfaCharSet.size(); // 默认两列：Flag 和 状态集合
    ui->tableWidget->setColumnCount(n);

    // 字符和第X列存起来对应
    map<char, int> headerCharNum;

    // 设置表头
    QStringList headerLabels;
    headerLabels << "标志" << "状态集合";
    int headerCount = 3;
    for (const auto& ch : dfaCharSet) {
        headerLabels << QString(ch);
        headerCharNum[ch] = headerCount++;
    }
    ui->tableWidget->setHorizontalHeaderLabels(headerLabels);

    // 设置行数
    int rowCount = dfaTable.size();
    ui->tableWidget->setRowCount(rowCount);

    // 填充数据
    int row = 0;
    for (auto& dfaNode : dfaTable) {

        // Flag 列
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(dfaNode.flag)));

        // 状态集合 列
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::fromStdString("{" + set2string(dfaNode.nfaStates) + "}")));

        // 状态转换 列
        int col = 2;
        for (const auto& transitionEntry : dfaNode.transitions) {
            string re = set2string(transitionEntry.second);

            // 放到指定列数据
            ui->tableWidget->setItem(row, headerCharNum[transitionEntry.first] - 1, new QTableWidgetItem(QString::fromStdString("{" + re + "}")));
            col++;
        }

        row++;
    }

    // 调整列宽
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 显示表格
    ui->tableWidget->show();
}

// DFA最小化
void Widget::on_pushButton_5_clicked()
{
    ui->tableWidget->clearContents(); // 清除表格中的数据
    ui->tableWidget->setRowCount(0); // 清除所有行
    ui->tableWidget->setColumnCount(0); // 清除所有列

    // 设置列数
    int n = 2 + dfaCharSet.size(); // 默认两列：Flag 和 状态集合
    ui->tableWidget->setColumnCount(n);

    // 字符和第X列存起来对应
    map<char, int> headerCharNum;

    // 设置表头
    QStringList headerLabels;
    headerLabels << "标志" << "状态集合";
    int headerCount = 3;
    for (const auto& ch : dfaCharSet) {
        headerLabels << QString(ch);
        headerCharNum[ch] = headerCount++;
    }
    ui->tableWidget->setHorizontalHeaderLabels(headerLabels);

    // 设置行数
    int rowCount = dfaMinTable.size();
    ui->tableWidget->setRowCount(rowCount);

    // 填充数据
    int row = 0;
    for (auto& dfaNode : dfaMinTable) {

        // Flag 列
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(dfaNode.flag)));

        // 状态集合 列
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(dfaNode.id)));

        // 状态转换 列
        int col = 2;
        for (const auto& transitionEntry : dfaNode.transitions) {
            // 放到指定列数据
            ui->tableWidget->setItem(row, headerCharNum[transitionEntry.first] - 1, new QTableWidgetItem(transitionEntry.second == -1 ? QString::fromStdString("") : QString::number(transitionEntry.second)));
            col++;
        }

        row++;
    }

    // 调整列宽
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 显示表格
    ui->tableWidget->show();
}

// 获取C++程序
void Widget::on_pushButton_2_clicked()
{
    ui->tableWidget->hide();
    ui->plainTextEdit->show();

    ui->plainTextEdit->setPlainText(QString::fromStdString(resultCode));
}

void Widget::on_pushButton_6_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("选择文件"), QDir::homePath(), tr("文本文件 (*.txt);;所有文件 (*.*)"));

    if (!filePath.isEmpty())
    {
        ifstream inputFile;
        QTextCodec* code = QTextCodec::codecForName("GB2312");

        string selectedFile = code->fromUnicode(filePath.toStdString().c_str()).data();
        inputFile.open(selectedFile.c_str(), ios::in);


        //        cout<<filePath.toStdString();
        //        ifstream inputFile(filePath.toStdString());
        if (!inputFile) {
            QMessageBox::critical(this, "错误信息", "导入错误！无法打开文件，请检查路径和文件是否被占用！");
            cerr << "Error opening file." << endl;
        }
        // 读取文件内容并显示在 plainTextEdit_2
        stringstream buffer;
        buffer << inputFile.rdbuf();
        QString fileContents = QString::fromStdString(buffer.str());
        ui->plainTextEdit_2->setPlainText(fileContents);
    }

}

void Widget::on_pushButton_7_clicked()
{
    // 保存结果到文本文件
    QString saveFilePath = QFileDialog::getSaveFileName(this, tr("保存结果文件"), QDir::homePath(), tr("文本文件 (*.txt)"));
    if (!saveFilePath.isEmpty() && !ui->plainTextEdit_2->toPlainText().isEmpty()) {
        QFile outputFile(saveFilePath);
        if (outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&outputFile);
            stream << ui->plainTextEdit_2->toPlainText();
            outputFile.close();
            QMessageBox::about(this, "提示", "导出成功！");
        }
    }
    else if (ui->plainTextEdit_2->toPlainText().isEmpty())
    {
        QMessageBox::warning(this, tr("提示"), tr("输入框为空，请重试！"));
    }
}
