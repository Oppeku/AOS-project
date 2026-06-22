import { createFileRoute } from "@tanstack/react-router";
import { Desktop } from "@/components/desktop/Desktop";

export const Route = createFileRoute("/")({
  head: () => ({
    meta: [
      { title: "AOS — A Nature-Inspired Desktop Environment" },
      {
        name: "description",
        content:
          "AOS is a calm, nature-inspired desktop environment concept. Explore system apps like Files, Settings, Terminal, Calculator and more in your browser.",
      },
      { property: "og:title", content: "AOS — A Nature-Inspired Desktop Environment" },
      {
        property: "og:description",
        content:
          "A calm, nature-inspired desktop environment UI concept with system apps you can open, move and resize.",
      },
    ],
  }),
  component: Index,
});

function Index() {
  return <Desktop />;
}
