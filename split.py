#!/usr/bin/env python3

# Copyright 2023 TIsland Crew
# SPDX-License-Identifier: Apache-2.0

import argparse
from os import get_terminal_size
from os.path import getsize, exists
from pathlib import Path
from struct import pack, unpack
from sys import argv

MAX_BLOCK_SIZE=16384

BTX_OPEN_ID=0x87
BTX_CHUNK_ID=0x88

turbo = False # FIXME:

#read file in BLOCK_SIZE chunks, write to TAP with
#  ID 0x88, vars -- total number of chunks, hdadd -- current chunk number
#  chunk with ID 0x87 means 'open file but ignore the data', required for
#  slow esxdos where opening file takes long time

def split(name, delay=False, block_size=(MAX_BLOCK_SIZE/2), pause=0, split=False):
  if not exists(name):
    print("Not a file, skipping:", name)
    return
  filesize=getsize(name)
  if None == block_size:
    block_size = MAX_BLOCK_SIZE//2 if filesize < 49152 else MAX_BLOCK_SIZE
  nchunks=filesize//block_size
  if (filesize%block_size) > 0: nchunks += 1
  dosname = mkdosname(name)
  print('{} size {}, {} chunk{} "{}"'
      .format(name, filesize, nchunks, 's' if nchunks > 1 else '', dosname))
  if turbo: ftyp = '.xchtzx'
  else: ftyp = '.xchtap'
  with open(name, 'rb') as f:
    ordinal = 1
    print("Output:", name+ftyp)
    if split:
      while True:
        data = f.read(block_size)
        if data:
          oname = name+ftyp+'.'+str(ordinal).zfill(4)
          with open(oname, 'wb') as tap:
            packchunk(tap, dosname, ordinal, nchunks, data, delay)
            ordinal += 1
        else:
          break
    else:
      with open(name+ftyp, 'wb') as tap:
        if turbo: tzx_start(tap)
        data = f.read(block_size)
        while data:
          packchunk(tap, dosname, ordinal, nchunks, data, delay)
          if pause > 0 and ordinal < nchunks:
            tape_data(tap, bytearray([0x55] * pause * 256))
          ordinal += 1
          data = f.read(block_size)
    print('Done with {}'.format(name).ljust(get_tty_width()))

def get_tty_width():
  columns = get_terminal_size().columns
  if None == columns or columns < 32:
    columns = 32
  return columns

def mkdosname(fname):
  # dos name is 8+3, but we only have 10 chars
  p = Path(fname)
  suffix = p.suffix[0:4]
  stem = p.stem.translate({ord('.'):None})
  if len(suffix) > 0:
    return stem[0:10-len(suffix)]+suffix
  else:
    return stem

def chksum(*args):
  chksum = 0
  for data in args:
    for b in data: chksum ^= b
  return chksum

def packchunk(t, dosname, ordinal, total, data, delay=False):
    name = dosname.ljust(10).encode('ASCII')
    # do we need OPEN_FILE chunk?
    if 1 == ordinal and delay:
      tape_header(t, BTX_OPEN_ID, name, len(data), ordinal, total)
      tape_data(t, bytearray([0x77]))
    # header
    tape_header(t, BTX_CHUNK_ID, name, len(data), ordinal, total)
    # data
    tape_data(t, data)
    print('Written chunk {} size {}\r'.format(ordinal, len(data))[:get_tty_width()-1], end='')

def tzx_start(t):
  tzx = bytearray(11)
  tzx[0:8] = pack('<7s', 'ZXTape!'.encode('ASCII'))
  tzx[7] = 0x1a
  tzx[8] = 0x01
  tzx[9] = 0x14
  t.write(tzx)

def tzx_turbo_speed_header_mcleod(t, size): # mcload timings
  bh = bytearray(18)
  bh[0:2] = pack('<H', 1500) # PILOT
  bh[2:4] = pack('<H', 400) # SYNC 1st
  bh[4:6] = pack('<H', 800) # SYNC 2nd
  bh[6:8] = pack('<H', 500) # ZERO
  bh[8:10] = pack('<H', 1000) # ONE
  bh[10:12] = pack('<H', 600) # PILOT len
  bh[12:13] = pack('<B', 8) # last byte used bits
  bh[13:15] = pack('<H', 1000 ) # pause after
  bh[15:17] = pack('<H', size) # data length
  bh[17] = 0 # our length fits 2 bytes
  t.write(b'\x11')
  t.write(bh)


def tzx_turbo_speed_header(t, size):
  bh = bytearray(18)
  bh[0:2] = pack('<H', 0x580) # PILOT
  bh[2:4] = pack('<H', 0x18D) # SYNC 1st
  bh[4:6] = pack('<H', 0x13d) # SYNC 2nd
  bh[6:8] = pack('<H', 0x145) # ZERO
  bh[8:10] = pack('<H', 0x289) # ONE
  bh[10:12] = pack('<H', 0x12e3) # PILOT len
  bh[12:13] = pack('<B', 8) # last byte used bits
  bh[13:15] = pack('<H', 0x13e ) # pause after
  bh[15:17] = pack('<H', size) # data length
  bh[17] = 0 # our length fits 2 bytes
  t.write(b'\x11')
  t.write(bh)

def tape_header(t, type_id, name, size, ordinal, total):
  hdr = bytearray(19)
  # hdr[0] = 0, flag byte 00 (header)
  hdr[1] = type_id
  hdr[2:12] = pack('<10s', name)
  hdr[12:14] = pack('<H', size)
  hdr[14:16] = pack('<H', ordinal) # param1 - this block ordinal
  hdr[16:18] = pack('<H', total) # param2 - total # of blocks
  hdr[18] = chksum(hdr) # note that [18] must be 0!!!
  if turbo:
    tzx_turbo_speed_header(t, len(hdr))
  else:
    t.write(pack('<H', len(hdr)))
  t.write(hdr)

def tape_data(t, data):
  if turbo:
    tzx_turbo_speed_header(t, len(data)+2)
  else:
    t.write(pack('<H', len(data)+2))
  t.write(b'\xff')
  t.write(data)
  t.write(pack('<B', chksum([0xff], data)))

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
  parser.add_argument('-t', '--turbo', action='store_true', default=False)
  args = parser.parse_args()

  if None != args.block_size and args.block_size > MAX_BLOCK_SIZE:
    raise TypeError('Block size cannot be larger than {}'.format(MAX_BLOCK_SIZE))
  if args.split and args.pause > 0:
    print("WARNING: no pause added when splitting output")
  turbo = args.turbo
  print(args)

  for name in args.files:
    split(name, not args.no_delay, args.block_size, args.pause, args.split)

# EOF vim: ts=2:sw=2:et:ai:
