import type { ReactNode, SVGProps } from 'react';

export type IconName =
  | 'undo'
  | 'redo'
  | 'dice'
  | 'save'
  | 'left'
  | 'right'
  | 'up'
  | 'down'
  | 'sliders'
  | 'midi'
  | 'sun'
  | 'moon'
  | 'help'
  | 'info'
  | 'download'
  | 'upload'
  | 'folder'
  | 'record'
  | 'play'
  | 'stop'
  | 'close'
  | 'copy'
  | 'paste'
  | 'reset'
  | 'power'
  | 'plus'
  | 'minus'
  | 'trash'
  | 'wave'
  | 'keyboard'
  | 'search'
  | 'check'
  | 'shuffle'
  | 'headphones';

const paths: Record<IconName, ReactNode> = {
  undo: (
    <>
      <path d="M9 14 4 9l5-5" />
      <path d="M4 9h9a7 7 0 0 1 0 14h-2" />
    </>
  ),
  redo: (
    <>
      <path d="m15 14 5-5-5-5" />
      <path d="M20 9h-9a7 7 0 0 0 0 14h2" />
    </>
  ),
  dice: (
    <>
      <rect x="4" y="4" width="16" height="16" rx="3.5" />
      <circle cx="8.5" cy="8.5" r="1.1" fill="currentColor" stroke="none" />
      <circle cx="15.5" cy="8.5" r="1.1" fill="currentColor" stroke="none" />
      <circle cx="12" cy="12" r="1.1" fill="currentColor" stroke="none" />
      <circle cx="8.5" cy="15.5" r="1.1" fill="currentColor" stroke="none" />
      <circle cx="15.5" cy="15.5" r="1.1" fill="currentColor" stroke="none" />
    </>
  ),
  save: (
    <>
      <path d="M5 4h11l3 3v13H5z" />
      <path d="M8 4v5h7V4" />
      <path d="M8 20v-7h8v7" />
    </>
  ),
  left: <path d="m14.5 5-7 7 7 7" />,
  right: <path d="m9.5 5 7 7-7 7" />,
  up: <path d="m5 14.5 7-7 7 7" />,
  down: <path d="m5 9.5 7 7 7-7" />,
  sliders: (
    <>
      <path d="M4 7h9M17 7h3M4 17h3M11 17h9" />
      <circle cx="15" cy="7" r="2" />
      <circle cx="9" cy="17" r="2" />
    </>
  ),
  midi: (
    <>
      <circle cx="12" cy="12" r="8" />
      <circle cx="9" cy="10" r="1" fill="currentColor" stroke="none" />
      <circle cx="15" cy="10" r="1" fill="currentColor" stroke="none" />
      <path d="M8.5 14.5h7M12 14v3" />
    </>
  ),
  sun: (
    <>
      <circle cx="12" cy="12" r="4" />
      <path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4" />
    </>
  ),
  moon: <path d="M20 14.4A8 8 0 1 1 9.6 4a6.5 6.5 0 0 0 10.4 10.4Z" />,
  help: (
    <>
      <circle cx="12" cy="12" r="8.5" />
      <path d="M9.7 9.3a2.4 2.4 0 1 1 3.4 2.2c-.7.3-1.1.9-1.1 1.7v.4M12 16.8h.01" />
    </>
  ),
  info: (
    <>
      <circle cx="12" cy="12" r="8.5" />
      <path d="M12 11v5M12 8h.01" />
    </>
  ),
  download: (
    <>
      <path d="M12 4v11M8 11l4 4 4-4" />
      <path d="M5 19h14" />
    </>
  ),
  upload: (
    <>
      <path d="M12 16V5M8 9l4-4 4 4" />
      <path d="M5 19h14" />
    </>
  ),
  folder: <path d="M3.5 7.5h6l2-2H20v13H3.5z" />,
  record: <circle cx="12" cy="12" r="5.5" fill="currentColor" stroke="none" />,
  play: <path d="M8 5.5v13l11-6.5z" />,
  stop: <rect x="7" y="7" width="10" height="10" rx="1" />,
  close: <path d="m6 6 12 12M18 6 6 18" />,
  copy: (
    <>
      <rect x="8.5" y="8.5" width="11" height="11" rx="2" />
      <path d="M15.5 8.5V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v7.5a2 2 0 0 0 2 2h2.5" />
    </>
  ),
  paste: (
    <>
      <path d="M9 5H6.5A1.5 1.5 0 0 0 5 6.5v13A1.5 1.5 0 0 0 6.5 21h11a1.5 1.5 0 0 0 1.5-1.5v-13A1.5 1.5 0 0 0 17.5 5H15" />
      <rect x="9" y="3" width="6" height="4" rx="1.5" />
    </>
  ),
  reset: (
    <>
      <path d="M4.5 9A8 8 0 1 1 4 14" />
      <path d="M4 4v5.5h5.5" />
    </>
  ),
  power: (
    <>
      <path d="M12 3v8" />
      <path d="M6.3 6.3a7.5 7.5 0 1 0 11.4 0" />
    </>
  ),
  plus: <path d="M12 5v14M5 12h14" />,
  minus: <path d="M5 12h14" />,
  trash: (
    <>
      <path d="M4.5 7h15M9 7V4.5h6V7M6.5 7l1 13h9l1-13" />
      <path d="M10 11v5M14 11v5" />
    </>
  ),
  wave: (
    <>
      <path d="M3 12h2M7 8v8M11 4v16M15 7v10M19 10v4M21 12h0" />
    </>
  ),
  keyboard: (
    <>
      <rect x="3" y="6" width="18" height="12" rx="2" />
      <path d="M7 10h.01M10.5 10h.01M14 10h.01M17.5 10h.01M7 14h.01M17 14h.01M10 14h4" />
    </>
  ),
  search: (
    <>
      <circle cx="11" cy="11" r="6.5" />
      <path d="m16 16 4 4" />
    </>
  ),
  check: <path d="m5 12.5 4.5 4.5L19 6.5" />,
  shuffle: (
    <>
      <path d="M4 7h3l10 10h3M17 4l3 3-3 3M4 17h3l2-2M17 20l3-3-3-3" />
    </>
  ),
  headphones: (
    <>
      <path d="M4 13v-1a8 8 0 0 1 16 0v1" />
      <rect x="3" y="13" width="4" height="7" rx="2" />
      <rect x="17" y="13" width="4" height="7" rx="2" />
    </>
  ),
};

interface IconProps extends Omit<SVGProps<SVGSVGElement>, 'name'> {
  name: IconName;
  size?: number;
}

export function Icon({ name, size = 16, ...rest }: IconProps) {
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      {...rest}
    >
      {paths[name]}
    </svg>
  );
}
