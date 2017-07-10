
This is a continuation of the Audio-Router program found at http://github.com/audiorouterdev/audio-router . Audio-Router is a Windows program that allows routing the audio output from various programs to multiple device outputs, similar to CheVolume.


I apologise if you encounter any bugs while using this program. Any that existed in audiorouterdev's work may still be in here, as I haven't got around to cleaning up much. There's alot of commented out code, and the syntax is pretty dirty as the code currently stands (I intend to fix this eventually with Uncrustify in the next few days). I'm also still fairly amateur at c++ and will need to study this code a bit before I can fix any existing bugs. That being said, do not hesitate to leave any bugs or suggestions in the issues. I will not be able to find all the bugs by myself so any feedback helps. 

I do not yet have any changelog at the moment, I plan to add one, but i will do that after a few days of tweaking and cleaning up the code. If you want to see a semi-alternative changelog, just check the commit history inside the develop branch. You can also download a build of the develop branch using appveyor below

(Appveyor still being configured, please check back here later for a link)

For now, the main visible difference between this fork and the original is a system tray icon included.

The branches are:

develop: this is the branch that all commits are distributed on.
open: this is the default branch of audiorouterdev's repo, all code in this branch was prior to me taking over, and will remain there for a nice walk down memory lane or something.
Any other branches are personal branches.
Contributing

I am open to any contributions. If you have any questions, feel free to leave an issue, comment, whatever you want, and I will do my best to get back to you as soon as I can.

To contribute code, you will need Visual Studio 2017 (Community edition is okay, it's what I use), and the c++ runtimes (I am unsure what this project specifically requires still, as I've not got there yet).

Fork the project and then branch off of develop. Add any codes, bug fixes, or features you want in that new branch. Then, just post a pull request and wait for me or someone else to check it out. You pull request it, I merge it, hoo ha the whole world works.

Right now, pull requests are set to reviews required, but since this revival project was just created recently, this won't always be necessary. If the code makes sense to me and works, I'll probably just merge it no questions asked (unless you have any questions or want a review, in such case just leave a note in the message). I only have it set to this because, in the event this project takes off anywhere, I wish to encourage engagement and discussion as much as possible.

(Please note, this is a rough draft of the readme and is subjected to change later on. As it stands, I'm still in the process of setting this all up by myself, so I have alot on my plate, so this readme is not one of my highest priorities at the second. But If you are not satisfied with this readme, let me know. If the program works great for you but this readme makes you wish i was dead, let me know.)

(The previous readme from audiorouterdev can be found in `README_old.md`)
