This implementation of my shell uses a similar reading mechanism to spell check, where we use
buffers and read() to obtain input from the file that we are given to read from, which depends on
the number of argument lists passed in. We perform a split and execute function on each of these
lines, where we tokenize each word in the line and add it to an argument list. We also
check each token and if there is redirection, we store the filename for redirection and break if we
detect a pipe. We also store the redirection filenames differently if there is a pipe detected (we
check this before creating the argument list). If there is a pipe present, we make a second
argument list for the right side of the pipe and send both argument lists as well as each of their
filenames used for redirection into a separate piping execution function, where we use pipe and
fork() as well as the file names to execute the pipe and set the output of the left side as the input
to the right side. If there is no pipe, we simply execute the command in a function called
execute_c, where we call fork and have built in functions as well (which we implemented by
comparing the first argument of the string to the words we were trying to implement). The built
ins include (pwd, cd, exit, and which). We also had a function where we got the pathnames of
the token we wanted to execute because we could only use execv, not execvp.
To handle conditionals we made a global variable that kept track of the status of the previous
command and executed the next command based on the value of prevstatus and on the conditional detected
the add space function ensure that commands such as foo>bar are read just like foo > bar

Design Choices:
- Our piping implementation assumes a correct user input.
- Typing the command cd without anything after prints out a message and redirects you home
- To simplify memory management and improve performance, fixed-size buffers are used instead
of dynamic memory allocation in certain parts of the code. This approach reduces complexity and
avoids potential memory issues. We statically allocated everything instead of using strdup() to
prioritize time over space, no more than 200 arguments per line is supported because the maximum
buffer length is 200.


Some test cases we tried:

./mysh gave us interactive mode and exited successfully after writing exit (works if arguments are passed after)

./mysh

Mysh > exit bye gave us bye and then exited

./mysh test.txt gave use batch mode and ended after executing

If echo hello was in test.txt, the terminal would print out hello

./mysh works in interactive mode INSIDE ./mysh

Running mysh

Mysh > ./mysh test.txt

Also executes test.txt in batch mode, and does not exit upon execution because still in original ./mysh

./mysh dev/tty

Will display welcome to my shell

Executes ./mysh because the second argument is the terminal

Running mysh

Mysh > pwd

Displays current working directory and continues

Mysh > cd ..

Mysh > pwd

Displays current working directory (changed to parent of previous) and continues

./mysh

Welcome to mysh!

Mysh > echo hello bye > newfile

Cat newfile displays hello in mysh

./mysh

Welcome to mysh!

Mysh > wc -l < newfile > output

Cat output displays 1 in mysh

./mysh

Welcome to mysh!

Mysh > ls | grep file

displays all files in current directory with “file”

./mysh

Welcome to mysh!

Mysh > ls | grep file > list

Displays nothing

Mysh > cat list

displays all files in current directory with “file”

Ls | sort —> sorts all files

Ls *.txt | sort —> sorts all files with .txt

Ls | sort > output —> sorts all files and sends to output

echo hello > bar : sends hello to bar

echo hello > bar > foo : sends helo to foo

./c1 then ./c1 prints for both programs

./c1 else ./c2 only prints program 1
