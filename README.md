## Benefits of `Grain` over existing file parsers
* Multiple field delimiters
* Multiple files
* Concept = parent-child hierarchies: file -> line -> column -> date -> year

## Example

### Input (my_file.txt)
```
name: Kevin Smith   dob: 1972-09-07
name: Laura Arnold  dob: 2001-04-17
```

### Script
```
file testFile = "my_file.txt";
sep line("\n"), column(), initial("")[0];
sep component("-");

scan testFile.line
  print column[1] " " column[2]>initial "\n"
  switch column[4]
    print "\t" <column[3] " year=" component[0] " month=" component[1] " day=" component[2] "\n"
    switch <
  print column[2]
```

`scan` -- all but the last level must be finite 
print -- must be finite
swtich -- must be finite

### Output
```
Kevin S
  dob: year=1972 month=09 day=07
Laura A
  dob: year=2001 month=04 day=17
```

### Program:
* `var [name] = ""` - save as string in name/string `struct` array (def 5), allocate default 100 chars. Variable names don't change so you can malloc specific.
* `file [name] = "filename"` - save as name/filePointer in files `struct` array (def 5)
* `[name] = "string"` assignment. Check vars then files (if file, close existing one first)
* `print [item]` - only one item per `print`.  Provide double-quoted string or variable. 
* `sep name("sep")[index]`
* `scan`
* `switch`
* maths `+ - / % *` - only two items 
* string concat - only two items 


### Simplify This
What are the absolute basic building blocks required for a user to carry out anything?
* `if` `> < ==` (only latter is appropriate for strings)
* `switch`
  * Shifts parent context.
* `scan a.b.c` 
    * Will loop through all occurences of c in b in a
    * `a` and `b` _must_ be indexed to a finite occurence, you cannot say "all dates in all columns in all lines", this requires multiple `scan` commands
* `=` assignment
* `print`


## Context shifting: `switch a>b`
* Shifts parent context
```
print line[0].column[3].year
print line[0].column[3].month
```
is equivalent to 
```
switch line[0].column[3]
    print year
    print month
    switch
```
* A `switch` statement on its own will revert back to the parent
* Future:
  * Use `>` and `<` instead of `.`
  * Allow accessing parent variables with < arrow?

## Looping: `scan a>b>c`
* Will iterate through all occurences of `c` in `b` in `a`
* `a` and `b` must be indexed to finite occurences, you cannot say "scan all dates in all columns in all lines" - this requires multiple `scan` commands

## Comparators: `if x == y`
* Conditional statements: `if` `else if` `else`
* Operators: `<` `>` `==` (only the latter is appropriate for strings)
* For now:
    * Can only compare two things.  
    * Nest multiple `if`.
* Future:
    * Allow && and ||

## Syntax Rules
* Semicolon terminates statements.
* Multiple `var`/`sep` declarations can be strung together with a comma.
* Variables are loosely typed.  Data type is inferred from usage.  Stored as string.  Temporarily converted to number if mathematical operator used.
* Do not use `+` symbol for string concatenation, just use adjacent variable names or double-quoted strings.

## Programming
* Begin reading file
* If `sep ` followed by whitespace, user is declaring a new separator
    * Syntax:
        * `sep column("_");`
        * `sep column();`
        * `sep year("/")[0];`
        * `sep line("\n"), column(), date("/"), year("/")[0];`
    * Read alphanumeric characters until open brackets
    * Save contents of brackets as delimiter 
    * If square brackets, save index
    * Else hit comma or semicolon 
    * Questions:
        * Should you be allowed to use lambdas and `awk`-style expressions in the index? - *No, not in version 1*
            * `sep col(i => i%2==0)` - extracts `col`s with an even index
            * `sep col( col%2==0 )` - extracts `col`s that are numerical _and_ even
            * `sep col( col~"name:" )` - extracts `col`s that include the text `name:`
        * Do we need a reserved word for when referring to column index instead of column content?  i.e. `i` used above
        * What if a user defines `sep` in a loop?
            * Stop the code from parsing the `sep` declaration each time.  You must detect whether you have already entered this loop and ignore.
        * Are `sep`s defined within scope or chronologically?  Should it use the recent definition or only the definition in this block?
    * Separator `struct` in code:
        * `name`
        * `delimiter`
        * `index`
* If `file ` followed by whitespace, user is opening a file
    * Syntax:
        * `file testFile = "my_file.txt";`
        * `file testFile = $1;`
        * `var filename = "my_file.txt"; file testFile = filename;`
    * Read alphanumeric characters until whitespace or equals
    * Ignore equals and subsequent whitespace 
    * If double quotes, user is providing a string filename.  Save characters until closing double quotes then scan to newline.
    * If dollar sign, user is providing a command line argument
    * If alphanumeric characters, user is providing a variable name.  Throw error if variable doesn't exist.
    * Continue reading until whitespace, comma or semicolon
    * Questions:
        * Is the equals necessary?  It could just be `file <var name> <filepath>`.
    * Code:
      * No file `struct` needed, can forget the filename once opened.
      * If the user reuses the variable name to open a new file, just remember to close the old one first or you'll leak it.
* If `var ` followed by whitespace, user is declaring a variable
    * Syntax:
        * `var test = "hello";`
        * `var myVariable = 12;`
        * `var secondVariable = otherVariable;`
        * `var test = "hello", myVariable = 12, anotherVar = otherVariable;`
    * Read alphanumeric characters until either whitespace or equals
    * Only hyphens or underscores allowed in variable names
    * Variable name cannot only be numerical (most other languages do not allow first character to be a number, I don't think that's necessary, though is bad practice)
    * Skip equals and whitespace
    * If comma or semicolon or newline, user is not defining variable 
    * If number ...
    * If double quotes ...
    * If alphanumeric then user is assigning the value of an existing variable.  Throw error if variable does not exist.
    * Beware that (in the case of string concatenation) the user might add more than one item: `var combined = variableOne " " variableTwo`
    * Variables do not have data types.  Data types are inferred upon usage.  Mathematical operators can only be used on numbers, otherwise treated as text.  Do _not_ use `+` as a string concatenator - I hate it. 
    * Questions:
        * Again, is the equals necessary? `var test "hello"` would be fine, this is how command line arguments such as `grep` work.
        * Is the `var` mandatory?  Many scripting languages (`awk`, `python` and `JavaScript`) will assert unrecognised words as new variables.


# Old Notes 
____________________________________
## `sep name("delimiter")[index]`

* Defines a new separator
* `Delimiter`
  * String can be one or more characters
  * Whitespace used by default if string is ommitted
  * Characters handled individually if `""` empty string is specified 
* Returns
  * String if only one occurence
  * Iterator if multiple occurrences
* Examples
  * `sep column` breaks `"hello there world"` into `"hello" "there" "world"`
  * `sep line("\n")`
  * `sep section("-")` breaks `"name date-and-time event error"` into `"name date" "and" "time event error"`
  * `sep year("/")[2]` breaks `"01/03/1994"` into `"1994"`
  * `sep name("name: ")[1] breaks `"name: Calum"` into `"Calum"`
* Index
  * Specifies occurence(s) to return
  * Lambda expressions can be used for the `index`
    * `sep column()[column % 2 == 0]` breaks `"hello there world how are you"` into `"there" "how" "you"`
    * `sep date("/")[date != 1]` breaks `"01/03/1994"` into `"01" "1994"`


## `select <child> from <parent>`
* Switches parent context to child 
* `child` is a previously defined separator
* `parent` can be inferred from context if `from <parent>` is omitted
* Returns 
  * string if only one occurence
  * loop iterator if multiple occurrences 
* Code block must be terminated with `deselect` to revert context back to previous parent
* Examples:
  * `select line from file`
  * `select column from line`
  * `select line from file; select column; print year; deselect`
