## Syntax Decisions

Line = `Robert Clarke birth_01/02/1993_15:36`
We want to print his birth year

You need child of parent
Parent can sometimes be inferred

select child # sets child as parent within context of following block

Lines:
```
robert_clarke birth_event date_01//02//1993
stacy_meadows birth_event date_11//06//2001
```

```
file testFile = "my_file.txt"
sep event("_")[1]       # declares new separator called "event", sets delimiter to underscore, extracts index 1
sep column()            # declares new separator called "column", default delimiter is whitespace
sep date("//")          # declares new separator called "date", delimiter can be a string - not just one character      

select testFile         # sets file as parent
  select column[0]      # sets first column as parent
    print event "\t"    # prints child "event" from parent "column"
    deselect            # reverts parent back to file
  select column[2]      # sets third column as parent
    print event "\n"    # prints child "event" from parent "column"
    deselect

select and scan
select = sets that instance to a parent
scan = iterates through occurences of child in parent and sets each child as parent 

select testFile
  scan line
  

```

Would print:

```
clarke  01/02/1993
meadows 11/06/2001
```


```
file testFile = "my_file.txt"
sep year("/")[2]
sep section("_")[0]

select year from section from testFile
    
```


Lines
```
Robert Jones Clarke birth_01/02/1993_15:36 marriage_29/11/2020_12:04
birth_17/12/1905_07:42 retired_16/02/1955_17:00 remarried_09/09/1975_11:35 Jonathan Smith
Helen P R Meadow birth_15/06/1986
```
We want to print:
```
Name
  Event Year
  Event Year
```
Such as:
```
Robert Jones Clarke
  birth 1993 
  marriage 2020
Jonathan Smith
  birth 1905 
  retired 1955
  remarried 1975
Helen P R Meadow
  birth 1986
```

Problems we face:
* Each person has 
    * Different event names
    * Different number of events
    * Different number of names 
* Names can appear before _or_ after events

Logic:
* Scan each line.  A line is a newline separator.
* Scan each column. A column is a whitepsace separator.
* If column doesn't contain an underscore, it is a name
* If column does contain an underscore
    * Print the event name from before the first underscore
    * Extract the date from between the first and second underscores
    * Print the year from after the last forward slash
    
Verbose logic:
```
file testFile = "myFile.txt"
sep line("\n")
sep column()
sep section("_")
sep name("_")[0]
sep year("/")[2]

for line in testFile
  $events = ""
  $names = ""
  for column in line
    if column has section
        $events = $events "\t" name " "
        select section[1]
            $events = $events year "\n"
    else
        $names = $names " " column
  print $names "\n" $events
```

 

#### Example One:

```
sep line("\n")
sep column(" ")
sep section = "_"
sep date = "/"

for line in file
  print column[0] "\n"
  for column in line
    if column has section
      print "\t" section[0] " "
      select section[1]
        print date[1] "\n" 
```

#### Example Two:

```
sep line("\n")
sep name()[0]
sep event("_")[0]
sep month("/")[1]

for line in file
  print name "\n"
  for column in line
    if column has section
      print "\t" event " "
      select 
```

```
sep line("\n")
sep column()[3]
sep 


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
