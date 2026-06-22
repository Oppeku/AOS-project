import { useEffect, useState } from "react";
import { Search } from "lucide-react";
import { useDesktop } from "@/lib/desktop-store";
import { ALL_APPS } from "./apps";
import { cn } from "@/lib/utils";

export function AppOverview() {
  const { overviewOpen, setOverview, openApp } = useDesktop();
  const [query, setQuery] = useState("");

  useEffect(() => {
    if (overviewOpen) setQuery("");
    function onKey(e: KeyboardEvent) {
      if (e.key === "Escape") setOverview(false);
    }
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [overviewOpen, setOverview]);

  if (!overviewOpen) return null;

  const filtered = ALL_APPS.filter((a) =>
    a.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <div
      className="absolute inset-0 z-[9500] flex flex-col items-center bg-[oklch(0.18_0.02_152/0.55)] px-6 pt-16 backdrop-blur-2xl animate-fade-in"
      onClick={() => setOverview(false)}
    >
      <div className="relative w-full max-w-md" onClick={(e) => e.stopPropagation()}>
        <Search className="pointer-events-none absolute left-3.5 top-1/2 size-5 -translate-y-1/2 text-white/60" />
        <input
          autoFocus
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Type to search apps…"
          className="w-full rounded-2xl border border-white/15 bg-white/10 py-3 pl-11 pr-4 text-white outline-none placeholder:text-white/50 focus:border-white/40"
        />
      </div>

      <div
        className="mt-12 grid w-full max-w-3xl grid-cols-[repeat(auto-fill,minmax(108px,1fr))] gap-3"
        onClick={(e) => e.stopPropagation()}
      >
        {filtered.map((app) => (
          <button
            key={app.id}
            onClick={() => openApp(app.id)}
            className="flex flex-col items-center gap-2 rounded-2xl p-3 transition-colors hover:bg-white/10"
          >
            <div
              className={cn(
                "grid size-16 place-items-center rounded-2xl bg-gradient-to-br text-white shadow-lg",
                app.tile,
              )}
            >
              <app.icon className="size-8" />
            </div>
            <span className="text-center text-xs font-medium text-white">{app.name}</span>
          </button>
        ))}
        {filtered.length === 0 && (
          <p className="col-span-full text-center text-sm text-white/60">No apps found</p>
        )}
      </div>
    </div>
  );
}
