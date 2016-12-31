Wii U Menu Sort
ver 0.1.0

WARNING:  TRY ON REDNAND FIRST.
I am not responsible for bricked consoles.

That said, I tried 10 sorts on 2 accounts on USA sysNAND without issue.

This will allow you to alphabetically sort your icons on the Wii U Menu.
Icons in folders are also sorted.
Sorts are done per user account.
Not much is shown for a UI.  It will just start sorting and tell you when it is done in 5-10s.

The following items will not move:
Folders and system icons (Disc, Settings, etc)
Homebrew Launcher
CHBC (untested)
Any IDs specified in dontmove.hex

dontmove.hex must be edited with a hex editor.  Do Not use notepad, etc.
Only use the last 4 bytes of the title ID.
The included sample file has:
10 17 9B 00  Brain Age: Train Your Brain In Minutes A Day
10 10 5A 00  Netflix
10 10 57 00  YouTube
4bytes * 3 titles = 12byte file.
The file can be as large or small as you want, depending on the IDs you don't want to move.
It must be a multiple of 4 bytes.

You can also use dontmoveX.hex where X is 0-9, or A or B.
This allows each user to have separate selection of non-movable icons.
This is the last digit of the 8000000X save folder used by the current user.
Shown by the program as User ID: X

dontmoveX.hex takes priority over dontmove.hex.
Only 1 file is used.  The IDs are not merged.
You can delete the files if not needed.

It just does a basic string compare.  Not sure how well it will work in foreign languages.
Will have to find a small UTF-8 normalize library.

You will need to compile the Wii U version of libfat.
https://github.com/dimok789/libfat
make from the wiiu\ directory.
Copy the headers and complied library to portlibs\ppc\...
