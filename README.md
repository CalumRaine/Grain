Only two ways to split a string

Up to next --> Up to next --> Up to next

OR

Between x and y

Switch parent contexts

## Benefits of `Grain` over existing file parsers
* Multiple field delimiters are allowed
* Multiple files are allowed
* The purpose-oriented syntax is simple

## Concepts
File parsing is about parent and child relationships.

| Parent | Child | Delimiter | Example |
| ------ | ----- | --------- | ------- |
| File  | Line  | "\n" | File = Name Age Location \n Jonathan 22 Sunderland \n Stacey 39 Manchester |
| Line  | Column | " " | Line = Hello there world how are you |
| Column  | Date | "\_" | Column = Carl_07/11/1990_Scotland |
| Date | Year | "/" | Date = 07/11/1990 |

`Grain` allows switching contexts between parents to extract different substrings.

## Example

### Input (my_file.txt)
```
name: Kevin Smith   dob: 1972/09/07
name: Laura Arnold  dob: 2001/04/17
```

### Script
```
file testFile = "my_file.txt"
sep line("\n")
sep column
sep year("/")[0]

select line from testFile
  print column[1] " "
  select column[4]
    print year "\n"
```

### Output
```
Kevin 1972
Laura 2001
```

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
