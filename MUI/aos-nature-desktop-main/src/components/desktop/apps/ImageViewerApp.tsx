import { useState } from "react";
import { ChevronLeft, ChevronRight, ZoomIn } from "lucide-react";
import { cn } from "@/lib/utils";
import wall1 from "@/assets/aos-wallpaper.jpg";
import wall2 from "@/assets/aos-wallpaper-2.jpg";

const photos = [
  { src: wall1, title: "Mountain Lake", meta: "3.2 MB · 1920×1080" },
  { src: wall2, title: "Green Valley", meta: "2.8 MB · 1920×1080" },
];

export function ImageViewerApp() {
  const [index, setIndex] = useState(0);
  const photo = photos[index];

  return (
    <div className="flex h-full flex-col bg-[oklch(0.2_0.02_152)]">
      <div className="relative flex flex-1 items-center justify-center overflow-hidden">
        <img
          src={photo.src}
          alt={photo.title}
          width={1920}
          height={1080}
          loading="lazy"
          className="max-h-full max-w-full object-contain"
        />
        <button
          onClick={() => setIndex((i) => (i - 1 + photos.length) % photos.length)}
          className="absolute left-3 grid size-9 place-items-center rounded-full bg-black/40 text-white backdrop-blur transition-colors hover:bg-black/60"
        >
          <ChevronLeft className="size-5" />
        </button>
        <button
          onClick={() => setIndex((i) => (i + 1) % photos.length)}
          className="absolute right-3 grid size-9 place-items-center rounded-full bg-black/40 text-white backdrop-blur transition-colors hover:bg-black/60"
        >
          <ChevronRight className="size-5" />
        </button>
      </div>

      <div className="flex items-center justify-between gap-3 border-t border-white/10 bg-[oklch(0.18_0.02_152)] px-4 py-2.5 text-panel-foreground">
        <div>
          <p className="text-sm font-medium text-white">{photo.title}</p>
          <p className="text-xs text-white/60">{photo.meta}</p>
        </div>
        <div className="flex items-center gap-3">
          <ZoomIn className="size-4 text-white/70" />
          <div className="flex gap-1.5">
            {photos.map((_, i) => (
              <button
                key={i}
                onClick={() => setIndex(i)}
                className={cn(
                  "size-2 rounded-full transition-colors",
                  i === index ? "bg-white" : "bg-white/30",
                )}
              />
            ))}
          </div>
        </div>
      </div>
    </div>
  );
}
