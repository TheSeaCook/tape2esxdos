#!/usr/bin/env python3

# Copyright 2023,24 TIsland Crew
# SPDX-License-Identifier: Apache-2.0

import argparse
from abc import ABC, abstractmethod
from os import get_terminal_size
from os.path import getsize, exists
from pathlib import Path
from struct import pack, unpack
from sys import argv

MAX_BLOCK_SIZE=16384

BTX_OPEN_ID=0x87
BTX_CHUNK_ID=0x88

#read file in BLOCK_SIZE chunks, write to TAP with
#  ID 0x88, vars -- total number of chunks, hdadd -- current chunk number
#  chunk with ID 0x87 means 'open file but ignore the data', required for
#  slow esxdos where opening file takes long time

class ChunkWriter(ABC):
  def __init__(self, name, count, pause=False, delay=False, loader=False):
    self._name = name
    self._count = count
    self._pause = pause
    self._delay = delay
    self._loader = loader
    # internal state
    self._tapname = mkdosname(name).ljust(10).encode('ASCII')
    self._ordinal = 1
    self._suffix = '.xchtap'

  def filename(self):
    return self._name + self._suffix

  def __enter__(self):
    return self.open()

  def __exit__(self, *args):
    self.close()

  @abstractmethod
  def open(self):
    return self

  @abstractmethod
  def close(self):
    pass

  def write(self, chunk):
    if 1 == self._ordinal:
      self._write_tape_header()
    self._write_tape_block(BTX_CHUNK_ID, chunk)
    if self._pause > 0 and self._ordinal < self._count:
      self._write_tape_block_data(bytearray([0x55] * self._pause * 256))
    self._ordinal += 1

  def _write_tape_header(self):
    if self._loader:
      self._write_loader("t2esx-zx0.tap", "t2esx.tap")
    if self._delay:
      self._write_tape_block(BTX_OPEN_ID, bytearray([0x77]))

  def _write_tape_block(self, type_id, chunk):
    self._write_tape_block_header(type_id, len(chunk))
    self._write_tape_block_data(type_id, chunk)

  @abstractmethod
  def _write_loader(*args):
    pass

  @abstractmethod
  def _write_tape_block_header(self, type_id, size):
    pass

  @abstractmethod
  def _write_tape_block_data(self, type_id, data):
    pass


class TapWriter(ChunkWriter):

  def open(self):
    self._handle = open(self._name + self._suffix, 'wb')
    return self

  def close(self):
    self._handle.close()

  def _write_tap_block_prologue(self, block_size):
    self._handle.write(pack('<H', block_size))

  def _write_tape_block_header(self, type_id, size):
    hdr = bytearray(19)
    hdr[0] = 0 # flag byte 00 (header)
    hdr[1] = type_id
    hdr[2:12] = pack('<10s', self._tapname)
    hdr[12:14] = pack('<H', size)
    hdr[14:16] = pack('<H', self._ordinal) # param1 - this block ordinal
    hdr[16:18] = pack('<H', self._count) # param2 - total # of blocks
    hdr[18] = 0 # note that [18] must be 0!!! or checksum will be wrong
    hdr[18] = chksum(hdr) # note that [18] must be 0!!!
    self._write_tap_block_prologue(len(hdr))
    self._handle.write(hdr)

  def _write_tape_block_data(self, type_id, data):
    self._write_tap_block_prologue(len(data)+2)
    self._handle.write(b'\xff')
    self._handle.write(data)
    self._handle.write(pack('<B', chksum([0xff], data)))

  def _append_tap(self, loader):
    with open(loader, 'rb') as l:
      self._handle.write(l.read()) # FIXME: fixed length buffer?

  def _write_loader(self, *loaders):
    for loader in loaders:
      if exists(loader) and getsize(loader)>0:
        print("Adding", loader, "to the bundle")
        self._append_tap(loader)
        return
    print("ERROR: None of the loaders found", loaders)


class SplittingTapWriter(TapWriter):
  def open(self):
    return self

  def close(self):
    pass # no op

  def write(self, chunk):
    oname = self._name + self._suffix + str(self._ordinal).zfill(4)
    with open(oname, 'wb') as tap:
      self._handle = tap
      super().write(chunk)
      self._handle = None


class TzxWriter(TapWriter):
  def __init__(self, *args):
    super().__init__(*args)
    self._suffix = '.xchtzx'

  def _append_tap(self, loader):
    tap2tzx(loader, self._handle)

  def _write_tape_header(self):
    tzx = bytearray(11)
    tzx[0:8] = pack('<7s', 'ZXTape!'.encode('ASCII'))
    tzx[7] = 0x1a
    tzx[8] = 0x01
    tzx[9] = 0x14
    self._handle.write(tzx)
    super()._write_tape_header()

  def _write_tap_block_prologue(self, size):
    bh = bytearray(18)
    bh[0:2] = pack('<H', 1408) # PILOT
    bh[2:4] = pack('<H', 397) # SYNC 1st
    bh[4:6] = pack('<H', 317) # SYNC 2nd
    bh[6:8] = pack('<H', 325) # ZERO
    bh[8:10] = pack('<H', 649) # ONE
    bh[10:12] = pack('<H', 4835) # PILOT len
    bh[12:13] = pack('<B', 8) # last byte used bits
    bh[13:15] = pack('<H', 318) # pause after
    bh[15:17] = pack('<H', size) # data length
    bh[17] = 0 # our length fits 2 bytes
    self._handle.write(b'\x11')
    self._handle.write(bh)


def split(name, args):
  if not exists(name):
    print("Not a file, skipping:", name)
    return
  filesize=getsize(name)
  if None == args.block_size:
    block_size = MAX_BLOCK_SIZE//2 if filesize < 49152 else MAX_BLOCK_SIZE
  else:
    block_size = args.block_size
  nchunks=filesize//block_size
  if (filesize%block_size) > 0: nchunks += 1
  dosname = mkdosname(name)
  print('{} size {}, {} chunk{} "{}"'
      .format(name, filesize, nchunks, 's' if nchunks > 1 else '', dosname))

  if args.turbo:
    writer = TzxWriter
  elif args.split:
    writer = SplittingTapWriter
  else:
    writer = TapWriter

  with open(name, 'rb') as f:
    with writer(name, nchunks, args.pause, not args.no_delay, args.bundle) as tape:
      print("Output:", tape.filename());
      data = f.read(block_size)
      while data:
        tape.write(data)
        print('.', end='', flush=True)
        data = f.read(block_size)

  print('\rDone with {}'.format(name).ljust(get_tty_width()))

def get_tty_width():
  columns = get_terminal_size().columns
  if None == columns or columns < 32:
    columns = 32
  return columns

def mkdosname(fname):
  # dos name is 8+3, but we only have 10 chars
  p = Path(fname)
  suffix = p.suffix[0:4]
  stem = p.stem.translate({ord('.'):None, ord(' '):None})
  if len(suffix) > 0:
    return stem[0:10-len(suffix)]+suffix
  else:
    return stem

def chksum(*args):
  chksum = 0
  for data in args:
    for b in data: chksum ^= b
  return chksum

def tap2tzx(name, write_handle): # convert TAP into TZX
  # NOTE: no TZX header added, only blocks are converted!
  with open(name, 'rb') as tap:
    while True:
      lenbts = tap.read(2)
      if not lenbts: break
      size = unpack('<H', lenbts)[0]
      block = tap.read(size)
      if not block or len(block) != size:
        raise Error('Cannot read {} bytes from {}'.format(size, name))
      write_handle.write(b'\x10') # ID 10 - Standard Speed Data Block
      write_handle.write(pack('<H', 1000)) # Pause after this block (ms.)
      write_handle.write(pack('<H', size)) # Length of data that follow
      write_handle.write(block) # Data as in .TAP files

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
      description='Prepares .xchtap file(s) for t2esx'
  )
  parser.add_argument('files', nargs='+')
  parser.add_argument('-b', '--block-size', type=int, required=False,
      help='chunk size, optional, up to {}'.format(MAX_BLOCK_SIZE))
  parser.add_argument('-s', '--split', action='store_true',
      help='store each chunk in a separate file')
  parser.add_argument('-p', '--pause', type=int, default=0,
      help='add pause (x 256b) between data chunks')
  parser.add_argument('-d', '--no-delay', action='store_true', default=False,
      help='skip delay before first data chunk, added by default')
  parser.add_argument('-t', '--turbo', action='store_true', default=False,
      help='produce TZX file at 2x speed, requires special t2esx build')
  parser.add_argument('-u', '--bundle', action='store_true', default=False,
      help='bundle t2esx.tap with the data')
  args = parser.parse_args()

  if None != args.block_size and args.block_size > MAX_BLOCK_SIZE:
    raise TypeError('Block size cannot be larger than {}'.format(MAX_BLOCK_SIZE))
  if args.split and args.pause > 0:
    print("WARNING: no pause added when splitting output")
    args.pause = 0
  if args.split and args.turbo:
    print("WARNING: no turbo when splitting output")
    args.turbo = False

  for name in args.files:
    split(name, args)

# EOF vim: ts=2:sw=2:et:ai:
