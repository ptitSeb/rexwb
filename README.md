ReXWB

What is it?
-----------
This little tool allow to resample a xwb file.

xwb are WaveBank sound bank from XNA framework.
This tool allow to take individual file and change the sample rate.

It has been tested only with the wavebanks of Stardew Valley.


How to build
------------
You will need libsox to build.
It should work on any linux distrib, but you probably a fairly recent GCC version (I used 7.3).
Create a `build` sufolder and simple `cmake` and `make` should work...
so basicaly, from the freshly cloned folder:
`mkdir build`
`cd build`
`cmake ..`
`make`

How to launch
-------------

You need at least 3 argument: infile.wxb outfile.wxb rate
you can also use `-f` to force the MS ADPCM sound to be converted to PCM 16bits. PCM are 4 times bigger then ADPCM, but the loading time of thegame should be shorter, and conversion time too (but resulting wxb file will be bigger)
An example could be:
`./rewxb Content/XACT/Wave\ Bank.wxb new.wxb 11025 -f`
Another optionnal parameter is `-p`, in that case no verbose message is shown, only a percentage number (to be used with a zenity progress bar).

Note that input and output wxb file *MUST* be different.


Disclaimer
----------
This SOFTWARE PRODUCT is provided by THE PROVIDER "as is" and "with all faults." THE PROVIDER makes no representations or warranties of any kind concerning the safety, suitability, lack of viruses, inaccuracies, typographical errors, or other harmful components of this SOFTWARE PRODUCT. There are inherent dangers in the use of any software, and you are solely responsible for determining whether this SOFTWARE PRODUCT is compatible with your equipment and other software installed on your equipment. You are also solely responsible for the protection of your equipment and backup of your data, and THE PROVIDER will not be liable for any damages you may suffer in connection with using, modifying, or distributing this SOFTWARE PRODUCT.

(Of course, the code is hugly and poorly commented, but it worked on my usecase, so it may be usefull for other)
