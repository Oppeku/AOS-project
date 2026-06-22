import { useRef, type PointerEvent as ReactPointerEvent } from "react";
import { X, Minus, Square } from "lucide-react";
import { useDesktop, type WindowInstance } from "@/lib/desktop-store";
import { APP_REGISTRY } from "./apps";
import { cn } from "@/lib/utils";

const TOPBAR_H = 36;
const DOCK_SPACE = 92;

export function AppWindow({ win }: { win: WindowInstance }) {
  const {
    focusWindow,
    closeWindow,
    minimizeWindow,
    toggleMaximize,
    moveWindow,
    resizeWindow,
    topZ,
  } = useDesktop();
  const meta = APP_REGISTRY[win.appId];
  const Body = meta.component;
  const dragState = useRef<{ dx: number; dy: number } | null>(null);
  const resizeState = useRef<{ sx: number; sy: number; sw: number; sh: number } | null>(null);

  const focused = win.z === topZ;

  function onHeaderDown(e: ReactPointerEvent) {
    if (win.maximized) return;
    focusWindow(win.id);
    dragState.current = { dx: e.clientX - win.x, dy: e.clientY - win.y };
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  }

  function onHeaderMove(e: ReactPointerEvent) {
    if (!dragState.current) return;
    const maxX = window.innerWidth - 80;
    const maxY = window.innerHeight - DOCK_SPACE;
    const x = Math.min(Math.max(e.clientX - dragState.current.dx, -win.width + 120), maxX);
    const y = Math.min(Math.max(e.clientY - dragState.current.dy, TOPBAR_H), maxY);
    moveWindow(win.id, x, y);
  }

  function onHeaderUp(e: ReactPointerEvent) {
    dragState.current = null;
    (e.target as HTMLElement).releasePointerCapture?.(e.pointerId);
  }

  function onResizeDown(e: ReactPointerEvent) {
    e.stopPropagation();
    focusWindow(win.id);
    resizeState.current = { sx: e.clientX, sy: e.clientY, sw: win.width, sh: win.height };
    (e.target as HTMLElement).setPointerCapture(e.pointerId);
  }

  function onResizeMove(e: ReactPointerEvent) {
    if (!resizeState.current) return;
    const w = Math.max(300, resizeState.current.sw + (e.clientX - resizeState.current.sx));
    const h = Math.max(220, resizeState.current.sh + (e.clientY - resizeState.current.sy));
    resizeWindow(win.id, w, h);
  }

  function onResizeUp(e: ReactPointerEvent) {
    resizeState.current = null;
    (e.target as HTMLElement).releasePointerCapture?.(e.pointerId);
  }

  const style = win.maximized
    ? { left: 0, top: TOPBAR_H, width: "100vw", height: `calc(100vh - ${TOPBAR_H + DOCK_SPACE - 16}px)`, zIndex: win.z }
    : { left: win.x, top: win.y, width: win.width, height: win.height, zIndex: win.z };

  if (win.minimized) return null;

  return (
    <div
      className={cn(
        "animate-window-in absolute flex flex-col overflow-hidden rounded-2xl border glass-window shadow-window",
        focused ? "border-border" : "border-border/40",
      )}
      style={style}
      onPointerDown={() => focusWindow(win.id)}
    >
      <div
        className="flex h-9 shrink-0 cursor-grab items-center gap-2 border-b border-border/50 bg-window-header px-3 no-select active:cursor-grabbing"
        onPointerDown={onHeaderDown}
        onPointerMove={onHeaderMove}
        onPointerUp={onHeaderUp}
        onDoubleClick={() => toggleMaximize(win.id)}
      >
        <meta.icon className="size-4 text-primary" />
        <span className="flex-1 truncate text-sm font-medium">{meta.name}</span>
        <div className="flex items-center gap-1.5" onPointerDown={(e) => e.stopPropagation()}>
          <WinBtn label="Minimize" onClick={() => minimizeWindow(win.id)} className="hover:bg-secondary">
            <Minus className="size-3" />
          </WinBtn>
          <WinBtn label="Maximize" onClick={() => toggleMaximize(win.id)} className="hover:bg-secondary">
            <Square className="size-2.5" />
          </WinBtn>
          <WinBtn
            label="Close"
            onClick={() => closeWindow(win.id)}
            className="hover:bg-destructive hover:text-destructive-foreground"
          >
            <X className="size-3" />
          </WinBtn>
        </div>
      </div>

      <div className="relative flex-1 overflow-hidden bg-card/70">
        <Body />
      </div>

      {!win.maximized && (
        <div
          className="absolute bottom-0 right-0 size-4 cursor-nwse-resize"
          onPointerDown={onResizeDown}
          onPointerMove={onResizeMove}
          onPointerUp={onResizeUp}
        />
      )}
    </div>
  );
}

function WinBtn({
  children,
  onClick,
  label,
  className,
}: {
  children: React.ReactNode;
  onClick: () => void;
  label: string;
  className?: string;
}) {
  return (
    <button
      aria-label={label}
      onClick={onClick}
      className={cn(
        "grid size-6 place-items-center rounded-full text-muted-foreground transition-colors",
        className,
      )}
    >
      {children}
    </button>
  );
}
