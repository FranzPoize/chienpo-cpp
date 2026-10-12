I want you to convert /home/franz/project/real-chienpo/ to cpp. I want you to stick to the architecture as much as possible apart from the client server approach.

We want to do this to remove the client - server architecture that we use.

For the build system look into /home/franz/gamedev/snacman . The project use conan and cmake to build the project.
We want to use the graphics library that snacman use and also the sound library. In the base real-chienpo project we use ImGui for the UI, we want to use it in the cpp version.

The data for the game is stored in a array of big struct we want to keep it that way. Most of the data structure should be structs, avoid class and inheritance if possible (The game class should be kept).
For the json parsing and dump use a c library that is as simple as possible you can look for one yourself.
Use the sounds library that is used in snacman if some feature are missing for our usage make a note of it in the code by comment.
We want to target older ubuntu version that may not have our version of libc so take that into consideration and write some documentation on how to build for those targets.
