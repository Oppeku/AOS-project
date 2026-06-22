import { Grid3x3 } from "lucide-react";
import { useDesktop } from "@/lib/desktop-store";
import { PINNED_APPS } from "./apps";
import { cn } from "@/lib/utils";

export function Dock() {
  const { openApp, windows, setOverview, overviewOpen } = useDesktop();

  const isOpen = (appId: string) =>
    windows.some((w) => w.appId === appId);

  return (
    <div className="pointer-events-none absolute inset-x-0 bottom-0 z-[9000] flex justify-center pb-3">
      <nav className="pointer-events-auto flex items-end gap-2 rounded-2xl border border-white/10 px-2.5 py-2 glass-panel shadow-dock no-select">
        {PINNED_APPS.map((app) => (
          <DockButton
            key={app.id}
            label={app.name}
            active={isOpen(app.id)}
            onClick={() => openApp(app.id)}
          >
            <div
              className={cn(
                "grid size-12 place-items-center rounded-xl bg-gradient-to-br text-white shadow-md",
                app.tile,
              )}
            >
              <app.icon className="size-6" />
            </div>
          </DockButton>
        ))}

        <div className="mx-1 h-12 w-px self-center bg-white/15" />

        <DockButton label="All Apps" active={overviewOpen} onClick={() => setOverview(!overviewOpen)}>
          <div className="grid size-12 place-items-center rounded-xl bg-white/10 text-panel-foreground">
            <Grid3x3 className="size-6" />
          </div>
        </DockButton>
      </nav>
    </div>
  );
}

function DockButton({
  children,
  label,
  active,
  onClick,
}: {
  children: React.ReactNode;
  label: string;
  active?: boolean;
  onClick: () => void;
}) {
  return (
    <button
      onClick={onClick}
      className="group relative flex flex-col items-center transition-transform duration-150 hover:-translate-y-1"
    >
      <span className="pointer-events-none absolute -top-9 whitespace-nowrap rounded-md bg-panel px-2 py-1 text-xs text-panel-foreground opacity-0 shadow-panel transition-opacity group-hover:opacity-100">
        {label}
      </span>
      {children}
      <span
        className={cn(
          "mt-1 h-1 w-1 rounded-full transition-colors",
          active ? "bg-panel-foreground" : "bg-transparent",
        )}
      />
    </button>
  );
}
