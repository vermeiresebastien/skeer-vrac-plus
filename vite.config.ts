/// <reference types="vitest/config" />
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    host: '127.0.0.1',
    port: 5173,
    strictPort: true,
    watch: {
      ignored: [
        '**/shots/**',
        '**/refs/**',
        '**/fixtures/**',
        '**/tests/**',
        '**/test-results/**',
        '**/native/**',
        // The export target rewrites a 23 MB exe and a 24 MB VST3 module in place. Watching them
        // makes `npm run dev` die with "EBUSY: resource busy or locked, watch launch/SliceForge.exe"
        // the moment someone runs `--target launch` alongside it. Nothing there is source.
        '**/launch/**',
        '**/*.zip',
        '**/*.wav',
        '**/dev-server.log',
      ],
    },
  },
  test: {
    exclude: ['**/node_modules/**', '**/dist/**', '**/native/**'],
  },
  preview: {
    host: '127.0.0.1',
    port: 4173,
    strictPort: true,
  },
  build: {
    target: 'es2022',
    sourcemap: true,
  },
});
