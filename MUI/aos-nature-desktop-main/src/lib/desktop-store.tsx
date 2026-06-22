import { createContext, useCallback, useContext, useMemo, useReducer, type ReactNode } from "react";
import { APP_REGISTRY, type AppId } from "@/components/desktop/apps";

export interface WindowInstance {
  id: string;
  appId: AppId;
  x: number;
  y: number;
  width: number;
  height: number;
  z: number;
  minimized: boolean;
  maximized: boolean;
  prev?: { x: number; y: number; width: number; height: number };
}

interface DesktopState {
  windows: WindowInstance[];
  topZ: number;
  overviewOpen: boolean;
}

type Action =
  | { type: "open"; appId: AppId }
  | { type: "close"; id: string }
  | { type: "focus"; id: string }
  | { type: "minimize"; id: string }
  | { type: "toggleMax"; id: string }
  | { type: "move"; id: string; x: number; y: number }
  | { type: "resize"; id: string; width: number; height: number }
  | { type: "setOverview"; open: boolean };

let counter = 0;
const nextId = () => `win-${++counter}`;

function reducer(state: DesktopState, action: Action): DesktopState {
  switch (action.type) {
    case "open": {
      const existing = state.windows.find((w) => w.appId === action.appId);
      const z = state.topZ + 1;
      if (existing) {
        return {
          ...state,
          overviewOpen: false,
          topZ: z,
          windows: state.windows.map((w) =>
            w.id === existing.id ? { ...w, minimized: false, z } : w,
          ),
        };
      }
      const meta = APP_REGISTRY[action.appId];
      const offset = (state.windows.length % 6) * 28;
      const width = meta.defaultSize?.width ?? 720;
      const height = meta.defaultSize?.height ?? 480;
      return {
        ...state,
        overviewOpen: false,
        topZ: z,
        windows: [
          ...state.windows,
          {
            id: nextId(),
            appId: action.appId,
            x: 120 + offset,
            y: 80 + offset,
            width,
            height,
            z,
            minimized: false,
            maximized: false,
          },
        ],
      };
    }
    case "close":
      return { ...state, windows: state.windows.filter((w) => w.id !== action.id) };
    case "focus": {
      const z = state.topZ + 1;
      return {
        ...state,
        topZ: z,
        windows: state.windows.map((w) =>
          w.id === action.id ? { ...w, z, minimized: false } : w,
        ),
      };
    }
    case "minimize":
      return {
        ...state,
        windows: state.windows.map((w) =>
          w.id === action.id ? { ...w, minimized: true } : w,
        ),
      };
    case "toggleMax":
      return {
        ...state,
        windows: state.windows.map((w) => {
          if (w.id !== action.id) return w;
          if (w.maximized) {
            return { ...w, maximized: false, ...(w.prev ?? {}) };
          }
          return {
            ...w,
            maximized: true,
            prev: { x: w.x, y: w.y, width: w.width, height: w.height },
          };
        }),
      };
    case "move":
      return {
        ...state,
        windows: state.windows.map((w) =>
          w.id === action.id ? { ...w, x: action.x, y: action.y } : w,
        ),
      };
    case "resize":
      return {
        ...state,
        windows: state.windows.map((w) =>
          w.id === action.id ? { ...w, width: action.width, height: action.height } : w,
        ),
      };
    case "setOverview":
      return { ...state, overviewOpen: action.open };
    default:
      return state;
  }
}

interface DesktopContextValue extends DesktopState {
  openApp: (appId: AppId) => void;
  closeWindow: (id: string) => void;
  focusWindow: (id: string) => void;
  minimizeWindow: (id: string) => void;
  toggleMaximize: (id: string) => void;
  moveWindow: (id: string, x: number, y: number) => void;
  resizeWindow: (id: string, width: number, height: number) => void;
  setOverview: (open: boolean) => void;
}

const DesktopContext = createContext<DesktopContextValue | null>(null);

export function DesktopProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(reducer, {
    windows: [],
    topZ: 10,
    overviewOpen: false,
  });

  const openApp = useCallback((appId: AppId) => dispatch({ type: "open", appId }), []);
  const closeWindow = useCallback((id: string) => dispatch({ type: "close", id }), []);
  const focusWindow = useCallback((id: string) => dispatch({ type: "focus", id }), []);
  const minimizeWindow = useCallback((id: string) => dispatch({ type: "minimize", id }), []);
  const toggleMaximize = useCallback((id: string) => dispatch({ type: "toggleMax", id }), []);
  const moveWindow = useCallback(
    (id: string, x: number, y: number) => dispatch({ type: "move", id, x, y }),
    [],
  );
  const resizeWindow = useCallback(
    (id: string, width: number, height: number) =>
      dispatch({ type: "resize", id, width, height }),
    [],
  );
  const setOverview = useCallback((open: boolean) => dispatch({ type: "setOverview", open }), []);

  const value = useMemo(
    () => ({
      ...state,
      openApp,
      closeWindow,
      focusWindow,
      minimizeWindow,
      toggleMaximize,
      moveWindow,
      resizeWindow,
      setOverview,
    }),
    [
      state,
      openApp,
      closeWindow,
      focusWindow,
      minimizeWindow,
      toggleMaximize,
      moveWindow,
      resizeWindow,
      setOverview,
    ],
  );

  return <DesktopContext.Provider value={value}>{children}</DesktopContext.Provider>;
}

export function useDesktop() {
  const ctx = useContext(DesktopContext);
  if (!ctx) throw new Error("useDesktop must be used within DesktopProvider");
  return ctx;
}
