# Grain
## A file parsing scripting language

<table>
  <tbody>
    <tr>
      <td>Roger</td>
      <td>Clarke</td>
      <td>01/07/86 - 15:14</td>
    </tr>
    <tr>
      <td>Arnold</td>
      <td>Webber</td>
      <td>09/05/93 - 12:02</td>
    </tr>
    <tr>
      <td>Jason</td>
      <td>Marshall</td>
      <td>03/12/75 - 21:33</td>
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
def time("-")
def hour(":")[0]

for line in file
  select column[2]
    select time[1]
      select hour
        hour = hour + 1
  print line
```

Or, more succinctly:

```
def line("\n")
def time("-")[1]
def hour(":")[0]

for line in "my_file.txt"
  select hour from time from line
    hour = hour + 1
  print line
```

However, you don't need to use lines.  As stated, the file itself is the root record.  Here you could print everything that occurs between brackets in a file:

```
def openBracket("(")
def closeBracket(")")[0]

for openBracket in "my_file.txt"
  for closeBracket in openBracket
    print closeBracket
``

## Defining Separators

1. `def field` defines a separator called `field`.  No separator is specified so whitespace is used by default
2. `def field(",") defines a comma separator called `field`
3. `def hours(":")[0] defines a separator called `hours` that will extract everything preceding the first colon 
4. `def year("/")[2] defines a separator called `year` that will extract everything between the 2nd and 3rd slash
5. `def letter()` or `def letter("")` defines an empty separator called `letter` that would treat characters individually 

## Iterator

`for x in y`

* Breaks `y` into segments of `x` and iterates through them
 

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
