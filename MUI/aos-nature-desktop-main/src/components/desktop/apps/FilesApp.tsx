import { useState } from "react";
import {
  Folder,
  FileText,
  Image as ImageIcon,
  Music,
  Download,
  Home,
  Star,
  Trash2,
  HardDrive,
  ChevronRight,
  LayoutGrid,
  List,
} from "lucide-react";
import { cn } from "@/lib/utils";

const places = [
  { id: "home", name: "Home", icon: Home },
  { id: "desktop", name: "Desktop", icon: LayoutGrid },
  { id: "documents", name: "Documents", icon: FileText },
  { id: "downloads", name: "Downloads", icon: Download },
  { id: "pictures", name: "Pictures", icon: ImageIcon },
  { id: "music", name: "Music", icon: Music },
  { id: "starred", name: "Starred", icon: Star },
  { id: "trash", name: "Trash", icon: Trash2 },
];

type Item = { name: string; type: "folder" | "file"; size?: string; kind?: string };

const contents: Record<string, Item[]> = {
  home: [
    { name: "Documents", type: "folder" },
    { name: "Downloads", type: "folder" },
    { name: "Pictures", type: "folder" },
    { name: "Music", type: "folder" },
    { name: "Projects", type: "folder" },
    { name: "welcome.txt", type: "file", size: "2 KB", kind: "text" },
    { name: "aos-notes.md", type: "file", size: "8 KB", kind: "text" },
  ],
  documents: [
    { name: "Reports", type: "folder" },
    { name: "resume.pdf", type: "file", size: "120 KB", kind: "doc" },
    { name: "budget.csv", type: "file", size: "14 KB", kind: "doc" },
  ],
  pictures: [
    { name: "Wallpapers", type: "folder" },
    { name: "mountain-lake.jpg", type: "file", size: "3.2 MB", kind: "image" },
    { name: "green-valley.jpg", type: "file", size: "2.8 MB", kind: "image" },
  ],
  downloads: [{ name: "aos-1.0-release-notes.txt", type: "file", size: "6 KB", kind: "text" }],
};

function iconFor(item: Item) {
  if (item.type === "folder") return Folder;
  if (item.kind === "image") return ImageIcon;
  return FileText;
}

export function FilesApp() {
  const [place, setPlace] = useState("home");
  const [view, setView] = useState<"grid" | "list">("grid");
  const items = contents[place] ?? [];
  const placeName = places.find((p) => p.id === place)?.name ?? "Home";

  return (
    <div className="flex h-full">
      <aside className="flex w-52 shrink-0 flex-col gap-0.5 overflow-y-auto border-r border-border/60 bg-secondary/40 p-2 aos-scroll">
        {places.map((p) => (
          <button
            key={p.id}
            onClick={() => setPlace(p.id)}
            className={cn(
              "flex items-center gap-3 rounded-lg px-3 py-2 text-left text-sm transition-colors",
              place === p.id ? "bg-primary text-primary-foreground" : "hover:bg-background/70",
            )}
          >
            <p.icon className="size-4" />
            {p.name}
          </button>
        ))}
        <div className="mt-3 border-t border-border/60 pt-2">
          <div className="flex items-center gap-3 rounded-lg px-3 py-2 text-sm text-muted-foreground">
            <HardDrive className="size-4" />
            AOS Disk
          </div>
        </div>
      </aside>

      <div className="flex flex-1 flex-col overflow-hidden">
        <div className="flex items-center justify-between border-b border-border/60 px-4 py-2.5">
          <div className="flex items-center gap-1 text-sm text-muted-foreground">
            <Home className="size-4" />
            <ChevronRight className="size-3.5" />
            <span className="font-medium text-foreground">{placeName}</span>
          </div>
          <div className="flex gap-1 rounded-lg border border-border/60 p-0.5">
            <button
              onClick={() => setView("grid")}
              className={cn("rounded p-1.5", view === "grid" && "bg-secondary")}
            >
              <LayoutGrid className="size-4" />
            </button>
            <button
              onClick={() => setView("list")}
              className={cn("rounded p-1.5", view === "list" && "bg-secondary")}
            >
              <List className="size-4" />
            </button>
          </div>
        </div>

        <div className="flex-1 overflow-y-auto p-4 aos-scroll">
          {items.length === 0 ? (
            <div className="grid h-full place-items-center text-sm text-muted-foreground">
              This folder is empty
            </div>
          ) : view === "grid" ? (
            <div className="grid grid-cols-[repeat(auto-fill,minmax(96px,1fr))] gap-2">
              {items.map((item) => {
                const Icon = iconFor(item);
                return (
                  <button
                    key={item.name}
                    className="flex flex-col items-center gap-2 rounded-xl p-3 text-center transition-colors hover:bg-secondary/70"
                  >
                    <Icon
                      className={cn(
                        "size-10",
                        item.type === "folder" ? "text-accent" : "text-muted-foreground",
                      )}
                    />
                    <span className="line-clamp-2 text-xs">{item.name}</span>
                  </button>
                );
              })}
            </div>
          ) : (
            <div className="overflow-hidden rounded-xl border border-border/60">
              {items.map((item, i) => {
                const Icon = iconFor(item);
                return (
                  <div
                    key={item.name}
                    className={cn(
                      "flex items-center gap-3 px-4 py-2.5 text-sm hover:bg-secondary/60",
                      i !== items.length - 1 && "border-b border-border/50",
                    )}
                  >
                    <Icon
                      className={cn(
                        "size-4",
                        item.type === "folder" ? "text-accent" : "text-muted-foreground",
                      )}
                    />
                    <span className="flex-1">{item.name}</span>
                    <span className="text-xs text-muted-foreground">
                      {item.type === "folder" ? "Folder" : item.size}
                    </span>
                  </div>
                );
              })}
            </div>
          )}
        </div>
        <div className="border-t border-border/60 px-4 py-1.5 text-xs text-muted-foreground">
          {items.length} items
        </div>
      </div>
    </div>
  );
}
