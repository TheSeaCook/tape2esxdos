#!/usr/bin/env python3

import argparse
from os.path import getsize, exists
from pathlib import Path
import re
from struct import pack, unpack
from sys import argv

MAX_BLOCK_SIZE=16384

BTX_OPEN_ID=0x87
BTX_CHUNK_ID=0x88

#read file in BLOCK_SIZE chunks, write to TAP with
#  ID 0x88, vars -- total number of chunks, hdadd -- current chunk number

def split(name, delay=False, block_size=(MAX_BLOCK_SIZE/2), pause=0, split=False):
  if not exists(name):
    print("Not a file, skipping:", name)
    return
  filesize=getsize(name)
  nchunks=filesize//block_size
  if (filesize%block_size) > 0: nchunks += 1
  print(name, "size", filesize, "chunks:", nchunks)
  with open(name, 'rb') as f:
    ordinal = 1
    dosname = mkdosname(name)
    print("Output:", name+'.xchtap')
    if split:
      while True:
        data = f.read(block_size)
        if data:
          oname = name+'.xchtap.'+str(ordinal).zfill(3)
          with open(oname, 'wb') as tap:
            packchunk(tap, dosname, ordinal, nchunks, data)
            ordinal += 1
        else:
          break
    else:
      with open(name+'.xchtap', 'wb') as tap:
        data = f.read(block_size)
        while data:
          packchunk(tap, dosname, ordinal, nchunks, data, delay)
          if pause > 0:
            tape_data(tap, bytearray([0x55] * pause * 256))
          ordinal += 1
          data = f.read(block_size)
    print('Done with', name, '          ')

def mkdosname(fname):
  # dos name is 8+3, but we only have 10 chars
  p = Path(fname)
  suffix = p.suffix[0:4]
  if len(suffix) > 0:
    return re.sub(
      r"(\.{2,})",
      '.',
      (p.stem[0:10-len(suffix)]+suffix)
    )
  else:
    return p.stem[0:8]

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
    print('Written fragment', ordinal, "size", len(data),'\r',end='')

def tape_header(t, type_id, name, size, ordinal, total):
  hdr = bytearray(19)
  # hdr[0] = 0, flag byte 00 (header)
  hdr[1] = type_id
  hdr[2:12] = pack('<10s', name)
  hdr[12:14] = pack('<H', size)
  hdr[14:16] = pack('<H', ordinal) # param1 - this block ordinal
  hdr[16:18] = pack('<H', total) # param2 - total # of blocks
  hdr[18] = chksum(hdr) # note that [18] must be 0!!!
  t.write(pack('<H', len(hdr)))
  t.write(hdr)

def tape_data(t, data):
  t.write(pack('<H', len(data)+2))
  t.write(b'\xff')
  t.write(data)
  t.write(pack('<B', chksum([0xff], data)))

if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('files', nargs='+')
  parser.add_argument('-b', '--block-size', type=int, default=8192)
  parser.add_argument('-s', '--split', action='store_true',
      help='store each chunk in a separate file')
  parser.add_argument('-p', '--pause', type=int, default=0,
      help='interval pause duration x 256b')
  parser.add_argument('-d', '--delay', action='store_true',
      help='delay before first data chunk')
  args = parser.parse_args()
  if args.block_size > MAX_BLOCK_SIZE:
    raise TypeError('Block size cannot be larger than {}'.format(MAX_BLOCK_SIZE))
  for name in args.files:
    split(name, args.delay, args.block_size, args.pause, args.split)

# EOF vim: ts=2:sw=2:et:ai:
