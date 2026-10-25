UsbNet360 @VERSION@
Use a USB Ethernet adapter as the network connection on a homebrew Xbox 360.
https://github.com/marcusisdahl/UsbNet360

UsbNet360.xex is a DashLaunch plugin, not an app: starting it from Aurora does
nothing. It runs inside the Xbox kernel and works only on dashboard/kernel
17559 with a retail kernel. A driver bug can freeze the console, so keep a copy
of your launch.ini to switch back.

To turn it on:

1. Open launch.ini (DashLaunch's settings file, on the root of your USB drive
   or Hdd:) and make UsbNet360 the FIRST plugin. Move your other plugins down
   one number. DashLaunch loads at most 5 plugins.

     [Plugins]
     plugin1 = Hdd:\Apps\UsbNet360\UsbNet360.xex
     plugin2 = Hdd:\Plugins\xbdm.xex

   Use the folder the store put this file in. Hdd:\Apps\UsbNet360\ is the
   usual one.
2. Plug a supported adapter into a USB port on the Xbox, connect it to your
   router or switch with a network cable, and restart the console.
3. Aurora, FTP servers and your other apps now use the adapter. The IP address
   comes from your router as usual.

Supported chipsets: ASIX AX88179 (TP-Link UE306), AX88179A/B, AX88772D,
AX88279, AX88178A, AX88178, and Realtek RTL8153/RTL8153B (TP-Link UE300).

To remove it, take its line out of launch.ini and restart. It doesn't touch
the NAND.

The faster, experimental UsbNet360-acksmall.xex is on the GitHub page above.

UsbNet360 is free software under the GNU General Public License, version 2
only (LICENSE.txt). It is a fork of CUsbEnet by proferabg. The source code of
this build is at https://github.com/marcusisdahl/UsbNet360/tree/@VERSION@
It isn't affiliated with Microsoft, TP-Link, ASIX Electronics or Realtek.
