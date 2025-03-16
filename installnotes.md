# Install notes

There are my notes from getting sable working on the ulb machine.

Installing spot
```
wget http://www.lrde.epita.fr/dload/spot/spot-2.12.2.tar.gz
tar -xvzf spot-2.12.2.tar.gz
rm spot-2.12.2.tar.gz
cd spot-2.12.2.tar.gz
./configure --with-pythondir=/usr/lib/python3.10
make
sudo make install
```

Getting it compiling proved a bit tricky - there are some weird dynamic linking behaviours. the following URL was very helpful in explaining what was going on.

On unix, it seems to compile with the following:
```
g++ -std=c++17 sable.cc -lspot -lboost_program_options -lbddx -o sable -Wl,-rpath=/usr/local/lib
```

Before it will be able to read TLSF files, you will need to install syfco, and point the `make_ltl.sh` to where it is installed. For me on OSX and Ubuntu I didn't have to change `make_ltl.sh`, but you will need to if `syfco` doesn't install in the same place as it did for me.
```
# not inside the sable directory
git clone https://github.com/reactive-systems/syfco.git
cd syfco
stack install
```