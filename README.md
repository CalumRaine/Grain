# Grain

#### A file parsing scripting language that uses parent-child hierarchies to permit multiple field delimiters and files

## Example

### Input (my_file.txt)
```
name: Kevin Smith   dob: 1972-09-07
name: Laura Arnold  dob: 2001-04-17
```

### Script
```
file testFile("my_file.txt", *)
sep line("\n")
sep column()
sep initial("")[0]
sep component("-")

in testFile
	in line
		print column[1] " "
		in column[2]
			print initial "\n"
		out

		print "\t" column[3]
		in column[4]
			print " year=" component[0] " month=" component[1] " day=" component[2] "\n"
		out
	out
out
```

### Output
```
Kevin S
  dob: year=1972 month=09 day=07
Laura A
  dob: year=2001 month=04 day=17
```

### To Do
* Initialise variables to 0, by default
* Implement whitespace scanning for seps

### Roadmap
1. `var name = ""`
    * Save as key(name):value("string") `struct` array
    * Everything saved as and treated as a string.
    * Temporarily converted to numbers if command includes mathematical operators.  Result saved back as string.
    * Multiple declarations can be strung together with a comma.
3. `file name("filename", "separator")`
    *  `separator` is the most basic chunk that will be loaded from the file at any time
    *  This is essential because the user cannot go backwards
    *  Writing `*` will load the entire file into memory
        *  Meaning lines could be directly addressed in any order just like columns usually are.  
        *  Be careful with big files.
    *  Usually "\n" would be the default `separator`
    *  Other `separator`s could be `}` if you are reading JSON format
    *  If file variable is redefined, remember to close the existing one first
5. `name = "string"` assignment.
7. `print item`
    * Provide double-quoted string or variable. 
9. `sep name("delimiter")[index]`
    * `delimiter` string used as separator
    * Can be multiple characters
    * Empty `""` treats characters individually
    * If `sep` redefined:
        * Should the most recent definition be used?
        * Or only the definition within the current scope?
    * Future: allow regex expressions, such as [a-d], !/hello/ etc
11. `scan [parent].[child].[grandchild]` --- DEPRECATED by IN command
    * Initiaes a loop over `granchild` in `child` in `parent`
    * Any number of parent-child hierarchies are allowed
    * `parent` and `child` _must_ be a single entity, such as `col[0]` and not all columns
    * Future: possibly allow `parent` and `child` to be iterators too, i.e. nested loops declared on one line 
13. `switch [parent]` ---- DEPRECATED by IN command
    * All subsequent child delimiters are in reference to this parent
    * Similar to `cd` in `BASH`
    * `print line[0].col[3] line[0].col[4]` == `switch line[0]; print col[3] col[4]`
    * Use `switch` on its own to revert back to previous parent context (like `cd ..`)
    * Future: Allow accessing parent variables from within child block, like `print ../line[2] col[4]`
    * Note: might be confused with existing `switch/case` statement in other languages.  Maybe `loop/load` are a better pair of commands?
15. maths `+ - / % *`
    * Will cause variable to be temporarily converted to number, or throw error if impossible
    * Only one operator in any command
    * Future: implement BODMAS to allow for building equations with multiple operators.
17. string concatenation `myVariable = myVariable " plus this string here"` 
    * Do _not_ use `+` operator.  This should be banished from all languages.
    * Note: could mathematical operators have string interpretations?
        * `+` = concatenation
        * `-` = remove
        * `/` = split
18. Comparators `if > < ==` 
    * Only latter is appropriate for strings
    * Future:
        * Allow `&&` and `||` chaining.  Nested `if`s would be required at the moment.
        * Allow `<` and `>` for checking alphabetical order of strings
19. Use semicolon to terminate statements
    * Makes it easier for inline scripting on the command line, instead of writing a script in a file 
20. Allow indexes to use expressions
    * `scan col[ col%2 == 0]` only loops through columns that are divisible by 2
    * `scan col[ col~"hello" ]` only loops through columns that contain the word "hello"
21. Consider using `>` and `<` as the parent-child and child-parent connectors
    * `print line>col[4]>date` instead of `print line.col[4].date`
    * Accessing parent variables would be allowed: `print <line[2] col[3]`
    * `switch <` would revert back to previous parent context
    * Potential confusion with greater/less than comparators
