#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define READ_SIZE 500
enum position	{START, STOP};
enum segment 	{FILE_SEG = 0, FIELD_SEG = 1};
enum comparator {LE = 0, LT = 1, GE = 2, GT = 3, EQ = 4, NE = 5};
enum tokenType 	{TERMINATOR = 6, QUOTE = 7, VARIABLE = 8, COMMA = 9, ASSIGNMENT = 10, ASTERISK = 11, MATHS = 11, NUMBER = 12}; 
enum null 	{NOT_FOUND = -1, NO_INDEX = -1, STRING = -1};
enum boolean	{FALSE, TRUE};

struct fileDict {
	char *key; 		// filename
	char *delimiter; 	// delimiter
	int len;		// length of delimiter
	FILE *fp;
};

struct fileStruct {
	int count;
	struct fileDict *dict;
} fileSegs;

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
} fieldSegs;

struct loopStruct {
	int type; 	// 0 = file  ; 1 = field
	int isLoop;	// 0 = false ; 1 = true
	int addr;	// address offet to find relevant file/field struct
	int index;	// occurence of file/field segment to locate
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
						   && txt[cursors[STOP]] != ','
						   && txt[cursors[STOP]] != '('
						   && txt[cursors[STOP]] != ')'
						   && txt[cursors[STOP]] != '['
						   && txt[cursors[STOP]] != ']' ; ++cursors[STOP]);
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

int findFileSeg(char *txt, int *cursors){
	// Returns index of file in fileDict where key matches txt substring.  Or -1
	for (int f=0; f < fileSegs.count; ++f) if (substringEquals(fileSegs.dict[f].key, txt, cursors)) return f;
	return NOT_FOUND;
}

int findFieldSeg(char *txt, int *cursors){
	// Returns index of field in fieldDict where key matches txt substring.  Or -1
	for (int s=0; s < fieldSegs.count; ++s) if (substringEquals(fieldSegs.dict[s].key, txt, cursors)) return s;
	return NOT_FOUND;
}

float substring2Num(char *txt, int *cursors){
	// Converts txt[START-STOP] to float
	float result = 0;
	int pos, isNeg=(txt[cursors[START]]=='-');
	for (pos=cursors[START] + isNeg; pos < cursors[STOP] && txt[pos] != '.'; ++pos){
		result *= 10;
		result += (float)(txt[pos] - 48);
	}

	if (txt[pos] == '.'){
		++pos;
		for (int shift = 10; pos < cursors[STOP]; shift *= 10, ++pos){
			result += (float)(txt[pos]-48) / (float)shift;
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
		result += (float)(txt[pos] - 48);
	}

	if (txt[pos] == '.'){
		++pos;
		for (int shift = 10; txt[pos] != 0; shift *= 10, ++pos){
			result += (float)(txt[pos]-48) / (float)shift;
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
	// Load field position after skipping "index" number of fieldSegs
	while (index-- && (from = getNextField(txt, field->val, from, to)) != -1) from = (field->val == NULL ? skipWhitespace(txt, from) : from + field->len) ;
	return from;
}

char *loadFile(char *buff, struct fileDict file, int *length, int index){
	// Skip "index" number of "file" records and load next into "buff"
	// Saves buff length into *length

	int buffCap, cursor, found, in;
	for (buffCap=0, index+=1; index; buffCap=0, --index){
		do {
			cursor = buffCap;
			buffCap += READ_SIZE;
			buff = realloc(buff, (buffCap+1) * sizeof(char));
			in = fread(&buff[cursor], sizeof(char), READ_SIZE, file.fp);
			found = getNextField(buff, file.delimiter, cursor, cursor + in);
		} while (found == NOT_FOUND  &&  in == READ_SIZE);

		if (found != NOT_FOUND) fseek(file.fp, (long)(0 - (feof(file.fp) ? in : buffCap) + found + file.len), SEEK_CUR);
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
	if (loop->type == FIELD_SEG){
		// Load FIELD_SEG
		struct loopStruct *parent = &loops.stack[loops.ptr-1];
		loop->buff = parent->buff;
		if (loop->index == NO_INDEX) loop->start = parent->start;
		else if ( (loop->start = skipFields(loop->buff, &fieldSegs.dict[loop->addr], loop->index, parent->start, parent->stop)) == NOT_FOUND){
			fprintf(stderr, "ERROR: field segment '%s[%i]' is out of range.\n", fieldSegs.dict[loop->addr].key, loop->index);
			exit(1);
		}	
		if ( (loop->stop = getNextField(loop->buff, fieldSegs.dict[loop->addr].val, loop->start, parent->stop)) == NOT_FOUND)
			loop->stop = parent->stop;
	}
	else {	
		// Load FILE_SEG
		loop->buff = loadFile(NULL, fileSegs.dict[loop->addr], &loop->stop, loop->index);
		if (loop->buff == NULL) return --loops.ptr == -1 ? loops : loadLoop(scriptLine, scriptFile);
		else loop->start = 0;
	}

	// Check from here
	if (loop->chain == TRUE){
		++loops.ptr;
		return resetLoop(scriptLine, scriptFile);
	}

	fseek(scriptFile, loop->cmd, SEEK_SET);
	// To here
	return loops;
}

struct loopStack loadLoop(char *scriptLine, FILE *scriptFile){
	// Advance loopStruct cursors
	// Use loadLoop on the way down the chain, use resetLoop on the way up the chain
	struct loopStruct *loop = &loops.stack[loops.ptr];
	struct loopStruct *parent = &loops.stack[loops.ptr-1];

	if (loop->isLoop == FALSE) 
		return --loops.ptr == -1 || loops.stack[loops.ptr].chain == FALSE ? loops : loadLoop(scriptLine, scriptFile);
	else if (loop->type == FIELD_SEG){
		loop->start = (fieldSegs.dict[loop->addr].val == NULL ? skipWhitespace(loop->buff, loop->stop) : loop->stop + fieldSegs.dict[loop->addr].len);
					       // delim != whitespace		    && delim == char-by-char	 	 && reached final char
		if (loop->start > parent->stop || fieldSegs.dict[loop->addr].val != NULL && fieldSegs.dict[loop->addr].val[0] == 0 && loop->start == parent->stop)
			return --loops.ptr == -1 || loops.stack[loops.ptr].chain == FALSE ? loops : loadLoop(scriptLine, scriptFile);
		else if ( (loop->stop = getNextField(loop->buff, fieldSegs.dict[loop->addr].val, loop->start, parent->stop)) == NOT_FOUND)
			loop->stop = parent->stop;
	}
	else if ( (loop->buff = loadFile(loop->buff, fileSegs.dict[loop->addr], &loop->stop, 0)) == NULL)
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
	// Token could refer to a variable, file segment, field segment.  Could be a number or string quote.
	// If substring, puts start/stop in outCurs.  Else string, puts STRING (-1) in outCurs[START]
	// If memory allocated for string, returns TRUE.  Else returns FALSE.
	// If no token, returns TERMINATOR (-1)

	switch (getNextToken(inTxt, inCurs)){
	case ASTERISK:
		// User provided asterisk (*), which means "entire buffer"
		// Error check that they didn't actually provide a different incorrect mathematical operator
		*outTxt = loops.stack[loops.ptr].buff;
		if (loops.stack[loops.ptr].type == FIELD_SEG){
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
		if (inTxt[inCurs[STOP]] == '['){ // Segment
			int addr = findFieldSeg(inTxt, inCurs);
			if (addr != NOT_FOUND) { // Field seg
				getNextToken(inTxt, inCurs);
				struct loopStruct *loop = &loops.stack[loops.ptr];
				outCurs[START] = skipFields(loop->buff, &fieldSegs.dict[addr], (int)token2Num(inTxt, inCurs), loop->start, loop->stop);
				if (outCurs[START] == NOT_FOUND){
					fprintf(stderr, "ERROR: field segment '%s[%i]' is out of range.\n", fieldSegs.dict[addr].key, (int)token2Num(inTxt, inCurs));
					exit(1);
				}
				outCurs[STOP] = getNextField(loop->buff, fieldSegs.dict[addr].val, outCurs[START], loop->stop);
				if (outCurs[STOP] == NOT_FOUND) outCurs[STOP] = loop->stop;
				*outTxt = loops.stack[loops.ptr].buff;
				return FALSE;
			}
			else {			// File seg
				getNextToken(inTxt, inCurs);
				outCurs[START] = STRING;
				*outTxt = loadFile(NULL, fileSegs.dict[findFileSeg(inTxt, inCurs)], &addr, (int)token2Num(inTxt, inCurs));
				return TRUE;
			}
		}
		else {				// Variable
			outCurs[START] = STRING;
			*outTxt = vars.dict[findVar(inTxt,inCurs)].val;
			return FALSE;
		}
	default:
		return TERMINATOR;
	}
}

char *varStrAss(char *scriptLine, int *cursors){
	// Assign multiple concatenated strings to a variable
	char *buff = NULL;
	for (int status=getNextToken(scriptLine, cursors); status != TERMINATOR && status != COMMA; status = scriptLine[cursors[STOP]] == ',' ? COMMA : getNextToken(scriptLine, cursors)){
		if (status == QUOTE || status == NUMBER) buff = substringJoin(buff, scriptLine, cursors[START], cursors[STOP]);
		else if (status == ASTERISK) {
			struct loopStruct *loop = &loops.stack[loops.ptr];
			buff = (loop->type == FIELD_SEG ? substringJoin(buff, loop->buff, loop->start, loop->stop) : stringJoin(buff, loop->buff));
		}
		else if (scriptLine[cursors[STOP]] == '['){
			int addr = findFieldSeg(scriptLine, cursors);
			if (addr != NOT_FOUND ) {
				getNextToken(scriptLine, cursors);
				struct loopStruct *loop = &loops.stack[loops.ptr];
				int start = skipFields(loop->buff, &fieldSegs.dict[addr], (int)token2Num(scriptLine, cursors) , loop->start, loop->stop);
				if (start == NOT_FOUND){
					fprintf(stderr, "ERROR: field segment '%s[%i]' is out of range.\n", fieldSegs.dict[addr].key, (int)token2Num(scriptLine, cursors));
					exit(1);
				}
				int stop = getNextField(loop->buff, fieldSegs.dict[addr].val, start, loop->stop);
				if (stop == NOT_FOUND) stop = loop->stop;
				buff = substringJoin(buff, loop->buff, start, stop);
			}
			else {
				addr = findFileSeg(scriptLine, cursors);
				getNextToken(scriptLine, cursors);
				char *string = loadFile(NULL, fileSegs.dict[addr], &addr, (int)token2Num(scriptLine, cursors));
				buff = stringJoin(buff, string);
				free(string);
			}
		}
		else buff = stringJoin(buff, vars.dict[findVar(scriptLine, cursors)].val);
	}
	return buff;
}

int substringIsNum(char *txt, int from, int to){
	for (  ; from < to; ++from) if (txt[from] < 48 && txt[from] != 46 || txt[from] > 57) return FALSE;
	return TRUE;
}

int stringIsNum(char *txt){
	for (int pos=0; txt[pos] != 0; ++pos) if (txt[pos] < 48 && txt[pos] != 46 || txt[pos] > 57) return FALSE;
	return TRUE;
}

int compareTokens(char *txtA, int *cursA, char *txtB, int *cursB){
	// Return 0 if A == B ; > 0 if A > B ; < 0 if A < B
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
	while (fgets(scriptLine, READ_SIZE + 1, scriptFile)){
		cursors[STOP] = -1;
		getNextToken(scriptLine, cursors);
		if (substringEquals("elif", scriptLine, cursors)) return FALSE;
		else if (substringEquals("fi", scriptLine, cursors)) return TRUE;
		else if (substringEquals("else", scriptLine, cursors)) return TRUE;
	}
}

int main(int argc, char **argv){
	vars.count = 0, vars.dict = NULL;
	fieldSegs.count = 0, fieldSegs.dict = NULL;
	fileSegs.count = 0, fileSegs.dict = NULL;
	loops.ptr = -1, loops.cap = 0, loops.stack = NULL;
	clock_t clockStart;
	char scriptLine[READ_SIZE + 1];
	FILE *scriptFile = fopen(argv[1], "r");
	for ( fgets(scriptLine, READ_SIZE + 1, scriptFile) ; !feof(scriptFile); fgets(scriptLine, READ_SIZE + 1, scriptFile) ) {
		int cursors[2] = {0, -1};
		if (getNextToken(scriptLine, cursors) == TERMINATOR) continue;
		else if (substringEquals("var", scriptLine, cursors)){
			do {
				getNextToken(scriptLine, cursors);
				int addr;
				if ((addr = findFieldSeg(scriptLine, cursors)) != NOT_FOUND){
					fprintf(stderr, "ERROR: '%s' already defined as field segment.\n", fieldSegs.dict[addr].key);
					exit(1);
				}
				else if ((addr = findFileSeg(scriptLine, cursors)) != NOT_FOUND){
					fprintf(stderr, "ERROR: '%s' already defined as file segment.\n", fileSegs.dict[addr].key);
					exit(1);
				}
				else if ((addr = findVar(scriptLine, cursors)) == NOT_FOUND){
					// Allocate new variable
					addr = vars.count++;
					vars.dict = realloc(vars.dict, vars.count * sizeof(struct varDict));
					vars.dict[addr].val = NULL;
					vars.dict[addr].key = substringSave(NULL, scriptLine, cursors);
				}

				if (scriptLine[cursors[STOP]] == '=' || 
				    scriptLine[cursors[STOP]] != ',' && getNextToken(scriptLine, cursors) == ASSIGNMENT){
					// User is initialising variable, as well as declaring.
					vars.dict[addr].val = varStrAss(scriptLine, cursors);
				}
				else if (vars.dict[addr].val == NULL || vars.dict[addr].val[0] != 0){
					// Initialise empty variable
					vars.dict[addr].val = realloc(vars.dict[addr].val, sizeof(char));
					vars.dict[addr].val[0] = 0;
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
			if ((addr = findVar(scriptLine, cursors)) != NOT_FOUND){
				fprintf(stderr, "ERROR: '%s' already defined as variable.\n", vars.dict[addr].key);
				exit(1);
			}
			else if ((addr = findFieldSeg(scriptLine, cursors)) != NOT_FOUND){
				fprintf(stderr, "ERROR: '%s' already defined as field segment.\n", fieldSegs.dict[addr].key);
				exit(1);
			}
			else if ((addr = findFileSeg(scriptLine, cursors)) == NOT_FOUND){
				// Allocate new file segment
				addr = fileSegs.count++;
				fileSegs.dict = realloc(fileSegs.dict, fileSegs.count * sizeof(struct fileDict));
				fileSegs.dict[addr].key = substringSave(NULL, scriptLine, cursors);
			}
			else {
				// Clear/Close this file segment
				free(fileSegs.dict[addr].delimiter);
				fclose(fileSegs.dict[addr].fp);
			}

			struct fileDict *file = &fileSegs.dict[addr];

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
				if (getNextToken(scriptLine, cursors) == QUOTE) {
					file->len = cursors[STOP] - cursors[START];
					file->delimiter = substringSave(NULL, scriptLine, cursors);
				}
				else {
					file->delimiter = stringSave(NULL, vars.dict[findVar(scriptLine, cursors)].val);
					for (file->len = 0; file->delimiter[file->len] != 0; ++file->len);
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
			if ((addr = findVar(scriptLine, cursors)) != NOT_FOUND){
				fprintf(stderr, "ERROR: '%s' already defined as variable.\n", vars.dict[addr].key);
				exit(1);
			}
			else if ((addr = findFileSeg(scriptLine, cursors)) != NOT_FOUND){
				fprintf(stderr, "ERROR: '%s' already defined as file segment.\n", fileSegs.dict[addr].key);
				exit(1);
			}
			else if ((addr=findFieldSeg(scriptLine, cursors)) == NOT_FOUND){
				addr = fieldSegs.count++;
				fieldSegs.dict = realloc(fieldSegs.dict, fieldSegs.count * sizeof(struct fieldDict));
				fieldSegs.dict[addr].key = substringSave(NULL, scriptLine, cursors);
			}
			else free(fieldSegs.dict[addr].val);

			struct fieldDict *field = &fieldSegs.dict[addr];

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
				loop->cmd = ftell(scriptFile);

				// Get type and addr
				getNextToken(scriptLine, cursors);
				loop->addr = findFileSeg(scriptLine, cursors);
				if ( loop->type = (loop->addr == NOT_FOUND) ) loop->addr = findFieldSeg(scriptLine, cursors);

				// Get index
				if (scriptLine[cursors[STOP]] == '['){
					loop->isLoop = FALSE;
					getNextToken(scriptLine, cursors);
					loop->index = (int)token2Num(scriptLine, cursors);
				}
				else {
					loop->isLoop = TRUE;
					loop->index = loop->type == FIELD_SEG? -1 : 0;
				}

				loop->chain = FALSE; 
				loops = resetLoop(scriptLine, scriptFile);

			} while ( loops.ptr != -1 && (loops.stack[loops.ptr].chain = (scriptLine[cursors[STOP]] == '.')) );

			if (loops.ptr == -1){
				for (int loopCount=1; loopCount; ){
					fgets(scriptLine, READ_SIZE + 1, scriptFile);
					cursors[STOP] = -1;
					getNextToken(scriptLine, cursors);
					if (substringEquals("in", scriptLine, cursors)) ++loopCount;
					else if (substringEquals("out", scriptLine, cursors)) --loopCount;
				}
			}
		}
		else if (substringEquals("out", scriptLine, cursors) || substringEquals("cont", scriptLine, cursors)){
			loops = loadLoop(scriptLine, scriptFile);
		}
		else if (substringEquals("if", scriptLine, cursors)){
			while (comparator(scriptLine, cursors) == FALSE   &&   nextIf(scriptLine, scriptFile, cursors) == FALSE);
		}
		else if (substringEquals("elif", scriptLine, cursors) || substringEquals("else", scriptLine, cursors)){
			for (fgets(scriptLine, READ_SIZE + 1, scriptFile) ; substringEquals("fi", scriptLine, cursors) == FALSE; fgets(scriptLine, READ_SIZE + 1, scriptFile), cursors[STOP]=-1, getNextToken(scriptLine, cursors));
		}
		else if (substringEquals("fi", scriptLine, cursors)){
		}
		else if (substringEquals("break", scriptLine, cursors)){
			do {
				if (loops.stack[loops.ptr].type == FILE_SEG) free(loops.stack[loops.ptr].buff);
			} while ( --loops.ptr >= 0 && loops.stack[loops.ptr].chain == TRUE);

			for (int loopCount=1; loopCount; ){
				fgets(scriptLine, READ_SIZE + 1, scriptFile);
				cursors[STOP] = -1;
				getNextToken(scriptLine, cursors);
				if (substringEquals("in", scriptLine, cursors)) ++loopCount;
				else if (substringEquals("out", scriptLine, cursors)) --loopCount;
			}
		}
		else {
			int destAddr = findVar(scriptLine, cursors);
			if (scriptLine[cursors[STOP]] == '=' || getNextToken(scriptLine, cursors) == ASSIGNMENT){
				char *newVar = varStrAss(scriptLine, cursors);
				free(vars.dict[destAddr].val);
				vars.dict[destAddr].val = newVar;
			}
			else {
				float augend = string2Num(vars.dict[destAddr].val), addend;
				do {
					char op = scriptLine[cursors[START]];
					char *subTxt;
					int augCurs[2];
					int toFree = retrieveToken(augCurs, &subTxt, scriptLine, cursors);
					addend = augCurs[START] == NOT_FOUND ? string2Num(subTxt) : substring2Num(subTxt, augCurs);
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
				vars.dict[destAddr].val = num2String(vars.dict[destAddr].val, augend);
			}
		}
	}
	return 0;
}
