import { defineConfig } from 'vitest/config';
import path from 'path';

export default defineConfig({
  test: {
    watch: false,
    globals: true,
    environment: 'node',
    include: ['test/**/*.test.ts', 'scripts/**/*.test.ts'],
    setupFiles: ['test/setup.ts'],
    // Native addons require sequential file execution to avoid race conditions
    // with static constructor references (EncodedAudioChunk::constructor, etc.)
    fileParallelism: false,
    coverage: {
      provider: 'v8',
      reporter: ['text', 'json', 'html'],
    },
  },
  resolve: {
    alias: {
      '@pproenca/node-webcodecs': path.resolve(__dirname, './lib'),
    },
  },
});
