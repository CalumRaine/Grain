#include <stdio.h>
#include <stdlib.h>

#define READ_SIZE 500
#define SEP 1

enum pos {START, STOP};
enum tokenType {TERMINATOR, QUOTE, VARIABLE, COMMA, ASSIGNMENT, MATHS};
enum boolean {FALSE, TRUE, PENDING};

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
	int index;
	int bounce;	// 0 =  false; 1 = true; 2 = pending
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

int findToken(char *txt, int *cursors){
	// Moves cursors[START] and cursors[STOP] around next token
	// Returns int representing type of token found

	// START scans from *after* END
	cursors[START] = cursors[STOP] + 1;

	// Skip leading whitespace
	while (txt[cursors[START]] == ' ' || txt[cursors[START]] == '\t') ++cursors[START];

	// Check if terminator, quote or other
	switch (txt[cursors[START]]){
	case '\n':
	case ';':
	case '\0':
		return TERMINATOR;
	case ',':
		cursors[STOP] = cursors[START];
		return COMMA;
	case '=':
		cursors[STOP] = cursors[START];
		return ASSIGNMENT;
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
		cursors[STOP] = cursors[START];
		return MATHS;
	case '\'':
	case '"':
	case '`':
		// Found quote.  Scan to matching quotation mark.
		for (cursors[STOP] = cursors[START] + 1; txt[cursors[STOP]] != txt[cursors[START]] ; ++cursors[STOP]){
			// Check for escape character
			if (txt[cursors[STOP]] == '\\'){
				// Shuffle string left
				for (int i=cursors[STOP], j=cursors[STOP]+1; txt[i]!=0; ++i, ++j) txt[i] = txt[j];
				// Convert escape character
				switch (txt[cursors[STOP]]){
				case 'n':
					txt[cursors[STOP]] = '\n';
					break;
				case 't':
					txt[cursors[STOP]] = '\t';
					break;
				case '\\':
					txt[cursors[STOP]] = '\\';
					break;
				case '\'':
					txt[cursors[STOP]] = '\'';
					break;
				case '"':
					txt[cursors[STOP]] = '"';
					break;
				case '`':
					txt[cursors[STOP]] = '`';
					break;
				}
			}
		}
		++cursors[START];
		return QUOTE;
	default:
		for (cursors[STOP] = cursors[START]+1; txt[cursors[STOP]] != ' ' 
						   && txt[cursors[STOP]] != '\t' 
						   && txt[cursors[STOP]] != '\n' 
					           && txt[cursors[STOP]] != 0 
					           && txt[cursors[STOP]] != '=' 
						   && txt[cursors[STOP]] != ';'
						   && txt[cursors[STOP]] != ','
						   && txt[cursors[STOP]] != '.'
						   && txt[cursors[STOP]] != '('
						   && txt[cursors[STOP]] != ')'
						   && txt[cursors[STOP]] != '['
						   && txt[cursors[STOP]] != ']' ; ++cursors[STOP]);
		return VARIABLE;
	}
}

int substringEquals(char *comp, char *txt, int *cursors){
	//fprintf(stderr, "SUBSTRING EQUALS %p %p %p %i %i\n", comp, txt, cursors, cursors[START], cursors[STOP]);
	// Checks if comp matches txt[START-STOP]
	for (int compPos=0, txtPos=cursors[START]; txtPos < cursors[STOP]; ++compPos, ++txtPos) if (comp[compPos] != txt[txtPos]) return 0;
	return 1;
}

int stringEquals(char *comp, char *txt){
	//fprintf(stderr, "STRING EQUALS %p %p\n", comp, txt);
	// Checks if comp matches txt
	for (int i=0; comp[i] != 0 && txt[i] != 0; ++i) if (comp[i] != txt[i]) return 0;
	return 1;
}

char *substringSave(char *dest, char *source, int *cursors){
	//fprintf(stderr, "SUBSTRING SAVE %p %p %p %i %i", dest, source, cursors, cursors[START], cursors[STOP]);
	// Reallocate dest
	// Copy source[START-STOP] into dest
	dest = realloc(dest, (1 + cursors[STOP] - cursors[START]) * sizeof(char));
	dest[cursors[STOP]-cursors[START]] = 0;
	for (int destPos=0; cursors[START] < cursors[STOP]; ++cursors[START], ++destPos) dest[destPos] = source[cursors[START]]; 
	return dest;
}

char *stringSave(char *dest, char *source){
	//fprintf(stderr, "STRING SAVE %p %p\n", dest, source);
	// Reallocate dest
	// Copy source into dest
	int length;
	for (length = 0; source[length] != 0; ++length);

	dest = realloc(dest, (length+1) * sizeof(char));
	for (int pos=0; pos < length; ++pos) dest[pos] = source[pos];
	dest[length] = 0;

	return dest;
}

int findVar(struct varStruct vars, char *txt, int *cursors){
	//fprintf(stderr, "FIND VAR %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Returns index of var in varDict where key matches txt substring.  Or -1
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	return -1;
}

int findFile(struct fileStruct files, char *txt, int *cursors){
	//fprintf(stderr, "FIND FILE %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Returns index of file in fileDict where key matches txt substring.  Or -1
	for (int f=0; f < files.count; ++f) if (substringEquals(files.dict[f].key, txt, cursors)) return f;
	return -1;
}

int findSep(struct sepStruct seps, char *txt, int *cursors){
	//fprintf(stderr, "FIND SEP %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Returns index of sep in sepDict where key matches txt substring.  Or -1
	for (int s=0; s < seps.count; ++s) if (substringEquals(seps.dict[s].key, txt, cursors)) return s;
	return -1;
}

int substring2Int(char *txt, int *cursors){
	//fprintf(stderr, "SUBSTRING 2 INT %p %p %i %i\n", txt, cursors, cursors[START], cursors[STOP]);
	// Converts txt[START-STOP] to int
	int result = 0;
	for (int i=cursors[START]; i < cursors[STOP]; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int string2Int(char *txt){
	//fprintf(stderr, "STRING 2 INT %s\n", txt);
	// Converts txt to int
	int result=0;
	for (int i=0; txt[i] != 0; ++i){
		result *= 10;
		result += (int)(txt[i] - 48);
	}
	return result;
}

int loadSepStop(char *txt, char *sep, int start, int stop){
	//fprintf(stderr, "LOAD SEP STOP %p %s %i %i\n", txt, sep, start, stop);
	// Returns sep starting position in txt[start-stop]
	for (int i=start, j; i < stop; ++i){
		for (j=0; sep[j] != 0 && txt[i+j] == sep[j]; ++j);
		if (sep[j] == 0) return i;
	}
	return -1;
}

int loadSepStart(char *txt, struct sepDict sep, int index, int from, int to){
	//fprintf(stderr, "LOAD SEP START %p %s %i %i %i\n", txt, sep.val, index, from, to);
	// Skip sep position after index number of seps
	while (index-- && (from = loadSepStop(txt, sep.val, from, to)) != 1) from += sep.len;
	return from;
}

char *loadFile(char *buff, struct fileDict file, int *length, int index){
	//fprintf(stderr, "LOAD FILE %p %s %i\n", buff, file.key, index);
	// Skip index number of file records and load next into buff
	// Saves buff length into length
	int buffCap, lastCap, end, read;
	for (buffCap=0, index+=1; index; buffCap=0, --index){
		do {
			lastCap = buffCap;
			buffCap += READ_SIZE;
			buff = realloc(buff, (buffCap+1) * sizeof(char));
			read = fread(&buff[lastCap], sizeof(char), READ_SIZE, file.fp);
		} while ( (end=loadSepStop(buff, file.sep, lastCap, lastCap+read)) == -1  &&  read == READ_SIZE);

		if (end != -1) fseek(file.fp, (long)(0 - read + end + file.len), SEEK_CUR);
	}

	*length = (end == -1 ? lastCap + read : lastCap + end);

	if (*length == 0){
		free(buff);
		return NULL;
	}
	else {
		buff[*length] = 0;
		return realloc(buff, (1 + *length) * sizeof(char));
	}
}

int breakLoop(struct loopStruct loop, char *scriptLine, FILE *scriptFile){
	//fprintf(stderr, "BREAK LOOP %s %s\n", loop.type == SEP ? "SEP" : "FILE", scriptLine); 
	// Move cursor to next OUT command
	// Free buffer if loop has type FILE

	int cursors[2];
	for (int loopCount=1; loopCount; ){
		fgets(scriptLine, READ_SIZE + 1, scriptFile);
		cursors[STOP] = -1;
		findToken(scriptLine, cursors);
		if (substringEquals("in", scriptLine, cursors)) ++loopCount;
		else if (substringEquals("out", scriptLine, cursors)) --loopCount;
	}
	return 1;
}

void printSubstring(char *txt, int start, int stop){
	//fprintf(stderr, "PRINT SUBSTRING %p %i %i\n", txt, start, stop);
	// Temporarily null terminate substring for printing
	char swap = txt[stop];
	txt[stop] = 0;
	printf("%s", &txt[start]);
	txt[stop] = swap;
}

int token2Int(struct varStruct vars, char *txt, int *cursors){
	//fprintf(stderr, "TOKEN 2 INT %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Convert token to integer.  Either variable or string number.
	return txt[cursors[START]] >= 48 && txt[cursors[START]] <= 57 ? substring2Int(txt, cursors) : string2Int(vars.dict[findVar(vars, txt, cursors)].val);
}

char *int2String(char *txt, int num){
	//fprintf(stderr, "INT 2 STRING %p %i\n", txt, num);
	// Convert integer to string
	int digits = num < 1 ? 1 : 0;
	for (int cpy=num; cpy != 0; cpy/=10, ++digits);

	txt = realloc(txt, (digits+1)*sizeof(char));
	txt[digits]=0;

	if (num < 0) txt[0]='-';

	do {
		txt[--digits] = (num % 10) + 48;
	} while ( (num/=10) != 0);

	return txt;
}

char *substringJoin(char *dest, char *source, int from, int to){
	//fprintf(stderr, "SUBSTRING JOIN %p %p %i %i\n", dest, source, from, to);
	// Join source[from-to] to end of dest
	int len=0;
	while (dest[len++] != 0);
	
	dest = realloc(dest, (len + to - from + 1) * sizeof(char));
	for (int dPos=len, sPos=from; sPos < to; ++dPos, ++sPos) dest[dPos] = source[sPos];
	dest[len + to - from] = 0;
	return dest;
}

char *stringJoin(char *dest, char *src){
	//fprintf(stderr, "STRING JOIN %p %p : %s %s\n", dest, src, dest, src);
	// Append src to dest
	int dLen=0, sLen=0;
	while (dest[dLen++] != 0);
	while (src[sLen++] != 0);

	dest = realloc(dest, (dLen + sLen + 1) * sizeof(char));
	for (int dPos=dLen, sPos=0; src[sPos]!=0; ++dPos, ++sPos) dest[dPos] = src[sPos];
	dest[dLen + sLen] = 0;
	return dest;
}

char *varStrAss(struct varStruct vars, struct sepStruct seps, struct fileStruct files, struct loopStack loops, char *scriptLine, int *cursors){
	//fprintf(stderr, "VAR STR ASS %i %s %i %i\n", loops.ptr, scriptLine, cursors[START], cursors[STOP]);
	// Assign multiple concatenated strings to a variable
	char *buff = NULL;
	for (int status=findToken(scriptLine, cursors), addr, start, stop; status != TERMINATOR && status != COMMA; status = scriptLine[cursors[STOP]] == ',' ? COMMA : findToken(scriptLine, cursors)){
		if (status == QUOTE || scriptLine[cursors[START]] >= 48 && scriptLine[cursors[START]] <= 57) buff = substringJoin(buff, scriptLine, cursors[START], cursors[STOP]);
		else if (scriptLine[cursors[STOP]] == '['){
			if ( (addr=findSep(seps, scriptLine, cursors)) != -1 ) {
				findToken(scriptLine, cursors);
				start = loadSepStart(loops.stack[loops.ptr].buff, seps.dict[addr], token2Int(vars, scriptLine, cursors), loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
				stop = loadSepStop(loops.stack[loops.ptr].buff, seps.dict[addr].val, start, loops.stack[loops.ptr].stop);
				if (stop == -1) stop = loops.stack[loops.ptr].stop;
				buff = substringJoin(buff, loops.stack[loops.ptr].buff, start, stop);
			}
			else if ( (addr=findFile(files, scriptLine, cursors)) != -1 ){
				findToken(scriptLine, cursors);
				char *string = loadFile(NULL, files.dict[addr], &start, token2Int(vars, scriptLine, cursors));
				buff = stringJoin(buff, string);
				free(string);
			}
		}
		else buff = stringJoin(buff, vars.dict[findVar(vars, scriptLine, cursors)].val);
	}
	return buff;
}

struct loopStack loadLoop(struct loopStack loops, struct sepStruct seps, struct fileStruct files, char *scriptLine, FILE *scriptFile);
struct loopStack resetLoop(struct loopStack loops, struct sepStruct seps, struct fileStruct files, char *scriptLine, FILE *scriptFile){
	// Setup loopStruct cursors 
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];
	//fprintf(stderr, "RESET LOOP loops.ptr %i type %i\n", loops.ptr, loop->type);
	loop->buff = parent->buff;

	loop->start = loop->index == -1 ? parent->start : loadSepStart(loop->buff, seps.dict[loop->addr], loop->index, parent->start, parent->stop);
	if (loop->start == -1){
		if (loop->type != SEP) free(loop->buff);
		return --loops.ptr == -1 ? loops : loadLoop(loops, seps, files, scriptLine, scriptFile);
	}
	else if ( (loop->stop = loadSepStop(loop->buff, seps.dict[loop->addr].val, loop->start, parent->stop)) == -1) loop->stop = parent->stop;

	if (loop->bounce == TRUE){
		++loops.ptr;
		return resetLoop(loops, seps, files, scriptLine, scriptFile);
	}
	else if (loop->bounce == PENDING) loop->bounce = FALSE;

	return loops;
}

struct loopStack loadLoop(struct loopStack loops, struct sepStruct seps, struct fileStruct files, char *scriptLine, FILE *scriptFile){
	// Advance loopStruct cursors
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];
	//fprintf(stderr, "LOAD LOOP loops.ptr %i type %i\n", loops.ptr, loop->type);

	if (loop->isLoop == FALSE) {
		if (loop->bounce == FALSE) loop->bounce = PENDING;
		return --loops.ptr == -1 ? loops : loadLoop(loops, seps, files, scriptLine, scriptFile);
	}
	else if (loop->type == SEP){
		loop->start = loop->stop + seps.dict[loop->addr].len;
		if (loop->start > parent->stop) {
			if (loop->bounce == FALSE) loop->bounce = PENDING;
			return --loops.ptr == -1 ? loops : loadLoop(loops, seps, files, scriptLine, scriptFile);
		}
		else if ( (loop->stop = loadSepStop(loop->buff, seps.dict[loop->addr].val, loop->start, parent->stop)) == -1) loop->stop = parent->stop;
		switch (loop->bounce){
		case TRUE:
			++loops.ptr;
			return resetLoop(loops, seps, files, scriptLine, scriptFile);
		case PENDING:
			loop->bounce = FALSE;
		default:
			return loops;
		}
	}
	else if ( (loop->buff = loadFile(loop->buff, files.dict[loop->addr], &loop->stop, 0)) == NULL) {
		if (loop->bounce == FALSE) loop->bounce = PENDING;
		return --loops.ptr == -1 ? loops : loadLoop(loops, seps, files, scriptLine, scriptFile);
	}
	else switch (loop->bounce){
	case TRUE:
		++loops.ptr;
		return resetLoop(loops, seps, files, scriptLine, scriptFile);
	case PENDING:
		loop->bounce = FALSE;
	default:
		return loops;
	}
}

int main(int argc, char **argv){
	struct varStruct vars = { .count = 0, .dict = NULL };
	struct sepStruct seps = { .count = 0, .dict = NULL };
	struct fileStruct files = { .count = 0, .dict = NULL};
	struct loopStack loops = { .ptr = -1, .cap = 0, .stack = NULL};

	char scriptLine[READ_SIZE + 1];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, READ_SIZE + 1, scriptFile) ; !feof(scriptFile); fgets(scriptLine, READ_SIZE + 1, scriptFile) ) {
		//fprintf(stderr, "\t\t*INPUT LINE*: %s", scriptLine);
		int cursors[2] = {0, -1};
		if (!findToken(scriptLine, cursors)) continue;
		else if (substringEquals("var", scriptLine, cursors)){
			//fprintf(stderr, "\t\tVAR command\n");
			do {
				findToken(scriptLine, cursors);
				vars.dict = realloc(vars.dict, (vars.count + 1) * sizeof(struct varDict));
				struct varDict *var = &vars.dict[vars.count];
				var->val = NULL;
				var->key = substringSave(NULL, scriptLine, cursors);
				++vars.count;
				if (scriptLine[cursors[STOP]] == '=' || scriptLine[cursors[STOP]] != ',' && findToken(scriptLine, cursors) == ASSIGNMENT){
					var->val = varStrAss(vars, seps, files, loops, scriptLine, cursors);
				}
			} while (scriptLine[cursors[START]] == ',' || scriptLine[cursors[STOP]] == ',');
		}
		else if (substringEquals("print", scriptLine, cursors)){
			//fprintf(stderr, "\t\tPRINT command\n");
			int status;
			do {
				status = findToken(scriptLine, cursors);
				switch (status){
				case VARIABLE:
					if (scriptLine[cursors[STOP]] == '['){
						int addr;
						if ( (addr=findSep(seps, scriptLine, cursors)) != -1 ) {
							findToken(scriptLine, cursors);
							int start = loadSepStart(loops.stack[loops.ptr].buff, seps.dict[addr], token2Int(vars, scriptLine, cursors), loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
							int stop = loadSepStop(loops.stack[loops.ptr].buff, seps.dict[addr].val, start, loops.stack[loops.ptr].stop);
							if (stop == -1) stop = loops.stack[loops.ptr].stop;
							printSubstring(loops.stack[loops.ptr].buff, start, stop);
						}
						else {
							findToken(scriptLine, cursors);
							char *string = loadFile(NULL, files.dict[findFile(files, scriptLine, cursors)], &addr, token2Int(vars, scriptLine, cursors));
							printf("%s", string);
							free(string);
						}
					}
					else printf("%s", vars.dict[findVar(vars, scriptLine, cursors)].val);
					break;
				case QUOTE:
					printSubstring(scriptLine, cursors[START], cursors[STOP]);
					break;
				}
			} while (status != TERMINATOR);
		}
		else if (substringEquals("file", scriptLine, cursors)){
			//fprintf(stderr, "\t\tFILE command\n");

			// Get name
			findToken(scriptLine, cursors);
			files.dict = realloc(files.dict, (files.count + 1) * sizeof(struct fileDict));

			struct fileDict *file = &files.dict[files.count];
			file->key = substringSave(NULL, scriptLine, cursors);

			// Get filename
			if (findToken(scriptLine, cursors) == QUOTE) {
				char swap = scriptLine[cursors[STOP]];
				scriptLine[cursors[STOP]] = 0;
				file->fp = fopen(&scriptLine[cursors[START]], "r");
				scriptLine[cursors[STOP]] = swap;
			}
			else file->fp = fopen(vars.dict[findVar(vars, scriptLine, cursors)].val, "r");

			// Get separator
			if (scriptLine[cursors[STOP]] == ',' || findToken(scriptLine, cursors) == COMMA){
				if (findToken(scriptLine, cursors) == QUOTE) {
					file->len = cursors[STOP] - cursors[START];
					file->sep = substringSave(NULL, scriptLine, cursors);
				}
				else {
					file->sep = stringSave(NULL, vars.dict[findVar(vars, scriptLine, cursors)].val);
					for (file->len = 0; file->sep[file->len] != 0; ++file->len);
				}
			}
			else {
				file->len = 1;
				file->sep = malloc(2 * sizeof(char));
				file->sep[0] = '\n';
				file->sep[1] = 0;
			}

			++files.count;
		}
		else if (substringEquals("sep", scriptLine, cursors)){
			//fprintf(stderr, "\t\tSEP command\n");
			seps.dict = realloc(seps.dict, (seps.count + 1) * sizeof(struct sepDict));

			struct sepDict *sep = &seps.dict[seps.count];

			// Get name
			findToken(scriptLine, cursors);
			sep->key = substringSave(NULL, scriptLine, cursors);

			// Get separator
			if (findToken(scriptLine, cursors) == QUOTE) {
				if (cursors[START] == cursors[STOP]){
					sep->val = malloc(sizeof(char));
					sep->val[0] = 0;
					sep->len = 1;
				}
				else {
					sep->len = cursors[STOP] - cursors[START];
					sep->val = substringSave(NULL, scriptLine, cursors);
				}
			}
			else if (scriptLine[cursors[START]] != ')'){
				sep->val = stringSave(NULL, vars.dict[findVar(vars, scriptLine, cursors)].val);
			}
			else {
				sep->len = 1;
				sep->val = malloc(2 * sizeof(char));
				sep->val[0] = ' ';
				sep->val[1] = 0;
			}

			++seps.count;
		}
		else if (substringEquals("in", scriptLine, cursors)){
			//fprintf(stderr, "\t\tIN command\n");
			do {
				//fprintf(stderr, "\t\t\tIteration\n");
				if (++loops.ptr == loops.cap){
					++loops.cap;
					loops.stack = realloc(loops.stack, loops.cap * sizeof(struct loopStruct));
				}

				struct loopStruct *loop = &loops.stack[loops.ptr];

				// Save cmd
				loop->cmd = ftell(scriptFile);

				// Get type and addr
				findToken(scriptLine, cursors);
				loop->addr = findFile(files, scriptLine, cursors);
				if ( loop->type = (loop->addr == -1) ) loop->addr = findSep(seps, scriptLine, cursors);

				// Get index
				if (scriptLine[cursors[STOP]] == '['){
					loop->isLoop = FALSE;
					findToken(scriptLine, cursors);
					loop->index = token2Int(vars, scriptLine, cursors);
				}
				else {
					loop->isLoop = TRUE;
					loop->index = loop->type == SEP? -1 : 0;
				}

				// Load SEP
				if (loop->type == SEP){
					struct loopStruct parent = loops.stack[loops.ptr-1];
					loop->buff = parent.buff;

					// This is bad code
					loop->start = loop->index == -1 ? parent.start : loadSepStart(loop->buff, seps.dict[loop->addr], loop->index, parent.start, parent.stop);
					if (loop->start != -1 && (loop->stop = loadSepStop(loop->buff, seps.dict[loop->addr].val, loop->start, parent.stop)) == -1){
						loop->stop = parent.stop;
					}
					else if (loop->start == -1){
						loops.ptr -= breakLoop(*loop, scriptLine, scriptFile);
					}
				}

				// Load FILE
				else {
					loop->buff = loadFile(NULL, files.dict[loop->addr], &loop->stop, loop->index);

					if (loop->buff == NULL){
						loops.ptr -= breakLoop(*loop, scriptLine, scriptFile);
					}
					else {
						loop->start = 0;
					}
				}
			} while ( loops.stack[loops.ptr].bounce = (scriptLine[cursors[STOP]] == '.') );
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			//fprintf(stderr, "\t\tOUT comand\n");
			loops = loadLoop(loops, seps, files, scriptLine, scriptFile);
			if (loops.ptr != -1) fseek(scriptFile, loops.stack[loops.ptr].cmd, SEEK_SET);
		}
		else {
			//fprintf(stderr, "\t\tASS\n");
			int status, destIndex = findVar(vars, scriptLine, cursors);
			if (scriptLine[cursors[STOP]] == '-' || (status=findToken(scriptLine, cursors)) == ASSIGNMENT){
				//fprintf(stderr, "\t\tString mode\n");
				findToken(scriptLine, cursors);
				char *newVar = varStrAss(vars, seps, files, loops, scriptLine, cursors);
				free(vars.dict[destIndex].val);
				vars.dict[destIndex].val = newVar;
			}
			else if (status == MATHS){
				//fprintf(stderr, "\t\tMaths mode\n");
				int result = string2Int(vars.dict[destIndex].val);
				do {
					switch(scriptLine[cursors[START]]){
					case '+':
						findToken(scriptLine, cursors);
						result += token2Int(vars, scriptLine, cursors);
						break;
					case '-':
						findToken(scriptLine, cursors);
						result -= token2Int(vars, scriptLine, cursors);
						break;
					case '*':
						findToken(scriptLine, cursors);
						result *= token2Int(vars, scriptLine, cursors);
						break;
					case '/':
						findToken(scriptLine, cursors);
						result /= token2Int(vars, scriptLine, cursors);
						break;
					case '%':
						findToken(scriptLine, cursors);
						result %= token2Int(vars, scriptLine, cursors);
						break;
					}
				} while (findToken(scriptLine, cursors) != TERMINATOR);
				int2String(vars.dict[destIndex].val, result);
			}
		}
	}
	
	return 0;
}
