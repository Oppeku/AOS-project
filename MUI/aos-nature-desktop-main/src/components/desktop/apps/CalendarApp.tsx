import { useState } from "react";
import { ChevronLeft, ChevronRight } from "lucide-react";
import { cn } from "@/lib/utils";

const WEEKDAYS = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
const MONTHS = [
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December",
];

const events: Record<string, { time: string; title: string; color: string }[]> = {
  "10": [{ time: "09:00", title: "Team sync", color: "bg-primary" }],
  "15": [
    { time: "11:30", title: "Trail hike", color: "bg-accent" },
    { time: "18:00", title: "Dinner", color: "bg-[oklch(0.6_0.15_30)]" },
  ],
  "22": [{ time: "14:00", title: "AOS release review", color: "bg-[oklch(0.55_0.1_240)]" }],
};

export function CalendarApp() {
  const today = new Date();
  const [view, setView] = useState({ year: today.getFullYear(), month: today.getMonth() });
  const [selected, setSelected] = useState(today.getDate());

  const firstDay = new Date(view.year, view.month, 1).getDay();
  const daysInMonth = new Date(view.year, view.month + 1, 0).getDate();
  const cells: (number | null)[] = [
    ...Array(firstDay).fill(null),
    ...Array.from({ length: daysInMonth }, (_, i) => i + 1),
  ];

  const isCurrentMonth =
    view.year === today.getFullYear() && view.month === today.getMonth();

  function shift(delta: number) {
    setView((v) => {
      const m = v.month + delta;
      return { year: v.year + Math.floor(m / 12), month: ((m % 12) + 12) % 12 };
    });
  }

  const dayEvents = events[String(selected)] ?? [];

  return (
    <div className="flex h-full">
      <div className="flex flex-1 flex-col p-5">
        <div className="mb-4 flex items-center justify-between">
          <h2 className="text-xl font-semibold">
            {MONTHS[view.month]} {view.year}
          </h2>
          <div className="flex gap-1">
            <button onClick={() => shift(-1)} className="rounded-lg p-2 hover:bg-secondary">
              <ChevronLeft className="size-4" />
            </button>
            <button
              onClick={() => setView({ year: today.getFullYear(), month: today.getMonth() })}
              className="rounded-lg px-3 text-sm hover:bg-secondary"
            >
              Today
            </button>
            <button onClick={() => shift(1)} className="rounded-lg p-2 hover:bg-secondary">
              <ChevronRight className="size-4" />
            </button>
          </div>
        </div>

        <div className="grid grid-cols-7 gap-1 text-center text-xs font-medium text-muted-foreground">
          {WEEKDAYS.map((d) => (
            <div key={d} className="py-1">
              {d}
            </div>
          ))}
        </div>
        <div className="mt-1 grid flex-1 grid-cols-7 gap-1">
          {cells.map((day, i) => {
            const isToday = isCurrentMonth && day === today.getDate();
            const hasEvent = day && events[String(day)];
            return (
              <button
                key={i}
                disabled={!day}
                onClick={() => day && setSelected(day)}
                className={cn(
                  "relative flex flex-col items-center rounded-lg p-1.5 text-sm transition-colors",
                  !day && "pointer-events-none",
                  day && "hover:bg-secondary",
                  selected === day && "bg-secondary",
                  isToday && "bg-primary text-primary-foreground hover:bg-primary",
                )}
              >
                {day}
                {hasEvent && (
                  <span
                    className={cn(
                      "mt-0.5 size-1 rounded-full",
                      isToday ? "bg-primary-foreground" : "bg-accent",
                    )}
                  />
                )}
              </button>
            );
          })}
        </div>
      </div>

      <aside className="w-64 shrink-0 border-l border-border/60 bg-secondary/40 p-4">
        <p className="text-sm font-medium">
          {MONTHS[view.month]} {selected}
        </p>
        <p className="mb-4 text-xs text-muted-foreground">
          {dayEvents.length} event{dayEvents.length !== 1 && "s"}
        </p>
        <div className="space-y-2">
          {dayEvents.length === 0 && (
            <p className="text-sm text-muted-foreground">No events scheduled.</p>
          )}
          {dayEvents.map((e) => (
            <div
              key={e.title}
              className="flex items-start gap-2 rounded-lg border border-border/60 bg-card/60 p-3"
            >
              <span className={cn("mt-1 h-8 w-1 rounded-full", e.color)} />
              <div>
                <p className="text-sm font-medium">{e.title}</p>
                <p className="text-xs text-muted-foreground">{e.time}</p>
              </div>
            </div>
          ))}
        </div>
      </aside>
    </div>
  );
}
