#include <stdio.h>
#include <stdlib.h>

#define READ_SIZE 500
#define SEP 1

enum pos {START, END};
enum tokenType {TERMINATOR, QUOTE, VARIABLE};

struct fileDict {
	char *key;
	char *sep;
	FILE *file;
};

struct fileStruct {
	int count;
	struct fileDict *dict;
};

struct dictStruct {
	char *key;
	char *value; // sep: null = whitespace, value[0] = 0 means individual characters
};

struct varStruct {
	int count;
	struct dictStruct *dict;
};

struct loopStruct {
	int type; 	// 0 = file; 1 = sep
	int loop;	// 0 = false ; 1 = true
	int addr;	// address offet to find relevant file/sep
	long cmd; 	// fseek to start of loop in script
	char *buff;
	int start;
	int stop;
};

struct loopStack {
	int count;
	int cap;
	struct loopStruct *stack;
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
		txt[cursors[END]] = 0;		// NOTE: can't do this anymore because script lines are reused
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
	// Scans until txt != cursor
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
	// Returns index of var in dictStruct where key matches txt substring
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	return -1;
}

int findFile(struct fileStruct files, char *txt, int *cursors){
	// Returns index of file in fileDict where key matches txt substring
	for (int f=0; f < files.count; ++f) if (substringEquals(files.dict[f].key, txt, cursors)) return f;
	return -1;
}

int findSep(struct varStruct seps, char *txt, int *cursors){
	// Returns index of sep in dictStruct where key matches txt substring
	for (int s=0; s < seps.count; ++s) if (substringEquals(seps.dict[s].key, txt, cursors)) return s;
	return -1;
}

int substring2Num(char *txt, int *cursors){
	// Converts string in txt to integer
	int result = 0;
	for (int i=cursors[START]; i < cursors[END]; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int string2Num(char *txt){
	// Converts string to integer
	int result=0;
	for (int i=0; txt[i] != 0; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int sepSearch(char *txt, char *sep, int start, int stop){
	for (int i=start, j; i < stop; ++i){
		for (j=0; sep[j] != 0 && txt[i+j] == sep[j]; ++j);
		if (sep[j] == 0) return i;
	}
	return -1;
}

char *loadFile(char *input, FILE *file, char *sep, int *length){
	int buffCap = 0, lastCap, end, read;
	do {
		lastCap = buffCap;
		buffCap += READ_SIZE;
		input = realloc(input, (buffCap+1) * sizeof(char));
		read = fread(&input[lastCap], sizeof(char), READ_SIZE, file);
	} while ( read == READ_SIZE  &&  (end=sepSearch(input, sep, lastCap, lastCap + read)) == -1);

	*length = (end == -1 ? lastCap + read : end);
	if (length==0){
		free(input);
		return NULL;
	}
	else {
		fseek(file, ftell(file) - read - end - lastCap, SEEK_SET);
		input[*length] = 0;
		input = realloc(input, (1 + *length) * sizeof(char));
		return input;
	}
}

int main(int argc, char **argv){
	struct varStruct vars = { .count = 0, .dict = NULL };
	struct varStruct seps = { .count = 0, .dict = NULL };
	struct fileStruct files = { .count = 0, .dict = NULL};
	struct loopStack loops = { .count = 0, .cap = 0, .stack = NULL};

	char scriptLine[READ_SIZE + 1];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, READ_SIZE + 1, scriptFile) ; !feof(scriptFile); fgets(scriptLine, READ_SIZE + 1, scriptFile) ) {
		fprintf(stderr, "\t\t*LINE*: %s", scriptLine);
		int cursors[2] = {0, -1};
		if (!findToken(scriptLine, cursors)) {
			fprintf(stderr, "\t\tBlank line\n\n"); 
			continue;
		}
		else if (substringEquals("var", scriptLine, cursors)){
			fprintf(stderr, "\t\tVar declaration\n");
			findToken(scriptLine, cursors);
			vars.dict = realloc(vars.dict, (vars.count + 1) * sizeof(struct dictStruct));
			vars.dict[vars.count].value = NULL;
			vars.dict[vars.count].key = NULL;
			vars.dict[vars.count].key = substringSave(vars.dict[vars.count].key, scriptLine, cursors);
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

			// Get name
			findToken(scriptLine, cursors);
			files.dict = realloc(files.dict, (files.count + 1) * sizeof(struct fileDict));
			files.dict[files.count].sep = NULL;
			files.dict[files.count].key = NULL;
			files.dict[files.count].key = substringSave(files.dict[files.count].key, scriptLine, cursors);

			// Get filename
			if (findToken(scriptLine, cursors) == QUOTE) {
				fprintf(stderr, "\t\tFilename is quote\n");
				files.dict[files.count].file = fopen(&scriptLine[cursors[START]], "r");
			}
			else {
				fprintf(stderr, "\t\tFilename is variable\n");
				files.dict[files.count].file = fopen(vars.dict[findVar(vars, scriptLine, cursors)].value, "r");
			}

			// Get separator
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

			// Get name
			findToken(scriptLine, cursors);
			seps.dict = realloc(seps.dict, (seps.count + 1) * sizeof(struct dictStruct));
			seps.dict[seps.count].key = NULL;
			seps.dict[seps.count].value = NULL;
			seps.dict[seps.count].key = substringSave(seps.dict[seps.count].key, scriptLine, cursors);

			// Get separator
			if (findToken(scriptLine, cursors) == QUOTE) {
				if (cursors[START] == cursors[END]){
					fprintf(stderr, "\t\tEmpty separator provided.  Treat characters individually\n");
					seps.dict[seps.count].value = -1;
				}
				else {
					fprintf(stderr, "\t\tSeparator is quote\n");
					seps.dict[seps.count].value = substringSave(seps.dict[seps.count].value, scriptLine, cursors);
				}
			}
			else if (scriptLine[cursors[START]] != ')'){
				fprintf(stderr, "\t\tSeparator is variable name\n");
				seps.dict[seps.count].value = stringSave(seps.dict[seps.count].value, vars.dict[findVar(vars, scriptLine, cursors)].value);
			}
			else {
				fprintf(stderr, "\t\tDefault whitespace separator used\n");
			}

			fprintf(stderr, "\t\tSep declared.  name = %s sep = %s\n\n", seps.dict[seps.count].key, seps.dict[seps.count].value == -1 ? "wspace" : seps.dict[seps.count].value);
			++seps.count;
		}
		else if (substringEquals("in", scriptLine, cursors)){
			fprintf(stderr, "\t\tIN loop begins ; cnt = %i ; cap = %i\n", loops.count, loops.cap);
			if (loops.count == loops.cap){
				fprintf(stderr, "\t\tAdding another loop to loop stack\n");
				++loops.cap;
				loops.stack = realloc(loops.stack, loops.cap * sizeof(struct loopStruct));
			}

			fprintf(stderr, "\t\tscript pos = %li\n", loops.stack[loops.count].cmd);

			// Save cmd
			loops.stack[loops.count].cmd = ftell(scriptFile);

			// Get type and addr
			findToken(scriptLine, cursors);
			loops.stack[loops.count].addr = findFile(files, scriptLine, cursors);
			if ( loops.stack[loops.count].type = (loops.stack[loops.count].addr == -1) ) {
				fprintf(stderr, "\t\tLoop type = sep %i %s\n", loops.stack[loops.count].addr, seps.dict[loops.stack[loops.count].addr].key);
				loops.stack[loops.count].addr = findSep(seps, scriptLine, cursors);
			}
			else fprintf(stderr, "\t\tLoop type = file %i %s\n", loops.stack[loops.count].addr, files.dict[loops.stack[loops.count].addr].key);

			// Get index
			int skip;
			findToken(scriptLine,cursors);
			if (scriptLine[cursors[END]] == '['){
				fprintf(stderr, "\t\tIndex is provided\n");
				loops.stack[loops.count].loop = 0;
				findToken(scriptLine, cursors);
				if (scriptLine[cursors[START]] >= 48 && scriptLine[cursors[START]] <= 57) {
					fprintf(stderr, "\t\tIndex is numerical\n");
					skip = substring2Num(scriptLine, cursors);
				}
				else {
					fprintf(stderr, "\t\tIndex is variable name\n");
					skip = string2Num(vars.dict[findVar(vars, scriptLine, cursors)].value);
				}
				fprintf(stderr, "\t\tIndex is %i\n", skip);
			}
			else {
				skip = 0;
				loops.stack[loops.count].loop = 1;
				fprintf(stderr, "\t\tNo index provided\n");
			}

			// Load SEP
			if (loops.stack[loops.count].type == SEP){
				fprintf(stderr, "\t\tTransferring buffer\n");
				loops.stack[loops.count].buff = loops.stack[loops.count-1].buff;

				loops.stack[loops.count].start = loops.stack[loops.count-1].start;
				while (skip-- != 0 && loops.stack[loops.count].start != -1 && loops.stack[loops.count].start < loops.stack[loops.count-1].end){
					loops.stack[loops.count].start = sepSearch(loops.stack[loops.count].buffer, seps.dict[loops.stack[loops.count].addr].sep, loops.stack[loops.count].start, loops.stack[loops.count-1].end);
					loops.stack[loops.count].start += seps.dict[loops.stack[loops.count].addr].length;
				}
				if (skip > 0){
					if (loops.stack[loops.count].start >= loops.stack[loops.count-1].end) {
						fprintf(stderr, "\t\tEnd of buffer found\n");
						for (int loopCount=1; loopCount;  ){
							fgets(scriptLine, READ_SIZE + 1, scriptFile);
							cursors[END] = -1;
							findToken(scriptLine, cursors);
							if (substringEquals("in", scriptLine, cursors)) ++loopCount;
							else if (substringEquals("out", scriptLine, cursors)) --loopCount;
						}
						--loops.count;
					}
					else loops.stack[loops.count].end = loops.stack[loops.count-1].end;
				}
				else {
					loops.stack[loops.count].end = sepSearch(loops.stack[loops.count].buffer, seps.dict[loops.stack[loops.count].addr].sep, loops.stack[loops.count].start, loops.stack[loops.count-1].end);
					if (loops.stack[loops.count].end == -1) loops.stack[loops.count].end = loops.stack[loops.count-1].end;
				}
			}
			// Load FILE
			else {
				fprintf(stderr, "\t\tLoading file\n");
				loops.stack[loops.count].start = 0;
				loops.stack[loops.count].buff = NULL;
				do {
					loops.stack[loops.count].buff = loadFile(loops.stack[loops.count].buff, files.dict[loops.stack[loops.count].addr].file, files.dict[loops.stack[loops.count].addr].sep, &loops.stack[loops.count].stop);
				} while (index-- != 0 && input != NULL);
				if (loops.stack[loops.count].buff == NULL){
					fprintf(stderr, "\t\tEnd of file found\n");
					for (int loopCount=1; loopCount;  ){
						fgets(scriptLine, READ_SIZE + 1, scriptFile);
						cursors[END] = -1;
						findToken(scriptLine, cursors);
						if (substringEquals("in", scriptLine, cursors)) ++loopCount;
						else if (substringEquals("out", scriptLine, cursors)) --loopCount;
					}
					--loops.count;
				}
				else fprintf(stderr, "\t\tBuffer length is %i\n", loops.stack[loops.count].stop);
			}
			++loops.count;
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			// if sep
				// if loop
					// get next
					// if -1 
						// if start > end, break loop
						// else adopt parent end + fseek back to cmd
				// else break loop
			// else file
				// if loop
					// get next
					// if null, break loop + fseek back to cmd

			// if successful
			fseek(scriptFile, loops.stack[loops.count-1].cmd, SEEK_SET);

			// otherwise don't skim backwards in file
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
