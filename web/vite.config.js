import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  // The XR packages pull their own three; a second copy breaks instanceof checks.
  resolve: { dedupe: ['three'] },
  server: { port: 5173 },
});
