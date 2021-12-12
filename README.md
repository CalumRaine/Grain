# Grain
## A file parsing scripting language

<table>
  <tbody>
    <tr>
      <td>Roger</td>
      <td>Clarke</td>
      <td>01/07/86</td>
      <td>stamp_15:14</td>
    </tr>
    <tr>
      <td>Arnold</td>
      <td>Webber</td>
      <td>09/05/93</td>
      <td>stamp_12:02</td>
    </tr>
    <tr>
      <td>Jason</td>
      <td>Marshall</td>
      <td>03/12/75</td>
      <td>stamp_21:33</td>
    </tr>
  </tbody>
</table>

## Breaks Streams into User-Defined Segments

* The root stream is always the file
* A typical example includes: file --> line --> column --> time --> hour 
* If we wanted to shift time zones in a file, we could:

```
file = "my_file.txt"
def line("\n")
def column
def time("_")
def hour(":")

for line in file
  select column[3]
    select time[1]
      select hour[0]
        hour = hour + 1
  print line
```

Or, more succinctly:

```
def line("\n")
def lastColumn()[-1]
def time("-")[1]
def hour(":")[0]

for line in "my_file.txt"
  select hour from time from lastColumn from line
    hour = hour + 1
  print hour 
```

However, you don't need to use lines.  As stated, the file itself is the root segment.  Here you could print everything that occurs between brackets in a file:

```
def openBracket("(")
def closeBracket(")")[0]

for openBracket in "my_file.txt"
  for closeBracket in openBracket
    print closeBracket
```

## Defining Separators `def name(separator)[index]`

* Minimal required syntax is `def name` or `def name()`, which uses whitespace as the default `separator`
* `separator` is a string of one or more characters that defines the delimiter
* An empty `separator`, such as `def name("")` will treat each character individually 
* An index can be defined to automatically isolate a particular occurence 
* An index can be a lambda expression instead of just a number

#### Examples

input.txt = "the fat, black cat liked: milk, cheese and grapes"

* `def column` or `def column()` --> _(see Iterator example)_
* `def field(",") --> _(see Iterator example)_
* `def sep("and ")[1] --> "grapes"
* `def sep("")`[i => i%2==0] --> "h", "f", "t", "l", "c" ...


## Iterator `for x in y`

* Breaks `y` into segments of `x` and iterates through them

#### Examples
* `for column in "input.txt"` --> "the " "fat, " "black " "cat " "liked: " "milk, " "cheese " "and " "grapes"
* `for field in "input.txt" --> "the fat," " black cat liked: milk," " cheese and grapes"
 

## Extractor

`select x from y`

* Redefines buffer for subsequent commands
* Used to build compound separators (e.g. subcolumns of columns)
* Note: Changes still modify the original file stream

```
select line from 


## `z = select x from y`

* Save a copy of the extractor result in `z`
* Without this, changes to `x` would modify the original file stream

## Loose typing 

* No int/string types declared
* Type deduced from operator usage
    * Maths: + - / * % > < (throw error if operator used on NaN)
    * Text: "" (do not concatenate with '+')

## Segment Reassignment

`column[2] = "there " column[2]`

* If the input is "hello world", the output would be "hello" "there world"; still only two columns

## Segment Insertion

`insert "there " before column[2]

* If the input is "hello world", the output would be "hello" "there" "world"; now three columns

Also valid:

`insert "there " after column[2]`
```
x = "there "
insert x before column[2]
```

## Inline Substitution

`insert $(select x from y) before column[2]`
