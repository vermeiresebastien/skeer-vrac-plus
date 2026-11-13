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
