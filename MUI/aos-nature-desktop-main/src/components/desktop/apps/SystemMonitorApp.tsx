import { useEffect, useState } from "react";
import { Cpu, MemoryStick, HardDrive, Activity, X } from "lucide-react";
import { cn } from "@/lib/utils";

interface Proc {
  name: string;
  user: string;
  cpu: number;
  mem: number;
}

const baseProcs: Proc[] = [
  { name: "aurora-shell", user: "explorer", cpu: 4.2, mem: 182 },
  { name: "aos-compositor", user: "root", cpu: 3.1, mem: 240 },
  { name: "files", user: "explorer", cpu: 1.4, mem: 96 },
  { name: "terminal", user: "explorer", cpu: 0.8, mem: 42 },
  { name: "settings", user: "explorer", cpu: 0.6, mem: 70 },
  { name: "system-monitor", user: "explorer", cpu: 2.0, mem: 58 },
  { name: "network-manager", user: "root", cpu: 0.3, mem: 34 },
  { name: "audio-server", user: "root", cpu: 0.5, mem: 28 },
];

function useTick(interval = 1500) {
  const [, setT] = useState(0);
  useEffect(() => {
    const id = setInterval(() => setT((t) => t + 1), interval);
    return () => clearInterval(id);
  }, [interval]);
}

function jitter(base: number, amp: number) {
  return Math.max(0, Math.min(100, base + (Math.random() - 0.5) * amp));
}

export function SystemMonitorApp() {
  useTick();
  const cpu = jitter(28, 18);
  const mem = jitter(46, 8);
  const disk = 62;

  const procs = baseProcs
    .map((p) => ({ ...p, cpu: Math.max(0, p.cpu + (Math.random() - 0.5) * 1.5) }))
    .sort((a, b) => b.cpu - a.cpu);

  return (
    <div className="flex h-full flex-col bg-card">
      <div className="grid grid-cols-3 gap-3 border-b border-border/60 p-4">
        <Gauge label="CPU" value={cpu} unit="%" icon={Cpu} color="oklch(0.6 0.18 25)" />
        <Gauge label="Memory" value={mem} unit="%" icon={MemoryStick} color="oklch(0.55 0.13 152)" sub="7.4 / 16 GB" />
        <Gauge label="Disk" value={disk} unit="%" icon={HardDrive} color="oklch(0.6 0.1 220)" sub="310 / 512 GB" />
      </div>

      <div className="flex items-center gap-2 px-4 py-2 text-sm font-medium">
        <Activity className="size-4 text-primary" />
        Processes
      </div>

      <div className="flex-1 overflow-y-auto px-2 pb-2 aos-scroll">
        <div className="grid grid-cols-[1fr_auto_auto_auto] gap-x-4 px-2 pb-1 text-xs font-medium text-muted-foreground">
          <span>Name</span>
          <span className="text-right">User</span>
          <span className="text-right">CPU</span>
          <span className="text-right">Memory</span>
        </div>
        {procs.map((p) => (
          <div
            key={p.name}
            className="group grid grid-cols-[1fr_auto_auto_auto] items-center gap-x-4 rounded-lg px-2 py-2 text-sm hover:bg-secondary/60"
          >
            <span className="flex items-center gap-2 font-mono text-xs">
              {p.name}
              <X className="size-3.5 text-muted-foreground opacity-0 transition-opacity group-hover:opacity-100" />
            </span>
            <span className="text-right text-xs text-muted-foreground">{p.user}</span>
            <span
              className={cn(
                "text-right tabular-nums text-xs",
                p.cpu > 3 ? "text-accent" : "text-muted-foreground",
              )}
            >
              {p.cpu.toFixed(1)}%
            </span>
            <span className="text-right tabular-nums text-xs text-muted-foreground">
              {p.mem} MB
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}

function Gauge({
  label,
  value,
  unit,
  icon: Icon,
  color,
  sub,
}: {
  label: string;
  value: number;
  unit: string;
  icon: React.ComponentType<{ className?: string }>;
  color: string;
  sub?: string;
}) {
  return (
    <div className="rounded-xl border border-border/60 bg-secondary/40 p-3">
      <div className="mb-2 flex items-center gap-2 text-xs text-muted-foreground">
        <Icon className="size-3.5" />
        {label}
      </div>
      <div className="mb-2 text-2xl font-semibold tabular-nums">
        {value.toFixed(0)}
        <span className="text-base font-normal text-muted-foreground">{unit}</span>
      </div>
      <div className="h-1.5 overflow-hidden rounded-full bg-muted">
        <div
          className="h-full rounded-full transition-all duration-700"
          style={{ width: `${value}%`, backgroundColor: color }}
        />
      </div>
      {sub && <p className="mt-1.5 text-[11px] text-muted-foreground">{sub}</p>}
    </div>
  );
}
