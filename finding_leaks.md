How I set up leak detection on OSX:

Using the tool `leaks`, which I think comes with OSX.

I compile an executable with `-g` and nothing else special (not using address sanitiser, i found it buggy). In this instance the compile invocation was:


`clang++ -std=c++17 -I/Users/scott/usr/include -I/opt/homebrew/Cellar/boost/1.85.0/include -L/Users/scott/usr/lib test.cc -lspot -lbddx -g -o meowmeow`

I then run the executable, but i've put in a bunch of "pause" points for when I want to check for leaks. I run it in its own terminal session, because just for the execution I want to have `export MallocStackLogging=1`, otherwise I can't get file:line references for the sources of the leaks.

In a third terminal session I run `leaks 'pgrep -f meowmeow'` (with BACKTICKS not apostrophes) to check the currently-running executable's memory use and leaks.