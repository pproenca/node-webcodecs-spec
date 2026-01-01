// test/setup.ts
import { beforeAll, afterAll } from 'vitest';

// Global test setup
beforeAll(() => {
  console.log('Test suite starting...');
});

afterAll(() => {
  console.log('Test suite complete.');
});
