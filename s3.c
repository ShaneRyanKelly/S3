// Hand-written S3 compiler in C
#include <stdio.h>  // needed by I/O functions
#include <stdlib.h> // needed by malloc and exit
#include <string.h> // needed by str functions
#include <ctype.h>  // needed by isdigit, etc.
#include <time.h>   // needed by asctime

// Constants

// No boolean type in C.  
// Any nonzero number is true.  Zero is false.
#define TRUE 1      
#define FALSE 0  

// Sizes for arrays  
#define MAX 180            // size of string arrays
#define SYMTABSIZE 1000    // symbol table size

#define END 0
#define PRINTLN 1
#define PRINT 2
#define UNSIGNED 3
#define ID 4
#define ASSIGN 5
#define SEMICOLON 6
#define LEFTPAREN 7
#define RIGHTPAREN 8
#define PLUS 9
#define MINUS 10
#define TIMES 11
#define DIVIDE 12
#define NIL 13
#define ERROR 14
#define LEFTBRACKET 15
#define RIGHTBRACKET 16
#define STRING 17
#define READINT 18

time_t timer;    // for asctime

// Prototypes
// Function definition or prototype must 
// precede function call so compiler can 
// check for correct type, number of args
void expr(void);
void statementList(void);

// Global Variables

// tokenImage used in error messages.  See consume function.
char *tokenImage[19] =   
{
  "<END>",
  "\"println\"",
  "\"print\"",
  "<UNSIGNED>",
  "<ID>",
  "\"=\"",
  "\";\"",
  "\"(\"",
  "\")\"",
  "\"+\"",
  "\"-\"",
  "\"*\"",
  "\"/\"",
  "<NIL>",
  "<ERROR>",
  "\"{\"",
  "\"}\"",
  "<STRING>",
  "\"readint\""
};
char buffer[122];
char inFileName[MAX], outFileName[MAX], inputLine[MAX];
int debug = FALSE;
int inString = FALSE;
int currentLabel = 0;
char *symbol[SYMTABSIZE];     // symbol table
int symbolx;                  // index into symbol table

//create new type named TOKEN
typedef struct tokentype
{
   int kind;
   int beginLine, beginColumn, endLine, endColumn;
   char *image;
   struct tokentype *next;
} TOKEN;


FILE *inFile, *outFile;     // file pointers

int currentChar = '\n';
int currentColumnNumber;
int currentLineNumber;
TOKEN *currentToken;
TOKEN *previousToken; 

//-----------------------------------------
// Abnormal end.  
// Close files so s3.a has max info for debugging
void abend(void)
{
   fclose(inFile);
   fclose(outFile);
   exit(1);
}
//-----------------------------------------
void displayErrorLoc(void)
{
    printf("Error on line %d column %d\n", currentToken ->
       beginLine, currentToken -> beginColumn);
}
//-----------------------------------------
// enter symbol into symbol table if not already there
void enter(char *s)
{
   int i = 0;
   while (i < symbolx)
   {
      if (!strcmp(s, symbol[i]))
         break;
      i++;
   }

   // if s is not in symbol table, then add it 
   
   if (i == symbolx)
	{ 
      if (symbolx < SYMTABSIZE)
		{
        	symbol[symbolx++] = s;
		}
	
      else
      {
         printf("System error: symbol table overflow\n");
         abend();
      }
	}
}
//-----------------------------------------
void getNextChar(void)
{
    if (currentChar == END)
      return;

    if (currentChar == '\n')        // need next line?
    {
      // fgets returns 0 (false) on EOF
      if (fgets(inputLine, sizeof(inputLine), inFile))
      {
        // output source line as comment
        fprintf(outFile, "; %s", inputLine);
        currentColumnNumber = 0;
        currentLineNumber++;   
      }                                
      else  // at end of file
      {                
         currentChar = END;
         return;
      }
    }

    // get next char from inputLine
    currentChar =  inputLine[currentColumnNumber++];

    // test for single-line comment
	 if (inString == 0 && currentChar == '/' && inputLine[currentColumnNumber++] == '/')
	 {
	 		currentChar = '\n';
 	 }
}
//---------------------------------------
// This function is tokenizer (aka lexical analyzer, scanner)
TOKEN *getNextToken(void)
{
	 char buffer[80];    // buffer to build image 
    int bufferx = 0;    // index into buffer
    TOKEN *t;	        // will point to taken struct

    // skip whitespace
    while (isspace(currentChar))
      getNextChar();

    // construct token to be returned to parser
    t = (TOKEN *)malloc(sizeof(TOKEN));   
    t -> next = NULL;

    // save start-of-token position
    t -> beginLine = currentLineNumber;
    t -> beginColumn = currentColumnNumber;

    // check for END
    if (currentChar == END)
    {
      t -> image = "<END>";
      t -> endLine = currentLineNumber;
      t -> endColumn = currentColumnNumber;
      t -> kind = END;
    }
	 
    else  // check for unsigned int

    if (isdigit(currentChar)) 
    {
      do
      {
        buffer[bufferx++] = currentChar; 
        t -> endLine = currentLineNumber;
        t -> endColumn = currentColumnNumber;
        getNextChar();
      } while (isdigit(currentChar));

      buffer[bufferx] = '\0';   // term string with null char
      // save buffer as String in token.image
      // strdup allocates space and copies string to it
      t -> image = strdup(buffer);   
      t -> kind = UNSIGNED;
    }
	 
    else  // check for identifier
    if (isalpha(currentChar)) 
    { 
      do  // build token image in buffer
      {
        buffer[bufferx++] = currentChar; 
        t -> endLine = currentLineNumber;
        t -> endColumn = currentColumnNumber;
        getNextChar();
      } while (isalnum(currentChar));
      
      buffer[bufferx] = '\0';

      // save buffer as String in token.image
      t -> image = strdup(buffer);

      // check if keyword
      if (!strcmp(t -> image, "println"))
        t -> kind = PRINTLN;
		else if (!strcmp(t -> image, "print"))
		  t -> kind = PRINT;
		else if (!strcmp(t -> image, "readint"))
			t -> kind = READINT;
      else  // not a keyword so kind is ID
        t -> kind = ID;
	  
    }
	 
	else if (currentChar == '"')
	{
		inString = TRUE;
		do
		{
			buffer[bufferx++] = currentChar;
			t -> endLine = currentLineNumber;
			t -> endColumn = currentColumnNumber;
			getNextChar();
		} while (currentChar != '"');
		inString = FALSE;
		buffer[bufferx++] = currentChar;
	    getNextChar();
		buffer[bufferx] = '\0';

		t -> image = strdup(buffer);
	
		t -> kind = STRING;
	}
    else  // process single-character token
    {  
      switch(currentChar)
      {
        case '=':
          t -> kind = ASSIGN;
          break;                               
        case ';':
          t -> kind = SEMICOLON;
          break;                               
        case '(':
          t -> kind = LEFTPAREN;
          break;                               
        case ')':
          t -> kind = RIGHTPAREN;
          break;                               
        case '+':
          t -> kind = PLUS;
          break;                               
        case '-':
          t -> kind = MINUS;
          break;                               
        case '*':
          t -> kind = TIMES;
          break; 
		  case '/':
			 t -> kind = DIVIDE;
		    break;    
		  case '{':
			 t -> kind = LEFTBRACKET;
			 break;
		  case '}':
			 t -> kind = RIGHTBRACKET;
			 break;                          
        default:
          t -> kind = ERROR;
          break;                               
      }
      // save currentChar as string in image field
      t -> image = (char *)malloc(2);  // get space
      (t -> image)[0] = currentChar;   // move in string
      (t -> image)[1] = '\0';
      
      // save end-of-token position
      t -> endLine = currentLineNumber;
      t -> endColumn = currentColumnNumber;

      getNextChar();  // read beyond end of token
    }
    // token trace appears as comments in output file
 
    // set debug to true to check tokenizer
    if (debug)
      fprintf(outFile, 
        "; kd=%3d bL=%3d bC=%3d eL=%3d eC=%3d     im=%s\n",
        t -> kind, t -> beginLine, t -> beginColumn, 
        t -> endLine, t -> endColumn, t -> image);

    return t;     // return token to parser
}     
//-----------------------------------------
//
// Advance currentToken to next token.
//
void advance(void)
{
    static int firstTime = TRUE;
 

    if (firstTime)
    {
       firstTime = FALSE;
       currentToken = getNextToken();
    }
    else
    {
       previousToken = currentToken; 

       // If next token is on token list, advance to it.
       if (currentToken -> next != NULL)
         currentToken = currentToken -> next;
	
       // Otherwise, get next token from token mgr and 
       // put it on the list.
       else
         currentToken = (currentToken -> next) = 
            getNextToken();
    }
}
//-----------------------------------------
// If the kind of the current token matches the
// expected kind, then consume advances to the next
// token. Otherwise, it throws an exception.
//
void consume(int expected)
{
    if (currentToken -> kind == expected)
      advance();
    else
    {
       displayErrorLoc();       
       printf("Scanning %s, expecting %s\n", 
          currentToken -> image, tokenImage[expected]);
       abend();
    }
}
//-----------------------------------------
// This function not used in S3
// getToken(i) returns ith token without advancing
// in token stream.  getToken(0) returns 
// previousToken.  getToken(1) returns currentToken.
// getToken(2) returns next token, and so on.
//
TOKEN *getToken(int i)
{
    TOKEN *t;
    int j;
    if (i <= 0)
      return previousToken;

    t = currentToken;
    for (j = 1; j < i; j++)  // loop to ith token
    {
      // if next token is on token list, move t to it
      if (t -> next != NULL)
        t = (t -> next);

      // Otherwise, get next token from token mgr and 
      // put it on the list.
      else
        t = (t -> next) = getNextToken();
    } 
    return t;
}
//-----------------------------------------
char * getLabel(void)
{
	char *label = (char *)malloc(20);
	sprintf(label, "@L%d", currentLabel);
	currentLabel++;
	return label;
}
//-----------------------------------------
// emit one-operand instruction
void emitInstruction1(char *op)
{
    fprintf(outFile, "          %-4s\n", op); 
}
//-----------------------------------------
// emit two-operand instruction
// function overloading not supported by C
void emitInstruction2(char *op, char *opnd)
{           
    fprintf(outFile, 
       "          %-4s      %s\n", op,opnd); 
}
//-----------------------------------------
void emitdw(char *label, char *value)
{         
    char temp[80];
    strcpy(temp, label);
    strcat(temp, ":");
      
    fprintf(outFile,
       "%-9s dw        %s\n", temp, value);
}
//-----------------------------------------
void endCode(void)
{
    int i;
    emitInstruction1("\n          halt\n");

    // emit dw for each symbol in the symbol table
    for (i=0; i < symbolx; i++) 
       emitdw(symbol[i], "0");      
}
//-----------------------------------------
void factor(void)
{  
    TOKEN *t;
	char *opnd = malloc(52);
   // char temp[MAX];
    switch(currentToken -> kind)
    {
      case UNSIGNED:
        t = currentToken;
        consume(UNSIGNED);
        emitInstruction2("pwc", t -> image);
        break;
      case PLUS:
		consume(PLUS);
		factor();
		break;

      case MINUS:
			consume(MINUS);
        	switch(currentToken -> kind)
			{
				case UNSIGNED:
					t = currentToken;
					consume(UNSIGNED);
					strcpy(opnd, "-");
					strcat(opnd, t -> image);
					emitInstruction2("pwc", opnd);
					break;
				case ID:
					t = currentToken;
					consume(ID);
					enter(t -> image);
					emitInstruction2("p", t -> image);
					emitInstruction1("neg");
					break;
				case LEFTPAREN:
					consume(LEFTPAREN);
					expr();
					consume(RIGHTPAREN);
					emitInstruction1("neg");
					break;
				case PLUS:
					consume(PLUS);
					factor();
					emitInstruction1("neg");
					break;
				case MINUS:
					consume(MINUS);
					factor();
					break;
			}
			break;
      case ID:
        t = currentToken;
        consume(ID);
        enter(t -> image);
        emitInstruction2("p", t -> image);
        break;
      case LEFTPAREN:
        consume(LEFTPAREN);
        expr();
        consume(RIGHTPAREN);
        break;
	  case LEFTBRACKET:
		consume(LEFTBRACKET);
		expr();
		break;
	  case SEMICOLON:
		consume(SEMICOLON);
		break;
      default:
        displayErrorLoc();
        printf("Scanning %s, expecting factor\n", currentToken ->
           image);
        abend();
    }
}
//-----------------------------------------
void factorList(void)
{
    switch(currentToken -> kind)
    {
      case TIMES:
        consume(TIMES);
        factor();
        emitInstruction1("mult");
        factorList();
        break;
		case DIVIDE:
			consume(DIVIDE);
			factor();
			emitInstruction1("div");
			factorList();
			break;
      case PLUS:
		case MINUS:
      case RIGHTPAREN:
      case SEMICOLON:
		case LEFTBRACKET:
        ;
        break;
      default:
        displayErrorLoc();
        printf("Scanning %s, expecting op, \")\", or \";\"\n",
           currentToken -> image);
        abend();
    }
}
//-----------------------------------------
void term(void)
{
    factor();
    factorList();
}
//-----------------------------------------
void termList(void)
{
    switch(currentToken -> kind)
    {
      case PLUS:
        consume(PLUS);
        term();
        emitInstruction1("add");
        termList();
        break;
		case MINUS:
			consume(MINUS);
			term();
			emitInstruction1("sub");
			termList();
			break;
      case RIGHTPAREN:
      case SEMICOLON:
        ;
        break;
		case LEFTBRACKET:
		case RIGHTBRACKET:
			;
			break;
      default:
        displayErrorLoc();
        printf(
           "Scanning %s, expecting \"+\", \")\", or \";\"\n",
           currentToken -> image);
        abend();
    }
}
//-----------------------------------------
void expr(void)
{
    term();
    termList();
}
//-----------------------------------------
void printArg(void)
{
	TOKEN *t;
	
	switch(currentToken -> kind)
	{
		case STRING:
			t = currentToken;
			consume(STRING);
			char *label = getLabel();
			char *buffer = malloc (strlen(label));
			strcpy(buffer, label);
			emitInstruction2("pc", buffer);
			emitInstruction1("sout");
			char *lab = (char *)malloc(512);
			strcpy(lab, "^");
			strcat(lab, label);
			emitdw(lab, t -> image);
			break;
		case RIGHTPAREN:
			break;
		default:
			expr();
			emitInstruction1("dout");
			break;
	}
}
//-----------------------------------------
void assignmentTail(void)
{
	if (getToken(1) -> kind == ID && getToken(2) -> kind == ASSIGN)
	{
		TOKEN *t;
		t = currentToken;
		consume(ID);
		enter(t -> image);
		emitInstruction2("pc", t -> image);
		consume(ASSIGN);
		assignmentTail();
		emitInstruction1("dupe");
		emitInstruction1("rot");
		emitInstruction1("stav");
	}
	else
	{
		expr();
		consume(SEMICOLON);
	}
}
//-----------------------------------------
void assignmentStatement(void)
{
    TOKEN *t;
    t = currentToken;
    consume(ID);
    enter(t -> image);
    emitInstruction2("pc", t -> image);
    consume(ASSIGN);
    assignmentTail();
    emitInstruction1("stav");
}
//-----------------------------------------
void printStatement(void)
{
	 consume(PRINT);
	 consume(LEFTPAREN);
	 printArg();
	 consume(RIGHTPAREN);
	 consume(SEMICOLON);
}
//-----------------------------------------
void printlnStatement(void)
{
		consume(PRINTLN);                                   
    	consume(LEFTPAREN);
    	printArg();
    	emitInstruction2("pc", "\'\\n\'");
    	emitInstruction1("aout");
    	consume(RIGHTPAREN);
    	consume(SEMICOLON);
}
//-----------------------------------------
void compoundStatement(void)
{
	consume(LEFTBRACKET);
	statementList();
	consume(RIGHTBRACKET);
}
//-----------------------------------------
void nullStatement(void)
{
	 consume(SEMICOLON);
}
//-----------------------------------------
void readIntStatement(void)
{
	TOKEN *t;
	consume(READINT);
	consume(LEFTPAREN);
	t = currentToken;
	consume(ID);
	enter(t -> image);
	emitInstruction2("pc", t -> image);
	emitInstruction1("din");
	emitInstruction1("stav");
	consume(RIGHTPAREN);
}
//-----------------------------------------
void statement(void)
{
    switch(currentToken -> kind)
    {
	
      case ID: 
        assignmentStatement(); 
        break;
      case PRINTLN:   
        printlnStatement(); 
        break;
		case PRINT:
			printStatement();
			break;
		case READINT:
			readIntStatement();
			break;
	   	case SEMICOLON:
		    nullStatement();
			break;
		case LEFTBRACKET:
			compoundStatement();
			break;
		case RIGHTBRACKET:
			;
			break;
      default:  
        displayErrorLoc();      
        printf("Scanning %s, expecting statement\n",
           currentToken -> image);
        abend();
    }
}
//-----------------------------------------
void statementList(void)
{
    switch(currentToken -> kind)
    {
      case ID:
      case PRINTLN:
		case PRINT:
		case SEMICOLON:
		case LEFTBRACKET:
		case READINT:
        statement();
        statementList();
        break;
      case END:
		case RIGHTBRACKET:
        ;                  
        break;
      default:
        displayErrorLoc();
        printf(
           "Scanning %s, expecting statement or end of file\n",
           currentToken -> image);        
        abend();
    }
//-----------------------------------------
}
//-----------------------------------------
void program(void)
{
    statementList();
    endCode();
}
//-----------------------------------------
void parse(void)
{
    advance();
    program();   // program is start symbol for grammar
}
//-----------------------------------------
int main(int argc, char *argv[])
{
   printf("S3 compiler written by Shane Kelly\n");
   if (argc != 2)
   {
      printf("Incorrect number of command line args\n");
      exit(1);
   }
    // build the input and output file names
    strcpy(inFileName, argv[1]);
    strcat(inFileName, ".s");       // append extension

    strcpy(outFileName, argv[1]);
    strcat(outFileName, ".a");      // append extension

    inFile = fopen(inFileName, "r");
    if (!inFile)
    {
       printf("Error: Cannot open %s\n", inFileName);
       exit(1);
    }
    outFile = fopen(outFileName, "w");
    if (!outFile)
    {
       printf("Error: Cannot open %s\n", outFileName);
       exit(1);
    }
    
    time(&timer);     // get time
    fprintf(outFile, "; Shane R Kelly    %s",
       asctime(localtime(&timer)));
    fprintf(outFile, 
       "; Output from S3 compiler\n");

    parse();

    fclose(inFile);
    
    // must close output file or will lose most recent writes
    fclose(outFile);     

    // 0 return code means compile ended without error 
    return 0;  
}
                                
