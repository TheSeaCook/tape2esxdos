#!/usr/bin/env python3

import os
import os.path
import unittest
from struct import unpack
import tempfile

from split import split

class TapReader():
  def __init__(self, file):
    self._file = file
    self._handle = None

  def __enter__(self):
    self._handle = open(self._file, 'rb')
    return self

  def __exit__(self, *args):
    if None != self._handle:
      self._handle.close()
      self._handle = None

  def digest(self):
    data = [ ]
    while True:
      size_bytes = self._handle.read(2)
      if not size_bytes or len(size_bytes) != 2: break
      size = unpack('<H', size_bytes)[0]
      block = self._handle.read(size)
      if len(block) != size: break
      # validate checksum?
      data.append(size)
      flag = int(block[0])
      data.append(flag)
      if 0x00 == flag:
        data.append(int(block[1])) # type id
    return data

class TzxReader(TapReader):
  def digest(self):
    data = [ ]
    hdr = self._handle.read(10)
    if not hdr or len(hdr) != 10: return data
    (sign, mark, major, minor) = unpack('<7sbbb', hdr)
    data.append(ord('!'))
    while True:
      blkid_b = self._handle.read(1)
      if not blkid_b: break
      blkid = unpack('b', blkid_b)[0]
      if 0x10 == blkid:
        hdr = self._handle.read(4)
        if not hdr or len(hdr) != 4: break
        (pause, length) = unpack('<HH', hdr)
        dummy = self._handle.read(length)
        if not dummy or len(dummy) != length: break
        data.append(0x10)
        data.append(length)
      elif 0x11 == blkid:
        hdr = self._handle.read(18)
        if not hdr or len(hdr) != 18: break
        (pilot, sync1, sync2, zero, one, plen, bits, pause, length, unused) = unpack(
            '<HHHHHHbHHb', hdr)
        dummy  = self._handle.read(length)
        if not dummy or len(dummy) != length: break
        data.append(0x11)
        data.append(length)
      else:
        data.append(0xff)
    return data

class TestChunkWriter(unittest.TestCase):
  def setUp(self):
    self.args = type('args', (object,), {
      "block_size":None, "turbo":False, "pause":False, "no_delay":False, "bundle":False, "split":False
    })()
    (handle, self._tap) = tempfile.mkstemp()
    with open(self._tap, 'wb') as tap:
      tap.write(bytearray(b'Hello world!'))

  def tearDown(self):
    os.remove(self._tap)

  def _get_digest(self, klass, args):
    file = split(self._tap, args)
    digest = None
    with klass(file) as tape:
      digest = tape.digest()
      print("Tape digest:", digest)
    os.remove(file)
    return digest

  def test_tap_none(self):
    self.assertCountEqual([19, 0, 135, 3, 255, 19, 0, 136, 14, 255],
        self._get_digest(TapReader, self.args))

  def test_tap_no_delay(self):
    self.args.no_delay = True
    self.assertCountEqual([19, 0, 136, 14, 255],
        self._get_digest(TapReader, self.args))

  def test_tap_pause(self):
    self.args.pause = True
    self.assertCountEqual([19, 0, 135, 3, 255, 19, 0, 136, 14, 255],
        self._get_digest(TapReader, self.args))

  def test_tap_bundle(self):
    if not os.path.exists('code-zx0.bin'): return
    llen = os.path.getsize('code-zx0.bin')+2 # flag+checksum
    self.args.bundle = True
    self.assertCountEqual([19, 0, 0, 125, 255, 19, 0, 3, llen, 255, 19, 0, 135, 3, 255, 19, 0, 136, 14, 255],
        self._get_digest(TapReader, self.args))

  def test_tap_split(self):
    self.args.split = True
    pass # FIXME: not implemented yet

  def test_tzx_none(self):
    self.args.turbo = True
    self.assertCountEqual([33, 17, 19, 17, 3, 17, 19, 17, 14],
        self._get_digest(TzxReader, self.args))

  def test_tzx_no_delay(self):
    self.args.turbo = True
    self.args.no_delay = True
    self.assertCountEqual([33, 17, 19, 17, 14],
        self._get_digest(TzxReader, self.args))

  def test_tzx_pause(self):
    self.args.turbo = True
    self.args.pause = True
    self.assertCountEqual([33, 17, 19, 17, 3, 17, 19, 17, 14],
        self._get_digest(TzxReader, self.args))

  def test_tzx_bundle(self):
    if not os.path.exists('code-zx0.bin'): return
    llen = os.path.getsize('code-zx0.bin')+2 # flag+checksum
    self.args.turbo = True
    self.args.bundle = True
    self.assertCountEqual([33, 16, 19, 16, 125, 16, 19, 16, llen, 17, 19, 17, 3, 17, 19, 17, 14],
        self._get_digest(TzxReader, self.args))

  def test_tzx_split(self):
    self.args.turbo = True
    self.args.split = True
    pass # FIXME: not implemented yet


if '__main__' == __name__:
  # better use python3 -m unittest -v
  unittest.main()

# EOF vim: et:ai:ts=4:sw=4:
