#include <stdio.h>
#include <stdlib.h>

#define READ_SIZE 500
enum position	{START, STOP};
enum boolean	{FALSE, TRUE};
enum iterators 	{FILE_ITER = 0, FIELD_ITER = 1, VAR = 2};
enum comparator {LE = 0, LT = 1, GE = 2, GT = 3, EQ = 4, NE = 5};
enum null 	{NOT_FOUND = -1, NO_INDEX = -1, NO_LOOP = -1, STRING = -1};
enum errors 	{OOR, NO_DOLLAR, NO_BUFFER, NO_FILE_ITER, INDEX_VAR, NOT_EXIST, NOT_NUM, ASSIGN, EXISTS, ESC_SEQ, NO_EQUALS, NO_FI, NO_OUT};
enum tokenType 	{TERMINATOR = 6, QUOTE = 7, VARIABLE = 8, COMMA = 9, ASSIGNMENT = 10, DOLLAR = 11, MATHS = 12, NUMBER = 13}; 

struct fileDict {
	char *key; 		// filename
	char *delimiter; 	// delimiter
	int len;		// length of delimiter
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

struct fieldDict {
	char *key;
	char *val;
	int len;
};

struct fieldStruct {
	int count;
	struct fieldDict *dict;
} fields;

struct loopStruct {
	int type; 	// 0 = file  ; 1 = field
	int isLoop;	// 0 = false ; 1 = true
	int addr;	// address offet to find relevant file/field struct
	int index;	// occurence of file/field iterator to locate
	int chain;	// 0 =  false; 1 = true
	long cmd; 	// fseek to start of loop in script
	char *buff;
	int start;
	int stop;
};

struct loopStack {
	int ptr;	// stack pointer
	int cap;	// stack capacity
	struct loopStruct *stack;
} loops;

void throwError(int errNum, char *errStr, int errA, int errB){
	// errA and errB are used to pass integers, they could be represent types or substring coordinates.  Set to -1 if unused.
	switch(errNum){
	case OOR:
		fprintf(stderr, "ERROR: %s iterator '%s[%i]' is out of range.\n", errB == FIELD_ITER ? "field" : "file", errStr, errA);
		break;
	case NO_DOLLAR:
		fprintf(stderr, "ERROR: expected dollar '$'.  Found '%c'.\n", *errStr);
		break;
	case NO_BUFFER:
		fprintf(stderr, "ERROR: no buffer set.  Asterisk '*' only relevant within an 'in' block.\n");
		break;
	case NO_FILE_ITER:
		if (errB != -1) errStr[errB]=0;
		fprintf(stderr, "ERROR: field iterator '%s' cannot be used without first reading into a file iterator.\n", errB != -1 ? &errStr[errA] : errStr);
		break;
	case INDEX_VAR:
		fprintf(stderr, "ERROR: variable '%s' cannot be indexed.\n", errStr);
		break;
	case NOT_EXIST:
		errStr[errB]=0;
		fprintf(stderr, "ERROR: '%s' does not exist.\n", &errStr[errA]);
		break;
	case NO_INDEX:
		fprintf(stderr, "ERROR: %s iterator '%s' requires an index in this context.\n", errB == FIELD_ITER ? "field" : "file", errStr);
		break;
	case NOT_NUM:
		if (errB != -1) errStr[errB] = 0;
		fprintf(stderr, "ERROR: '%s' is not a valid number.\n", errB != -1 ? &errStr[errA] : errStr);
		break;
	case ASSIGN:
		fprintf(stderr, "ERROR: %s iterator '%s' cannot be assigned to.\n", errA == FIELD_ITER ? "field" : "file", errStr);
		break;
	case EXISTS:
		fprintf(stderr, "ERROR: '%s' already exists as %s.\n", errStr, errA == FIELD_ITER ? "field iterator" : (errA == FILE_ITER ? "file iterator" : "variable"));
		break;
	case ESC_SEQ:
		fprintf(stderr, "ERROR: '\\%c' escape sequence not recognised.  Valid escape sequences include \\n, \\t, \\\\, \\', \\` and \\\".\n", *errStr);
		break;
	case NO_EQUALS:
		fprintf(stderr, "ERROR: expected equals '=' assignment operator.  Found '%c'.\n", *errStr);
		break;
	case NO_FI:
		fprintf(stderr, "ERROR: 'if' block without closing 'fi' statement.\n");
		break;
	case NO_OUT:
		fprintf(stderr, "ERROR: 'in' block without closing 'out' statement.\n");
		break;
	}
	exit(errNum);
}

int getNextToken(char *txt, int *cursors){
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
	case '$':
		cursors[STOP] = cursors[START];
		return DOLLAR;
	case '=':
		if (txt[cursors[START]+1] == '='){
			cursors[STOP] = cursors[START]+1;
			return EQ;
		}
		else {
			cursors[STOP] = cursors[START];
			return ASSIGNMENT;
		}
	case '<':
		if (txt[cursors[START]+1] == '='){
			cursors[STOP] = cursors[START]+1;
			return LE;
		}
		else {
			cursors[STOP] = cursors[START];
			return LT;
		}
	case '>':
		if (txt[cursors[START]+1] == '='){
			cursors[STOP] = cursors[START]+1;
			return GE;
		}
		else {
			cursors[STOP] = cursors[START];
			return GT;
		}
	case '!':
		if (txt[cursors[START]+1] == '='){
			cursors[STOP] = cursors[START]+1;
			return NE;
		}
		else {
			fprintf(stderr, "! token unrecognised\n");
			exit(1);
		}
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
		cursors[STOP] = cursors[START]+1;
		return MATHS;
	case '\'':
	case '"':
	case '`':
		// Found quote.  Scan to matching quotation mark.
		for (cursors[STOP] = cursors[START] + 1; txt[cursors[STOP]] != txt[cursors[START]] ; ++cursors[STOP]){
			// Check for escape character.  If found, shuffle string left and convert.
			if (txt[cursors[STOP]] == '\\'){
				for (int i=cursors[STOP], j=cursors[STOP]+1; txt[i]!=0; ++i, ++j) txt[i] = txt[j];
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
				default:
					throwError(ESC_SEQ, &txt[cursors[STOP]], -1, -1);
				}
			}
		}
		++cursors[START];
		return QUOTE;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		for (cursors[STOP] = cursors[START]+1; txt[cursors[STOP]] != ' ' 
						   && txt[cursors[STOP]] != '\t' 
						   && txt[cursors[STOP]] != '\n' 
					           && txt[cursors[STOP]] != 0 
					           && txt[cursors[STOP]] != '=' 
						   && txt[cursors[STOP]] != ';'
						   && txt[cursors[STOP]] != ',' ; ++cursors[STOP]);
		return NUMBER;
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
	// Checks if comp matches txt[START-STOP]
	int compPos=0;
	for (int txtPos=cursors[START]; txtPos < cursors[STOP]; ++compPos, ++txtPos) if (comp[compPos] != txt[txtPos]) return FALSE;
	return comp[compPos] == 0;
}

int stringEquals(char *comp, char *txt){
	// Checks if comp matches txt
	for (int i=0; comp[i] != 0 && txt[i] != 0; ++i) if (comp[i] != txt[i]) return FALSE;
	return TRUE;
}

char *substringSave(char *dest, char *source, int *cursors){
	// Reallocate dest and copy source[START-STOP] into dest
	dest = realloc(dest, (1 + cursors[STOP] - cursors[START]) * sizeof(char));
	dest[cursors[STOP]-cursors[START]] = 0;
	for (int destPos=0; cursors[START] < cursors[STOP]; ++cursors[START], ++destPos) dest[destPos] = source[cursors[START]]; 
	return dest;
}

char *stringSave(char *dest, char *source){
	// Reallocate dest and copy source into dest
	int length = 0;
	while (source[length] != 0) ++length;

	dest = realloc(dest, (length+1) * sizeof(char));
	for (int pos=0; pos < length; ++pos) dest[pos] = source[pos];
	dest[length] = 0;

	return dest;
}

int findVar(char *txt, int *cursors){
	// Returns index of var in varDict where key matches txt substring.  Or -1
	for (int v=0; v < vars.count; ++v) if (substringEquals(vars.dict[v].key, txt, cursors)) return v;
	return NOT_FOUND;
}

int findFileIter(char *txt, int *cursors){
	// Returns index of file in fileDict where key matches txt substring.  Or -1
	for (int f=0; f < files.count; ++f) if (substringEquals(files.dict[f].key, txt, cursors)) return f;
	return NOT_FOUND;
}

int findFieldIter(char *txt, int *cursors){
	// Returns index of field in fieldDict where key matches txt substring.  Or -1
	for (int s=0; s < fields.count; ++s) if (substringEquals(fields.dict[s].key, txt, cursors)) return s;
	return NOT_FOUND;
}

int substringIsNum(char *txt, int from, int to){
	for (int decimalCount=0  ; from < to; ++from) {
		if ( (decimalCount += (txt[from]==46)) > 1
		   || txt[from] != 46 && txt[from] < 48
		   || txt[from] > 57) return FALSE;
	}
	return TRUE;
}

int stringIsNum(char *txt){
	for (int pos=0, decimalCount=0; txt[pos] != 0; ++pos) {
		if ( (decimalCount += (txt[pos]==46)) > 1
		   || txt[pos] != 46 && txt[pos] < 48
		   || txt[pos] > 57) return FALSE;
	}
	return TRUE;
}

float substring2Num(char *txt, int *cursors){
	// Converts txt[START-STOP] to float
	float result = 0;
	int pos, isNeg=(txt[cursors[START]]=='-');
	for (pos=cursors[START] + isNeg; pos < cursors[STOP] && txt[pos] != '.'; ++pos){
		result *= 10;
		if (txt[pos] < 48 || txt[pos] > 57) throwError(NOT_NUM, txt, cursors[START], cursors[STOP]);
		else result += (float)(txt[pos] - 48);
	}

	if (txt[pos] == '.'){
		++pos;
		for (int shift = 10; pos < cursors[STOP]; shift *= 10, ++pos){
			if (txt[pos] < 48 || txt[pos] > 57) throwError(NOT_NUM, txt, cursors[START], cursors[STOP]);
			else result += (float)(txt[pos]-48) / (float)shift;
		}
	}
	return isNeg ? 0 - result : result;
}

float string2Num(char *txt){
	// Converts txt to float
	float result=0;
	int pos, isNeg=(txt[0]=='-');
	for (pos=isNeg; txt[pos] != 0 && txt[pos] != '.'; ++pos){
		result *= 10;
		if (txt[pos] < 48 || txt[pos] > 57) throwError(NOT_NUM, txt, -1, -1);
		else result += (float)(txt[pos] - 48);
	}

	if (txt[pos] == '.'){
		++pos;
		for (int shift = 10; txt[pos] != 0; shift *= 10, ++pos){
			if (txt[pos] < 48 || txt[pos] > 57) throwError(NOT_NUM, txt, -1, -1);
			else result += (float)(txt[pos]-48) / (float)shift;
			//result += (float)(txt[pos]-48) / (float)shift;
		}
	}

	return isNeg ? 0 - result : result;
}

int skipWhitespace(char *txt, int from){
	while (txt[++from] == ' ' || txt[from] == '\t' || txt[from] == '\n');
	return from;
}

int getNextField(char *txt, char *delimiter, int start, int stop){
	// Returns delimiter starting position in txt[start-stop]
	if (delimiter == NULL){ // delimiter == whitespace
		while (txt[start] != ' ' && txt[start] != '\t' && txt[start] != '\n' && start < stop) ++start  ;
		return start >= stop ? NOT_FOUND : start;
	}
	else for (int i=start, j; i < stop; ++i){
		for (j=0; delimiter[j] != 0 && txt[i+j] == delimiter[j]; ++j);
		if (delimiter[j] == 0) return i + (delimiter[0] == 0);
	}
	return NOT_FOUND;
}

int skipFields(char *txt, struct fieldDict *field, int index, int from, int to){
	// Load field position after skipping "index" number of fields
	while (index-- && (from = getNextField(txt, field->val, from, to)) != -1) from = (field->val == NULL ? skipWhitespace(txt, from) : from + field->len) ;
	return from;
}

char *loadFile(char *buff, struct fileDict file, int *length, int index){
	// Skip "index" number of "file" records and load next into "buff"
	// Saves buff length into *length

	int buffCap=0, cursor, found, in;
	for (int ind = index+1; ind; buffCap=0, --ind){
		do {
			cursor = buffCap;
			buffCap += READ_SIZE;
			buff = realloc(buff, (buffCap+1) * sizeof(char));
			in = fread(&buff[cursor], sizeof(char), READ_SIZE, file.fp);
			found = getNextField(buff, file.delimiter, cursor, cursor + in);
		} while (found == NOT_FOUND  &&  in == READ_SIZE);

		if (found != NOT_FOUND) fseek(file.fp, (long)(0 - (feof(file.fp) ? in : buffCap) + found + file.len), SEEK_CUR);
		if (feof(file.fp) && ind > 1) throwError(OOR, file.key, index, -1);
	}

	*length = (found == NOT_FOUND ? cursor + in : found);

	if (feof(file.fp) && *length==0){
		free(buff);
		return NULL;
	}
	else {
		buff[*length] = 0;
		return realloc(buff, (1 + *length) * sizeof(char));
	}
}

void printSubstring(char *txt, int start, int stop){
	// Temporarily null terminate substring for printing
	char swap = txt[stop];
	txt[stop] = 0;
	printf("%s", &txt[start]);
	txt[stop] = swap;
}

float token2Num(char *txt, int *cursors){
	// Convert token to integer.  Either variable or string number.
	return txt[cursors[START]] >= 48 && txt[cursors[START]] <= 57 ? substring2Num(txt, cursors) : string2Num(vars.dict[findVar(txt, cursors)].val);
}

char *num2String(char *txt, float num){
	int isNeg = (num < 0);
	if (isNeg) num *= -1;
	int upper = (int)num, upperDigits = 0, lowerDigits = 0;
	float lower = num - upper;

	for (int cpy=upper; cpy ; cpy/=10, ++upperDigits);
	while ( (lower - (int)lower) > 0.0000000000000001f) lower*=10, ++lowerDigits;

	// total digits: add one for decimal place, one for negative symbol and one if number == 0
	int totalDigits = upperDigits + lowerDigits + (lowerDigits > 0) + isNeg + (upper == 0);
	txt = realloc(txt, (totalDigits + 1) * sizeof(char));
	txt[totalDigits] = 0;

	for (int cpy = (int)lower, pos = lowerDigits; pos; cpy/=10, --pos) txt[--totalDigits] = (cpy % 10) + 48;
	if (lowerDigits) txt[--totalDigits] = '.';

	if (upper == 0) txt[--totalDigits] = '0';
	else for (int cpy = upper; cpy ; cpy/=10) txt[--totalDigits] = (cpy % 10) + 48;
	
	if (isNeg) txt[--totalDigits] = '-';
		
	return txt;
}

char *substringJoin(char *dest, char *src, int from, int to){
	// Join source[from-to] to end of dest
	int len = 0;
	if (dest != NULL) while (dest[len] != 0) ++len;
	
	dest = realloc(dest, (len + to - from + 1) * sizeof(char));
	for (int dPos=len, sPos=from; sPos < to; ++dPos, ++sPos) dest[dPos] = src[sPos];
	dest[len + to - from] = 0;
	return dest;
}

char *stringJoin(char *dest, char *src){
	// Append src to dest
	if (src == NULL) return dest;

	int dLen=0, sLen=0;
	if (dest != NULL) while (dest[dLen] != 0) ++dLen;
	while (src[sLen] != 0) ++sLen;

	dest = realloc(dest, (dLen + sLen + 1) * sizeof(char));
	for (int dPos=dLen, sPos=0; src[sPos]!=0; ++dPos, ++sPos) dest[dPos] = src[sPos];
	dest[dLen + sLen] = 0;
	return dest;
}

struct loopStack loadLoop(char *scriptLine, FILE *scriptFile);
struct loopStack resetLoop(char *scriptLine, FILE *scriptFile){
	// Setup loopStruct cursors 
	// Use resetLoop on the way up the chain, use loadLoop on the way down the chain
	
	struct loopStruct *loop = &loops.stack[loops.ptr];
	if (loop->type == FIELD_ITER){
		// Load FIELD_ITER
		struct loopStruct *parent = &loops.stack[loops.ptr-1];
		loop->buff = parent->buff;
		if (loop->index == NO_INDEX) loop->start = parent->start;
		else if ( (loop->start = skipFields(loop->buff, &fields.dict[loop->addr], loop->index, parent->start, parent->stop)) == NOT_FOUND)
			throwError(OOR, fields.dict[loop->addr].key, loop->index, -1);
		if ( (loop->stop = getNextField(loop->buff, fields.dict[loop->addr].val, loop->start, parent->stop)) == NOT_FOUND)
			loop->stop = parent->stop;
	}
	else {	
		// Load FILE_ITER
		loop->buff = loadFile(NULL, files.dict[loop->addr], &loop->stop, loop->index);
		if (loop->buff == NULL) return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
		else loop->start = 0;
	}

	if (loop->chain == TRUE){
		++loops.ptr;
		return resetLoop(scriptLine, scriptFile);
	}
	else if (loop->cmd == -1) loop->cmd = ftell(scriptFile);
	else fseek(scriptFile, loop->cmd, SEEK_SET);

	return loops;
}

struct loopStack loadLoop(char *scriptLine, FILE *scriptFile){
	// Advance loopStruct cursors
	// Use loadLoop on the way down the chain, use resetLoop on the way up the chain
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];

	if (loop->isLoop == FALSE) 
		return --loops.ptr == -1 || loops.stack[loops.ptr].chain == FALSE ? loops : loadLoop(scriptLine, scriptFile);
	else if (loop->type == FIELD_ITER){
		loop->start = (fields.dict[loop->addr].val == NULL ? skipWhitespace(loop->buff, loop->stop) : loop->stop + fields.dict[loop->addr].len);
					       // delim != whitespace		    && delim == char-by-char	 	 && reached final char
		if (loop->start > parent->stop || fields.dict[loop->addr].val != NULL && fields.dict[loop->addr].val[0] == 0 && loop->start == parent->stop)
			return --loops.ptr == -1 || loops.stack[loops.ptr].chain == FALSE ? loops : loadLoop(scriptLine, scriptFile);
		else if ( (loop->stop = getNextField(loop->buff, fields.dict[loop->addr].val, loop->start, parent->stop)) == NOT_FOUND)
			loop->stop = parent->stop;
	}
	else if ( (loop->buff = loadFile(loop->buff, files.dict[loop->addr], &loop->stop, 0)) == NULL)
		return --loops.ptr == -1 || loops.stack[loops.ptr].chain == FALSE ? loops : loadLoop(scriptLine, scriptFile);
	
	if (loop->chain == TRUE){
		++loops.ptr;
		return resetLoop(scriptLine, scriptFile);
	}
	else {
		fseek(scriptFile, loop->cmd, SEEK_SET);
		return loops;
	}
}

int retrieveToken(int *outCurs, char **outTxt, char *inTxt, int *inCurs){
	// Converts next token to value.  Gets token from inTxt[start-stop].  Returns string in outTxt[start-stop]
	// Token could refer to a variable, file iterator or field iterator.  Could be a number or string quote.
	// If substring, puts start/stop in outCurs.  Else string, puts STRING (-1) in outCurs[START]
	// If memory allocated for string, returns TRUE.  Else returns FALSE.
	// If no token, returns TERMINATOR (-1)

	int addr;
	switch (getNextToken(inTxt, inCurs)){
	case DOLLAR:
		// User provided dollar ($), which means "entire buffer"
		if (inTxt[inCurs[START]] != '$') throwError(NO_DOLLAR, &inTxt[inCurs[START]], -1, -1);
		else if (loops.ptr == NO_LOOP) throwError(NO_BUFFER, NULL, -1, -1);
		*outTxt = loops.stack[loops.ptr].buff;
		if (loops.stack[loops.ptr].type == FIELD_ITER){
			outCurs[START] = loops.stack[loops.ptr].start;
			outCurs[STOP] = loops.stack[loops.ptr].stop;
		}
		else outCurs[START] = STRING;
		return FALSE;
	case NUMBER:
	case QUOTE:
		outCurs[START] = inCurs[START];
		outCurs[STOP] = inCurs[STOP];
		*outTxt = inTxt;
		return FALSE;
	case VARIABLE:
		if (inTxt[inCurs[STOP]] == '['){ 											// Iterator
			if ((addr = findFieldIter(inTxt, inCurs)) != NOT_FOUND) { 							// Field iterator
				if (loops.ptr == NO_LOOP) throwError(NO_FILE_ITER, fields.dict[addr].key, -1, -1);
				getNextToken(inTxt, inCurs);
				struct loopStruct *loop = &loops.stack[loops.ptr];
				outCurs[START] = skipFields(loop->buff, &fields.dict[addr], (int)token2Num(inTxt, inCurs), loop->start, loop->stop);
				if (outCurs[START] == NOT_FOUND) throwError(OOR, fields.dict[addr].key, (int)token2Num(inTxt, inCurs), FIELD_ITER);
				outCurs[STOP] = getNextField(loop->buff, fields.dict[addr].val, outCurs[START], loop->stop);
				if (outCurs[STOP] == NOT_FOUND) outCurs[STOP] = loop->stop;
				*outTxt = loops.stack[loops.ptr].buff;
				return FALSE;
			}
			else if ((addr = findFileIter(inTxt, inCurs)) != NOT_FOUND){							// File iterator
				getNextToken(inTxt, inCurs);
				outCurs[START] = STRING;
				*outTxt = loadFile(NULL, files.dict[addr], &addr, (int)token2Num(inTxt, inCurs));
				return TRUE;
			}
			else if ((addr = findVar(inTxt, inCurs)) != NOT_FOUND) throwError(INDEX_VAR, vars.dict[addr].key, -1, -1);	// Var error
			else throwError(NOT_EXIST, inTxt, inCurs[START], inCurs[STOP]);							// Unknown error
		}
		else if ((addr = findVar(inTxt, inCurs)) != NOT_FOUND){									// Variable
			outCurs[START] = STRING;
			*outTxt = vars.dict[addr].val;
			return FALSE;
		}
		else if ((addr=findFieldIter(inTxt, inCurs)) != NOT_FOUND ) throwError(NO_INDEX, fields.dict[addr].key, FIELD_ITER, -1);
		else if ((addr=findFileIter(inTxt, inCurs)) != NOT_FOUND) throwError(NO_INDEX, files.dict[addr].key, FILE_ITER, -1);
		else throwError(NOT_EXIST, inTxt, inCurs[START], inCurs[STOP]);								// Unknown error
	case COMMA:
	case TERMINATOR:
		return TERMINATOR;
	default:
		fprintf(stderr, "Unknown token\n");
		exit(1);
	}
}

char *varStrAss(char *scriptLine, int *cursors){
	// Assign multiple concatenated strings to a variable
	char *out = NULL, *buff;
	int freeBuff, buffCurs[2];
	while (scriptLine[cursors[STOP]] != ',' && (freeBuff=retrieveToken(buffCurs, &buff, scriptLine, cursors)) != TERMINATOR){
		out = buffCurs[START] == STRING ? stringJoin(out, buff) : substringJoin(out, buff, buffCurs[START], buffCurs[STOP]);
		if (freeBuff) free(buff); 
	}
	return out;
}

char *varMthAss(int varAddr, char *scriptLine, int *cursors){
	float augend = string2Num(vars.dict[varAddr].val), addend;
	do {
		char op = scriptLine[cursors[START]];
		char *subTxt;
		int augCurs[2];
		int toFree = retrieveToken(augCurs, &subTxt, scriptLine, cursors);
		addend = augCurs[START] == STRING ? string2Num(subTxt) : substring2Num(subTxt, augCurs);
		if (toFree == TRUE) free(subTxt);
		switch(op){
		case '+':
			augend += addend;
			break;
		case '-':
			augend -= addend;
			break;
		case '*':
			augend *= addend;
			break;
		case '/':
			augend /= addend;
			break;
		case '%':
			augend = (int)augend % (int)addend;
			break;
		}
	} while (getNextToken(scriptLine, cursors) != TERMINATOR);
	return num2String(vars.dict[varAddr].val, augend);
}

int compareTokens(char *txtA, int *cursA, char *txtB, int *cursB){
	// Return 0 if A == B ; > 0 if A > B ; < 0 if A < B

	// Inefficient.  IsNum() functions are used, then used again in the 2Num() functions.
	int aIsNum = cursA[START] == STRING ? stringIsNum(txtA) : substringIsNum(txtA, cursA[START], cursA[STOP]);
	int bIsNum = cursB[START] == STRING ? stringIsNum(txtB) : substringIsNum(txtB, cursB[START], cursB[STOP]);
	if (aIsNum && bIsNum){
		float a = cursA[START] == STRING ? string2Num(txtA) : substring2Num(txtA, cursA);
		float b = cursB[START] == STRING ? string2Num(txtB) : substring2Num(txtB, cursB);
		return a == b ? 0 : a > b ? 1 : -1;
	}
	else {
		int result;
		for(int aPos=cursA[START] == STRING ? 0 : cursA[START], bPos=cursB[START] == STRING ? 0 : cursB[START]  ; 
			(cursA[START] == STRING && txtA[aPos] != 0 || cursA[START] != STRING && aPos < cursA[STOP]) 
		     && (cursB[START] == STRING && txtB[bPos] != 0 || cursB[START] != STRING && bPos < cursB[STOP]) 
		     && (result = txtA[aPos]-txtB[bPos]) == 0 ; ++aPos, ++bPos); 
		return result;
	}
}

int comparator(char *scriptLine, int *cursors){
	// Return TRUE/FALSE result of statement
	// Gets tokens from IF and interprets comparator operand
	int cursA[2], cursB[2];
	char *txtA, *txtB;
	
	int freeA = retrieveToken(cursA, &txtA, scriptLine, cursors), freeB, result;

	int operator = getNextToken(scriptLine, cursors);
	if (operator == VARIABLE){
		int inc = substringEquals("inc", scriptLine, cursors);
		freeB = retrieveToken(cursB, &txtB, scriptLine, cursors);
		
		// getNextField() requires txtA start/stop coords
		if (cursA[START] == STRING) for (cursA[STOP]=0; txtA[cursA[STOP]] != 0; ++cursA[STOP]);

		// getNextField() requires txtB as null-terminated string
		char swap;
		if (cursB[START] != STRING) {
			swap = txtB[cursB[STOP]];
			txtB[cursB[STOP]] = 0;
		}

		result = (getNextField(txtA, &txtB[cursB[START]], cursA[START] == -1 ? 0 : cursA[START], cursA[STOP]) != NOT_FOUND);
		if (!inc) result = !result;
		if (cursB[START] != STRING) txtB[cursB[STOP]] = swap;
	}
	else {
		freeB = retrieveToken(cursB, &txtB, scriptLine, cursors);
		result = compareTokens(txtA, cursA, txtB, cursB);
		switch (operator){
			case LT:
				result = result < 0;
				break;
			case LE:
				result = result <= 0;
				break;
			case GT:
				result = result > 0;
				break;
			case GE:
				result = result >= 0;
				break;
			case EQ:
				result = result == 0;
				break;
			case NE:
				result = result != 0;
				break;
			default:
				fprintf(stderr, "Comparator %i not recognised\n", operator);
				exit(1);
		}
	}

	int andFlag=FALSE, orFlag=FALSE;
	if ( getNextToken(scriptLine, cursors) != TERMINATOR && (andFlag=substringEquals("and", scriptLine, cursors)) == FALSE ) 
		orFlag = substringEquals("or", scriptLine, cursors);

	if (freeA) free(txtA);
	if (freeB) free(txtB);

	if (result == TRUE) return andFlag ? comparator(scriptLine, cursors) : TRUE;
	else return orFlag ? comparator(scriptLine, cursors) : FALSE;
}

int nextIf(char *scriptLine, FILE *scriptFile, int *cursors){
	// Finds next IF block
	// Returns TRUE if execution should resume
	// Returns FALSE if a further "elif" test is required
	while (fgets(scriptLine, READ_SIZE + 1, scriptFile) && !feof(scriptFile)){
		cursors[STOP] = -1;
		getNextToken(scriptLine, cursors);
		if (substringEquals("elif", scriptLine, cursors)) return FALSE;
		else if (substringEquals("fi", scriptLine, cursors)) return TRUE;
		else if (substringEquals("else", scriptLine, cursors)) return TRUE;
	}
	throwError(NO_FI, NULL, -1, -1);
}

void endLoop(char *scriptLine, FILE *scriptFile){
	for (int loopCount=1; loopCount; ){
		fgets(scriptLine, READ_SIZE + 1, scriptFile);
		if (feof(scriptFile)) throwError(NO_OUT, NULL, -1, -1);
		int cursors[2] = {-1, -1};
		getNextToken(scriptLine, cursors);
		if (substringEquals("in", scriptLine, cursors)) ++loopCount;
		else if (substringEquals("out", scriptLine, cursors)) --loopCount;
	}
}

int main(int argc, char **argv){
	vars.count = 0, vars.dict = NULL;
	fields.count = 0, fields.dict = NULL;
	files.count = 0, files.dict = NULL;
	loops.ptr = -1, loops.cap = 0, loops.stack = NULL;
	char scriptLine[READ_SIZE + 1];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, READ_SIZE + 1, scriptFile) ; !feof(scriptFile); fgets(scriptLine, READ_SIZE + 1, scriptFile) ) {
		int cursors[2] = {0, -1};
		if (getNextToken(scriptLine, cursors) == TERMINATOR) continue;
		else if (substringEquals("var", scriptLine, cursors)){
			do {
				getNextToken(scriptLine, cursors);
				int addr;
				if ((addr = findFieldIter(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, fields.dict[addr].key, FIELD_ITER, -1);
				else if ((addr = findFileIter(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, files.dict[addr].key, FILE_ITER, -1);
				else if ((addr = findVar(scriptLine, cursors)) == NOT_FOUND){
					// Allocate new variable
					addr = vars.count++;
					vars.dict = realloc(vars.dict, vars.count * sizeof(struct varDict));
					vars.dict[addr].val = NULL;
					vars.dict[addr].key = substringSave(NULL, scriptLine, cursors);
				}
			
				switch(scriptLine[cursors[STOP]]){
				case '=':
					// String assignment without whitespace
					vars.dict[addr].val = varStrAss(scriptLine, cursors);
					break;
				case '+':
				case '-':
				case '/':
				case '*':
				case '%':
					// Maths assignment without whitespace
					if (scriptLine[cursors[STOP]+1] != '=') throwError(NO_EQUALS, &scriptLine[cursors[STOP]+1], -1, -1);
					vars.dict[addr].val = realloc(vars.dict[addr].val, 2 * sizeof(char));
					vars.dict[addr].val[0] = '0';
					vars.dict[addr].val[1] = '\0';
					vars.dict[addr].val = varMthAss(addr, scriptLine, cursors);
					break;
				default:
					switch(getNextToken(scriptLine,cursors)){
					case ASSIGNMENT:
						// String assignment with whitespace
						vars.dict[addr].val = varStrAss(scriptLine, cursors);
						break;
					case MATHS:
						// Maths assignment with whitespace
						if (scriptLine[cursors[STOP]] != '=') throwError(NO_EQUALS, &scriptLine[cursors[STOP]], -1, -1);
						vars.dict[addr].val = realloc(vars.dict[addr].val, 2 * sizeof(char));
						vars.dict[addr].val[0] = '0';
						vars.dict[addr].val[1] = '\0';
						vars.dict[addr].val = varMthAss(addr, scriptLine, cursors);
						break;
					default:
						if (vars.dict[addr].val == NULL || vars.dict[addr].val[0] != 0){
							// No assignment: initialise empty variable
							vars.dict[addr].val = realloc(vars.dict[addr].val, sizeof(char));
							vars.dict[addr].val[0] = 0;
						}
						break;
					}
				}
			} while (scriptLine[cursors[START]] == ',' || scriptLine[cursors[STOP]] == ','); // Multiple comma-separated var declarations
		}
		else if (substringEquals("print", scriptLine, cursors)){
			char *buff;
			int freeBuff, printCurs[2];
			while ( (freeBuff=retrieveToken(printCurs, &buff, scriptLine, cursors)) != TERMINATOR){
				if (printCurs[START] == NOT_FOUND) printf("%s", buff);
				else printSubstring(buff, printCurs[START], printCurs[STOP]);
				if (freeBuff == TRUE) free(buff);
			}
		}
		else if (substringEquals("file", scriptLine, cursors)){
			// Get name
			getNextToken(scriptLine, cursors);

			int addr;
			if ((addr = findVar(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, vars.dict[addr].key, VAR, -1);
			else if ((addr = findFieldIter(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, fields.dict[addr].key, FIELD_ITER, -1);
			else if ((addr = findFileIter(scriptLine, cursors)) == NOT_FOUND){
				// Allocate new file iterator
				addr = files.count++;
				files.dict = realloc(files.dict, files.count * sizeof(struct fileDict));
				files.dict[addr].key = substringSave(NULL, scriptLine, cursors);
			}
			else {
				// Clear/Close this file iterator
				free(files.dict[addr].delimiter);
				fclose(files.dict[addr].fp);
			}

			struct fileDict *file = &files.dict[addr];

			// Get filename
			if (getNextToken(scriptLine, cursors) == QUOTE) {
				char swap = scriptLine[cursors[STOP]];
				scriptLine[cursors[STOP]] = 0;
				file->fp = fopen(&scriptLine[cursors[START]], "r");
				scriptLine[cursors[STOP]] = swap;
			}
			else file->fp = fopen(vars.dict[findVar(scriptLine, cursors)].val, "r");

			// Get delimiter
			if (scriptLine[cursors[STOP]] == ',' || getNextToken(scriptLine, cursors) == COMMA){
				switch(getNextToken(scriptLine, cursors)){
				case QUOTE:
					file->len = cursors[STOP] - cursors[START];
					file->delimiter = substringSave(NULL, scriptLine, cursors);
					break;
				case MATHS:
					if (scriptLine[cursors[START]] != '*') ; // throw some kind of error
					else ; // load entire file
				case DOLLAR:
					// What if they put a file segment into a file segment?
					// retrieveToken() might be a better choice for this switch statement
				case VARIABLE:
					file->delimiter = stringSave(NULL, vars.dict[findVar(scriptLine, cursors)].val);
					for (file->len = 0; file->delimiter[file->len] != 0; ++file->len);
				default:
					// Raise invalid token error
				}
			}
			else {  // No delimiter provided.  Default = newline (\n)
				file->len = 1;
				file->delimiter = malloc(2 * sizeof(char));
				file->delimiter[0] = '\n';
				file->delimiter[1] = 0;
			}
		}
		else if (substringEquals("field", scriptLine, cursors)){
			// Get name
			getNextToken(scriptLine, cursors);
			int addr;
			if ((addr = findVar(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, vars.dict[addr].key, VAR, -1);
			else if ((addr = findFileIter(scriptLine, cursors)) != NOT_FOUND) throwError(EXISTS, files.dict[addr].key, FILE_ITER, -1);
			else if ((addr=findFieldIter(scriptLine, cursors)) == NOT_FOUND){
				addr = fields.count++;
				fields.dict = realloc(fields.dict, fields.count * sizeof(struct fieldDict));
				fields.dict[addr].key = substringSave(NULL, scriptLine, cursors);
			}
			else free(fields.dict[addr].val);

			struct fieldDict *field = &fields.dict[addr];

			// Get delimiter
			if (getNextToken(scriptLine, cursors) == QUOTE) {
				field->len = cursors[STOP] - cursors[START];
				field->val = substringSave(NULL, scriptLine, cursors);
			}
			else if (scriptLine[cursors[START]] != ')'){
				field->val = stringSave(NULL, vars.dict[findVar(scriptLine, cursors)].val);
				for (field->len = 0; field->val[field->len] != 0; ++field->len);
			}
			else {  // No delimiter provided.  Default = whitespace	
				field->len = 0;
				field->val = NULL;
			}
		}
		else if (substringEquals("in", scriptLine, cursors)){
			do {
				if (++loops.ptr == loops.cap){
					++loops.cap;
					loops.stack = realloc(loops.stack, loops.cap * sizeof(struct loopStruct));
				}

				struct loopStruct *loop = &loops.stack[loops.ptr];

				// Save cmd
				loop->cmd = -1;

				// Get type and addr
				getNextToken(scriptLine, cursors);
				loop->addr = findFileIter(scriptLine, cursors);
				if ( loop->type = (loop->addr == NOT_FOUND) ) {
					if (!loops.ptr) throwError(NO_FILE_ITER, scriptLine, cursors[START], cursors[STOP]);
					loop->addr = findFieldIter(scriptLine, cursors);
				}

				// Get index
				if (scriptLine[cursors[STOP]] == '['){
					loop->isLoop = FALSE;
					getNextToken(scriptLine, cursors);
					loop->index = (int)token2Num(scriptLine, cursors);
				}
				else {
					loop->isLoop = TRUE;
					loop->index = loop->type == FIELD_ITER? -1 : 0;
				}

				loop->chain = FALSE; 
				loops = resetLoop(scriptLine, scriptFile);

			} while ( loops.ptr != -1 && (loops.stack[loops.ptr].chain = (scriptLine[cursors[STOP]] == '.')) );

			if (loops.ptr == NO_LOOP) endLoop(scriptLine, scriptFile);
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			loops = loadLoop(scriptLine, scriptFile);
		}
		else if (substringEquals("cont", scriptLine, cursors)){
			loops = loadLoop(scriptLine, scriptFile);
			if (loops.ptr == NO_LOOP) endLoop(scriptLine, scriptFile);
		}
		else if (substringEquals("if", scriptLine, cursors)){
			while (comparator(scriptLine, cursors) == FALSE   &&   nextIf(scriptLine, scriptFile, cursors) == FALSE);
		}
		else if (substringEquals("elif", scriptLine, cursors) || substringEquals("else", scriptLine, cursors)){
			for (fgets(scriptLine, READ_SIZE + 1, scriptFile) ; substringEquals("fi", scriptLine, cursors) == FALSE; fgets(scriptLine, READ_SIZE + 1, scriptFile), cursors[STOP]=-1, getNextToken(scriptLine, cursors));
		}
		else if (substringEquals("break", scriptLine, cursors)){
			do {
				if (loops.stack[loops.ptr].type == FILE_ITER) free(loops.stack[loops.ptr].buff);
			} while ( --loops.ptr >= 0 && loops.stack[loops.ptr].chain == TRUE);

			endLoop(scriptLine, scriptFile);
		}
		else if (substringEquals("exit", scriptLine, cursors)){
			break;
		}
		else if (substringEquals("fi", scriptLine, cursors) == FALSE){
			int destAddr = findVar(scriptLine, cursors);
			if (destAddr == NOT_FOUND){
				int err;
				if ((err=findFieldIter(scriptLine, cursors)) != NOT_FOUND) throwError(ASSIGN, fields.dict[err].key, FIELD_ITER, -1);
				else if ((err=findFileIter(scriptLine, cursors)) != NOT_FOUND) throwError(ASSIGN, files.dict[err].key, FILE_ITER, -1);
				else throwError(NOT_EXIST, scriptLine, cursors[START], cursors[STOP]);
			}

			if (scriptLine[cursors[STOP]] == '=' || getNextToken(scriptLine, cursors) == ASSIGNMENT){
				char *newVar = varStrAss(scriptLine, cursors);
				free(vars.dict[destAddr].val);
				vars.dict[destAddr].val = newVar;
			}
			else {
				vars.dict[destAddr].val = varMthAss(destAddr, scriptLine, cursors);
			}
		}
	}
	// CLEAN UP
	fclose(scriptFile);

	// Free loop buffers
	for ( ; loops.ptr > -1; --loops.ptr) if (loops.stack[loops.ptr].type == FILE_ITER) free(loops.stack[loops.ptr].buff);
	if (loops.stack != NULL) free(loops.stack);

	// Free variables
	for (int var=0; var < vars.count; ++var) free(vars.dict[var].key), free(vars.dict[var].val);
	if (vars.dict != NULL) free(vars.dict);

	// Free file iterators
	for (int file=0; file < files.count; ++file) free(files.dict[file].key), free(files.dict[file].delimiter), fclose(files.dict[file].fp);
	if (files.dict != NULL) free(files.dict);

	// Free field iterators
	for (int field=0; field < fields.count; ++field) free(fields.dict[field].key), free(fields.dict[field].val);
	if (fields.dict != NULL) free(fields.dict);

	return 0;
}
