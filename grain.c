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

char parseEscapeSequence(char *scriptLine, int index);
int varNameValid(char *varName);
int varFirstCharValid(char c);
int varCharValid(char c);
int getStringLength(char *string);
int stringEquals(char *comp, char *scriptLine, int cursor, int end);

int findWord(char *scriptLine, int cursor){
	while (scriptLine[cursor] != ' ' && scriptLine[cursor] != '\t' && scriptLine[cursor] != '\n') ++cursor;
	return cursor-1;
	
}

void shuffleString(char *scriptLine, int cursor){
	do {
		scriptLine[cursor] = scriptLine[++cursor];
	} while (scriptLine[cursor] != 0);
}

char convertEscapeSequence(char escapeChar){
	switch (escapeChar) {
		case 'n':
			return '\n';
		case 't':
			return '\t';
		case '\\':
			return '\\';
		case '"':
			return '"';
		case '\'':
			return '\'';
	}
}

int findString(char *scriptLine, int cursor, char quote){
	while (scriptLine[++cursor] != quote){
		if (scriptLine[cursor] == '\\'){
			scriptLine[cursor] = convertEscapeSequence(scriptLine[cursor+1]);
			shuffleString(scriptLine, cursor+1);
		}
	}
	return cursor;
}

int skipWhitespace(char *scriptLine, int cursor){
	while (scriptLine[cursor] == ' ' || scriptLine[cursor] == '\t') ++cursor;
	return cursor;
}

int main(int argc, char **argv){
	struct varStruct variables = { .count = 0, .dict = NULL };

	char scriptLine[100];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, 100, scriptFile) ; !feof(scriptFile); fgets(scriptLine, 100, scriptFile) ) {
		int wordStart = skipWhitespace(scriptLine, 0);
		int wordEnd = findWord(scriptLine, wordStart);
		if (scriptLine[wordEnd] == '\n') continue;
		else if (stringEquals("var", scriptLine, wordStart, wordEnd)){
			wordStart = skipWhitespace(scriptLine, wordEnd+1);
			wordEnd = findWord(scriptLine, wordStart);
			variables.dict = realloc(variables.dict, (variables.count + 1) * sizeof(struct varDict));
			int wordLength = wordEnd - wordStart + 1;
			variables.dict[variables.count].key = malloc( (wordLength + 1) * sizeof(char) );
			variables.dict[variables.count].key[wordLength] = 0;
			for (int pos=0; wordStart <= wordEnd; ++pos, ++wordStart) variables.dict[variables.count].key[pos] = scriptLine[wordStart];
			fprintf(stderr, "Variable '%s' declared.\n", variables.dict[variables.count].key);
		}
		else if (stringEquals("print", scriptLine, wordStart, wordEnd)){
			wordStart = skipWhitespace(scriptLine, wordEnd+1);
			if (scriptLine[wordStart] == '"' || scriptLine[wordStart] == '\'') {
				wordEnd = findString(scriptLine, wordStart+1, scriptLine[wordStart]);
				scriptLine[wordEnd]=0;
				printf("%s", &scriptLine[wordStart+1]);
			}
			else {
				wordEnd = findWord(scriptLine, wordStart);
			}
		}
	}
	
	return 0;
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

int stringEquals(char *comp, char *stringLine, int cursor, int end){
	for (int compPos=0; cursor <= end; ++compPos, ++cursor) if (comp[compPos] != stringLine[cursor]) return 0;
	return 1;
}
