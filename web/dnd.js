// Copyright 2024 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

    "use strict";

    window.addEventListener('unhandledrejection', function(event) {
        console.warn('Unhandled exception from promise:', event.promise, event.reason);
    });

    class Syslog {
        constructor(id) {
            this.id = id
            this.queue = [ ]
        }
        log(msg) {
            if ('complete' === document.readyState) {
                this.flush()
                this._log(msg)
                this.log = this._log;
            } else {
                this.queue.push(msg)
            }
        }
        flush() {
            for (const m of this.queue) {
                this._log(m)
            }
            this.queue.length = 0;
        }
        _log(msg) {
            console.log('syslog.log', msg)
            const rec = document.createElement('p')
            rec.textContent = msg
            document.getElementById('syslog').appendChild(rec)
            document.getElementById('syslog').open = true
        }
    }

    class ProgressBar {
        constructor(progressBar, label) {
            this.progressBar = progressBar;
            this.label = label;
            this.progressBar.style.setProperty(
                '--buffered-width', '0%'
            )
        }
        setLength(length) {
            this.progressBar.max = length;
            this.progressBar.style.setProperty(
                '--buffered-width', '0%'
            )
        }
        setLabel(idx, total) {
            this.label.textContent = idx + '/' + total;
        }
        setChunk(offset, size) {
            console.log("setChunk", offset, size)
            this.chunkOffset = offset;
            this.chunkSize = size;
            this.progressBar.style.setProperty(
                '--buffered-width', `${Math.ceil(100 * (offset + size) / this.progressBar.max)}%`
            )
        }
        set(value) {
            this.progressBar.value = value;
        }
        advance() {
            this.progressBar.value = this.chunkOffset + this.chunkSize;
        }
        setChunkProgressFromAudio(audio) {
            this.progressBar.value = Math.ceil(this.chunkOffset
                + (this.chunkSize / audio.duration) * audio.currentTime);
        }
    }

    const syslog = new Syslog('syslog');

    (function(logger){ // Worker setup
        if (!logger) logger = console;
        function createWorker() {
            try {
                // add ?r=' + Math.random()); in dev environment to ensure reload
                // and ?r=RELTAG'); for released version
                const worker = new Worker('worker.js?v=2.1');
                var workerError;
                worker.addEventListener('error', workerError = function (evnt) {
                    console.log("Error creating Worker: ", evnt)
                    logger.log("Cannot create Worker, using same thread emulation");
                    createSingleThreaded()
                }, false)
                setTimeout(() => worker.removeEventListener('error', workerError), 1000);
                window.t2esx = worker;
                window.addEventListener('beforeunload', (evnt) => {
                    window.t2esx.terminate();
                    console.log("WebWorker terminated");
                });
            } catch (e) {
                logger.log("Cannot create Worker, using same thread emulation");
                createSingleThreaded()
            }
        }
        function createSingleThreaded() {
            function load(path) {
                const script = document.createElement('script');
                return new Promise((resolve, reject) => {
                    script.onload = () => resolve(script);
                    script.onerror = (e) => reject(e);
                    script.src = path;
                    document.head.append(script)
                });
            }
            return Promise.all([
                load('./wav.js'),
                load('./tzx.js'),
                load('./split.js')
            ]).then(() => {
                console.log('"Emulator" modules loaded')
                const channel = new MessageChannel();
                channel.port2.onmessage = t2esx.onmessage;
                window.t2esx = channel.port1;
                // https://developer.mozilla.org/en-US/docs/Web/API/MessagePort/start
                window.t2esx.start(); // required, unless .onmessage is used
                window.t2esx.terminate = function() { } // stub for beforeunload
            })
        }
        if (window.Worker && !document.URL.startsWith('file:')) {
            createWorker();
        } else {
            logger.log('WARNING: WebWorker is not available! Using "emulation", everything will less responsive!');
            createSingleThreaded();
        }
    })(syslog);

    if ('undefined' === typeof(SharedArrayBuffer)) {
        console.warn("SharedArrayBuffer replaced with ArrayBuffer")
        window.SharedArrayBuffer = ArrayBuffer
        syslog.log('WARNING: SharedArrayBuffer not available, memory consumption will be high!')
    }

    // DnD support
    function dndSetup(dropArea, droppedFilesHandler) {
        const handlers = [
            // https://developer.mozilla.org/en-US/docs/Web/API/HTML_Drag_and_Drop_API#define_a_drop_zone
            { events: ['dragenter', 'dragover', 'dragleave', 'drop'],
                handler: (evnt) => {
                    evnt.preventDefault();
                    evnt.stopPropagation();
                }
            },
            // actual implementation
            { events: ['dragenter', 'dragover'],
                handler: (evnt) => dropArea.classList.add('highlight')
            },
            { events: ['dragleave', 'drop'],
                handler: (evnt) => dropArea.classList.remove('highlight')
            },
            { events: ['change'],
                handler: evnt => droppedFilesHandler(evnt.target.files),
            },
            { events: ['drop'],
                handler: evnt => droppedFilesHandler(evnt.dataTransfer.files),
            }
        ];
        handlers.forEach(hndlr => {
            hndlr.events.forEach(name => {
                dropArea.addEventListener(name, hndlr.handler, false);
            });
        });
    }

    function mediaSetup() {
        const sel = document.getElementById('audio-sink');
        if (!navigator.mediaDevices) {
            syslog.log("No mediaDevices")
            sel.disabled = true;
        } else if (navigator?.mediaDevices.selectAudioOutput) {
            console.log("has selectAudioOutput")
            document.getElementById('help-mic').style.display = 'none';
            const btn = document.createElement('button')
            btn.textContent = 'Select Audio Output (Default)'
            sel.replaceWith(btn)
            btn.addEventListener('click', (evnt) => {
                let preferred;
                if (!!localStorage.getItem('audioDeviceId') && !window.audioDeviceId?.restored) {
                    preferred = { deviceId: localStorage.getItem('audioDeviceId') };
                    localStorage.setItem('audioDeviceId.restored', true);
                }
                navigator.mediaDevices.selectAudioOutput(preferred).then((info) => {
                    window.sink = { "deviceId":info.deviceId }
                    btn.textContent = info.label;
                    console.log('Selected sink', info);
                    localStorage.setItem('audioDeviceId', info.deviceId);
		            window.audioDeviceId = { restored: true };
                })
            }, false);
        } else if (navigator?.mediaDevices.enumerateDevices) {
            if (document.URL.startsWith('file:')) {
                syslog.log('WARNING: No access to all audio devices, default output only.')
                sel.disabled = true;
                document.getElementById('help-mic').style.display = 'none';
            } else {
            sel.addEventListener('focus', async function handler(evnt) {
                console.log("Audio init");
                sel.removeEventListener('focus', handler, false);
                navigator.mediaDevices.getUserMedia({audio:true})
                    .then(async (devices) => {
                        devices.getTracks().forEach(track => track.stop());
                        const sinks = (await navigator.mediaDevices.enumerateDevices())
                        .filter( (device) => device.kind === "audiooutput" );
                        evnt.target.options.length = 0;
                        sinks.forEach((device, i) => evnt.target.append(new Option( device.label || `device ${i}`, device.deviceId )) );
                        evnt.target.addEventListener('input', (evnt) => {
                            window.sink = { "deviceId": evnt.target.value }
                            console.log("Sink:", window.sink.deviceId)
                            })
                }, false)
            }, false)
            }
        }
    }

    // chrome says "Failed to parse audio contentType: audio/wav; codecs=" ???
    function checkMediaCapabilities() {
        if (!('mediaCapabilities' in navigator)) {
            console.warn('No mediaCapabilities');
            return;
        }
        const wavCfg = {
            type: 'file',
            audio: { contentType: 'audio/wav', channels: 1, samplerate: 44100, bitrate: 8*1*44100 }
        };
        navigator.mediaCapabilities.decodingInfo(wavCfg).then((result) => {
            console.log("decodingInfo", result);
        })
        // wavCfg.type = 'record';
        // navigator.mediaCapabilities.encodingInfo(wavCfg).then((result) => {
        //     console.log("encodingInfo", result);
        // })
        console.log("MediaSource", MediaSource.isTypeSupported('audio/wav'))
        console.log("HTML", new Audio().canPlayType('audio/wav'))
    }

    class T2esxUI {
        constructor() {
            this.blockSize = this._element_setup(
                't2esx-block', this._saveOption, 'blockSize');
            this.delay = this._element_setup(
                't2esx-delay', this._saveCheckbox, 'delay');
            this.pause = this._element_setup(
                't2esx-pause', this._saveOption, 'pause');
            this.speed = this._element_setup(
                't2esx-speed', this._saveOption, 'speed');
            this.bundle = this._element_setup(
                't2esx-bundle', this._saveCheckbox, 'bundle');
            this.speed.addEventListener('change',
                (e) => this._speedWarning(e.target.value), false);
            document.getElementById('clear-storage').addEventListener(
                'click', (e) => localStorage.clear(), false
            );
        }
        _element_setup(id, saveFn, tag) {
            const element = document.getElementById(id);
            element.addEventListener('change',
                (e) => saveFn(tag, e), false);
            return element;
        }
        _saveOption(tag, event) {
            localStorage.setItem(tag, event.target.value);
        }
        _restoreOption(tag, element) {
            const value = localStorage.getItem(tag);
            if (!!value) {
                element.value = value;
            }
        }
        _saveCheckbox(tag, event) {
            localStorage.setItem(tag, event.target.checked);
        }
        _restoreCheckbox(tag, element, undefValue) {
            const checked = localStorage.getItem(tag);
            if (null != checked) {
                element.checked = 'true' == checked;
            } else if ('undefined' !== typeof(undefValue)) {
                element.checked = undefValue;
            }
        }
        _speedWarning(value) {
            if ('8' == value || '4' == value) {
                document.getElementById('warn-high-speed').style.display = 'block';
            } else {
                document.getElementById('warn-high-speed').style.display = 'none';
            }
        }
        restore() {
            this._restoreOption('blockSize', this.blockSize);
            this._restoreCheckbox('delay', this.delay, true);
            this._restoreOption('pause', this.pause);
            this._restoreOption('speed', this.speed);
            this._restoreOption('bundle', this.bundle);
            this._speedWarning(this.speed.value);
        }
    }

    let player;
    
window.addEventListener('load', function(event) {
    syslog.flush();

    new T2esxUI().restore();

    player = new AudioPlayer(
        document.getElementById('audio-player'),
        new ProgressBar(document.getElementById('progress-bar'), document.getElementById('progress-label'))
    );
    
    if (/Firefox\/123\.0/.test(navigator.userAgent)) {
        syslog.log("WARN: Firefox 123.0 disables support for WAV files if 'media.ffvpx.enabled' is on, check about:support and adjust using about:config, if necessary.");
    }

    
    dndSetup(document.getElementById('drop-area'), handleFiles);
    
    mediaSetup();

    checkMediaCapabilities();

})

function handleFiles(files) {
    console.log("Got files", files)
    cleanup()
    window.files = files
    let idx = 0;
    ;([...files]).forEach((file) => {
        addRecord(file.name, idx++)
    })
}
function addRecord(name, idx) {
    const clone = document.querySelector('#links-row').content.cloneNode(true)
    clone.querySelector('b').textContent
        = `${name} (${Math.ceil(files[idx].size/1024)}K) -> ${name2tap(files[idx].name.toUpperCase())}`
    clone.querySelectorAll('a').forEach((node) => {
        Object.assign(node, {
            action: node.classList[0],
            fileName: name,
            idx: idx,
            tapeType: node.classList[1]
        });
        node.addEventListener('click', streamingEventHandler, false);
    })
    document.querySelector('#links').appendChild(clone)
}
function streamingEventHandler(event) {
    var node = event.target;
    while ('A' !== node.nodeName) node = node.parentNode;
    console.log("Node click", node);
    if ('tape' === node.action) {
        downloadTape(`${node.fileName}.${node.tapeType}`,
            new TapeChunkedSource(files[node.idx],
                parseInt(document.getElementById('t2esx-block').value, 10),
                `${node.fileName}`,
                node.tapeType)
        );
    } else if ('wav' == node.action) {
        playWav(
            new WavChunkedSource(files[node.idx],
                parseInt(document.getElementById('t2esx-block').value, 10),
                `${node.fileName}`,
                node.tapeType)
        );
    } else {
        console.warn("Unsupported action", node.action);
    }
}
async function downloadTape(label, source) {
    const tape = [ ];
    for (let i=0; i<source.count(); i++) {
        source.set(i);
        // we need a copy here to avoid
        // "Failed to construct 'Blob': The provided ArrayBufferView value must not be shared."
        tape.push(new Uint8Array((await source.chunkAsUint8Array()).bytes));
    }
    downloadBlob(label, new Blob(tape, {type: 'application/octet-stream'}));
    tape.length = 0;
}
function playWav(source) {
    player.setSource(source);
    player.play(0);
}

class ChunkedSource {
    constructor(count) {
        this.idx = 0;
        this.total = count;
    }
    count() {
        return this.total;
    }
    position() {
        return this.idx;
    }
    set(idx) { // set chunk index
        // if (idx >= 0 && idx < this.count) {
            this.idx = idx;
        // }
    }
    /*boolean*/ next() {
        const hasNext = (this.position() + 1) < this.count();
        if (hasNext) this.set(this.position() + 1);
        return hasNext;
    }
    size() { // size in bytes
        throw Error("Not implemented: ChunkedSource.size()");
    }
    /*Promise -> Uint8Array*/ chunkAsUint8Array() {
        throw Error("Not implemented: ChunkedSource.chunkAsUint8Array()");
    }
}
class FileChunkedSource extends ChunkedSource {
    constructor(file, chunkSize) {
        super(Math.ceil(file.size / chunkSize));
        this.file = file;
        this.chunkSize = chunkSize;
    }
    getChunkSize() {
        return this.chunkSize;
    }
    size() {
        return this.file.size;
    }
    /*Promise -> Uint8Array*/ chunkAsUint8Array() {
        return FileReaderPromise(
            this.file.slice(this.idx * this.chunkSize,
                (this.idx + 1) * this.chunkSize)
        );
    }
}
class MeasuredFileChunkedSource extends FileChunkedSource {
    constructor(file, chunkSize, label, type) {
        super(file, chunkSize);
        this.label = Array.from(new TextEncoder().encode(name2tap(label)));
        this.type = type;
    }
    // we need to know underlying file chunk size
    getSourceChunkSize() {
        return this?.sourceChunkSize;
    }
    /*Promise -> Uint8Array*/ chunkAsUint8Array() {
        return super.chunkAsUint8Array().then((chunk) => {
            this.sourceChunkSize = chunk.byteLength;
            return chunk;
        });
    }
}
class TapeChunkedSource extends MeasuredFileChunkedSource {
    /*Promise -> Uint8Array*/ chunkAsUint8Array() {
        return super.chunkAsUint8Array().then((chunk) => {
            return T2ESX({'type': this.type,
                'name': this.label,
                'bytes': chunk,
                'ordinal': this.position() + 1,
                'total': this.count(),
                'options': t2esxOptions()
            });
        });
    }
}
class WavChunkedSource extends MeasuredFileChunkedSource {
    /*Promise -> Uint8Array*/ chunkAsUint8Array() {
        return super.chunkAsUint8Array().then((chunk) => {
            const options = Object.assign(t2esxOptions(), { 'type': this.type })
            return T2ESX({'type': 'wav',
                'name': this.label,
                'bytes': chunk,
                'ordinal': this.position() + 1,
                'total': this.count(),
                'options': options
            });
        });
    }

}
function EventDataPromise(target, tag) {
    return new Promise((resolve, reject) => {
        const oneTimeHandler = (e) => {
            e.target.removeEventListener(tag, oneTimeHandler, false);
            console.log('EventDataPromise response:', e.data);
            resolve(e.data);
        }
        target.addEventListener(tag, oneTimeHandler, false);
        // TODO: reject on timeout
    });
}
function T2ESX(message) {
    const promise = EventDataPromise(window.t2esx, 'message');
    window.t2esx.postMessage(message);
    return promise;
}

class AudioPlayer {
    // UI DOM Element
    constructor(element, progress) {
        this.source = null; // FIXME:
        this.audio = new Audio();
        this.current = 0;
        this.total = 0; // FIXME:
        this.progress = progress;
        this.stop = element.querySelector('#playback-stop');
        this.stop.addEventListener('click', (e) => this.onStop(e), false);
        this.prev = element.querySelector('#playback-prev');
        this.prev.addEventListener('click', (e) => this.onPrev(e), false);
        this.next = element.querySelector('#playback-next');
        this.next.addEventListener('click', (e) => this.onNext(e), false);
        //
        this.audio.addEventListener('timeupdate', (e) => this._onAudioTimeupdate(e), false);
        this.audio.addEventListener('ended', (e) => this._onAudioEnded(e), false);
        this.audio.addEventListener('error', (e) => this._onAudioError(e), false);
        this.onTrackChange(0, 1, false);
    }
    setSource(/*ChunkedSource*/ source) {
        this.source = source;
        this.current = source.position();
        this.total = source.count();
        this.progress.setLength(this.source.size());
    }
    play(idx) {
        this.source.set(idx);
        this.current = this.source.position();
        this.progress.setLabel(this.current+1, this.source.count());
        this.source.chunkAsUint8Array().then((data) => {
            this.progress.setChunk(
                this.source.getChunkSize() * this.source.position(),
                this.source.getSourceChunkSize());
            this.onTrackChange(this.current, this.total, true);
            this._playback(
                new Blob([new Uint8Array(data.bytes)], {type: 'audio/wave'}),
                window.sink?.deviceId)
        });
    }
    _playback(blob, deviceId) {
        this.audio.preservesPitch = false; // no processing when sped up!!!
        this.setMedia(blob);
        if (!!deviceId) {
            this.audio.setSinkId(deviceId);
        }
        this.audio.play();
    }
    setMedia(blob) {
        this._clearMedia();
        this.audio.src = URL.createObjectURL(blob);
    }
    reset() {
        this.progress.set(0);
        this.progress.setLength(0);
        this.progress.setLabel(' ', ' ');
        this.onTrackChange(0, 1, false);
    }
    _clearMedia() {
        const url = this.audio?.src;
        if (!!url) {
            delete this.audio.src;
            URL.revokeObjectURL(url);
        }
    }
    _onAudioTimeupdate(event) {
        // sometimes 'duration' is NaN AND we're getting timeupdate???
        if (this.audio.duration > 0) {
            this.progress.setChunkProgressFromAudio(this.audio);
        }
    }
    _onAudioEnded(event) {
        this._clearMedia();
        const next = this.current + 1;
        if (next < this.total) {
            this.play(next);
        } else { // that was the last track
            this.onTrackChange(0, 1, false);
        }
    }
    _onAudioError(event) {
        this._clearMedia();
        this.onTrackChange(0, 1, false);
    }
    onTrackChange(current, total, playing) {
        this.prev.disabled = 0 == current;
        this.next.disabled = (current + 1) == total;
        this.stop.disabled = !playing;
    }
    onStop(event) {
        if (!!this.audio) {
            this.audio.pause();
            // FIXME: wrong
            this.current = this.total - 1; // pretend we're done with the last track
            this.audio.dispatchEvent(new Event('ended'));
        }
    }
    onPrev(event) {
        if (!!this.audio) {
            this.play(this.current - 1);
            //this.onStop(event);
        }
    }
    onNext(event) {
        if (!!this.audio) {
            this.play(this.current + 1);
            //this.onStop(event);
        }
    }
}

function t2esxOptions() {
    return {
        'delay': document.getElementById('t2esx-delay').checked,
        'pause': parseInt(document.getElementById('t2esx-pause').value),
        'speed': parseInt(document.getElementById('t2esx-speed').value),
        'bundle': document.getElementById('t2esx-bundle').checked
    }
}
function name2tap(s) {
    const idx = s.lastIndexOf('.')
    const suffix = s.substring(idx).replace(/[^A-Za-z0-9]/g, '').substring(0, 3)
    const name = s.substring(0, idx).replace(/[^A-Za-z0-9]/g, '').substring(0, 10-suffix.length-(suffix.length>0 ? 1 : 0))
    return (name + '.' + suffix).padEnd(10, ' ')
}
function FileReaderPromise(buffer) {
    const reader = new FileReader()
    const promise = new Promise((resolve, reject) => {
        reader.onerror = () => {
            reader.abort()
            reject()
        }
        reader.onload = () => {
            console.log(reader.result)
            resolve(new Uint8Array(reader.result))
        }
    })
    reader.readAsArrayBuffer(buffer)
    return promise
}

function downloadBlob(name, blob) {
    const link = document.createElement('a');
    link.download = name;
    link.href = URL.createObjectURL(blob);
    link.click();
    URL.revokeObjectURL(link.href);
}

function cleanup() {
    const node = document.querySelector('#links')
    node.replaceChildren(node.querySelector('#links-row'))
    player.reset();
}
// EOF vim: et:ai:ts=4:sw=4:
