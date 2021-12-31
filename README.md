# Grain

#### A file parsing scripting language

Scanning files to extract or modify columns is a common task.  Existing languages, such as `awk`, do this well.  While the user _can_ declare which character string should be used to distinguish column fields (such as whitespace or commas), a file with multiple field delimiters presents a challenge.  For example, extracting the day component from the date column below requires both whitespace delimiting and forward slash delimiting.

```
Forename	Surname Date
John		Smith	06/11/1982
```

`Grain` is primarily designed with this challenge in mind.

## Specification

### General Syntax

* Usage: `./grain script.gr`
* Statements are terminated by a newline
* Comments are initiated with a semicolon `;` and all remaining text on that line is ignored by the command interpreter
* `Grain` is case-sensitive.  All commands are lowercase

### Output: `print`

Items following a `print` command will be output to the console.  This includes strings, numbers and variable values.  Valid strings are surrounded by matching double quotes `"`, single quotes `'` or backticks.  Within this outer pair of quotes, quote characters of the other two types can be freely used.

Escape sequences (such as tab `\t` and `\n` characters) are automatically converted.  The `print` command does *not* append a newline by default.  Multiple adjacent items separated by whitespace can be printed in one command.

```
var age = 87
print "My age is " age ".\n"
```
```
Output:
>>> My age is 87.
```

The asterisk `*` is interpreted by `print` as a special character that refers to the current before (discussed later).

Floating point numbers are printed up to five decimal places; trailing zeroes are not printed.

### Variable Declaration: `var`

Variables are declared with the `var` command.  Valid variable names contain only alphanumeric characters but the _first_ character cannot be a number.  The underscore `_` character is a valid character in variable names.  `Grain` is case-sensitive, therefore `apple` and `Apple` would be two different variables.

`Grain` is a loosely typed language; all variables are referred to as `var`.  Data types are inferred at _usage_ time, not declaration time.  One variable could therefore be treated both numerically _and_ as a text string in different usage contexts throughout its lifetime.

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

### Variable Assignment

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

There are two assignment modes: string mode and maths mode.  A mixture of maths mode and string mode can *not* be used within the same command.

Variable declaration is carried out from left to right, making the follow example valid.

```
var foo = "hello", bar = foo
print foo "\n"
```
```
Output:
>>> hello
```


#### Strings & Concatenation

Adjacent values after the assignment operator will be concatenated.  Do *not* use the plus `+` symbol to concatenate strings.  The variable's own name can be included in the assignment, such as in the append example below.

```
var foo = "hello"
foo = foo " world\n"
print foo
```
```
Output:
>>> hello world

```

#### Arithmetic

Prepending the assignment operator with a mathematical operator enters maths mode: `+= -= *= /= %=`.  Strings are automatically converted to numbers when maths mode is instigated.  Suitable strings must only contain numerical characters or a decimal point.  `Grain` supports floating point arithmetic up to five decimal places.

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

Adjacent numbers, separated by whitespace without mathematical operators, will be treated as strings and undergo concatenation.

```
var foo = 1 1 "\n"
print foo
```
```
Output:
>>> 11
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

Stream segments can also included in both maths mode and string mode variable assignments (below).

### Stream Segments

A stream refers to text being scanned from a file.  A segment refers to a portion of that file stream.  Files are often parsed line-by-line; a line is therefore an example of a segment.  Lines are often scanned column-by-column; a column is another example of a segment.  `Grain` has `file` and `field` segments.

Segments may initially be difficult to understand but should become far more intuitive once learning about the `in` command later.

#### File Segments

File declarators provide a filename and a field delimiter.  The given filename is automatically opened and can be parsed in your script.  `Grain` permits parsing of multiple files in one script.


Field delimiters define the basic segment into which a file should be parsed.  For example, files are usually parsed line-by-line, so the newline `\n` character is the delimiter in this example.

The example below would create a `file` segment variable called `myFile`, open "example.txt" and loading would occur up to full-stop `.` characters - effectively parsing the file sentence-by-sentence.

```
file myFile("example.txt", ".")
```

The delimiter is optional.  If no delimiter is provided (`file newFile("example.txt")`), a newline delimiter is used by default.  If an empty delimiter is provided (`file another("example.txt", "")`) then the file would be parsed character-by-character.

#### Field Segments

Field declarators provide a delimiter with which to parse a stream.  This stream could be defined by another field segment or directly by a file segment.  At least one file segment must be present to use a field segment (otherwise there is no text to parse).

The example below declares a field segment variable that breaks a stream at forward slashes.

```
field separator("/")
```

If no delimiter is provided (`field second()`) then any whitespace is used as the delimiter.  Whitespace is defined by any string consisting of one or more space, tab or newline characters.  If any empty delimiter is provided (`field letters("")`) then each character is treated individually.

#### Delimiters

Delimiters can be directly written as strings or a variable name whose value is a string.  Delimiters can be multiple characters.  The below examples are all valid delimiters.

```
var testVariable = "hello"
file novel("Great_Expectations.txt", testVariable)
field segment("Pip")
field segment("-")
```

### Index Offsets

A particular occurence of a segment can be specified with an index offset, as shown below.  

``` 
file input("example.txt")
print input[1]
```

Offsets are 0-indexed, thus the above example will print the _second_ line from the given file.  It could be thought of as _"Print one after the next line"_, therefore `print input[0]` would print the next line.

A *crucial* difference between `file` and `field` segments is that `field` segments simply refer to coordinates in a `file` segment, whereas `file` segments load directly from disk, sequentially.  This traversal occurs in a forward direction only.  Therefore two calls to `print file[0]` will actually print different results because _"the next line"_ moves forward with each call.  This does not occur with `field` segments. 

### Buffers & Loops: `in`

The `in` command provides a lot of functionality to `Grain`.  It can be used to define a buffer and also initiate a loop.  Our segments must be found _in_ something, right?  Lines must be _in_ a file, columns must be _in_ a line, dates must be _in_ a column.  So `in` defines the buffer on which our future commands will be executed.  The below example will print the fourth column from each line in the file.  No `file` or `field` delimiters are provided so the default newline and whitespace delimiters are used, respectively.  

```
file text("example.txt")
field column()

in text
	print column[3]
out
```

Both `file` and `field` segments are valid for use with the `in` command.  Particular segments can be specified with index offsets.  However using segments without an index will instantiate a loop over all occurences.  The `in` command is the _only_ place in grain where segments can be used without an offset like this.

The `out` syntax defines the end of a loop.  Indentation is *not* mandatory in `Grain` but is shown here for clarity.

Nested loops are permitted.  Alternatively, multiple segments can be chained together with the dot `.` syntax.  They are set up in a parent-child hierarchy.

```
file text("example.txt")
field column()
field date("/")

in text
	in column[3]
		print date[1]
	out
out

in text.column[3]
	print date[1]
out
```

Both loops in the above example have equivalent functionality.  If a `date` segment occured in every column, then the user could have removed the index to iterate through all columns.  Indexed and non-indexed segments can be used in any order, as shown below.  Using the `print` command with the asterisk `*` symbol outputs the entirety of the buffer defined by the current loop.  The following example prints a line count adjacent to each slash-delimited section of a `date` from the fourth `column` from each line of `text`.

```
file text("example.txt")
field column()
field date("/")

var lineCount = 0

in text.column[3].date
	lineCount += 1
	print * " " lineCount "\n"
out
```
```
Input file:
John Smith 06/11/29
Caitlin Maurice 22/04/04
```
```
Output:
>>> 06 1
>>> 11 1
>>> 29 1
>>> 22 2
>>> 04 2
>>> 04 3
```

#### Conditional Statements: `if`, `elif`, `else` and `fi`

Valid comparators include variable values, strings, `file` segments, `field` segments and numbers.  

Valid operators include equality `==`, less than `<`, greater than `>`, less than or equal `<=`, greater than or equal `>=` and not equal `!=`.

Should an `if` statement return false, further tests could be carried out with "else if" `elif` statements.  A final `else` statement will execute commands if all prior statements are false.

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
