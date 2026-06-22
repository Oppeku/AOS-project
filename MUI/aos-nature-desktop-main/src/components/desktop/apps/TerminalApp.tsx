import { useEffect, useRef, useState } from "react";

interface Line {
  type: "input" | "output";
  text: string;
}

const FS = ["Documents", "Downloads", "Pictures", "Music", "Projects", "welcome.txt"];

const banner = [
  "AOS Terminal 1.0 — type 'help' for commands",
];

export function TerminalApp() {
  const [lines, setLines] = useState<Line[]>(banner.map((t) => ({ type: "output", text: t })));
  const [value, setValue] = useState("");
  const scrollRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  useEffect(() => {
    scrollRef.current?.scrollTo({ top: scrollRef.current.scrollHeight });
  }, [lines]);

  function run(cmd: string) {
    const trimmed = cmd.trim();
    const out: string[] = [];
    const [name, ...args] = trimmed.split(/\s+/);
    switch (name) {
      case "":
        break;
      case "help":
        out.push(
          "Available commands:",
          "  help        show this message",
          "  ls          list files",
          "  whoami      print current user",
          "  uname       system information",
          "  date        current date and time",
          "  echo [msg]  print a message",
          "  neofetch    system summary",
          "  clear       clear the screen",
        );
        break;
      case "ls":
        out.push(FS.join("   "));
        break;
      case "whoami":
        out.push("explorer");
        break;
      case "uname":
        out.push("AOS 1.0 forest x86_64 GNU/AOS");
        break;
      case "date":
        out.push(new Date().toString());
        break;
      case "echo":
        out.push(args.join(" "));
        break;
      case "pwd":
        out.push("/home/explorer");
        break;
      case "neofetch":
        out.push(
          "        ▲ ▲ ▲      explorer@aos",
          "       ▲ AOS ▲     ------------",
          "      ▲ ▲ ▲ ▲ ▲    OS: AOS 1.0 Forest",
          "                   Shell: aosh 1.0",
          "                   DE: Aurora",
          "                   Theme: Forest",
          "                   Uptime: 2 hours",
        );
        break;
      case "clear":
        setLines([]);
        return;
      default:
        out.push(`aosh: command not found: ${name}`);
    }

    setLines((prev) => [
      ...prev,
      { type: "input", text: trimmed },
      ...out.map((text) => ({ type: "output" as const, text })),
    ]);
  }

  return (
    <div
      className="flex h-full flex-col bg-[oklch(0.18_0.02_152)] p-3 font-mono text-sm text-[oklch(0.92_0.05_150)]"
      onClick={() => inputRef.current?.focus()}
    >
      <div ref={scrollRef} className="flex-1 overflow-y-auto aos-scroll">
        {lines.map((l, i) => (
          <div key={i} className="whitespace-pre-wrap break-words leading-relaxed">
            {l.type === "input" ? (
              <span>
                <span className="text-[oklch(0.7_0.13_152)]">explorer@aos</span>
                <span className="text-muted-foreground">:~$ </span>
                {l.text}
              </span>
            ) : (
              l.text
            )}
          </div>
        ))}
        <div className="flex">
          <span className="text-[oklch(0.7_0.13_152)]">explorer@aos</span>
          <span className="text-[oklch(0.6_0.02_150)]">:~$&nbsp;</span>
          <input
            ref={inputRef}
            autoFocus
            value={value}
            onChange={(e) => setValue(e.target.value)}
            onKeyDown={(e) => {
              if (e.key === "Enter") {
                run(value);
                setValue("");
              }
            }}
            className="flex-1 bg-transparent caret-[oklch(0.7_0.13_152)] outline-none"
            spellCheck={false}
          />
        </div>
      </div>
    </div>
  );
}
