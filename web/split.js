// Copyright 2024 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

// t2esx core logic

'use strict';

// we assume tzx.js and wav.js are already loaded
const t2esx = (function() {
if ('undefined' === typeof(SharedArrayBuffer)) {
    console.warn("SharedArrayBuffer replaced with ArrayBuffer")
    SharedArrayBuffer = ArrayBuffer;
}

const TapeType = {
    TAP: 'tap',
    TZX: 'tzx'
};

let loaderBuffer;

fetch(new Request('t2esx-zx0.tap'))
    .then((response) => { 
        if (response.ok) {
            response.blob();
        } else {
            throw Error('Bad response code ' + response.status);
        }
    }).then((blob) => {
        console.log("Fetch OK for loader", blob)
        return blob.arrayBuffer()
    }).then((arrayBuffer) => {
        loaderBuffer = arrayBuffer;
        console.log("Loader buffer ready", loaderBuffer.byteLength);
    })
    .catch(err => console.warn('Fetch loader', err));

function messageHandler(event) {
    console.log("Inside worker:", event.data);
    if (event?.data?.type) {
        const data = event.data;
        const type = data.type;
        if ('tap' === type) {
            const tap = chunk2tap(data.name,
                data.bytes, data.ordinal, data.total,
                data.options)
            postBack('tap', tap, data);
        } else
        if ('tzx' === type) {
            const tap = chunk2tzx(data.name,
                data.bytes, data.ordinal, data.total,
                data.options)
            postBack('tzx', tap, data);
        } else
        if ('wav' === type) {
            const chunk2tape = (TapeType.TZX === data.options?.type
                ? chunk2tzx : chunk2tap);
            const wav = generateWav(data.options?.type,
                chunk2tape(data.name, data.bytes,
                    data.ordinal, data.total,
                    data.options),
                data.options?.speed)
            postBack('wav', wav, data);
        }
    } else {
        console.log("PING/PONG", event.data);
        postMessage(event.data); // simple debug echo
    }
}

function postBack(type, bytes, data) {
    // TODO: re-construct data
    delete data.bytes; // no need to pass it back
    //console.log('postBack', type, bytes.byteLength, data);
    postMessage({
        'type': type, 'bytes': bytes, 'ordinal': data.ordinal, 'total': data.total
    })
}

function generateWav(tapeType, bytes, speed) {
    if (!speed) speed = 1;
    console.time("generateWav", speed)
    const wave = wav.create(1, 44100, wav.SampleSize.EIGHT)
    try {
        const tape = bytes
        const data = {
            getLength: () => { return tape.byteLength },
            getByte: (i) => { return tape[i] }
        }
        const machineSettings = { ...tzx.MachineSettings.ZXSpectrum48 };
        if (TapeType.TZX === tapeType) {
            if (speed > 1) console.warn("No TURBO for TZX files");
            tzx.convertTzxToAudio(machineSettings, data, wave);
        } else {
            if (speed > 1 && speed < 9) {
                machineSettings.clockSpeed *= speed;
                console.log('machine settings', machineSettings);
            }
            tzx.convertTapToAudio(machineSettings, data, wave);
        }
    } catch (e) {
        console.warn('Unable to play tape:', e)
    } finally {
        console.timeEnd('generateWav')
    }
    const output = wave.toByteArray()
    const parcel = new Uint8Array(new SharedArrayBuffer(output.length))
    parcel.set(output, 0)
    console.log('Parcel:', parcel)
    return parcel
}

class Buffer {
    constructor(size) {
        this.buffer = new Uint8Array(new SharedArrayBuffer(size));
        this.pos = 0;
    }
    arrayBuffer() {
        return this.buffer;
    }
    append(...bytes) {
        for (const arg of bytes) {
            console.log("append", arg)
            this.buffer.set(arg, this.pos)
            this.pos += arg.length
        }
    }
    checksum(start, length) {
        let sum = 0
        for (let i=0; i<length; i++) {
            sum ^= this.buffer[start + i]
        }
        console.log("checksum", sum, start+length)
        this.buffer[start + length - 1] = sum
    }
}

class Chunk2Tape {
    constructor(tapePrologue, blockPrologue) {
        this.tapePrologue = tapePrologue;
        this.blockPrologue = blockPrologue;
    }
    chunk2tape(name, chunk, ordinal, total, options) {
    console.log("chunk2tape", name, chunk, ordinal, total, options);
    const /*bool*/ needsLoader = 1 == ordinal && !!options.bundle && !!loaderBuffer;
    const extras = needsLoader ? loaderBuffer.byteLength : 0;
    // BP - block prologue, 2 bytes (length) for TAP, XYZ for TZX
    // overhead: 19 header+BP, +BP+2 bytes per block (len+flag+checksum)
    //    + delay (19+DP + 1+BP+2) + pause (BP+2+pause*256)
    const BPL = this.blockPrologue(0).length;
    // for WAV conversion we need each chunk to be a valid TZX file
    // for download this is not necessary since chunks are concatenated
    const TZX_HACK = (TapeType.TZX === options?.type ? this.tapePrologue().length : 0);
    const size = (1 == ordinal ? this.tapePrologue().length : 0)
        + (1 == ordinal && options.delay ? 19+BPL + BPL+2+1 : 0)
        + 19+BPL + chunk.byteLength + BPL + 2
        + (options.pause > 0 && ordinal < total > 0 ? BPL+2 + options.pause*256 : 0)
        + (ordinal > 1 ? TZX_HACK : 0)
        + extras;
    console.log("Expected chunk size", size)
    const buffer = new Buffer(size)
    var pos = 0;
    if (1 == ordinal) {
        buffer.append(this.tapePrologue());
        if (needsLoader) {
            buffer.append(new Uint8Array(loaderBuffer));
        }
        if (options.delay) {
            const placeholder = new Uint8Array([0x77]);
            this.header(buffer, name, 0x87, ordinal, total, placeholder.byteLength);
            this.data(buffer, placeholder);
        }
    } else if (TapeType.TZX === options?.type) { // FIXME: ???
        buffer.append(this.tapePrologue());
    }
    this.header(buffer, name, 0x88, ordinal, total, chunk.byteLength);
    this.data(buffer, chunk)
    if (options.pause > 0 && ordinal < total) {
        // no header here
        this.data(buffer, new Uint8Array(new Array(options.pause * 256).fill(0x55)))
    }
    console.log("tap:", buffer)
    return buffer.arrayBuffer();
    }
    header(buffer, name, typeId, param1, param2, len) {
        buffer.append(this.blockPrologue(19)) // block prologue
        const start = buffer.pos
        buffer.append([
            0x00, // flag byte, header
            typeId // type id
            ],
            name, // 10 bytes
            [
            (len % 256), Math.floor(len / 256),
            param1 % 256, Math.floor(param1 / 256),
            param2 % 256, Math.floor(param2 / 256),
            0 // checksum placeholder
        ])
        buffer.checksum(start, buffer.pos - start)
    }
    data(buffer, chunk) {
        const sz = chunk.byteLength + 2
        buffer.append(this.blockPrologue(sz))
        const start = buffer.pos
        buffer.append([
            0xff,
            ],
            chunk,
            [ 0 ]) // checksum placeholder
        buffer.checksum(start, buffer.pos - start)
    }
}

class Chunk2TAP extends Chunk2Tape {
    constructor() {
        super(tapePrologue, blockPrologue);
        function tapePrologue() {
            return [];
        }
        function blockPrologue(size) {
            return [ size%256, Math.floor(size/256)];
        }
    }
}

class Chunk2TZX extends Chunk2Tape {
    constructor() {
        super(tapePrologue, blockPrologue);
        function tapePrologue() {
            return [
                90, 88, 84, 97, 112, 101, 33, // ZXTape!
                0x1a, 0x01, 0x14
            ];
        }
        function blockPrologue(size) {
            return [
                0x11, // turbo speed block type id
                0x80, 0x05, // PILOT: 1408
                0x8d, 0x01, // SYNC 1st: 397
                0x3d, 0x01, // SYNC 2nd: 317
                0x45, 0x01, // ZERO: 325
                0x89, 0x02, // ONE: 649
                0xe3, 0x12, // PILOT len: 4835
                0x08, // last byte used bits
                0x3e, 0x01, // pause after
                size%256, Math.floor(size/256), 0, // data length
            ];
        }
    }
}
function chunk2tap(name, chunk, ordinal, total, options) {
    return new Chunk2TAP().chunk2tape(name, chunk, ordinal, total, options);
}

function chunk2tzx(name, chunk, ordinal, total, options) {
    return new Chunk2TZX().chunk2tape(name, chunk, ordinal, total, options);
}

return {
    onmessage: messageHandler
};
})();

if (typeof exports !== 'undefined') {
    exports.messageHandler = t2esx.messageHandler;
}
// EOF vim: et:ai:ts=4:sw=4:
