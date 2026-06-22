import { useEffect, useState } from "react";
import { Wifi, Volume2, BatteryFull, Search, Power } from "lucide-react";
import { useDesktop } from "@/lib/desktop-store";
import { cn } from "@/lib/utils";

export function TopBar() {
  const { overviewOpen, setOverview } = useDesktop();
  const [now, setNow] = useState(new Date());

  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(id);
  }, []);

  return (
    <header className="absolute inset-x-0 top-0 z-[9999] flex h-9 items-center justify-between px-3 text-panel-foreground glass-panel no-select">
      <div className="flex items-center gap-1">
        <button
          onClick={() => setOverview(!overviewOpen)}
          className={cn(
            "flex items-center gap-2 rounded-lg px-2.5 py-1 text-sm font-semibold transition-colors hover:bg-white/10",
            overviewOpen && "bg-white/15",
          )}
        >
          <span className="grid size-4 place-items-center rounded bg-primary text-[10px] font-bold text-primary-foreground">
            A
          </span>
          Activities
        </button>
      </div>

      <button
        onClick={() => setOverview(true)}
        className="absolute left-1/2 -translate-x-1/2 text-sm font-medium tabular-nums transition-opacity hover:opacity-80"
      >
        {now.toLocaleDateString([], { weekday: "short", month: "short", day: "numeric" })}{"  "}
        {now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })}
      </button>

      <div className="flex items-center gap-1">
        <button
          onClick={() => setOverview(true)}
          className="grid size-7 place-items-center rounded-lg transition-colors hover:bg-white/10"
        >
          <Search className="size-4" />
        </button>
        <div className="flex items-center gap-2.5 rounded-lg px-2.5 py-1 transition-colors hover:bg-white/10">
          <Wifi className="size-4" />
          <Volume2 className="size-4" />
          <BatteryFull className="size-4" />
        </div>
        <button className="grid size-7 place-items-center rounded-lg transition-colors hover:bg-white/10">
          <Power className="size-4" />
        </button>
      </div>
    </header>
  );
}
