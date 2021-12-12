# Grain
## A file parsing scripting language

<table>
  <tbody>
    <tr>
      <td>Roger</td>
      <td>Clarke</td>
      <td>01/07/86</td>
      <td>15:14</td>
    </tr>
    <tr>
      <td>Arnold</td>
      <td>Webber</td>
      <td>09/05/93</td>
      <td>12:02</td>
    </tr>
    <tr>
      <td>Jason</td>
      <td>Marshall</td>
      <td>03/12/75</td>
      <td>21:33</td>
    </tr>
  </tbody>
</table>
  
```
def column()            # should you be able to define column as default without ().  Default is whitespace.
def line("\n")
def date()[0]
def minute(":")[0]

for line in file
  select date in column[2]
  
for/in and select/from 
for will iterate, select will extract

copy date into testVar // now changes do not modify original line

select line from "my_file.txt"
  print "surname is " column[1]
  select date from line
    print "date is" date
    
select line
    column[1] = column[1] "'s"
  select column[3]
    select minute
      minute = minute + 4
  
select minute from column[3]
  minute = 
  
 
select means load into buffer (treat as temporary variable)

Two ways to reach minute:

select line
  select column[3]
    select minute
      minute = minute + 4
      print minute
      
print minute from column[3] from line in "my_file.txt"
    
open vs scan

scan means iterate through occurences

scan "my_file.txt"
  print date
  
scan column in line
  print "This column is " column
```
