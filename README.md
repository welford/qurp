=======================================
QURP
=======================================
###Quake Raspberry Pi Edition.

A barebones port of Quake 1 from id Software for Raspberry Pi. Made with the intention of    

This port is from the original source found here : https://github.com/id-Software/Quake All the original legal stuff can be found in the original readme files.

##Raspberry Pi Edition

Downloading & Prerequisites
------------------------

1. Get git ``Sudo apt-get install git-core``

2. Make a directory somewhere for qurp
the directory will clone with a qurp forlder at the root, so maybe be careful not to end up with something like /home/user/projects/qurp/qurp/

3. Clone qurp	
``git clone https://github.com/welford/qurp.git``
OR
``git clone git://github.com/welford/qurp.git``

For some reason using the 2nd option meant that I couldn't commit changed back to the respository, 	i'm not yet sure of the difference, but probably bext to use the first one for now

4. @kramble added audio support via SDL, you'll need to use  ``Sudo apt-get install libsdl1.2-dev``

5. input is handled via libudev, you'll also need to use ``-sudo apt-get install libudev-dev``


Compiling & Building
------------------------

You will need to place a copy of the original games files under Winquake/id1. It doesn't *yet* work with the shareware version.

- Be wary that linux's file system is case sensitive, if you see a PAK0.pak or PAK1.pak within the Winquake/id1 folder rename them to pak0.pak and pak1.pak respectivly 

In my case I have qurp installed in /home/pi/programming/qurp

Navigate to [installed directory]/Winquake and run either
```bash
./build.sh debug_gl
```

or

```bash 
./build.sh release_gl 
```

Which will build the game into  [installed directory]/WinQuake/debugarm/bin or  [installed directory]/WinQuake/releasearm/bin respectively

run the game with the commands:
```bash
./releasearm/bin/glquake
./debugarm/bin/glquake
```

##PC Edition

This also compiles/builds/runs on a windows PC using VS2015. Windows support is mostly for ease of development purposes.   

Contributors
------------------------

https://github.com/welford/qurp/graphs/contributors

Issues
------------------------

- Input Issues

it seems as though input recognition can be tempermental at times (i've noticed this exclusively on my overclocked pi though)