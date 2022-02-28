declare const BEGINNING: number;
declare const RELATIVE: number;
declare const END: number;

declare function extract(callbacks: {
	read: (buf: Buffer, sz: number) => number,
	seek: (pos: number, whence: number) => boolean,
	tell: () => number,
	frames: (frames: Buffer[]) => void
}): void;

declare function extractBuffer(buf: Buffer, frames: (frames: Buffer[]) => void): void;

export {
	BEGINNING,
	RELATIVE,
	END,
	extract,
	extractBuffer
};
