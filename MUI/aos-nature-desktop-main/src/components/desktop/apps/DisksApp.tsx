import { HardDrive, Database } from "lucide-react";

const segments = [
  { label: "System", value: 42, color: "oklch(0.55 0.13 152)" },
  { label: "Applications", value: 28, color: "oklch(0.6 0.15 30)" },
  { label: "Documents", value: 14, color: "oklch(0.6 0.1 220)" },
  { label: "Media", value: 56, color: "oklch(0.65 0.14 330)" },
  { label: "Free", value: 172, color: "oklch(0.88 0.015 150)" },
];

const total = segments.reduce((s, x) => s + x.value, 0);

const volumes = [
  { name: "AOS Root", mount: "/", size: "512 GB", used: "340 GB", type: "SSD", health: "Good" },
  { name: "Forest Backup", mount: "/mnt/backup", size: "1 TB", used: "210 GB", type: "HDD", health: "Good" },
  { name: "USB Drive", mount: "/media/usb", size: "32 GB", used: "8 GB", type: "Removable", health: "Good" },
];

export function DisksApp() {
  return (
    <div className="flex h-full flex-col gap-5 overflow-y-auto bg-card p-5 aos-scroll">
      <div className="rounded-xl border border-border/60 bg-secondary/40 p-5">
        <div className="mb-3 flex items-center gap-2 text-sm font-medium">
          <HardDrive className="size-4 text-primary" />
          AOS Root — 512 GB SSD
        </div>
        <div className="flex h-4 overflow-hidden rounded-full">
          {segments.map((s) => (
            <div
              key={s.label}
              style={{ width: `${(s.value / total) * 100}%`, backgroundColor: s.color }}
              title={`${s.label}: ${s.value} GB`}
            />
          ))}
        </div>
        <div className="mt-4 flex flex-wrap gap-x-5 gap-y-2">
          {segments.map((s) => (
            <div key={s.label} className="flex items-center gap-2 text-xs">
              <span className="size-3 rounded-sm" style={{ backgroundColor: s.color }} />
              <span>{s.label}</span>
              <span className="text-muted-foreground">{s.value} GB</span>
            </div>
          ))}
        </div>
      </div>

      <div>
        <div className="mb-2 flex items-center gap-2 text-sm font-medium">
          <Database className="size-4 text-primary" />
          Volumes
        </div>
        <div className="overflow-hidden rounded-xl border border-border/60">
          {volumes.map((v, i) => (
            <div
              key={v.name}
              className={`flex items-center gap-4 p-4 ${
                i !== volumes.length - 1 ? "border-b border-border/50" : ""
              }`}
            >
              <div className="grid size-10 place-items-center rounded-lg bg-primary/15 text-primary">
                <HardDrive className="size-5" />
              </div>
              <div className="flex-1">
                <p className="text-sm font-medium">{v.name}</p>
                <p className="text-xs text-muted-foreground">
                  {v.mount} · {v.type}
                </p>
              </div>
              <div className="text-right">
                <p className="text-sm tabular-nums">
                  {v.used} <span className="text-muted-foreground">/ {v.size}</span>
                </p>
                <p className="text-xs text-primary">{v.health}</p>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
}
