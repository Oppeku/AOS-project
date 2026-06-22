import { Cpu, MemoryStick, HardDrive, Monitor } from "lucide-react";

const specs = [
  { label: "OS Name", value: "AOS 1.0 “Forest”" },
  { label: "Desktop", value: "Aurora Shell" },
  { label: "Kernel", value: "aos-kernel 6.1" },
  { label: "Processor", value: "AOS Virtual CPU @ 3.2 GHz × 8" },
  { label: "Memory", value: "16 GB" },
  { label: "Graphics", value: "AOS Compositor (software)" },
  { label: "Disk", value: "512 GB SSD" },
  { label: "Hostname", value: "aos-desktop" },
];

export function AboutApp() {
  return (
    <div className="flex h-full flex-col items-center overflow-y-auto bg-card p-6 text-center aos-scroll">
      <div className="relative mb-4 grid size-24 place-items-center rounded-3xl bg-gradient-to-br from-[oklch(0.55_0.13_152)] to-[oklch(0.65_0.12_75)] text-primary-foreground shadow-lg">
        <span className="text-4xl font-bold tracking-tight">A</span>
      </div>
      <h1 className="text-2xl font-bold">AOS</h1>
      <p className="text-sm text-muted-foreground">Version 1.0 — “Forest”</p>
      <p className="mt-1 max-w-xs text-xs text-muted-foreground">
        A calm, nature-inspired desktop environment. This is a UI concept demo.
      </p>

      <div className="mt-6 grid w-full max-w-sm grid-cols-2 gap-3">
        <Stat icon={Cpu} label="8 Cores" />
        <Stat icon={MemoryStick} label="16 GB RAM" />
        <Stat icon={HardDrive} label="512 GB SSD" />
        <Stat icon={Monitor} label="1920×1080" />
      </div>

      <div className="mt-6 w-full max-w-sm overflow-hidden rounded-xl border border-border/60 text-left">
        {specs.map((s, i) => (
          <div
            key={s.label}
            className={`flex justify-between gap-4 px-4 py-2.5 text-sm ${
              i !== specs.length - 1 ? "border-b border-border/50" : ""
            }`}
          >
            <span className="text-muted-foreground">{s.label}</span>
            <span className="text-right font-medium">{s.value}</span>
          </div>
        ))}
      </div>

      <p className="mt-6 text-xs text-muted-foreground">© 2026 AOS Project · Made with 🌲</p>
    </div>
  );
}

function Stat({
  icon: Icon,
  label,
}: {
  icon: React.ComponentType<{ className?: string }>;
  label: string;
}) {
  return (
    <div className="flex flex-col items-center gap-1.5 rounded-xl border border-border/60 bg-secondary/40 py-3">
      <Icon className="size-5 text-primary" />
      <span className="text-xs font-medium">{label}</span>
    </div>
  );
}
