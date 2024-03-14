## ZX Spectrum Bulk Data Transfer

A set of tools for transferring arbitrary files (of arbitrary length, up
to 1Gb) to ZX Spectrum machine via tape input (EAR). No additional
hardware required.

t2esx utility is intended to transfer data to the ZX Spectrum via the
EAR port without any additional hardware. If you have the means to play
back TAP files, t2esx allows you to transfer any arbitrary amount of raw
data (theoretical upper limit – 1G). Utility provided in two flavours –
“regular” 48k programme and the esxdos dot command. There are no other
restrictions, utility uses standard ROM routine for loading data blocks
from the audio input port, so it works on any Spectrum model. It was
originally created for the Sizif-512 board, which allows up to 4x speed
increase during tape loading. 640K (TRD disk image) can be transferred
in less than 15 minutes at 4x speed. Utility uses very basic data stream
integrity control, allowing to re-start data transfer of a failed chunk.

The “regular” build enables data transfer with zero setup. One simply
concatenates the t2esx TAP file with the data file or requests split.py
to prepend the TAP build and send it to Speccy. Of course, dot command
has to be copied to the target Speccy first.

Since version `2.0` experimental builds with fast loading are available.
These are prepared for the owners of the original 48 and 128 hardware,
where CPU overlocking is not possible. If you have a modern re-creation,
it is advised to increase the CPU clock frequency instead (e.g. Next
can load "tapes" at 8x speed when using 28Mhz). It should be easy to
spot fast loader enabled version, it shows `tM.N` instead of `vM.N`.

### Web Front End

A [web front end](https://tape2esxdos.sourceforge.io/t2esx/) is
available, enabling true "zero setup" data transfers. Options match
`split.py` command line parameters. If you do not trust our web service,
it should be possible to deploy all files locally even on the local
filesystem. A proper web server sending CORS headers is required to
enable WebWorker, though.

NOTE: you HAVE to copy compiled t2esx-zx0.tap into the `web`
directory to ensure that "Prepend T2ESX" options is working as intended
if you plan on using web frontend locally. It fails silently if
t2esx-zx0.tap is missing.

> Two (out of three) authors use multiple audio output devices, thus we
> have a rather complicated logic for setting up output channel.
> Most of the users will never experience any of these issues.

Web version has a weird edge case. Dealing with multiple audio devices
is quite problematic due to the browser's security policy. On Chrome
based browsers we have to request "microphone" permission. No audio is
recorded. Again, when in doubt just run everything locally.

### Preparing data file

Utility uses non-standard tape block identifiers, so data for transfer
must be prepared with the `split.py` script. split.py normally produces
a single TAP with 8K (when transferring less than 48k) or 16k data
chunks (see `--block_size` argument).  The maximum chunk size is 16K,
apparently bigger chunks reduce the time required to transmit data
headers and meta information.

`split.py` can insert placeholder blocks between data chunks, this is
required if Speccy’s storage device is unable to quickly save the chunk
(see `--pause` command line argument).

If your audio equipment is not reliable, “split” mode can be enabled
with the `--split` command line argument. Is this case split.py creates
a separate TAP file for each data chunk, simplifying retries. So far in
our limited testing there was no need to use it, you can simply loop the
data TAP file and eventually t2esx will load all the chunks.

Additional options generally required only if you want to use higher
than normal transfer speed (e.g. on Next running at 28MHz it is possible
to use 8x playback speed, but you may need --pause 2 or longer,
depending on your SD card write performance).

### split.py command line arguments

```
usage: split.py [-h] [-b BLOCK_SIZE] [-s] [-p PAUSE] [-d] [-t] [-u] files [files ...]

Prepares .xchtap file(s) for t2esx

positional arguments:
  files

options:
  -h, --help            show this help message and exit
  -b BLOCK_SIZE, --block-size BLOCK_SIZE
                        chunk size, optional, up to 16384
  -s, --split           store each chunk in a separate file
  -p PAUSE, --pause PAUSE
                        add pause (x 256b) between data chunks
  -d, --no-delay        skip delay before first data chunk, added by default
  -t, --turbo           produce TZX file at 2x speed, requires special t2esx build
  -u, --bundle          bundle t2esx.tap with the data
```

### t2esx output

t2esx utility displays the following status indicators (`n` means chunk
number):

- `?` – waiting for a header
- `O` - **o**pening file for writing
- `nL` – **l**oading data chunk number `n`
- `nS` – **s**aving data chunk number `n`
- `nE` – means there was an **e**rror while loading chunk `n`
- `!` – unexpected chunk number detected, code will keep loading headers
  looking for the right chunk

It is recommended to try split parameters on a short test file, two or
three chunks long.

Troubleshooting:

- slow disk subsystem
    - `O` still visible when the next HEADER block starts (note: since
      1.2 a short DATA block follows initial `OPEN_FILE` header). It
      means opening file takes too long. Let us know your
      hardware/software details (Spectrum model, Div\*\*\* model/make, SD
      card brand and capacity).  Workaround: just re-start upload,
      `t2esx` will ignore already processed parts.
    - `nL` still visible when the next block starts. Writing block takes
      too long, try reducing chunk size (unlikely to work) or
      increasing the pause duration
    - finally, it is possible that **all** disk operations may take very
      long time. Most likely the filesystem/card is too fragmented.
      Workaround: use `--split` option and manually start playback for
      each chunk.

“Tape” build has no input arguments and never overwrites target file if
it already exists. Dot command does not overwrite files by default, and
it accepts “`-f`” command line argument, which allows it to
unconditionally save target file, overwriting any existing data. Dot
command should be able to allocate buffer automatically. However, in
some configurations it may not be possible. If you get `M RAMTOP no good
(NNNNN)`, please do `CLEAR NNNNN` (recommended value for `NNNNN` is
`45055`). You may force dot command to use BASIC WORKSPACE only by
supplying "`-w`" command line argument. NOTE: unless "`-wl`" argument
used, code expects buffer to be in the uncontended memory, so WORKSPACE
should have at least 16384+80 bytes above 0x8000. If you are absolutely
sure the buffer can be placed in the lower/contended RAM, use "`-wl`"
flag. Buffer size can be adjusted with "`-bNNNN`" argument.  Buffer size
cannot be smaller than 1024 bytes and greater than 16384 bytes. Normally
you do not need to use `-w`/`-wl`/`-b` flags, unless you need to
preserve existing BASIC code and machine code or whatever you have above
RAMTOP since the region allocated from the WORKSPACE if guaranteed to
reside in the completely unused memory region.

> Unsupported/invalid flags are silently ignored, "`-h`" displays quick
> summary as shown below.

```
t2esx v2.x BulkTX (C)23,24 TIsland
 .t2esx [-bSIZE] [-f] [-w[l]] [-t[0-3]]
```

"Turbo" builds (the ones for the classic hardware) will detect
overlocked CPUs and report it as

```
W: flaky 2x     CPU @XMHz
```

NOTE: Next ZXOS esxdos API layer does not allow overwriting currently
running dot command (error #8). On the Next you cannot do `.cd /dot`
`.t2esx -f` to self-update, you have to load the new version somewhere
else and later move it under `/dot`.

Brief summary of all esxdos dot command arguments:
```
-h      - short usage summary

-f      - always overwrite target file (never is the default)
-w      - allocate buffer in WORKSPACE only, uncontended RAM only
-wl     - allocate buffer in WORKSPACE, allow contended RAM
-bSIZE  - buffer size (1024..16384)
          allocated buffer size reported in the second line
```
NextZXOS only:
```
-t[0-3] - set CPU frequency
          when not specified -- use whatever is configured,
          currently selected CPU frequency is reported in the
          third line
          '-t' only means "use 3.5MGHz"
          0/1/2/3 means "3.5/7/14/28 Mhz"s
```

### Build Variants

- `ALL` (**v**2.A) should be working on any hardware, not recommended,
  biggest code which has something completely unused on any platform.
Can load "turbo" TZX files (produced by the `split.py`), but those
should NOT be used with overclocked CPUs as they are less reliable
compared to the regular TAP played back at 2x/4x/8x speed.
- `CPU` (**V**2.A) is for modern re-creations with CPU overclocking,
  reports detected CPU speed to simplify speed choice for TAP/TZX
playback.
- `Next` (**n**2.A) is built specifically for the ZX Spectrum Next, it
  automatically increases CPU speed to the maximum enabling transfers at
8x speed. Dot command only, use `-tn` (`n` - 1, 2, 3) to choose 7, 14 or
28 MHz and `-t` or `-t0` to force 3MHz. Tape build does not try to
change CPU speed, only reports fast settings. Unlike `CPU` builds it
uses Next API to query clock speed.
- `48k` (**t**2.A) is able to load 2x/"turbo" TZX files produced by the
  `split.py`. This is for real 48/128/+2/+3 hardware.

### Data transfer tips

If you need to transfer more than one file, you can use regular `tar` to
bundle all of them together and, optionally, compress it with `zx7`
utility (make sure you have `dzx7` installed on your Speccy). Under
NextZXOS it is easier to use regular ZIP.

> Note: esxdos 0.89 does not support long file names, all files in
> archive **must** follow 8+3 naming convention or you will not be able
> to unpack them on Speccy. Do not get confused by the NextZXOS long
> file name support, NextZXOS offers esxdos-compatible API, but has no
> esxdos code.

### Example

![screencast](docs/screencast.gif)

Let's say we want to transfer the t2esx esxdos build to a Speccy:

1. On the host computer prepare source file `T2ESX`

```
$ ./split.py T2ESX
T2ESX size 3491 chunks: 1
Output: T2ESX.xchtap
Done with T2ESX
```

2. Combine the TAP build and the data file:

```
$ cat t2esx.tap T2ESX.xchtap > _.tap
```

> Since version 2.0 you can call split.py with `-u` flag, it will
> automatically prepend TAP build to the data file. TAP build should be
> in the current directory.

3. Transfer the TAP version and the data file to the Speccy:

```
$ tape2wav -r 44100 _.tap - | pacat -p --format=u8 --channels=1 --rate=44100
```

And do not forget to do `LOAD ""` on Speccy :)

> *You may want to use `--device` if you have more than one audio sink*

You will see something like
```
t2esx v2.x BulkTX (C)23,24 TIsland
'T2ESX' - 1 chunks
1
'T2ESX' DONE

[...]
9 STOP statement, 10:2
```

4. On the Speccy check newly uploaded `t2esx` dot command:

```
.ls t2esx
t2esx                   3491 DD.MM.YYYY
```

5. If you try uploading the same TAP again you will see

```
[...]
Can't open 'T2ESX' 18
[...]
```

Tape version does not overwrite the target file, you need to move/delete
it first. Dot command has `-f` command line argument enabling
unconditional overwrites. This is how you may want to replace t2esx in
the `/BIN` directory (assuming you you already have the previous version
there):

```
.cd /bin
.t2esx -f
```

And on your host just play `T2ESX.xchtap`

Dot command may display the following message:

```
t2esx v2.x BulkTX (C)23,24 TIsland
M RAMTOP no good (45055)
```

It means you need to move `RAMTOP` at least to the address suggested by
the `t2esx`, e.g. `CLEAR 45055`. Tape version sets `RAMTOP` in the BASIC
loader. Dot command tries to allocate required buffer automatically, but
sometimes it is not possible and manual RAMTOP adjustment (`CLEAR
45055`) may be required.
