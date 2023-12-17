## ZX Spectrum Bulk Data Transfer

A set of tools for transferring arbitrary files (of arbitrary lengh, up to 1Gb)
to ZX Spectrum machine via tape input (MIC). No additional hardware required.

t2esx utility is intended to transfer data to the ZX Spectrum via the
MIC port without any additional hardware. If you have the means to play
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
concatenates the t2esx TAP file with the data file and send it to
Speccy. Of course, dot command has to be copied to the target Speccy
first.

Utility uses non-standard tape block identifiers, so data for transfer
must be prepared with the `split.py` script. split.py normally produces a
single TAP with 8K data chunks (see `--block_size` argument). The maximum
chunk size is 16K, apparently smaller number of chunks reduces the time
required to transmit data headers and meta information. `split.py` can
insert placeholder blocks between data chunks, this is required if
Speccy’s storage device is unable to quickly save the chunk (see `--pause`
command line argument). If your audio equipment is not reliable, “split”
mode can be enabled with the `--split` command line argument. Is this case
split.py creates a separate TAP file for each data chunk, simplifying
retries. So far in our limited testing there was no need to use it, you
can simply loop the data TAP file and eventually t2esx will load all the
chunks.

t2esx utility display the following status indicators (`N` means chunk
number):

- `?` – waiting for a header
- `NL` – loading data chunk number N
- `NS` – saving data chunk number N
- `NE` – appears briefly, before `?` indicator is shown, means there was
  an error while loading chunk N
- `!` – unexpected chunk number detected, code will keep loading headers
  looking for the right chunk

“regular” build has no input arguments and never overwrites target file
if it already exists. Dot command does not overwrite files by default
and it accepts “`-f`” command line argument, which allows it to
unconditionally save target file, overwriting any existing data.  RAMTOP
has to be manually adjusted before launching the dot command (using
regular `CLEAR NNNNN`), maximum suggested value reported by the utility
(`45055` at the moment).
