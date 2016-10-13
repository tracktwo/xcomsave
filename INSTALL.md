This guide assumes that your Operating System is Linux (Should be the same on *BSD). It may work on Windows but it wasn't tested. 

= Downloading and bulding xcomsave
1. Clone xcomsave: 'git clone https://github.com/tracktwo/xcomsave.git; cd xcomsave'
2. Clone json11: 'rm -rf json11;git clone https://github.com/dropbox/json11.git'
3. Build xcomsave: 'mkdir build; cd build; cmake ../.;' 
4. Compile: 'make; cd ..'
5. Copy json2xcom and xcom2json to a new directory. Those are the only files you need.
