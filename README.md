=======================================
QURP:
=======================================
A barebones port of quake one for the Raspberry Pi

This port is from the original source found here : https://github.com/id-Software/Quake 
All the original lgal stuff can be found in the original readme files.

Downloading
------------------------

1.get git
Sudo apt-get install git-core
2. make a directory somewhere for qurp
the directory will clone with a qurp forlder at the root, so maybe be careful not to end up with something like /home/user/projects/qurp/qurp/
3. clone qurp	
git clone https://github.com/welford/qurp.git
OR
git clone git://github.com/welford/qurp.git

For some reason using the 2nd option meant that i couldn't commit changed back to the respository, 	i'm not yet sure of the differece, but probably bext to use the first one for now

Building
------------------------

-You will to place a copy of the original games files under Winquake/id1

In my case I have qurp installed in /home/pi/programming/qurp

Navigate to [installed directory]/Winquake and run either
	./Build.sh build_debug 
	./Build.sh build_release

Issues
------------------------

- The release version crashes on starting the game from the main menu, it will run the demos though
- no mouse support & poor keyboard support
  might i sbe better to use SDL? can it be included as an internal library?
- Performance Issues
  The graphics commands probably need a big overhaul, currently the wrapper created one big VBO 
  which is filled until a state change is made that would require a flush of the previously buffered commands
  It's almost definitely better to create VBOs for individual data where possible and upload the vertices only once.
- Lots of others
