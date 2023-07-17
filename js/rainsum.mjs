import fs from 'fs';
import process from 'process';
import yargs from 'yargs';
import { hideBin } from 'yargs/helpers';
import { rainstormHash } from './lib/rainstorm.mjs';

const argv = yargs(hideBin(process.argv))
    .option('mode', {
        alias: 'm',
        description: 'Mode: digest or stream',
        type: 'string',
    })
    .option('algorithm', {
        alias: 'a',
        description: 'Specify the hash algorithm to use',
        type: 'string',
    })
    .option('size', {
        alias: 's',
        description: 'Specify the size of the hash',
        type: 'number',
    })
    .option('output-file', {
        alias: 'o',
        description: 'Output file for the hash',
        type: 'string',
    })
    .option('input-file', {
        alias: 'i',
        description: 'Input file to hash',
        type: 'string',
    })
    .help()
    .alias('help', 'h')
    .argv;

async function hashBuffer(mode, algorithm, seed, buffer, outputStream, hashSize) {
    const hash = await rainstormHash(hashSize, seed, buffer.toString());
    outputStream.write(hash);
}

async function hashAnything(mode, algorithm, seed, inputPath, outputPath, size) {
    let buffer = [];
    if (inputPath === '/dev/stdin') {
        for await (const chunk of process.stdin) {
            buffer.push(chunk);
        }
        buffer = Buffer.concat(buffer);
    } else {
        buffer = fs.readFileSync(inputPath);
    }

    const outputStream = fs.createWriteStream(outputPath);
    await hashBuffer(mode, algorithm, seed, buffer, outputStream, size);
}

async function main() {
    const mode = argv.mode || 'digest';
    const algorithm = argv.algorithm || 'rainstorm';
    const size = argv.size || 256;
    const inputPath = argv['input-file'] || '/dev/stdin';
    const outputPath = argv['output-file'] || '/dev/stdout';
    const seed = BigInt(argv['seed'] || 0);

    await hashAnything(mode, algorithm, seed, inputPath, outputPath, size);
}

main();

