// UPDATE: TOKENISER NEEDS TO TOKENISE THINGS WITHIN DOUBLE QUOTES AS A SINGLE TOKEN!

#include <stdio.h>
#include <stdlib.h>

struct fileDict {
	char *key;
	FILE *file;
};

struct varDict {
	char *key;
	char *value;
};

struct varStruct {
	int count;
	struct varDict *dict;
};

struct tokenStruct {
	int count;
	char **tokens;
};

struct tokenStruct tokenise(char *line);
void freeTokenStruct(struct tokenStruct tokens);
int whitespace(char c);
int parseVarDeclare(char *varName, struct varStruct *variables);
void parsePrintString(char *scriptLine);
char parseEscapeSequence(char *scriptLine, int index);
int findQuotes(char *scriptLine);
int varExists(char *varName, struct varStruct *variables);
int varNameValid(char *varName);
int varFirstCharValid(char c);
int varCharValid(char c);
int getStringLength(char *string);
int stringEquals(char *a, char *b);
struct tokenStruct tokenise(char *line);
int main(char **argv){
	struct varStruct variables = { .count = 0, .dict = NULL };

	char scriptLine[100];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, 100, scriptFile) ; !feof(scriptFile); fgets(scriptLine, 100, scriptFile) ) {
		int errorNumber;
		struct tokenStruct tokens = tokenise(scriptLine);
		if (tokens.count == 0) continue;
		else if (stringEquals(tokens.tokens[0], "var")) {
			if (tokens.count != 2) return fprintf(stderr, "Expected variable name.  Error on line: %s", scriptLine);
			else if (errorNumber = parseVarDeclare(tokens.tokens[1], &variables)) {
				switch(errorNumber){
					case 1:
						return fprintf(stderr, "Invalid variable name: %s\n", tokens.tokens[1]);
					case 2:
						return fprintf(stderr, "Redefinition of variable: \"%s\"\n", tokens.tokens[1]);
				}
			}
		}
		else if (stringEquals(tokens.tokens[0], "print")) {
			if (tokens.count != 2) return fprintf(stderr, "Expected variable name or string.  Error on line: %s", scriptLine);
			else if (tokens.tokens[1][0]) parsePrintString(scriptLine);
			else {
				int varPos = varExists(tokens.tokens[1], &variables);
				if (varPos == -1) fprintf(stderr, "Variable \"%s\" does not exist\n", tokens.tokens[1]);
				else printf("%s", variables.dict[varPos].value);
			}
		}
		freeTokenStruct(tokens);
	}
	
	return 0;
}

int parseVarDeclare(char *varName, struct varStruct *variables){
	if (!varNameValid(varName)) return 1;
	else if (varExists(varName, variables) == -1) return 2;

	variables->dict = realloc(variables->dict, (variables->count + 1) * sizeof(struct varDict));

	int varLength = getStringLength(varName);

	variables->dict[variables->count].key = malloc( (varLength + 1) * sizeof(char) );
	variables->dict[variables->count].value = NULL;

	for (int pos=0; varName[pos]!=0; ++pos) variables->dict[variables->count].key[pos] = varName[pos];
	variables->dict[variables->count].key[varLength] = 0;
	++(variables->count);

	return 0;
}

void parsePrintString(char *scriptLine){
	int start=findQuotes(scriptLine), pos=start;
	for (  ; scriptLine[pos] != '"'; ++pos) if (scriptLine[pos] == '\\') parseEscapeSequence(scriptLine, pos);
	scriptLine[pos] = 0;
	printf("%s", &scriptLine[start]);
}

char parseEscapeSequence(char *scriptLine, int index){
	switch (scriptLine[index+1]){
		case 'n':
			scriptLine[index] = '\n';
			break;
		case 't':
			scriptLine[index] = '\t';
			break;
		case '"':
			scriptLine[index] = '"';
			break;
		case '\'':
			scriptLine[index] = '\'';
			break;
		case '\\':
			scriptLine[index] = '\\';
		default:
			exit(fprintf(stderr, "Unrecognised escape sequence: \\%c\n", scriptLine[index+1]));
	}
	
	// Shuffle subsequent characters left
	for (index = index+1; scriptLine[index] != '"' ; ) scriptLine[index] = scriptLine[index++];
	scriptLine[index] = 0;
}

int findQuotes(char *scriptLine){
	for (int i=6;  ; ++i) if (scriptLine[i] == '"') return i+1;
}

int varExists(char *varName, struct varStruct *variables){
	for (int i=0; i < variables->count; ++i) if ( stringEquals(varName, variables->dict[i].key) ) return i;
	return -1;
}

int varNameValid(char *varName){
	if (!varFirstCharValid(varName[0])) return 0;
	for (int i=1; varName[i]!=0; ++i) if (!varFirstCharValid(varName[i])) return 0;
	return 1;
}

int varFirstCharValid(char c){
	return c == '_' || c > 64 && c < 91 || c > 96 && c < 123;
}

int varCharValid(char c){
	return c == '_' || c > 64 && c < 91 || c > 96 && c < 123 || c > 47 && c < 58;
}

int getStringLength(char *string){
	int pos=0;
	while (string[pos++] != 0);
	return pos;
}

int stringEquals(char *a, char *b){
	for (int aPos=0, bPos=0;  ; ++aPos, ++bPos) if (a[aPos] != b[bPos]) return 0;
	return 1;
}

struct tokenStruct tokenise(char *line){
	// UPDATE: TOKENISER NEEDS TO TOKENISE THINGS WITHIN DOUBLE QUOTES AS A SINGLE TOKEN!



	struct tokenStruct tokens;
	tokens.count = 0;

	// How many tokens are there?
	for (int pos=0; line[pos]!=0 ; ){
		if (!whitespace(line[pos]) ) {
			++tokens.count;
			while (!whitespace(line[++pos]));
		}
		else ++pos;
	}

	// Allocate char** for tokens
	tokens.tokens = malloc(tokens.count * sizeof(char*));

	// Allocate each token length
	for (int pos=0, token=0; line[pos]!=0 ; ){
		if (!whitespace(line[pos])) {
			int length=2;
			while (!whitespace(line[++pos])) ++length;
			tokens.tokens[token] = malloc(length * sizeof(char));
			++token;
		}
		else ++pos;
	}

	// Copy each word
	for (int linePos=0, token=0; line[linePos] != 0 ; ){
		if (!whitespace(line[linePos])) {
			int tokenPos=0;
			do {
				tokens.tokens[token][tokenPos++] = line[linePos];
			} while (!whitespace(line[++linePos]));
			tokens.tokens[token][tokenPos] = 0;
			++token;
		}
		else ++linePos;
	}

	return tokens;
}

int whitespace(char c){
	return c == ' ' || c == '\t' || c == '\n' || c == 0;
}

void freeTokenStruct(struct tokenStruct tokens){
	for (int token=0; token < tokens.count; ++token) free(tokens.tokens[token]);
	free(tokens.tokens);
}
