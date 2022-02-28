import fsp from 'node:fs/promises';

import {extractBuffer, FRAME_SIZE} from './index.mjs';

if (!process.argv[2]) {
	throw new Error('missing input');
}

const buf = await fsp.readFile(process.argv[2]);

console.log({FRAME_SIZE});

extractBuffer(buf, frames => {
	console.log(frames.length);
	for (const frame of frames) {
		for (let i = 0; i < frame.length; i += 3) {
			process.stdout.write(`\x1b[48;2;${frame[i]};${frame[i+1]};${frame[i+2]}m `);
		}
		console.log();
	}
});
console.log('DONE')
