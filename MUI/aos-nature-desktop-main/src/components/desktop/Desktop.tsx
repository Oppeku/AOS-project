import { useState } from "react";
import { DesktopProvider, useDesktop } from "@/lib/desktop-store";
import { TopBar } from "./TopBar";
import { Dock } from "./Dock";
import { AppWindow } from "./AppWindow";
import { AppOverview } from "./AppOverview";
import { ALL_APPS, type AppId } from "./apps";
import { cn } from "@/lib/utils";
import wallpaper from "@/assets/aos-wallpaper.jpg";

const DESKTOP_ICONS: AppId[] = ["files", "settings", "about"];

function DesktopInner() {
  const { windows, openApp } = useDesktop();
  const [selected, setSelected] = useState<AppId | null>(null);

  return (
    <div
      className="relative h-screen w-screen overflow-hidden bg-cover bg-center"
      style={{ backgroundImage: `url(${wallpaper})` }}
      onPointerDown={() => setSelected(null)}
    >
      <TopBar />

      {/* Desktop shortcuts */}
      <div className="absolute left-4 top-12 flex flex-col gap-2">
        {DESKTOP_ICONS.map((id) => {
          const app = ALL_APPS.find((a) => a.id === id)!;
          return (
            <button
              key={id}
              onClick={(e) => {
                e.stopPropagation();
                setSelected(id);
              }}
              onDoubleClick={() => openApp(id)}
              className={cn(
                "flex w-20 flex-col items-center gap-1.5 rounded-xl p-2 text-center transition-colors",
                selected === id ? "bg-white/20 backdrop-blur-sm" : "hover:bg-white/10",
              )}
            >
              <div className={cn("grid size-11 place-items-center rounded-xl bg-gradient-to-br text-white shadow-md", app.tile)}>
                <app.icon className="size-6" />
              </div>
              <span className="text-xs font-medium text-white drop-shadow-[0_1px_2px_rgba(0,0,0,0.6)]">
                {app.name}
              </span>
            </button>
          );
        })}
      </div>

      {windows.map((win) => (
        <AppWindow key={win.id} win={win} />
      ))}

      <AppOverview />
      <Dock />
    </div>
  );
}

export function Desktop() {
  return (
    <DesktopProvider>
      <DesktopInner />
    </DesktopProvider>
  );
}
