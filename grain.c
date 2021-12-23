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
	fprintf(stderr, "\t\t\tFINDVARS called\n");
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) {
		fprintf(stderr, "\t\t\tVariable found at %i\n", v);
		return v;
	}
	fprintf(stderr, "\t\t\tVariable not found\n");
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
	fprintf(stderr, "\t\t\tSUBSTRING2NUM called\n");
	// Converts string in txt to integer
	int result = 0;
	for (int i=cursors[START]; i < cursors[END]; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int string2Num(char *txt){
	fprintf(stderr, "\t\t\tSTRING2NUM called\n");
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

int skipSep(char *txt, struct sepDict sep, int index, int cursor, int to){
	fprintf(stderr, "\t\t\tSKIPSEP called: index %i cursor %i to %i\n", index, cursor, to);
	while (index-- && (cursor = sepSearch(txt, sep.val, cursor, to)) != 1) cursor += sep.len;
	fprintf(stderr, "\t\t\tReturning %i\n", cursor);
	return cursor;
}

char *loadFile(char *buff, struct fileDict file, int *length, int index){
	fprintf(stderr, "\t\t\tLOADFILE called: index %i\n", index);
	int buffCap, lastCap, end, read;
	for (buffCap=0, index+=1; index; buffCap=0, --index){
		fprintf(stderr, "\t\t\titeration %i\n", index);
		do {
			lastCap = buffCap;
			buffCap += READ_SIZE;
			buff = realloc(buff, (buffCap+1) * sizeof(char));
			read = fread(&buff[lastCap], sizeof(char), READ_SIZE, file.fp);
			fprintf(stderr, "\t\t\tlastCap %i buffCap %i read %i\n", lastCap, buffCap, read);
		} while ( (end=sepSearch(buff, file.sep, lastCap, lastCap+read)) == -1  &&  read == READ_SIZE);

		if (end != -1) fseek(file.fp, (long)(0 - read + end + file.len), SEEK_CUR);
	}

	*length = (end == -1 ? lastCap + read : lastCap + end);

	if (*length == 0){
		fprintf(stderr, "\t\t\tLength is 0.  Return NULL\n");
		free(buff);
		return NULL;
	}
	else {
		fprintf(stderr, "\t\t\tLength is %i\n", *length);
		buff[*length] = 0;
		fprintf(stderr, "\t\t\tBuffer = %s\n", buff);
		return realloc(buff, (1 + *length) * sizeof(char));
	}
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

void printSubstring(char *txt, int start, int stop){
	fprintf(stderr, "\t\t\tPRINTSUBSTRING called: %p %i %i\n", txt, start, stop);
	char swap = txt[stop];
	txt[stop] = 0;
	fprintf(stderr, "\t\t\t%s\n", &txt[start]);
	printf("%s", &txt[start]);
	txt[stop] = swap;
}

int index2Int(struct varStruct vars, char *txt, int *cursors){
	fprintf(stderr, "\t\t\tINDEX2INT called\n");
	return txt[cursors[START]] >= 48 && txt[cursors[START]] <= 57 ? substring2Num(txt, cursors) : string2Num(vars.dict[findVar(vars, txt, cursors)].val);
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
			int status;
			do {
				status = findToken(scriptLine, cursors);
				fprintf(stderr, "\t\ttoken status is %i\n", status);
				switch (status){
				//case TERMINATOR:
					//fprintf(stderr, "\t\tPrint current buffer\n\n");
					//if (loops.stack[loops.ptr].type == SEP) printSubstring(loops.stack[loops.ptr].buff, loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
					//else printf("%s", loops.stack[loops.ptr].buff);
				//	break;
				case VARIABLE:
					fprintf(stderr, "\t\tPrint variable or sep or file\n");
					int addr;
					if (scriptLine[cursors[END]] == '['){
						fprintf(stderr, "\t\tIndex provided.  Must be sep or file\n");
						if ( (addr=findSep(seps, scriptLine, cursors)) != -1 ) {
							fprintf(stderr, "\t\tSep found (index=%i name=%s sep=%s)\n", addr, seps.dict[addr].key, seps.dict[addr].val);
							findToken(scriptLine, cursors);
							int index = index2Int(vars, scriptLine, cursors);
							fprintf(stderr, "\t\tIndex is %i\n", index);
							char *buff = loops.stack[loops.ptr].buff;
							int stop, start = skipSep(buff, seps.dict[addr], index, loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
							if (start == -1){
								fprintf(stderr, " does not exist\n");
								exit(0);
							}
							else if ( (stop = sepSearch(buff, seps.dict[addr].val, start, loops.stack[loops.ptr].stop)) == -1) stop = cursors[END];
							printSubstring(buff, start, stop);
						}
						else if ( (addr=findFile(files, scriptLine, cursors)) != -1 ){
							fprintf(stderr, "\t\tFile found (index=%i name=%s sep=%s)\n", addr, files.dict[addr].key, files.dict[addr].sep);
							findToken(scriptLine, cursors);
							int index = index2Int(vars, scriptLine, cursors);
							fprintf(stderr, "\t\tIndex is %i\n", index);
							char *string = loadFile(NULL, files.dict[addr], &index, index); // &index is naughty
							printf("%s\n", string);
							free(string);
						}
						else {
							printSubstring(scriptLine, cursors[START], cursors[END]);
							fprintf(stderr, " does not exist\n");
							exit(0);
						}
					}
					else if ( (addr=findVar(vars, scriptLine, cursors)) == -1) {
						printSubstring(scriptLine, cursors[START], cursors[END]);
						fprintf(stderr, " does not exist\n");
						exit(0);
					}
					else {
						fprintf(stderr, "\t\tPrint variable\n\n");
						printf("%s", vars.dict[findVar(vars, scriptLine, cursors)].val);
					}
					break;
				case QUOTE:
					fprintf(stderr, "\t\tPrint quotation\n\n");
					printSubstring(scriptLine, cursors[START], cursors[END]);
					break;
				}
			} while (status != TERMINATOR);
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
					sep->len = cursors[END] - cursors[START];
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
				loop->addr = findSep(seps, scriptLine, cursors);
				fprintf(stderr, "\t\tLoop type = sep (index=%i name=%s sep=%s)\n", loop->addr, seps.dict[loop->addr].key, seps.dict[loop->addr].val);
			}
			else fprintf(stderr, "\t\tLoop type = file (%i %s %s)\n", loop->addr, files.dict[loop->addr].key, files.dict[loop->addr].sep);

			// Get index
			int index;
			if (scriptLine[cursors[END]] == '['){
				fprintf(stderr, "\t\tIndex is provided\n");
				loop->isLoop = FALSE;
				findToken(scriptLine, cursors);
				index = index2Int(vars, scriptLine, cursors);
				fprintf(stderr, "\t\tIndex is %i\n", index);
			}
			else {
				fprintf(stderr, "\t\tNo index provided\n");
				index = 0;
				loop->isLoop = TRUE;
			}

			// Load SEP
			if (loop->type == SEP){
				fprintf(stderr, "\t\tLoop type = SEP\n");
				struct loopStruct parent = loops.stack[loops.ptr-1];
				loop->buff = parent.buff;
				fprintf(stderr, "\t\tLoop buff = %p\n", loop->buff);

				// This is bad code
				loop->start = index ? skipSep(loop->buff, seps.dict[loop->addr], index, parent.start, parent.stop) : parent.start;
				if (loop->start != -1 && (loop->stop = sepSearch(loop->buff, seps.dict[loop->addr].val, loop->start, parent.stop)) == -1){
					fprintf(stderr, "\t\tSep not found.  Scanning to end of parent buffer\n\n");
					loop->stop = parent.stop;
				}
				else if (loop->start == -1){
					fprintf(stderr, "\t\tEnd of buffer found\n\n");
					loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
				}
			}
			// Load FILE
			else {
				fprintf(stderr, "\t\tLoading file\n");
				loop->buff = loadFile(NULL, files.dict[loop->addr], &loop->stop, index);

				if (loop->buff == NULL){
					fprintf(stderr, "\t\tEnd of file found\n\n");
					loops.ptr -= breakFun(*loop, scriptLine, scriptFile);
				}
				else {
					loop->start = 0;
					fprintf(stderr, "\t\tBuffer %p length is %i\n\n", loop->buff, loop->stop);
				}
			}
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			fprintf(stderr, "\t\tOUT comand\n");
			struct loopStruct *loop = &loops.stack[loops.ptr];
			if (loop->isLoop == FALSE) {
				fprintf(stderr, "\t\tisLoop = FALSE.  Breaking out of loop\n\n");
				--loops.ptr;
			}
			else if (loop->type == SEP){
				fprintf(stderr, "\t\tLoop type = SEP\n");
				int parentStop = loops.stack[loops.ptr-1].stop;
				loop->start = loop->stop + seps.dict[loop->addr].len;
				if (loop->start > parentStop) {
					fprintf(stderr, "\t\tEnd of buffer found\n");
					--loops.ptr;
				}
				else if ( (loop->stop = sepSearch(loop->buff, seps.dict[loop->addr].val, loop->start, parentStop)) == -1) {
					fprintf(stderr, "\t\tSep not found, scan to end of parent\n");
					fprintf(stderr, "\t\tReturning to beginning of loop\n\n");
					loop->stop = parentStop;
					fseek(scriptFile, loop->cmd, SEEK_SET);
				}
				else {
					fprintf(stderr, "\t\tSep found.  Scanning from %i to %i\n", loop->start, loop->stop);
					fprintf(stderr, "\t\tReturning to beginning of loop\n\n");
					fseek(scriptFile, loop->cmd, SEEK_SET);
				}

			}
			else if ( (loop->buff = loadFile(loop->buff, files.dict[loop->addr], &loop->stop, 0)) == NULL) {
				fprintf(stderr, "\t\tloop type = file\n");
				fprintf(stderr, "\t\tfound end of file\n");
				free(loop->buff);
				--loops.ptr;
			}
			else {
				fprintf(stderr, "\t\tloop type = file\n");
				fprintf(stderr, "\t\tReturning to beginning of loop\n\n");
				fseek(scriptFile, loop->cmd, SEEK_SET);
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
