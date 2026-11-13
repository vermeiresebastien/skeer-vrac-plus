import { createRoot } from 'react-dom/client';
import '@fontsource/archivo/400.css';
import '@fontsource/archivo/500.css';
import '@fontsource/archivo/600.css';
import '@fontsource/archivo/700.css';
import '@fontsource/archivo-narrow/400.css';
import '@fontsource/archivo-narrow/500.css';
import '@fontsource/archivo-narrow/600.css';
import '@fontsource/ibm-plex-mono/400.css';
import '@fontsource/ibm-plex-mono/500.css';
import App from './App';
import './styles.css';
import './css/panels.css';
import './css/modals.css';
import './css/responsive.css';

const root = document.getElementById('root');
if (!root) throw new Error('Root element missing');

createRoot(root).render(<App />);
