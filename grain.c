#include <stdio.h>
#include <stdlib.h>

#define READ_SIZE 500
#define SEP 1

enum pos {START, END};
enum tokenType {TERMINATOR, QUOTE, VARIABLE};
enum boolean {FALSE, TRUE};

struct fileDict {
	char *key; // filename
	char *sep;
	int len;
	FILE *fp;
};

struct fileStruct {
	int count;
	struct fileDict *dict;
};

struct varDict {
	char *key;
	char *val; 
};

struct varStruct {
	int count;
	struct varDict *dict;
};

struct sepDict {
	char *key;
	char *val;
	int len;
};

struct sepStruct {
	int count;
	struct sepDict *dict;
};

struct loopStruct {
	int type; 	// 0 = file; 1 = sep
	int isLoop;	// 0 = false ; 1 = true
	int addr;	// address offet to find relevant file/sep
	long cmd; 	// fseek to start of loop in script
	char *buff;
	int start;
	int stop;
};

struct loopStack {
	int ptr;
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
	* Escape characters automatically converted and removed from string */

	cursors[START] = cursors[END] + 1;

	// Skip whitespace
	while (txt[cursors[START]] == ' ' || txt[cursors[START]] == '\t') ++cursors[START];

	// Check if terminator, quote or other
	switch (txt[cursors[START]]){
	case '\n':
	case ';':
	case '\0':
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
	// Returns index of var in varDict where key matches txt substring
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	return -1;
}

int findFile(struct fileStruct files, char *txt, int *cursors){
	// Returns index of file in fileDict where key matches txt substring
	for (int f=0; f < files.count; ++f) if (substringEquals(files.dict[f].key, txt, cursors)) return f;
	return -1;
}

int findSep(struct sepStruct seps, char *txt, int *cursors){
	// Returns index of sep in sepDict where key matches txt substring
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
	fprintf(stderr, "\t\t\tSEPSEARCH called\n");
	fprintf(stderr, "\t\t\tFind %s\n", sep);
	fprintf(stderr, "\t\t\tBetween %i and %i\n", start, stop);
	for (int i=start, j; i < stop; ++i){
		for (j=0; sep[j] != 0 && txt[i+j] == sep[j]; ++j);
		if (sep[j] == 0) {
			fprintf(stderr, "\t\t\tFound at %i\n", i);
			return i;
		}
	}
	fprintf(stderr, "\t\t\tNot found\n");
	return -1;
}

char *loadFile(char *input, FILE *file, char *sep, int *length){
	fprintf(stderr, "\t\t\tLOADFILE called\n");
	int buffCap = 0, lastCap, end=-1, read;
	do {
		lastCap = buffCap;
		buffCap += READ_SIZE;
		input = realloc(input, (buffCap+1) * sizeof(char));
		read = fread(&input[lastCap], sizeof(char), READ_SIZE, file);
		fprintf(stderr, "\t\t\tlastCap %i buffCap %i read %i\n", lastCap, buffCap, read);
	} while ( read == READ_SIZE  &&  (end=sepSearch(input, sep, lastCap, lastCap + read)) == -1);

	*length = (read == READ_SIZE ? end : lastCap + read);
	if (*length == 0){
		free(input);
		return NULL;
	}
	else if (read == READ_SIZE){
		fprintf(stderr, "\t\tSep must have been found\n");
		int spare=0;
		for (int pos=0; sep[pos]!=0; ++pos, --spare);
		spare += (READ_SIZE - (end % READ_SIZE));
		fseek(file, (long) spare, SEEK_CUR);
		fprintf(stderr, "\t\t\tScanning file cursor backwards by %i\n", spare);
	}
	input[*length] = 0;
	return realloc(input, (1 + *length) * sizeof(char));
}

int breakFun(struct loopStruct loop, char *scriptLine, FILE *scriptFile){
	fprintf(stderr, "\t\t\tbreak function called\n");
	if (loop.type != SEP) fprintf(stderr, "\t\t\tFreeing file buffer\n"), free(loop.buff);

	int cursors[2];
	for (int loopCount=1; loopCount; ){
		fgets(scriptLine, READ_SIZE + 1, scriptFile);
		cursors[END] = -1;
		findToken(scriptLine, cursors);
		if (substringEquals("in", scriptLine, cursors)) fprintf(stderr, "\t\t\tFound 'in', inc loop count\n"), ++loopCount;
		else if (substringEquals("out", scriptLine, cursors)) fprintf(stderr, "\t\t\tFound 'out', dec loop count\n"), --loopCount;
	}
	return 1;
}

void printSubstring(char *txt, int *cursors){
	char swap = txt[cursors[END]];
	txt[cursors[END]] = 0;
	fprintf(stderr, "\t\t%s\n", &txt[cursors[START]]);
	txt[cursors[END]] = swap;
}

int main(int argc, char **argv){
	struct varStruct vars = { .count = 0, .dict = NULL };
	struct sepStruct seps = { .count = 0, .dict = NULL };
	struct fileStruct files = { .count = 0, .dict = NULL};
	struct loopStack loops = { .ptr = -1, .cap = 0, .stack = NULL};

	char scriptLine[READ_SIZE + 1];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, READ_SIZE + 1, scriptFile) ; !feof(scriptFile); fgets(scriptLine, READ_SIZE + 1, scriptFile) ) {
		fprintf(stderr, "\t\t*INPUT LINE*: %s", scriptLine);
		int cursors[2] = {0, -1};
		if (!findToken(scriptLine, cursors)) {
			fprintf(stderr, "\t\tBlank line\n\n"); 
			continue;
		}
		else if (substringEquals("var", scriptLine, cursors)){
			fprintf(stderr, "\t\tVAR command\n");
			findToken(scriptLine, cursors);
			vars.dict = realloc(vars.dict, (vars.count + 1) * sizeof(struct varDict));
			struct varDict *var = &vars.dict[vars.count];
			var->val = var->key = NULL;
			var->key = substringSave(var->key, scriptLine, cursors);
			fprintf(stderr, "\t\t%s declared\n\n", var->key);
			++vars.count;
		}
		else if (substringEquals("print", scriptLine, cursors)){
			fprintf(stderr, "\t\tPRINT command\n");

			switch (findToken(scriptLine, cursors)){
			case TERMINATOR:
				fprintf(stderr, "\t\tPrint current buffer\n\n");
				printf("%s", loops.stack[loops.ptr].buff);
				break;
			case QUOTE:
				fprintf(stderr, "\t\tPrint quotation\n\n");
				char swap = scriptLine[cursors[END]];
				scriptLine[cursors[END]] = 0;
				printf("%s", &scriptLine[cursors[START]]);
				scriptLine[cursors[END]] = swap;
				break;
			default:
				fprintf(stderr, "\t\tPrint variable\n\n");
				printf("%s", vars.dict[findVar(vars, scriptLine, cursors)].val);
			}
		}
		else if (substringEquals("file", scriptLine, cursors)){
			fprintf(stderr, "\t\tFILE command\n");

			// Get name
			findToken(scriptLine, cursors);
			files.dict = realloc(files.dict, (files.count + 1) * sizeof(struct fileDict));

			struct fileDict *file = &files.dict[files.count];
			file->key = file->sep = NULL;
			file->key = substringSave(file->key, scriptLine, cursors);

			// Get filename
			if (findToken(scriptLine, cursors) == QUOTE) {
				fprintf(stderr, "\t\tFilename = quote\n");
				char swap = scriptLine[cursors[END]];
				scriptLine[cursors[END]] = 0;
				file->fp = fopen(&scriptLine[cursors[START]], "r");
				scriptLine[cursors[END]] = swap;
			}
			else {
				fprintf(stderr, "\t\tFilename = variable\n");
				file->fp = fopen(vars.dict[findVar(vars, scriptLine, cursors)].val, "r");
			}

			// Get separator
			findToken(scriptLine, cursors);
			if (scriptLine[cursors[START]] == ','){
				fprintf(stderr, "\t\tSeparator provided\n");
				if (findToken(scriptLine, cursors) == QUOTE) {
					fprintf(stderr, "\t\tSeparator = quote\n");
					file->len = cursors[END] - cursors[START];
					file->sep = substringSave(file->sep, scriptLine, cursors);
				}
				else {
					fprintf(stderr, "\t\tSeparator = variable\n");
					file->sep = stringSave(file->sep, vars.dict[findVar(vars, scriptLine, cursors)].val);
					for (file->len = 0; file->sep[file->len] != 0; ++file->len);
				}
			}
			else {
				fprintf(stderr, "\t\tDefault separator = newline\n");
				file->sep = malloc(2 * sizeof(char));
				file->sep[0] = '\n';
				file->sep[1] = 0;
				file->len = 1;
			}

			fprintf(stderr, "\t\tFile declared.  Name = %s Sep = %s Len = %i\n\n", file->key, file->sep, file->len);
			++files.count;
		}
		else if (substringEquals("sep", scriptLine, cursors)){
			fprintf(stderr, "\t\tSEP command\n");
			seps.dict = realloc(seps.dict, (seps.count + 1) * sizeof(struct sepDict));

			struct sepDict *sep = &seps.dict[seps.count];
			sep->key = NULL;
			sep->val = NULL;

			// Get name
			findToken(scriptLine, cursors);
			sep->key = substringSave(sep->key, scriptLine, cursors);

			// Get separator
			if (findToken(scriptLine, cursors) == QUOTE) {
				if (cursors[START] == cursors[END]){
					fprintf(stderr, "\t\tSeparator = empty (treat characters individually)\n");
					sep->val = malloc(sizeof(char));
					sep->val[0] = 0;
					sep->len = 1;
				}
				else {
					fprintf(stderr, "\t\tSeparator = quote\n");
					sep->len = cursors[END] - cursors[START] + 1;
					sep->val = substringSave(sep->val, scriptLine, cursors);
				}
			}
			else if (scriptLine[cursors[START]] != ')'){
				fprintf(stderr, "\t\tSeparator = variable\n");
				sep->val = stringSave(sep->val, vars.dict[findVar(vars, scriptLine, cursors)].val);
			}
			else {
				fprintf(stderr, "\t\tDefault separator = whitespace\n");
			}

			fprintf(stderr, "\t\tSep declared;  name = %s ; sep = %s\n\n", sep->key, sep->val == NULL? "wspace" : sep->val[0] == 0 ? "indv" : sep->val);
			++seps.count;
		}
		else if (substringEquals("in", scriptLine, cursors)){
			fprintf(stderr, "\t\tIN command\n");
			++loops.ptr;
			if (loops.ptr == loops.cap){
				++loops.cap;
				fprintf(stderr, "\t\tAdding loop %i/%i to stack\n", loops.ptr, loops.cap);
				loops.stack = realloc(loops.stack, loops.cap * sizeof(struct loopStruct));
			}
			else fprintf(stderr, "\t\tUsing loop %i/%i of stack\n", loops.ptr, loops.cap);

			struct loopStruct *loop = &loops.stack[loops.ptr];

			// Save cmd
			loop->cmd = ftell(scriptFile);

			// Get type and addr
			findToken(scriptLine, cursors);
			loop->addr = findFile(files, scriptLine, cursors);
			if ( loop->type = (loop->addr == -1) ) {
				fprintf(stderr, "\t\tLoop type = sep (%i %s %s)\n", loop->addr, seps.dict[loop->addr].key, seps.dict[loop->addr].val);
				loop->addr = findSep(seps, scriptLine, cursors);
			}
			else fprintf(stderr, "\t\tLoop type = file (%i %s %s)\n", loop->addr, files.dict[loop->addr].key, files.dict[loop->addr].sep);

			// Get index
			int skip;
			if (scriptLine[cursors[END]] == '['){
				fprintf(stderr, "\t\tIndex is provided\n");
				loop->isLoop = FALSE;
				findToken(scriptLine, cursors);
				//printSubstring(scriptLine, cursors);
				if (scriptLine[cursors[START]] >= 48 && scriptLine[cursors[START]] <= 57) {
					fprintf(stderr, "\t\tIndex = numerical\n");
					skip = substring2Num(scriptLine, cursors);
				}
				else {
					fprintf(stderr, "\t\tIndex = variable name (%i %s %s)\n", findVar(vars, scriptLine, cursors), vars.dict[findVar(vars, scriptLine, cursors)].key, vars.dict[findVar(vars,scriptLine,cursors)].val);
					skip = string2Num(vars.dict[findVar(vars, scriptLine, cursors)].val);
				}
				fprintf(stderr, "\t\tIndex is %i\n", skip);
			}
			else {
				fprintf(stderr, "\t\tNo index provided\n");
				skip = 0;
				loop->isLoop = TRUE;
			}

			// Load SEP
			if (loop->type == SEP){
				struct loopStruct parent = loops.stack[loops.ptr-1];
				loop->buff = parent.buff;

				loop->start = parent.start;
				while (skip-- != 0 && loop->start != -1 && loop->start < parent.stop){
					loop->start = sepSearch(loop->buff, seps.dict[loop->addr].val, loop->start, parent.stop);
					loop->start += seps.dict[loop->addr].len;
				}

				if (loop->start >= parent.stop) {
					fprintf(stderr, "\t\tEnd of buffer found\n\n");
					loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
				}
				else if (loop -> start != -1){
					loop->stop = sepSearch(loop->buff, seps.dict[loop->addr].val, loop->start, parent.stop);
					if (loop->stop == -1) loop->stop = parent.stop;
				}
				else loop->stop = parent.stop;
			}
			// Load FILE
			else {
				fprintf(stderr, "\t\tLoading file\n");
				loop->start = 0;
				loop->buff = NULL;
				do {
					loop->buff = loadFile(loop->buff, files.dict[loop->addr].fp, files.dict[loop->addr].sep, &loop->stop);
				} while (skip-- != 0 && loop->buff != NULL);

				if (loop->buff == NULL){
					fprintf(stderr, "\t\tEnd of file found\n\n");
					loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
				}
				else fprintf(stderr, "\t\tBuffer length is %i\n\n", loop->stop);
			}
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			fprintf(stderr, "\t\tOUT comand\n");
			struct loopStruct *loop = &loops.stack[loops.ptr];
			if (loop->isLoop == FALSE) {
				fprintf(stderr, "\t\tBreaking out of loop\n\n");
				loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
			}
			else {
				if (loop->type == SEP){
					int parentStop = loops.stack[loops.ptr-1].stop;
					if ( (loop->start += seps.dict[loop->addr].len) > parentStop) {
						loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
					}
					else if ( (loop->stop = sepSearch(loop->buff, seps.dict[loop->addr].val, loop->start, parentStop)) == -1) {
						loop->stop = parentStop;
						fseek(scriptFile, loop->cmd, SEEK_SET);
					}
				}
				else if ( (loop->buff = loadFile(loop->buff, files.dict[loop->addr].fp, files.dict[loop->addr].sep, &loop->stop)) == NULL) {
					fprintf(stderr, "\t\tloop type = file\n");
					fprintf(stderr, "\t\tfound end of file\n");
					loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
				}
				else {
					fprintf(stderr, "\t\tloop type = file\n");
					fprintf(stderr, "\t\treturning to beginning of loop\n");
					fseek(scriptFile, loop->cmd, SEEK_SET);
				}
			}
		}
		else {
			fprintf(stderr, "\t\tAssuming variable assignment\n");
			int destIndex = findVar(vars, scriptLine, cursors);
			fprintf(stderr, "\t\tAssigning to '%s'\n", vars.dict[destIndex].key);
			findToken(scriptLine, cursors);
			if (substringEquals("=", scriptLine, cursors)){
				// assignment
				if (findToken(scriptLine, cursors) == QUOTE){
					vars.dict[destIndex].val = substringSave(vars.dict[destIndex].val, scriptLine, cursors);
					fprintf(stderr, "\t\tAssigning quote: '%s'\n\n", vars.dict[destIndex].val);
				}
				else {
					// copying val from variable
					fprintf(stderr, "\t\tAssigning variable val\n");
					int sourceIndex = findVar(vars, scriptLine, cursors);
					fprintf(stderr, "\t\tAssigning val from %s : %s\n\n", vars.dict[sourceIndex].key, vars.dict[sourceIndex].val);
					vars.dict[destIndex].val = stringSave(vars.dict[destIndex].val, vars.dict[sourceIndex].val);
				}
			}
		}
	}
	
	return 0;
}
