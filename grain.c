#include <stdio.h>
#include <stdlib.h>

#define START 0
#define END 1

#define TERMINATOR 0
#define QUOTE 1
#define VARIABLE 2

struct fileDict {
	char *key;
	char *sep;
	FILE *file;
};

struct varDict {
	char *key;
	char *value;
};

struct sepDict {
	char *key;
	char *value;
	int index;
};

struct varStruct {
	int count;
	struct varDict *dict;
};

struct fileStruct {
	int count;
	struct fileDict *dict;
};

struct sepStruct {
	int count;
	struct sepDict *dict;
};

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

int findToken(char *txt, int *cursors){
	/* Moves cursors around next token
		* cursor[START] = 1st character (read from here)
		* cursor[END] = after last character (read up to not including here)
	 * Token is:
		* Word surrounded by whitespace (command or variable name)
		* Text within matching quotes (' or " or `)
	 * Return:
		0 if find terminator
		1 if find quotation.
		2 if find word.
	 * cursors[START] begins scanning from after cursors[END]
	 * Quotations:
		* Escape characters automatically converted and removed from string
		* Closing quotation converted to 0 terminator	*/

	cursors[START] = cursors[END] + 1;

	// Skip whitespace
	while (txt[cursors[START]] == ' ' || txt[cursors[START]] == '\t') ++cursors[START];

	// Check if terminator, quote or other
	switch (txt[cursors[START]]){
	case '\n':
	case ';':
	case '0':
		return TERMINATOR;
	case '\'':
	case '"':
	case '`':
		// Found quote.  Scan to matching quotation mark.
		for (cursors[END] = cursors[START] + 1; txt[cursors[END]] != txt[cursors[START]] ; ++cursors[END]){
			// Check for escape character
			if (txt[cursors[END]] == '\\'){
				// Shuffle string left
				for (int i=cursors[END], j=cursors[END]+1; txt[i]!=0; ++i, ++j) txt[i] = txt[j];
				// Convert escape character
				switch (txt[cursors[END]]){
				case 'n':
					txt[cursors[END]] = '\n';
					break;
				case 't':
					txt[cursors[END]] = '\t';
					break;
				case '\\':
					txt[cursors[END]] = '\\';
					break;
				case '\'':
					txt[cursors[END]] = '\'';
					break;
				case '"':
					txt[cursors[END]] = '"';
					break;
				case '`':
					txt[cursors[END]] = '`';
					break;
				}
			}
		}
		++cursors[START];
		txt[cursors[END]] = 0;
		return QUOTE;
	default:
		for (cursors[END] = cursors[START]+1; txt[cursors[END]] != ' ' 
						   && txt[cursors[END]] != '\t' 
						   && txt[cursors[END]] != '\n' 
					           && txt[cursors[END]] != 0 
						   && txt[cursors[END]] != ';'
						   && txt[cursors[END]] != ','
						   && txt[cursors[END]] != '('
						   && txt[cursors[END]] != ')'
						   && txt[cursors[END]] != '['
						   && txt[cursors[END]] != ']' ; ++cursors[END]);
		return VARIABLE;
	}
}

int substringEquals(char *comp, char *txt, int *cursors){
	// Scans txt from cursors[START] to cursors[END] until character doesn't match comp string
	for (int compPos=0, txtPos=cursors[START]; txtPos < cursors[END]; ++compPos, ++txtPos) if (comp[compPos] != txt[txtPos]) return 0;
	return 1;
}

int stringEquals(char *comp, char *txt){
	for (int i=0; comp[i] != 0 && txt[i] != 0; ++i) if (comp[i] != txt[i]) return 0;
	return 1;
}

char *substringSave(char *dest, char *source, int *cursors){
	// Reallocate memory in dest for source string from cursors[START] up to (not including) cursors[END]
	// Copies source string to dest string from cursors[START] up to (not including) cursors[END]
	// Returns newly allocated dest address

	dest = realloc(dest, (1 + cursors[END] - cursors[START]) * sizeof(char));
	dest[cursors[END]-cursors[START]] = 0;
	for (int destPos=0; cursors[START] < cursors[END]; ++cursors[START], ++destPos) dest[destPos] = source[cursors[START]]; 
	return dest;
}

char *stringSave(char *dest, char *source){
	int length;
	for (length = 0; source[length] != 0; ++length);

	dest = realloc(dest, (length+1) * sizeof(char));
	for (int pos=0; pos < length; ++pos) dest[pos] = source[pos];
	dest[length] = 0;

	return dest;
}

int findVar(struct varStruct vars, char *txt, int *cursors){
	for (int v=0; v < vars.count; ++v){
		if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	}
	return -1;
}

int substring2Num(char *txt, int *cursors){
	int result = 0;
	for (int i=cursors[START]; i < cursors[END]; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int string2Num(char *txt){
	int result=0;
	for (int i=0; txt[i] != 0; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int main(int argc, char **argv){
	struct varStruct vars = { .count = 0, .dict = NULL };
	struct sepStruct seps = { .count = 0, .dict = NULL };
	struct fileStruct files = { .count = 0, .dict = NULL};

	char scriptLine[100];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, 100, scriptFile) ; !feof(scriptFile); fgets(scriptLine, 100, scriptFile) ) {
		fprintf(stderr, "\t\t*LINE*: %s", scriptLine);
		int cursors[2] = {0, -1};
		if (!findToken(scriptLine, cursors)) {
			fprintf(stderr, "\t\tBlank line\n\n"); 
			continue;
		}
		else if (substringEquals("var", scriptLine, cursors)){
			fprintf(stderr, "\t\tVar declaration\n");
			findToken(scriptLine, cursors);
			vars.dict = realloc(vars.dict, (vars.count + 1) * sizeof(struct varDict));
			vars.dict[vars.count].key = substringSave(vars.dict[vars.count].key, scriptLine, cursors);
			vars.dict[vars.count].value = NULL;
			fprintf(stderr, "\t\t%s declared\n\n", vars.dict[vars.count].key);
			++vars.count;
		}
		else if (substringEquals("print", scriptLine, cursors)){
			fprintf(stderr, "\t\tPrint command\n");
			if (findToken(scriptLine, cursors) == QUOTE) fprintf(stderr, "\t\tPrinting quotation\n\n"), printf("%s", &scriptLine[cursors[START]]);
			else {
				fprintf(stderr, "\t\tPrinting variable\n\n");
				printf("%s", vars.dict[findVar(vars, scriptLine, cursors)].value);
			}
		}
		else if (substringEquals("file", scriptLine, cursors)){
			fprintf(stderr, "\t\tFile declaration\n");
			findToken(scriptLine, cursors);
			files.dict = realloc(files.dict, (files.count + 1) * sizeof(struct fileDict));
			files.dict[files.count].sep = NULL;
			files.dict[files.count].key = NULL;
			files.dict[files.count].key = substringSave(files.dict[files.count].key, scriptLine, cursors);

			if (findToken(scriptLine, cursors) == QUOTE) {
				fprintf(stderr, "\t\tFilename is quote\n");
				files.dict[files.count].file = fopen(&scriptLine[cursors[START]], "r");
			}
			else {
				fprintf(stderr, "\t\tFilename is variable\n");
				files.dict[files.count].file = fopen(vars.dict[findVar(vars, scriptLine, cursors)].value, "r");
			}

			
			findToken(scriptLine, cursors);
			if (scriptLine[cursors[START]] == ','){
				fprintf(stderr, "\t\tFile separator provided\n");
				if (findToken(scriptLine, cursors) == QUOTE) {
					fprintf(stderr, "\t\tSeparator is quote\n");
					files.dict[files.count].sep = substringSave(files.dict[files.count].sep, scriptLine, cursors);
				}
				else {
					fprintf(stderr, "\t\tSeparator is var name\n");
					files.dict[files.count].sep = stringSave(files.dict[files.count].sep, vars.dict[findVar(vars, scriptLine, cursors)].value);
				}
			}
			else {
				fprintf(stderr, "\t\tDefault separator used\n");
				files.dict[files.count].sep = malloc(2 * sizeof(char));
				files.dict[files.count].sep[0] = '\n';
				files.dict[files.count].sep[1] = 0;
			}

			fprintf(stderr, "\t\tFile declared.  Name = %s Sep = %s\n\n", files.dict[files.count].key, files.dict[files.count].sep);
			++files.count;
		}
		else if (substringEquals("sep", scriptLine, cursors)){
			fprintf(stderr, "\t\tSep declaration\n");
			findToken(scriptLine, cursors);
			seps.dict = realloc(seps.dict, (seps.count + 1) * sizeof(struct sepStruct));
			seps.dict[seps.count].key = NULL;
			seps.dict[seps.count].value = NULL;
			seps.dict[seps.count].key = substringSave(seps.dict[seps.count].key, scriptLine, cursors);
			
			if (findToken(scriptLine, cursors) == QUOTE) {
				fprintf(stderr, "\t\tSeparator is quote\n");
				seps.dict[seps.count].value = substringSave(seps.dict[seps.count].value, scriptLine, cursors);
			}
			else {
				fprintf(stderr, "\t\tSeparator is variable name\n");
				seps.dict[seps.count].value = stringSave(seps.dict[seps.count].value, vars.dict[findVar(vars, scriptLine, cursors)].value);
			}

			findToken(scriptLine, cursors);
			if (scriptLine[cursors[END]] == '[') {
				fprintf(stderr, "\t\tIndex provided\n");
				findToken(scriptLine, cursors);
				if (scriptLine[cursors[START]] >= 48 && scriptLine[cursors[START]] <= 57) {
					fprintf(stderr, "\t\tIndex is numerical\n");
					seps.dict[seps.count].index = substring2Num(scriptLine, cursors);
				}
				else {
					fprintf(stderr, "\t\tIndex is variable name\n");
					seps.dict[seps.count].index = string2Num(vars.dict[findVar(vars, scriptLine, cursors)].value);
				}
			}
			else {
				fprintf(stderr, "\t\tNo index provided\n");
				seps.dict[seps.count].index = -1;
			}

			fprintf(stderr, "\t\tSep declared.  name = %s sep = %s index = %i\n\n", seps.dict[seps.count].key, seps.dict[seps.count].value, seps.dict[seps.count].index);
		}
		else {
			fprintf(stderr, "\t\tAssuming variable assignment\n");
			int destIndex = findVar(vars, scriptLine, cursors);
			fprintf(stderr, "\t\tAssigning to '%s'\n", vars.dict[destIndex].key);
			findToken(scriptLine, cursors);
			if (substringEquals("=", scriptLine, cursors)){
				// assignment
				//fprintf(stderr, "ERR: assignment\n");
				if (findToken(scriptLine, cursors) == QUOTE){
					fprintf(stderr, "\t\tAssigning quote: '%s'\n\n", &scriptLine[cursors[START]]);
					vars.dict[destIndex].value = substringSave(vars.dict[destIndex].value, scriptLine, cursors);
				}
				else {
					// copying value from variable
					fprintf(stderr, "\t\tAssigning variable value\n");
					int sourceIndex = findVar(vars, scriptLine, cursors);
					fprintf(stderr, "\t\tAssigning value from %s : %s\n\n", vars.dict[sourceIndex].key, vars.dict[sourceIndex].value);
					vars.dict[destIndex].value = stringSave(vars.dict[destIndex].value, vars.dict[sourceIndex].value);
				}
			}
		}
	}
	
	return 0;
}
