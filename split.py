#!/usr/bin/env python3

from os.path import getsize, exists
from pathlib import Path
from struct import pack, unpack
from sys import argv

# not sure what's at the top, full 16K may not fit
BLOCK_SIZE=16384

#read file in BLOCK_SIZE chunks, write to TAP with
#  ID 0x88, vars -- total number of chunks, hdadd -- current chunk number

def split(name, block_size=(BLOCK_SIZE/2)):
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
    with open(name+'.xchtap', 'wb') as tap:
      data = f.read(block_size)
      while data:
        packchunk(tap, dosname, ordinal, nchunks, data)
        if pause > 0:
          tape_data(tap, bytearray([0x55] * pause * 256))
        ordinal += 1
        data = f.read(block_size)
    print('Done with', name)

def mkdosname(fname):
  return p.stem[0:8]

def chksum(*args):
  chksum = 0
  for data in args:
    for b in data: chksum ^= b
  return chksum

def packchunk(t, dosname, ordinal, total, data):
    hdr = bytearray(19) # hdr[0] = 0, flag byte 00 (header)
    hdr[1] = 0x88 # type id
    hdr[2:12] = pack('<10s', dosname.ljust(10).encode('ASCII')) # name FIXME
    hdr[12:14] = pack('<H', len(data)) # data length
    hdr[14:16] = pack('<H', ordinal) # param1 - this block ordinal
    hdr[16:18] = pack('<H', total) # param2 - total # of blocks
    hdr[18] = chksum(hdr)
    # header
    t.write(pack('<H', len(hdr)))
    t.write(hdr)
    # data
    tape_data(t, data)
    print('Written fragment', ordinal, "size", len(data),'\r',end='')
    # TODO: real hardware needs time to write the chunk
    # we need to add a "placeholder" with dummy data after each data chunk

def tape_data(t, data):
  t.write(pack('<H', len(data)+2))
  t.write(b'\xff')
  t.write(data)
  t.write(pack('<B', chksum([0xff], data)))

if __name__ == '__main__':
  for name in argv[1:]:
    split(name, BLOCK_SIZE)

# EOF vim: ts=2:sw=2:et:ai:
