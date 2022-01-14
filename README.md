# Grain

#### A file parsing scripting language

Scanning files to extract or modify columns is a common task.  Existing languages, such as `awk`, do this well.  While the user _can_ declare which character string should be used to distinguish column fields (such as whitespace or commas), a file with multiple field delimiters presents a challenge.  For example, extracting the "day" component from the Date column below requires both whitespace delimiting and forward slash delimiting.

```
Forename	Surname 	Date
John		Smith		06/11/1982
```

`Grain` is primarily designed with this challenge in mind.  It is a personal hobby project; bugs are guaranteed and output reliability is not.  Major backward-incompatible revisions to the language should be expected.

## Specification

### General Syntax

* Usage: `grain script.gr`
* Statements are terminated by a newline.
* Comments are initiated with a semicolon `;`. All remaining text on that line is ignored by the interpreter.
* `Grain` is case sensitive.  All commands are lowercase.
* The `exit` command will immediately quit from anywhere in the script.

### 1) Output: `print`

Items following a `print` command will be output to the console.  This includes strings, numbers, variable values and iterators - the latter of which are described below.  Valid strings are surrounded by matching double quotes `"`, single quotes `'` or backticks \`.  Within this outer pair of quotes, quote characters of the other two types can be freely used.  Spurious quotation marks can be escaped with a backslash `\` if necessary.

Escape sequences (such as tab `\t` and newline `\n` characters) are automatically converted.  The `print` command does *not* append a newline by default.  Multiple adjacent items separated by whitespace can be printed in one command.

```
var age = 87
print "My age is " age ".\n"
```
```
Output:
>>> My age is 87.
```

The dollar `$` is interpreted by `print` as a special character that refers to the current iterator (discussed later).

Floating point numbers are printed up to five decimal places; trailing zeroes are not printed.

### 2) Variable Declaration: `var`

Variables are declared with the `var` command.  Valid variable names contain only alphanumeric characters but the _first_ character cannot be a number.  The underscore `_` character is also valid in variable names.  `Grain` is case sensitive, therefore `apple` and `Apple` are two different variables.

`Grain` is a loosely typed language; all variables are referred to as `var`.  Data types are inferred at _usage_ time, not declaration time.  One variable could therefore be treated both numerically _and_ textually in different usage contexts throughout its lifetime.

```
var foo
var bar
```

Multiple variable declarations can be chained with the comma separator.

```
var foo, bar
```

All variables are declared as empty.  Redeclaring the same variable will empty its value.

```
var foo
foo = "hello world"
print foo "\n"
var foo
print foo "\n"
```

```
output:
>>> hello world
>>> 
```

Variable declaration and assignment can be combined (below).

### 3) Variable Assignment

Values are assigned to variables with the assignment `=` operator.  Valid values include strings, numbers or other variable names.  Assigning another variable name will copy that variable's value into the assignee. 

```
var foo = "hello"
var bar = foo
print bar "\n"
```
```
Output:
>>> hello
```

Variable declaration is carried out from left to right, making the following example valid.

```
var foo = "hello", bar = foo
print bar "\n"
```
```
Output:
>>> hello
```

There are two assignment modes: string mode and maths mode.  These examples have demonstrated string mode, using the bare assignment operator.  Prepending a mathematical operator to the assignment operator initiates maths mode (shown later).  A mixture of maths mode and string mode can *not* be used within the same command.

#### 3.1 String Mode & Concatenation

Adjacent values after the assignment operator are concatenated.  Do *not* use the plus `+` symbol to concatenate strings.  The variable's own name can be included in the assignment, such as in the append example below.

```
var foo = "hello"
foo = foo " world\n"
print foo
```
```
Output:
>>> hello world

```

#### 3.2 Maths Mode

Prepending the assignment operator with a mathematical operator enters maths mode: `+= -= *= /= %=`.  Strings are automatically converted to numbers when maths mode is initiated.  Suitable strings must only contain numerical characters or a decimal point.  `Grain` supports floating point arithmetic up to five decimal places.  Note that the modulo `%` operator can only be used on whole integers.  If a modulo attempt is made with a floating point decimal, the fractional part will be ignored (no rounding will take place).

```
var foo = "Result: ", bar = 4
bar += 8
print foo bar "\n"
```
```
Output:
>>> Result: 12
```

Multiple arithmetic operations can be strung together with mathematical operators.  Note that arithmetic is strictly carried out from left to right, BODMAS is not implemented.  Brackets are not valid in arithmetic operations.

```
var foo = 8
foo /= 2 + 6 - 9
print foo "\n"
```
```
Output:
>>> 1
```
As variables are initialised to empty, mathematical operations can be carried out in the declaration.

```
var foo = 12, bar += 8 - 4 + foo
print bar "\n"
```
```
Output:
>>> 16
```

Only fractional numbers will be printed with a decimal point.  

```
var foo = 6.5, bar += foo - 0.5
print "foo = " foo "\n"
print "bar = " bar "\n"
```
```
Output:
>>> foo = 6.5
>>> bar = 6
```

Adjacent numbers, separated by whitespace without mathematical operators, will be treated as strings and undergo concatenation.

```
var foo = 1 1 "\n"
print foo
```
```
Output:
>>> 11
```

Iterators can also be included in maths mode and string mode variable assignments (below).

### 4) Iterators

Files are often parsed line-by-line; a line is therefore an example of an iterator.  Lines are often scanned column-by-column; a column is another example of an iterator.  `Grain` has `file` and `field` iterators.

Understanding these iterators will become more intuitive upon learning about the `in` command later.

#### 4.2) File Iterators

A file iterator can be declared by providing a filename and an optional delimiter.  The given filename is automatically opened and can be parsed in your script.  `Grain` permits parsing of multiple files in one script.

The delimiter defines the basic segment into which a file should be parsed.  For example, files are usually parsed line-by-line, so the newline `\n` character is the delimiter in this example.

The example below would create a `file` iterator called `myFile`, then open "example.txt" and loading would occur up to sequential period `.` characters, which effectively parses the file sentence-by-sentence.

```
file myFile("example.txt", ".")
```

The delimiter is optional.  If no delimiter is provided then a newline delimiter is used by default.  If an empty delimiter is provided then the file is parsed character-by-character.  If the given delimiter does not occur in the file, the entire file is loaded into memory.  Moreover, providing the special asterisk `*` character as the delimiter will also load the entire file into memory.

```
file newFile("example.txt") 	; No delimiter provided, newline character used by default
file another("example.txt", "") ; Empty delimiter provided, file loaded character-by-character
```

Redefining a `file` iterator with the same name is allowed.  The new filename and delimiter will be updated and used from thereon.  Providing the same filename in this redefinition is the equivalent of reopening the file and rescanning from the beginning.

#### 4.3) Field Iterators

A field iterator provides a delimiter with which to parse a text stream.  It is declared like a file iterator, minus the filename.  At least one file iterator must be present to use a field iterator (otherwise there is no text to parse).  The parsed stream could also be defined by another "parent" field iterator (see the `in` command for examples).  

The example below declares a field iterator variable that breaks a stream at forward slashes.

```
field separator("/")
```

If no delimiter is provided (`field second()`) then whitespace is used as the delimiter.  Whitespace is defined as a series of one or more spaces, tabs or newline characters.  If an empty delimiter is provided (`field letters("")`) then each character is treated individually.

Redefining a `field` iterator with the same name is allowed.  The provided delimiter will be updated and used from thereon.

#### 4.4) Delimiters

Delimiters can be directly written as strings or a variable name whose value is a string.  Delimiters can be multiple characters.  The below examples are all valid delimiters.

```
var testVariable = "hello"
file novel("Great_Expectations.txt", testVariable)
field segment("Pip")
field segment("-")
```

### 5) Index Offsets

A particular occurence of an iterator can be specified with an index offset, as shown below.  

``` 
file input("example.txt")
print input[1]
```

Offsets must be integers.  A variable name can be provided if it represents an integer.  A string may be provided if it can successfully be converted to an integer.  If a floating point number is provided, the number will be used after discarding the fractional part (no rounding will occur).

Offsets are 0-indexed, thus the above example will print the _second_ line from the given file.  It could be thought of as _"Print one after the next line"_ and so `print input[0]` would print the next line.

The 0<sup>th</sup> index scans from the beginning of the stream to the first occurence of the delimiter _or_ the end of the stream, whichever occurs first.  This means `segment[0]` should always exist for all defined iterators, even if the delimiter does not occur in the stream.  The delimiter itself is *not* included in the buffer stream.

One crucial difference between `file` and `field` iterators is that `field` iterators simply refer to coordinates in a `file` iterator, whereas `file` iterators load directly from disk, sequentially.  This traversal occurs in a forward direction only.  Therefore two calls to `print file[0]` will print different results because _"the next line"_ moves forward with each call.  This does not occur with `field` iterators. 

```
Input File:
this is the first line
this is the second line
this is the third line
```
```
file thisFile("example.txt")
print thisFile[1] "\n" thisFile[0] "\n"
```
```
Output:
>>> this is the second line
>>> this is the third line
```

As mentioned, redefining a `file` iterator with the same filename will reopen the file, thus the example above becomes:  

```
file thisFile("example.txt")
print thisFile[1] "\n" 
file thisFile("example.txt")
print thisFile[1] "\n"
```
```
Output:
>>> this is the second line
>>> this is the second line
```

### 6) Buffers & Loops: `in`

The `in` command provides a lot of functionality to `Grain`.  It can be used to define a buffer and also initiate a loop.  Our iterators must be found _in_ something, right?  Lines must be _in_ a file, columns must be _in_ a line, dates must be _in_ a column.  So `in` defines the buffer on which our future commands will be executed.  The below example will print the fourth column from each line in the file.  

```
file text("example.txt")
field column()

in text
	print column[3]
out
```

Both `file` and `field` iterators are valid for use with the `in` command.  Particular segments can be specified with index offsets.  However iterators without an index will instantiate a loop over all occurences.  The `in` command is the only place in `Grain` where iterators can be used in the absence of an index like this.

The `out` syntax defines the end of a loop.  Indentation is not mandatory in `Grain` but is shown here for clarity.

Nested loops are permitted.  Alternatively, multiple iterators can be chained together with the dot `.` syntax.  They are set up in a parent-child hierarchy.

```
file text("example.txt")
field column()
field date("/")

in text
	in column[3]
		print date[1]
	out
out

file text("example.txt") ; reopen file
in text.column[3]
	print date[1]
out
```

Both loops in the above example have equivalent functionality.  If a `date` segment occured in every column, then the user could have removed the index to iterate through all columns.  Indexed and non-indexed iteraotrs can be used in any order, as shown below.  Using the `print` command with the dollar `$` symbol outputs the iterator defined by the current loop.  The following example prints a line count adjacent to each slash-delimited section of a `date` from the fourth `column` from each line of `text`.

```
Input file:
John Smith 06/11/29
Caitlin Maurice 22/04/04
```
```
file text("example.txt")
field column()
field date("/")

var lineCount = 0

in text.column[3].date
	lineCount += 1
	print $ " " lineCount "\n"
out
```
```
Output:
>>> 06 1
>>> 11 1
>>> 29 1
>>> 22 2
>>> 04 2
>>> 04 2
```

The `break` command causes the program to immediately exit the current loop.

The `cont` (continue) command causes the program to immediately begin executing the next iteration of the current loop.

### 7) Conditional Statements: `if`, `elif`, `else` and `fi`

Valid comparators include variable values, strings, `file` iterators, `field` iterators and numbers.

If both comparators can be converted to numbers, they will be treated as numbers, otherwise both comparators will be treated as text.

Valid operators include equality `==`, less than `<`, greater than `>`, less than or equal `<=`, greater than or equal `>=` and not equal `!=`.

In the event of text comparison, the `<` and `>` operators refer to alphabetical ordering.

There are two further operands, `inc` (includes) and `exc` (excludes) to check whether string A contains or does not contain the given substring B.

Should an `if` statement return false, further tests can be carried out with "else if" `elif` statements.  A final `else` statement will execute commands if all prior statements are false.

All conditional blocks must be terminated with the `fi` statement.

Multiple statements can be chained with the `and` or `or` logical operators.  The `and` operator has precedence over the `or` operator.

```
print "You have "
var num = 17
if num < 10 and num != 0
	print "a few "
elif num > 10 or num == "millions"
	print "lots of "
else 
	print "no "
fi
print "books.\n"
	
```
```
Output:
>>> You have lots of books.
```

### 8) Scope of Variables & Segments

`Grain` implements _no_ scoping.  All variables and iterators are globally accessible throughout the script from when they are defined.  

In the case of redefinition, the most recent definition is used.  Even if redefinition occurs within an `in` loop or `if` block, the updated values will persist once outside of that block or loop.

## Future Improvements

### Direct Stream Editing

An iterator can be copied into a variable and modified then printed.

```
var variable = column[2]
variable += 7
```

It would be good to directly modify iterators and print them inline with the rest of the stream.

```
column[2] += 7
print $
```

This would currently require looping through all other columns before/after `column[2]` and printing those.

### Single line commands from Standard Input

Currently `Grain` can only be passed a file of commands, each of which are on a separate line.  It would be good to permit a semicolon terminator so commands can be written inline and piped directly into `Grain` on the command line, without need for a script file.  (This would of course require comments to be defined via `/` instead of `;`.

### Allow strings and variables in the `in` command

Array-like looping behaviour would be enabled if the following syntax was permitted.

```
var line = "one_two_three_four"
field delim("_")
field column()
in "0 1 2 3".column
	print line.delim[column] "\n"
out
```
```
Output:
>>> one
>>> two
>>> three
>>> four
```
