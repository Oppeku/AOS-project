import { useEffect, useState } from "react";

const zones = [
  { city: "Local", tz: undefined as string | undefined },
  { city: "London", tz: "Europe/London" },
  { city: "New York", tz: "America/New_York" },
  { city: "Tokyo", tz: "Asia/Tokyo" },
  { city: "Sydney", tz: "Australia/Sydney" },
];

function useNow() {
  const [now, setNow] = useState(new Date());
  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(id);
  }, []);
  return now;
}

export function ClocksApp() {
  const now = useNow();

  return (
    <div className="flex h-full flex-col bg-card">
      <div className="flex flex-col items-center justify-center border-b border-border/60 py-8">
        <p className="font-mono text-6xl font-light tabular-nums">
          {now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })}
        </p>
        <p className="mt-2 text-sm text-muted-foreground">
          {now.toLocaleDateString([], {
            weekday: "long",
            year: "numeric",
            month: "long",
            day: "numeric",
          })}
        </p>
      </div>

      <div className="flex-1 overflow-y-auto p-4 aos-scroll">
        <p className="mb-2 px-1 text-xs font-medium uppercase tracking-wide text-muted-foreground">
          World Clock
        </p>
        <div className="overflow-hidden rounded-xl border border-border/60">
          {zones.map((z, i) => {
            const time = z.tz
              ? now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", timeZone: z.tz })
              : now.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
            return (
              <div
                key={z.city}
                className={`flex items-center justify-between px-4 py-3 ${
                  i !== zones.length - 1 ? "border-b border-border/50" : ""
                }`}
              >
                <span className="text-sm font-medium">{z.city}</span>
                <span className="font-mono text-lg tabular-nums">{time}</span>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
