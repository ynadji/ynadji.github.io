+++
title = "Running EmoTracker in Linux with Wine"
tags = [
    "smz3",
    "randomizers",
    "snes",
]
categories = [
    "open-source",
    "gaming",
]
date = "2022-01-26"
draft = false
+++

[EmoTracker](https://emotracker.net/) is a tracking tool for
randomized versions of emulated games. It's written using
[WPF](https://docs.microsoft.com/en-us/dotnet/desktop/wpf/overview/),
which doesn't appear to work with old versions of
[Wine](https://www.winehq.org/). While Linux isn't supported by the
devs, it seems to work fine enough for me with newer versions of Wine
and some tweaks. I didn't see instructions on the Discord, so I posted
these instructions [as a
gist](https://gist.github.com/ynadji/f0fe86e76f03238eaa2f925f105c4aa8)
to help other folks out. It doesn't seem Google indexes gists well, so
I went ahead and moved it here.

```
~/tmp/roms sif*
¡ neofetch
            .-/+oossssoo+/-.               yacin@sif 
        `:+ssssssssssssssssss+:`           --------- 
      -+ssssssssssssssssssyyssss+-         OS: Ubuntu 20.04.3 LTS x86_64 
    .ossssssssssssssssssdMMMNysssso.       Kernel: 5.4.0-92-generic 
   /ssssssssssshdmmNNmmyNMMMMhssssss/      Uptime: 15 days, 8 hours, 41 mins 
  +ssssssssshmydMMMMMMMNddddyssssssss+     Packages: 4145 (dpkg), 10 (snap) 
 /sssssssshNMMMyhhyyyyhmNMMMNhssssssss/    Shell: zsh 5.8 
.ssssssssdMMMNhsssssssssshNMMMdssssssss.   Resolution: 1920x1080, 2560x1440 
+sssshhhyNMMNyssssssssssssyNMMMysssssss+   DE: Xfce 
ossyNMMMNyMMhsssssssssssssshmmmhssssssso   WM: Xfwm4 
ossyNMMMNyMMhsssssssssssssshmmmhssssssso   WM Theme: Greybird 
+sssshhhyNMMNyssssssssssssyNMMMysssssss+   Theme: Greybird [GTK2/3] 
.ssssssssdMMMNhsssssssssshNMMMdssssssss.   Icons: elementary-xfce-darker [GTK2/3] 
 /sssssssshNMMMyhhyyyyhdNMMMNhssssssss/    Terminal: urxvt 
  +sssssssssdmydMMMMMMMMddddyssssssss+     Terminal Font: inconsolata 
   /ssssssssssshdmNNNNmyNMMMMhssssss/      CPU: AMD Ryzen 9 3900X (24) @ 3.800GHz 
    .ossssssssssssssssssdMMMNysssso.       GPU: NVIDIA GeForce GTX 1660 SUPER 
      -+sssssssssssssssssyyyssss+-         Memory: 78385MiB / 128803MiB 
        `:+ssssssssssssssssss+:`
            .-/+oossssoo+/-.                                       
                                                                   

~/tmp/roms sif*
¡ wine --version       
wine-7.0
```

# Instructions

I've only tested with Wine Stable 7.0. At least Wine 5.18 is needed
because the required `CPAcquireContext` [was implemented
then](https://github.com/wine-mirror/wine/blob/e909986e6ea5ecd49b2b847f321ad89b2ae4f6f1/ANNOUNCE#L187),
so it's possible earlier versions work. Install 7.0 from [this Ubuntu
guide](https://ubuntuhandbook.org/index.php/2022/01/wine-stable-7-0-released/). Try
to find similar ones for your distro.

Install winetricks:

```
$ sudo apt install winetricks
```

Next, you need to prep Wine to run WPF properly. This is based on
[this reddit
post](https://www.reddit.com/r/linux4noobs/comments/firqs9/getting_windows_wpf_applications_to_run_with_wine/)
I found.  Unnecessary commands were removed. It's important to note
that this may mess up games that you play directly with Wine, as it
disables hardware acceleration.

```
$ winetricks -q dxvk155
$ winetricks -q d3dcompiler_47
$ wine reg add "HKCU\\SOFTWARE\\Microsoft\\Avalon.Graphics" /v DisableHWAcceleration /t REG_DWORD /d 1 /f
```

After this, [download](https://emotracker.net/download/), install, and
run EmoTracker.

```
$ wine ~/Downloads/emotracker_setup.exe
$ wine '.wine/drive_c/Program Files (x86)/EmoTracker/EmoTracker.exe'
```

If it starts and looks like the image below, you're good to go! 

{{< image src="../../../../images/blog/emotracker-1.png" alt="EmoTracker in Wine (if set up correctly). Note the messed up (but functional) toolbar icons." style="border-radius: 8px;" >}}

You'll notice the title bar icons are just squares next to the
minimize/maximize/close buttons in the upper-right (see the first
image). The tooltips show up so you can install packages from there. I
suspect that the "I am seeing weird squares in the title bar..." entry
from the FAQ on Discord likely is the culprit, so we need to install
the font and get Wine to recognize it. According to
[this](https://askubuntu.com/questions/86335/installing-other-fonts-on-wine),
I should be able to install the required font and Wine should notice
it with:

```
$ mkdir -p ~/.fonts
$ curl https://cdn.discordapp.com/attachments/386970120616148994/406207753531686933/seguisym.ttf -o ~/.fonts/seguisym.ttf
$ sudo fc-cache -fv
```

However, it doesn't appear to work for me. I also tried saving the TTF file to:

- `~/.wine/drive_c/Windows/fonts`
- `c:\windows\fonts` i.e. `~/.wine/drive_c/windows/Fonts`

These didn't work either.

So far I've only tested this with the SMZ3 combo randomizer, but it
seems to work fine:

{{< image src="../../../../images/blog/emotracker-2.png" alt="EmoTracker in Wine with the installed SMZ3 pack." style="border-radius: 8px;" >}}

# Closing Notes

I've never used EmoTracker on Windows so I'm not sure the expected
behavior. But here's a list of things I know are broken:

- Settings/Package Manager/Reset Tracker buttons don't load.

Let me know if you can get the fonts working or run into any trouble
getting EmoTracker up and running with Wine.
