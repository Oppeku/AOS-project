import { useMemo, useState } from "react";
import { FileText, Save, Type } from "lucide-react";

const initial = `Welcome to AOS Text Editor

A clean place to write, jot notes, and draft ideas.

• Everything here runs in your browser
• This is a UI demo of the AOS desktop
• Try the Files, Terminal and Settings apps too

Happy exploring 🌲`;

export function TextEditorApp() {
  const [text, setText] = useState(initial);
  const [saved, setSaved] = useState(true);

  const stats = useMemo(() => {
    const words = text.trim() ? text.trim().split(/\s+/).length : 0;
    const lines = text.split("\n").length;
    return { words, chars: text.length, lines };
  }, [text]);

  return (
    <div className="flex h-full flex-col bg-card">
      <div className="flex items-center justify-between border-b border-border/60 px-3 py-2">
        <div className="flex items-center gap-2 text-sm">
          <FileText className="size-4 text-primary" />
          <span className="font-medium">welcome.txt</span>
          {!saved && <span className="size-1.5 rounded-full bg-accent" />}
        </div>
        <button
          onClick={() => setSaved(true)}
          className="flex items-center gap-1.5 rounded-lg bg-primary px-3 py-1.5 text-xs font-medium text-primary-foreground transition-opacity hover:opacity-90"
        >
          <Save className="size-3.5" />
          Save
        </button>
      </div>
      <textarea
        value={text}
        onChange={(e) => {
          setText(e.target.value);
          setSaved(false);
        }}
        spellCheck={false}
        className="flex-1 resize-none bg-transparent p-5 font-mono text-sm leading-relaxed outline-none aos-scroll"
      />
      <div className="flex items-center gap-4 border-t border-border/60 px-4 py-1.5 text-xs text-muted-foreground">
        <span className="flex items-center gap-1">
          <Type className="size-3.5" />
          {stats.words} words
        </span>
        <span>{stats.chars} characters</span>
        <span>{stats.lines} lines</span>
        <span className="ml-auto">{saved ? "Saved" : "Unsaved changes"}</span>
      </div>
    </div>
  );
}
