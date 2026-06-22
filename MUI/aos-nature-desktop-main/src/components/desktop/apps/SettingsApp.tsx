import { useState } from "react";
import {
  Wifi,
  Bluetooth,
  Monitor,
  Palette,
  Bell,
  Volume2,
  BatteryCharging,
  Lock,
  Globe,
  User,
  Search,
  ChevronRight,
} from "lucide-react";
import { cn } from "@/lib/utils";
import { Switch } from "@/components/ui/switch";
import { Slider } from "@/components/ui/slider";

const sections = [
  { id: "wifi", name: "Wi-Fi", icon: Wifi, sub: "Mountain-Net" },
  { id: "bluetooth", name: "Bluetooth", icon: Bluetooth, sub: "On" },
  { id: "display", name: "Displays", icon: Monitor, sub: "1920 × 1080" },
  { id: "appearance", name: "Appearance", icon: Palette, sub: "Forest" },
  { id: "sound", name: "Sound", icon: Volume2, sub: "65%" },
  { id: "notifications", name: "Notifications", icon: Bell, sub: "Allowed" },
  { id: "power", name: "Power", icon: BatteryCharging, sub: "Balanced" },
  { id: "privacy", name: "Privacy & Security", icon: Lock, sub: "Protected" },
  { id: "network", name: "Network", icon: Globe, sub: "Connected" },
  { id: "users", name: "Users", icon: User, sub: "explorer" },
] as const;

type SectionId = (typeof sections)[number]["id"];

export function SettingsApp() {
  const [active, setActive] = useState<SectionId>("appearance");

  return (
    <div className="flex h-full">
      <aside className="flex w-60 shrink-0 flex-col gap-1 overflow-y-auto border-r border-border/60 bg-secondary/40 p-3 aos-scroll">
        <div className="relative mb-2">
          <Search className="pointer-events-none absolute left-2.5 top-1/2 size-4 -translate-y-1/2 text-muted-foreground" />
          <input
            placeholder="Search"
            className="w-full rounded-lg border border-border/60 bg-background/60 py-2 pl-8 pr-3 text-sm outline-none focus:border-primary"
          />
        </div>
        {sections.map((s) => (
          <button
            key={s.id}
            onClick={() => setActive(s.id)}
            className={cn(
              "flex items-center gap-3 rounded-lg px-3 py-2 text-left text-sm transition-colors",
              active === s.id ? "bg-primary text-primary-foreground" : "hover:bg-background/70",
            )}
          >
            <s.icon className="size-4 shrink-0" />
            <span className="flex-1 truncate">{s.name}</span>
            <span
              className={cn(
                "text-xs",
                active === s.id ? "text-primary-foreground/80" : "text-muted-foreground",
              )}
            >
              {s.sub}
            </span>
          </button>
        ))}
      </aside>

      <div className="flex-1 overflow-y-auto p-6 aos-scroll">
        <SettingsContent section={active} />
      </div>
    </div>
  );
}

function Row({
  title,
  desc,
  children,
}: {
  title: string;
  desc?: string;
  children: React.ReactNode;
}) {
  return (
    <div className="flex items-center justify-between gap-4 border-b border-border/50 py-4 last:border-0">
      <div>
        <p className="text-sm font-medium">{title}</p>
        {desc && <p className="text-xs text-muted-foreground">{desc}</p>}
      </div>
      {children}
    </div>
  );
}

function Card({ children }: { children: React.ReactNode }) {
  return <div className="rounded-xl border border-border/60 bg-card/60 px-4">{children}</div>;
}

function SettingsContent({ section }: { section: SectionId }) {
  const meta = sections.find((s) => s.id === section)!;
  const [volume, setVolume] = useState([65]);
  const [brightness, setBrightness] = useState([80]);

  return (
    <div className="mx-auto max-w-xl space-y-6 animate-fade-in">
      <header className="flex items-center gap-3">
        <div className="grid size-11 place-items-center rounded-xl bg-primary/15 text-primary">
          <meta.icon className="size-5" />
        </div>
        <div>
          <h2 className="text-lg font-semibold">{meta.name}</h2>
          <p className="text-xs text-muted-foreground">AOS System Settings</p>
        </div>
      </header>

      {section === "appearance" && (
        <>
          <Card>
            <Row title="Theme" desc="Switch between light and dark mode">
              <div className="flex gap-2">
                {["Light", "Dark"].map((t, i) => (
                  <span
                    key={t}
                    className={cn(
                      "rounded-lg border px-3 py-1.5 text-xs",
                      i === 0
                        ? "border-primary bg-primary text-primary-foreground"
                        : "border-border",
                    )}
                  >
                    {t}
                  </span>
                ))}
              </div>
            </Row>
            <Row title="Accent color">
              <div className="flex gap-2">
                {[
                  "bg-[oklch(0.55_0.13_152)]",
                  "bg-[oklch(0.6_0.15_30)]",
                  "bg-[oklch(0.55_0.1_240)]",
                  "bg-[oklch(0.6_0.18_330)]",
                ].map((c, i) => (
                  <span
                    key={c}
                    className={cn(
                      "size-6 rounded-full ring-offset-2 ring-offset-card",
                      c,
                      i === 0 && "ring-2 ring-ring",
                    )}
                  />
                ))}
              </div>
            </Row>
          </Card>
          <Card>
            <Row title="Animations" desc="Window and menu motion">
              <Switch defaultChecked />
            </Row>
            <Row title="Transparency" desc="Glass effect on panels">
              <Switch defaultChecked />
            </Row>
          </Card>
        </>
      )}

      {section === "sound" && (
        <Card>
          <Row title="Output volume">
            <Slider value={volume} onValueChange={setVolume} max={100} className="w-40" />
          </Row>
          <Row title="Input volume">
            <Slider defaultValue={[40]} max={100} className="w-40" />
          </Row>
          <Row title="System sounds">
            <Switch defaultChecked />
          </Row>
        </Card>
      )}

      {section === "display" && (
        <Card>
          <Row title="Resolution" desc="Built-in display">
            <span className="text-sm text-muted-foreground">1920 × 1080</span>
          </Row>
          <Row title="Brightness">
            <Slider value={brightness} onValueChange={setBrightness} max={100} className="w-40" />
          </Row>
          <Row title="Night Light" desc="Warmer colors after sunset">
            <Switch defaultChecked />
          </Row>
        </Card>
      )}

      {section === "wifi" && (
        <Card>
          <Row title="Wi-Fi">
            <Switch defaultChecked />
          </Row>
          {["Mountain-Net", "Pine-Lodge 5G", "ValleyGuest"].map((n, i) => (
            <Row key={n} title={n} desc={i === 0 ? "Connected, secured" : "Secured"}>
              {i === 0 ? (
                <Wifi className="size-4 text-primary" />
              ) : (
                <ChevronRight className="size-4 text-muted-foreground" />
              )}
            </Row>
          ))}
        </Card>
      )}

      {!["appearance", "sound", "display", "wifi"].includes(section) && (
        <Card>
          <Row title={`${meta.name} enabled`} desc="Manage this system feature">
            <Switch defaultChecked />
          </Row>
          <Row title="Status" desc="Current state">
            <span className="text-sm text-muted-foreground">{meta.sub}</span>
          </Row>
          <Row title="Automatic updates">
            <Switch defaultChecked />
          </Row>
        </Card>
      )}
    </div>
  );
}
