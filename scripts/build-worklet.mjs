import { build } from 'esbuild';
import { mkdir } from 'node:fs/promises';

await mkdir('public', { recursive: true });

await build({
  entryPoints: ['src/audio/worklet.ts'],
  bundle: true,
  format: 'iife',
  target: 'es2022',
  platform: 'browser',
  outfile: 'public/vrac-worklet.js',
  sourcemap: false,
  minify: false,
  logLevel: 'info',
});
