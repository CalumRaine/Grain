#include <stdio.h>
#include <stdlib.h>

#define READ_SIZE 500
#define SEP 1

enum pos {START, STOP};
enum tokenType {TERMINATOR, QUOTE, VARIABLE, COMMA, ASSIGNMENT, MATHS};
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
} files;

struct varDict {
	char *key;
	char *val; 
};

struct varStruct {
	int count;
	struct varDict *dict;
} vars;

struct sepDict {
	char *key;
	char *val;
	int len;
};

struct sepStruct {
	int count;
	struct sepDict *dict;
} seps;

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
} loops;

int findToken(char *txt, int *cursors){
	//fprintf(stderr, "FIND TOKEN %p %i %i\n", txt, cursors[START], cursors[STOP]);
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
	int compPos=0;
	for (int txtPos=cursors[START]; txtPos < cursors[STOP]; ++compPos, ++txtPos) if (comp[compPos] != txt[txtPos]) return 0;
	return comp[compPos] == 0;
}

int stringEquals(char *comp, char *txt){
	//fprintf(stderr, "STRING EQUALS %p %p\n", comp, txt);
	// Checks if comp matches txt
	for (int i=0; comp[i] != 0 && txt[i] != 0; ++i) if (comp[i] != txt[i]) return 0;
	return 1;
}

char *substringSave(char *dest, char *source, int *cursors){
	//fprintf(stderr, "SUBSTRING SAVE %p %p %p %i %i\n", dest, source, cursors, cursors[START], cursors[STOP]);
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

int findVar(char *txt, int *cursors){
	//fprintf(stderr, "FIND VAR %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Returns index of var in varDict where key matches txt substring.  Or -1
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	return -1;
}

int findFile(char *txt, int *cursors){
	//fprintf(stderr, "FIND FILE %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Returns index of file in fileDict where key matches txt substring.  Or -1
	for (int f=0; f < files.count; ++f) if (substringEquals(files.dict[f].key, txt, cursors)) return f;
	return -1;
}

int findSep(char *txt, int *cursors){
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

int token2Int(char *txt, int *cursors){
	//fprintf(stderr, "TOKEN 2 INT %p %i %i\n", txt, cursors[START], cursors[STOP]);
	// Convert token to integer.  Either variable or string number.
	return txt[cursors[START]] >= 48 && txt[cursors[START]] <= 57 ? substring2Int(txt, cursors) : string2Int(vars.dict[findVar(txt, cursors)].val);
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

char *substringJoin(char *dest, char *src, int from, int to){
	//fprintf(stderr, "SUBSTRING JOIN %p %p %i %i\n", dest, src, from, to);
	// Join source[from-to] to end of dest
	int len=0;
	if (dest!=NULL) while (dest[len++] != 0);
	
	dest = realloc(dest, (len + to - from + 1) * sizeof(char));
	for (int dPos=len, sPos=from; sPos < to; ++dPos, ++sPos) dest[dPos] = src[sPos];
	dest[len + to - from] = 0;
	return dest;
}

char *stringJoin(char *dest, char *src){
	//fprintf(stderr, "STRING JOIN %p %p : %s %s\n", dest, src, dest, src);
	// Append src to dest
	int dLen=0, sLen=0;
	if (dest!=NULL) while (dest[dLen++] != 0);
	while (src[sLen++] != 0);

	dest = realloc(dest, (dLen + sLen + 1) * sizeof(char));
	for (int dPos=dLen, sPos=0; src[sPos]!=0; ++dPos, ++sPos) dest[dPos] = src[sPos];
	dest[dLen + sLen] = 0;
	return dest;
}

char *varStrAss(char *scriptLine, int *cursors){
	//fprintf(stderr, "VAR STR ASS %i %i %i %s\n", loops.ptr, cursors[START], cursors[STOP], scriptLine);
	// Assign multiple concatenated strings to a variable
	char *buff = NULL;
	for (int status=findToken(scriptLine, cursors), addr, start, stop; status != TERMINATOR && status != COMMA; status = scriptLine[cursors[STOP]] == ',' ? COMMA : findToken(scriptLine, cursors)){
		if (status == QUOTE || scriptLine[cursors[START]] >= 48 && scriptLine[cursors[START]] <= 57) buff = substringJoin(buff, scriptLine, cursors[START], cursors[STOP]);
		else if (scriptLine[cursors[STOP]] == '['){
			if ( (addr=findSep(scriptLine, cursors)) != -1 ) {
				findToken(scriptLine, cursors);
				start = loadSepStart(loops.stack[loops.ptr].buff, seps.dict[addr], token2Int(scriptLine, cursors), loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
				stop = loadSepStop(loops.stack[loops.ptr].buff, seps.dict[addr].val, start, loops.stack[loops.ptr].stop);
				if (stop == -1) stop = loops.stack[loops.ptr].stop;
				buff = substringJoin(buff, loops.stack[loops.ptr].buff, start, stop);
			}
			else {
				addr = findFile(scriptLine, cursors);
				findToken(scriptLine, cursors);
				char *string = loadFile(NULL, files.dict[addr], &start, token2Int(scriptLine, cursors));
				buff = stringJoin(buff, string);
				free(string);
			}
		}
		else buff = stringJoin(buff, vars.dict[findVar(scriptLine, cursors)].val);
	}
	return buff;
}

struct loopStack loadLoop(char *scriptLine, FILE *scriptFile);
struct loopStack resetLoop(char *scriptLine, FILE *scriptFile){
	// Setup loopStruct cursors 
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];
	//fprintf(stderr, "RESET LOOP loops.ptr %i type %i\n", loops.ptr, loop->type);
	loop->buff = parent->buff;

	loop->start = loop->index == -1 ? parent->start : loadSepStart(loop->buff, seps.dict[loop->addr], loop->index, parent->start, parent->stop);
	if (loop->start == -1){
		if (loop->type != SEP) free(loop->buff);
		return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
	}
	else if ( (loop->stop = loadSepStop(loop->buff, seps.dict[loop->addr].val, loop->start, parent->stop)) == -1) loop->stop = parent->stop;

	if (loop->bounce == TRUE){
		++loops.ptr;
		return resetLoop(scriptLine, scriptFile);
	}

	return loops;
}

struct loopStack loadLoop(char *scriptLine, FILE *scriptFile){
	// Advance loopStruct cursors
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];
	//fprintf(stderr, "LOAD LOOP loops.ptr %i type %i\n", loops.ptr, loop->type);

	if (loop->isLoop == FALSE) return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
	else if (loop->type == SEP){
		if ( (loop->start = loop->stop + seps.dict[loop->addr].len) > parent->stop) 
			return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
		else if ( (loop->stop = loadSepStop(loop->buff, seps.dict[loop->addr].val, loop->start, parent->stop)) == -1) 
			loop->stop = parent->stop;
	}
	else if ( (loop->buff = loadFile(loop->buff, files.dict[loop->addr], &loop->stop, 0)) == NULL) {
		return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
	}
	
	if (loop->bounce){
		++loops.ptr;
		return resetLoop(scriptLine, scriptFile);
	}
	else return loops;
}

int retrieveToken(int *outCurs, char **outTxt, char *scriptLine, int *cursors){
	//fprintf(stderr, "RETRIEVE TOKEN called: %i %i %p %p %i %i\n", outCurs[START], outCurs[STOP], outTxt, scriptLine, cursors[START], cursors[STOP]);
	switch (findToken(scriptLine, cursors)){
	case QUOTE:
		outCurs[START] = cursors[START];
		outCurs[STOP] = cursors[STOP];
		*outTxt = scriptLine;
		return FALSE;
	case VARIABLE:
		if (scriptLine[cursors[STOP]] == '['){
			int addr;
			if ( (addr=findSep(scriptLine, cursors)) != -1 ) {
				findToken(scriptLine, cursors);
				outCurs[START] = loadSepStart(loops.stack[loops.ptr].buff, seps.dict[addr], token2Int(scriptLine, cursors), loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
				outCurs[STOP] = loadSepStop(loops.stack[loops.ptr].buff, seps.dict[addr].val, outCurs[START], loops.stack[loops.ptr].stop);
				if (outCurs[STOP] == -1) outCurs[STOP] = loops.stack[loops.ptr].stop;
				*outTxt = loops.stack[loops.ptr].buff;
				return FALSE;
			}
			else {
				findToken(scriptLine, cursors);
				outCurs[START] = -1;
				*outTxt = loadFile(NULL, files.dict[findFile(scriptLine, cursors)], &addr, token2Int(scriptLine, cursors));
				return TRUE;
			}
		}
		else {
			outCurs[START] = -1;
			*outTxt = vars.dict[findVar(scriptLine,cursors)].val;
			return FALSE;
		}
	}
}

int substringIsNum(char *txt, int from, int to){
	//fprintf(stderr, "SUBSTRING IS NUM %p %i %i\n", txt, from, to);
	for (  ; from < to; ++from) if (txt[from] < 48 || txt[from] > 57) return FALSE;
	return TRUE;
}

int stringIsNum(char *txt){
	//fprintf(stderr, "STRING IS NUM %p\n", txt);
	for (int pos=0; txt[pos]!=0; ++pos) if (txt[pos] < 48 || txt[pos] > 57) return FALSE;
	return TRUE;
}

int compareTokens(char *txtA, int *cursA, char *txtB, int *cursB){
	//fprintf(stderr, "COMPARE TOKENS: %p %i %i %p %i %i\n", txtA, cursA[START], cursA[STOP], txtB, cursB[START], cursB[STOP]);
	int aIsNum = cursA[START] == -1 ? stringIsNum(txtA) : substringIsNum(txtA, cursA[START], cursA[STOP]);
	int bIsNum = cursB[START] == -1 ? stringIsNum(txtB) : substringIsNum(txtB, cursB[START], cursB[STOP]);
	if (aIsNum && bIsNum){
		int a = cursA[START] == -1 ? string2Int(txtA) : substring2Int(txtA, cursA);
		int b = cursB[START] == -1 ? string2Int(txtB) : substring2Int(txtB, cursB);
		return a - b;
	}
	else {
		int result;
		for(int aPos=cursA[START] == -1 ? 0 : cursA[START], bPos=cursB[START] == -1 ? 0 : cursB[START]  ; 
			(cursA[START] == -1 && txtA[aPos]!=0 || cursA[START] != -1 && aPos < cursA[STOP]) 
		     && (cursB[START] == -1 && txtB[bPos]!=0 || cursB[START] != -1 && bPos < cursB[STOP]) 
		     && (result = txtA[aPos]-txtB[bPos]) == 0 ; ++aPos, ++bPos); 
		return result;
	}
}

int comparator(char *scriptLine, int *cursors){
	//fprintf(stderr, "COMPARATOR called %p %i %i\n", scriptLine, cursors[START], cursors[STOP]);
	int cursA[2], cursB[2];
	char *txtA, *txtB;
	
	int freeA = retrieveToken(cursA, &txtA, scriptLine, cursors);

	int opCurs;
	for (opCurs = cursors[STOP]+1; scriptLine[opCurs] == ' ' || scriptLine[opCurs] == '\t'; ++opCurs);
	for (cursors[STOP] = opCurs+1; scriptLine[cursors[STOP]] != ' ' && scriptLine[cursors[STOP]] != '\t'; ++cursors[STOP]);

	int freeB = retrieveToken(cursB, &txtB, scriptLine, cursors);

	int result = compareTokens(txtA, cursA, txtB, cursB);
	if (freeA) free(txtA);
	if (freeB) free(txtB);

	int andFlag=FALSE, orFlag=FALSE;
	if ( findToken(scriptLine, cursors) != TERMINATOR && (andFlag=substringEquals("and", scriptLine, cursors)) == FALSE ) orFlag = substringEquals("or", scriptLine, cursors);

	if (scriptLine[opCurs] == '=') {
		if (result == 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
		else return orFlag ? comparator(scriptLine, cursors) : FALSE;
	}
	else if (scriptLine[opCurs] == '<') {
		if (scriptLine[opCurs+1] == '=') {
			if (result <= 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
			else return orFlag ? comparator(scriptLine, cursors) : FALSE;
		}
		else if (result < 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
		else return orFlag ? comparator(scriptLine, cursors) : FALSE;
	}
	else if (scriptLine[opCurs] == '>') {
		if (scriptLine[opCurs+1] == '=') {
			if (result >= 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
			else return orFlag ? comparator(scriptLine, cursors) : FALSE;
		}
		else if (result > 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
		else return orFlag ? comparator(scriptLine, cursors) : FALSE;
	}
	else if (result != 0) return andFlag ? comparator(scriptLine, cursors) : TRUE;
	else return orFlag ? comparator(scriptLine, cursors) : FALSE;
}

int nextIf(char *scriptLine, FILE *scriptFile, int *cursors){
	//fprintf(stderr, "NEXT IF %p %i %i\n", scriptLine, cursors[START], cursors[STOP]);
	while (fgets(scriptLine, READ_SIZE + 1, scriptFile)){
		cursors[STOP] = -1;
		findToken(scriptLine, cursors);
		if (substringEquals("elif", scriptLine, cursors)) return FALSE;
		else if (substringEquals("fi", scriptLine, cursors)) return TRUE;
		else if (substringEquals("else", scriptLine, cursors)) return TRUE;
	}
}

int main(int argc, char **argv){
	vars.count = 0, vars.dict = NULL;
	seps.count = 0, seps.dict = NULL;
	files.count = 0, files.dict = NULL;
	loops.ptr = -1, loops.cap = 0, loops.stack = NULL;

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
					var->val = varStrAss(scriptLine, cursors);
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
						if ( (addr=findSep(scriptLine, cursors)) != -1 ) {
							findToken(scriptLine, cursors);
							int start = loadSepStart(loops.stack[loops.ptr].buff, seps.dict[addr], token2Int(scriptLine, cursors), loops.stack[loops.ptr].start, loops.stack[loops.ptr].stop);
							int stop = loadSepStop(loops.stack[loops.ptr].buff, seps.dict[addr].val, start, loops.stack[loops.ptr].stop);
							if (stop == -1) stop = loops.stack[loops.ptr].stop;
							printSubstring(loops.stack[loops.ptr].buff, start, stop);
						}
						else {
							addr = findFile(scriptLine, cursors);
							findToken(scriptLine, cursors);
							char *string = loadFile(NULL, files.dict[addr], &addr, token2Int(scriptLine, cursors));
							printf("%s", string);
							free(string);
						}
					}
					else printf("%s", vars.dict[findVar(scriptLine, cursors)].val);
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
			else file->fp = fopen(vars.dict[findVar(scriptLine, cursors)].val, "r");

			// Get separator
			if (scriptLine[cursors[STOP]] == ',' || findToken(scriptLine, cursors) == COMMA){
				if (findToken(scriptLine, cursors) == QUOTE) {
					file->len = cursors[STOP] - cursors[START];
					file->sep = substringSave(NULL, scriptLine, cursors);
				}
				else {
					file->sep = stringSave(NULL, vars.dict[findVar(scriptLine, cursors)].val);
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
				sep->val = stringSave(NULL, vars.dict[findVar(scriptLine, cursors)].val);
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
				loop->addr = findFile(scriptLine, cursors);
				if ( loop->type = (loop->addr == -1) ) loop->addr = findSep(scriptLine, cursors);

				// Get index
				if (scriptLine[cursors[STOP]] == '['){
					loop->isLoop = FALSE;
					findToken(scriptLine, cursors);
					loop->index = token2Int(scriptLine, cursors);
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
					if (loop->buff == NULL) loops.ptr -= breakLoop(*loop, scriptLine, scriptFile);
					else loop->start = 0;
				}
			} while ( loops.stack[loops.ptr].bounce = (scriptLine[cursors[STOP]] == '.') );
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			//fprintf(stderr, "\t\tOUT comand\n");
			loops = loadLoop(scriptLine, scriptFile);
			if (loops.ptr != -1) fseek(scriptFile, loops.stack[loops.ptr].cmd, SEEK_SET);
		}
		else if (substringEquals("if", scriptLine, cursors)){
			//fprintf(stderr, "\t\tIF command\n");
			while ( comparator(scriptLine, cursors) == FALSE && nextIf(scriptLine, scriptFile, cursors) == FALSE);
		}
		else if (substringEquals("elif", scriptLine, cursors) || substringEquals("else", scriptLine, cursors)){
			//fprintf(stderr, "\t\tELIF or ELSE\n");
			for (fgets(scriptLine, READ_SIZE + 1, scriptFile) ; substringEquals("fi", scriptLine, cursors) == FALSE; fgets(scriptLine, READ_SIZE + 1, scriptFile), cursors[STOP]=-1, findToken(scriptLine, cursors));
		}
		else if (substringEquals("fi", scriptLine, cursors)){
			//fprintf(stderr, "FI found\n");
		}
		else {
			//fprintf(stderr, "\t\tASS\n");
			int status, destIndex = findVar(scriptLine, cursors);
			if (scriptLine[cursors[STOP]] == '-' || (status=findToken(scriptLine, cursors)) == ASSIGNMENT){
				//fprintf(stderr, "\t\tString mode\n");
				findToken(scriptLine, cursors);
				char *newVar = varStrAss(scriptLine, cursors);
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
						result += token2Int(scriptLine, cursors);
						break;
					case '-':
						findToken(scriptLine, cursors);
						result -= token2Int(scriptLine, cursors);
						break;
					case '*':
						findToken(scriptLine, cursors);
						result *= token2Int(scriptLine, cursors);
						break;
					case '/':
						findToken(scriptLine, cursors);
						result /= token2Int(scriptLine, cursors);
						break;
					case '%':
						findToken(scriptLine, cursors);
						result %= token2Int(scriptLine, cursors);
						break;
					}
				} while (findToken(scriptLine, cursors) != TERMINATOR);
				int2String(vars.dict[destIndex].val, result);
			}
		}
	}
	return 0;
}
