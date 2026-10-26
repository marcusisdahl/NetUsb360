# Release test checklist

Run this on a console before publishing a release, with each XEX in the release (`NetUsb360.xex` and `NetUsb360-acksmall.xex`). It takes about an hour per file. Write down the adapter, the XEX, the result and anything odd in the release notes or the pull request.

Set `StatusFile = 1` in `NetUsb360.ini` for the whole run, and keep xbWatson open if you can. Save `NetUsb360-status.txt` (or the log) after each long transfer: those numbers are the baseline the next release is compared with.

## Setup

- Kernel 17559 retail, NetUsb360 as the first plugin, the adapter in a rear USB port.
- A wired PC on the same switch, FileZilla and FastFTP's network test.
- Write down the start-up lines: version and build, kernel, settings, adapter and USB ID.

## Start-up

1. Cold boot with the adapter plugged in. The on-screen message names the adapter and link speed within about 20 seconds.
2. Cold boot with no adapter. After about 45 seconds the message says no supported adapter was found, and the console otherwise works normally.
3. Cold boot with the cable unplugged. The message says the adapter has no network link.
4. Boot with a `NetUsb360.ini` containing a typo (`Acksmal = 1`) and a bad value (`Notify = maybe`). The log names both lines, and the driver starts with its defaults.

## Transfers

Use FastFTP's network test with the **All data** check for 1 and 2, and FileZilla for the rest.

1. PC to Xbox, 10 GB or more, unlimited. Every byte checks. Note the speed.
2. Xbox to PC, 10 GB or more, unlimited. Every byte checks, no freeze. Note the speed. This is the direction where the acksmall build froze once.
3. Both directions at once for 10 minutes.
4. A folder of 5,000 small files (1 to 100 KiB) to the Xbox, then back.
5. Aurora's built-in FTP, one 2 GB file each way.

## Plug and cable

1. Pull the network cable during a transfer, wait 10 seconds, plug it back in. The link comes back and a new transfer works.
2. Unplug the adapter while idle, wait, plug it back in. The network comes back without a restart.
3. Unplug the adapter during a transfer. The console doesn't freeze. Plug it back in and start a new transfer.
4. Restart the console from the dashboard and with the power button, five times each. No freeze on the way down.

## Titles

1. Start and quit an Xbox 360 game twice, then transfer a file.
2. Start an original Xbox game (for example with NetDisc360) and use the network from it.
3. Go back to the dashboard and transfer a file.

## Settings

With `NetUsb360.xex`, set `Acksmall = 1` and repeat Transfers 1 and 2. With `NetUsb360-acksmall.xex`, set `Acksmall = 0` and repeat Transfer 1. The status file shows the settings in effect each time.

## Other chipsets

The 1.1 receive tuning (24 KiB transfers and batched delivery) was measured on the original AX88179 only. When you test an AX88179A/B, AX88772D, AX88279 or RTL8153 adapter, run Transfers 1 and 2 with the status file on and record the speeds here, so later tuning for those chips has a baseline:

| Adapter | Chipset | USB ID | XEX | PC to Xbox | Xbox to PC | Notes |
|---|---|---|---|---|---|---|
| TP-Link UE306 | AX88179 | `0B95:1790` rev 1 | acksmall 1.1 | 27.66 MiB/s | | Baseline from the README |
