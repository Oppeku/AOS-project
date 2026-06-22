import { useState } from "react";
import {
  Folder,
  TerminalSquare,
  FileText,
  Calculator,
  Activity,
  CalendarDays,
  HardDrive,
  Image as ImageIcon,
  Clock,
  Settings,
  Search,
  Check,
  Download,
} from "lucide-react";
import { cn } from "@/lib/utils";

const categories = ["All", "System", "Utilities", "Accessories"];

const apps = [
  { name: "Files", desc: "Browse and manage your folders", icon: Folder, cat: "System", installed: true },
  { name: "Terminal", desc: "Command-line access to AOS", icon: TerminalSquare, cat: "System", installed: true },
  { name: "Settings", desc: "Configure your system", icon: Settings, cat: "System", installed: true },
  { name: "Text Editor", desc: "Lightweight note taking", icon: FileText, cat: "Accessories", installed: true },
  { name: "Calculator", desc: "Basic and scientific math", icon: Calculator, cat: "Accessories", installed: true },
  { name: "System Monitor", desc: "Live resource usage", icon: Activity, cat: "System", installed: true },
  { name: "Calendar", desc: "Plan your days and events", icon: CalendarDays, cat: "Utilities", installed: true },
  { name: "Disks", desc: "Inspect drives and storage", icon: HardDrive, cat: "Utilities", installed: true },
  { name: "Photos", desc: "View your image library", icon: ImageIcon, cat: "Accessories", installed: true },
  { name: "Clocks", desc: "World clock and timers", icon: Clock, cat: "Utilities", installed: false },
];

export function SoftwareApp() {
  const [cat, setCat] = useState("All");
  const [query, setQuery] = useState("");

  const filtered = apps.filter(
    (a) =>
      (cat === "All" || a.cat === cat) &&
      a.name.toLowerCase().includes(query.toLowerCase()),
  );

  return (
    <div className="flex h-full flex-col bg-card">
      <div className="border-b border-border/60 p-4">
        <div className="relative mx-auto max-w-md">
          <Search className="pointer-events-none absolute left-3 top-1/2 size-4 -translate-y-1/2 text-muted-foreground" />
          <input
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Search AOS Software"
            className="w-full rounded-xl border border-border/60 bg-secondary/50 py-2.5 pl-10 pr-3 text-sm outline-none focus:border-primary"
          />
        </div>
        <div className="mt-3 flex justify-center gap-2">
          {categories.map((c) => (
            <button
              key={c}
              onClick={() => setCat(c)}
              className={cn(
                "rounded-full px-3.5 py-1.5 text-xs font-medium transition-colors",
                cat === c ? "bg-primary text-primary-foreground" : "bg-secondary hover:bg-secondary/70",
              )}
            >
              {c}
            </button>
          ))}
        </div>
      </div>

      <div className="grid flex-1 grid-cols-2 content-start gap-3 overflow-y-auto p-4 aos-scroll">
        {filtered.map((a) => (
          <div
            key={a.name}
            className="flex items-center gap-3 rounded-xl border border-border/60 bg-secondary/40 p-3"
          >
            <div className="grid size-12 shrink-0 place-items-center rounded-xl bg-primary/15 text-primary">
              <a.icon className="size-6" />
            </div>
            <div className="min-w-0 flex-1">
              <p className="text-sm font-medium">{a.name}</p>
              <p className="truncate text-xs text-muted-foreground">{a.desc}</p>
            </div>
            {a.installed ? (
              <span className="flex items-center gap-1 rounded-lg bg-primary/10 px-2.5 py-1.5 text-xs font-medium text-primary">
                <Check className="size-3.5" />
                Installed
              </span>
            ) : (
              <button className="flex items-center gap-1 rounded-lg bg-primary px-2.5 py-1.5 text-xs font-medium text-primary-foreground hover:opacity-90">
                <Download className="size-3.5" />
                Get
              </button>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
