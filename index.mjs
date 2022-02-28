import ddbNative from './stub.cjs';

const {extractFrames, BEGINNING, END, RELATIVE, FRAME_SIZE} = ddbNative;

export {
	BEGINNING,
	END,
	RELATIVE,
	FRAME_SIZE
};

export function extract({read, seek, tell, frames}) {
	return extractFrames(read, seek, tell, frames);
}

export function extractBuffer(buf, onFrames) {
	if (!Buffer.isBuffer(buf)) {
		throw new TypeError('first argument must be buffer');
	}

	if (typeof onFrames !== 'function') {
		throw new TypeError('second argument must be callback function');
	}

	if (buf.length === 0) {
		throw new RangeError('empty buffer');
	}

	let cursor = 0;
	let remaining = buf.length;
	const r = extract({
		read(dest, sz) {
			const toWrite = Math.min(sz, remaining);
			buf.copy(dest, 0, cursor, cursor + toWrite);
			cursor += toWrite;
			remaining -= toWrite;
			return toWrite;
		},
		seek(pos, w) {
			switch (w) {
				case BEGINNING:
					cursor = Math.min(buf.length, pos);
					break;
				case RELATIVE:
					cursor = Math.min(buf.length, cursor + pos);
					break;
				case END:
					cursor = Math.max(0, buf.length - pos);
					break;
				default:
					throw new Error('unexpected direction value: ' + w.toString());
			}

			remaining = buf.length - cursor;
			return true;
		},
		tell() {
			return cursor;
		},
		frames: onFrames
	});

	if (!r) {
		throw new Error('extraction failed without error (potentially bug in bindings)');
	}
}
