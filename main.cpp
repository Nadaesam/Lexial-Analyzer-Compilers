#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace std;


/*
{ Sample program
  in TINY language
  compute factorial
}

read x; {input an integer}
if 0<x then {compute only if x>=1}
  fact:=1;
  repeat
    fact := fact * x;
    x:=x-1
  until x=0;
  write fact {output factorial}
end
*/

// sequence of statements separated by ;
// no procedures - no declarations
// all variables are integers
// variables are declared simply by assigning values to them :=
// if-statement: if (boolean) then [else] end
// repeat-statement: repeat until (boolean)
// boolean only in if and repeat conditions < = and two mathematical expressions
// math expressions integers only, + - * / ^
// I/O read write
// Comments {}

////////////////////////////////////////////////////////////////////////////////////
// Strings /////////////////////////////////////////////////////////////////////////

bool Equals(const char* a, const char* b)
{
    return strcmp(a, b)==0;
}

bool StartsWith(const char* a, const char* b)
{
    int nb=strlen(b);
    return strncmp(a, b, nb)==0;
}

void Copy(char* a, const char* b, int n=0)
{
    if(n>0)
    {
        strncpy(a, b, n);
        a[n]=0;
    }
    else strcpy(a, b);
}

void AllocateAndCopy(char** a, const char* b)
{
    if(b==0)
    {
        *a=0;
        return;
    }
    int n=strlen(b);
    *a=new char[n+1];
    strcpy(*a, b);
}

////////////////////////////////////////////////////////////////////////////////////
// Input and Output ////////////////////////////////////////////////////////////////

#define MAX_LINE_LENGTH 10000

struct InFile
{
    FILE* file;
    int cur_line_num;

    char line_buf[MAX_LINE_LENGTH];
    int cur_ind, cur_line_size;

    InFile(const char* str)
    {
        file=0;
        if(str) file=fopen(str, "r");
        cur_line_size=0;
        cur_ind=0;
        cur_line_num=0;
    }
    ~InFile()
    {
        if(file) fclose(file);
    }

    void SkipSpaces()
    {
        while(cur_ind<cur_line_size)
        {
            char ch=line_buf[cur_ind];
            if(ch!=' ' && ch!='\t' && ch!='\r' && ch!='\n') break;
            cur_ind++;
        }
    }

    bool SkipUpto(const char* str)
    {
        while(true)
        {
            SkipSpaces();
            while(cur_ind>=cur_line_size)
            {
                if(!GetNewLine()) return false;
                SkipSpaces();
            }

            if(StartsWith(&line_buf[cur_ind], str))
            {
                cur_ind+=strlen(str);
                return true;
            }
            cur_ind++;
        }
        return false;
    }

    bool GetNewLine()
    {
        cur_ind=0;
        line_buf[0]=0;
        if(!fgets(line_buf, MAX_LINE_LENGTH, file)) return false;
        cur_line_size=strlen(line_buf);
        if(cur_line_size==0) return false; // End of file
        cur_line_num++;
        return true;
    }

    char* GetNextTokenStr()
    {
        SkipSpaces();
        while(cur_ind>=cur_line_size)
        {
            if(!GetNewLine()) return 0;
            SkipSpaces();
        }
        return &line_buf[cur_ind];
    }

    void Advance(int num)
    {
        cur_ind+=num;
    }
};

struct OutFile
{
    FILE* file;
    OutFile(const char* str)
    {
        file=0;
        if(str) file=fopen(str, "w");
    }
    ~OutFile()
    {
        if(file) fclose(file);
    }

    void Out(const char* s)
    {
        fprintf(file, "%s\n", s);
        fflush(file);
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Compiler Parameters /////////////////////////////////////////////////////////////

struct CompilerInfo
{
    InFile in_file;
    OutFile out_file;
    OutFile debug_file;

    CompilerInfo(const char* in_str, const char* out_str, const char* debug_str)
        : in_file(in_str), out_file(out_str), debug_file(debug_str)
    {
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Scanner /////////////////////////////////////////////////////////////////////////

#define MAX_TOKEN_LEN 40

enum TokenType
{
    IF, THEN, ELSE, END, REPEAT, UNTIL, READ, WRITE,
    ASSIGN, EQUAL, LESS_THAN,
    PLUS, MINUS, TIMES, DIVIDE, POWER,
    SEMI_COLON,
    LEFT_PAREN, RIGHT_PAREN,
    LEFT_BRACE, RIGHT_BRACE,
    ID, NUM,
    ENDFILE, ERROR
};

// Used for debugging only /////////////////////////////////////////////////////////
const char* TokenTypeStr[]=
{
    "If", "Then", "Else", "End", "Repeat", "Until", "Read", "Write",
    "Assign", "Equal", "LessThan",
    "Plus", "Minus", "Times", "Divide", "Power",
    "SemiColon",
    "LeftParen", "RightParen",
    "LeftBrace", "RightBrace",
    "ID", "Num",
    "EndFile", "Error"
};

struct Token
{
    TokenType type;
    char str[MAX_TOKEN_LEN+1];

    Token()
    {
        str[0]=0;
        type=ERROR;
    }
    Token(TokenType _type, const char* _str)
    {
        type=_type;
        Copy(str, _str);
    }
};

const Token reserved_words[]=
{
    Token(IF, "if"),
    Token(THEN, "then"),
    Token(ELSE, "else"),
    Token(END, "end"),
    Token(REPEAT, "repeat"),
    Token(UNTIL, "until"),
    Token(READ, "read"),
    Token(WRITE, "write")
};
const int num_reserved_words=sizeof(reserved_words)/sizeof(reserved_words[0]);

// if there is tokens like < <=, sort them such that sub-tokens come last: <= <
// the closing comment should come immediately after opening comment
const Token symbolic_tokens[]=
{
    Token(ASSIGN, ":="),
    Token(EQUAL, "="),
    Token(LESS_THAN, "<"),
    Token(PLUS, "+"),
    Token(MINUS, "-"),
    Token(TIMES, "*"),
    Token(DIVIDE, "/"),
    Token(POWER, "^"),
    Token(SEMI_COLON, ";"),
    Token(LEFT_PAREN, "("),
    Token(RIGHT_PAREN, ")"),
    Token(LEFT_BRACE, "{"),
    Token(RIGHT_BRACE, "}")
};
const int num_symbolic_tokens=sizeof(symbolic_tokens)/sizeof(symbolic_tokens[0]);

inline bool IsDigit(char ch)
{
    return (ch>='0' && ch<='9');
}
inline bool IsLetter(char ch)
{
    return ((ch>='a' && ch<='z') || (ch>='A' && ch<='Z'));
}
inline bool IsLetterOrUnderscore(char ch)
{
    return (IsLetter(ch) || ch=='_');
}

void GetNextToken(CompilerInfo* pci, Token* ptoken)
{
    ptoken->type=ERROR;
    ptoken->str[0]=0;

    int i;
    char* s=pci->in_file.GetNextTokenStr();
    if(!s)
    {
        ptoken->type=ENDFILE;
        ptoken->str[0]=0;
        return;
    }

    for(i=0; i<num_symbolic_tokens; i++)
    {
        if(StartsWith(s, symbolic_tokens[i].str))
            break;
    }

    if(i<num_symbolic_tokens)
    {
        if(symbolic_tokens[i].type==LEFT_BRACE)
        {
            pci->in_file.Advance(strlen(symbolic_tokens[i].str));
            if(!pci->in_file.SkipUpto(symbolic_tokens[i+1].str)) return;
            return GetNextToken(pci, ptoken);
        }
        ptoken->type=symbolic_tokens[i].type;
        Copy(ptoken->str, symbolic_tokens[i].str);
    }
    else if(IsDigit(s[0]))
    {
        int j=1;
        while(IsDigit(s[j])) j++;

        ptoken->type=NUM;
        Copy(ptoken->str, s, j);
    }
    else if(IsLetterOrUnderscore(s[0]))
    {
        int j=1;
        while(IsLetterOrUnderscore(s[j])) j++;

        ptoken->type=ID;
        Copy(ptoken->str, s, j);

        for(i=0; i<num_reserved_words; i++)
        {
            if(Equals(ptoken->str, reserved_words[i].str))
            {
                ptoken->type=reserved_words[i].type;
                break;
            }
        }
    }

    int len=strlen(ptoken->str);
    if(len>0) pci->in_file.Advance(len);
}
////////////////////////////////////////////////////////////////////////////////////
// Parser //////////////////////////////////////////////////////////////////////////

// program -> stmtseq
// stmtseq -> stmt { ; stmt }
// stmt -> ifstmt | repeatstmt | assignstmt | readstmt | writestmt
// ifstmt -> if exp then stmtseq [ else stmtseq ] end
// repeatstmt -> repeat stmtseq until expr
// assignstmt -> identifier := expr
// readstmt -> read identifier
// writestmt -> write expr
// expr -> mathexpr [ (<|=) mathexpr ]
// mathexpr -> term { (+|-) term }    left associative
// term -> factor { (*|/) factor }    left associative
// factor -> newexpr { ^ newexpr }    right associative
// newexpr -> ( mathexpr ) | number | identifier

enum NodeKind
{
    IF_NODE, REPEAT_NODE, ASSIGN_NODE, READ_NODE, WRITE_NODE,
    OPER_NODE, NUM_NODE, ID_NODE
};

// Used for debugging only /////////////////////////////////////////////////////////
const char* NodeKindStr[]=
{
    "If", "Repeat", "Assign", "Read", "Write",
    "Oper", "Num", "ID"
};

enum ExprDataType {VOID, INTEGER, BOOLEAN};

// Used for debugging only /////////////////////////////////////////////////////////
const char* ExprDataTypeStr[]=
{
    "Void", "Integer", "Boolean"
};

#define MAX_CHILDREN 3

struct TreeNode
{
    TreeNode* child[MAX_CHILDREN];
    TreeNode* sibling; // used for sibling statements only

    NodeKind node_kind;

    union
    {
        TokenType oper;
        int num;
        char* id;
    }; // defined for expression/int/identifier only
    ExprDataType expr_data_type; // defined for expression/int/identifier only

    int line_num;

    TreeNode()
    {
        int i;
        for(i=0; i<MAX_CHILDREN; i++) child[i]=0;
        sibling=0;
        expr_data_type=VOID;
    }
};

struct ParseInfo
{
    Token next_token;
};

TreeNode* exp_evaluate(CompilerInfo*ci, ParseInfo*pi);
TreeNode* stmt_seq(CompilerInfo*ci, ParseInfo*pi);
TreeNode* stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* if_stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* repeat_stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* assign_stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* read_stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* write_stmt(CompilerInfo* ci, ParseInfo* pi);
TreeNode* math_exp_calc(CompilerInfo* ci, ParseInfo* pi);
TreeNode* factor(CompilerInfo* ci, ParseInfo* pi);
TreeNode* term(CompilerInfo* ci, ParseInfo* pi);
TreeNode* new_exp(CompilerInfo* ci, ParseInfo* pi);

//Ensuring the current token aligns with the expected type
//then advance to the next token
void Matching_Perform(CompilerInfo* ci, ParseInfo* pi, TokenType exT)
{
    //Move to the next token
    GetNextToken(ci, &pi->next_token);

}

//stmtseq -> stmt { ; stmt }
//Parse the first statement and then iterate over subsequent statements,
TreeNode* stmt_seq(CompilerInfo* ci, ParseInfo* pi)
{
    //Parse the intial statement in the sequence
    TreeNode* FirstT = stmt(ci, pi);

    //Initialize the pointer of the last_tree to the first_tree
    TreeNode* LastT = FirstT;

    //Check if there are more statements in the sequence
    while (pi->next_token.type != ENDFILE && pi->next_token.type != ELSE && pi->next_token.type != END &&
            pi->next_token.type != UNTIL)
    {
         //Make sure that the statements are separated by semicolons
        Matching_Perform(ci, pi, SEMI_COLON);

        //Parse the next statement in the sequence
        TreeNode* NextT = stmt(ci, pi);

        //Assign the next_tree to be as a sibling to the last_tree
        LastT->sibling = NextT;

        //Update the last_tree
        LastT = NextT;
    }

    //Return the first tree
    return FirstT;
}

// stmt -> ifstmt | repeatstmt | assignstmt | readstmt | writestmt
// creating a tree node representing the parsed statement
TreeNode* stmt(CompilerInfo* ci, ParseInfo* pi)
{
    //Inialize a pointer to the new tree node
    TreeNode* newT = nullptr;

     //Check if the type of the next token is assignment statement
     if (pi->next_token.type == ID)
        newT = assign_stmt(ci, pi);
    //Check if the type of the next token is IF statement
    else if (pi->next_token.type == IF)
        newT = if_stmt(ci, pi);
    //Check if the type of the next token is a WRITE statement
    else if (pi->next_token.type == WRITE)
        newT = write_stmt(ci, pi);
    //Check if the type of the next token is a READ statement
    else if (pi->next_token.type == READ)
        newT = read_stmt(ci, pi);
    //Check if the type of the next token is a REPEAT statement
    else if (pi->next_token.type == REPEAT)
        newT = repeat_stmt(ci, pi);
    //If none, then output this error message
    else
        cout << "ERROR: Unexpected token in stmt !" << endl;

    //Return the tree
    return newT;
}

//ifstmt -> if exp then stmtseq [ else stmtseq ] end
//create a tree node for the if statement
TreeNode* if_stmt(CompilerInfo* ci, ParseInfo* pi)
{
    //Create a new node for if statement
    TreeNode* newT = new TreeNode;
    newT->node_kind = IF_NODE;

    //Match IF keyword
    Matching_Perform(ci, pi, IF);

    //Parse the condition expression and set it as to be the child of the if node
    newT->child[0] = exp_evaluate(ci, pi);

    //Match THEN keyword
    Matching_Perform(ci, pi, THEN);

    //Parse the statement sequence for the true branch and set it as to be the second child
    newT->child[1] = stmt_seq(ci, pi);

    //Check for ELSE branch
    if (pi->next_token.type == ELSE)
    {
        //Match ELSE keyword
        Matching_Perform(ci, pi, ELSE);

        //Parse the statement sequence for the branch of "false"  and set it as the third child
        newT->child[2] = stmt_seq(ci, pi);
    }

    //Match the END keyword
    Matching_Perform(ci, pi, END);

    //Return the tree
    return newT;
}

//parse and create node tree for the repeat statement
TreeNode* repeat_stmt(CompilerInfo* ci, ParseInfo* pi)
{
    // Create a new node for repeat statement
    TreeNode* newT = new TreeNode;
    newT->node_kind = REPEAT_NODE;

    //Match REPEAT keyword
    Matching_Perform(ci, pi, REPEAT);

    //parse the statement sequence as to be first child of this repeat node
    newT->child[0] = stmt_seq(ci, pi);

    //Match UNTIL keyword
    Matching_Perform(ci, pi, UNTIL);

    //Parse the expression as to be the second child of this repeat node
    newT->child[1] = exp_evaluate(ci, pi);

    //Return the tree
    return newT;
}

// assignstmt -> identifier := expr
// Function to parse&create a new tree node for the assignment statement
TreeNode* assign_stmt(CompilerInfo* ci, ParseInfo* pi)
{
    //Create a new node for this assignment statement
    TreeNode* newT = new TreeNode;
    newT->node_kind = ASSIGN_NODE;

    //Check if next token is identifier
    if (pi->next_token.type == ID)
    {
        //then allocate memory for this identifier
        AllocateAndCopy(&newT->id, pi->next_token.str);
    }

    //Match the identifier token
    Matching_Perform(ci, pi, ID);

    //Match the assignment operator =
    Matching_Perform(ci, pi, ASSIGN);

    //Parse the expression to be the right-hand side of it, then set it as to be a child of it
    newT->child[0] = exp_evaluate(ci, pi);

    //Return the tree
    return newT;
}

// writestmt -> write expr
// Function to parse and create a node for the write statement
TreeNode* write_stmt(CompilerInfo* ci, ParseInfo* pi)
{

    //Create a new node to be for the write statement
    TreeNode* TR = new TreeNode;
    TR->node_kind = WRITE_NODE;

    // Perform Matching write keyword
    Matching_Perform(ci, pi, WRITE);
    //Assign the expression as its child
    TR->child[0] = exp_evaluate(ci, pi);

    // Return the tree
    return TR;
}


// readstmt -> read identifier
//Function to parse and create node for the readstatement
TreeNode* read_stmt(CompilerInfo* ci, ParseInfo* pi)
{
    // Create a new node to be for this read statement
    TreeNode* T = new TreeNode;
    T->node_kind = READ_NODE;

    //Matching with  keyword read
    Matching_Perform(ci, pi, READ);

    //Check the next token is identifier
    if (pi->next_token.type == ID)
    {
        //then allocate memory for this identifier
        AllocateAndCopy(&T->id, pi->next_token.str);
    }

    //Matching the identifier token
    Matching_Perform(ci, pi, ID);

    //Return the tree
    return T;
}

// expr -> mathexpr [ (<|=) mathexpr ]
// Function to parse and create an expression node
TreeNode* exp_evaluate(CompilerInfo* ci, ParseInfo* pi)
{
    //create a tree node for the mathematical expression
    TreeNode* t1 = math_exp_calc(ci, pi);

    // Check if the next token = or < operator
    if ( pi->next_token.type == LESS_THAN || pi->next_token.type == EQUAL)
    {
        //Create a newnode for the operator of the comparison
        TreeNode* t2 = new TreeNode;
        t2->node_kind = OPER_NODE;
        t2->oper = pi->next_token.type;

        //assign the left child as to be the previous tree
        t2->child[0] = t1;
        //perform matching with the operator
        Matching_Perform(ci, pi, pi->next_token.type);

        // Parse and set the right child as the next mathematical expression
        t2->child[1] = math_exp_calc(ci, pi);

        // Update the tree
        t1 = t2;
    }
    // Return the tree
    return t1;
}

// mathexpr -> term { (+|-) term }    left associative
// Function to parse and create a mathematical expression node
TreeNode* math_exp_calc(CompilerInfo* ci, ParseInfo* pi)
{

    //Parse and create a tree node for the leftmost term
    TreeNode* Tree = term(ci, pi);

    //parsing while the next token is + or - operator
    while (  pi->next_token.type == MINUS || pi->next_token.type == PLUS )
    {
        //Create a new node for the operator
        TreeNode* newTree = new TreeNode;
        newTree->node_kind = OPER_NODE;
        newTree->oper = pi->next_token.type;

        //assign the left child as to be the previous tree
        newTree->child[0] = Tree;
        //perform matching with the operator
        Matching_Perform(ci, pi, pi->next_token.type);

        //Parse and assign the right child as to be the next term
        newTree->child[1] = term(ci, pi);

        //Update the tree
        Tree = newTree;
    }

    // Return the tree
    return Tree;
}

// factor -> newexpr { ^ newexpr }
//create a factor expression node
TreeNode* factor(CompilerInfo* ci, ParseInfo* pi)
{
    //create a tree node using the new_exp
    TreeNode* FirsT = new_exp(ci, pi);

    //Check if the next token ^ operator
    if (pi->next_token.type == POWER)
    {
        //Create a new node for the operation of power operation
        TreeNode* SecT = new TreeNode;
        SecT->node_kind = OPER_NODE;
        SecT->oper = pi->next_token.type;

        // Set the base expression as the left child
        SecT->child[0] = FirsT;

        //Perform matching function with the token of the power
        Matching_Perform(ci, pi, pi->next_token.type);

        //Parse and set the exponent expression as the right child
        SecT->child[1] = factor(ci, pi);

        //Update the tree to the new operator tree for left associativity
        FirsT = SecT;
    }
    //return the base node of the tree expression If the power operator is not present
    return FirsT;
}

//Constructs a tree node for a term expression by parsing factors
//term -> factor { (*|/) factor }    left associative
TreeNode* term(CompilerInfo* ci, ParseInfo* pi)
{

    // create a tree node for the factor of the leftmost
    TreeNode* Tree_1 = factor(ci, pi);

    //parsing while the next token is a * or / operator
    while ( pi->next_token.type == DIVIDE || pi->next_token.type == TIMES )
    {
        // Create a new tree node for the operator
        TreeNode* Tree_2 = new TreeNode;
        Tree_2->node_kind = OPER_NODE;
        Tree_2->oper = pi->next_token.type;

        //Set the left child as the previous tree
        Tree_2->child[0] = Tree_1;
        //perform matching with the operator
        Matching_Perform(ci, pi, pi->next_token.type);

        //set the right child as the next factor
        Tree_2->child[1] = factor(ci, pi);

        //Update the tree
        Tree_1 = Tree_2;
    }
    // Return the final tree
    return Tree_1;
}

//newexpr -> ( mathexpr ) | number | identifier
//Function to reate new expression node based on the next token and handling cases of numeric literals, identifiers, and parenthesized expressions
TreeNode* new_exp(CompilerInfo* ci, ParseInfo* pi)
{
    TreeNode* t = nullptr; //Inialize tree node pointer

    //Check if the next token is a numeric
    if (pi->next_token.type == NUM)
    {
        //create node
        t = new TreeNode;
        t->node_kind = NUM_NODE;

        //Convert the numeric literal to integer
        char* N_st = pi->next_token.str;
        t->num = atoi(N_st);//converting the string N_st to an integer using the atoi function

        //go to the next token by matching the NUM token
        Matching_Perform(ci, pi, NUM);

        //Return the tree node
        return t;
    }

    //Check the type of the next token
    if (pi->next_token.type == ID)
    {
        //Create an identifier node
        t = new TreeNode;
        t->node_kind = ID_NODE;

        //Copy the string
        AllocateAndCopy(&t->id, pi->next_token.str);

        //go to the next token by matching the ID token
        Matching_Perform(ci, pi, ID);

        // Return the created tree node
        return t;
    }

    //Check the next token left parenthesis
    if (pi->next_token.type == LEFT_PAREN)
    {
        //matching the LEFT_PAREN token
        Matching_Perform(ci, pi, LEFT_PAREN);

        //Parse the expression within the parentheses
        t = exp_evaluate(ci, pi);

        //matching the Right_PAREN token
        Matching_Perform(ci, pi, RIGHT_PAREN);

        // Return the tree
        return t;
    }

    //If none of this expected cases, then display this error message
    cout << "ERROR: Unexpected token in newexpr !" << endl;
}

// program -> stmtseq
//utilizing a statement sequence parser to generate a syntax tree
TreeNode* Parser(CompilerInfo* ci)
{
    //Making object of ParserInfo
    ParseInfo pi;

    //Retrieve the next token from the input program
    GetNextToken(ci, &pi.next_token);

    //Parse the statement sequence and construct the syntax tree
    TreeNode* ParseT = stmt_seq(ci, &pi);

    //Check if it is reached the end, then it will display this error message
    if (pi.next_token.type != ENDFILE)
    {
        //Display an error message  for the unexpected token
        cout << "Error: Unexpected token ," << " Code ends before file ends." << endl;
    }

    //Return the parse tree
    return ParseT;
}

// Function to display the structure of the tree
void PrintTree(TreeNode* node, int sh=0)
{
    int i, NSH=3;
    for(i=0; i<sh; i++) printf(" ");

    printf("[%s]", NodeKindStr[node->node_kind]);

    if(node->node_kind==OPER_NODE) printf("[%s]", TokenTypeStr[node->oper]);
    else if(node->node_kind==NUM_NODE) printf("[%d]", node->num);
    else if(node->node_kind==ID_NODE || node->node_kind==READ_NODE || node->node_kind==ASSIGN_NODE ) printf("[%s]", node->id);

    if(node->expr_data_type!=VOID) printf("[%s]", ExprDataTypeStr[node->expr_data_type]);

    printf("\n");

    for(i=0; i<MAX_CHILDREN; i++) if(node->child[i]) PrintTree(node->child[i], sh+NSH);
    if(node->sibling) PrintTree(node->sibling, sh);
}

//Function  to release the tree and free the memory
void Release_Tree(TreeNode* root)
{
    //do nothing
    if (root == NULL)
    {
        return;
    }

    //Release the children
    int i = 0;
    while (i < MAX_CHILDREN)
    {
        if (root->child[i])
        {
            Release_Tree(root->child[i]);
        }
        i++;
    }

    //free the node
    free(root);
}

int main()
{
    CompilerInfo ci("input.txt", "output.txt", "debug.txt");

    TreeNode* pt = Parser(&ci);

    //Print the structure of the parse tree's terminal (leaf) nodes
    cout << "Parse Tree :" << endl;
    PrintTree(pt,0);

    //Release the parse tree
    Release_Tree(pt);
    return 0;
}
