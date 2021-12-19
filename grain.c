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

int findWhitespace(char *txt, int i){
	// Increments cursor until finds whitespace
	while (txt[i] != ' ' && txt[i] != '\t' && txt[i] != '\n') ++i;
	return i-1;
}

void stringShiftLeft(char *txt, int i){
	// Shuffle string characters left from cursor to null terminator
	do {
		txt[i] = txt[i+1];
	} while (txt[++i] != 0);
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
		case '`':
			return '`';
		case '\'':
			return '\'';
	}
}

int isQuote(char c){
	return c == '"' || c == '\'' || c == '`';
}

int findQuote(char *txt, int i){
	// Scans from cursor character to matching character
	// Converts escape sequences along the way
	char quote = txt[i];
	
	while (txt[++i] != quote){
		if (txt[i] == '\\'){
			txt[i] = convertEscapeSequence(txt[i+1]);
			stringShiftLeft(txt, i+1);
		}
	}
	return i;
}

int skipWhitespace(char *txt, int i){
	while (txt[i] == ' ' || txt[i] == '\t') ++i;
	return i;
}

int substringEquals(char *comp, char *txt, int cursor, int end){
	// Scans txt from cursor to end unless character doesn't match comp string
	//fprintf(stderr, "%p %i %i %s\n", comp, cursor, end, txt);
	for (int compPos=0; cursor <= end; ++compPos, ++cursor) if (comp[compPos] != txt[cursor]) return 0;
	return 1;
}

int stringEquals(char *comp, char *txt){
	for (int i=0; comp[i] != 0 && txt[i] != 0; ++i) if (comp[i] != txt[i]) return 0;
	return 1;
}

int main(int argc, char **argv){
	struct varStruct variables = { .count = 0, .dict = NULL };

	char scriptLine[100];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, 100, scriptFile) ; !feof(scriptFile); fgets(scriptLine, 100, scriptFile) ) {
		int wordStart = skipWhitespace(scriptLine, 0);
		int wordEnd = findWhitespace(scriptLine, wordStart);
		if (scriptLine[wordEnd] == '\n') continue;
		else if (substringEquals("var", scriptLine, wordStart, wordEnd)){
			wordStart = skipWhitespace(scriptLine, wordEnd+1);
			wordEnd = findWhitespace(scriptLine, wordStart);
			variables.dict = realloc(variables.dict, (variables.count + 1) * sizeof(struct varDict));
			variables.dict[variables.count].key = malloc( (wordEnd - wordStart + 2) * sizeof(char) );
			variables.dict[variables.count].key[wordEnd - wordStart + 1] = 0;
			for (int pos=0; wordStart <= wordEnd; ++pos, ++wordStart) variables.dict[variables.count].key[pos] = scriptLine[wordStart];
			//fprintf(stderr, "Variable '%s' declared. %p %p\n", variables.dict[variables.count].key, variables.dict, variables.dict[variables.count].key);
			++variables.count;
		}
		else if (substringEquals("print", scriptLine, wordStart, wordEnd)){
			wordStart = skipWhitespace(scriptLine, wordEnd+1);
			if (isQuote(scriptLine[wordStart])){
				wordEnd = findQuote(scriptLine, wordStart);
				scriptLine[wordEnd]=0;
				printf("%s", &scriptLine[wordStart+1]);
			}
			else {
				wordEnd = findWhitespace(scriptLine, wordStart);
				for (int v=0; v < variables.count; ++v) {
					if (substringEquals(variables.dict[v].key, scriptLine, wordStart, wordEnd)){
						printf("%s", variables.dict[v].value);
						break;
					}
				}
			}
		}
		else {
			int varIndex;
			for (varIndex = 0; substringEquals(variables.dict[varIndex].key, scriptLine, wordStart, wordEnd) == 0 ; ++varIndex);
			wordStart = skipWhitespace(scriptLine, wordEnd+1);
			wordEnd = findWhitespace(scriptLine, wordStart);
			if (substringEquals("=", scriptLine, wordStart, wordEnd)){
				// assignment
				wordStart = skipWhitespace(scriptLine, wordEnd+1);
				wordEnd = findWhitespace(scriptLine, wordStart);
				if (isQuote(scriptLine[wordStart])){
					wordEnd = findQuote(scriptLine, wordStart);
					++wordStart;
					scriptLine[wordEnd]=0;
					variables.dict[varIndex].value = realloc(variables.dict[varIndex].value, (wordEnd - wordStart + 2) * sizeof(char));
					for (int pos=0; wordStart <= wordEnd; ++pos, ++wordStart) variables.dict[varIndex].value[pos] = scriptLine[wordStart];
				}
				else {
					// copying value from variable
					//fprintf(stderr, "Assigning variable value\n");
				}
				
			}
			else if (substringEquals("+=", scriptLine, wordStart, wordEnd)){
				// addition
				wordStart = skipWhitespace(scriptLine, wordEnd+1);
				wordEnd = findWhitespace(scriptLine, wordStart);
				// word should equal a number
				// convert var to number
				// carry out operation
			}
			else if (substringEquals("-=", scriptLine, wordStart, wordEnd)){
				// subtraction
				wordStart = skipWhitespace(scriptLine, wordEnd+1);
				wordEnd = findWhitespace(scriptLine, wordStart);
			}
			else if (substringEquals("/=", scriptLine, wordStart, wordEnd)){
				// divison
				wordStart = skipWhitespace(scriptLine, wordEnd+1);
				wordEnd = findWhitespace(scriptLine, wordStart);
			}
			else if (substringEquals("*=", scriptLine, wordStart, wordEnd)){
				// multiplication
				wordStart = skipWhitespace(scriptLine, wordEnd+1);
				wordEnd = findWhitespace(scriptLine, wordStart);
			}
		}
	}
	
	return 0;
}
